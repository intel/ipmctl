/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Debug.h>
#include <Library/BaseMemoryLib.h>
#include "CommandParser.h"
#include "Common.h"
#include "DeletePcdCommand.h"
#include <PcdCommon.h>

/**
  Command syntax definition
**/
struct Command DeletePcdCommand =
{
  DELETE_VERB,                                                                     //!< verb
  {
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty}  //!< options
  },
  {                                                                                //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional},
    {PCD_TARGET, L"", PCD_LSA_TARGET_VALUE, TRUE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                         //!< properties
  L"Clear the namespace LSA partition on one or more DIMMs",                       //!< help
	DeletePcdCmd
};


STATIC
EFI_STATUS
ValidatePcdTarget(
  IN     CHAR16 *pTargetValue
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pTargetValue == NULL) {
    goto Finish;
  }

  if (StrICmp(pTargetValue, PCD_LSA_TARGET_VALUE) != 0) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the Delete PCD command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_FOUND couldn't open Config Protocol
  @retval EFI_ABORTED internal
**/
EFI_STATUS
DeletePcdCmd(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *pTargetValue = NULL;
  UINT32 DimmIndex = 0;
  UINT32 DimmHandle = 0;
  UINT32 Index = 0;
  BOOLEAN Force = FALSE;
  BOOLEAN Confirmation = FALSE;
  CHAR16 *pCommandStatusMessage = NULL;

  NVDIMM_ENTRY();

  SetDisplayInfo(L"DeletePcd", ResultsView);

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** Get config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Check force option **/
  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
  ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
    Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pTargetValue = GetTargetValue(pCmd, PCD_TARGET);
  ReturnCode = ValidatePcdTarget(pTargetValue);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_PCD);
    goto Finish;
  }

  /* If no dimms specified then use all dimms */
  if (DimmIdsCount == 0) {

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

  pCommandStatusMessage = CatSPrint(NULL, L"Clear LSA namespace partition");

  if (!Force) {
    for (Index = 0; Index < DimmIdsCount; Index++) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);

      ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      Print(L"Clear LSA namespace partition on DIMM (" FORMAT_STR L"). ", DimmStr);
      ReturnCode = PromptYesNo(&Confirmation);
      if (EFI_ERROR(ReturnCode) || !Confirmation) {
        ReturnCode = EFI_NOT_STARTED;
        goto Finish;
      }
    }
  }

  Print(L"\n");

  ReturnCode = pNvmDimmConfigProtocol->DeletePcd(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount,
                                                  pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(L"Clear LSA namespace partition", L" on DIMM ", pCommandStatus);
    goto Finish;
  }

  DisplayCommandStatus(pCommandStatusMessage, L" on DIMM ", pCommandStatus);

Finish:
  FREE_POOL_SAFE(pCommandStatusMessage);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Delete PCD command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDeletePcdCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DeletePcdCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
