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
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty},
    {L"", RECOVER_OPTION, L"", L"", FALSE, ValueEmpty},
  },
  {                                                                      //!< targets
    {FORMAT_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_ID, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                               //!< properties
  L"",                                                                   //!< help
  StartFormat
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

  NVDIMM_ENTRY();
  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
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
      Print(FORMAT_STR_NL, CLI_INFO_NO_NON_FUNCTIONAL_DIMMS);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }

    pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
    if (pDimms == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, DimmCount, pDimms);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  } else {
    ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }

    if (!Recovery) {
      if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
        Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
        ReturnCode = EFI_INVALID_PARAMETER;
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
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
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

      Print(FORMAT_STR L"(" FORMAT_STR L")\n", CLI_FORMAT_DIMM_PROMPT_STR, DimmStr);
      ReturnCode = PromptYesNo(&Confirmation);
      if (EFI_ERROR(ReturnCode) || !Confirmation) {
        ReturnCode = EFI_NOT_STARTED;
        goto Finish;
      }
    }
  }

  Print(FORMAT_STR_NL, CLI_FORMAT_DIMM_STARTING_FORMAT, DimmStr);

  ReturnCode = pNvmDimmConfigProtocol->DimmFormat(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, Recovery, pCommandStatus);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL,CLI_FORMAT_DIMM_REBOOT_REQUIRED_STR);
  } else {
    DisplayCommandStatus(CLI_INFO_START_FORMAT, L"", pCommandStatus);
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  }

Finish:
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


