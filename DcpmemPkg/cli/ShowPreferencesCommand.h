/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_PREFERENCES_COMMAND_
#define _SHOW_PREFERENCES_COMMAND_

#include <Uefi.h>
#include "CommandParser.h"

/**
  Register the Show Preferences command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowPreferencesCommand();

/**
  Execute the Show Preferences command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowPreferences(
  IN     struct Command *pCmd
  );

#endif /** _SHOW_PREFERENCES_COMMAND_ **/
