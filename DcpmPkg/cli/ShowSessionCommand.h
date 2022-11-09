/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_SESSION_COMMAND_
#define _SHOW_SESSION_COMMAND_

#include "CommandParser.h"


/**
Execute the show host server command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_OUT_OF_RESOURCES memory allocation failure
@retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
ShowSession(
   IN     struct Command *pCmd
);

/**
  Register the show session command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowSessionCommand(
  );

#endif /* _SHOW_SESSION_COMMAND_ */
