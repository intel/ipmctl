/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DUMPLOADPOOLS_H_
#define _DUMPLOADPOOLS_H_

#include "Uefi.h"
#include <Protocol/SimpleFileSystem.h>
#include <Dimm.h>
#include <Region.h>

#define NUMBER_OF_TOKENS_IN_DUMP_CONFIG_LINE_V1 10
#define NUMBER_OF_TOKENS_IN_DUMP_CONFIG_LINE_V2 12

enum DumpFilePrintCodes {
  DumpFileBom,
  DumpVersion,
  DumpDimmConfigDescription,
  DumpSeparation,
  DumpNewLine,
  DumpLastNotUsed
};

typedef struct _PERSISTENT_ENTRY {
  UINT64 PersistentSize;
  INTERLEAVE_FORMAT InterleaveFormat;
  UINT16 PersistentIndex;
} PERSISTENT_ENTRY;

typedef struct _DIMM_CONFIG {
  DIMM *pDimm;
  UINT16 Socket;
  NfitDeviceHandle DeviceHandle;
  UINT64 Capacity;
  UINT64 VolatileSize;
  PERSISTENT_ENTRY Persistent[MAX_IS_PER_DIMM];
  UINT16 LabelVersionMajor;
  UINT16 LabelVersionMinor;
} DIMM_CONFIG;

/**
  Write dump Pool Goal Configuration header to file

  @param[in] FileHandle File handle to write to

  @retval EFI_SUCCESS  Header written.
  @retval other  Error codes from Write function.
**/
EFI_STATUS
WriteDumpFileHeader(
  IN     EFI_FILE_HANDLE FileHandle
  );

/**
  Dump Pool Goal Configurations into file

  @param[in] pFileHandle File handler
  @param[in] DimmConfigs Array of DIMM_CONFIG
  @param[in] DimmConfigsNum Size of DimmConfigs array

  @retval EFI_SUCCESS Pool Goal Configuration dump successful
  @retval EFI_INVALID_PARAMETER Invalid Parameter during dump
  @retval EFI_OUT_OF_RESOURCES Not enough memory to dump
  @retval EFI_NO_MAPPING Wrong Pool Goal Configuration status
**/
EFI_STATUS
DumpConfigToFile(
  IN     EFI_FILE_HANDLE pFileHandle,
  IN     DIMM_CONFIG DimmConfigs[],
  IN     UINT32 DimmConfigsNum
  );

/**
  Set up pool goal structures to be loaded.

  @param[in] pDimms Array of Dimms
  @param[out] pDimmsConfig Array of Dimm Config
  @param[in] DimmsNum Number of elements in pDimms and pDimmsConfig
  @param[in] pFileString Contains Pool Goal Configuration
  @param[in, out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Configuration is valid and loaded properly
  @retval EFI_INVALID_PARAMETER Invalid Parameter during load
  @retval other Return Codes from TrimLineBuffer,
                GetLoadPoolData, GetLoadDimmData, GetLoadValue functions
**/
EFI_STATUS
SetUpGoalStructures(
  IN     DIMM *pDimms[],
     OUT DIMM_CONFIG DimmsConfig[],
  IN     UINT32 DimmsNum,
  IN     CHAR8 *pFileString,
  IN OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Retrieves Dimm Config data from string

  @param[in] pDataLine Pointer to string with Config Dimm data
  @param[out] pDimmConfigData Pointer structure where to write retrieved data

  @retval EFI_SUCCESS Dimm data retrieved successfully
  @retval EFI_INVALID_PARAMETER Invalid Parameter
  @retval EFI_OUT_OF_RESOURCES Could not allocate memory
**/
EFI_STATUS
GetLoadDimmConfigData(
  IN     CHAR8 *pDataLine,
     OUT DIMM_CONFIG *pDimmConfigData
  );

/**
  Get array of current config from configured dimms.

  The caller is responsible for freeing of ppDimmConfigsArg.

  @param[out] ppDimmConfigsArg Pointer to output array of DIMM_CONFIG
  @param[out] pDimmConfigsNumArg Pointer to output number of DIMM_CONFIG items

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER one or more parameters are invalid
**/
EFI_STATUS
GetDimmsCurrentConfig(
     OUT DIMM_CONFIG **ppDimmConfigsArg,
     OUT UINT32 *pDimmConfigsNumArg
  );

/**
  Verify config loaded from file and prepare goal config parameters for create goal config function

  @param[in] Socket Socket ID
  @param[in] DimmsConfig Array of Dimm Config
  @param[in] DimmsConfigNum Number of elements in DimmsConfig
  @param[out] DimmIds Array of DIMM IDs
  @param[out] pDimmIdsNum Number of items in array of DIMM IDs
  @param[out] pPersistentMemType Persistent memory type
  @param[out] pVolatilePercent Volatile region size in percents
  @param[out] pReservedPercent Reserved region size in percents
  @param[out] pLabelVersionMajor Label Major Version to use
  @param[out] pLabelVersionMinor Label Minor Version to use
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER one or more parameters are invalid
  @retval EFI_ABORTED improper configuration
**/
EFI_STATUS
ValidateAndPrepareLoadConfig(
  IN     UINT16 Socket,
  IN     DIMM_CONFIG DimmsConfig[],
  IN     UINT32 DimmsConfigNum,
     OUT UINT16 DimmIds[MAX_DIMMS_PER_SOCKET],
     OUT UINT32 *pDimmIdsNum,
     OUT UINT8 *pPersistentMemType,
     OUT UINT32 *pVolatilePercent,
     OUT UINT32 *pReservedPercent,
     OUT UINT16 *pLabelVersionMajor,
     OUT UINT16 *pLabelVersionMinor,
     OUT COMMAND_STATUS *pCommandStatus
  );

#endif /** _DUMPLOADPOOLS_H_ **/
