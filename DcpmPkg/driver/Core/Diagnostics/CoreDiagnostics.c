/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "CoreDiagnostics.h"
#include "QuickDiagnostic.h"
#include "ConfigDiagnostic.h"
#include "SecurityDiagnostic.h"
#include "FwDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

/**
  Append to the results string for a paricular diagnostic test, and modify
  the test state as per the message being appended.

  @param[in] pStrToAppend Pointer to the message string to be appended
  @param[in] DiagStateMask State corresonding to the string that is being appended
  @param[in out] ppResult Pointer to the result string of the particular test-suite's messages
  @param[in out] pDiagState Pointer to the particular test state

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
AppendToDiagnosticsResult (
  IN     DIMM *pDimm OPTIONAL,
  IN     UINT32 Code OPTIONAL,
  IN     CHAR16 *pStrToAppend,
  IN     UINT8 DiagStateMask,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (pStrToAppend == NULL || ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *ppResultStr = CatSPrintClean(*ppResultStr, FORMAT_STR_NL, pStrToAppend);

  *pDiagState |= DiagStateMask;

Finish:
  FREE_POOL_SAFE(pStrToAppend);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Helper function to convert the diagnostic test's result-state to its corresponding string form

  @param[in] DiagState The result-state of the test

  @retval NULL if the passed reuslt-state was invalid
  @return String form of the diagnostic test's result-state
**/
STATIC
CHAR16 *
GetDiagnosticState(
  IN     UINT8 DiagState
  )
{
  NVDIMM_ENTRY();
  CHAR16 *pDiagStateStr = NULL;

  if (DiagState & DIAG_STATE_MASK_ABORTED) {
    pDiagStateStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_STATE_ABORTED), NULL);
  } else if (DiagState & DIAG_STATE_MASK_FAILED) {
    pDiagStateStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_STATE_FAILED), NULL);
  } else if (DiagState & DIAG_STATE_MASK_WARNING) {
    pDiagStateStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_STATE_WARNING), NULL);
  } else if (DiagState & DIAG_STATE_MASK_OK) {
    pDiagStateStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_STATE_OK), NULL);
  } else if ((DiagState & DIAG_STATE_MASK_ALL) <= DIAG_STATE_MASK_OK) {
 pDiagStateStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_STATE_OK), NULL);
  }

  NVDIMM_EXIT();
  return pDiagStateStr;
}

/**
  Helper function to retrieve the diagnostics test-name from the test index

  @param[in] DiagnosticTestIndex Diagnostic test index

  @retval NULL if the passed test index was invalid
  @return String form of the diagnostic test name
**/
STATIC
CHAR16 *
GetDiagnosticTestName(
  IN    UINT8 DiagnosticTestIndex
  )
{
  CHAR16 *pDiagTestStr = NULL;

  NVDIMM_ENTRY();

  switch (DiagnosticTestIndex) {
    case  QuickDiagnosticIndex:
      pDiagTestStr =  HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_QUICK_NAME), NULL);
      break;
    case ConfigDiagnosticIndex:
      pDiagTestStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_NAME), NULL);
      break;
    case SecurityDiagnosticIndex:
      pDiagTestStr =  HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_SECURITY_NAME), NULL);
      break;
    case FwDiagnosticIndex:
      pDiagTestStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_NAME), NULL);
      break;
    default:
      NVDIMM_DBG("invalid diagnostic test");
      break;
  }

  NVDIMM_EXIT();
  return pDiagTestStr;
}

