/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWUTILITY_H_
#define _FWUTILITY_H_

#include <Uefi.h>
#include "NvmTypes.h"

#define MAX_FIRMWARE_IMAGE_SIZE_KB 260
#define FIRMWARE_RECOVERY_IMAGE_SIZE_KB 1024
/**
  The maximum file size that a new firmware image can have - in bytes.
**/
#define MAX_FIRMWARE_IMAGE_SIZE_B         KIB_TO_BYTES(MAX_FIRMWARE_IMAGE_SIZE_KB)
#define FIRMWARE_SPI_IMAGE_SIZE_B    KIB_TO_BYTES(FIRMWARE_RECOVERY_IMAGE_SIZE_KB)
#define NO_FW_GIVEN_VERSION_MSG           L"None"

/** Firmware types **/
#define FW_TYPE_PRODUCTION  29
#define FW_TYPE_DFX         30
#define FW_TYPE_DEBUG       31

#define FW_COMMIT_ID_LENGTH 40
#define FW_COMMIT_ID_STR_LENGTH 41
#define FW_BUILD_CONFIGURATION_LENGTH 16
#define FW_BUILD_CONFIGURATION_STR_LENGTH 17

/** Staged FW statuses **/
#define FW_NO_NEW_FW_STAGED  0
#define FW_NEW_FW_STAGED     1

/** Firmware update statuses **/
#define FW_UPDATE_STATUS_STAGED_SUCCESS 1
#define FW_UPDATE_STATUS_LOAD_SUCCESS 2
#define FW_UPDATE_STATUS_FAILED 3

#define IN_MB_SIZE          (1 << 20)   //!< Size of the OS mailbox large input payload
#define OUT_MB_SIZE         (1 << 20)   //!< Size of the OS mailbox large output payload
#define IN_PAYLOAD_SIZE     (128)       //!< Total size of the input payload registers
#define OUT_PAYLOAD_SIZE    (128)       //!< Total size of the output payload registers

#define MB_COMPLETE 0x1
#define STATUS_MASK 0xFF

#pragma pack(push)
#pragma pack(1)
typedef struct {
  UINT32 InputPayloadSize;
  UINT32 LargeInputPayloadSize;
  UINT32 OutputPayloadSize;
  UINT32 LargeOutputPayloadSize;
  UINT8 InputPayload[IN_PAYLOAD_SIZE];
  UINT8 LargeInputPayload[IN_MB_SIZE];
  UINT8 OutPayload[OUT_PAYLOAD_SIZE];
  UINT8 LargeOutputPayload[OUT_MB_SIZE];
  UINT32 DimmID;
  UINT8 Opcode;
  UINT8 SubOpcode;
  UINT8 Status;
} FW_CMD;
#pragma pack(pop)

/**
  Version struct definition
**/
typedef union {
  struct {
    UINT8 Digit2:4;
    UINT8 Digit1:4;
  } Nibble;
  UINT8 Version;
} VERSION_BYTE;

typedef union {
  struct {
    UINT16 Digit4:4;
    UINT16 Digit3:4;
    UINT16 Digit2:4;
    UINT16 Digit1:4;
  } Nibble;
  UINT16 Build;
} BUILD_WORD;

typedef union {
   struct {
    UINT8 Digit2;
    UINT8 Digit1;
  } Byte;
  UINT16 Version;
} API_VERSION;

typedef union {
  struct {
    UINT16 Year:16;
    UINT8 Month:8;
    UINT8 Day:8;
  } Separated;
  UINT32 Value;
} DATE;

/**
  All BCD encoded aa.bb.cc.dddd
**/
#pragma pack(push)
#pragma pack(1)
typedef struct _FW_VERSION {
  BUILD_WORD BuildNumber;             //!< dddd
  VERSION_BYTE SecurityVersionNumber; //!< cc
  VERSION_BYTE RevisionNumber;        //!< bb
  VERSION_BYTE ProductNumber;         //!< aa
} FW_VERSION;

/**
  FW Image header: Intel CSS Header (128 bytes)
**/
typedef struct {
  UINT32 ModuleType;    //!< moduleType = LT_MODULETYPE_CSS
  UINT32 HeaderLen;     //!< headerLen == dword_sizeof(fixedHeader) + (modulusSize * 2) + exponentSize
  UINT32 HeaderVersion; //!< bits [31:16] are major version, bits [15:0] are minor version
  UINT32 ModuleID;      //!< if bit 31 == 1 this is a debug module
  UINT32 ModuleVendor;  //!< moduleVendor = 0x00008086
  DATE Date;            //!< BCD format: yyyymmdd
  UINT32 Size;          //!< Size of entire module (header, crypto(modulus, exponent, signature), data) in DWORDs
  UINT32 KeySize;       //!< Size of RSA public key in DWORDs
  UINT32 ModulusSize;   //!< Size of RSA public key modulus in DWORDs
  UINT32 ExponentSize;  //!< Size of RSA public key exponent
  UINT8  ImageType;
  FW_VERSION ImageVersion;
  UINT32 PerPartIdHigh;
  UINT32 PerPartIdLow;
  UINT32 ImageSize;
  API_VERSION FwApiVersion;
  UINT8 Reserved[68];
} FW_IMAGE_HEADER;

/**
  SPI Directory structure that holds SPI memory map
**/
typedef struct {
  UINT16 DirectoryVersion;
  UINT16 DirectorySize;
  UINT32 SpiSoftFusesDataOffset;
  UINT32 FwImageStage10Offset;
  UINT32 FwImageStage20Offset;
  UINT32 FwImageCopyStage10Offset;
  UINT32 FwImageCopyStage20Offset;
  UINT32 FwImageDfxState10Offset;
  UINT32 FwImageDfxStage20Offset;
} SPI_DIRECTORY;

#pragma pack(pop)

/**
  FW Image header information
**/
typedef struct {
  FW_VERSION ImageVersion;
  UINT8 FirmwareType;
  UINT32 ModuleVendor;  //!< moduleVendor = 0x00008086
  DATE Date;            //!< BCD format: yyyymmdd
  UINT32 Size;          //!< Size of entire module (header, crypto, data) in DWORDs
} FW_IMAGE_INFO;

/**
  The AEP module type code (taken from FW image)
**/
#define LT_MODULETYPE_CSS 0x6

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
  IN     FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
     OUT CHAR16 **ppError
  );

/**
  Checks if the firmware image is valid for an recovery.
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
ValidateRecoverySpiImage(
  IN     FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
     OUT CHAR16 **ppError
  );

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
  @param[in] Recovery flag indicates if this is standard or recovery image
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
  IN     BOOLEAN Recovery,
     OUT FW_IMAGE_HEADER **ppImageHeader,
     OUT CHAR16 **ppError
  );

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
  );

/**
  Convert FW API version parts to a string
**/
VOID
ConvertFwApiVersion(
     OUT CHAR16 Version[FW_API_VERSION_LEN],
  IN     UINT8 Major,
  IN     UINT8 Minor
  );

/**
  Check if new FW version is staged

  @param[in] StagedFwVersion Info about staged firmware version

  @retval TRUE if new FW version is staged
  @retval FALSE if new FW version is not staged
**/
BOOLEAN
IsFwStaged(
  IN FIRMWARE_VERSION StagedFwVersion
  );

#endif /** _FWUTILITY_H_ **/
