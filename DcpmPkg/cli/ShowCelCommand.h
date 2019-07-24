/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_CEL_COMMAND_H_
#define _SHOW_CEL_COMMAND_H_

#include <Uefi.h>
#include "NvmInterface.h"
#include "Common.h"

// Command Effect Description strings
#define NO_EFFECTS                              L"NE"
#define SECURITY_STATE_CHANGE                   L"SSC"
#define DIMM_CONFIGURATION_CHANGE_AFTER_REBOOT  L"DCC"
#define IMMEDIATE_DIMM_CONFIGURATION_CHANGE     L"IDCC"
#define QUIESCE_ALL_IO                          L"QIO"
#define IMMEDIATE_DIMM_DATA_CHANGE              L"IDDC"
#define TEST_MODE                               L"TM"
#define DEBUG_MODE                              L"DM"
#define IMMEDIATE_DIMM_POLICY_CHANGE            L"IDPC"
#define OPCODE_NOT_SUPPORTED                    L"N/A"

/**
  Register syntax of show -error
**/
EFI_STATUS
RegisterShowCelCommand(
);

/**
  Get command effect log command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
ShowCelCommand(
  IN    struct Command *pCmd
);

#endif