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
  @param[in] FWImageMaxSize is the maximum allowed size in bytes of the image.
  @param[out] pCommandStatus structure containing detailed NVM error codes

  @retval TRUE if the Image is valid for the update.
  @retval FALSE if the Image is not valid.
**/
BOOLEAN
ValidateImage(
  IN     NVM_FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
  IN     UINT64 FWImageMaxSize,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  if (ImageSize > FWImageMaxSize || ImageSize < sizeof(NVM_FW_IMAGE_HEADER)) {
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_WRONG_IMAGE_SIZE);
    return FALSE;
  }

  if (ImageSize % UPDATE_FIRMWARE_SMALL_PAYLOAD_DATA_PACKET_SIZE != 0) {
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_IMAGE_NOT_ALIGNED);
    NVDIMM_DBG("The buffer size is not aligned to %d bytes.\n", UPDATE_FIRMWARE_SMALL_PAYLOAD_DATA_PACKET_SIZE);
    return FALSE;
  }

  if (pImage->ModuleVendor != VENDOR_ID) {
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_VENDOR_NOT_COMPATIBLE);
    return FALSE;
  }

  if (pImage->ModuleType != LT_MODULE_TYPE_CSS) {
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_MODULE_TYPE_NOT_COMPATIBLE);
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
  @param[in] FWImageMaxSize is the maximum allowed size in bytes of the image.
  @param[in] SubsystemDeviceId is the identifier of the revision of Dimm (AEP vs BPS)

  @retval TRUE if the Image is valid for the update.
  @retval FALSE if the Image is not valid.
