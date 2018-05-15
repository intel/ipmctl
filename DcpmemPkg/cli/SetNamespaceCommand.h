/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SET_NAMESPACE_COMMAND_H_
#define _SET_NAMESPACE_COMMAND_H_

#include <Uefi.h>
#include "Common.h"

/**
  Register the set namespace command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterSetNamespaceCommand(
  );

/**
  Execute the set namespace command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
SetNamespace(
  IN     struct Command *pCmd
  );

#endif /** _SET_NAMESPACE_COMMAND_H_ **/
