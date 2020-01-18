/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Namespace.h>
#include "ConfigDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#define DIMMSPECS_TEST_INDEX 0
#define DUPLICATE_DIMM_TEST_INDEX 1
#define SYSTEMCAP_TEST_INDEX 2
#define NAMESPACE_LSA_TEST_INDEX 3
#define PCD_TEST_INDEX 4

/**
  Check if the Namespace Label area can be read and if it is in a format we understand

  @param[in] pDimm Pointer to the DIMM
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[in out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_NOT_FOUND Unable to locate relevant PCAT tables.
**/
STATIC
EFI_STATUS
CheckNamespaceLabelAreaIndex(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  LABEL_STORAGE_AREA *pLabelStorageArea = NULL;
  UINT16 Index = 0;

  NVDIMM_ENTRY();

  if (DimmCount == 0 || ppDimms == NULL || DimmCount > MAX_DIMMS ||
       ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; ++Index) {

    ReturnCode = GetDimmUid(ppDimms[Index], DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetDimmUid function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = GetPreferredValueAsString(ppDimms[Index]->DeviceHandle.AsUint32, DimmUid, DimmIdPreference == DISPLAY_DIMM_ID_HANDLE,
       DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetPreferredValueAsString function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = ReadLabelStorageArea(ppDimms[Index]->DimmID, &pLabelStorageArea);
    if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_NOT_FOUND) {
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_UNABLE_TO_READ_NS_INFO), EVENT_CODE_622, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    } else {
      ReturnCode = EFI_SUCCESS;
    }
    FreeLsaSafe(&pLabelStorageArea);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Parse and retrieve the sub table status fields from a COUT table.

  If a sub table is not discovered the MAX value for the data type will be returned since 0 is a valid
  status.

  @param[in] pConfigOutput, COUT table to parse
  @param[out] pPartitionSizeChangeTableStatus, Status from the partition size change table
  @param[out] pInterleaveInformationTableStatus_1, Status from the first discovered interleave information table
  @param[out] pInterleaveInformationTableStatus_2, Status from the second discovered interleave information table

**/
STATIC
VOID
GetPlatformConfigOutputPCATStatus(
  IN     NVDIMM_PLATFORM_CONFIG_OUTPUT *pConfigOutput,
     OUT UINT32 *pPartitionSizeChangeTableStatus,
     OUT UINT8 *pInterleaveInformationTableStatus_1,
     OUT UINT8 *pInterleaveInformationTableStatus_2
  )
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  UINT8 InterleaveTableNumber = 1;

  if (pConfigOutput == NULL ||
    pPartitionSizeChangeTableStatus == NULL ||
    pInterleaveInformationTableStatus_1 == NULL ||
    pInterleaveInformationTableStatus_2 == NULL) {
    NVDIMM_DBG("Invalid Parameter");
    return;
  }

  *pPartitionSizeChangeTableStatus = MAX_UINT32;
  *pInterleaveInformationTableStatus_1 = MAX_UINT8;
  *pInterleaveInformationTableStatus_2 = MAX_UINT8;

  /** Check if there is at least one PCAT table **/
  if (pConfigOutput->Header.Length <= sizeof(*pConfigOutput)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *) &pConfigOutput->pPcatTables;
  SizeOfPcatTables = pConfigOutput->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pConfigOutput);

  while ((UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)&pConfigOutput->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pCurPcatTable;

      *pPartitionSizeChangeTableStatus = pPartSizeChange->PartitionSizeChangeStatus;

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      if (IS_ACPI_HEADER_REV_MAJ_0_MIN_1_OR_MIN_2(pConfigOutput)) {
        NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pCurPcatTable;

        if (InterleaveTableNumber == 1) {
          *pInterleaveInformationTableStatus_1 = pInterleaveInfo->InterleaveChangeStatus;
          InterleaveTableNumber++;
        }
        else {
          *pInterleaveInformationTableStatus_2 = pInterleaveInfo->InterleaveChangeStatus;
        }

        pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
      } else if (IS_ACPI_HEADER_REV_MAJ_1_MIN_1_OR_MIN_2(pConfigOutput)) {
        NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pCurPcatTable;

        if (InterleaveTableNumber == 1) {
          *pInterleaveInformationTableStatus_1 = pInterleaveInfo->InterleaveChangeStatus;
          InterleaveTableNumber++;
        }
        else {
          *pInterleaveInformationTableStatus_2 = pInterleaveInfo->InterleaveChangeStatus;
        }

        pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
      }
    } else {
      NVDIMM_DBG("Unknown table discovered in COUT");
      break;
    }
  }
}

/**
  Create the detailed status string for when current configuration failed to apply

  @param[in] CCurStatus, Status byte from the CCUR table

  @retval NULL - failed to retrieve string from HII strings DB
  @retval Formatted CHAR16 string
**/
STATIC
CHAR16*
GetCCurDetailedStatusStr(
  IN     UINT16 CCurStatus
  )
{
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pCCurErrorMessage = NULL;
  CHAR16 *pReturnStr = NULL;

  NVDIMM_ENTRY();

  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_DIAG_CCUR_CONFIG_DETAILED_STATUS), NULL);

  switch (CCurStatus) {
  case DIMM_CONFIG_CPU_MAX_MEMORY_LIMIT_VIOLATION:
    pCCurErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_CPU_MAX_MEMORY_LIMIT_VIOLATION), NULL);
    break;
  case DIMM_CONFIG_DCPMM_NM_FM_RATIO_UNSUPPORTED:
    pCCurErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_NM_FM_RATIO_UNSUPPORTED), NULL);
    break;
  case DIMM_CONFIG_DCPMM_POPULATION_ISSUE:
    pCCurErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_POPULATION_VIOLATION), NULL);
    break;
  case DIMM_CONFIG_PM_MAPPED_VM_POPULATION_ISSUE:
    pCCurErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_POPULATION_VIOLATION_BUT_PM_MAPPED), NULL);
    break;
  default:
    pCCurErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_UNKNOWN), NULL);
    break;
  }

  pReturnStr = CatSPrint(NULL, pTmpStr, CCurStatus, pCCurErrorMessage);
  FREE_POOL_SAFE(pTmpStr);
  FREE_POOL_SAFE(pCCurErrorMessage);

  NVDIMM_EXIT();
  return pReturnStr;
}

