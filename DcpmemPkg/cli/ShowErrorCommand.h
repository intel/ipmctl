/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_ERROR_COMMAND_H_
#define _SHOW_ERROR_COMMAND_H_

#include <Uefi.h>
#include "NvmInterface.h"
#include "Common.h"

#define ERROR_THERMAL_OCCURRED_STR       L"Thermal Error occurred"
#define ERROR_MEDIA_OCCURRED_STR         L"Media Error occurred"
#define ERROR_SYSTEM_TIMESTAMP_STR      L"System Timestamp"
#define ERROR_THERMAL_TEMPERATURE_STR   L"Temperature"
#define ERROR_THERMAL_REPORTED_STR      L"Reported"

#define ERROR_MEDIA_DPA_STR             L"DPA"
#define ERROR_MEDIA_PDA_STR             L"PDA"
#define ERROR_MEDIA_RANGE_STR           L"Range"
#define ERROR_MEDIA_ERROR_TYPE_STR      L"Error Type"

#define HELP_TEXT_ERROR_LOG_SEQ_NUM_PROPERTY     L"<0, 65535>"
#define HELP_TEXT_ERROR_LOG_COUNT_PROPERTY       L"<0, 255>"
#define HELP_TEXT_ERROR_LOG_LEVEL_PROPERTY       L"Low|High"


/**
  Register syntax of show -error
**/
EFI_STATUS
RegisterShowErrorCommand(
  );

/**
  Get error log command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
ShowErrorCommand(
  IN    struct Command *pCmd
  );

#endif
