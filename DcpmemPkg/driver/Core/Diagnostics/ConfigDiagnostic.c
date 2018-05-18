/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Namespace.h>
#include "ConfigDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#ifdef OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) SendTheEventAndAppendToDiagnosticsResult(pDimm,pStr,StateMask,__COUNTER__,SYSTEM_EVENT_CAT_CONFIG,ppResult,pState)
#else // OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) AppendToDiagnosticsResult(pStr,StateMask,ppResult,pState)
#endif // OS_BUILD

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
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
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
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_UNABLE_TO_READ_NS_INFO), NULL);
#ifndef OS_BUILD
      pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr);
      FREE_POOL_SAFE(pTmpStr);
#else // OS_BUILD
      pTmpStr1 = pTmpStr;
#endif // OS_BUILD
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
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
  NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = NULL;
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
      pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurPcatTable;

      if (InterleaveTableNumber == 1) {
        *pInterleaveInformationTableStatus_1 = pInterleaveInfo->InterleaveChangeStatus;
        InterleaveTableNumber++;
      } else {
        *pInterleaveInformationTableStatus_2 = pInterleaveInfo->InterleaveChangeStatus;
      }

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
    } else {
      NVDIMM_DBG("Unknown table discovered in COUT");
      break;
    }
  }
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
  @retval Formated CHAR16 string
**/
STATIC
CHAR16*
GetDetailedStatusStr(
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
  CHAR16 *pTmpStr1 = NULL;
  CHAR16 *pReturnStr = NULL;

  ZeroMem(PartitionSizeChangeTableStatusStr, sizeof(PartitionSizeChangeTableStatusStr));
  ZeroMem(InterleaveInformationTableStatus_1Str, sizeof(InterleaveInformationTableStatus_1Str));
  ZeroMem(InterleaveInformationTableStatus_2Str, sizeof(InterleaveInformationTableStatus_2Str));

  NVDIMM_ENTRY();

  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NOT_AVAILABLE), NULL);

  if (pTmpStr != NULL) {
    if (PartitionSizeChangeTableStatus == MAX_UINT32) {
      StrnCpy(PartitionSizeChangeTableStatusStr, pTmpStr, MAX_PCD_TABLE_STATUS_LENGTH - 1);
    } else {
      UnicodeSPrint(PartitionSizeChangeTableStatusStr, sizeof(PartitionSizeChangeTableStatusStr), L"%d", PartitionSizeChangeTableStatus);
    }

    if (InterleaveInformationTableStatus_1 == MAX_UINT8) {
      StrnCpy(InterleaveInformationTableStatus_1Str, pTmpStr, MAX_PCD_TABLE_STATUS_LENGTH - 1);
    } else {
      UnicodeSPrint(InterleaveInformationTableStatus_1Str, sizeof(InterleaveInformationTableStatus_1Str), L"%d", (UINT32)InterleaveInformationTableStatus_1);
    }

    if (InterleaveInformationTableStatus_2 == MAX_UINT8) {
      StrnCpy(InterleaveInformationTableStatus_2Str, pTmpStr, MAX_PCD_TABLE_STATUS_LENGTH - 1);
    } else {
      UnicodeSPrint(InterleaveInformationTableStatus_2Str, sizeof(InterleaveInformationTableStatus_2Str), L"%d", InterleaveInformationTableStatus_2);
    }

    FREE_POOL_SAFE(pTmpStr);

  }
  pTmpStr1 = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_DETAILED_STATUS), NULL);
  pReturnStr = CatSPrint(NULL, pTmpStr1, CoutStatus, PartitionSizeChangeTableStatusStr, InterleaveInformationTableStatus_1Str, InterleaveInformationTableStatus_2Str);
  FREE_POOL_SAFE(pTmpStr1);

  NVDIMM_EXIT();
  return pReturnStr;
}