/**
  Create the detailed status string for when a goal failed to apply

  Since zero is a valid status and N/A is a possible return value. The MAX value for a given
  status data type will be interpreted as N/A

  @param[in] CoutStatus, Status byte from the COUT table
  @param[in] PartitionSizeChangeTableStatus, Status from the partition size change table
  @param[in] InterleaveInformationTableStatus_1, Status from the first discovered interleave information table
  @param[in] InterleaveInformationTableStatus_2, Status from the second discovered interleave information table

  @retval NULL - failed to retrieve string from HII strings DB
  @retval Formatted CHAR16 string
**/
STATIC
CHAR16*
GetCoutDetailedStatusStr(
  IN     UINT8 CoutStatus,
  IN     UINT32 PartitionSizeChangeTableStatus,
  IN     UINT8 InterleaveInformationTableStatus_1,
  IN     UINT8 InterleaveInformationTableStatus_2
)
{
  CHAR16 PartitionSizeChangeTableStatusStr[MAX_PCD_TABLE_STATUS_LENGTH];
  CHAR16 InterleaveInformationTableStatus_1Str[MAX_PCD_TABLE_STATUS_LENGTH];
  CHAR16 InterleaveInformationTableStatus_2Str[MAX_PCD_TABLE_STATUS_LENGTH];
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pCoutErrorMessage = NULL;
  CHAR16 *pTmpStr1 = NULL;
  CHAR16 *pReturnStr = NULL;

  ZeroMem(PartitionSizeChangeTableStatusStr, sizeof(PartitionSizeChangeTableStatusStr));
  ZeroMem(InterleaveInformationTableStatus_1Str, sizeof(InterleaveInformationTableStatus_1Str));
  ZeroMem(InterleaveInformationTableStatus_2Str, sizeof(InterleaveInformationTableStatus_2Str));

  NVDIMM_ENTRY();

  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NOT_AVAILABLE), NULL);

  if (pTmpStr != NULL) {
    if (PartitionSizeChangeTableStatus == MAX_UINT32) {
      StrnCpyS(PartitionSizeChangeTableStatusStr, MAX_PCD_TABLE_STATUS_LENGTH, pTmpStr, MAX_PCD_TABLE_STATUS_LENGTH - 1);
    }
    else {
      UnicodeSPrint(PartitionSizeChangeTableStatusStr, sizeof(PartitionSizeChangeTableStatusStr), L"%d", PartitionSizeChangeTableStatus);
    }

    if (InterleaveInformationTableStatus_1 == MAX_UINT8) {
      StrnCpyS(InterleaveInformationTableStatus_1Str, MAX_PCD_TABLE_STATUS_LENGTH, pTmpStr, MAX_PCD_TABLE_STATUS_LENGTH - 1);
    }
    else {
      UnicodeSPrint(InterleaveInformationTableStatus_1Str, sizeof(InterleaveInformationTableStatus_1Str), L"%d", (UINT32)InterleaveInformationTableStatus_1);
    }

    if (InterleaveInformationTableStatus_2 == MAX_UINT8) {
      StrnCpyS(InterleaveInformationTableStatus_2Str, MAX_PCD_TABLE_STATUS_LENGTH, pTmpStr, MAX_PCD_TABLE_STATUS_LENGTH - 1);
    }
    else {
      UnicodeSPrint(InterleaveInformationTableStatus_2Str, sizeof(InterleaveInformationTableStatus_2Str), L"%d", InterleaveInformationTableStatus_2);
    }

    FREE_POOL_SAFE(pTmpStr);
  }

  switch (CoutStatus) {
  case CONFIG_OUTPUT_STATUS_ERROR:
    if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_FW_ERROR) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_DCPMM_FIRMWARE_ERROR), NULL);
    }
    else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_EXCEED_DRAM_DECODERS ||
            InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_EXCEED_DRAM_DECODERS ||
            InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_EXCEED_DRAM_DECODERS) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_INSUFFICIENT_SILICON_RESOURCES), NULL);
    }
    else if (InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_EXCEED_MAX_SPA_SPACE ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_EXCEED_MAX_SPA_SPACE) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_INSUFFICIENT_SPA_SPACE), NULL);
    }
    else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_DIMM_MISSING ||
             InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_DIMM_MISSING ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_DIMM_MISSING) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_DIMM_MISSING_IN_ISET), NULL);
    }
    else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_ISET_MISSING ||
             InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_ISET_MISSING ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_ISET_MISSING) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_MATCHING_ISET_NOT_FOUND), NULL);
    }
    else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_EXCEED_SIZE) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_EXCEEDS_PARTITION_SIZE), NULL);
    }
    else if (InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_CIN_MISSING ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_CIN_MISSING) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_CIN_MISSING), NULL);
    }
    else if (InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_CHANNEL_NOT_MATCH), NULL);
    }
    else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_UNSUPPORTED_ALIGNMENT ||
             InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_UNSUPPORTED_ALIGNMENT), NULL);
    }
    else if (InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_REQUEST_UNSUPPORTED ||
             InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_REQUEST_UNSUPPORTED) {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_REQUEST_UNSUPPORTED), NULL);
    }
    else {
      pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_UNKNOWN), NULL);
    }
    break;
  case CONFIG_OUTPUT_STATUS_CPU_MAX_MEMORY_LIMIT_VIOLATION:
    pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_CPU_MAX_MEMORY_LIMIT_VIOLATION), NULL);
    break;
  case CONFIG_OUTPUT_STATUS_NM_FM_RATIO_UNSUPPORTED:
    pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_NM_FM_RATIO_UNSUPPORTED), NULL);
    break;
  case CONFIG_OUTPUT_STATUS_POPULATION_ISSUE:
    pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_POPULATION_VIOLATION), NULL);
    break;
  default:
    pCoutErrorMessage = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_STATUS_UNKNOWN), NULL);
    break;
  }

  pTmpStr1 = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_DIAG_COUT_CONFIG_DETAILED_STATUS), NULL);

  pReturnStr = CatSPrintClean(NULL, pTmpStr1, CoutStatus, pCoutErrorMessage, PartitionSizeChangeTableStatusStr,
    InterleaveInformationTableStatus_1Str, InterleaveInformationTableStatus_2Str);

  FREE_POOL_SAFE(pTmpStr1);
  FREE_POOL_SAFE(pCoutErrorMessage);

  NVDIMM_EXIT();
  return pReturnStr;
}

