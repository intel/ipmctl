/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FW_UTILITY_H_
#define _FW_UTILITY_H_

#include "NvmTypes.h"
#include "NvmStatus.h"

#define MAX_FIRMWARE_IMAGE_SIZE_KB 788
#define FIRMWARE_RECOVERY_IMAGE_SPI_GEN1_SIZE_KB 1024
#define FIRMWARE_RECOVERY_IMAGE_SPI_GEN2_SIZE_KB 2048
/**
  The maximum file size that a new firmware image can have - in bytes.
**/
#define MAX_FIRMWARE_IMAGE_SIZE_B        KIB_TO_BYTES(MAX_FIRMWARE_IMAGE_SIZE_KB)
#define FIRMWARE_SPI_IMAGE_GEN1_SIZE_B    KIB_TO_BYTES(FIRMWARE_RECOVERY_IMAGE_SPI_GEN1_SIZE_KB)
#define FIRMWARE_SPI_IMAGE_GEN2_SIZE_B    KIB_TO_BYTES(FIRMWARE_RECOVERY_IMAGE_SPI_GEN2_SIZE_KB)

// Keep this updated. Used for determining max input file size for now
#define MAX_FIRMWARE_SPI_IMAGE_SIZE_B    FIRMWARE_SPI_IMAGE_GEN2_SIZE_B
#define NO_FW_GIVEN_VERSION_MSG           L"None"

#define UPDATE_FIRMWARE_SMALL_PAYLOAD_DATA_PACKET_SIZE  64

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

/** Quiesce required values **/
#define QUIESCE_NOT_REQUIRED 0
#define QUIESCE_REQUIRED     1

/** Staged fw activatable values **/
#define STAGED_FW_NOT_ACTIVATABLE 0
#define STAGED_FW_ACTIVATABLE 1

#define IN_MB_SIZE          (1 << 20)   //!< Size of the OS mailbox large input payload
#define OUT_MB_SIZE         (1 << 20)   //!< Size of the OS mailbox large output payload
#define IN_PAYLOAD_SIZE     (128)       //!< Total size of the input payload registers
#define OUT_PAYLOAD_SIZE    (128)       //!< Total size of the output payload registers

#define MB_COMPLETE 0x1
#define STATUS_MASK 0xFF

#define DETAILS_STR                            L"Details: "
#define DETAILS_CANT_USE_IMAGE                 DETAILS_STR L"Can't use " FORMAT_STR L"on " FORMAT_STR
#define DETAILS_SVNDE_NOT_ENABLED              DETAILS_STR L"SVNDE is not enabled"
#define DETAILS_REVISION_NUMBER_MISMATCH       DETAILS_STR L"Revision number mismatch"
#define DETAILS_PRODUCT_NUMBER_MISMATCH        DETAILS_STR L"Product number mismatch"
#define DETAILS_FILE_READ_ERROR                DETAILS_STR L"Could not read the file"
#define DETAILS_MEM_ALLOCATION_ERROR_FW_IMG    DETAILS_STR L"Could not allocate memory for the firmware image"
#define DETAILS_FILE_TOO_SMALL                 DETAILS_STR L"The file is too small"
#define DETAILS_FILE_TOO_LARGE                 DETAILS_STR L"The file is too large"
#define DETAILS_UNKNOWN_SUBSYSTEM_DEVICE       DETAILS_STR L"Subsystem Device Id is unknown"
#define DETAILS_FILE_IMG_INFO_NOT_ACCESSIBLE   DETAILS_STR L"Could not get the file information"
#define DETAILS_FILE_NOT_VALID                 DETAILS_STR L"The specified source file is not valid"
#define DETAILS_WRONG_IMAGE_SIZE               DETAILS_STR L"Wrong image size"
#define DETAILS_IMAGE_NOT_ALIGNED              DETAILS_STR L"Wrong data size - buffer not aligned"
#define DETAILS_MODULE_TYPE_NOT_COMPATIBLE     DETAILS_STR L"Module type not compatible"
#define DETAILS_VENDOR_NOT_COMPATIBLE          DETAILS_STR L"Vendor not compatible"

#pragma pack(push)
#pragma pack(1)
typedef struct {
  UINT8 Opcode;
  UINT8 SubOpcode;
  UINT8 TransportInterface;
  UINT8 Reserved[5];
  UINT32 Timeout;
  UINT8 Data[IN_PAYLOAD_SIZE];
} NVM_INPUT_PAYLOAD_SMBUS_OS_PASSTHRU;
#pragma pack(pop)