**/
BOOLEAN
ValidateRecoverySpiImage(
  IN     NVM_FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
  IN     UINT64 FWImageMaxSize,
  IN     UINT16 SubsystemDeviceId
  )
{
  BOOLEAN ReturnValue = FALSE;
  COMMAND_STATUS *pTmpCmdStatus = NULL;

  pTmpCmdStatus = AllocateZeroPool(sizeof(COMMAND_STATUS));
  if (pTmpCmdStatus == NULL) {
    NVDIMM_ERR("Out of memory");
    ReturnValue = FALSE;
    goto Finish;
  }

  ReturnValue = ValidateImage(pImage, ImageSize, FWImageMaxSize, pTmpCmdStatus);
  if (ReturnValue) {
    NVDIMM_ERR("This is standard firmware image. Please provide recovery image");
    ReturnValue = FALSE;
    goto Finish;
  }

  if (SubsystemDeviceId == SPD_DEVICE_ID_10) {
    NVDIMM_ERR("First generation PMem modules are not supported for SPI image recovery. A 1.x release of this software is required.");
    goto Finish;
  }

  if (SubsystemDeviceId != SPD_DEVICE_ID_15) {
    NVDIMM_ERR("A PMem module is reporting an unexpected device id. SPI image recovery is not supported.");
    goto Finish;
  }

  if (ImageSize != FIRMWARE_SPI_IMAGE_GEN2_SIZE_B) {
    NVDIMM_ERR("The image has wrong size! Please try another image.");
    goto Finish;
  }

  if (pImage->ModuleVendor != VENDOR_ID || pImage->ModuleType != LT_MODULE_TYPE_CSS) {
    NVDIMM_ERR("The firmware is not compatible with the PMem module.");
    goto Finish;
  }
  ReturnValue = TRUE;

Finish:
  FREE_POOL_SAFE(pTmpCmdStatus);
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

  if (FW_VERSION_UNDEFINED_BY_VERSION(Product, Revision, SecurityVersion, Build)) {
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
  @param[in] SubsystemDeviceId identifier for dimm generation
  @param[in] FWImageMaxSize is the maximum allowed size in bytes of the image.
  @param[out] ppImageHeader the pointer to the pointer of the Image Header that has been
    read from the file. It takes NULL value if there was a reading error.
  @param[out] pCommandStatus structure containing detailed NVM error codes

  @retval TRUE if the file is valid for the update.
  @retval FALSE if the file is not valid.
**/
BOOLEAN
LoadFileAndCheckHeader(
  IN     CHAR16 *pFilePath,
  IN     CONST CHAR16 *pWorkingDirectory OPTIONAL,
  IN     BOOLEAN FlashSPI,
  IN     UINT16 SubsystemDeviceId,
  IN     UINT64 FWImageMaxSize,
     OUT NVM_FW_IMAGE_HEADER **ppImageHeader,
     OUT COMMAND_STATUS *pCommandStatus
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

  NVDIMM_ENTRY();

  ZeroMem(&FileHandle, sizeof(FileHandle));
  ZeroMem(&SpiDirectory, sizeof(SpiDirectory));

  ReturnCode = OpenFileBinary(pFilePath, &FileHandle, pWorkingDirectory, FALSE);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("OpenFile returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_FILE_NOT_VALID);
    ReturnValue = FALSE;
    goto Finish;
  }

  ReturnCode = GetFileSize(FileHandle, &FileSize);
  BuffSize = FileSize;

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("GetFileSize returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_FILE_IMG_INFO_NOT_ACCESSIBLE);
    ReturnValue = FALSE;
    goto FinishClose;
  }

  if (SubsystemDeviceId == SPD_DEVICE_ID_10) {
    if ((!FlashSPI && BuffSize > FWImageMaxSize) ||
      (FlashSPI && BuffSize > FIRMWARE_SPI_IMAGE_GEN1_SIZE_B)) {
      NVDIMM_ERR("File size is too large. It equals: %d.\n", BuffSize);
      CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
          DETAILS_FILE_TOO_LARGE);
      ReturnValue = FALSE;
      goto FinishClose;
    }
  } else if (SubsystemDeviceId == SPD_DEVICE_ID_15) {
    if ((!FlashSPI && BuffSize > FWImageMaxSize) ||
      (FlashSPI && BuffSize > FIRMWARE_SPI_IMAGE_GEN2_SIZE_B)) {
      NVDIMM_ERR("File size is too large. It equals: %d.\n", BuffSize);
      CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
          DETAILS_FILE_TOO_LARGE);
      ReturnValue = FALSE;
      goto FinishClose;
    }
  } else if (SubsystemDeviceId == SPD_DEVICE_ID_20) {
    if ((!FlashSPI && BuffSize > FWImageMaxSize)) {
      NVDIMM_ERR("File size is too large. It equals: %d.\n", BuffSize);
      CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
          DETAILS_FILE_TOO_LARGE);
      ReturnValue = FALSE;
      goto FinishClose;
    }
  } else {
    NVDIMM_ERR("Unknown Subsystem Device Id received: %d.\n", SubsystemDeviceId);
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_UNKNOWN_SUBSYSTEM_DEVICE);
    ReturnValue = FALSE;
    goto FinishClose;
  }

  if (BuffSize < sizeof(NVM_FW_IMAGE_HEADER)) {
    NVDIMM_ERR("File size is too small. It equals: %d.\n", BuffSize);
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_FILE_TOO_SMALL);
    ReturnValue = FALSE;
    goto FinishClose;
  }

  /**
    In this case it is possible that user provided valid, standard FW image
    instead of FlashSPI one. We have to verify and inform about such case
   **/
  if (FlashSPI && BuffSize <= FWImageMaxSize) {
    VerifyNormalImage = TRUE;
  }

  /**
    Here we cast from UINT64 to UINTN, because the ShellReadFile takes UINTN as the buffer size.
    Fortunately our buffer will be smaller even if UINTN is 32-bit.
  **/
  BuffSize = sizeof(NVM_FW_IMAGE_HEADER);
  pImageBuffer = AllocatePool(BuffSize);

  if (pImageBuffer == NULL) {
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_MEM_ALLOCATION_ERROR_FW_IMG);
    ReturnValue = FALSE;
    goto FinishClose;
  }

  BuffSizeTemp = BuffSize;

  if (FlashSPI && !VerifyNormalImage) {
    ReturnCode = FileHandle->Read(FileHandle, &BuffSpiSize, &SpiDirectory);
    if (EFI_ERROR(ReturnCode) || BuffSpiSize != sizeof(SpiDirectory)) {
      CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
          DETAILS_FILE_READ_ERROR);
      ReturnValue = FALSE;
      goto FinishClose;
    }

    ReturnCode = FileHandle->SetPosition(FileHandle, SpiDirectory.FwImageStage1Offset);
    if (EFI_ERROR(ReturnCode)) {
      CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
          DETAILS_FILE_READ_ERROR);
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
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_FILE_READ_ERROR);
    ReturnValue = FALSE;
    goto FinishClose;

  }
  if (FlashSPI) {
    ReturnValue = ValidateRecoverySpiImage(*ppImageHeader, FileSize, FWImageMaxSize, SubsystemDeviceId);
  } else {
    ReturnValue = ValidateImage(*ppImageHeader, FileSize, FWImageMaxSize, pCommandStatus);
  }

FinishClose:
  FileHandle->Close(FileHandle);
Finish:
  NVDIMM_EXIT_I64(ReturnValue);
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

