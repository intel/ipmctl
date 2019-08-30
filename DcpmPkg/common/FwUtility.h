/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FWUTILITY_H_
#define _FWUTILITY_H_

#include "NvmTypes.h"

#define MAX_FIRMWARE_IMAGE_SIZE_KB 788
#define FIRMWARE_RECOVERY_IMAGE_SPI_AEP_SIZE_KB 1024
#define FIRMWARE_RECOVERY_IMAGE_SPI_BPS_SIZE_KB 2048
/**
  The maximum file size that a new firmware image can have - in bytes.
**/
#define MAX_FIRMWARE_IMAGE_SIZE_B        KIB_TO_BYTES(MAX_FIRMWARE_IMAGE_SIZE_KB)
#define FIRMWARE_SPI_IMAGE_AEP_SIZE_B    KIB_TO_BYTES(FIRMWARE_RECOVERY_IMAGE_SPI_AEP_SIZE_KB)
#define FIRMWARE_SPI_IMAGE_BPS_SIZE_B    KIB_TO_BYTES(FIRMWARE_RECOVERY_IMAGE_SPI_BPS_SIZE_KB)
// Keep this updated. Used for determining max input file size for now
#define MAX_FIRMWARE_SPI_IMAGE_SIZE_B    FIRMWARE_SPI_IMAGE_BPS_SIZE_B
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
  UINT8 Opcode;
  UINT8 SubOpcode;
  UINT8 TransportInterface;
  UINT8 Reserved[5];
  UINT32 Timeout;
  UINT8 Data[IN_PAYLOAD_SIZE];
} INPUT_PAYLOAD_SMBUS_OS_PASSTHRU;
#pragma pack(pop)

// Additional bytes to deal with DSM calls
#define IN_PAYLOAD_SIZE_EXT_PAD (sizeof(INPUT_PAYLOAD_SMBUS_OS_PASSTHRU) - IN_PAYLOAD_SIZE)

#pragma pack(push)
#pragma pack(1)
typedef struct {
  UINT32 InputPayloadSize;
  UINT32 LargeInputPayloadSize;
  UINT32 OutputPayloadSize;
  UINT32 LargeOutputPayloadSize;
  // Additional buffer for potential OS special passthrough
  // See use of SubopExtVendorSpecific in PassThru()
  UINT8 InputPayload[IN_PAYLOAD_SIZE + IN_PAYLOAD_SIZE_EXT_PAD];
  UINT8 LargeInputPayload[IN_MB_SIZE];
  UINT8 OutPayload[OUT_PAYLOAD_SIZE];
  UINT8 LargeOutputPayload[OUT_MB_SIZE];
  UINT32 DimmID;
  UINT8 Opcode;
  UINT8 SubOpcode;
  UINT8 Status;
#ifdef OS_BUILD
  UINT8 DsmStatus;
#endif
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
  VERSION_BYTE SecurityRevisionNumber; //!< cc
  VERSION_BYTE RevisionNumber;        //!< bb
  VERSION_BYTE ProductNumber;         //!< aa
} FW_VERSION;

/**
  FW Image header: Intel CSS Header (128 bytes)
**/
typedef struct {
  UINT32 ModuleType;         //!< moduleType = LT_MODULETYPE_CSS
  UINT32 HeaderLen;          //!< headerLen == dword_sizeof(fixedHeader) + (modulusSize * 2) + exponentSize
  UINT32 HeaderVersion;      //!< bits [31:16] are major version, bits [15:0] are minor version
  UINT32 ModuleID;           //!< if bit 31 == 1 this is a debug module
  UINT32 ModuleVendor;       //!< moduleVendor = 0x00008086
  DATE Date;                 //!< BCD format: yyyymmdd
  UINT32 Size;               //!< Size of entire module (header, crypto(modulus, exponent, signature), data) in DWORDs
  UINT32 KeySize;            //!< Size of RSA public key in DWORDs
  UINT32 ModulusSize;        //!< Size of RSA public key modulus in DWORDs
  UINT32 ExponentSize;       //!< Size of RSA public key exponent
  UINT8  ImageType;          //!< Image type: 0x1D - PRQ, 0x1E - DFX, 0x1F - DBG
  FW_VERSION ImageVersion;   //!< Image version
  UINT32 PerPartIdHigh;      //!< Part id high
  UINT32 PerPartIdLow;       //!< Part id low
  UINT32 ImageSize;          //!< Size of (header , signature , data) in DWORDs (ie size - 65 DWORDs)
  API_VERSION FwApiVersion;  //!< BCD format: aa.bb
  UINT8 StageNumber;         //!< Stage number: 0 - stage1 , 1 - stage 2
  UINT32 fwImageStartAddr;   //!< Firmware SRAM start address. This value must agree with actual image start produced by the .ld file. The fwImageStartAddr address MUST be 64 byte aligned.
  UINT16 VendorId;           //!< Specifies vendor. Intel (0x8086)
  UINT16 DeviceId;           //!< Specifies root arch type (ekv, bwv, cwv, etc .).
  UINT16 RevisionId;         //!< Specifies base and metal steppings of arch (A0, A1 , S0, S1, B0, B2, etc.).
  UINT8 NumberofStages;      //!< Specifies the number of expected stages to load. 1 - one stage to load , 2 - two stages to load
  UINT8 Reserved[56];
} FW_IMAGE_HEADER;

