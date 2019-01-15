/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Debug.h>
#include <Types.h>
#include "AcpiParsing.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <NvmWorkarounds.h>
#include <PlatformConfigData.h>
#include <ShowAcpi.h>
#include <NvmDimmDriver.h>

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

GUID gSpaRangeVolatileRegionGuid = SPA_RANGE_VOLATILE_REGION_GUID;

GUID gSpaRangePmRegionGuid = SPA_RANGE_PM_REGION_GUID;

GUID gSpaRangeControlRegionGuid = SPA_RANGE_CONTROL_REGION_GUID;

GUID gSpaRangeBlockDataWindowRegionGuid = SPA_RANGE_BLOCK_DATA_WINDOW_REGION_GUID;

GUID gSpaRangeRawVolatileRegionGuid = SPA_RANGE_RAW_VOLATILE;

GUID gSpaRangeIsoVolatileRegionGuid = SPA_RANGE_ISO_VOLATILE;

GUID gSpaRangeRawPmRegionGuid = SPA_RANGE_RAW_PM;

GUID gSpaRangeIsoPmRegionGuid = SPA_RANGE_ISO_PM;

GUID gAppDirectPmTypeGuid = APPDIRECT_PM_TYPE;

GUID gSpaRangeMailboxCustomGuid = SPA_RANGE_MAILBOX_CUSTOM_GUID;

/**
  CopyMemoryAndAddPointerToArray - Copies the data and adds the result pointer to an array of pointers.

  @param[in, out] ppTable pointer to the pointers. Warning! This pointer will be freed.
  @param[in] pToAdd pointer to the data that the caller wants to add to the array.
  @param[in] DataSize size of the data that are supposed to be copied.
  @param[in] NewPointerIndex index in the table that the new pointer should have.

  @retval NULL - if a memory allocation failed.
  @retval pointer to the new array of pointers (with the new one at the end).
**/
STATIC
VOID **
CopyMemoryAndAddPointerToArray(
  IN OUT VOID **ppTable,
  IN     VOID *pToAdd,
  IN     UINT32 DataSize,
  IN     UINT32 *pNewPointerIndex
  );