/**
  Check CCUR table for a broken interleave set and update the list of broken interleave sets if one is discovered

  @param[in] pCurrentConfig, CCUR table to check for broken interleave set when DIMM(s) are misplaced
  @param[in] MissingDimm Flag to indicate if one of the DIMMs in an interleave set is missing
  @param[in out] pBrokenISs array of broken IS information to update
  @param[in out] pBrokenISCount, current number of discovered broken IS's in the array

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
STATIC
EFI_STATUS
UpdateBrokenInterleaveSets(
  IN     NVDIMM_CURRENT_CONFIG *pCurrentConfig,
  IN     BOOLEAN MissingDimm,
  IN OUT BROKEN_IS *pBrokenISs,
  IN OUT UINT16 *pBrokenISCount
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  DIMM *pDimm = NULL;
  UINT16 Index = 0;
  UINT16 BrokenISArrayIndex = 0;
  UINT16 DimmIdIndex = 0;
  BOOLEAN BrokenISFound = FALSE;
  BOOLEAN DimmIdFound = FALSE;
  BOOLEAN DimmLocationIssue = FALSE;
  BOOLEAN PcdRevision_1 = FALSE;
  ACPI_REVISION PcdRevision;
  UINT32 TmpDimmSerialNumber = 0;
  DIMM_UNIQUE_IDENTIFIER TmpDimmUid;
  DIMM_LOCATION DimmLocation;
  DIMM_LOCATION CurrentDimmLocation;
  PMTT_MODULE_INFO *pPmttModuleInfo = NULL;
  UINT8 NumOfDimmsInInterleaveSet = 0;
  UINT16 InterleaveSetIndex = 0;
  UINT16 InterleaveInfoHeaderLength = 0;
  VOID *pCurrentIdentInfo = NULL;

  NVDIMM_ENTRY();

  if (pCurrentConfig == NULL || pBrokenISs == NULL || pBrokenISCount == NULL || *pBrokenISCount > MAX_IS_CONFIGS) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Invalid Parameter");
    goto Finish;
  }

  if (IS_ACPI_HEADER_REV_INVALID(pCurrentConfig)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Error: Invalid revision value %d for PCD Current Config table.", pCurrentConfig->Header.Revision.AsUint8);
    goto Finish;
  }

  ZeroMem(&TmpDimmUid, sizeof(TmpDimmUid));
  PcdRevision_1 = (pCurrentConfig->Header.Revision.AsUint8 == NVDIMM_CONFIGURATION_TABLES_REVISION_1);
  PcdRevision.AsUint8 = pCurrentConfig->Header.Revision.AsUint8;

  // For DIMM misplaced or re-ordering issue use DIMM location data in CCUR (PCD Rev 1.1) for validation
  if (!MissingDimm && !IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
    NVDIMM_DBG("DIMM Location details of any interleave set in CCUR table not available.");
    goto Finish;
  }

  /** Check if there is at least one PCAT table **/
  if (pCurrentConfig->Header.Length <= sizeof(*pCurrentConfig)) {
    goto Finish;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *)&pCurrentConfig->pPcatTables;
  SizeOfPcatTables = pCurrentConfig->Header.Length - (UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)pCurrentConfig);

  while ((UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)&pCurrentConfig->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *)pCurPcatTable;
      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcdRevision)) {
        NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pCurPcatTable;
        pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *)&pInterleaveInfo->pIdentificationInfoList;
        NumOfDimmsInInterleaveSet = pInterleaveInfo->NumOfDimmsInInterleaveSet;
        InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
        InterleaveInfoHeaderLength = pInterleaveInfo->Header.Length;
      }
      else if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
        NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pCurPcatTable;
        pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *)&pInterleaveInfo->pIdentificationInfoList;
        NumOfDimmsInInterleaveSet = pInterleaveInfo->NumOfDimmsInInterleaveSet;
        InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
        InterleaveInfoHeaderLength = pInterleaveInfo->Header.Length;
      }

      // for each dimm identifier in the table
      for (Index = 0; Index < NumOfDimmsInInterleaveSet; Index++) {
        if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcdRevision)) {
          NVDIMM_IDENTIFICATION_INFORMATION *pCurrentIdentInfoTemp = (NVDIMM_IDENTIFICATION_INFORMATION *)pCurrentIdentInfo;
          TmpDimmSerialNumber = pCurrentIdentInfoTemp->DimmIdentification.Version1.DimmSerialNumber;
          CopyMem_S(&TmpDimmUid, sizeof(DIMM_UNIQUE_IDENTIFIER), &(pCurrentIdentInfoTemp->DimmIdentification.Version2.Uid), sizeof(DIMM_UNIQUE_IDENTIFIER));
        }
        else if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
          NVDIMM_IDENTIFICATION_INFORMATION3 *pCurrentIdentInfoTemp = (NVDIMM_IDENTIFICATION_INFORMATION3 *)pCurrentIdentInfo;
          CopyMem_S(&TmpDimmUid, sizeof(DIMM_UNIQUE_IDENTIFIER), &(pCurrentIdentInfoTemp->DimmIdentification), sizeof(DIMM_UNIQUE_IDENTIFIER));
          CopyMem_S(&DimmLocation, sizeof(DIMM_LOCATION), &(pCurrentIdentInfoTemp->DimmLocation), sizeof(DIMM_LOCATION));
        }

        if (PcdRevision_1) {
          pDimm = GetDimmBySerialNumber(&gNvmDimmData->PMEMDev.Dimms, TmpDimmSerialNumber);
        }
        else {
          pDimm = GetDimmByUniqueIdentifier(&gNvmDimmData->PMEMDev.Dimms, TmpDimmUid);
        }

        if (!MissingDimm) {
          if (pDimm != NULL) {
            pPmttModuleInfo = GetDimmModuleByPidFromPmtt(pDimm->DimmID, gNvmDimmData->PMEMDev.pPmttHead);
            if (pPmttModuleInfo == NULL) {
              NVDIMM_DBG("DIMM Module not found in PMTT");
            }
          }
          else {
            NVDIMM_DBG("DIMM ID missing from DIMM Inventory. Incorrect Config Status value!");
            ReturnCode = EFI_INVALID_PARAMETER;
            goto Finish;
          }

          DimmLocationIssue = FALSE;
          //Compare the Identification Info DIMM location with the current DIMM location in PMTT
          if (pPmttModuleInfo != NULL) {
            if ((DimmLocation.Split.SocketId != pPmttModuleInfo->SocketId) ||
              (DimmLocation.Split.DieId != pPmttModuleInfo->DieId) ||
              (DimmLocation.Split.MemControllerId != pPmttModuleInfo->MemControllerId) ||
              (DimmLocation.Split.ChannelId != pPmttModuleInfo->ChannelId) ||
              (DimmLocation.Split.SlotId != pPmttModuleInfo->SlotId)) {
              DimmLocationIssue = TRUE;
              CurrentDimmLocation.Split.SocketId = pPmttModuleInfo->SocketId;
              CurrentDimmLocation.Split.DieId = pPmttModuleInfo->DieId;
              CurrentDimmLocation.Split.MemControllerId = pPmttModuleInfo->MemControllerId;
              CurrentDimmLocation.Split.ChannelId = pPmttModuleInfo->ChannelId;
              CurrentDimmLocation.Split.SlotId = pPmttModuleInfo->SlotId;
            }
          }
          //Compare the Identification Info DIMM location with the current DIMM location in NFIT if PMMT not present
          else {
            if ((DimmLocation.Split.SocketId != pDimm->SocketId) ||
              (DimmLocation.Split.DieId != MAX_DIEID_SINGLE_DIE_SOCKET) ||
              (DimmLocation.Split.MemControllerId != pDimm->ImcId) ||
              (DimmLocation.Split.ChannelId != pDimm->ChannelId) ||
              (DimmLocation.Split.SlotId != pDimm->ChannelPos)) {
              DimmLocationIssue = TRUE;
              CurrentDimmLocation.Split.SocketId = pDimm->SocketId;
              CurrentDimmLocation.Split.DieId = MAX_DIEID_SINGLE_DIE_SOCKET;
              CurrentDimmLocation.Split.MemControllerId = pDimm->ImcId;
              CurrentDimmLocation.Split.ChannelId = pDimm->ChannelId;
              CurrentDimmLocation.Split.SlotId = pDimm->ChannelPos;
            }
          }
        }

        // If a certain dimm identifier is missing from DimmInventory or not in the right order
        // for a given Interleave Table, mark it as a bad Interleave set index
        if (pDimm == NULL || DimmLocationIssue) {
          BrokenISFound = FALSE;
          // Check and see if index is already in the bad list
          for (BrokenISArrayIndex = 0; BrokenISArrayIndex < *pBrokenISCount; BrokenISArrayIndex++) {
            if (pBrokenISs[BrokenISArrayIndex].InterleaveSetIndex == InterleaveSetIndex) {
              BrokenISFound = TRUE;
              break;
            }
          }

          // if yes check and see if serial number is in the bad struct and add it if not
          if (BrokenISFound) {
            DimmIdFound = FALSE;
            for (DimmIdIndex = 0; DimmIdIndex < pBrokenISs[BrokenISArrayIndex].MissingDimmCount; DimmIdIndex++) {
              if (((PcdRevision_1) && (TmpDimmSerialNumber == pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[DimmIdIndex].SerialNumber)) ||
                ((!PcdRevision_1) && (0 == CompareMem(&TmpDimmUid, &pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[DimmIdIndex],
                  sizeof(DIMM_UNIQUE_IDENTIFIER))))) {
                DimmIdFound = TRUE;
                break;
              }
            }

            if (!DimmIdFound) {
              if (PcdRevision_1) {
                pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount].SerialNumber = TmpDimmSerialNumber;
              }
              else {
                CopyMem_S(&pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount], sizeof(DIMM_UNIQUE_IDENTIFIER),
                  &TmpDimmUid, sizeof(DIMM_UNIQUE_IDENTIFIER));
              }
              if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
                CopyMem_S(&pBrokenISs[BrokenISArrayIndex].MisplacedDimmLocations[pBrokenISs[BrokenISArrayIndex].MissingDimmCount], sizeof(DIMM_LOCATION),
                  &DimmLocation, sizeof(DIMM_LOCATION));
                if (!MissingDimm) {
                  CopyMem_S(&pBrokenISs[BrokenISArrayIndex].CurrentDimmLocations[pBrokenISs[BrokenISArrayIndex].MissingDimmCount], sizeof(DIMM_LOCATION),
                    &CurrentDimmLocation, sizeof(DIMM_LOCATION));
                }
              }
              pBrokenISs[BrokenISArrayIndex].MissingDimmCount++;
            }
            // if no add to the next empty index
          }
          else {
            pBrokenISs[*pBrokenISCount].InterleaveSetIndex = InterleaveSetIndex;
            if (PcdRevision_1) {
              pBrokenISs[*pBrokenISCount].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount].SerialNumber = TmpDimmSerialNumber;
            }
            else {
              CopyMem_S(&pBrokenISs[*pBrokenISCount].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount], sizeof(DIMM_UNIQUE_IDENTIFIER),
                &TmpDimmUid, sizeof(DIMM_UNIQUE_IDENTIFIER));
            }
            if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
              CopyMem_S(&pBrokenISs[*pBrokenISCount].MisplacedDimmLocations[pBrokenISs[BrokenISArrayIndex].MissingDimmCount], sizeof(DIMM_LOCATION),
                &DimmLocation, sizeof(DIMM_LOCATION));
              if (!MissingDimm) {
                CopyMem_S(&pBrokenISs[*pBrokenISCount].CurrentDimmLocations[pBrokenISs[BrokenISArrayIndex].MissingDimmCount], sizeof(DIMM_LOCATION),
                  &CurrentDimmLocation, sizeof(DIMM_LOCATION));
              }
            }
            pBrokenISs[*pBrokenISCount].MissingDimmCount++;
            *pBrokenISCount = *pBrokenISCount + 1;
          }
        }

        if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcdRevision)) {
          pCurrentIdentInfo = (UINT8 *)pCurrentIdentInfo + sizeof(NVDIMM_IDENTIFICATION_INFORMATION);
        }
        else if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
          pCurrentIdentInfo = (UINT8 *)pCurrentIdentInfo + sizeof(NVDIMM_IDENTIFICATION_INFORMATION3);
        }
      }

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, InterleaveInfoHeaderLength);
    }
    else {
      NVDIMM_DBG("Unknown table discovered in CCUR");
      break;
    }
  }

  Finish:
    NVDIMM_EXIT_I64(ReturnCode);
    return ReturnCode;
}

