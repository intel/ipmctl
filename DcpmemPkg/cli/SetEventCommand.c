/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Types.h>
#include "CommandParser.h"
#include "SetEventCommand.h"
#include "event.h"
#include "Common.h"
#include "os_types.h"
#include "Convert.h"

EFI_STATUS
SetEvent(IN struct Command *pCmd);

/**
Command syntax definition
**/
struct Command SetEventCommand =
{
  SET_VERB,                                                          //!< verb
  {                                                                   //!< options
    { L"", L"", L"", L"", FALSE, ValueOptional },
  },
  {                                                                   //!< targets
    { EVENT_TARGET, L"", HELP_TEXT_EVENT_ID, TRUE, ValueRequired }
  },
  {																	//!< properties
    { ACTION_REQ_PROPERTY, L"", HELP_TEXT_SET_ACTION_REQ_PROPERTY, TRUE, ValueRequired },
  },
  L"Set event's action required flag on/off",         //!< help
  SetEvent
};

/**
Execute the Set Event command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
SetEvent(
	IN     struct Command *pCmd
)
{
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pPropertyValue = NULL;
  CHAR16 *pTargetValue = NULL;
  BOOLEAN IsNumber = FALSE;
  UINT64 ParsedNumber = 0;
  UINT32 EventId;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
	  Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
	  NVDIMM_DBG("Failed on InitializeCommandStatus");
	  goto Finish;
  }

  if (ContainTarget(pCmd, EVENT_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, EVENT_TARGET);
    IsNumber = GetU64FromString(pTargetValue, &ParsedNumber);
    if (!IsNumber) {
      NVDIMM_WARN("Event ID is not a number");
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_EVENT);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    else if (ParsedNumber > MAX_UINT32_VALUE) {
      NVDIMM_WARN("Event ID value %d is invalid", ParsedNumber);
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_EVENT);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    else {
      EventId = (UINT32) ParsedNumber;
    }
  }

  ReturnCode = GetPropertyValue(pCmd, ACTION_REQ_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If Count property exists, check it validity
    IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
    if (!IsNumber) {
      NVDIMM_WARN("Action required is not a number");
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_ACTION_REQUIRED);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    } else if (ParsedNumber != 0 ) {
      NVDIMM_WARN("Action required value %d is invalid", ParsedNumber);
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_ACTION_REQUIRED);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  } else {
    NVDIMM_WARN("Failed on GetPropertyValue");
    goto Finish;
  }

  if (0 == nvm_clear_action_required(EventId)) {
    ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
    Print(L"Acknowledge Event: Success\n");
  }
  else {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    Print(L"Acknowledge Event: Error\n");
  }

Finish:
  DisplayCommandStatus(L"Set event", L" on", pCommandStatus);
  FreeCommandStatus(&pCommandStatus);
  return ReturnCode;
}

/*
* Register the set event command
*/
EFI_STATUS
RegisterSetEventCommand(
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVDIMM_ENTRY();
  Rc = RegisterCommand(&SetEventCommand);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}
