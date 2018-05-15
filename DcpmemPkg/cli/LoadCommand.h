/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LOADCOMMAND_H_
#define _LOADCOMMAND_H_

#include <Uefi.h>
#include "CommandParser.h"
#include <Debug.h>
#include <Types.h>

/**
  Register the load command
**/
EFI_STATUS
RegisterLoadCommand (
  );

/**
  Execute the load command
  @param[in] pCmd the command structure that contains the user input data.

  @retval EFI_SUCCESS if everything went OK - including the firmware load process.
  @retval EFI_INVALID_PARAMETER if the user input is invalid or the file validation fails
  @retval EFI_UNSUPPORTED if the driver is not loaded or there are no AEPs in the system.
  @retval EFI_NOT_FOUND if there is no AEP with the user specified PID
**/
EFI_STATUS
Load(
  IN     struct Command *pCmd
  );

#endif /** _LOADCOMMAND_H_ **/
