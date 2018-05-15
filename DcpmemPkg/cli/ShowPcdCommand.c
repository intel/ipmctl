/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Debug.h>
#include <Library/BaseMemoryLib.h>
#include "CommandParser.h"
#include "Common.h"
#include "ShowPcdCommand.h"
#include <PcdCommon.h>

/**
  Command syntax definition
**/
struct Command ShowPcdCommand =
{
  SHOW_VERB,                                                          //!< verb
  {{L"", L"", L"", L"", FALSE, ValueOptional}},                       //!< options
  {                                                                   //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional},
    {PCD_TARGET, L"", PCD_CONFIG_TARGET_VALUE L"|" PCD_LSA_TARGET_VALUE, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Show pool configuration goal stored on one or more AEPs",         //!< help
  ShowPcd
};

STATIC
EFI_STATUS
GetPcdTarget(
  IN     CHAR16 *pTargetValue,
     OUT UINT8 *pPcdTarget
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pTargetValue == NULL || pPcdTarget == NULL) {
    goto Finish;
  }

  if (StrLen(pTargetValue) == 0) {
    *pPcdTarget = PCD_TARGET_ALL;
  } else if (StrICmp(pTargetValue, PCD_CONFIG_TARGET_VALUE) == 0) {
    *pPcdTarget = PCD_TARGET_CONFIG;
  } else if (StrICmp(pTargetValue, PCD_LSA_TARGET_VALUE) == 0) {
    *pPcdTarget = PCD_TARGET_NAMESPACES;
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the Show PCD command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_FOUND couldn't open Config Protocol
  @retval EFI_ABORTED internal
**/
EFI_STATUS
ShowPcd(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DIMM_PCD_INFO *pDimmPcdInfo = NULL;
  UINT32 DimmPcdInfoCount = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  CHAR16 *pTargetValue = NULL;
  UINT8 PcdTarget = PCD_TARGET_ALL;
  UINT32 Index = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];

  NVDIMM_ENTRY();

  SetDisplayInfo(L"ShowPcd", ResultsView);

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
  ReturnCode = GetPcdTarget(pTargetValue, &PcdTarget);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_PCD);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetPcd(pNvmDimmConfigProtocol, PcdTarget, pDimmIds, DimmIdsCount,
      &pDimmPcdInfo, &DimmPcdInfoCount, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(L"Get Platform Config Data", L" on", pCommandStatus);
    goto Finish;
  }

  for (Index = 0; Index < DimmPcdInfoCount; Index++) {
    ReturnCode = GetPreferredDimmIdAsString(pDimmPcdInfo[Index].DimmId, pDimmPcdInfo[Index].DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n\n", DIMM_ID_STR, DimmStr);

    if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_CONFIG) {
      PrintPcdConfigurationHeader(pDimmPcdInfo[Index].pConfHeader);
    }

    if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_NAMESPACES) {
      PrintLabelStorageArea(pDimmPcdInfo[Index].pLabelStorageArea);
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FreeDimmPcdInfoArray(pDimmPcdInfo, DimmPcdInfoCount);
  pDimmPcdInfo = NULL;
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Show PCD command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowPcdCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowPcdCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
