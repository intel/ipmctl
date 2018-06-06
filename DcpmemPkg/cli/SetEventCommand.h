/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SET_EVENT_COMMAND_H_
#define _SET_EVENT_COMMAND_H_

#include <Uefi.h>

#define HELP_TEXT_SET_ACTION_REQ_PROPERTY L"0"

/**
Execute the Set Event command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
SetEvent(
  IN  struct Command *pCmd
);

/*
* Register the set event command
*/
EFI_STATUS
RegisterSetEventCommand(
);

#endif //_SET_EVENT_COMMAND_H_
