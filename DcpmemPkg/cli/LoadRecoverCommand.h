/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LOADRECOVERCOMMAND_H_
#define _LOADRECOVERCOMMAND_H_

#include <Uefi.h>
#include "CommandParser.h"
#include <Debug.h>
#include <Types.h>

/**
  Register the load recover command
**/
EFI_STATUS
RegisterLoadRecoverCommand (
);

/**
  Execute the load recover command
  @param[in] pCmd the command structure that contains the user input data.

  @retval EFI_SUCCESS if everything went OK - including the firmware load process.
  @retval EFI_INVALID_PARAMETER if the user input is invalid or the file validation fails
  @retval EFI_UNSUPPORTED if the driver is not loaded or there are no AEPs in the system.
  @retval EFI_NOT_FOUND if there is no AEP with the user specified PID
**/
EFI_STATUS
LoadRecover(
  IN     struct Command *pCmd
);

#endif /** _LOADRECOVERCOMMAND_H_ **/