/**
  Check the platform configuration data for errors

  @param[in] pDimm Pointer to the DIMM
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[in out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_NOT_FOUND Unable to locate relevant PCAT tables.
**/
STATIC
EFI_STATUS
CheckPlatformConfigurationData(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pDetailedStatusStr = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  NVDIMM_CURRENT_CONFIG *pPcdCurrentConf = NULL;
  NVDIMM_PLATFORM_CONFIG_OUTPUT *pPcdOutputConf = NULL;
  NVDIMM_PLATFORM_CONFIG_INPUT *pPcdInputConf = NULL;
  UINT32 PartitionSizeChangeTableStatus = MAX_UINT32;
  UINT8 InterleaveInformationTableStatus_1 = MAX_UINT8;
  UINT8 InterleaveInformationTableStatus_2 = MAX_UINT8;
  UINT16 Index = 0;
  UINT16 Index2 = 0;
  UINT16 BrokenISCount = 0;
  BROKEN_IS *pBrokenISs = NULL;
  CHAR16 *pTmpDimmIdStr = NULL;
  ACPI_REVISION PcdRevision;

  enum PcdErrorTypes
  {
    PcdSuccess = 0,
    PcdErrorGoalData,
    PcdErrorInsufficientResources,
    PcdErrorFirmware,
    PcdErrorMissingDimm,
    PcdErrorDimmLocationIssue,
    PcdErrorCurConfig,
    PcdErrorUnknown,
  } PcdErrorType;

  PcdErrorType = PcdSuccess;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(DimmUid, sizeof(DimmUid));
  ZeroMem(&PcdRevision, sizeof(PcdRevision));

  if (DimmCount == 0 || ppDimms == NULL || DimmCount > MAX_DIMMS ||
       ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  pBrokenISs = AllocateZeroPool(sizeof(*pBrokenISs) * MAX_IS_CONFIGS);

  if (pBrokenISs == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; ++Index) {

    ReturnCode = GetDimmUid(ppDimms[Index], DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetDimmUid function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = GetPreferredValueAsString(ppDimms[Index]->DeviceHandle.AsUint32, DimmUid, DimmIdPreference == DISPLAY_DIMM_ID_HANDLE,
       DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetPreferredValueAsString function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], FALSE, &pPcdConfHeader);
#ifdef MEMORY_CORRUPTION_WA
    if (ReturnCode == EFI_DEVICE_ERROR) {
       ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], FALSE, &pPcdConfHeader);
    }
#endif // MEMORY_CORRUPTION_WA
    if (!EFI_ERROR(ReturnCode)) {
      if (pPcdConfHeader->CurrentConfStartOffset == 0 || pPcdConfHeader->CurrentConfDataSize == 0) {
        // Dimm not configured
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_DIMM_NOT_CONFIGURED), EVENT_CODE_606, DIAG_STATE_MASK_OK, ppResultStr, pDiagState,
          DimmStr);
        FREE_POOL_SAFE(pPcdConfHeader);
        NVDIMM_WARN("There is no Current Config table");
        continue;
      }

      pPcdCurrentConf = GET_NVDIMM_CURRENT_CONFIG(pPcdConfHeader);

      if (!IsPcdCurrentConfHeaderValid(pPcdCurrentConf, ppDimms[Index]->PcdOemPartitionSize)) {
        ReturnCode = EFI_VOLUME_CORRUPTED;
      }

      PcdRevision.AsUint8 = pPcdCurrentConf->Header.Revision.AsUint8;
    }

    if (!EFI_ERROR(ReturnCode)) {
      // Check for any errors in PCD CCUR
      switch (pPcdCurrentConf->ConfigStatus) {
      case DIMM_CONFIG_SUCCESS:
        PcdErrorType = PcdSuccess;
        break;
      case DIMM_CONFIG_IS_INCOMPLETE:
        PcdErrorType = PcdErrorMissingDimm;
        break;
      case DIMM_CONFIG_NO_MATCHING_IS:
      case DIMM_CONFIG_NEW_DIMM:
        PcdErrorType = PcdErrorDimmLocationIssue;
        break;
      case DIMM_CONFIG_DCPMM_POPULATION_ISSUE:
      case DIMM_CONFIG_PM_MAPPED_VM_POPULATION_ISSUE:
      case DIMM_CONFIG_DCPMM_NM_FM_RATIO_UNSUPPORTED:
      case DIMM_CONFIG_CPU_MAX_MEMORY_LIMIT_VIOLATION:
        PcdErrorType = PcdErrorCurConfig;
        break;
      }

      if (PcdErrorType != PcdSuccess) {
        goto ProcessPcdError;
      }

      if (pPcdConfHeader->ConfInputStartOffset == 0 || pPcdConfHeader->ConfInputDataSize == 0) {
        FREE_POOL_SAFE(pPcdConfHeader);
        NVDIMM_WARN("There is no Input Config table");
        continue;
      }

      pPcdInputConf = GET_NVDIMM_PLATFORM_CONFIG_INPUT(pPcdConfHeader);

      if (!IsPcdConfInputHeaderValid(pPcdInputConf, ppDimms[Index]->PcdOemPartitionSize)) {
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (pPcdConfHeader->ConfOutputStartOffset == 0 || pPcdConfHeader->ConfOutputDataSize == 0) {
        // No Output table defined yet means the goal has not been applied yet
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_GOAL_NOT_APPLIED), EVENT_CODE_609, DIAG_STATE_MASK_OK, ppResultStr, pDiagState,
          DimmStr);
      }
    }

    if (!EFI_ERROR(ReturnCode)) {
      if (pPcdConfHeader->ConfOutputStartOffset == 0 || pPcdConfHeader->ConfOutputDataSize == 0) {
        FREE_POOL_SAFE(pPcdConfHeader);
        NVDIMM_WARN("There is no Output Config table");
        continue;
      }

      pPcdOutputConf = GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(pPcdConfHeader);

      if (!IsPcdConfOutputHeaderValid(pPcdOutputConf, ppDimms[Index]->PcdOemPartitionSize)) {
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (pPcdOutputConf->SequenceNumber != pPcdInputConf->SequenceNumber) {
        // The goal has not been applied yet
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_GOAL_NOT_APPLIED), EVENT_CODE_609, DIAG_STATE_MASK_OK, ppResultStr, pDiagState,
          DimmStr);
      }
    }

    if (EFI_ERROR(ReturnCode)) {
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_INVALID_PCD_DATA), EVENT_CODE_621, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        DimmStr);
      continue;
    }

    GetPlatformConfigOutputPCATStatus(
      pPcdOutputConf,
      &PartitionSizeChangeTableStatus,
      &InterleaveInformationTableStatus_1,
      &InterleaveInformationTableStatus_2);

    // Check for any errors in applying the goal request (PCD CIN)
    switch (pPcdCurrentConf->ConfigStatus) {
    case DIMM_CONFIG_SUCCESS:
      PcdErrorType = PcdSuccess;
      break;
    case DIMM_CONFIG_OLD_CONFIG_USED:
    case DIMM_CONFIG_BAD_CONFIG:
      if (pPcdOutputConf->ValidationStatus == CONFIG_OUTPUT_STATUS_ERROR) {
        if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_FW_ERROR) {
          PcdErrorType = PcdErrorFirmware;
        } else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_EXCEED_DRAM_DECODERS ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_EXCEED_DRAM_DECODERS ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_EXCEED_MAX_SPA_SPACE ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_MIRROR_FAILED ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_EXCEED_DRAM_DECODERS ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_EXCEED_MAX_SPA_SPACE ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_MIRROR_FAILED) {
          PcdErrorType = PcdErrorInsufficientResources;
        } else if (PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_DIMM_MISSING ||
          PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_ISET_MISSING ||
          PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_EXCEED_SIZE ||
          PartitionSizeChangeTableStatus == PARTITION_SIZE_CHANGE_STATUS_UNSUPPORTED_ALIGNMENT ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_DIMM_MISSING ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_ISET_MISSING ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_CIN_MISSING ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_REQUEST_UNSUPPORTED ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_DIMM_MISSING ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_ISET_MISSING ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_CIN_MISSING ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_REQUEST_UNSUPPORTED ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT) {
          PcdErrorType = PcdErrorGoalData;
        } else {
          PcdErrorType = PcdErrorUnknown;
        }
      } else if (pPcdOutputConf->ValidationStatus == CONFIG_OUTPUT_STATUS_CPU_MAX_MEMORY_LIMIT_VIOLATION ||
        pPcdOutputConf->ValidationStatus == CONFIG_OUTPUT_STATUS_NM_FM_RATIO_UNSUPPORTED ||
        pPcdOutputConf->ValidationStatus == CONFIG_OUTPUT_STATUS_POPULATION_ISSUE) {
        PcdErrorType = PcdErrorGoalData;
      } else {
        PcdErrorType = PcdErrorUnknown;
      }
      break;
    case DIMM_CONFIG_IN_CHECKSUM_NOT_VALID:
    case DIMM_CONFIG_REVISION_NOT_SUPPORTED:
      PcdErrorType = PcdErrorGoalData;
      break;
    default:
      PcdErrorType = PcdErrorUnknown;
    }

  ProcessPcdError:
    // Print Message based on PcdError
    switch (PcdErrorType) {
    case PcdSuccess:
      break;
    case PcdErrorFirmware:
      pDetailedStatusStr = GetCoutDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_GOAL_FAILED_FIRMWARE), EVENT_CODE_626, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        DimmStr, pDetailedStatusStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      break;
    case PcdErrorGoalData:
      pDetailedStatusStr = GetCoutDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_GOAL_FAILED_DATA), EVENT_CODE_624, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        DimmStr, pDetailedStatusStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      break;
    case PcdErrorCurConfig:
      pDetailedStatusStr = GetCCurDetailedStatusStr(pPcdCurrentConf->ConfigStatus);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CURRENT_CONFIG_FAILED_DATA), EVENT_CODE_633, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        DimmStr, pDetailedStatusStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      break;
    case PcdErrorInsufficientResources:
      pDetailedStatusStr = GetCoutDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_GOAL_FAILED_INSUFFICIENT_RESOURCES), EVENT_CODE_625, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        DimmStr, pDetailedStatusStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      break;
    case PcdErrorMissingDimm:
      ReturnCode = UpdateBrokenInterleaveSets(pPcdCurrentConf, TRUE, pBrokenISs, &BrokenISCount);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to update broken interleave sets");
        *pDiagState |= DIAG_STATE_MASK_ABORTED;
        goto Finish;
      }
      break;
    case PcdErrorDimmLocationIssue:
      ReturnCode = UpdateBrokenInterleaveSets(pPcdCurrentConf, FALSE, pBrokenISs, &BrokenISCount);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to update broken interleave sets");
        *pDiagState |= DIAG_STATE_MASK_ABORTED;
        goto Finish;
      }
      break;
    case PcdErrorUnknown:
    default:
      pDetailedStatusStr = GetCoutDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_GOAL_FAILED_UNKNOWN), EVENT_CODE_627, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        DimmStr, pDetailedStatusStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      break;
    }

    FREE_POOL_SAFE(pPcdConfHeader);
  }

  for (Index = 0; Index < BrokenISCount; Index++) {
    if (pBrokenISs[Index].MissingDimmCount > 0) {
      for (Index2 = 0; Index2 < pBrokenISs[Index].MissingDimmCount; Index2++) {
        if (IS_ACPI_REV_MAJ_0_MIN_2(PcdRevision)) {
          pTmpDimmIdStr = CatSPrint(NULL, L"%04x-%02x-%04x-%08x", EndianSwapUint16(pBrokenISs[Index].MissingDimmIdentifier[Index2].ManufacturerId),
          pBrokenISs[Index].MissingDimmIdentifier[Index2].ManufacturingLocation,
          EndianSwapUint16(pBrokenISs[Index].MissingDimmIdentifier[Index2].ManufacturingDate),
         EndianSwapUint32(pBrokenISs[Index].MissingDimmIdentifier[Index2].SerialNumber));
        } else {
          pTmpDimmIdStr = CatSPrint(NULL, L"0x%08x", EndianSwapUint32(pBrokenISs[Index].MissingDimmIdentifier[Index2].SerialNumber));
        }

        if (PcdErrorType == PcdErrorMissingDimm) {
          if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision)) {
            APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_IS_BROKEN_DIMMS_MISSING_LOCATION), EVENT_CODE_631, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
              pBrokenISs[Index].InterleaveSetIndex, pTmpDimmIdStr, pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.SocketId,
              pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.DieId, pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.MemControllerId,
              pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.ChannelId, pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.SlotId);
          }
          else {
            APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_IS_BROKEN_DIMMS_MISSING), EVENT_CODE_628, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
              pBrokenISs[Index].InterleaveSetIndex, pTmpDimmIdStr);
          }
        }

        if (PcdErrorType == PcdErrorDimmLocationIssue && (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(PcdRevision))) {
          APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_IS_BROKEN_DIMMS_MISPLACED_LOCATION), EVENT_CODE_632, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
            pBrokenISs[Index].InterleaveSetIndex, pTmpDimmIdStr, pBrokenISs[Index].CurrentDimmLocations[Index2].Split.SocketId,
            pBrokenISs[Index].CurrentDimmLocations[Index2].Split.DieId, pBrokenISs[Index].CurrentDimmLocations[Index2].Split.MemControllerId,
            pBrokenISs[Index].CurrentDimmLocations[Index2].Split.ChannelId, pBrokenISs[Index].CurrentDimmLocations[Index2].Split.SlotId,
            pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.SocketId, pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.DieId,
            pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.MemControllerId, pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.ChannelId,
            pBrokenISs[Index].MisplacedDimmLocations[Index2].Split.SlotId);
        }
        FREE_POOL_SAFE(pTmpDimmIdStr);
      }
    }
  }