// Additional bytes to deal with DSM calls
#define IN_PAYLOAD_SIZE_EXT_PAD (sizeof(NVM_INPUT_PAYLOAD_SMBUS_OS_PASSTHRU) - IN_PAYLOAD_SIZE)

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
} NVM_FW_CMD;

#pragma pack(pop)

// FW commands that are supposed to work even if the module firmware is unresponsive
#define FW_CMD_INTERFACE_INDEPENDENT(Opcode, SubOpcode) (Opcode == PtEmulatedBiosCommands && SubOpcode == SubopGetBSR)

/**
  Version struct definition
**/
typedef union {
  struct {
    UINT8 Digit2:4;
    UINT8 Digit1:4;
  } Nibble;
  UINT8 Version;
} NVM_VERSION_BYTE;

typedef union {
  struct {
    UINT16 Digit4:4;
    UINT16 Digit3:4;
    UINT16 Digit2:4;
    UINT16 Digit1:4;
  } Nibble;
  UINT16 Build;
} NVM_BUILD_WORD;

typedef union {
   struct {
    UINT8 Digit2;
    UINT8 Digit1;
  } Byte;
  UINT16 Version;
} NVM_API_VERSION;

typedef union {
  struct {
    UINT16 Year:16;
    UINT8 Month:8;
    UINT8 Day:8;
  } Separated;
  UINT32 Value;
} NVM_DATE;

/**
  All BCD encoded aa.bb.cc.dddd
**/
#pragma pack(push)
#pragma pack(1)
typedef struct _NVM_FW_VERSION {
  NVM_BUILD_WORD BuildNumber;             //!< dddd
  NVM_VERSION_BYTE SecurityRevisionNumber; //!< cc
  NVM_VERSION_BYTE RevisionNumber;        //!< bb
  NVM_VERSION_BYTE ProductNumber;         //!< aa
} NVM_FW_VERSION;

/**
  FW Image header: Intel CSS Header (128 bytes)
**/
typedef struct {
  UINT32 ModuleType;         //!< moduleType = LT_MODULE_TYPE_CSS
  UINT32 HeaderLen;          //!< headerLen == dword_sizeof(fixedHeader) + (modulusSize * 2) + exponentSize
  UINT32 HeaderVersion;      //!< bits [31:16] are major version, bits [15:0] are minor version
  UINT32 ModuleID;           //!< if bit 31 == 1 this is a debug module
  UINT32 ModuleVendor;       //!< moduleVendor = 0x00008086
  NVM_DATE Date;                 //!< BCD format: yyyymmdd
  UINT32 Size;               //!< Size of entire module (header, crypto(modulus, exponent, signature), data) in DWORDs
  UINT32 KeySize;            //!< Size of RSA public key in DWORDs
  UINT32 ModulusSize;        //!< Size of RSA public key modulus in DWORDs
  UINT32 ExponentSize;       //!< Size of RSA public key exponent
  UINT8  ImageType;          //!< Image type: 0x1D - PRQ, 0x1E - DFX, 0x1F - DBG
  NVM_FW_VERSION ImageVersion;   //!< Image version
  UINT32 PerPartIdHigh;      //!< Part id high
  UINT32 PerPartIdLow;       //!< Part id low
  UINT32 ImageSize;          //!< Size of (header , signature , data) in DWORDs (ie size - 65 DWORDs)
  NVM_API_VERSION FwApiVersion;  //!< BCD format: aa.bb
  UINT8 StageNumber;         //!< Stage number: 0 - stage1 , 1 - stage 2
  UINT32 fwImageStartAddr;   //!< Firmware SRAM start address. This value must agree with actual image start produced by the .ld file. The fwImageStartAddr address MUST be 64 byte aligned.
  UINT16 VendorId;           //!< Specifies vendor. Intel (0x8086)
  UINT16 DeviceId;           //!< Specifies root arch type (ekv, bwv, cwv, etc .).
  UINT16 RevisionId;         //!< Specifies base and metal steppings of arch (A0, A1 , S0, S1, B0, B2, etc.).
  UINT8 NumberOfStages;      //!< Specifies the number of expected stages to load. 1 - one stage to load , 2 - two stages to load
  UINT8 Reserved[56];
} NVM_FW_IMAGE_HEADER;

/**
  SPI Directory structure that holds SPI memory map
**/

// Copied March 2018 from spi_memory_map_ekvs1.h
#define SPI_DIRECTORY_VERSION_GEN1 1
// Copied August 2019 from spi_memory_map_bwv.h
#define SPI_DIRECTORY_VERSION_GEN2 2