/**
  ParseNfitTable - Performs deserialization from binary memory block into parsed structure of pointers.

  @param[in] pTable pointer to the memory containing the NFIT binary representation.

  @retval NULL if there was an error while parsing the memory.
  @retval pointer to the allocated header with parsed NFIT.
**/
ParsedFitHeader *
ParseNfitTable(
  IN     VOID *pTable
  )
{
  ParsedFitHeader *pParsedHeader = NULL;
  NFitHeader *pNFit = NULL;
  UINT8 *pTabPointer = NULL;
  SubTableHeader *pTableHeader = NULL;
  UINT32 RemainingNFITBytes = 0;
  //UINT32 Index = 0;
  //UINT32 NumOfDimms = 0;

  NVDIMM_ENTRY();

  if (pTable == NULL) {
    NVDIMM_DBG("The NFIT pointer is NULL.");
    goto FinishError;
  }

  pNFit = (NFitHeader *)pTable;

  pTabPointer = (UINT8 *)pTable + sizeof(NFitHeader);
  pTableHeader = (SubTableHeader *)pTabPointer;
  RemainingNFITBytes = pNFit->Header.Length - sizeof(*pNFit);
  pParsedHeader = (ParsedFitHeader *)AllocateZeroPool(sizeof(*pParsedHeader));

  if (pParsedHeader == NULL) {
    NVDIMM_DBG("Could not allocate memory.");
    goto FinishError;
  }

  pParsedHeader->pFit = (NFitHeader *)AllocateZeroPool(sizeof(NFitHeader));

  if (pParsedHeader->pFit == NULL) {
    NVDIMM_DBG("Could not allocate memory.");
    goto FinishError;
  }

  CopyMem_S(pParsedHeader->pFit, sizeof(NFitHeader), pNFit, sizeof(NFitHeader));

  while (RemainingNFITBytes > 0) {
    if (pTableHeader->Length == 0) {
      NVDIMM_DBG("Zero size entry found in nfit region.");
      goto FinishError;
    }

    RemainingNFITBytes -= pTableHeader->Length;

    switch(pTableHeader->Type) {
    case NVDIMM_SPA_RANGE_TYPE:
      pParsedHeader->ppSpaRangeTbles = (SpaRangeTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppSpaRangeTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->SpaRangeTblesNum);
      if (pParsedHeader->ppSpaRangeTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_NVDIMM_REGION_TYPE:
      pParsedHeader->ppNvDimmRegionTbles = (NvDimmRegionTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppNvDimmRegionTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->NvDimmRegionTblesNum);
      if (pParsedHeader->ppNvDimmRegionTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_INTERLEAVE_TYPE:
      pParsedHeader->ppInterleaveTbles = (InterleaveStruct **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppInterleaveTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->InterleaveTblesNum);
      if (pParsedHeader->ppInterleaveTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_SMBIOS_MGMT_INFO_TYPE:
      pParsedHeader->ppSmbiosTbles = (SmbiosTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppSmbiosTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->SmbiosTblesNum);
      if (pParsedHeader->ppSmbiosTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_CONTROL_REGION_TYPE:
      pParsedHeader->ppControlRegionTbles = (ControlRegionTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppControlRegionTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->ControlRegionTblesNum);
      if (pParsedHeader->ppControlRegionTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_BW_DATA_WINDOW_REGION_TYPE:
      pParsedHeader->ppBWRegionTbles = (BWRegionTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppBWRegionTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->BWRegionTblesNum);
      if (pParsedHeader->ppBWRegionTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_FLUSH_HINT_TYPE:
      pParsedHeader->ppFlushHintTbles = (FlushHintTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppFlushHintTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->FlushHintTblesNum);
      if (pParsedHeader->ppFlushHintTbles == NULL) {
        goto FinishError;
      }
      break;
    case NVDIMM_PLATFORM_CAPABILITIES_TYPE:
      pParsedHeader->ppPlatformCapabilitiesTbles = (PlatformCapabilitiesTbl **)CopyMemoryAndAddPointerToArray(
          (VOID **)pParsedHeader->ppPlatformCapabilitiesTbles, pTabPointer,
          pTableHeader->Length, &pParsedHeader->PlatformCapabilitiesTblesNum);
      if (pParsedHeader->ppPlatformCapabilitiesTbles == NULL) {
        goto FinishError;
      }
      break;
    default:
      break;
    }

    pTabPointer += pTableHeader->Length;
    pTableHeader = (SubTableHeader *)pTabPointer;
  }

  goto FinishSuccess;

FinishError:
  FreeParsedNfit(pParsedHeader);
  pParsedHeader = NULL;
FinishSuccess:
  NVDIMM_EXIT();
  return pParsedHeader;
}

/**
  Performs deserialization from binary memory block, containing PCAT tables, into parsed structure of pointers.

  @param[in] pTable pointer to the memory containing the PCAT binary representation.

  @retval NULL if there was an error while parsing the memory.
  @retval pointer to the allocated header with parsed PCAT.
**/
ParsedPcatHeader *
ParsePcatTable (
  IN     VOID *pTable
  )
{
  ParsedPcatHeader *pParsedPcat = NULL;                 //!< Output Parsed PCAT structures
  PLATFORM_CONFIG_ATTRIBUTES_TABLE *pPcatHeader = NULL; //!< PCAT header
  PCAT_TABLE_HEADER *pPcatSubTableHeader = NULL;        //!< PCAT subtable header
  UINT32 RemainingPcatBytes = 0;
  UINT32 Length = 0;

  NVDIMM_ENTRY();

  if (pTable == NULL) {
    NVDIMM_DBG("The PCAT pointer is NULL.");
    goto FinishError;
  }

  pPcatHeader = (PLATFORM_CONFIG_ATTRIBUTES_TABLE *) pTable;

  if (!IsChecksumValid(pPcatHeader, pPcatHeader->Header.Length)) {
    NVDIMM_DBG("The checksum of PCAT table is invalid.");
    goto FinishError;
  }

  pPcatSubTableHeader = (PCAT_TABLE_HEADER *) &pPcatHeader->pPcatTables;
  RemainingPcatBytes = pPcatHeader->Header.Length - sizeof(*pPcatHeader);

  pParsedPcat = (ParsedPcatHeader *) AllocateZeroPool(sizeof(*pParsedPcat));
  if (pParsedPcat == NULL) {
    NVDIMM_DBG("Could not allocate memory.");
    goto FinishError;
  }

  pParsedPcat->pPlatformConfigAttr = (PLATFORM_CONFIG_ATTRIBUTES_TABLE *) AllocateZeroPool(
    sizeof(*pParsedPcat->pPlatformConfigAttr));
  if (pParsedPcat->pPlatformConfigAttr == NULL) {
    NVDIMM_DBG("Could not allocate memory.");
    goto FinishError;
  }

  // Copying PCAT header to parsed structure
  CopyMem_S(pParsedPcat->pPlatformConfigAttr, sizeof(*pParsedPcat->pPlatformConfigAttr), pPcatHeader, sizeof(*pParsedPcat->pPlatformConfigAttr));

  // Looking for sub tables
  while (RemainingPcatBytes > 0) {
    Length = pPcatSubTableHeader->Length;

    if (Length == 0) {
      NVDIMM_DBG("Length can't be 0.");
      goto FinishError;
    }

    switch(pPcatSubTableHeader->Type) {
    case PCAT_TYPE_PLATFORM_CAPABILITY_INFO_TABLE:
      pParsedPcat->ppPlatformCapabilityInfo = (PLATFORM_CAPABILITY_INFO **) CopyMemoryAndAddPointerToArray(
          (VOID **) pParsedPcat->ppPlatformCapabilityInfo, pPcatSubTableHeader, Length,
          &pParsedPcat->PlatformCapabilityInfoNum);
      if (pParsedPcat->ppPlatformCapabilityInfo == NULL) {
        NVDIMM_DBG("Memory allocate error.");
        goto FinishError;
      }
      break;

    case PCAT_TYPE_INTERLEAVE_CAPABILITY_INFO_TABLE:
      pParsedPcat->ppMemoryInterleaveCapabilityInfo = (MEMORY_INTERLEAVE_CAPABILITY_INFO **) CopyMemoryAndAddPointerToArray(
          (VOID **) pParsedPcat->ppMemoryInterleaveCapabilityInfo, pPcatSubTableHeader, Length,
          &pParsedPcat->MemoryInterleaveCapabilityInfoNum);
      if (pParsedPcat->ppMemoryInterleaveCapabilityInfo == NULL) {
        NVDIMM_DBG("Memory allocate error.");
        goto FinishError;
      }
      break;

    case PCAT_TYPE_RUNTIME_INTERFACE_TABLE:
      pParsedPcat->ppRuntimeInterfaceValConfInput = (RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE **) CopyMemoryAndAddPointerToArray(
          (VOID **) pParsedPcat->ppRuntimeInterfaceValConfInput, pPcatSubTableHeader, Length,
          &pParsedPcat->RuntimeInterfaceValConfInputNum);
      if (pParsedPcat->ppRuntimeInterfaceValConfInput == NULL) {
        NVDIMM_DBG("Memory allocate error.");
        goto FinishError;
      }
      break;

    case PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE:
      pParsedPcat->ppConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE **) CopyMemoryAndAddPointerToArray(
          (VOID **) pParsedPcat->ppConfigManagementAttributesInfo, pPcatSubTableHeader, Length,
          &pParsedPcat->ConfigManagementAttributesInfoNum);
      if (pParsedPcat->ppConfigManagementAttributesInfo == NULL) {
        NVDIMM_DBG("Memory allocate error.");
        goto FinishError;
      }
      break;

    case PCAT_TYPE_SOCKET_SKU_INFO_TABLE:
      pParsedPcat->ppSocketSkuInfoTable = (SOCKET_SKU_INFO_TABLE **) CopyMemoryAndAddPointerToArray(
          (VOID **) pParsedPcat->ppSocketSkuInfoTable, pPcatSubTableHeader, Length,
          &pParsedPcat->SocketSkuInfoNum);
      if (pParsedPcat->ppSocketSkuInfoTable == NULL) {
        NVDIMM_DBG("Memory allocate error.");
        goto FinishError;
      }
      break;

    default:
      NVDIMM_WARN("Unknown type of PCAT table.");
      goto FinishError;
    }

    RemainingPcatBytes -= Length;
    pPcatSubTableHeader = (PCAT_TABLE_HEADER *) ((UINT8 *)pPcatSubTableHeader + Length);
  }

  goto FinishSuccess;

FinishError:
  FreeParsedPcat(pParsedPcat);
  pParsedPcat = NULL;
FinishSuccess:
  NVDIMM_EXIT();
  return pParsedPcat;
}

/**
  Performs deserialization from binary memory block containing PMTT table and checks if memory mode can be configured.

  @param[in] pTable pointer to the memory containing the PMTT binary representation.

  @retval false if topology does NOT allows MM.
  @retval true if topology allows MM.
**/
BOOLEAN
CheckIsMemoryModeAllowed(
  IN PMTT_TABLE *pPMTT
  )
{
  BOOLEAN MMCanBeConfigured = FALSE;
  BOOLEAN IsDDR = FALSE;
  BOOLEAN IsDCPM = FALSE;

  if (pPMTT == NULL) {
    goto Finish;
  }
  if (!IsChecksumValid(pPMTT, pPMTT->Header.Length)) {
    NVDIMM_WARN("The checksum of PMTT table is invalid.");
    goto Finish;
  }

  UINT64 Offset = sizeof(pPMTT->Header) + sizeof(pPMTT->Reserved);
  PMTT_COMMON_HEADER *pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
  while (Offset < pPMTT->Header.Length && pCommonHeader->Type == PMTT_TYPE_SOCKET) {
    // check if socket is enabled
    if (pCommonHeader->Flags) {
      Offset += sizeof(PMTT_SOCKET) + PMTT_COMMON_HDR_LEN;
      pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
      while (Offset < pPMTT->Header.Length && pCommonHeader->Type == PMTT_TYPE_iMC) {
        // check if iMC is enabled
        if (pCommonHeader->Flags) {
          Offset += sizeof(PMTT_iMC) + PMTT_COMMON_HDR_LEN;
          pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
          // check if at least one DCPMM is present
          while (Offset < pPMTT->Header.Length && pCommonHeader->Type == PMTT_TYPE_MODULE) {
            PMTT_MODULE *pModule = (PMTT_MODULE *)(((UINT8 *)pCommonHeader) + sizeof(pCommonHeader));
            // if IsDCPM is already set then continue to loop to find the offset of the next aggregated device
            if (!IsDCPM) {
              // bit 2 is set then DCPMM
              if ((pCommonHeader->Flags & PMTT_DDR_DCPM_FLAG) && pModule->SizeOfDimm > 0) {
                IsDCPM = TRUE;
              } else if (!(pCommonHeader->Flags & PMTT_DDR_DCPM_FLAG) && pModule->SizeOfDimm > 0) {
                IsDDR = TRUE;
              }
            }
            Offset += sizeof(PMTT_MODULE) + PMTT_COMMON_HDR_LEN;
            pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
          } // end of Module
          if (IsDDR && !IsDCPM) {
            MMCanBeConfigured = FALSE;
            goto Finish;
          }
          MMCanBeConfigured = TRUE;
          IsDDR = FALSE;
          IsDCPM = FALSE;
        } else {
          // iMC is disabled
          Offset += pCommonHeader->Length;
          pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
        }
      } // end of iMC
    } else { // socket is disabled
      Offset += pCommonHeader->Length;
      pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
    }
  } // end of socket

Finish:
  return MMCanBeConfigured;
}

/**
  Returns the FlushHint table associated with the provided NVDIMM region table.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] pNvDimmRegionTbl the NVDIMM region table that contains the index.
  @param[out] ppFlushHintTable pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
  @retval EFI_NOT_FOUND if there is no Interleave table with the provided index.
**/
EFI_STATUS
GetFlushHintTableForNvDimmRegionTable(
  IN     ParsedFitHeader *pFitHead,
  IN     NvDimmRegionTbl *pNvDimmRegionTbl,
     OUT FlushHintTbl **ppFlushHintTable
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT32 Index = 0;

  if (pFitHead == NULL || pNvDimmRegionTbl == NULL || ppFlushHintTable == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < pFitHead->FlushHintTblesNum; Index++) {
    if (pNvDimmRegionTbl->DeviceHandle.AsUint32 == pFitHead->ppFlushHintTbles[Index]->DeviceHandle.AsUint32) {
      *ppFlushHintTable = pFitHead->ppFlushHintTbles[Index];
      ReturnCode = EFI_SUCCESS;
    }
  }

Finish:
  return ReturnCode;
}

/**
  GetBlockDataWindowRegDescTabl - returns the Block Data Window Table associated with the provided Control Region Table.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] pControlRegionTable the Control Region table that contains the index.
  @param[out] ppBlockDataWindowTable pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if pFitHead or ControlRegionTbl or BWRegionTbl equals NULL.
  @retval EFI_NOT_FOUND if there is no Block Data Window Descriptor table with the provided index.
**/
EFI_STATUS
GetBlockDataWindowRegDescTabl(
  IN     ParsedFitHeader *pFitHead,
  IN     ControlRegionTbl *pControlRegTbl,
     OUT BWRegionTbl **ppBlockDataWindowTable
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT16 Index = 0;
  UINT16 ControlTableIndex = 0;

  if (pFitHead == NULL || pControlRegTbl == NULL || ppBlockDataWindowTable == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ControlTableIndex = pControlRegTbl->ControlRegionDescriptorTableIndex;

  for (Index = 0; Index < pFitHead->BWRegionTblesNum; Index++) {
    if (pFitHead->ppBWRegionTbles[Index]->ControlRegionStructureIndex == ControlTableIndex) {
      *ppBlockDataWindowTable = pFitHead->ppBWRegionTbles[Index];
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }

Finish:
  return ReturnCode;
}

/**
  Returns the ControlRegion table associated with the provided NVDIMM region table.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] pNvDimmRegionTbl the NVDIMM region table that contains the index.
  @param[out] ppControlRegionTable pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more input parameters equal NULL.
  @retval EFI_NOT_FOUND if there is no Control Region table with the provided index.
**/
EFI_STATUS
GetControlRegionTableForNvDimmRegionTable(
  IN     ParsedFitHeader *pFitHead,
  IN     NvDimmRegionTbl *pNvDimmRegionTbl,
     OUT ControlRegionTbl **ppControlRegionTable
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT16 Index = 0;
  UINT16 ControlTableIndex = 0;

  if (pFitHead == NULL || pNvDimmRegionTbl == NULL || ppControlRegionTable == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *ppControlRegionTable = NULL;
  ControlTableIndex = pNvDimmRegionTbl->NvdimmControlRegionDescriptorTableIndex;

  for (Index = 0; Index < pFitHead->ControlRegionTblesNum; Index++) {
    if (pFitHead->ppControlRegionTbles[Index]->ControlRegionDescriptorTableIndex == ControlTableIndex) {
      *ppControlRegionTable = pFitHead->ppControlRegionTbles[Index];
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }

Finish:
  return ReturnCode;
}

/**
  Get Control Region table for provided PhysicalID

  @param[in] pFitHead pointer to the parsed NFit Header structure
  @param[in] Pid Dimm PhysicalID
  @param[out] pControlRegionTables array to store Control Region tables pointers
  @param[in, out] pControlRegionTablesNum size of array on input, number of items stored in the array on output

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_BUFFER_TOO_SMALL There is more Control Region tables in NFIT than size of provided array
**/
EFI_STATUS
GetControlRegionTablesForPID(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 Pid,
     OUT ControlRegionTbl *pControlRegionTables[],
  IN OUT UINT32 *pControlRegionTablesNum
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 CurrentArrayNum = 0;
  ControlRegionTbl *pCtrlTable = NULL;
  BOOLEAN ContainedAlready = FALSE;

  NVDIMM_ENTRY();

  if (pFitHead == NULL || pControlRegionTables == NULL || pControlRegionTablesNum == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < pFitHead->NvDimmRegionTblesNum; Index++) {
    if (Pid == pFitHead->ppNvDimmRegionTbles[Index]->NvDimmPhysicalId) {
      ReturnCode = GetControlRegionTableForNvDimmRegionTable(
          pFitHead, pFitHead->ppNvDimmRegionTbles[Index], &pCtrlTable);

      /** Make sure the found Control Region table is not in the array already. **/
      ContainedAlready = FALSE;
      for (Index2 = 0; Index2 < CurrentArrayNum; Index2++) {
        if (pCtrlTable == pControlRegionTables[Index2]) {
          ContainedAlready = TRUE;
        }
      }

      if (!ContainedAlready) {
        if (CurrentArrayNum >= *pControlRegionTablesNum) {
          NVDIMM_ERR("There are more Control Region tables than length of the input array.");
          ReturnCode = EFI_BUFFER_TOO_SMALL;
          goto Finish;
        }
        pControlRegionTables[CurrentArrayNum] = pCtrlTable;
        CurrentArrayNum++;
      }
    }
  }

  *pControlRegionTablesNum = CurrentArrayNum;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  GetSpaRangeTable - returns the SpaRange Table with the provided Index.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] SpaRangeTblIndex index of the table to be found.
  @param[out] ppSpaRangeTbl pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if pFitHead or ppInterleaveTbl equals NULL.
  @retval EFI_NOT_FOUND if there is no Interleave table with the provided index.
**/
EFI_STATUS
GetSpaRangeTable(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 SpaRangeTblIndex,
     OUT SpaRangeTbl **ppSpaRangeTbl
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT16 Index = 0;

  if (pFitHead == NULL || ppSpaRangeTbl == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *ppSpaRangeTbl = NULL;

  for (Index = 0; Index < pFitHead->SpaRangeTblesNum; Index++) {
    if (pFitHead->ppSpaRangeTbles[Index]->SpaRangeDescriptionTableIndex == SpaRangeTblIndex) {
      *ppSpaRangeTbl = pFitHead->ppSpaRangeTbles[Index];
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }

Finish:
  return ReturnCode;
}

/**
  GetInterleaveTable - returns the Interleave Table with the provided Index.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] InterleaveTblIndex index of the table to be found.
  @param[out] ppInterleaveTbl pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if pFitHead or ppInterleaveTbl equals NULL.
  @retval EFI_NOT_FOUND if there is no Interleave table with the provided index.
**/
EFI_STATUS
GetInterleaveTable(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 InterleaveTblIndex,
     OUT InterleaveStruct **ppInterleaveTbl
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT16 Index = 0;

  if (pFitHead == NULL || ppInterleaveTbl == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *ppInterleaveTbl = NULL;

  for (Index = 0; Index < pFitHead->InterleaveTblesNum; Index++) {
    if (pFitHead->ppInterleaveTbles[Index]->InterleaveStructureIndex == InterleaveTblIndex) {
      *ppInterleaveTbl = pFitHead->ppInterleaveTbles[Index];
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }

Finish:
  return ReturnCode;
}

/**
  Finds in the provided Nfit structure the requested NVDIMM region.

  If the pAddrRangeTypeGuid equals NULL, the first table matching the Pid will be returned.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] Pid the Dimm ID that the NVDIMM region must be for.
  @param[in] pAddrRangeTypeGuid pointer to GUID type of the range that we are looking for. OPTIONAL
  @param[in] SpaRangeIndexProvided Determine if SpaRangeIndex is provided
  @param[in] SpaRangeIndex Looking for NVDIMM region table that is related with provided SPA table. OPTIONAL
  @param[out] ppNvDimmRegionTbl pointer to a pointer for the return NVDIMM region.

  @retval EFI_SUCCESS if the table was found and was returned.
  @retval EFI_INVALID_PARAMETER if one or more input parameters equal NULL.
  @retval EFI_NOT_FOUND if there is no NVDIMM region for the provided Dimm PID and AddrRangeType.
**/
EFI_STATUS
GetNvDimmRegionTableForPid(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 Pid,
  IN     GUID *pAddrRangeTypeGuid OPTIONAL,
  IN     BOOLEAN SpaRangeIndexProvided,
  IN     UINT16 SpaRangeIndex OPTIONAL,
     OUT NvDimmRegionTbl **ppNvDimmRegionTbl
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 Index = 0;
  SpaRangeTbl *pSpaRangeTbl = NULL;
  UINT16 SpaIndexInNvDimmRegion = 0;
  BOOLEAN Found = FALSE;

  if (pFitHead == NULL || Pid == DIMM_PID_ALL || Pid == DIMM_PID_INVALID || ppNvDimmRegionTbl == NULL) {
    goto Finish;
  }

  *ppNvDimmRegionTbl = NULL;

  for (Index = 0; Index < pFitHead->NvDimmRegionTblesNum; Index++) {
    if (pFitHead->ppNvDimmRegionTbles[Index]->NvDimmPhysicalId != Pid) {
      continue;
    }

    SpaIndexInNvDimmRegion = pFitHead->ppNvDimmRegionTbles[Index]->SpaRangeDescriptionTableIndex;
    Found = TRUE;

    if (SpaRangeIndexProvided && SpaIndexInNvDimmRegion != SpaRangeIndex) {
      Found = FALSE;
    }

    if (pAddrRangeTypeGuid != NULL) {
      pSpaRangeTbl = NULL;
      ReturnCode = GetSpaRangeTable(pFitHead, SpaIndexInNvDimmRegion, &pSpaRangeTbl);

      if (EFI_ERROR(ReturnCode) || pSpaRangeTbl == NULL ||
          CompareMem(&pSpaRangeTbl->AddressRangeTypeGuid,
          pAddrRangeTypeGuid, sizeof(pSpaRangeTbl->AddressRangeTypeGuid)) != 0) {
        Found = FALSE;
      }
    }

    if (Found) {
      *ppNvDimmRegionTbl = pFitHead->ppNvDimmRegionTbles[Index];
      ReturnCode = EFI_SUCCESS;
      break;
    } else {
      ReturnCode = EFI_NOT_FOUND;
    }
  }

Finish:
  return ReturnCode;
}

/**
  RdpaToSpa() - Convert Device Region Physical to System Physical Address

  @param[in] Rdpa Device Region Physical Address to convert
  @param[in] pNvDimmRegionTable The NVDIMM region that helps describe this region of memory
  @param[in] pInterleaveTable Interleave table referenced by the mdsparng_tbl
  @param[out] SpaAddr output for SPA address

  A memory device could have multiple regions. As such we cannot convert
  to a device physical address. Instead we refer to the address for a region
  within the device as device region physical address (RDPA), where Rdpa is
  a zero based address from the start of the region within the device.

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER on a divide by zero error
**/
EFI_STATUS
RdpaToSpa(
  IN     UINT64 Rdpa,
  IN     NvDimmRegionTbl *pNvDimmRegionTable,
  IN     SpaRangeTbl *pSpaRangeTable,
  IN     InterleaveStruct *pInterleaveTable OPTIONAL,
     OUT UINT64 *pSpaAddr
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 RotationSize = 0;
  UINT64 RotationNum = 0;
  UINT32 LineNum = 0;
  UINT64 StartSpaAddress = 0;

  if (!pSpaAddr || !pSpaRangeTable) {
    return EFI_INVALID_PARAMETER;
  }

  StartSpaAddress = pSpaRangeTable->SystemPhysicalAddressRangeBase + pNvDimmRegionTable->RegionOffset;

  if (pInterleaveTable != NULL) {
    if (!pInterleaveTable->LineSize || !pInterleaveTable->NumberOfLinesDescribed) {
      NVDIMM_DBG("Divide by Zero\n");
      ReturnCode = EFI_INVALID_PARAMETER;
      return ReturnCode;
    }

    RotationSize = pInterleaveTable->LineSize * pInterleaveTable->NumberOfLinesDescribed;
    RotationNum = Rdpa / RotationSize;
    LineNum = (UINT32)((Rdpa % RotationSize) / pInterleaveTable->LineSize);

    *pSpaAddr = StartSpaAddress
        + RotationNum * RotationSize * pNvDimmRegionTable->InterleaveWays
        + pInterleaveTable->LinesOffsets[LineNum] * pInterleaveTable->LineSize
        + Rdpa % pInterleaveTable->LineSize;

    return ReturnCode;
  } else {
    /** TODO: Not Interleaved **/
    *pSpaAddr = StartSpaAddress + Rdpa;
    return ReturnCode;
  }
}

/**
  CopyMemoryAndAddPointerToArray - Copies the data and adds the result pointer to an array of pointers.

  @param[in, out] ppTable pointer to the array of pointers. Warning! This pointer will be freed.
  @param[in] pToAdd pointer to the data that the caller wants to add to the array.
  @param[in] DataSize size of the data that are supposed to be copied.
  @param[in] NewPointerIndex index in the table that the new pointer should have.

  @retval NULL - if a memory allocation failed.
  @retval pointer to the new array of pointers (with the new one at the end).
**/
STATIC
VOID **
CopyMemoryAndAddPointerToArray(
  IN OUT VOID **ppTable,
  IN     VOID *pToAdd,
  IN     UINT32 DataSize,
  IN     UINT32 *pNewPointerIndex
  )
{
  VOID **ppNewTable = NULL;
  VOID *pData = NULL;

  if (pToAdd == NULL) {
    NVDIMM_ERR("Pointer to data for adding cannot be NULL.");
    goto Finish;
  }

  // Allocate the memory for the new entry to list of tables and for the contents of new entry
  ppNewTable = AllocatePool(sizeof(VOID *) * (*pNewPointerIndex + 1));
  pData = AllocatePool(DataSize);

  if (ppNewTable == NULL || pData == NULL) {
    NVDIMM_DBG("Could not allocate the memory.");
    goto Finish;
  }

  // Copy the array beginning only if there is any
  if (*pNewPointerIndex > 0 && ppTable != NULL) {
    CopyMem_S(ppNewTable, sizeof(VOID *) * (*pNewPointerIndex + 1), ppTable, sizeof(VOID *) * *pNewPointerIndex);
  }

  // Make a copy of the table to add
  CopyMem_S(pData, DataSize, pToAdd, DataSize);

  // Assign the new copied table to the array
  ppNewTable[*pNewPointerIndex] = pData;

  (*pNewPointerIndex)++; // Increment the array index

Finish:
  FREE_POOL_SAFE(ppTable);

  return ppNewTable;
}


/**
  Return the current memory mode chosen by the BIOS during boot-up. 1LM is
  the fallback option and will always be available. 2LM will only be enabled
  if the AllowedMemoryMode is 2LM, there is memory configured for 2LM, and
  it's in a BIOS-supported configuration. We read this information from the
  PCAT table provided by BIOS.

  @param[out] pResult The current memory mode chosen by BIOS

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_ABORTED PCAT tables not found
**/
EFI_STATUS
CurrentMemoryMode(
     OUT MEMORY_MODE *pResult
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  PLATFORM_CAPABILITY_INFO *pPlatformCapability = NULL;

  NVDIMM_ENTRY();

  if (pResult == NULL) {
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead == NULL || gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum != 1) {
    NVDIMM_DBG("Incorrect PCAT tables");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }
  pPlatformCapability = gNvmDimmData->PMEMDev.pPcatHead->ppPlatformCapabilityInfo[0];
  *pResult = pPlatformCapability->CurrentMemoryMode.MemoryModeSplit.CurrentVolatileMode;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Return the allowed memory mode selected in the BIOS setup menu under
  Socket Configuration -> Memory Configuration -> Memory Map -> Volatile Memory Mode.
  Even if 2LM is allowed, it implies that 1LM is allowed as well (even
  though the memory mode doesn't indicate this).
  We read this information from the PCAT table provided by BIOS.

  @param[out] pResult The allowed memory mode setting in BIOS

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_ABORTED PCAT tables not found
**/
EFI_STATUS
AllowedMemoryMode(
     OUT MEMORY_MODE *pResult
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  PLATFORM_CAPABILITY_INFO *pPlatformCapability = NULL;

  NVDIMM_ENTRY();

  if (pResult == NULL) {
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead == NULL || gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum != 1) {
    NVDIMM_DBG("Incorrect PCAT tables");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }
  pPlatformCapability = gNvmDimmData->PMEMDev.pPcatHead->ppPlatformCapabilityInfo[0];
  *pResult = pPlatformCapability->CurrentMemoryMode.MemoryModeSplit.AllowedVolatileMode;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve the PCAT Socket SKU info table for a given Socket

  @param[in] SocketId SocketID to retrieve the table for
  @param[out] ppSocketSkuInfoTable Sku info table referenced by socket ID

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_NOT_FOUND PCAT socket sku info table not found for given socketID
**/
EFI_STATUS
RetrievePcatSocketSkuInfoTable(
  IN     UINT32 SocketId,
     OUT SOCKET_SKU_INFO_TABLE **ppSocketSkuInfoTable
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (ppSocketSkuInfoTable == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead == NULL || gNvmDimmData->PMEMDev.pPcatHead->SocketSkuInfoNum == 0) {
    NVDIMM_DBG("Incorrect PCAT tables");
    goto Finish;
  }

  for (Index = 0; Index < gNvmDimmData->PMEMDev.pPcatHead->SocketSkuInfoNum; Index++) {
    if (SocketId == gNvmDimmData->PMEMDev.pPcatHead->ppSocketSkuInfoTable[Index]->SocketId) {
      *ppSocketSkuInfoTable = gNvmDimmData->PMEMDev.pPcatHead->ppSocketSkuInfoTable[Index];
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
