/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DELETE_PCD_COMMAND_
#define _DELETE_PCD_COMMAND_

/**
  Register the Delete PCD command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDeletePcdCommand(
  );

/**
  Execute the Delete PCD command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
**/
EFI_STATUS
DeletePcdCmd(
  IN     struct Command *pCmd
  );

#endif /* _DELETE_PCD_COMMAND_ */
