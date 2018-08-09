/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_CAP_COMMAND_H_
#define _SHOW_CAP_COMMAND_H_

#include "CommandParser.h"

/**
  Register the show cmd access policy command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowCmdAccessPolicyCommand(
  );

/**
  Execute the show cmd access policy command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND failed to open Config protocol, or run-time preferences could
                        not be retrieved
  @retval Other errors returned by the driver
**/
EFI_STATUS
ShowCmdAccessPolicy(
  IN     struct Command *pCmd
  );

#endif /* _SHOW_CAP_COMMAND_H_*/