/**
  Check a COUT table for a broken interleave set and update the list of broken interleave sets if one is discovered

  @param[in] pConfigOutput, COUT table to check for broken interleave set
  @param[in out] pBrokenISs array of broken IS information to update
  @param[in out] pBrokenISCount, current number of discovered broken IS's in the array

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
STATIC
EFI_STATUS
UpdateBrokenInterleaveSets(
  IN     NVDIMM_PLATFORM_CONFIG_OUTPUT *pConfigOutput,
  IN OUT BROKEN_IS *pBrokenISs,
  IN OUT UINT16 *pBrokenISCount
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  NVDIMM_IDENTIFICATION_INFORMATION *pCurrentIdentInfo = NULL;
  DIMM *pDimm = NULL;
  UINT16 Index = 0;
  UINT16 BrokenISArrayIndex = 0;
  UINT16 DimmIdIndex = 0;
  BOOLEAN BrokenISFound = FALSE;
  BOOLEAN DimmIdFound = FALSE;
  BOOLEAN PcdRevision_1 = FALSE;
  UINT32 TmpDimmSerialNumber = 0;
  DIMM_UNIQUE_IDENTIFIER TmpDimmUid;

  NVDIMM_ENTRY();

  if (pConfigOutput == NULL || pBrokenISs == NULL || pBrokenISCount == NULL || *pBrokenISCount > MAX_IS_CONFIGS) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Invalid Parameter");
    goto Finish;
  }

  if ((pConfigOutput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
      (pConfigOutput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Error: Invalid revision value %d for PCD Config Output table.", pConfigOutput->Header.Revision);
    goto Finish;
  }

  ZeroMem(&TmpDimmUid, sizeof(TmpDimmUid));
  PcdRevision_1 = (pConfigOutput->Header.Revision == NVDIMM_CONFIGURATION_TABLES_REVISION_1);

  /** Check if there is at least one PCAT table **/
  if (pConfigOutput->Header.Length <= sizeof(*pConfigOutput)) {
    goto Finish;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *) &pConfigOutput->pPcatTables;
  SizeOfPcatTables = pConfigOutput->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pConfigOutput);

  while ((UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)&pConfigOutput->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pCurPcatTable;
      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurPcatTable;
      pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *) &pInterleaveInfo->pIdentificationInfoList;

      // for each dimm identifier in the table
      for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
        TmpDimmSerialNumber = pCurrentIdentInfo->DimmIdentification.Version1.DimmSerialNumber;
        TmpDimmUid = pCurrentIdentInfo->DimmIdentification.Version2.Uid;
        if (PcdRevision_1) {
          pDimm = GetDimmBySerialNumber(&gNvmDimmData->PMEMDev.Dimms, TmpDimmSerialNumber);
        } else {
          pDimm = GetDimmByUniqueIdentifier(&gNvmDimmData->PMEMDev.Dimms, TmpDimmUid);
        }

        // If a certain dimm identifier is missing from DimmInventory for a given Interleave Table, mark it as a bad Interleave set index
        if (pDimm == NULL) {
          BrokenISFound = FALSE;
          // Check and see if index is already in the bad list
          for (BrokenISArrayIndex = 0; BrokenISArrayIndex < *pBrokenISCount; BrokenISArrayIndex++) {
            if (pBrokenISs[BrokenISArrayIndex].InterleaveSetIndex == pInterleaveInfo->InterleaveSetIndex) {
              BrokenISFound = TRUE;
              break;
            }
          }

          // if yes check and see if serial number is in the bad struct and add it if not
          if (BrokenISFound) {
            DimmIdFound = FALSE;
            for (DimmIdIndex = 0; DimmIdIndex < pBrokenISs[BrokenISArrayIndex].MissingDimmCount; DimmIdIndex++) {
              if (((PcdRevision_1) && (TmpDimmSerialNumber == pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[DimmIdIndex].SerialNumber)) ||
                  ((!PcdRevision_1) && (CompareMem(&TmpDimmUid, &pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[DimmIdIndex],
                                        sizeof(DIMM_UNIQUE_IDENTIFIER))))){
                DimmIdFound = TRUE;
                break;
              }
            }

            if (!DimmIdFound) {
              if (PcdRevision_1) {
                pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount].SerialNumber = TmpDimmSerialNumber;
              } else {
                CopyMem(&pBrokenISs[BrokenISArrayIndex].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount],
                  &TmpDimmUid, sizeof(DIMM_UNIQUE_IDENTIFIER));
              }
              pBrokenISs[BrokenISArrayIndex].MissingDimmCount++;
            }
          // if no add to the next empty index
          } else {
            pBrokenISs[*pBrokenISCount].InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
            if (PcdRevision_1) {
              pBrokenISs[*pBrokenISCount].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount].SerialNumber = TmpDimmSerialNumber;
            } else {
	      CopyMem(&pBrokenISs[*pBrokenISCount].MissingDimmIdentifier[pBrokenISs[BrokenISArrayIndex].MissingDimmCount],
                &TmpDimmUid, sizeof(DIMM_UNIQUE_IDENTIFIER));
            }
            pBrokenISs[*pBrokenISCount].MissingDimmCount++;
            *pBrokenISCount = *pBrokenISCount + 1;
          }
        }

        pCurrentIdentInfo++;
      }

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
    } else {
      NVDIMM_DBG("Unknown table discovered in COUT");
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
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
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
  CHAR16 *pDimmIdentifiersStr = NULL;
  CHAR16 *pTmpDimmIdStr = NULL;

  enum PcdErrorTypes
  {
    PcdSuccess = 0,
    PcdErrorGoalData,
    PcdErrorInsufficientResources,
    PcdErrorFirmware,
    PcdErrorMissingDimm,
    PcdErrorUnknown,
  } PcdErrorType;

  PcdErrorType = PcdSuccess;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(DimmUid, sizeof(DimmUid));

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

    ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], &pPcdConfHeader);
