/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

// Assume maximum size of strings appended to the end of the
// struct and allocate the whole struct as one piece. The HII spec doesn't
// specify where these strings should reside, so this is one of several possible
// implementations. The caller should free the entire struct, which will free
// the strings as well.
#define NVDIMM_VERSION_NAME_BYTE_OFFSET    NVDIMM_IMAGE_ID_NAME_BYTE_OFFSET + HII_MAX_STRING_BUFFER_LENGTH
#define NVDIMM_IMAGE_DESCRIPTOR_SIZE  NVDIMM_VERSION_NAME_BYTE_OFFSET + HII_MAX_STRING_BUFFER_LENGTH

/**
  In addition to the function information from the library header.

  As for the Intel NVM Dimm implementation, one DIMM stores only one Firmware,
  so the *DescriptorCount will be always 1.

  Even if there are more images on the FV we have access only to
  the main one.
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

  if (pImageInfoSize == NULL) {
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

  /**
    Here we are packing the 8-bit values into 6-bit containers to fit in the UINT32.
    If the versions will ever go above the 6-bit integer, we will lose that information,
    but there is nothing else that we can do and we have to agree with that.
  **/
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
  NVM_STATUS Status = NVM_ERR_OPERATION_NOT_STARTED;
  CONST CHAR16 *pSingleStatusCodeMessage = NULL;

  NVDIMM_ENTRY();

  if (ImageIndex < 1 || ImageIndex > SUPPORTED_DESCRIPTOR_COUNT) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (Image == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (ImageSize > MAX_FIRMWARE_IMAGE_SIZE_B) {
    *AbortReason = AllocateCopyPool(sizeof(pImageSizeError), pImageSizeError);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (ImageSize < sizeof(FW_IMAGE_HEADER)) {
    *AbortReason = AllocateCopyPool(sizeof(pImageContentError), pImageContentError);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  /**
    Inform the caller that we do not support progress reporting.
  **/
  if (Progress != NULL) {
    Progress(0);
  }

  ReturnCode = UpdateDimmFw(GET_DIMM_FROM_INSTANCE(This)->pDimm->DimmID, Image, ImageSize, FALSE, &Status);
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
  FREE_POOL_SAFE(pSingleStatusCodeMessage);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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

  SetMem(&FwVersion, sizeof(FwVersion), 0x0);

  *PackageVersion = PACKAGE_VERSION_DEFINED_BY_PACKAGE_NAME;

  pDimm = GET_DIMM_FROM_INSTANCE(This)->pDimm;

  ConvertFwVersion(FwVersion, pDimm->FwVer.FwProduct, pDimm->FwVer.FwRevision, pDimm->FwVer.FwSecurityVersion, pDimm->FwVer.FwBuild);

  *PackageVersionName = AllocateCopyPool(sizeof(FwVersion), FwVersion);

  /**
    Zero means that we don't support updating the Package Version Name.
  **/
  *PackageVersionNameMaxLen = 0;

  *AttributesSupported = PACKAGE_ATTRIBUTE_VERSION_UPDATABLE | PACKAGE_ATTRIBUTE_RESET_REQUIRED;

  /**
    We can't change the settings so what is supported is also set.
  **/
  *AttributesSetting = *AttributesSupported;
  /**
    This function returns UNSUPPORTED or SUCCESS,
    since we support it, we can't return any error codes.

    If there is an error, it will be passed by the OUT parameters.
  **/
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetImage (
  IN     EFI_FIRMWARE_MANAGEMENT_PROTOCOL *This,
  IN     UINT8 ImageIndex,
  IN OUT VOID *Image,
  IN OUT UINTN *ImageSize
  )
{
  /**
    We do not support this callback.
  **/
  return EFI_UNSUPPORTED;
}

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
  /**
    We do not support this callback.
  **/
  return EFI_UNSUPPORTED;
}

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
  /**
    We do not support this callback.
  **/
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
