/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SRC_CLI_SHOW_MEMORY_RESOURCES_COMMAND_H_
#define _SRC_CLI_SHOW_MEMORY_RESOURCES_COMMAND_H_

#include "CommandParser.h"

#define DISPLAYED_CAPACITY_STR                L"Capacity"
#define DISPLAYED_MEMORY_CAPACITY_STR         L"MemoryCapacity"
#define DISPLAYED_APPDIRECT_CAPACITY_STR      L"AppDirectCapacity"
#define DISPLAYED_UNCONFIGURED_CAPACITY_STR   L"UnconfiguredCapacity"
#define DISPLAYED_INACCESSIBLE_CAPACITY_STR   L"InaccessibleCapacity"
#define DISPLAYED_RESERVED_CAPACITY_STR       L"ReservedCapacity"

/**
  Execute the show memory resources command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
ShowMemoryResources(
  IN     struct Command *pCmd
  );

/**
  Register the show memory resources command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowMemoryResourcesCommand(
  );

#endif /* _SRC_CLI_SHOW_MEMORY_RESOURCES_COMMAND_H_ */
