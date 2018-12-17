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
  @retval EFI_UNSUPPORTED if the driver is not loaded or there are no DCPMMs in the system.
  @retval EFI_NOT_FOUND if there is no DIMM with the user specified PID
**/
EFI_STATUS
Load(
  IN     struct Command *pCmd
  );

/**
  For a given DIMM, this will evaluate what the return code should be
  @param[in] examine - if the examine flag was sent by the user
  @param[in] dimmReturnCode - the return code returned by the call to update the FW
  @param[in] dimmNvmStatus - the NVM status returned by the call to update the FW
  @param[out] generalNvmStatus - the NVM status to be applied to the general command status

  @retval the return code
**/
EFI_STATUS
GetDimmReturnCode(
  IN     BOOLEAN examine,
  IN EFI_STATUS dimmReturnCode,
  IN NVM_STATUS dimmNvmStatus,
  OUT NVM_STATUS * pGeneralNvmStatus
  );

#endif /** _LOADCOMMAND_H_ **/
