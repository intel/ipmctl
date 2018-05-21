/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Types.h>
#include "CommandParser.h"
#include "ShowEventCommand.h"
#include "event.h"
#include "Common.h"
#include "os_types.h"
#include "Convert.h"

EFI_STATUS
ShowEvent(IN struct Command *pCmd);

/**
Command syntax definition
**/
struct Command ShowEventCommand =
{
    SHOW_VERB,                                                          //!< verb
    {                                                                   //!< options
        { L"", L"", L"", L"", FALSE, ValueOptional },
    },
    {                                                                   //!< targets
        { EVENT_TARGET, L"", L"", TRUE, ValueEmpty },
        { DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired }
    },
    {																	//!< properties
        { SEVERITY_PROPERTY, L"", HELP_TEXT_EVENT_SEVERITY_PROPERTY, FALSE, ValueRequired },
        { CATEGORY_PROPERTY, L"", HELP_TEXT_EVENT_CATEGORY_PROPERTY, FALSE, ValueRequired },
        { ACTION_REQ_PROPERTY, L"", HELP_TEXT_EVENT_ACTION_REQ_PROPERTY, FALSE, ValueRequired },
        { COUNT_PROPERTY, L"", HELP_TEXT_EVENT_COUNT_PROPERTY, FALSE, ValueRequired }
    },
    L"Show event stored on one in the system log",         //!< help
    ShowEvent
};

