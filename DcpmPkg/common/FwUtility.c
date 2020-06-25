/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include "FwUtility.h"
#include "Utility.h"
#include <Library/UefiLib.h>
#include "Debug.h"
#include "Version.h"

/**
  Checks if the firmware image is valid for an update.
  Please note that even if this function returns TRUE, the image may not be fully
  valid. There are additional security and CRC checks made by the DIMM that may fail.

  @param[in] pImage is the buffer that contains the image we want to validate.
  @param[in] ImageSize is the size in bytes of the valid image data in the buffer.
    The buffer must be bigger or equal to the ImageSize.
  @param[out] ppError is the pointer to a Unicode string that will contain
    the details about the failure. The caller is responsible to free the allocated
    memory with the FreePool function.

  @retval TRUE if the Image is valid for the update.
  @retval FALSE if the Image is not valid.
**/
BOOLEAN
ValidateImage(
  IN     NVM_FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
     OUT CHAR16** ppError
  )
{
  if (ImageSize > MAX_FIRMWARE_IMAGE_SIZE_B || ImageSize < sizeof(NVM_FW_IMAGE_HEADER)) {
    *ppError = CatSPrint(NULL, L"The image has wrong size! Please try another image.");
    return FALSE;
  }

  if (ImageSize % UPDATE_FIRMWARE_SMALL_PAYLOAD_DATA_PACKET_SIZE != 0) {
    NVDIMM_DBG("The buffer size is not aligned to %d bytes.\n", UPDATE_FIRMWARE_SMALL_PAYLOAD_DATA_PACKET_SIZE);
    return FALSE;
  }

  if (pImage->ModuleVendor != VENDOR_ID || pImage->ModuleType != LT_MODULETYPE_CSS) {
    *ppError = CatSPrint(NULL, L"The firmware is not compatible with the PMem modules.");
    return FALSE;
  }

  return TRUE;
}

/**
  Checks if the firmware image is valid for an recovery over Spi.
  Please note that even if this function returns TRUE, the image may not be fully
  valid. There are additional security and CRC checks made by the DIMM that may fail.

  @param[in] pImage is the buffer that contains the image we want to validate.
  @param[in] ImageSize is the size in bytes of the valid image data in the buffer.
    The buffer must be bigger or equal to the ImageSize.
  @param[in] SubsystemDeviceId is the identifer of the revision of Dimm (AEP vs BPS)
  @param[out] ppError is the pointer to a Unicode string that will contain
    the details about the failure. The caller is responsible to free the allocated
    memory with the FreePool function.

  @retval TRUE if the Image is valid for the update.
  @retval FALSE if the Image is not valid.
**/
BOOLEAN
ValidateRecoverySpiImage(
  IN     NVM_FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
  IN     UINT16 SubsystemDeviceId,
     OUT CHAR16** ppError
  )
{
  CHAR16 *pTmpError = NULL;
  BOOLEAN ReturnValue = FALSE;

  ReturnValue = ValidateImage(pImage, ImageSize, &pTmpError);
  if (ReturnValue) {
    *ppError = CatSPrint(NULL, L"This is standard firmware image. Please provide recovery image");
    ReturnValue = FALSE;
    goto Finish;
  }

  if (SubsystemDeviceId == SPD_DEVICE_ID_10) {
    *ppError = CatSPrint(NULL, L"First generation " PMEM_MODULES_STR " are not supported for SPI image recovery. A 1.x release of this software is required.");
    goto Finish;
  }

  if (SubsystemDeviceId != SPD_DEVICE_ID_15) {
    *ppError = CatSPrint(NULL, PMEM_MODULE_STR L" is reporting an unexpected device id.  SPI image recovery is not supported.");
    goto Finish;
  }

  if (ImageSize != FIRMWARE_SPI_IMAGE_GEN2_SIZE_B) {
    *ppError = CatSPrint(NULL, L"The image has wrong size! Please try another image.");
    goto Finish;
  }

  if (pImage->ModuleVendor != VENDOR_ID || pImage->ModuleType != LT_MODULETYPE_CSS) {
    *ppError = CatSPrint(NULL, L"The firmware is not compatible with the " PMEM_MODULES_STR ".");
    goto Finish;
  }
  ReturnValue = TRUE;

Finish:
  FREE_POOL_SAFE(pTmpError);
  return ReturnValue;
}

/**
  Convert FW API version parts to a string
**/
VOID
ConvertFwApiVersion(
     OUT CHAR16 Version[FW_API_VERSION_LEN],
  IN     UINT8 Major,
  IN     UINT8 Minor
  )
{
  CHAR16 *tmp = NULL;

  NVDIMM_ENTRY();

  if ((Major == 0) && (Minor == 0)) {
    tmp = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
  } else {
    tmp = CatSPrint(NULL, L"%02d.%02d", Major, Minor);
  }
  if (tmp != NULL) {
    StrnCpyS(Version, FW_API_VERSION_LEN, tmp, FW_API_VERSION_LEN - 1);
    FREE_POOL_SAFE(tmp);
  }

  NVDIMM_EXIT();
}

