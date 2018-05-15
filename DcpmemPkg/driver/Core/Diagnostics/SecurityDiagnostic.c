/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SecurityDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#ifdef OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) SendTheEventAndAppendToDiagnosticsResult(pDimm,pStr,StateMask,__COUNTER__,SYSTEM_EVENT_CAT_SECURITY,ppResult,pState)
#else // OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) AppendToDiagnosticsResult(pStr,StateMask,ppResult,pState)
#endif // OS_BUILD

/**
  Run security diagnostics for the list of DIMMs, and appropriately
  populate the result messages, and test-state.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the result string of security diagnostics message
  @param[out] pDiagState Pointer to the security diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
RunSecurityDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
     OUT CHAR16 **ppResult,
     OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pInconsistentSecurityStatesStr = NULL;
  UINT8 DimmSecurityState = 0;
  UINT8 SecurityFlag = 0;
  UINT8 SecurityStateCount[SECURITY_STATES_COUNT];
  BOOLEAN InconsistencyFlag = FALSE;
  UINT8 Index = 0;

  NVDIMM_ENTRY();

  ZeroMem(SecurityStateCount, sizeof(SecurityStateCount));

  if (ppResult == NULL || pDiagState == NULL || DimmCount > MAX_DIMMS) {
    NVDIMM_DBG("The security diagnostics test aborted due to an internal error.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmCount == 0 || ppDimms == NULL) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NO_MANAGEABLE_DIMMS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
    goto Finish;
  }

  if (*ppResult != NULL) {
    NVDIMM_DBG("The passed result string for security diagnostics tests is not empty");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  for (Index = 0; Index < DimmCount; ++Index) {
    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
    }

    ReturnCode = GetDimmSecurityState(
      ppDimms[Index],
      PT_TIMEOUT_INTERVAL,
      &SecurityFlag);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmSecurityState of DIMM ID 0x%x", ppDimms[Index]->DeviceHandle.AsUint32);
      goto FinishError;
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
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_SECURITY_INCONSISTENT), NULL);
    pTmpStr = CatSPrintClean(pTmpStr, L" " FORMAT_STR FORMAT_STR, pInconsistentSecurityStatesStr, L".");
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_WARNING, ppResult, pDiagState);
  }

  if ((*pDiagState & DIAG_STATE_MASK_ALL) <= DIAG_STATE_MASK_OK) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_SECURITY_SUCCESS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_OK, ppResult, pDiagState);
  }
  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishError:
  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_SECURITY_ABORTED_INTERNAL_ERROR), NULL);
  APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
Finish:
  FREE_POOL_SAFE(pInconsistentSecurityStatesStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
