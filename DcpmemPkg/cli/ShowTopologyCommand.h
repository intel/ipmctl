/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_TOPOLOGY_COMMAND_H_
#define _SHOW_TOPOLOGY_COMMAND_H_

#include "Uefi.h"
#include "CommandParser.h"

/**
  Register the show topology command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowTopologyCommand(
  );

/**
  show -dimm display options
  some of common display options are in CommandParser.h or in ShowDimmsCommand.h
**/
#define NODE_CONTROLLER_ID_STR      L"NodeControllerID"

/**
  Execute the show topology command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
ShowTopology(
  IN     struct Command *pCmd
  );

#endif /** _SHOW_TOPOLOGY_COMMAND_H_ **/