/**
  Convert FW version parts to a string
**/
VOID
ConvertFwVersion(
     OUT CHAR16 Version[FW_VERSION_LEN],
  IN     UINT8 Product,
  IN     UINT8 Revision,
  IN     UINT8 SecurityVersion,
  IN     UINT16 Build
  )
{
  CHAR16 *tmp = NULL;

  NVDIMM_ENTRY();

  if (FW_VERSION_UNDEFINED_BYVERS(Product, Revision, SecurityVersion, Build)) {
    tmp = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
  } else {
    tmp = CatSPrint(NULL, L"%02d.%02d.%02d.%04d", Product, Revision, SecurityVersion, Build);
  }
  if (tmp != NULL) {
    StrnCpyS(Version, FW_VERSION_LEN, tmp, FW_VERSION_LEN - 1);
    FREE_POOL_SAFE(tmp);
  }

  NVDIMM_EXIT();
}

/**
  Check if new FW version is staged

  @param[in] StagedFwVersion Info about staged firmware version

  @retval TRUE if new FW version is staged
  @retval FALSE if new FW version is not staged
**/
BOOLEAN
IsFwStaged(
  IN FIRMWARE_VERSION StagedFwVersion
  )
{
  BOOLEAN Staged = FALSE;

  if (StagedFwVersion.FwProduct != 0 || StagedFwVersion.FwRevision != 0 ||
      StagedFwVersion.FwSecurityVersion != 0 || StagedFwVersion.FwBuild != 0) {
    Staged = TRUE;
  }

  return Staged;
}

