/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "DumpLoadRegions.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include "Utility.h"
#include "Namespace.h"
#include <Convert.h>

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

/**
  Write dump Pool Goal Configuration header to file

  @param[in] FileHandle File handle to write to

  @retval EFI_SUCCESS  Header written.
  @retval other  Error codes from Write function.
**/
EFI_STATUS
WriteDumpFileHeader(
  IN     EFI_FILE_HANDLE FileHandle
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR8 *pHeaderBuffer = NULL;

  pHeaderBuffer = AllocateZeroPool(MAX_LINE_BYTE_LENGTH);
  if (pHeaderBuffer == NULL) {
    NVDIMM_DBG("Could not allocate memory for Ascii buffer.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  AsciiSPrint(pHeaderBuffer, MAX_LINE_BYTE_LENGTH, "#SocketID,DimmHandle,Capacity,MemorySize,"
                                  "AppDirect1Size,AppDirect1Format,AppDirect1Mirrored,AppDirect1Index,"
                                  "AppDirect2Size,AppDirect2Format,AppDirect2Mirrored,AppDirect2Index\n");

  ReturnCode = WriteAsciiLine(FileHandle, pHeaderBuffer);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed in dump dimm config description.");
    goto Finish;
  }

Finish:
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index  = 0;
  CHAR8 *pLineBuffer = NULL;
  DIMM_CONFIG *pDimmConfig = NULL;

  if (pFileHandle == NULL || DimmConfigs == NULL) {
    NVDIMM_DBG("Invalid pointer in Dump function.");
    goto Finish;
  }

  pLineBuffer = AllocateZeroPool(MAX_LINE_BYTE_LENGTH);
  if (pLineBuffer == NULL) {
    NVDIMM_DBG("Could not allocate memory for unicode line buffer.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  /** Dump each dimm current config as a row in file **/
  for (Index = 0; Index < DimmConfigsNum; Index++) {
    pDimmConfig = &(DimmConfigs[Index]);

    ZeroMem(pLineBuffer, MAX_LINE_BYTE_LENGTH);

    /**
      Rounding down the capacities to match show goal implementation.
    **/
    pDimmConfig->Capacity = ROUNDDOWN(pDimmConfig->Capacity, BYTES_IN_GIBIBYTE);
    pDimmConfig->VolatileSize = ROUNDDOWN(pDimmConfig->VolatileSize, BYTES_IN_GIBIBYTE);

    /** Make sure that persistent sizes are divisible by GiB **/
    if (pDimmConfig->Capacity % BYTES_IN_GIBIBYTE != 0 ||
        pDimmConfig->VolatileSize % BYTES_IN_GIBIBYTE != 0 ||
        pDimmConfig->Persistent[FIRST_POOL_GOAL].PersistentSize % BYTES_IN_GIBIBYTE != 0 ||
        pDimmConfig->Persistent[SECOND_POOL_GOAL].PersistentSize % BYTES_IN_GIBIBYTE != 0) {
      NVDIMM_DBG("Config sizes are not aligned to GiB.");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    /** Prepare a line to save to file **/
    AsciiSPrint(pLineBuffer, MAX_LINE_BYTE_LENGTH, "%d,%d,%lld,%lld,%lld,%ld,%d,%d,%lld,%ld,%d,%d\n",
        pDimmConfig->Socket,
        pDimmConfig->DeviceHandle,
        BYTES_TO_GIB(pDimmConfig->Capacity),
        BYTES_TO_GIB(pDimmConfig->VolatileSize),
        BYTES_TO_GIB(pDimmConfig->Persistent[0].PersistentSize),
        pDimmConfig->Persistent[FIRST_POOL_GOAL].InterleaveFormat.AsUint32,
        (pDimmConfig->Persistent[FIRST_POOL_GOAL].Mirror) ? 1 : 0,
        pDimmConfig->Persistent[FIRST_POOL_GOAL].PersistentIndex,
        BYTES_TO_GIB(pDimmConfig->Persistent[SECOND_POOL_GOAL].PersistentSize),
        pDimmConfig->Persistent[SECOND_POOL_GOAL].InterleaveFormat.AsUint32,
        (pDimmConfig->Persistent[SECOND_POOL_GOAL].Mirror) ? 1 : 0,
        pDimmConfig->Persistent[SECOND_POOL_GOAL].PersistentIndex);

    /** Save the line to file **/
    ReturnCode = WriteAsciiLine(pFileHandle, pLineBuffer);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed in dump. DimmID 0x%04x.", pDimmConfig->pDimm->DeviceHandle.AsUint32);
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;
Finish:
  FREE_POOL_SAFE(pLineBuffer);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT32 DimmIndex = 0;
  UINT32 DimmConfigIndex = 0;
  DIMM_CONFIG CurrentDimmConfig;
  BOOLEAN Found = FALSE;
  UINT32 NumberOfLines = 0;
  CHAR8 **ppLinesBuffer = NULL;

  NVDIMM_ENTRY();

  SetMem(&CurrentDimmConfig, sizeof(CurrentDimmConfig), 0x0);

  if (DimmsConfig == NULL || pCommandStatus == NULL || pFileString == NULL) {
    NVDIMM_DBG("Invalid Pointer.");
    goto Finish;
  }

  // Split input file to lines (but ignore byte order mark)
  ppLinesBuffer = AsciiStrSplit(&pFileString[0], '\n', &NumberOfLines);
  if (ppLinesBuffer == NULL || NumberOfLines == 0) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (DimmIndex = 0; DimmIndex < DimmsNum; DimmIndex++) {

    Found = FALSE;

    for (Index = 1; Index < NumberOfLines; ++Index) {
      // Try to match line with Dimm Config
      ReturnCode = GetLoadDimmConfigData(ppLinesBuffer[Index], &CurrentDimmConfig);
      if (!EFI_ERROR(ReturnCode)) {
        if (pDimms[DimmIndex]->DeviceHandle.AsUint32 == CurrentDimmConfig.DeviceHandle.AsUint32 &&
           pDimms[DimmIndex]->SocketId == CurrentDimmConfig.Socket) {
           CurrentDimmConfig.pDimm = pDimms[DimmIndex];
           Found = TRUE;
           break;
        }
      }
    }

    if (Found) {
      if (DimmConfigIndex == DimmsNum) {
        NVDIMM_WARN("There are two or more the same Device Handle in the file.");
        ReturnCode = EFI_ABORTED;
        ResetCmdStatus(pCommandStatus, NVM_ERR_LOAD_INVALID_DATA_IN_FILE);
        goto Finish;
      }
      DimmsConfig[DimmConfigIndex] = CurrentDimmConfig;
      DimmConfigIndex++;
    } else {
      NVDIMM_WARN("Specified DIMM has no match in the file.");
      ReturnCode = EFI_ABORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_LOAD_INVALID_DATA_IN_FILE);
      goto Finish;
    }
  }

  if (DimmConfigIndex == DimmsNum) {
    ReturnCode = EFI_SUCCESS;
  } else {
    NVDIMM_WARN("Not all specified DIMMs have match in the file.");
    ReturnCode = EFI_ABORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_LOAD_DIMM_COUNT_MISMATCH);
    goto Finish;
  }

Finish:
  for (Index = 0; ppLinesBuffer != NULL && Index < NumberOfLines; ++Index) {
    FREE_POOL_SAFE(ppLinesBuffer[Index]);
  }
  FREE_POOL_SAFE(ppLinesBuffer);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT64 LineLength = 0;
  CHAR8 *pTrimmedLine = NULL;
  CHAR8 **ppTokenizedLine = NULL;
  UINT32 TokensCount = 0;
  UINT64 TmpValue = 0;
  NVDIMM_ENTRY();

  if (pDataLine == NULL || pDimmConfigData == NULL) {
    NVDIMM_DBG("Invalid pointer");
    goto Finish;
  }

  // Allocate memory
  pTrimmedLine = AllocateZeroPool(MAX_LINE_BYTE_LENGTH);
  if (pTrimmedLine == NULL) {
    NVDIMM_DBG("Failed to allocate memory.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Strip white characters
  LineLength = MAX_LINE_CHAR_LENGTH;
  ReturnCode = RemoveWhiteSpaces(pDataLine, pTrimmedLine, &LineLength);
  if (EFI_ERROR(ReturnCode) || LineLength == 0) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Skip comments
  if (pTrimmedLine[0] == '#') {
    NVDIMM_DBG("Comment skipped.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Split line into tokens by ','
  ppTokenizedLine = AsciiStrSplit(pTrimmedLine, ',', &TokensCount);
  if (((TokensCount != NUMBER_OF_TOKENS_IN_DUMP_CONFIG_LINE_V1) &&
       (TokensCount != NUMBER_OF_TOKENS_IN_DUMP_CONFIG_LINE_V2)) ||
      ppTokenizedLine == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  //Iterate over all tokens
  for (Index = 0; Index < TokensCount; Index++) {
    TmpValue = AsciiStrDecimalToUint64(ppTokenizedLine[Index]);

    switch (Index) {
      case 0: // Socket
        pDimmConfigData->Socket = (UINT16) TmpValue;
        break;
      case 1:// DeviceHandle
        pDimmConfigData->DeviceHandle.AsUint32 = (UINT32) TmpValue;
        break;
      case 2:// Capacity
        pDimmConfigData->Capacity = (UINT64) TmpValue;
        break;
      case 3:// VolatileSize
        pDimmConfigData->VolatileSize = (UINT64) TmpValue;
        break;
      case 4:// Persistent1Size
        pDimmConfigData->Persistent[0].PersistentSize = (UINT64) TmpValue;
        break;
      case 5:// Persistent1Format
        pDimmConfigData->Persistent[0].InterleaveFormat.AsUint32 = (UINT32) TmpValue;
        break;
      case 6:// Persistent1Mirrored
        pDimmConfigData->Persistent[0].Mirror = (TmpValue == 1) ? TRUE : FALSE;
        break;
      case 7:// Persistent1Index
        pDimmConfigData->Persistent[0].PersistentIndex = (UINT16) TmpValue;
        break;
      case 8:// Persistent2Size
        pDimmConfigData->Persistent[1].PersistentSize = (UINT64) TmpValue;
        break;
      case 9:// Persistent2Format
        pDimmConfigData->Persistent[1].InterleaveFormat.AsUint32 = (UINT32) TmpValue;
        break;
      case 10:// Persistent2Mirrored
        pDimmConfigData->Persistent[1].Mirror = (TmpValue == 1) ? TRUE : FALSE;
        break;
      case 11:// Persistent2Index
        pDimmConfigData->Persistent[1].PersistentIndex = (UINT16) TmpValue;
        break;
      case 12:// LabelVersionMajor
        pDimmConfigData->LabelVersionMajor = (UINT16) TmpValue;
        break;
      case 13:// LabelVersionMinor
        pDimmConfigData->LabelVersionMinor = (UINT16) TmpValue;
        break;
      default:
        NVDIMM_DBG("Invalid number of separators ',' and fields in file.");
        goto Finish;
    }
  }

  // If using older dump without label versions, set to default 1.2
  if (TokensCount == NUMBER_OF_TOKENS_IN_DUMP_CONFIG_LINE_V1) {
    pDimmConfigData->LabelVersionMajor = NSINDEX_MAJOR;
    pDimmConfigData->LabelVersionMinor = NSINDEX_MINOR_2;
  }

  ReturnCode = EFI_SUCCESS;
Finish:
  for (Index = 0; ppTokenizedLine != NULL && Index < TokensCount; ++Index) {
    FREE_POOL_SAFE(ppTokenizedLine[Index]);
  }
  FREE_POOL_SAFE(ppTokenizedLine);
  FREE_POOL_SAFE(pTrimmedLine);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pDimmNode = NULL;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmRegionNode = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  DIMM_CONFIG *pDimmConfigs = NULL;
  UINT32 DimmConfigsNum = 0;
  NVM_IS *pIS = NULL;
  NVM_IS *pISs[MAX_IS_CONFIGS];
  UINT32 ISsNum = 0;
  BOOLEAN AssignedAlready = FALSE;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 Index3 = 0;
  PERSISTENT_ENTRY PersistentTmp;
  BOOLEAN HasLatestLabelVersion = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(pISs, sizeof(pISs));
  ZeroMem(&PersistentTmp, sizeof(PersistentTmp));

  if (ppDimmConfigsArg == NULL || pDimmConfigsNumArg == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  /** Count a number of configured dimms **/
  DimmConfigsNum = 0;
  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (pDimm == NULL) {
      ReturnCode = EFI_ABORTED;
      goto FinishError;
    }

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    if (pDimm->Configured) {
      DimmConfigsNum++;
    }
  }

  if (DimmConfigsNum == 0) {
    *ppDimmConfigsArg = NULL;
    *pDimmConfigsNumArg = 0;
    goto Finish;
  }

  pDimmConfigs = (DIMM_CONFIG *) AllocateZeroPool(sizeof(*pDimmConfigs) * DimmConfigsNum);
  if (pDimmConfigs == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto FinishError;
  }

  Index = 0;
  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (pDimm == NULL) {
      ReturnCode = EFI_ABORTED;
      goto FinishError;
    }

    if (!IsDimmManageable(pDimm) || !pDimm->Configured) {
      continue;
    }

    pDimmConfigs[Index].pDimm = pDimm;
    pDimmConfigs[Index].Socket = pDimm->SocketId;
    pDimmConfigs[Index].DeviceHandle = pDimm->DeviceHandle;
    pDimmConfigs[Index].Capacity = pDimm->RawCapacity;
    pDimmConfigs[Index].VolatileSize = pDimm->VolatileCapacity;

    ReturnCode = CheckDimmNsLabelVersion(pDimm, &HasLatestLabelVersion);
    // If not ns label version, set it to default 1.2
    if (EFI_ERROR(ReturnCode) || (HasLatestLabelVersion)) {
      pDimmConfigs[Index].LabelVersionMajor = NSINDEX_MAJOR;
      pDimmConfigs[Index].LabelVersionMinor = NSINDEX_MINOR_2;
    } else {
      pDimmConfigs[Index].LabelVersionMajor = NSINDEX_MAJOR;
      pDimmConfigs[Index].LabelVersionMinor = NSINDEX_MINOR_1;
    }

    for (Index2 = 0; Index2 < pDimm->ISsNum; Index2++) {
      pIS = pDimm->pISs[Index2];

      LIST_FOR_EACH(pDimmRegionNode, &pIS->DimmRegionList) {
        pDimmRegion = DIMM_REGION_FROM_NODE(pDimmRegionNode);

        if (pDimm->SerialNumber == pDimmRegion->pDimm->SerialNumber) {
          pDimmConfigs[Index].Persistent[Index2].PersistentSize = pDimmRegion->PartitionSize;
        }
      }

      ASSERT(pDimmConfigs[Index].Persistent[Index2].PersistentSize != 0);

      pDimmConfigs[Index].Persistent[Index2].InterleaveFormat.InterleaveFormatSplit.iMCInterleaveSize =
          pIS->InterleaveFormatImc;
      pDimmConfigs[Index].Persistent[Index2].InterleaveFormat.InterleaveFormatSplit.ChannelInterleaveSize =
          pIS->InterleaveFormatChannel;
      pDimmConfigs[Index].Persistent[Index2].InterleaveFormat.InterleaveFormatSplit.NumberOfChannelWays =
          pIS->InterleaveFormatWays;
      pDimmConfigs[Index].Persistent[Index2].Mirror = pIS->MirrorEnable;

      /** Index of interleave sets assigning **/
      AssignedAlready = FALSE;
      for (Index3 = 0; Index3 < ISsNum; Index3++) {
        if (pISs[Index3] == pIS) {
          AssignedAlready = TRUE;
          break;
        }
      }
      if (AssignedAlready) {
        pDimmConfigs[Index].Persistent[Index2].PersistentIndex = (UINT16)(Index3 + 1);
      } else {
        /** Assign new index. **/
        pISs[ISsNum] = pIS;
        ISsNum++;
        pDimmConfigs[Index].Persistent[Index2].PersistentIndex = (UINT16)ISsNum;
      }
    }

    Index++;
  }

  /**
    Adjust position of persistent entries of the same interleave set to set them the same index in array.
    It causes that one interleave set will be in the same column in the dump file. Applicable only for
    more than one interleave set request.
  **/
  if (ISsNum > 1) {
    for (Index = 0; Index < DimmConfigsNum; Index++) {
      for (Index2 = Index + 1; Index2 < DimmConfigsNum; Index2++) {
        if (((pDimmConfigs[Index].Persistent[0].PersistentIndex == pDimmConfigs[Index2].Persistent[1].PersistentIndex) && (pDimmConfigs[Index].Persistent[0].PersistentIndex != 0)) ||
            ((pDimmConfigs[Index].Persistent[1].PersistentIndex == pDimmConfigs[Index2].Persistent[0].PersistentIndex) && (pDimmConfigs[Index].Persistent[1].PersistentIndex != 0))) {
          PersistentTmp = pDimmConfigs[Index2].Persistent[1];
          pDimmConfigs[Index2].Persistent[1] = pDimmConfigs[Index2].Persistent[0];
          pDimmConfigs[Index2].Persistent[0] = PersistentTmp;
        }
      }
    }
  }

  /** if everything went successfully, assign data to output variables **/
  *ppDimmConfigsArg = pDimmConfigs;
  *pDimmConfigsNumArg = DimmConfigsNum;
  goto Finish;

FinishError:
  if (ppDimmConfigsArg != NULL) {
    FREE_POOL_SAFE(*ppDimmConfigsArg);
  }
  if (pDimmConfigsNumArg != NULL) {
    *pDimmConfigsNumArg = 0;
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  DIMM_CONFIG *pDimmsConfigOnSocket[MAX_DIMMS_PER_SOCKET];
  UINT32 SpecifiedDimmsOnSocketNum = 0;
  UINT64 DimmsCapacity = 0;
  UINT64 VolatileCapacity = 0;
  UINT64 ReservedCapacity = 0;
  BOOLEAN Storage = TRUE;
  BOOLEAN AppDirectInterlaved = FALSE;
  UINT16 TempMajor = 0;
  UINT16 TempMinor = 0;
  BOOLEAN UseDefaultLabel = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(pDimmsConfigOnSocket, sizeof(pDimmsConfigOnSocket));

  if (DimmsConfig == NULL || DimmIds == NULL || pDimmIdsNum == NULL ||
      pPersistentMemType == NULL || pVolatilePercent == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  for (Index = 0, SpecifiedDimmsOnSocketNum = 0; Index < DimmsConfigNum; Index++) {
    if (DimmsConfig[Index].pDimm->SocketId == Socket) {
      ASSERT(SpecifiedDimmsOnSocketNum < MAX_DIMMS_PER_SOCKET);

      /** Limited dimms config only to specified socket's dimms **/
      pDimmsConfigOnSocket[SpecifiedDimmsOnSocketNum] = &DimmsConfig[Index];

      /** Preparing list of dimm IDs for create goal command **/
      DimmIds[SpecifiedDimmsOnSocketNum] = DimmsConfig[Index].pDimm->DimmID;

      SpecifiedDimmsOnSocketNum++;
    }
  }

  if (SpecifiedDimmsOnSocketNum > MAX_DIMMS_PER_SOCKET) {       // sanity check
    ReturnCode = EFI_ABORTED;
    goto FinishClean;
  }

  if (SpecifiedDimmsOnSocketNum > 0) {
    for (Index = 0; Index < SpecifiedDimmsOnSocketNum && Storage; Index++) {
      DimmsCapacity += pDimmsConfigOnSocket[Index]->Capacity;
      VolatileCapacity += pDimmsConfigOnSocket[Index]->VolatileSize;

      if (pDimmsConfigOnSocket[Index]->Capacity
        > pDimmsConfigOnSocket[Index]->VolatileSize
          + pDimmsConfigOnSocket[Index]->Persistent[0].PersistentSize
          + pDimmsConfigOnSocket[Index]->Persistent[1].PersistentSize)
      {
        ReservedCapacity += pDimmsConfigOnSocket[Index]->Capacity
          - pDimmsConfigOnSocket[Index]->VolatileSize
          - pDimmsConfigOnSocket[Index]->Persistent[0].PersistentSize
          - pDimmsConfigOnSocket[Index]->Persistent[1].PersistentSize;
      }

      if (pDimmsConfigOnSocket[Index]->Persistent[0].PersistentSize > 0) {
        Storage = FALSE;
      }

      for (Index2 = 0; Index2 < SpecifiedDimmsOnSocketNum && !AppDirectInterlaved; Index2++) {
        if (Index == Index2) {
          continue;
        }

        if (pDimmsConfigOnSocket[Index]->Persistent[0].PersistentIndex ==
            pDimmsConfigOnSocket[Index2]->Persistent[0].PersistentIndex) {
          AppDirectInterlaved = TRUE;
        }
      }
    }

    if (DimmsCapacity == 0 || VolatileCapacity > DimmsCapacity) {
      ReturnCode = EFI_ABORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_LOAD_IMPROPER_CONFIG_IN_FILE);
      goto FinishClean;
    } else {
      // This step rounds up the percentage to the ceiling. Example: if the percent says 26.3, we will convert
      // that to 27 before giving it to driver as the driver will adjust the capacity later rounding it down.
      *pVolatilePercent = (UINT32) ((ROUNDUP((VolatileCapacity * 100), DimmsCapacity)) / DimmsCapacity);

      *pReservedPercent = (UINT32) ((ROUNDDOWN((ReservedCapacity * 100), DimmsCapacity)) / DimmsCapacity);
    }

    if (Storage) {
      *pPersistentMemType = PM_TYPE_STORAGE;
    } else if (AppDirectInterlaved) {
      *pPersistentMemType = PM_TYPE_AD;
    } else {
      *pPersistentMemType = PM_TYPE_AD_NI;
    }
  } else {
    /** No DIMMs specified on that socket, so it's also successful path. **/
  }

  /** Label Version sanity check. Use default version if there are invalid inputs **/
  TempMajor = DimmsConfig[0].LabelVersionMajor;
  TempMinor = DimmsConfig[0].LabelVersionMinor;

  if ((TempMajor != NSINDEX_MAJOR) ||
      ((TempMinor != NSINDEX_MINOR_1) && (TempMinor != NSINDEX_MINOR_2))) {
    UseDefaultLabel = TRUE;
  }

  /** If versions on dimms don't match **/
  for (Index = 1; Index < DimmsConfigNum; Index++) {
    if ((DimmsConfig[Index].LabelVersionMajor != TempMajor) &&
        (DimmsConfig[Index].LabelVersionMinor != TempMinor)) {
  UseDefaultLabel = TRUE;
    }
  }

  if (UseDefaultLabel) {
    *pLabelVersionMajor = NSINDEX_MAJOR;
    *pLabelVersionMinor = NSINDEX_MINOR_2;
  } else {
    *pLabelVersionMajor = TempMajor;
    *pLabelVersionMinor = TempMinor;
  }

  *pDimmIdsNum = SpecifiedDimmsOnSocketNum;

  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishClean:
  /** Clean all set data **/
  ZeroMem(DimmIds, sizeof(DimmIds[0]) * MAX_DIMMS_PER_SOCKET);
  *pDimmIdsNum = 0;
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