#ifdef MEMORY_CORRUPTION_WA
	  if (ReturnCode == EFI_DEVICE_ERROR)
	  {
		  ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], &pPcdConfHeader);
	  }
#endif // MEMORY_CORRUPTION_WA
    if (!EFI_ERROR(ReturnCode)) {
      if (pPcdConfHeader->CurrentConfStartOffset == 0 || pPcdConfHeader->CurrentConfDataSize == 0) {
        // Dimm not configured
        pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_DIMM_NOT_CONFIGURED), NULL);
        pTmpStr1 = CatSPrint(NULL, pTmpStr, ppDimms[Index]->DeviceHandle.AsUint32);
        FREE_POOL_SAFE(pTmpStr);
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_OK, ppResultStr, pDiagState);
        FREE_POOL_SAFE(pPcdConfHeader);
        NVDIMM_WARN("There is no Current Config table");
        continue;
      }

      pPcdCurrentConf = GET_NVDIMM_CURRENT_CONFIG(pPcdConfHeader);

      if (pPcdCurrentConf->Header.Signature != NVDIMM_CURRENT_CONFIG_SIG) {
        NVDIMM_DBG("Incorrect signature of the AEP Current Config table");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (pPcdCurrentConf->Header.Length > ppDimms[Index]->PcdOemPartitionSize) {
        NVDIMM_DBG("Length of PCD Current Config header is greater than max PCD OEM partition size");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if ((pPcdCurrentConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
                  (pPcdCurrentConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
        NVDIMM_DBG("Revision of PCD Current Config table is invalid");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (!IsChecksumValid(pPcdCurrentConf, pPcdCurrentConf->Header.Length)) {
        NVDIMM_DBG("The Current Config table checksum is invalid.");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      }
    }

    if (!EFI_ERROR(ReturnCode)) {
      if (pPcdConfHeader->ConfInputStartOffset == 0 || pPcdConfHeader->ConfInputDataSize == 0) {
        FREE_POOL_SAFE(pPcdConfHeader);
        NVDIMM_WARN("There is no Input Config table");
        continue;
      }

      pPcdInputConf = GET_NVDIMM_PLATFORM_CONFIG_INPUT(pPcdConfHeader);

      if (pPcdInputConf->Header.Signature != NVDIMM_CONFIGURATION_INPUT_SIG) {
        NVDIMM_DBG("Incorrect signature of the AEP Config input table");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (pPcdInputConf->Header.Length > ppDimms[Index]->PcdOemPartitionSize) {
        NVDIMM_DBG("Length of PCD Config Input header is greater than max PCD OEM partition size");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if ((pPcdInputConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
        (pPcdInputConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
        NVDIMM_DBG("Revision of PCD Config Input table is invalid");
      } else if (!IsChecksumValid(pPcdInputConf, pPcdInputConf->Header.Length)) {
        NVDIMM_DBG("The Config input table checksum is invalid.");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (pPcdConfHeader->ConfOutputStartOffset == 0 || pPcdConfHeader->ConfOutputDataSize == 0) {
        // No Output table defined yet means the goal has not been applied yet
        pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_NOT_APPLIED), NULL);
        pTmpStr1 = CatSPrint(NULL, pTmpStr, ppDimms[Index]->DeviceHandle.AsUint32);
        FREE_POOL_SAFE(pTmpStr);
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_OK, ppResultStr, pDiagState);
      }
    }

    if (!EFI_ERROR(ReturnCode)) {
      if (pPcdConfHeader->ConfOutputStartOffset == 0 || pPcdConfHeader->ConfOutputDataSize == 0) {
        FREE_POOL_SAFE(pPcdConfHeader);
        NVDIMM_WARN("There is no Output Config table");
        continue;
      }

      pPcdOutputConf = GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(pPcdConfHeader);

      if (pPcdOutputConf->Header.Signature != NVDIMM_CONFIGURATION_OUTPUT_SIG) {
        NVDIMM_DBG("Incorrect signature of the AEP Config output table");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (pPcdOutputConf->Header.Length > ppDimms[Index]->PcdOemPartitionSize) {
        NVDIMM_DBG("Length of PCD Config Output header is greater than max PCD OEM partition size");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if ((pPcdOutputConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
                  (pPcdOutputConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
        NVDIMM_DBG("Revision of PCD Config Output table is invalid");
      } else if (!IsChecksumValid(pPcdOutputConf, pPcdOutputConf->Header.Length)) {
        NVDIMM_DBG("The Config output table checksum is invalid.");
        ReturnCode = EFI_VOLUME_CORRUPTED;
      } else if (CONFIG_OUTPUT_STATUS_SUCCESS == pPcdOutputConf->ValidationStatus) {
        // Goal applied successfully
        pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_APPLIED_SUCCESSFULLY), NULL);
        pTmpStr1 = CatSPrint(NULL, pTmpStr, ppDimms[Index]->DeviceHandle.AsUint32);
        FREE_POOL_SAFE(pTmpStr);
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_OK, ppResultStr, pDiagState);
      } else if (pPcdOutputConf->SequenceNumber != pPcdInputConf->SequenceNumber) {
        // The goal has not been applied yet
        pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_NOT_APPLIED), NULL);
        pTmpStr1 = CatSPrint(NULL, pTmpStr, ppDimms[Index]->DeviceHandle.AsUint32);
        FREE_POOL_SAFE(pTmpStr);
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_OK, ppResultStr, pDiagState);
      }
    }

    if (EFI_ERROR(ReturnCode)) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_UNABLE_TO_READ_POOL_INFO), NULL);
#ifndef OS_BUILD
      pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr);
      FREE_POOL_SAFE(pTmpStr);
#else // OS_BUILD
      pTmpStr1 = pTmpStr;
#endif // OS_BUILD
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
      continue;
    }

    GetPlatformConfigOutputPCATStatus(
      pPcdOutputConf,
      &PartitionSizeChangeTableStatus,
      &InterleaveInformationTableStatus_1,
      &InterleaveInformationTableStatus_2);

    switch (pPcdCurrentConf->ConfigStatus) {
    case DIMM_CONFIG_SUCCESS:
    case DIMM_CONFIG_NEW_DIMM:
      PcdErrorType = PcdSuccess;
      break;
    case DIMM_CONFIG_IS_INCOMPLETE:
      PcdErrorType = PcdErrorMissingDimm;
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
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH ||
          InterleaveInformationTableStatus_1 == INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_DIMM_MISSING ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_ISET_MISSING ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_CIN_MISSING ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH ||
          InterleaveInformationTableStatus_2 == INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT) {
          PcdErrorType = PcdErrorGoalData;
        } else {
          PcdErrorType = PcdErrorUnknown;
        }
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

    // Print Message based on PcdError
    switch (PcdErrorType) {
    case PcdSuccess:
      break;
    case PcdErrorFirmware:
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_FAILED_FIRMWARE), NULL);
      pDetailedStatusStr = GetDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr,pDetailedStatusStr);
      FREE_POOL_SAFE(pTmpStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
      break;
    case PcdErrorGoalData:
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_FAILED_DATA), NULL);
      pDetailedStatusStr = GetDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr,pDetailedStatusStr);
      FREE_POOL_SAFE(pTmpStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
      break;
    case PcdErrorInsufficientResources:
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_FAILED_INSUFFICIENT_RESOURCES), NULL);
      pDetailedStatusStr = GetDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr,pDetailedStatusStr);
      FREE_POOL_SAFE(pTmpStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
      break;
    case PcdErrorMissingDimm:
      ReturnCode = UpdateBrokenInterleaveSets(pPcdOutputConf, pBrokenISs, &BrokenISCount);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to update broken interleave sets");
        *pDiagState |= DIAG_STATE_MASK_ABORTED;
        goto Finish;
      }
      break;
    case PcdErrorUnknown:
    default:
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_GOAL_FAILED_UNKNOWN), NULL);
      pDetailedStatusStr = GetDetailedStatusStr(pPcdOutputConf->ValidationStatus, PartitionSizeChangeTableStatus, InterleaveInformationTableStatus_1, InterleaveInformationTableStatus_2);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr,pDetailedStatusStr);
      FREE_POOL_SAFE(pTmpStr);
      FREE_POOL_SAFE(pDetailedStatusStr);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
      break;
    }

    FREE_POOL_SAFE(pPcdConfHeader);
  }

  for (Index = 0; Index < BrokenISCount; Index++) {
    if (pBrokenISs[Index].MissingDimmCount > 0) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_IS_BROKEN_DIMMS_MISSING), NULL);

      for (Index2 = 0; Index2 < pBrokenISs[Index].MissingDimmCount; Index2++) {
	if (pPcdOutputConf->Header.Revision == NVDIMM_CONFIGURATION_TABLES_REVISION_2) {
	  pTmpDimmIdStr = CatSPrint(NULL, L"%04x-%02x-%04x-%08x", EndianSwapUint16(pBrokenISs[Index].MissingDimmIdentifier[Index2].ManufacturerId),
	    pBrokenISs[Index].MissingDimmIdentifier[Index2].ManufacturingLocation,
	    EndianSwapUint16(pBrokenISs[Index].MissingDimmIdentifier[Index2].ManufacturingDate),
            EndianSwapUint32(pBrokenISs[Index].MissingDimmIdentifier[Index2].SerialNumber));
	} else {
	  pTmpDimmIdStr = CatSPrint(NULL, L"0x%08x", EndianSwapUint32(pBrokenISs[Index].MissingDimmIdentifier[Index2].SerialNumber));
        }

        if (Index2 == 0) {
          pDimmIdentifiersStr = CatSPrint(NULL, FORMAT_STR, pTmpDimmIdStr);
        } else {
          pDimmIdentifiersStr = CatSPrintClean(pDimmIdentifiersStr, L", " FORMAT_STR, pTmpDimmIdStr);
        }
        FREE_POOL_SAFE(pTmpDimmIdStr);
      }
