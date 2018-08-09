/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_REGISTER_COMMAND_H_
#define _SHOW_REGISTER_COMMAND_H_

#include "CommandParser.h"

#define FW_MB_SMALL_OUTPUT_REG_USED 1

/**
  Register the show register command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowRegisterCommand(
  );

/**
  Execute the show register command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
ShowRegister(
  IN     struct Command *pCmd
  );

#endif /* _SHOW_REGISTER_COMMAND_H_ */
