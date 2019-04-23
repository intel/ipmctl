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

#define MAX_CHECKS_FOR_SUCCESSFUL_STAGING 40
#define MICROSECONDS_PERIOD_BETWEEN_STAGING_CHECKS 250000
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

/**
  For the lists provided, this will block until all dimms indicated in the StagedFwDimmIds report a non-zero staged FW image. This
  is intended to be run after a non-recovery (normal) FW update.

  @param[in] pCmd - The command object
  @param[in] pCommandStatus - The command status object
  @param[in] pNvmDimmConfigProtocol - The open config protocol
  @param[in] pReturnCodes - The current list of return codes for each DIMM
  @param[in] pNvmCodes - The current list of NVM codes for the FW work of each DIMM
  @param[in] pDimmTargets - The list of DIMMs for which a FW update was attempted
  @param[in] pDimmTargetsNum - The list length of the pDimmTargets list

  @retval EFI_SUCCESS - All dimms staged their fw as expected.
  @retval EFI_xxxx - One or more DIMMS did not stage their FW as expected.
**/
EFI_STATUS
BlockForFwStage(
  IN   struct Command *pCmd,
  IN   COMMAND_STATUS *pCommandStatus,
  IN   EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN   EFI_STATUS *pReturnCodes,
  IN   NVM_STATUS *pNvmCodes,
  IN   DIMM_INFO *pDimmTargets,
  IN   UINT32 pDimmTargetsNum
);

/**
Check to see if a FW has already been staged on a DIMM

@param[in] pCmd - The command object
@param[in] pNvmDimmConfigProtocol - The open config protocol
@param[in] DimmID - The ID of the dimm to check. Must be a functional DIMM
**/
BOOLEAN
FwHasBeenStaged(
  IN   struct Command *pCmd,
  IN   EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN   UINT16 DimmID
);
#endif /** _LOADCOMMAND_H_ **/
