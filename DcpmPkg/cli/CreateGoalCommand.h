/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CREATE_GOAL_COMMAND_
#define _CREATE_GOAL_COMMAND_

#include <Uefi.h>

// Cli will warn the user when suggested alignment is greater than this percentage
#define PROMPT_ALIGN_PERCENTAGE 10

/**
  Traverse targeted dimms to determine if Security is enabled in Unlocked state

  @param[in] SecurityFlag Security mask from FW

  @retval TRUE if at least one targeted dimm has security enabled in unlocked state
  @retval FALSE if none of targeted dimms have security enabled in unlocked state
**/
EFI_STATUS
EFIAPI
AreRequestedDimmsSecurityUnlocked(
  IN     DIMM_INFO *pDimmInfo,
  IN     UINT32 DimmCount,
  IN     UINT16 *ppDimmIds,
  IN     UINT32 pDimmIdsCount,
  OUT BOOLEAN *isDimmUnlocked
);

/**
  Register the Create Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterCreateGoalCommand();

/**
  Execute the Create Goal command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
CreateGoal(
  IN     struct Command *pCmd
  );

#endif /** _CREATE_GOAL_COMMAND_ **/
