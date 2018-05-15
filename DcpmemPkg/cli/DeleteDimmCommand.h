/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DELETE_DIMM_COMMAND_
#define _DELETE_DIMM_COMMAND_

#include <Uefi.h>
#include "CommandParser.h"

#define ERASE_STR                           L"Erase"

/**
  Register the delete dimm command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDeleteDimmCommand();

/**
  Execute the delete dimm command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
DeleteDimm(
  IN     struct Command *pCmd
  );

#endif /** _DELETE_DIMM_COMMAND_ **/
