/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SET_PREFERENCES_COMMAND_
#define _SET_PREFERENCES_COMMAND_

#include <Uefi.h>
#include "CommandParser.h"

#define CLI_SET_PREFERENCE_SUCCESS            L"Success"

#define PRINT_SET_PREFERENCES_EFI_ERR(PROPERTY, TYPE, RC) \
  Print(L"Set " FORMAT_STR L"=" FORMAT_STR L": Error: %d\n", PROPERTY, TYPE, RC);

#define PRINT_SET_PREFERENCES_SUCCESS(PROPERTY, TYPE) \
  Print(L"Set " FORMAT_STR L"=" FORMAT_STR L": " FORMAT_STR L"\n", PROPERTY, TYPE, CLI_SET_PREFERENCE_SUCCESS);


/**
  Register the Set Preferences command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterSetPreferencesCommand();

/**
  Execute the Set Preferences command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
SetPreferences(
  IN     struct Command *pCmd
  );

#endif /** _SET_PREFERENCES_COMMAND_ **/
