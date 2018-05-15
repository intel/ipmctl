/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _START_DIAGNOSTIC_COMMAND_H_
#define _START_DIAGNOSTIC_COMMAND_H_

#include <Uefi.h>

/**
  Register the Start Diagnostic command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterStartDiagnosticCommand(
  );

/**
  Execute the Start Diagnostic command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
StartDiagnosticCmd(
  IN     struct Command *pCmd
  );

#endif /** _START_DIAGNOSTIC_COMMAND_H_ **/
