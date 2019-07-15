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
#include <ReadRunTimePreferences.h>

#define DS_ROOT_PATH                       L"/DiagnosticList"
#define DS_DIAGNOSTIC_PATH                 L"/DiagnosticList/Diagnostic"
#define DS_DIAGNOSTIC_INDEX_PATH           L"/DiagnosticList/Diagnostic[%d]"
#define DS_SUBTEST_PATH                    L"/DiagnosticList/Diagnostic/SubTest"
#define DS_SUBTEST_INDEX_PATH              L"/DiagnosticList/Diagnostic[%d]/SubTest[%d]"

#define TEST_NAME_STR                      L"Test"
#define SUBTEST_NAME_STR                   L"SubTest"
#define RESULT_STR L"RESULT"

#define DIAG_ENTRY_EOL                     L'\n'

   /*
    *  PRINT LIST ATTRIBUTES
    *  --Test = Quick
    *     Message = The quick health check succeeded
    *     State = Ok
    *     Result Code = 0
    *     --SubTest = PCD
    *       State = ok
    *       Message.1 = X
    *       Event Code.1 = X
    */
PRINTER_LIST_ATTRIB StartDiagListAttributes =
{
 {
    {
      DIAGNOSTIC_NODE_STR,                                         //GROUP LEVEL TYPE
      L"\n--" TEST_NAME_STR L" = $(" TEST_NAME_STR L")",           //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls = %ls",                                //NULL or KEY VAL FORMAT STR
      TEST_NAME_STR                                                //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      SUBTEST_NAME_STR,                                                                 //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"--" SUBTEST_NAME_STR L" = $(" SUBTEST_NAME_STR L")",            //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT L"%ls = %ls",                                     //NULL or KEY VAL FORMAT STR
      SUBTEST_NAME_STR                                                                  //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

PRINTER_DATA_SET_ATTRIBS StartDiagDataSetAttribs =
{
  &StartDiagListAttributes,
  NULL
};

/**
  Command syntax definition
**/
COMMAND StartDiagnosticCommand =
{
  START_VERB,                                                     //!< verb
  {                                                               //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", LARGE_PAYLOAD_OPTION, L"", L"", HELP_LPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SMALL_PAYLOAD_OPTION, L"", L"", HELP_SPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    ,{L"", L"", L"", L"", L"",FALSE, ValueOptional}
#endif
  },
  {                                                               //!< targets
    {DIAGNOSTIC_TARGET, L"", ALL_DIAGNOSTICS_TARGETS, TRUE, ValueOptional},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                        //!< properties
  L"Run a diagnostic test on one or more DIMMs",                  //!< help
  StartDiagnosticCmd,
  TRUE
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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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
  DIAG_INFO *pFinalDiagnosticsResult = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  UINT8 Id = 0;
  CHAR16 *MsgStr = NULL;
  CHAR16 *EventCodeStr = NULL;
  CHAR16 **ppSplitDiagResultLines = NULL;
  CHAR16 **ppSplitDiagEventCode = NULL;
  UINT32 NumTokens = 0;
  UINT32 CodeTokens = 0;
  UINT32 i = 0;
  CHAR16** EventMesg = NULL;

  NVDIMM_ENTRY();

  SetDisplayInfo(L"Diagnostic", DiagView, NULL);

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = ReadRunTimePreferences(&DisplayPreferences, DISPLAY_CLI_INFO);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }
  DimmIdPreference = DisplayPreferences.DimmIdentifier;

  ChosenDiagTests = GetDiagnosticTestType(pCmd);
  if (ChosenDiagTests == DIAGNOSTIC_TEST_UNKNOWN || ((ChosenDiagTests & DIAGNOSTIC_TEST_ALL) != ChosenDiagTests)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_WRONG_DIAGNOSTIC_TARGETS, ALL_DIAGNOSTICS_TARGETS);
    goto Finish;
  }

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);

  if (pDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount, DIMM_INFO_CATEGORY_NONE, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pDimmTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
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

    DIAG_INFO *pLoc = pFinalDiagnosticsResult;

    PRINTER_BUILD_KEY_PATH(pPath, DS_DIAGNOSTIC_INDEX_PATH, Index);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TEST_NAME_STR, pLoc->TestName);

    EventMesg = StrSplit(pLoc->Message, DIAG_ENTRY_EOL, &i);
    if(EventMesg != NULL){
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath,L"Message" ,EventMesg[0]);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"State", pLoc->State);
      PRINTER_SET_KEY_VAL_UINT64(pPrinterCtx, pPath, L"Result Code", pLoc->ResultCode, DECIMAL);
      FREE_POOL_SAFE(EventMesg);
    }
    for (Id = 0; Id < MAX_NO_OF_DIAGNOSTIC_SUBTESTS; Id++) {
      if (pLoc->SubTestName[Id] != NULL) {
        PRINTER_BUILD_KEY_PATH(pPath, DS_SUBTEST_INDEX_PATH, Index, Id);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SUBTEST_NAME_STR, pLoc->SubTestName[Id]);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"State", pLoc->SubTestState[Id]);
        // Split message string and set printer in unique key-> value form.
        if (pLoc->SubTestMessage[Id] != NULL) {
          ppSplitDiagResultLines = StrSplit(pLoc->SubTestMessage[Id], DIAG_ENTRY_EOL, &NumTokens);
          if (ppSplitDiagResultLines == NULL) {
            NVDIMM_WARN("Message string split failed");
            FREE_POOL_SAFE(pPath);
            return EFI_OUT_OF_RESOURCES;
          }
          if (pLoc->SubTestEventCode[Id] != NULL) {
            ppSplitDiagEventCode = StrSplit(pLoc->SubTestEventCode[Id], DIAG_ENTRY_EOL, &CodeTokens);
          }
          for (i = 0; i < NumTokens; i++) {
            MsgStr = CatSPrint(NULL, L"Message.%d", i + 1);
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MsgStr, ppSplitDiagResultLines[i]);
            if (ppSplitDiagEventCode != NULL) {
              EventCodeStr = CatSPrint(NULL, L"EventCode.%d", i + 1);
              PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EventCodeStr, ppSplitDiagEventCode[i]);
            }
            FREE_POOL_SAFE(MsgStr);
            FREE_POOL_SAFE(EventCodeStr);
          }
          FreeStringArray(ppSplitDiagResultLines, NumTokens);
          if (ppSplitDiagEventCode != NULL) {
            FreeStringArray(ppSplitDiagEventCode, CodeTokens);
          }
        }
        FREE_POOL_SAFE(pLoc->SubTestName[Id]);
        FREE_POOL_SAFE(pLoc->SubTestMessage[Id]);
        FREE_POOL_SAFE(pLoc->SubTestState[Id]);
        FREE_POOL_SAFE(pLoc->SubTestEventCode[Id]);
      }
    }
    FREE_POOL_SAFE(pLoc->TestName);
    FREE_POOL_SAFE(pLoc->Message);
    FREE_POOL_SAFE(pLoc->State);
    FREE_POOL_SAFE(pFinalDiagnosticsResult);
  }

  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &StartDiagDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  // free all memory structures
  FREE_POOL_SAFE(pPath);
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