CHAR8 g_ascii_str[1024];
#define TO_ASCII(x) UnicodeStrToAsciiStrS(x, g_ascii_str, 1024)

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
)
{
	COMMAND_STATUS *pCommandStatus = NULL;
	EFI_STATUS ReturnCode = EFI_SUCCESS;
	CHAR16 *pPropertyValue = NULL;
	BOOLEAN IsNumber = FALSE;
	enum system_event_type SeverityType;
    enum system_event_category CategoryType;
    BOOLEAN ActrionReq;
	BOOLEAN IsSeverityTypeConfigured = FALSE;
    BOOLEAN IsCategoryTypeConfigured = FALSE;
    BOOLEAN IsActionReqTypeConfigured = FALSE;
	UINT64 ParsedNumber = 0;
	INT32 RequestedCount = EVENT_LOG_DEFAULT_COUNT;
	CHAR8 *StringBuffer = NULL;
    CHAR16 *WStringBuffer = NULL;
    UINT32 EventTypeMask = 0;
    EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
    DIMM_INFO *pDimms = NULL;
    UINT32 DimmCount = 0;
    CHAR16 *pTargetValue = NULL;
    char DimmUid[MAX_DIMM_UID_LENGTH]= { 0 };
    BOOLEAN IsDimmUidConfigured = FALSE;

    SetDisplayInfo(L"Event", TableTabView);

	ReturnCode = InitializeCommandStatus(&pCommandStatus);
	if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
		Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
		NVDIMM_DBG("Failed on InitializeCommandStatus");
		goto Finish;
	}

    // NvmDimmConfigProtocol required
    ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
    if (EFI_ERROR(ReturnCode)) {
        Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
        ReturnCode = EFI_NOT_FOUND;
        goto Finish;
    }

    // Populate the list of DIMM_INFO structures with relevant information
    ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
    if (EFI_ERROR(ReturnCode)) {
        goto Finish;
    }

    // check targets
    if (ContainTarget(pCmd, DIMM_TARGET)) {
        pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
        ReturnCode = GetDimmUidFromString(pTargetValue, pDimms, DimmCount, DimmUid);
        if (EFI_ERROR(ReturnCode)) {
            NVDIMM_ERR("Failed on GetDimmUidFromString");
            goto Finish;
        }
        IsDimmUidConfigured = TRUE;
    }

	ReturnCode = GetPropertyValue(pCmd, SEVERITY_PROPERTY, &pPropertyValue);
	if (!EFI_ERROR(ReturnCode)) {
		// If level property exists, check it validity
		if (StrICmp(pPropertyValue, PROPERTY_SEVERITY_VALUE_INFO) == 0) {
			SeverityType = SYSTEM_EVENT_TYPE_INFO;
            IsSeverityTypeConfigured = TRUE;
		} else if (StrICmp(pPropertyValue, PROPERTY_SEVERITY_VALUE_WARN) == 0) {
            SeverityType = SYSTEM_EVENT_TYPE_WARNING;
            IsSeverityTypeConfigured = TRUE;
		} else if (StrICmp(pPropertyValue, PROPERTY_SEVERITY_VALUE_ERROR) == 0) {
            SeverityType = SYSTEM_EVENT_TYPE_ERROR;
            IsSeverityTypeConfigured = TRUE;
		} else {
			ReturnCode = EFI_INVALID_PARAMETER;
			NVDIMM_WARN("Invalid Event Severity. Error Level can be %s ", TO_ASCII(HELP_TEXT_EVENT_SEVERITY_PROPERTY));
			Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_LEVEL);
			goto Finish;
		}
	} else {
		// If level property doesn't exists is ok, it is optional param, using default value
        IsSeverityTypeConfigured = FALSE;
		ReturnCode = EFI_SUCCESS;
	}

    ReturnCode = GetPropertyValue(pCmd, CATEGORY_PROPERTY, &pPropertyValue);
    if (!EFI_ERROR(ReturnCode)) {
        // If level property exists, check it validity
        if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_DIAG) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_DIAG;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_FW) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_FW;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_PLATCONF) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_CONFIG;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_PM) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_PM;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_QUICK) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_QUICK;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_SECURITY) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_SECURITY;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_HEALTH) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_HEALTH;
            IsCategoryTypeConfigured = TRUE;
        } else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_MGMT) == 0) {
            CategoryType = SYSTEM_EVENT_CAT_MGMT;
            IsCategoryTypeConfigured = TRUE;
        } else {
            ReturnCode = EFI_INVALID_PARAMETER;
            NVDIMM_WARN("Invalid Event Category. Supported categories %s ", TO_ASCII(HELP_TEXT_EVENT_CATEGORY_PROPERTY));
            Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_CATEGORY);
            goto Finish;
        }
    } else {
        // If level property doesn't exists is ok, it is optional param, using default value
        IsCategoryTypeConfigured = FALSE;
        ReturnCode = EFI_SUCCESS;
    }

    ReturnCode = GetPropertyValue(pCmd, ACTION_REQ_PROPERTY, &pPropertyValue);
    if (!EFI_ERROR(ReturnCode)) {
        // If Count property exists, check it validity
        IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
        if (!IsNumber) {
            NVDIMM_WARN("Action required is not a number");
            Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_ACTION_REQUIRED);
            ReturnCode = EFI_INVALID_PARAMETER;
            goto Finish;
        } else if (ParsedNumber == 0) {
            ActrionReq = FALSE;
            IsActionReqTypeConfigured = TRUE;
        } else if (ParsedNumber == 1) {
            ActrionReq = TRUE;
            IsActionReqTypeConfigured = TRUE;
        } else {
            NVDIMM_WARN("Action required value %d is invalid", ParsedNumber);
            Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_ACTION_REQUIRED);
            ReturnCode = EFI_INVALID_PARAMETER;
            goto Finish;
        }
    } else {
        // If count property doesn't exists is ok, it is optional param, using default value
        ReturnCode = EFI_SUCCESS;
    }

    ReturnCode = GetPropertyValue(pCmd, COUNT_PROPERTY, &pPropertyValue);
	if (!EFI_ERROR(ReturnCode)) {
		// If Count property exists, check it validity
		IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
		if (!IsNumber) {
			NVDIMM_WARN("Count value is not a number");
			Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
			ReturnCode = EFI_INVALID_PARAMETER;
			goto Finish;
		} else if ((ParsedNumber > EVENT_LOG_MAX_COUNT) || (ParsedNumber < EVENT_LOG_MIN_COUNT)) {
			NVDIMM_WARN("Count value %d doesn't fit into the range %d to %d", ParsedNumber, EVENT_LOG_MIN_COUNT, EVENT_LOG_MAX_COUNT);
			Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
			ReturnCode = EFI_INVALID_PARAMETER;
			goto Finish;
		}
		RequestedCount = (INT32)ParsedNumber;
	} else {
		// If count property doesn't exists is ok, it is optional param, using default value
		ReturnCode = EFI_SUCCESS;
	}

    // Create the event type mask
    if (TRUE == IsSeverityTypeConfigured) {
        switch (SeverityType)
        {
        case SYSTEM_EVENT_TYPE_INFO:
            EventTypeMask |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_INFO));
        case SYSTEM_EVENT_TYPE_WARNING:
            EventTypeMask |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_WARNING));
        case SYSTEM_EVENT_TYPE_ERROR:
            EventTypeMask |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_ERROR));
	default:
            break;
        }
    }
    if (TRUE == IsCategoryTypeConfigured) {
        EventTypeMask |= SYSTEM_EVENT_TYPE_CATEGORY_SET(SYSTEM_EVENT_TO_MASK(CategoryType));
    }
    if (TRUE == IsActionReqTypeConfigured) {
        EventTypeMask |= SYSTEM_EVENT_TYPE_AR_STATUS_SET(TRUE) | SYSTEM_EVENT_TYPE_AR_EVENT_SET(ActrionReq);
    }
    // Neither count nor type configured, just get all events
    if (IsDimmUidConfigured) {
        nvm_get_events_from_file(EventTypeMask, DimmUid, SYSTEM_EVENT_NOT_APPLICABLE, RequestedCount, NULL, &StringBuffer);
    }
    else {
        nvm_get_events_from_file(EventTypeMask, NULL, SYSTEM_EVENT_NOT_APPLICABLE, RequestedCount, NULL, &StringBuffer);
    }
    ResetCmdStatus(pCommandStatus, NVM_SUCCESS);

	if (NULL != StringBuffer) {
		size_t StringLen = AsciiStrSize(StringBuffer) + 1;
		WStringBuffer = (CHAR16 *) AllocateZeroPool(StringLen * sizeof(CHAR16));
		if (NULL != WStringBuffer) {
			if (NULL == AsciiStrToUnicodeStrS(StringBuffer, WStringBuffer, StringLen)) {
				ReturnCode = EFI_ABORTED;
				goto Finish;
			}
		} else {
			ReturnCode = EFI_BUFFER_TOO_SMALL;
			goto Finish;
		}
		Print(L"Time\tEventID\tSeverity\tActionRequired\tMessage\n");
		Print(FORMAT_STR, WStringBuffer);
	}

Finish:
	DisplayCommandStatus(L"Show event", L" on", pCommandStatus);
	FreeCommandStatus(&pCommandStatus);
    FREE_POOL_SAFE(StringBuffer);
    FREE_POOL_SAFE(WStringBuffer);
    FREE_POOL_SAFE(pDimms);
	return ReturnCode;
}

/*
* Register the show dimms command
*/
EFI_STATUS
RegisterShowEventCommand(
)
{
	EFI_STATUS Rc = EFI_SUCCESS;
	NVDIMM_ENTRY();
	Rc = RegisterCommand(&ShowEventCommand);

	NVDIMM_EXIT_I64(Rc);
	return Rc;
}
