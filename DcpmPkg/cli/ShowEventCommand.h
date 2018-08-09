/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_EVENT_COMMAND_H_
#define _SHOW_EVENT_COMMAND_H_

#include <Uefi.h>

#define PROPERTY_SEVERITY_VALUE_INFO		L"Info"
#define PROPERTY_SEVERITY_VALUE_WARN		L"Warning"
#define PROPERTY_SEVERITY_VALUE_ERROR		L"Error"

#define PROPERTY_CATEGORY_VALUE_DIAG        L"Diag"
#define PROPERTY_CATEGORY_VALUE_FW          L"FW"
#define PROPERTY_CATEGORY_VALUE_PLATCONF    L"Config"
#define PROPERTY_CATEGORY_VALUE_PM          L"PM"
#define PROPERTY_CATEGORY_VALUE_QUICK       L"Quick"
#define PROPERTY_CATEGORY_VALUE_SECURITY    L"Security"
#define PROPERTY_CATEGORY_VALUE_HEALTH      L"Health"
#define PROPERTY_CATEGORY_VALUE_MGMT        L"Mgmt"

#define HELP_TEXT_EVENT_SEVERITY_PROPERTY   L""PROPERTY_SEVERITY_VALUE_INFO"|"PROPERTY_SEVERITY_VALUE_WARN"|"PROPERTY_SEVERITY_VALUE_ERROR
#define HELP_TEXT_EVENT_CATEGORY_PROPERTY   L""PROPERTY_CATEGORY_VALUE_DIAG"|"PROPERTY_CATEGORY_VALUE_FW"|"PROPERTY_CATEGORY_VALUE_PLATCONF"|"\
                                            PROPERTY_CATEGORY_VALUE_PM"|"PROPERTY_CATEGORY_VALUE_QUICK"|"PROPERTY_CATEGORY_VALUE_SECURITY"|"\
                                            PROPERTY_CATEGORY_VALUE_HEALTH"|"PROPERTY_CATEGORY_VALUE_MGMT
#define HELP_TEXT_EVENT_ACTION_REQ_PROPERTY L"1|0"
#define HELP_TEXT_EVENT_COUNT_PROPERTY		L"<1..2147483647>"

#define EVENT_LOG_MAX_COUNT         0x7fffffff
#define EVENT_LOG_MIN_COUNT         1
#define EVENT_LOG_DEFAULT_COUNT     50

/**
Execute the Show Goal command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowEvent(
	IN     struct Command *pCmd
);

/*
* Register the show dimms command
*/
EFI_STATUS
RegisterShowEventCommand(
);

#endif //_SHOW_EVENT_COMMAND_H_
