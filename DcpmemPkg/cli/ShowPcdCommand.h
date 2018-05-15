/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_PCD_COMMAND_
#define _SHOW_PCD_COMMAND_

/**
  Register the Show PCD command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowPcdCommand(
  );

/**
  Execute the Show PCD command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
**/
EFI_STATUS
ShowPcd(
  IN     struct Command *pCmd
  );

#endif /* _SHOW_PCD_COMMAND_ */
