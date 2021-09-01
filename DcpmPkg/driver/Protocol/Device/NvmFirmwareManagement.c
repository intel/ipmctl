/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /**
 * @file NvmFirmwareManagement.c
 * @brief The file describes the UEFI Firmware Management Protocol support for Intel Optane Persistent Memory.
 **/

#include <Uefi.h>
#include "NvmFirmwareManagement.h"
#include <Debug.h>
#include <NvmDimmConfig.h>
#include <NvmDimmDriver.h>
#include <FwUtility.h>

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

EFI_GUID gNvmDimmFirmwareManagementProtocolGuid = EFI_FIRMWARE_MANAGEMENT_PROTOCOL_GUID;
EFI_GUID mNvmDimmFirmwareImageTypeGuid = EFI_DCPMM_FIRMWARE_IMAGE_TYPE_GUID;

#define SUPPORTED_DESCRIPTOR_COUNT 1
#define NVDIMM_IMAGE_ID 1
#define DESCRIPTOR_VERSION_DEFAULT 1

// Compilers align structs front and back to the largest scalar member,
// adding padding as necessary.
// http://www.catb.org/esr/structure-packing/
// There's a UINT64, so we should be good for making sure the string (CHAR16)
// buffers start on a 16-bit boundary
// Also, these offsets are relative from the start of the
// EFI_FIRMWARE_IMAGE_DESCRIPTOR struct
#define NVDIMM_IMAGE_ID_NAME_BYTE_OFFSET   sizeof(EFI_FIRMWARE_IMAGE_DESCRIPTOR)

// This value is specific to this file
#define NVDIMM_IMAGE_DESCRIPTOR_MAX_STRING_LEN  255
#define NVDIMM_IMAGE_DESCRIPTOR_MAX_STRING_BUFFER_LENGTH  (NVDIMM_IMAGE_DESCRIPTOR_MAX_STRING_LEN + 1) * sizeof(CHAR16)

// Assume maximum size of strings appended to the end of the
// struct and allocate the whole struct as one piece. The HII spec doesn't
// specify where these strings should reside, so this is one of several possible
// implementations. The caller should free the entire struct, which will free
// the strings as well.
#define NVDIMM_VERSION_NAME_BYTE_OFFSET    NVDIMM_IMAGE_ID_NAME_BYTE_OFFSET + NVDIMM_IMAGE_DESCRIPTOR_MAX_STRING_BUFFER_LENGTH
#define NVDIMM_IMAGE_DESCRIPTOR_SIZE  NVDIMM_VERSION_NAME_BYTE_OFFSET + NVDIMM_IMAGE_DESCRIPTOR_MAX_STRING_BUFFER_LENGTH

typedef struct _SET_IMAGE_ATTRIBUTES{
  BOOLEAN Force;
} SET_IMAGE_ATTRIBUTES;

/*
  In addition to the function information from the library header.

  As for the Intel PMem module implementation, one PMem module stores only one Firmware,
  so the *DescriptorCount will be always 1.

  Even if there are more images on the FV we have access only to
  the one.
*/

