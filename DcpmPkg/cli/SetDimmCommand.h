/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SET_DIMM_COMMAND_
#define _SET_DIMM_COMMAND_

#include <Uefi.h>
#include "CommandParser.h"
#include <Convert.h>
#define FW_LOG_LEVELS_COUNT 4
#define FW_LOG_LEVELS_INVALID_LEVEL 255

#define POISON_MEMORY_TYPE_COUNT 4
#define POISON_MEMORY_TYPE_MEMORYMODE  0x01 // currently allocated in Memory mode
#define POISON_MEMORY_TYPE_APPDIRECT   0x02 // currently allocated in AppDirect
#define POISON_MEMORY_TYPE_PATROLSCRUB 0x04// simulating an error found during a patrol scrub operation

/**
  Register the set dimm command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterSetDimmCommand();

/**
  Execute the set dimm command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
SetDimm(
  IN     struct Command *pCmd
  );

/**
  Print detailed information on root casuse of failure

  @param[in] FwReturnCode is a FW status code returned by PassThru protocol
  **/
void
PrintFailureDetails(
  IN     UINT8 FwReturnCode
  );

#endif /** _SET_DIMM_COMMAND_ **/
