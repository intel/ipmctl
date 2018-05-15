/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LOAD_GOAL_COMMAND_
#define _LOAD_GOAL_COMMAND_

#include <Uefi.h>

/**
  Register the Load Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterLoadGoalCommand();

/**
  Read and parse source file with Pool Goal structures to be loaded.

  @param[in] pFilePath Name is a pointer to a load file path
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the
                          physical device or logical device. The device path describes the location of the device
                          the handle is for.
  @param[out] pFileString Buffer for Pool Goal configuration from file

  @retval EFI_SUCCESS File read and parse success
  @retval EFI_INVALID_PARAMETER At least one of parameters is NULL or format of configuration in file is not proper
  @retval EFI_INVALID_PARAMETER memory allocation failure
**/
EFI_STATUS
ParseSourceDumpFile(
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT CHAR8 **pFileString
  );

/**
  Execute the Load Goal command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
LoadGoal(
  IN     struct Command *pCmd
  );

#endif /** _LOAD_GOAL_COMMAND_ **/