/**
  Tests a FW version to see if it is an undefined version

  @param  FwProduct
  @param  FwRevision
  @param  FwSecurityVersion
  @param  FwBuild

  @return TRUE if relevant fields are all 0.

**/
#define FW_VERSION_UNDEFINED_BY_VERSION(FwProduct, FwRevision, FwSecurityVersion, FwBuild) (FwProduct == 0 && \
                                              FwRevision == 0 && \
                                              FwSecurityVersion == 0 && \
                                              FwBuild == 0)

/**
  Tests a FW version to see if it is an undefined version

  @param  FirmwareVersionStruct           a FIRMWARE_VERSION struct

  @return TRUE if relevant fields are all 0.

**/
#define FW_VERSION_UNDEFINED(FirmwareVersionStruct) FW_VERSION_UNDEFINED_BY_VERSION(\
                                               FirmwareVersionStruct.FwProduct, \
                                               FirmwareVersionStruct.FwRevision, \
                                               FirmwareVersionStruct.FwSecurityVersion, \
                                               FirmwareVersionStruct.FwBuild)

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
} NVM_SPI_DIRECTORY_GEN1;

typedef struct
{
  UINT16 DirectoryVersion;
  UINT16 DirectorySize;
  UINT32 SoftFusesDataOffset;
  UINT32 FwImageStage1Offset;
  UINT32 FwImageStage2Offset;
  UINT32 FwImageCopyStage1Offset;
  UINT32 FwImageCopyStage2Offset;
  UINT32 FwImageDfxStage1Offset;
  UINT32 FwImageDfxStage2Offset;
  UINT32 MigrationDataOffset;
  UINT32 SxpSavedRegistersOffset;
  UINT32 SxpTrainingReportOffset;
  UINT32 SxpRmtResultsOffset;
  UINT32 SxpRawTrainingDataOffset;
  UINT32 BurninInputDataOffset;
  UINT32 BurninOutputDataOffset;
  UINT32 FconfigDataOffset;
  UINT32 SxpTimingParametersOffset;
  UINT32 PreInjectionModuleFrameworkOffset;
  UINT32 SpiDebugDataOffset;
  UINT32 NlogBackupOffset;
} NVM_SPI_DIRECTORY_GEN2;

#pragma pack(pop)



/**
  FW Image header information
**/
typedef struct {
  NVM_FW_VERSION ImageVersion;
  UINT8 FirmwareType;
  UINT32 ModuleVendor;  //!< moduleVendor = 0x00008086
  NVM_DATE Date;            //!< BCD format: yyyymmdd
  UINT32 Size;          //!< Size of entire module (header, crypto, data) in DWORDs
} NVM_FW_IMAGE_INFO;

/**
  The persistent memory module type code (taken from FW image)
**/
#define LT_MODULE_TYPE_CSS 0x6

/**
  Checks if the firmware image is valid for an update.
  Please note that even if this function returns TRUE, the image may not be fully
  valid. There are additional security and CRC checks made by the DIMM that may fail.

  @param[in] pImage is the buffer that contains the image we want to validate.
  @param[in] ImageSize is the size in bytes of the valid image data in the buffer.
    The buffer must be bigger or equal to the ImageSize.
  @param[in] FWImageMaxSize is the maximum allowed size in bytes of the image.
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
  IN     UINT64 FWImageMaxSize,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Checks if the firmware image is valid for an recovery.
  Please note that even if this function returns TRUE, the image may not be fully
  valid. There are additional security and CRC checks made by the DIMM that may fail.

  @param[in] pImage is the buffer that contains the image we want to validate.
  @param[in] ImageSize is the size in bytes of the valid image data in the buffer.
    The buffer must be bigger or equal to the ImageSize.
  @param[in] FWImageMaxSize is the maximum allowed size in bytes of the image.
  @param[in] SubsystemDeviceId is the identifier of the Dimm revision

  @retval TRUE if the Image is valid for the update.
  @retval FALSE if the Image is not valid.
**/
BOOLEAN
ValidateRecoverySpiImage(
  IN     NVM_FW_IMAGE_HEADER *pImage,
  IN     UINT64 ImageSize,
  IN     UINT64 FWImageMaxSize,
  IN     UINT16 SubsystemDeviceId
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
  IN     BOOLEAN Recovery,
  IN     UINT16 SubsystemDeviceId,
  IN     UINT64 FWImageMaxSize,
     OUT NVM_FW_IMAGE_HEADER **ppImageHeader,
     OUT COMMAND_STATUS *pCommandStatus
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

#endif /** _FW_UTILITY_H_ **/