/**
  SPI Directory structure that holds SPI memory map
**/

// Copied March 2018 from spi_memory_map_ekvs1.h
#define SPI_DIRECTORY_VERSION 1


/**
  Tests a FW version to see if it is an undefined version

  @param  FwProduct
  @param  FwRevision
  @param  FwSecurityVersion
  @param  FwBuild

  @return TRUE if relevant fields are all 0.

**/
#define FW_VERSION_UNDEFINED_BYVERS(FwProduct, FwRevision, FwSecurityVersion, FwBuild) (FwProduct == 0 && \
                                              FwRevision == 0 && \
                                              FwSecurityVersion == 0 && \
                                              FwBuild == 0)

/**
  Tests a FW version to see if it is an undefined version

  @param  FirmareVersionStruct           a FIRMWARE_VERSION struct

  @return TRUE if relevant fields are all 0.

**/
#define FW_VERSION_UNDEFINED(FirmareVersionStruct) FW_VERSION_UNDEFINED_BYVERS(\
                                               FirmareVersionStruct.FwProduct, \
                                               FirmareVersionStruct.FwRevision, \
                                               FirmareVersionStruct.FwSecurityVersion, \
                                               FirmareVersionStruct.FwBuild)

typedef struct {
  UINT16 DirectoryVersion;
  UINT16 DirectorySize;
  UINT32 SoftFusesDataOffset;
  UINT32 DirectoryCopyOffset;        // Not used
  UINT32 MfgCypherOffset;
  UINT32 FwImageOffset;
  UINT32 FwImageDfxOffset;
  UINT32 SpdDataOffset;
  UINT32 MigrationDataOffset;
  UINT32 FwImageCopyOffset;
  UINT32 SxpSavedRegistersOffset;
  UINT32 Reserved_1;                 // was DdrtSavedRegistersOffset;
  UINT32 BurninInputDataOffset;
  UINT32 BurninOutputDataOffset;
  UINT32 SxpRankInterleavingOffset;
  UINT32 DdrtIoMmrcTableOffset;
  UINT32 SxpIoMmrcTableOffset;
  UINT32 Reserved_2;                 // was FwStateDataOffset;
  UINT32 FconfigDataOffset;
  UINT32 SxpTimingParametersOffset;
  UINT32 SxpTrainingReportOffset;
  UINT32 SxpRmtResultsOffset;
  UINT32 SxpRawTrainingDataOffset;
  UINT32 PreInjectionModuleFrameworkOffset;
  UINT32 reserved[8];
  UINT8  reservedu8[3];
  UINT8  SpiEndOfDirectory;
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
  The persistent memory module type code (taken from FW image)
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
  @param[in] SubsystemDeviceId is the identifer of the revision of Dimm (AEP vs BPS)
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
  IN     UINT16 SubsystemDeviceId,
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
  IN     BOOLEAN Recovery,
  IN     UINT16 SubsystemDeviceId,
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

/**
  Calculates the checksum of passed in buffer and keeps a running
  value. Used by CR FW for computing their checksum, used in our code for get/set
  fconfig data.

  @param[in]  pBuffer Pointer to buffer; whose checksum needs to be calculated.
  @param[in]  NumBytes Number of bytes inside buffer to calculate checksum on.
  @param[in]  Csum current running checksum.

  @retval The computed checksum
**/
UINT32 RunningChecksum(
  IN VOID *pBuffer,
  IN UINT32 NumBytes,
  IN UINT32 Csum
  );

#endif /** _FWUTILITY_H_ **/
