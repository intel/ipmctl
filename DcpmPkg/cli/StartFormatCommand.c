/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/ShellLib.h>
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include "Common.h"
#include "StartFormatCommand.h"

/**
  Command syntax definition
**/
struct Command StartFormatCommand =
{
  START_VERB,                                                            //!< verb
  {                                                                      //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"",HELP_FORCE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", RECOVER_OPTION, L"", L"",L"Recovery Option", FALSE, ValueEmpty}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {                                                                      //!< targets
    {FORMAT_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_ID, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                               //!< properties
  L"Start Format Dimms",                                                                   //!< help
  StartFormat,								 //!< run function
  TRUE
};

EFI_STATUS
StartFormat(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pTargetValue = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN Force = FALSE;
  BOOLEAN Confirmation = FALSE;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  UINT32 Index = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN Recovery = FALSE;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  NVDIMM_ENTRY();
  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /**
    Printing will still work via compability mode if NULL so no need to check for NULL.
  **/
  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  if (containsOption(pCmd, RECOVER_OPTION)) {
    Recovery = TRUE;
    // Populate the list of DIMM_INFO structures with the DIMMs NOT found in NFIT
    ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimmCount(pNvmDimmConfigProtocol, &DimmCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (DimmCount == 0) {
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_NON_FUNCTIONAL_DIMMS);
      goto Finish;
    }

    pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
    if (pDimms == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, DimmCount, pDimms);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  } else {
    ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
    if (EFI_ERROR(ReturnCode)) {
      if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
      goto Finish;
    }
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }

    if (!Recovery) {
      if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
        goto Finish;
      }
    }
  }

  /* If no dimms specified then use all dimms */
  if (DimmIdsCount == 0) {

    FREE_POOL_SAFE(pDimmIds);
    pDimmIds = AllocateZeroPool(sizeof(*pDimmIds) * DimmCount);

    if (pDimmIds == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    for (Index =0; Index < DimmCount; Index++) {
      pDimmIds[Index] = pDimms[Index].DimmID;
    }
    DimmIdsCount = DimmCount;
  }

  /** Check force option **/
  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  if (!Force) {
    for (Index = 0; Index < DimmIdsCount; Index++) {
      ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
       }

      ReturnCode = GetPreferredDimmIdAsString(DimmHandle, Recovery ? NULL : pDimms[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, FORMAT_STR L" " FORMAT_STR L"\n", CLI_FORMAT_DIMM_PROMPT_STR, DimmStr);
      ReturnCode = PromptYesNo(&Confirmation);
      if (EFI_ERROR(ReturnCode) || !Confirmation) {
        ReturnCode = EFI_NOT_STARTED;
        goto Finish;
      }
    }
  }

  PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, CLI_FORMAT_DIMM_STARTING_FORMAT, DimmStr);

  ReturnCode = pNvmDimmConfigProtocol->DimmFormat(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, Recovery, pCommandStatus);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_FORMAT_DIMM_REBOOT_REQUIRED_STR);
  } else {
    DisplayCommandStatus(CLI_INFO_START_FORMAT, L"", pCommandStatus);
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Recover Format command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterStartFormatCommand()
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&StartFormatCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