/**
  Searches for the file in the provided working directory or the root of all connected
  devices compatible with the EfiSimpleFileSystemProtocol. The function stops on first
  file matching the name.
  After that the function reads the file and examine its FW Image Header.
  If the file header passes the checks, the function returns TRUE.
  Otherwise the function returns FALSE, and the detailed error can be found in the
  ppError OUT variable.

  It is the callers responsibility to free the allocated memory for the OUT variables
  using FreePool() function.

  @param[in] pFilePath - the Unicode string representing the file path relative to the
    devices root directory or the provided working directory.
  @param[in] pWorkingDirectory - the Unicode string representing the working directory
    relative to the devices root directory. The file path is simply appended to the
    working directory path.
  @param[in] FlashSPI flag indicates if this is standard or SPI image
  @param[in] SubsystemDeviceId identifer for dimm generation
  @param[out] ppImageHeader the pointer to the pointer of the Image Header that has been
    read from the file. It takes NULL value if there was a reading error.
  @param[out] ppError the pointer to the pointer of the Unicode string that will contain
    the result error message.

  @retval TRUE if the file is valid for the update.
  @retval FALSE if the file is not valid.
**/
BOOLEAN
LoadFileAndCheckHeader(
  IN     CHAR16 *pFilePath,
  IN     CONST CHAR16 *pWorkingDirectory OPTIONAL,
  IN     BOOLEAN FlashSPI,
  IN     UINT16 SubsystemDeviceId,
     OUT NVM_FW_IMAGE_HEADER **ppImageHeader,
     OUT CHAR16 **ppError
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_FILE_HANDLE FileHandle;
  NVM_SPI_DIRECTORY_GEN2 SpiDirectory;
  BOOLEAN ReturnValue = TRUE;
  UINT64 BuffSize = 0;
  UINT64 FileSize = 0;
  UINT64 BuffSizeTemp = 0;
  UINT64 BuffSpiSize = sizeof(SpiDirectory);
  BOOLEAN VerifyNormalImage = FALSE;
  VOID *pImageBuffer = NULL;

  ZeroMem(&FileHandle, sizeof(FileHandle));
  ZeroMem(&SpiDirectory, sizeof(SpiDirectory));

  ReturnCode = OpenFile(pFilePath, &FileHandle, pWorkingDirectory, FALSE);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("OpenFile returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    *ppError = CatSPrint(NULL, L"Error: The specified source file is not valid.\n");
    ReturnValue = FALSE;
    goto Finish;
  }

  ReturnCode = GetFileSize(FileHandle, &FileSize);
  BuffSize = FileSize;

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("GetFileSize returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    *ppError = CatSPrint(NULL, L"Error: Could not get the file information.\n");
    ReturnValue = FALSE;
    goto FinishClose;
  }

  if (SubsystemDeviceId == SPD_DEVICE_ID_10) {
    if ((!FlashSPI && BuffSize > MAX_FIRMWARE_IMAGE_SIZE_B) ||
      (FlashSPI && BuffSize > FIRMWARE_SPI_IMAGE_GEN1_SIZE_B)) {
      NVDIMM_DBG("File size equals: %d.\n", BuffSize);
      *ppError = CatSPrint(NULL, L"Error: The file is too large.\n");
      ReturnValue = FALSE;
      goto FinishClose;
    }
  } else if (SubsystemDeviceId == SPD_DEVICE_ID_15) {
    if ((!FlashSPI && BuffSize > MAX_FIRMWARE_IMAGE_SIZE_B) ||
      (FlashSPI && BuffSize > FIRMWARE_SPI_IMAGE_GEN2_SIZE_B)) {
      NVDIMM_DBG("File size equals: %d.\n", BuffSize);
      *ppError = CatSPrint(NULL, L"Error: The file is too large.\n");
      ReturnValue = FALSE;
      goto FinishClose;
    }
  } else {
    NVDIMM_DBG("Unknown Subsystem Device Id received: %d.\n", SubsystemDeviceId);
    *ppError = CatSPrint(NULL, L"Error: Subsystem Device Id is unknown. Cannot determine what file size should be.\n");
    ReturnValue = FALSE;
    goto FinishClose;
  }

  if (BuffSize < sizeof(NVM_FW_IMAGE_HEADER)) {
    NVDIMM_DBG("File size equals: %d.\n", BuffSize);
    *ppError = CatSPrint(NULL, L"Error: The file is too small.\n");
    ReturnValue = FALSE;
    goto FinishClose;
  }

  /**
    In this case it is possible that user provided valid, standard FW image
    instead of FlashSPI one. We have to verify and inform about such case
   **/
  if (FlashSPI && BuffSize <= MAX_FIRMWARE_IMAGE_SIZE_B) {
    VerifyNormalImage = TRUE;
  }

  /**
    Here we cast from UINT64 to UINTN, because the ShellReadFile takes UINTN as the buffer size.
    Fortunately our buffer will be smaller even if UINTN is 32-bit.
  **/
  BuffSize = sizeof(NVM_FW_IMAGE_HEADER);
  pImageBuffer = AllocatePool(BuffSize);

  if (pImageBuffer == NULL) {
    *ppError = CatSPrint(NULL, L"Error: Could not allocate memory for the firmware image.\n");
    ReturnValue = FALSE;
    goto FinishClose;
  }

  BuffSizeTemp = BuffSize;

  if (FlashSPI && !VerifyNormalImage) {
    ReturnCode = FileHandle->Read(FileHandle, &BuffSpiSize, &SpiDirectory);
    if (EFI_ERROR(ReturnCode) || BuffSpiSize != sizeof(SpiDirectory)) {
      *ppError = CatSPrint(NULL, L"Error: Could not read the file.\n");
      ReturnValue = FALSE;
      goto FinishClose;
    }

    ReturnCode = FileHandle->SetPosition(FileHandle, SpiDirectory.FwImageStage1Offset);
    if (EFI_ERROR(ReturnCode)) {
      *ppError = CatSPrint(NULL, L"Error: Could not read the file.\n");
      ReturnValue = FALSE;
      goto FinishClose;
    }
  }

  ReturnCode = FileHandle->Read(FileHandle, &BuffSize, pImageBuffer);

  /**
    Cast the buffer to the header pointer, so we can read the image information.
  **/
  *ppImageHeader = (NVM_FW_IMAGE_HEADER *)pImageBuffer;

  /**
    If the read function returned an error OR we read less bytes that the file length equals.
  **/
  if (EFI_ERROR(ReturnCode) || BuffSize != BuffSizeTemp) {
    *ppError = CatSPrint(NULL, L"Error: Could not read the file.\n");
    ReturnValue = FALSE;
    goto FinishClose;

  }
  if (FlashSPI) {
    ReturnValue = ValidateRecoverySpiImage(*ppImageHeader, FileSize, SubsystemDeviceId, ppError);
  } else {
    ReturnValue = ValidateImage(*ppImageHeader, FileSize, ppError);
  }

FinishClose:
  FileHandle->Close(FileHandle);
Finish:
  return ReturnValue;
}


/**
  Calculates the checksum of passed in buffer and keeps a running
  value. Used by CR FW for computing their checksum, used in our code for get/set
  fconfig data.

  @param[in]  pBuffer Pointer to buffer whose checksum needs to be calculated.
  @param[in]  NumBytes Number of bytes inside buffer to calculate checksum on.
  @param[in]  Csum current running checksum.

  @retval The computed checksum
**/
UINT32 RunningChecksum(
  IN VOID *pBuffer,
  IN UINT32 NumBytes,
  IN UINT32 Csum)

{
  UINT32 Index;
  UINT8 *pU8Buffer;

  Csum = (~(Csum) + 1);

  pU8Buffer = pBuffer;

  for (Index = 0; Index < NumBytes; Index++) {
    Csum = Csum + pU8Buffer[Index] * (1 << (8 * (Index % 4)));
  }
  Csum = ~(Csum) + 1;
  return Csum;
}

