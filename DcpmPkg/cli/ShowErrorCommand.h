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
#define ERROR_SYSTEM_TIMESTAMP_STR       L"System Timestamp"
#define ERROR_THERMAL_TEMPERATURE_STR    L"Temperature"
#define ERROR_THERMAL_REPORTED_STR       L"Reported"
#define ERROR_THERMAL_TYPE_STR           L"Temperature Type"

#define ERROR_MEDIA_DPA_STR               L"DPA"
#define ERROR_MEDIA_PDA_STR               L"PDA"
#define ERROR_MEDIA_RANGE_STR             L"Range"
#define ERROR_MEDIA_ERROR_TYPE_STR        L"Error Type"
#define ERROR_MEDIA_ERROR_FLAGS_STR       L"Error Flags"
#define ERROR_MEDIA_TRANSACTION_TYPE_STR  L"Transaction Type"

#define ERROR_SEQUENCE_NUMBER  L"Sequence Number"

#define ERROR_THERMAL_REPORTED_USER_ALARM  0 //0b000
#define ERROR_THERMAL_REPORTED_LOW         1 //0b001
#define ERROR_THERMAL_REPORTED_HIGH        2 //0b010
#define ERROR_THERMAL_REPORTED_CRITICAL    4 //0b100

#define ERROR_THERMAL_REPORTED_USER_ALARM_STR  L"User Alarm Trip"
#define ERROR_THERMAL_REPORTED_LOW_STR         L"Low"
#define ERROR_THERMAL_REPORTED_HIGH_STR        L"High"
#define ERROR_THERMAL_REPORTED_CRITICAL_STR    L"Critical"
#define ERROR_THERMAL_REPORTED_UNKNOWN_STR     L"Unknown"

#define ERROR_THERMAL_TYPE_MEDIA        0x00
#define ERROR_THERMAL_TYPE_CONTROLLER   0x01

#define ERROR_THERMAL_TYPE_MEDIA_STR       L"Media Temperature"
#define ERROR_THERMAL_TYPE_CONTROLLER_STR  L"Controller Temperature"
#define ERROR_THERMAL_TYPE_UNKNOWN_STR     L"Unknown"

#define ERROR_TYPE_UNCORRECTABLE          0x00
#define ERROR_TYPE_DPA_MISMATCH           0x01
#define ERROR_TYPE_AIT_ERROR              0x02
#define ERROR_TYPE_DATA_PATH_ERROR        0x03
#define ERROR_TYPE_LOCKED_ILLEGAL_ACCESS  0x04
#define ERROR_TYPE_PERCENTAGE_REMAINING   0x05
#define ERROR_TYPE_SMART_CHANGE           0x06
#define ERROR_TYPE_PERSISTENT_WRITE_ECC   0x07

#define ERROR_TYPE_UNCORRECTABLE_STR          L"Uncorrectable"
#define ERROR_TYPE_DPA_MISMATCH_STR           L"DPA Mismatch"
#define ERROR_TYPE_AIT_ERROR_STR              L"AIT Error"
#define ERROR_TYPE_DATA_PATH_ERROR_STR        L"Data Path Error"
#define ERROR_TYPE_LOCKED_ILLEGAL_ACCESS_STR  L"Locked/Illegal Access"
#define ERROR_TYPE_PERCENTAGE_REMAINING_STR   L"User Percentage Remaining Alarm Trip"
#define ERROR_TYPE_SMART_CHANGE_STR           L"Smart Health Status Change"
#define ERROR_TYPE_PERSISTENT_WRITE_ECC_STR   L"Persistent Write ECC"
#define ERROR_TYPE_UNKNOWN_STR                L"Unknown"

#define ERROR_FLAGS_PDA_VALID_STR  L"PDA Valid"
#define ERROR_FLAGS_DPA_VALID_STR  L"DPA Valid"
#define ERROR_FLAGS_INTERRUPT_STR  L"Interrupt"
#define ERROR_FLAGS_VIRAL_STR      L"Viral"

#define TRANSACTION_TYPE_2LM_READ         0x00
#define TRANSACTION_TYPE_2LM_WRITE        0x01
#define TRANSACTION_TYPE_PM_READ          0x02
#define TRANSACTION_TYPE_PM_WRITE         0x03
#define TRANSACTION_TYPE_AIT_READ         0x06
#define TRANSACTION_TYPE_AIT_WRITE        0x07
#define TRANSACTION_TYPE_WEAR_LEVEL_MOVE  0x08
#define TRANSACTION_TYPE_PATROL_SCRUB     0x09
#define TRANSACTION_TYPE_CSR_READ         0x0A
#define TRANSACTION_TYPE_CSR_WRITE        0x0B
#define TRANSACTION_TYPE_ARS              0x0C
#define TRANSACTION_TYPE_UNAVAILABLE      0x0D

#define TRANSACTION_TYPE_2LM_READ_STR         L"2LM Read"
#define TRANSACTION_TYPE_2LM_WRITE_STR        L"2LM Write"
#define TRANSACTION_TYPE_PM_READ_STR          L"PM Read"
#define TRANSACTION_TYPE_PM_WRITE_STR         L"PM Write"
#define TRANSACTION_TYPE_AIT_READ_STR         L"AIT Read"
#define TRANSACTION_TYPE_AIT_WRITE_STR        L"AIT Write"
#define TRANSACTION_TYPE_WEAR_LEVEL_MOVE_STR  L"Wear Level Mode"
#define TRANSACTION_TYPE_PATROL_SCRUB_STR     L"Patrol Scrub"
#define TRANSACTION_TYPE_CSR_READ_STR         L"CSR Read"
#define TRANSACTION_TYPE_CSR_WRITE_STR        L"CSR Write"
#define TRANSACTION_TYPE_ARS_STR              L"Address Range Scrub"
#define TRANSACTION_TYPE_UNAVAILABLE_STR      L"Unavailable"
#define TRANSACTION_TYPE_UNKNOWN_STR          L"Unknown"

#define HELP_TEXT_ERROR_LOG_SEQ_NUM_PROPERTY     L"<1, 65535>"
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
