/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SecurityDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#define ENCRYPTION_TEST_INDEX 0
#define INCONSISTANCY_TEST_INDEX 1

/**
  Run security diagnostics for the list of DIMMs, and appropriately
  populate the result structure.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] pResult Pointer of structure with diagnostics test result

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
RunSecurityDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  OUT DIAG_INFO *pResult
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pInconsistentSecurityStatesStr = NULL;
  UINT8 DimmSecurityState = 0;
  UINT32 SecurityFlag = 0;
  UINT8 SecurityStateCount[SECURITY_STATES_COUNT];
  BOOLEAN InconsistencyFlag = FALSE;
  UINT8 Index = 0;

  NVDIMM_ENTRY();

  ZeroMem(SecurityStateCount, sizeof(SecurityStateCount));

  if (pResult == NULL || DimmCount > MAX_DIMMS) {
    NVDIMM_DBG("The security diagnostics test aborted due to an internal error.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmCount == 0 || ppDimms == NULL) {
    NVDIMM_DBG("The dimm count and dimm information is missing");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pResult->SubTestName[ENCRYPTION_TEST_INDEX] = CatSPrint(NULL, L"Encryption status");
  pResult->SubTestName[INCONSISTANCY_TEST_INDEX] = CatSPrint(NULL, L"Inconsistency");
  for (Index = 0; Index < DimmCount; ++Index) {
    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_SECURITY_ABORTED_INTERNAL_ERROR), EVENT_CODE_805, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[ENCRYPTION_TEST_INDEX], &pResult->SubTestStateVal[ENCRYPTION_TEST_INDEX]);
      goto Finish;
    }

    if (ppDimms[Index]->SkuInformation.EncryptionEnabled == MODE_DISABLED) {
      APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_SECURITY_NOT_SUPPORTED), EVENT_CODE_804, DIAG_STATE_MASK_OK,
        &pResult->SubTestMessage[ENCRYPTION_TEST_INDEX], &pResult->SubTestStateVal[ENCRYPTION_TEST_INDEX]);
    }

    ReturnCode = GetDimmSecurityState(
      ppDimms[Index],
      PT_TIMEOUT_INTERVAL,
      &SecurityFlag);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmSecurityState of DIMM ID 0x%x", ppDimms[Index]->DeviceHandle.AsUint32);
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_SECURITY_ABORTED_INTERNAL_ERROR), EVENT_CODE_805, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[INCONSISTANCY_TEST_INDEX], &pResult->SubTestStateVal[INCONSISTANCY_TEST_INDEX]);
      goto Finish;
    }
    ConvertSecurityBitmask(SecurityFlag, &DimmSecurityState);

    // increase the count of the security state that the dimm is currently in
    SecurityStateCount[DimmSecurityState]++;
    DimmSecurityState = 0;
  }

  for (Index = 0; Index < SECURITY_STATES_COUNT; Index++) {
    if ((SecurityStateCount[Index] > 0) && (SecurityStateCount[Index] != DimmCount)) {
      pInconsistentSecurityStatesStr = CatSPrintClean(pInconsistentSecurityStatesStr, FORMAT_STR L"%d " FORMAT_STR,
        InconsistencyFlag ? L", " : L"", SecurityStateCount[Index], SecurityToString(gNvmDimmData->HiiHandle, Index));

      InconsistencyFlag = TRUE;
    }
  }

  if (InconsistencyFlag) {
    APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_SECURITY_INCONSISTENT), EVENT_CODE_802, DIAG_STATE_MASK_WARNING,
      &pResult->SubTestMessage[INCONSISTANCY_TEST_INDEX], &pResult->SubTestStateVal[INCONSISTANCY_TEST_INDEX], pInconsistentSecurityStatesStr);
  }

  ReturnCode = EFI_SUCCESS;
  goto Finish;

Finish:
  FREE_POOL_SAFE(pInconsistentSecurityStatesStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
