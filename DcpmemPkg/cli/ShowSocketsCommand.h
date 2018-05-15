/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_SOCKETS_COMMAND_H_
#define _SHOW_SOCKETS_COMMAND_H_

#include "CommandParser.h"

#define MAPPED_MEMORY_LIMIT_STR L"MappedMemoryLimit"
#define TOTAL_MAPPED_MEMORY_STR L"TotalMappedMemory"

/**
  Register the show sockets command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowSocketsCommand(
  );

/**
  Execute the show sockets command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND failed to open Config protocol, or run-time preferences could
                        not be retrieved, or user-specified socket ID is invalid
  @retval Other errors returned by the driver
**/
EFI_STATUS
ShowSockets(
  IN     struct Command *pCmd
  );

#endif /* _SHOW_SOCKETS_COMMAND_H_*/