/**
Returns information about the current firmware image(s) of the device.

@param[in] This A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
@param[in,out] ImageInfoSize A pointer to the size, in bytes, of the ImageInfo buffer. On input, this is the
size of the buffer allocated by the caller. On output, it is the size of the buffer
returned by the firmware if the buffer was large enough, or the size of the
buffer needed to contain the image(s) information if the buffer was too small.
@param[in,out] ImageInfo A pointer to the buffer in which firmware places the current image(s)
information. The information is an array of
EFI_FIRMWARE_IMAGE_DESCRIPTORs. See "Related Definitions".
@param[out] DescriptorVersion A pointer to the location in which firmware returns the version number
associated with the EFI_FIRMWARE_IMAGE_DESCRIPTOR. See "Related
Definitions".
@param[out] DescriptorCount A pointer to the location in which firmware returns the number of
descriptors or firmware images within this device.
@param[out] DescriptorSize A pointer to the location in which firmware returns the size, in bytes, of an
individual EFI_FIRMWARE_IMAGE_DESCRIPTOR.
@param[out] PackageVersion A version number that represents all the firmware images in the device. The
format is vendor specific and new version must have a greater value than the
old version. If PackageVersion is not supported, the value is 0xFFFFFFFF. A
value of 0xFFFFFFFE indicates that package version comparison is to be
performed using PackageVersionName. A value of 0xFFFFFFFD indicates
that package version update is in progress.
@param[out] PackageVersionName A pointer to a pointer to a null-terminated string representing the package
version name. The buffer is allocated by this function with AllocatePool(),
and it is the caller's responsibility to free it with a call to FreePool().

@retval EFI_SUCCESS The image information was successfully returned.
@retval EFI_BUFFER_TOO_SMAL The ImageInfo buffer was too small. The current buffer size
needed to hold the image(s) information is returned in ImageInfoSize.
@retval EFI_INVALID_PARAMETER The ImageInfoSize is NULL.
@retval EFI_DEVICE_ERROR Valid information could not be returned. Possible corrupted image.
**/
EFI_STATUS
EFIAPI
GetImageInfo (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  IN OUT UINTN *pImageInfoSize,
  IN OUT EFI_FIRMWARE_IMAGE_DESCRIPTOR *pImageInfo,
     OUT UINT32 *pDescriptorVersion,
     OUT UINT8 *pDescriptorCount,
     OUT UINTN *pDescriptorSize,
     OUT UINT32 *pPackageVersion,
     OUT CHAR16 **ppPackageVersionName
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  FW_VERSION_UNION  Uint32Version;
  CHAR16 *ImageIdName;
  CHAR16 *VersionName;

  NVDIMM_ENTRY();

  SetMem(&Uint32Version, sizeof(Uint32Version), 0x0);

  if (NULL == pImageInfoSize || NULL == This
    || (NULL == pImageInfo && *pImageInfoSize >= NVDIMM_IMAGE_DESCRIPTOR_SIZE)) {
    goto Finish;
  }

  //per UEFI spec, NULL pImageInfo with 0 pImageInfoSize is used to obtain size
  if (NULL == pImageInfo && 0 == *pImageInfoSize)
  {
    *pImageInfoSize = NVDIMM_IMAGE_DESCRIPTOR_SIZE;
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  if (*pImageInfoSize < NVDIMM_IMAGE_DESCRIPTOR_SIZE) {
    *pImageInfoSize = NVDIMM_IMAGE_DESCRIPTOR_SIZE;
    ReturnCode = EFI_BUFFER_TOO_SMALL;
    goto Finish;
  }

  pDimm = GET_DIMM_FROM_INSTANCE(This)->pDimm;
  if (pDimm == NULL) {
    goto Finish;
  }

  /*
    Here we are packing the 8-bit values into 6-bit containers to fit in the UINT32.
    If the versions will ever go above the 6-bit integer, we will lose that information,
    but there is nothing else that we can do and we have to agree with that.
  */
  Uint32Version.PackedVersion.FwProduct = pDimm->FwVer.FwProduct;
  Uint32Version.PackedVersion.FwRevision = pDimm->FwVer.FwRevision;
  Uint32Version.PackedVersion.FwSecurityVersion = pDimm->FwVer.FwSecurityVersion;
  Uint32Version.PackedVersion.FwBuild = pDimm->FwVer.FwBuild;

  pImageInfo[0].ImageIndex = 1;
  pImageInfo[0].ImageTypeId = mNvmDimmFirmwareImageTypeGuid;
  pImageInfo[0].ImageId = NVDIMM_IMAGE_ID;
  // Copy the image id name string to a region after the end of the struct
  ImageIdName = (CHAR16 *)(((UINT8 *)&pImageInfo[0]) + NVDIMM_IMAGE_ID_NAME_BYTE_OFFSET);
  StrnCpyS(ImageIdName, NVDIMM_IMAGE_ID_NAME_LEN, NVDIMM_IMAGE_ID_NAME, NVDIMM_IMAGE_ID_NAME_LEN - 1);
  // Pointer to that region
  pImageInfo[0].ImageIdName = ImageIdName;

  pImageInfo[0].Version = Uint32Version.AsUint32;

  // Creates fw version string and writes to a provided location
  VersionName = (CHAR16 *)(((UINT8 *)&pImageInfo[0]) + NVDIMM_VERSION_NAME_BYTE_OFFSET);
  ConvertFwVersion(VersionName, pDimm->FwVer.FwProduct, pDimm->FwVer.FwRevision, pDimm->FwVer.FwSecurityVersion, pDimm->FwVer.FwBuild);
  pImageInfo[0].VersionName = VersionName;

  pImageInfo[0].Size = MAX_FIRMWARE_IMAGE_SIZE_B; // No planned way to get installed image size.
                                                  // Set allowed maximum.
  pImageInfo[0].AttributesSupported = IMAGE_ATTRIBUTE_IMAGE_UPDATABLE
      | IMAGE_ATTRIBUTE_RESET_REQUIRED
      | IMAGE_ATTRIBUTE_IN_USE;
  pImageInfo[0].AttributesSetting = pImageInfo[0].AttributesSupported;
  // We use the Firmware Major API version as the compatibility indicator
  pImageInfo[0].Compatibilities = pDimm->FwVer.FwApiMajor;

  if (pPackageVersion != NULL) {
    *pPackageVersion = PACKAGE_VERSION_DEFINED_BY_PACKAGE_NAME;
  }

  if (ppPackageVersionName != NULL) {
    // Use existing version name string
    *ppPackageVersionName = AllocateCopyPool(FW_VERSION_LEN * sizeof(CHAR16), VersionName);

  }

  if (pDescriptorSize != NULL) {
    *pDescriptorSize = NVDIMM_IMAGE_DESCRIPTOR_SIZE;
  }

  if (pDescriptorCount != NULL) {
    *pDescriptorCount = SUPPORTED_DESCRIPTOR_COUNT;
  }

  if (pDescriptorVersion != NULL) {
    *pDescriptorVersion = DESCRIPTOR_VERSION_DEFAULT;
  }
  ReturnCode = EFI_SUCCESS;
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Updates the firmware image of the PMem module.

@remarks If ARS is in progress on any target PMem module,
an attempt will be made to abort ARS and to proceed with the firmware update.

@remarks A reboot is required to activate the updated firmware image and is
recommended to ensure ARS runs to completion.

@param[in] pThis is a pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
@param[in] ImageIndex A unique number identifying the firmware image(s) within the device. The
number is between 1 and DescriptorCount.
@param[in] Image Points to the new image.
@param[in] ImageSize Size of the new image in bytes
@param[in] VendorCode This enables vendor to implement vendor-specific firmware image update
policy. Null indicates the caller did not specify the policy or use the default
policy.
@param[in] Progress A function used by the driver to report the progress of the firmware update.
@param[out] AbortReason A pointer to a pointer to a null-terminated string providing more details for
the aborted operation. The buffer is allocated by this function with
AllocatePool(), and it is the caller's responsibility to free it with a call to
FreePool().

@retval EFI_SUCCESS The device was successfully updated with the new image.
@retval EFI_ABORTED The operation is aborted.
@retval EFI_INVALID_PARAMETER The Image was NULL.
@retval EFI_UNSUPPORTED The operation is not supported.
@retval EFI_SECURITY_VIOLATION The operation could not be performed due to an authentication
failure.
**/
EFI_STATUS
EFIAPI
SetImage (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  IN     UINT8 ImageIndex,
  IN     CONST VOID *Image,
  IN     UINTN ImageSize,
  IN     CONST VOID *VendorCode,
  IN     EFI_FIRMWARE_MANAGEMENT_UPDATE_IMAGE_PROGRESS Progress,
     OUT CHAR16 **AbortReason
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 pImageSizeError[] = L"Error: The image size is too large.";
  CHAR16 pImageContentError[] = L"Error: Invalid image file.";
  CHAR16 pImageVersionError[] = L"Error: Firmware version too low. Force required.";
  NVM_STATUS Status = NVM_ERR_OPERATION_NOT_STARTED;
  CONST CHAR16 *pSingleStatusCodeMessage = NULL;
  SET_IMAGE_ATTRIBUTES *SetImageAttributes = (SET_IMAGE_ATTRIBUTES *)VendorCode;
  COMMAND_STATUS *pCommandStatus = NULL;

  BOOLEAN Force = FALSE;

  NVDIMM_ENTRY();

  if (NULL == This || NULL == Image || NULL == AbortReason ||
      ImageIndex < 1 || ImageIndex > SUPPORTED_DESCRIPTOR_COUNT) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  CHECK_RESULT(InitializeCommandStatus(&pCommandStatus), Finish);

  if (ImageSize > MAX_FIRMWARE_IMAGE_SIZE_B) {
    *AbortReason = AllocateCopyPool(sizeof(pImageSizeError), pImageSizeError);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (ImageSize < sizeof(NVM_FW_IMAGE_HEADER)) {
    *AbortReason = AllocateCopyPool(sizeof(pImageContentError), pImageContentError);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  /*
     Inform the caller that we do not support progress reporting.
  */
  if (Progress != NULL) {
    Progress(0);
  }

  if (NULL != SetImageAttributes) {
    Force = SetImageAttributes->Force;
  }

  ReturnCode = ValidateImageVersion((NVM_FW_IMAGE_HEADER *)Image, Force, GET_DIMM_FROM_INSTANCE(This)->pDimm, &Status, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    if (Status == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {
      *AbortReason = AllocateCopyPool (StrSize(pImageVersionError), pImageVersionError);
    }
    goto Finish;
  }

  ReturnCode = FwCmdUpdateFw(GET_DIMM_FROM_INSTANCE(This)->pDimm, Image, ImageSize, &Status, NULL);

  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmData->HiiHandle, Status);
    if (pSingleStatusCodeMessage != NULL) {
      *AbortReason = AllocateCopyPool (StrSize(pSingleStatusCodeMessage), pSingleStatusCodeMessage);
    }
    NVDIMM_WARN("Could not update the firmware on the DIMM(s).\n");
    goto Finish;
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pSingleStatusCodeMessage);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Returns information about the firmware package on the specified PMem module.

@param[in] This A pointer to the EFI_FIRMWARE_MANAGEMENT_PROTOCOL instance.
@param[out] PackageVersion A version number that represents all the firmware images in the device. The
format is vendor specific and new version must have a greater value than the
old version. If PackageVersion is not supported, the value is 0xFFFFFFFF. A
value of 0xFFFFFFFE indicates that package version comparison is to be
performed using PackageVersionName. A value of 0xFFFFFFFD indicates
that package version update is in progress.
@param[out] PackageVersionName A pointer to a pointer to a null-terminated string representing the package
version name. The buffer is allocated by this function with AllocatePool(),
and it is the caller's responsibility to free it with a call to FreePool().
@param[out] PackageVersionNameMaxLen The maximum length of package version name if device supports update of
package version name. A value of 0 indicates the device does not support
update of package version name. Length is the number of Unicode
characters, including the terminating null character.
@param[out] AttributesSupported Package attributes that are supported by this device. See "Package Attribute
Definitions" for possible returned values of this parameter. A value of 1
indicates the attribute is supported and the current setting value is indicated
in AttributesSetting. A value of 0 indicates the attribute is not supported and
the current setting value in AttributesSetting is meaningless.
@param[out] AttributesSetting Package attributes. See "Package Attribute Definitions" for possible returned
values of this parameter.

@retval EFI_SUCCESS The image information was successfully returned.
@retval EFI_UNSUPPORTED The operation is not supported.
**/

EFI_STATUS
EFIAPI
GetPackageInfo (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
     OUT UINT32 *PackageVersion,
     OUT CHAR16 **PackageVersionName,
     OUT UINT32 *PackageVersionNameMaxLen,
     OUT UINT64 *AttributesSupported,
     OUT UINT64 *AttributesSetting
  )
{
  DIMM *pDimm = NULL;
  CHAR16 FwVersion[FW_VERSION_LEN];
  CONST UINT64 NvmFwMgmtAttributesSupported = PACKAGE_ATTRIBUTE_VERSION_UPDATABLE | PACKAGE_ATTRIBUTE_RESET_REQUIRED;

  SetMem(&FwVersion, sizeof(FwVersion), 0x0);

  if (NULL == This)
  {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL != PackageVersion) {
    *PackageVersion = PACKAGE_VERSION_DEFINED_BY_PACKAGE_NAME;
  }

  if (NULL != PackageVersionName) {

    pDimm = GET_DIMM_FROM_INSTANCE(This)->pDimm;

    ConvertFwVersion(FwVersion, pDimm->FwVer.FwProduct, pDimm->FwVer.FwRevision, pDimm->FwVer.FwSecurityVersion, pDimm->FwVer.FwBuild);

    *PackageVersionName = AllocateCopyPool(sizeof(FwVersion), FwVersion);
  }

  /*
    Zero means that we don't support updating the Package Version Name.
  */
  if (NULL != PackageVersionNameMaxLen) {
    *PackageVersionNameMaxLen = 0;
  }

  if (NULL != AttributesSupported) {
    *AttributesSupported = NvmFwMgmtAttributesSupported;
  }

  /*
    We can't change the settings so what is supported is also set.
  */
  if (NULL != AttributesSetting) {
    *AttributesSetting = NvmFwMgmtAttributesSupported;
  }
  /*
    This function returns UNSUPPORTED or SUCCESS,
    since we support it, we can't return any error codes.

    If there is an error, it will be passed by the OUT parameters.
  */
  return EFI_SUCCESS;
}

/**
Retrieves a copy of the current firmware image of the device.

@remarks This is not supported.

@retval EFI_UNSUPPORTED The operation is not supported.
**/
EFI_STATUS
EFIAPI
GetImage (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  IN     UINT8 ImageIndex,
  IN OUT VOID *Image,
  IN OUT UINTN *ImageSize
  )
{
  /*
    We do not support this callback.
  */
  return EFI_UNSUPPORTED;
}

/**
Checks if the firmware image is valid for the device.

@remarks This is not supported.

@retval EFI_UNSUPPORTED The operation is not supported.
**/
EFI_STATUS
EFIAPI
CheckImage (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  IN     UINT8 ImageIndex,
  IN     CONST VOID *Image,
  IN     UINTN ImageSize,
     OUT UINT32 *ImageUpdatable
  )
{
  /*
    We do not support this callback.
  */
  return EFI_UNSUPPORTED;
}

/**
Updates information about the firmware package.

@remarks This is not supported.

@retval EFI_UNSUPPORTED The operation is not supported.
**/
EFI_STATUS
EFIAPI
SetPackageInfo (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  IN     CONST VOID *Image,
  IN     UINTN ImageSize,
  IN     CONST VOID *VendorCode,
  IN     UINT32 PackageVersion,
  IN     CONST CHAR16 *PackageVersionName
  )
{
  /*
    We do not support this callback.
  */
  return EFI_UNSUPPORTED;
}

EFI_FIRMWARE_MANAGEMENT_PROTOCOL gNvmDimmFirmwareManagementProtocol =
{
  GetImageInfo,
  GetImage,
  SetImage,
  CheckImage,
  GetPackageInfo,
  SetPackageInfo
};
