/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "DeleteGoalCommand.h"
#include "Common.h"

/**
  Command syntax definition
**/
struct Command DeleteGoalCommand =
{
  DELETE_VERB,                                                        //!< verb
  {{L"", L"", L"", L"", FALSE, ValueOptional}},                       //!< options
  {                                                                   //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                               //!< properties
  L"Delete the region configuration goal from one or more DIMMs",        //!< help
  DeleteGoal
};


/**
  Execute the Delete Goal command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
DeleteGoal(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pTargetValue = NULL;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketIdsCount = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;

  NVDIMM_ENTRY();

  SetDisplayInfo(L"DeleteGoalResults", ResultsView);

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** Need NvmDimmConfigProtocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pTargetValue, &pSocketIds, &SocketIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      NVDIMM_DBG("Failed on GetSocketsFromString");
      goto Finish;
    }
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->DeleteGoalConfig(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pSocketIds,
      SocketIdsCount, pCommandStatus);

  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);

  DisplayCommandStatus(L"Delete memory allocation goal", L" from", pCommandStatus);

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Delete Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDeleteGoalCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DeleteGoalCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