#ifndef OS_BUILD
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pBrokenISs[Index].InterleaveSetIndex, pDimmIdentifiersStr);
#else
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pBrokenISs[Index].InterleaveSetIndex);
#endif
      FREE_POOL_SAFE(pTmpStr);
      FREE_POOL_SAFE(pDimmIdentifiersStr);
      APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
  }

Finish:
  FREE_POOL_SAFE(pBrokenISs);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check for uninitialized dimms in the system

  @param[in out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_NOT_FOUND Unable to locate relevant PCAT tables.
**/
STATIC
EFI_STATUS
CheckUninitializedDimms(
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pNode = NULL;
  DIMM *pCurDimm = NULL;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;

  NVDIMM_ENTRY();

  if (ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.UninitializedDimms) {
    pCurDimm = DIMM_FROM_NODE(pNode);

    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_DIMM_FAILED_TO_INITIALIZE), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pCurDimm->DeviceHandle.AsUint32);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pCurDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
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
  CHAR16 *pTmpStr = NULL;
  PLATFORM_CAPABILITY_INFO *pPlatformCapability = NULL;

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
  if (gNvmDimmData->PMEMDev.pPcatHead != NULL && gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum == 1) {
    pPlatformCapability = gNvmDimmData->PMEMDev.pPcatHead->ppPlatformCapabilityInfo[0];

    if (!IS_BIT_SET_VAR(pPlatformCapability->MgmtSwConfigInputSupport, BIT0)) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_NO_OS_PROVISIONING), NULL);
      APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
    }
  } else {
    ReturnCode = EFI_NOT_FOUND;
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
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
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;

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

      CopyMem(ppDuplicateDimmUids[TotalNumDuplicateUID], CandidateDimmUID, MAX_DIMM_UID_LENGTH * sizeof(CHAR16));
      TotalNumDuplicateUID++;

      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_DUPLICATE_DIMM_UID), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, NumDuplicateUID, CandidateDimmUID);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
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
  appropriately populate the result messages, and test-state.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the result string of platform config diagnostics message
  @param[out] pDiagState Pointer to the platform config diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
RunConfigDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
     OUT CHAR16 **ppResult,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pTmpStr = NULL;
  SYSTEM_CAPABILITIES_INFO SysCapInfo;

  NVDIMM_ENTRY();

  if (ppResult == NULL || pDiagState == NULL || DimmCount > MAX_DIMMS) {
    NVDIMM_DBG("The platform configuration diagnostics test aborted due to an internal error.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (*ppResult != NULL) {
    NVDIMM_DBG("The passed result string for platform configuration diagnostics tests is not empty");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  ReturnCode = CheckUninitializedDimms(ppResult, pDiagState);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for uninitialized dimms failed.");
    if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
      goto FinishError;
    }
  }

  if (DimmCount == 0 || ppDimms == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NO_MANAGEABLE_DIMMS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
    goto Finish;
  }

  ReturnCode = CheckDimmUIDDuplication(ppDimms, DimmCount, ppResult, pDiagState);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for duplicate UID numbers failed.");
    if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
      goto FinishError;
    }
  }

  ReturnCode = GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig, &SysCapInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on GetSystemCapabilitiesInfo");
    goto Finish;
  }

  if (!SysCapInfo.AdrSupported) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_NO_ADR_SUPPORT), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_FAILED, ppResult, pDiagState);
  }

  ReturnCode = CheckSystemSupportedCapabilities(ppDimms, DimmCount, DimmIdPreference, ppResult, pDiagState);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for System supported capabilities failed.");
    if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
      goto FinishError;
    }
  }

  ReturnCode = CheckNamespaceLabelAreaIndex(ppDimms, DimmCount, DimmIdPreference, ppResult, pDiagState);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for Namespace label retrieve failed.");
    if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
      goto FinishError;
    }
  }

  ReturnCode = CheckPlatformConfigurationData(ppDimms, DimmCount, DimmIdPreference, ppResult, pDiagState);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for platform configuration data failed.");
    if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
      goto FinishError;
    }
  }

  if ((*pDiagState & DIAG_STATE_MASK_ALL) <= DIAG_STATE_MASK_OK) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_SUCCESS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_OK, ppResult, pDiagState);
  }
  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishError:
  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_ABORTED_INTERNAL_ERROR), NULL);
  APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
