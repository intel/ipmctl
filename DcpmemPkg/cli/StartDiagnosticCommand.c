/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "Debug.h"
#include "Utility.h"
#include "Common.h"
#include "NvmInterface.h"
#include "StartDiagnosticCommand.h"
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

/**
  Command syntax definition
**/
COMMAND StartDiagnosticCommand =
{
  START_VERB,                                                     //!< verb
  {{L"", L"", L"", L"", FALSE, ValueOptional}                     //!< options
#ifdef OS_BUILD
  ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {                                                               //!< targets
    {DIAGNOSTIC_TARGET, L"", ALL_DIAGNOSTICS_TARGETS, TRUE, ValueOptional},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired},
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                        //!< properties
  L"Run a diagnostic test on one or more DIMMs",                  //!< help
  StartDiagnosticCmd
};

/**
  Get Diagnostic types to be executed from COMMAND

  @param[in] pCmd command from CLI

  @retval One of DIAGNOSTIC_TEST_X types
  @retval DIAGNOSTIC_TEST_UNKNOWN if any error occurs
**/
UINT8
GetDiagnosticTestType(
  IN     COMMAND *pCmd
)
{
  CHAR16 *pDiagnosticTargetValue = NULL;
  CHAR16 **ppStringElements = NULL;
  UINT16 Index = 0;
  UINT8 ChosenDiagnosticTests = DIAGNOSTIC_TEST_UNKNOWN;
  UINT32 ElementsCount = 0;

  if (pCmd == NULL) {
    goto Finish;
  }

  if (!ContainTarget(pCmd, DIAGNOSTIC_TARGET)) {
    goto Finish;
  }
  pDiagnosticTargetValue = GetTargetValue(pCmd, DIAGNOSTIC_TARGET);

  ppStringElements = StrSplit(pDiagnosticTargetValue, L',', &ElementsCount);
  if (ppStringElements == NULL) {
    /** If no diagnostic test was given explicitly, start all test**/
    ChosenDiagnosticTests |= DIAGNOSTIC_TEST_ALL;
    goto Finish;
  }

  for (Index = 0; Index < ElementsCount; ++Index) {
    if (StrICmp(ppStringElements[Index], QUICK_TEST_TARGET_VALUE) == 0) {
      ChosenDiagnosticTests |= DIAGNOSTIC_TEST_QUICK;
    } else if (StrICmp(ppStringElements[Index], CONFIG_TEST_TARGET_VALUE) == 0) {
      ChosenDiagnosticTests |= DIAGNOSTIC_TEST_CONFIG;
    } else if (StrICmp(ppStringElements[Index], SECURITY_TEST_TARGET_VALUE) == 0) {
      ChosenDiagnosticTests |= DIAGNOSTIC_TEST_SECURITY;
    } else if (StrICmp(ppStringElements[Index], FW_TEST_TARGET_VALUE) == 0) {
      ChosenDiagnosticTests |= DIAGNOSTIC_TEST_FW;
    } else {
      ChosenDiagnosticTests = DIAGNOSTIC_TEST_UNKNOWN;
      goto Finish;
    }
  }

Finish:
  FreeStringArray(ppStringElements, ElementsCount);
  return ChosenDiagnosticTests;
}


/**
  Execute the Start Diagnostic command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
StartDiagnosticCmd(
  IN     COMMAND *pCmd
  )
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  CHAR16 *pDimmTargetValue = NULL;
  UINT8 ChosenDiagTests = DIAGNOSTIC_TEST_UNKNOWN;
  UINT8 CurrentDiagTest = DIAGNOSTIC_TEST_UNKNOWN;
  UINT32 Index = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT8 DimmIdPreference = DISPLAY_DIMM_ID_HANDLE;
  CHAR16 *pFinalDiagnosticsResult = NULL;

  NVDIMM_ENTRY();

  SetDisplayInfo(L"Diagnostic", DiagView);

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
  DimmIdPreference = DisplayPreferences.DimmIdentifier;

  ChosenDiagTests = GetDiagnosticTestType(pCmd);
  if (ChosenDiagTests == DIAGNOSTIC_TEST_UNKNOWN || ((ChosenDiagTests & DIAGNOSTIC_TEST_ALL) != ChosenDiagTests)) {
      Print(FORMAT_STR_SPACE FORMAT_STR_NL, CLI_ERR_WRONG_DIAGNOSTIC_TARGETS, ALL_DIAGNOSTICS_TARGETS);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
  }

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);

  if (pDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount, DIMM_INFO_CATEGORY_NONE, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pDimmTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  for (Index = 0; Index < DIAGNOSTIC_TEST_COUNT; ++Index) {
    CurrentDiagTest = (ChosenDiagTests & (1 << Index));
    if (CurrentDiagTest == 0) {
      /** Test is not selected, skip **/
      continue;
    }

    ReturnCode = pNvmDimmConfigProtocol->StartDiagnostic(
        pNvmDimmConfigProtocol,
        pDimmIds,
        DimmIdsCount,
        CurrentDiagTest,
        DimmIdPreference,
        &pFinalDiagnosticsResult);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Diagnostics failed");
    }
  }

  //Print Cli diagnostics result
#ifdef OS_BUILD //todo, implement ST->ConOut in OS, for now just Print it
  if (NULL != pFinalDiagnosticsResult) {
    Print(pFinalDiagnosticsResult);
  }
  else {
    NVDIMM_ERR("The final diagnostic result string not allocated; NULL pointer");
  }
#else
  if ((pFinalDiagnosticsResult != NULL) && ((gST != NULL) && (gST->ConOut != NULL))) {
    ReturnCode = gST->ConOut->OutputString(gST->ConOut, pFinalDiagnosticsResult);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_PRINTING_DIAGNOSTICS_RESULTS);
    }
  }
#endif
Finish:
  // free all memory structures
  FREE_POOL_SAFE(pFinalDiagnosticsResult);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the start diagnostic command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterStartDiagnosticCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&StartDiagnosticCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

