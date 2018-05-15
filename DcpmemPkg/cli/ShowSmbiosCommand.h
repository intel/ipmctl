/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_SMBIOS_COMMAND_H_
#define _SHOW_SMBIOS_COMMAND_H_

#include "CommandParser.h"

/**
  Register the show SMBIOS tables command.
**/
EFI_STATUS
registerShowSmbiosCommand(
  );

/**
  Execute the show SMBIOS command.
**/
EFI_STATUS
showSmbios(
  IN     struct Command *pCmd
  );

#endif /** _SHOW_SMBIOS_COMMAND_H_ **/