/**
  Add headers to the message results from all the tests that were run,
  and then append those messages into one single Diagnostics result string

  @param[in] pBuffer Array of the result messages for all tests
  @param[in] DiagState Array of the result state for all tests
  @param[out] ppResult Pointer to the final result string for all tests that were run

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
CombineDiagnosticsTestResults(
  IN     CHAR16 *pBuffer[],
  IN     UINT8 DiagState[],
     OUT CHAR16 **ppResult
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN Index = 0;
  CHAR16 *pColonMarkStr = NULL;
  CHAR16 *pTestNameValueStr = NULL;
  CHAR16 *pDiagStateValueStr = NULL;
  CHAR16 *pTempHeaderStr = NULL;

  NVDIMM_ENTRY();

  if (pBuffer == NULL || ppResult == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pColonMarkStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DCPMM_COLON_MARK), NULL);

  for (Index = 0; Index < DIAGNOSTIC_TEST_COUNT; Index++) {
    if (pBuffer[Index] != NULL) {

      //creating test name string
      pTestNameValueStr = GetDiagnosticTestName((UINT8)Index);
      if (pTestNameValueStr == NULL) {
        NVDIMM_DBG("Retrieval of the test name failed");
        //log as warning state and a message
        continue;
      }

      //creating state string
      pDiagStateValueStr = GetDiagnosticState(DiagState[Index]);
      if (pDiagStateValueStr == NULL) {
        NVDIMM_DBG("Retrieval of the test state failed");
        FREE_POOL_SAFE(pTestNameValueStr);
        //log as warning state and a message
        continue;
      }

      pTempHeaderStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_TEST_NAME_HEADER), NULL);
      *ppResult = CatSPrintClean(*ppResult, FORMAT_STR FORMAT_STR L" " FORMAT_STR_NL, pTempHeaderStr, pColonMarkStr, pTestNameValueStr);
      FREE_POOL_SAFE(pTempHeaderStr);
      FREE_POOL_SAFE(pTestNameValueStr);

      pTempHeaderStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_STATE_HEADER), NULL);
      *ppResult = CatSPrintClean(*ppResult, FORMAT_STR FORMAT_STR L" " FORMAT_STR_NL, pTempHeaderStr, pColonMarkStr, pDiagStateValueStr);
      FREE_POOL_SAFE(pTempHeaderStr);
      FREE_POOL_SAFE(pDiagStateValueStr);

      pTempHeaderStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_MESSAGE_HEADER), NULL);
      *ppResult = CatSPrintClean(*ppResult, FORMAT_STR  FORMAT_STR L"\n" FORMAT_STR_NL, pTempHeaderStr, pColonMarkStr, pBuffer[Index]);
      FREE_POOL_SAFE(pTempHeaderStr);

      FREE_POOL_SAFE(pTestNameValueStr);
      FREE_POOL_SAFE(pDiagStateValueStr);
    }
  }

Finish:
  FREE_POOL_SAFE(pColonMarkStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  The fundamental core diagnostics function that is used by both
  the NvmDimmConfig protocol and the DriverDiagnostic protoocls.

  It runs the specified diagnotsics tests on the list of specified dimms,
  and returns a single combined test result message

  @param[in] ppDimms The platform DIMM pointers list
  @param[in] DimmsNum Platform DIMMs count
  @param[in] pDimmIds Pointer to an array of user-specified DIMM IDs
  @param[in] DimmIdsCount Number of items in the array of user-specified DIMM IDs
  @param[in] DiagnosticsTest The selected tests bitmask
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the structure with information about test

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
CoreStartDiagnostics(
  IN     DIMM **ppDimms,
  IN     UINT32 DimmsNum,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 DiagnosticsTest,
  IN     UINT8 DimmIdPreference,
  OUT    DIAG_INFO **ppResult
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  DIMM **ppSpecifiedDimms = NULL;
  UINT16 SpecifiedDimmsNum = 0;
  LIST_ENTRY *pDimmList = NULL;
  UINT32 PlatformDimmsCount = 0;
  DIMM *pCurrentDimm = NULL;
  UINTN Index = 0;
  DIAG_INFO *pBuffer = NULL;
  pBuffer = AllocateZeroPool(sizeof(*pBuffer));

  NVDIMM_ENTRY();

  if (ppDimms == NULL || ppResult == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pBuffer == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  *ppResult = pBuffer;

  if ((DimmIdPreference != DISPLAY_DIMM_ID_HANDLE) &&
    (DimmIdPreference != DISPLAY_DIMM_ID_UID)) {
    NVDIMM_DBG("Invalid value for Dimm Id preference");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if ((DiagnosticsTest & DIAGNOSTIC_TEST_ALL) == 0) {
    NVDIMM_DBG("Invalid diagnostics test");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Populate the specified dimms for quick diagnostics
  if ((DiagnosticsTest & DIAGNOSTIC_TEST_QUICK) && (DimmIdsCount > 0)) {
    if (pDimmIds == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    pDimmList = &gNvmDimmData->PMEMDev.Dimms;
    ReturnCode = GetListSize(pDimmList, &PlatformDimmsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on DimmListSize");
      goto Finish;
    }

    if (DimmIdsCount > PlatformDimmsCount) {
      NVDIMM_DBG("User specified Dimm count exceeds the platform Dimm count.");
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    ppSpecifiedDimms = AllocateZeroPool(DimmIdsCount * sizeof(DIMM *));
    if (ppSpecifiedDimms == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    for (Index = 0; Index < DimmIdsCount; Index++) {
      pCurrentDimm = GetDimmByPid(pDimmIds[Index], pDimmList);
      if (pCurrentDimm == NULL) {
        NVDIMM_DBG("Failed on GetDimmByPid. Does DIMM 0x%04x exist?", pDimmIds[Index]);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }

      ppSpecifiedDimms[SpecifiedDimmsNum] = pCurrentDimm;
      SpecifiedDimmsNum++;
    }
  }

  if (DiagnosticsTest & DIAGNOSTIC_TEST_QUICK) {
    pBuffer->TestName = GetDiagnosticTestName(QuickDiagnosticIndex);
    if (SpecifiedDimmsNum > 0) {
      TempReturnCode = RunQuickDiagnostics(ppSpecifiedDimms, (UINT16)SpecifiedDimmsNum, DimmIdPreference, pBuffer);
    }
    else {
      TempReturnCode = RunQuickDiagnostics(ppDimms, (UINT16)DimmsNum, DimmIdPreference, pBuffer);
    }
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Quick diagnostics failed. (" FORMAT_EFI_STATUS ")", TempReturnCode);
    }
    TempReturnCode = UpdateTestState(pBuffer, QuickDiagnosticIndex);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Quick diagnostics failed while updating state.");
    }
  }
  else if (DiagnosticsTest & DIAGNOSTIC_TEST_CONFIG) {
    pBuffer->TestName = GetDiagnosticTestName(ConfigDiagnosticIndex);
    TempReturnCode = RunConfigDiagnostics(ppDimms, (UINT16)DimmsNum, DimmIdPreference, pBuffer);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Platform configuration diagnostics failed. (" FORMAT_EFI_STATUS ")", TempReturnCode);
    }
    TempReturnCode = UpdateTestState(pBuffer, ConfigDiagnosticIndex);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Platform configuration diagnostics failed while updating state.");
    }
  }
  else if (DiagnosticsTest & DIAGNOSTIC_TEST_SECURITY) {
    pBuffer->TestName = GetDiagnosticTestName(SecurityDiagnosticIndex);
    TempReturnCode = RunSecurityDiagnostics(ppDimms, (UINT16)DimmsNum, DimmIdPreference, pBuffer);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Security diagnostics failed. (" FORMAT_EFI_STATUS ")", TempReturnCode);
    }
    TempReturnCode = UpdateTestState(pBuffer, SecurityDiagnosticIndex);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Securit diagnostics failed while updating state.");
    }
  }
  else if (DiagnosticsTest & DIAGNOSTIC_TEST_FW) {
    pBuffer->TestName = GetDiagnosticTestName(FwDiagnosticIndex);
    TempReturnCode = RunFwDiagnostics(ppDimms, (UINT16)DimmsNum, DimmIdPreference, pBuffer);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Firmware and consistency settings diagnostics failed. (" FORMAT_EFI_STATUS ")", TempReturnCode);
    }
    TempReturnCode = UpdateTestState(pBuffer, FwDiagnosticIndex);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Firmware and consistency settings diagnostics failed while updating state.");
    }
  }
  else {
    NVDIMM_DBG("Invalid Diagnostic Test Id");
    ReturnCode = EFI_INVALID_PARAMETER;
  }


Finish:
  FREE_POOL_SAFE(ppSpecifiedDimms);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function should be used to update status of the test based on information stored
  inside diagnostic information structure.

  @param[in] pBuffer Pointer to Diagnostic information structure
  @param[in] DiagnosticTestIndex Test Index

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
UpdateTestState(
  IN   DIAG_INFO *pBuffer,
  IN   UINT8 DiagnosticTestIndex
)
{

  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN Id = 0;
  BOOLEAN IsTestPassed = TRUE;

  if (pBuffer != NULL) {
    for (Id = 0; Id < MAX_NO_OF_DIAGNOSTIC_SUBTESTS; Id++) {
      if (pBuffer->SubTestName[Id] != NULL)
      {
        pBuffer->SubTestState[Id] = GetDiagnosticState(pBuffer->SubTestStateVal[Id]);
        pBuffer->StateVal |= (pBuffer->SubTestStateVal[Id] & DIAG_STATE_MASK_ALL);
      }
    }
    if ((pBuffer->StateVal & DIAG_STATE_MASK_ALL) > DIAG_STATE_MASK_OK) {
      IsTestPassed = FALSE;
    }
    pBuffer->State = GetDiagnosticState(pBuffer->StateVal);

    if (pBuffer->Message == NULL) {
      if (IsTestPassed == TRUE) {
        switch (DiagnosticTestIndex) {
        case  QuickDiagnosticIndex:
          APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_QUICK_SUCCESS), EVENT_CODE_500, DIAG_STATE_MASK_OK, &pBuffer->Message, &pBuffer->StateVal);
          break;
        case ConfigDiagnosticIndex:
          APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_CONFIG_SUCCESS), EVENT_CODE_600, DIAG_STATE_MASK_OK, &pBuffer->Message, &pBuffer->StateVal);
          break;
        case SecurityDiagnosticIndex:
          APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_SECURITY_SUCCESS), EVENT_CODE_800, DIAG_STATE_MASK_OK, &pBuffer->Message, &pBuffer->StateVal);
          break;
        case FwDiagnosticIndex:
          APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_SUCCESS), EVENT_CODE_900, DIAG_STATE_MASK_OK, &pBuffer->Message, &pBuffer->StateVal);
          break;
        default:
          NVDIMM_DBG("invalid diagnostic test");
          break;
        }
      }
    }
  }
  else {
    NVDIMM_DBG("invalid parameter to set state");
    ReturnCode = EFI_INVALID_PARAMETER;
  }

  return ReturnCode;
}