Finish:
  FREE_POOL_SAFE(pPcdConfHeader);
  FREE_POOL_SAFE(pBrokenISs);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check for uninitialized dimms in the system

  @param[in out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state
  @param  DimmIdPreference Dimm id preference value

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_NOT_FOUND Unable to locate relevant PCAT tables.
**/
STATIC
EFI_STATUS
CheckUninitializedDimms(
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState,
  IN     UINT8 DimmIdPreference
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pNode = NULL;
  DIMM *pCurDimm = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];

  NVDIMM_ENTRY();

  if (ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurDimm = DIMM_FROM_NODE(pNode);
    if (pCurDimm->NonFunctional == FALSE) {
      continue;
    }
    ZeroMem(DimmStr, sizeof(DimmStr));
    ZeroMem(DimmUid, sizeof(DimmUid));
    ReturnCode = GetDimmUid(pCurDimm, DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetDimmUid function for DIMM ID 0x%x failed.", pCurDimm->DeviceHandle.AsUint32);
      continue;
    }
    ReturnCode = GetPreferredValueAsString(pCurDimm->DeviceHandle.AsUint32, DimmUid, DimmIdPreference == DISPLAY_DIMM_ID_HANDLE,
      DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetPreferredValueAsString function for DIMM ID 0x%x failed.", pCurDimm->DeviceHandle.AsUint32);
      continue;
    }
    APPEND_RESULT_TO_THE_LOG(pCurDimm, STRING_TOKEN(STR_CONFIG_DIMM_FAILED_TO_INITIALIZE), EVENT_CODE_618, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
      DimmStr);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check the various supported system capabilities

  @param[in] pDimm Pointer to the DIMM
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[in out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_NOT_FOUND Unable to locate relevant PCAT tables.
**/
STATIC
EFI_STATUS
CheckSystemSupportedCapabilities(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BOOLEAN ConfigChangeSupported = FALSE;

  NVDIMM_ENTRY();

  if (DimmCount == 0 || ppDimms == NULL || DimmCount > MAX_DIMMS ||
       ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  /** Check if BIOS supports changing configuration through management software **/
  ReturnCode = CheckIfBiosSupportsConfigChange(&ConfigChangeSupported);
  if (ReturnCode == EFI_LOAD_ERROR) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  } else if (ReturnCode == EFI_UNSUPPORTED || !ConfigChangeSupported) {
    APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_NO_OS_PROVISIONING), EVENT_CODE_623, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check for duplicate Dimm UID values in the platform, and accordingly append to
  the platform config diagnostics result.
  Also, accordingly modifies the test-state.

  @param[in] pDimm Pointer to the DIMM
  @param[in] DimmCount DIMMs count
  @param[in out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_OUT_OF_RESOURCES when memory allocation fails.
**/
STATIC
EFI_STATUS
CheckDimmUIDDuplication(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 PossibleNumDuplicateUID = 0;
  UINT16 NumDuplicateUID = 0;
  UINT16 TotalNumDuplicateUID = 0;
  CHAR16 **ppDuplicateDimmUids = NULL;
  UINT32 Index = 0;
  UINT32 TestUIDIndex = 0;
  UINT32 DuplicateUIDIndex = 0;
  CHAR16 CandidateDimmUID[MAX_DIMM_UID_LENGTH];
  CHAR16 TestDimmUID[MAX_DIMM_UID_LENGTH];
  BOOLEAN DuplicateFound = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(CandidateDimmUID, sizeof(CandidateDimmUID));
  ZeroMem(TestDimmUID, sizeof(TestDimmUID));

  if (DimmCount == 0 || ppDimms == NULL || DimmCount > MAX_DIMMS ||
       ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  PossibleNumDuplicateUID = DimmCount / 2;

  ppDuplicateDimmUids = AllocateZeroPool(sizeof(*ppDuplicateDimmUids) * PossibleNumDuplicateUID);

  if (ppDuplicateDimmUids == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; Index++) {
    DuplicateFound = FALSE;
    NumDuplicateUID = 1;

    ReturnCode = GetDimmUid(ppDimms[Index], CandidateDimmUID, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetDimmUid function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
      goto Finish;
    }

    for (DuplicateUIDIndex = 0; DuplicateUIDIndex < TotalNumDuplicateUID; DuplicateUIDIndex++) {
      if (StrICmp(CandidateDimmUID, ppDuplicateDimmUids[DuplicateUIDIndex]) == 0) {
        DuplicateFound = TRUE;
        break;
      }
    }

    if (DuplicateFound) {
      continue;
    }

    for (TestUIDIndex = Index + 1; TestUIDIndex < DimmCount; TestUIDIndex++) {

      ZeroMem(TestDimmUID, sizeof(TestDimmUID));

      ReturnCode = GetDimmUid(ppDimms[TestUIDIndex], TestDimmUID, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("GetDimmUid function for DIMM ID 0x%x failed.", ppDimms[TestUIDIndex]->DeviceHandle.AsUint32);
        *pDiagState |= DIAG_STATE_MASK_ABORTED;
        goto Finish;
      }
      if (StrICmp(TestDimmUID, CandidateDimmUID) == 0) {
        NumDuplicateUID++;
      }
    }

    if (NumDuplicateUID > 1) {
      ppDuplicateDimmUids[TotalNumDuplicateUID] = AllocateZeroPool(MAX_DIMM_UID_LENGTH * sizeof(CHAR16));

      if (ppDuplicateDimmUids[TotalNumDuplicateUID] == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        *pDiagState |= DIAG_STATE_MASK_ABORTED;
        goto Finish;
      }

      CopyMem_S(ppDuplicateDimmUids[TotalNumDuplicateUID], MAX_DIMM_UID_LENGTH * sizeof(CHAR16), CandidateDimmUID, MAX_DIMM_UID_LENGTH * sizeof(CHAR16));
      TotalNumDuplicateUID++;

      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_CONFIG_DUPLICATE_DIMM_UID), EVENT_CODE_608, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        NumDuplicateUID, CandidateDimmUID);
    }
  }

Finish:
  if (ppDuplicateDimmUids != NULL) {
    for (DuplicateUIDIndex = 0; DuplicateUIDIndex < PossibleNumDuplicateUID; DuplicateUIDIndex++) {
      FREE_POOL_SAFE(ppDuplicateDimmUids[DuplicateUIDIndex]);
    }
  }
  FREE_POOL_SAFE(ppDuplicateDimmUids);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Run platform configuration diagnostics for the list of DIMMs, and
  appropriately populate the result structure.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] pResult Pointer of structure with diagnostics test result

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
RunConfigDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  OUT DIAG_INFO *pResult
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  SYSTEM_CAPABILITIES_INFO SysCapInfo;

  NVDIMM_ENTRY();
  // Clear the pointer before using the struct
  SysCapInfo.PtrInterleaveFormatsSupported = 0;
  SysCapInfo.PtrInterleaveSize = 0;

  if (pResult == NULL || DimmCount > MAX_DIMMS) {

    NVDIMM_DBG("The platform configuration diagnostics test aborted due to an internal error.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmCount == 0 || ppDimms == NULL) {
    ReturnCode = EFI_SUCCESS;
    APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_NO_MANAGEABLE_DIMMS), EVENT_CODE_601, DIAG_STATE_MASK_OK,
      &pResult->Message, &pResult->StateVal);
    goto Finish;
  }

  pResult->SubTestName[DIMMSPECS_TEST_INDEX] = CatSPrint(NULL, L"Dimm specs");
  ReturnCode = CheckUninitializedDimms(&pResult->SubTestMessage[DIMMSPECS_TEST_INDEX], &pResult->SubTestStateVal[DIMMSPECS_TEST_INDEX],DimmIdPreference);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for uninitialized dimms failed.");
    if ((pResult->SubTestStateVal[DIMMSPECS_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_ABORTED_INTERNAL_ERROR), EVENT_CODE_630, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[DIMMSPECS_TEST_INDEX], &pResult->SubTestStateVal[DIMMSPECS_TEST_INDEX]);
      goto Finish;
    }
  }

  pResult->SubTestName[DUPLICATE_DIMM_TEST_INDEX] = CatSPrint(NULL, L"Duplicate Dimm");
  ReturnCode = CheckDimmUIDDuplication(ppDimms, DimmCount, &pResult->SubTestMessage[DUPLICATE_DIMM_TEST_INDEX], &pResult->SubTestStateVal[1]);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for duplicate UID numbers failed.");
    if ((pResult->SubTestStateVal[DUPLICATE_DIMM_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_ABORTED_INTERNAL_ERROR), EVENT_CODE_630, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[DUPLICATE_DIMM_TEST_INDEX], &pResult->SubTestStateVal[DUPLICATE_DIMM_TEST_INDEX]);
      goto Finish;
    }
  }

  pResult->SubTestName[SYSTEMCAP_TEST_INDEX] = CatSPrint(NULL, L"System Capability");
  ReturnCode = GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig, &SysCapInfo);
  if ((pResult->SubTestStateVal[SYSTEMCAP_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
    APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_ABORTED_INTERNAL_ERROR), EVENT_CODE_630, DIAG_STATE_MASK_ABORTED,
      &pResult->SubTestMessage[SYSTEMCAP_TEST_INDEX], &pResult->SubTestStateVal[SYSTEMCAP_TEST_INDEX]);
    goto Finish;
  }

  if (!SysCapInfo.AdrSupported) {
    APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_NO_ADR_SUPPORT), EVENT_CODE_629, DIAG_STATE_MASK_FAILED,
      &pResult->SubTestMessage[SYSTEMCAP_TEST_INDEX], &pResult->SubTestStateVal[SYSTEMCAP_TEST_INDEX]);
  }

  ReturnCode = CheckSystemSupportedCapabilities(ppDimms, DimmCount, DimmIdPreference, &pResult->SubTestMessage[SYSTEMCAP_TEST_INDEX], &pResult->SubTestStateVal[SYSTEMCAP_TEST_INDEX]);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for System supported capabilities failed.");
    if ((pResult->SubTestStateVal[SYSTEMCAP_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_ABORTED_INTERNAL_ERROR), EVENT_CODE_630, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[SYSTEMCAP_TEST_INDEX], &pResult->SubTestStateVal[SYSTEMCAP_TEST_INDEX]);
      goto Finish;
    }
  }

  pResult->SubTestName[NAMESPACE_LSA_TEST_INDEX] = CatSPrint(NULL, L"Namespace LSA");
  ReturnCode = CheckNamespaceLabelAreaIndex(ppDimms, DimmCount, DimmIdPreference, &pResult->SubTestMessage[NAMESPACE_LSA_TEST_INDEX], &pResult->SubTestStateVal[NAMESPACE_LSA_TEST_INDEX]);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for Namespace label retrieve failed.");
    if ((pResult->SubTestStateVal[NAMESPACE_LSA_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_ABORTED_INTERNAL_ERROR), EVENT_CODE_630, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[NAMESPACE_LSA_TEST_INDEX], &pResult->SubTestStateVal[NAMESPACE_LSA_TEST_INDEX]);
      goto Finish;
    }
  }

  pResult->SubTestName[PCD_TEST_INDEX] = CatSPrint(NULL, L"PCD");
  ReturnCode = CheckPlatformConfigurationData(ppDimms, DimmCount, DimmIdPreference, &pResult->SubTestMessage[PCD_TEST_INDEX], &pResult->SubTestStateVal[PCD_TEST_INDEX]);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for platform configuration data failed.");
    if ((pResult->SubTestStateVal[PCD_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_ABORTED_INTERNAL_ERROR), EVENT_CODE_630, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[PCD_TEST_INDEX], &pResult->SubTestStateVal[PCD_TEST_INDEX]);
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;
  goto Finish;

Finish:
  FREE_HII_POINTER(SysCapInfo.PtrInterleaveFormatsSupported);
  FREE_HII_POINTER(SysCapInfo.PtrInterleaveSize);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
