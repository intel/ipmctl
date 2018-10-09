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

#define DS_ROOT_PATH                       L"/EventList"
#define DS_EVENT_PATH                      L"/EventList/EventEntry"
#define DS_EVENT_INDEX_PATH                L"/EventList/EventEntry[%d]"
#define EVENT_ENTRY_TOKEN_CNT              6
#define EVENT_ENTRY_TOKEN_DELIM            L'\t'
#define EVENT_ENTRY_EOL                    L'\n'
#define EVENT_ENTRY_TIME_STR               L"Time"
#define EVENT_ENTRY_TIME_MAX_STR_WIDTH     20
#define EVENT_ENTRY_ID_STR                 L"EventID"
#define EVENT_ENTRY_ID_MAX_STR_WIDTH       8
#define EVENT_ENTRY_SEVERITY_STR           L"Severity"
#define EVENT_ENTRY_SEVERITY_MAX_STR_WIDTH 13
#define EVENT_ACTION_REQ_STR               L"ActionRequired"
#define EVENT_ENTRY_CODE_STR               L"Code"
#define EVENT_ENTRY_CODE_MAX_STR_WIDTH     5
#define EVENT_MESG_STR                     L"Message"
#define EVENT_MESG_MAX_STR_WIDTH           30

 /*
  *  PRINTER TABLE ATTRIBUTES (6 columns)
  *   Time | EventID | Severity | AR | Code | Message
  *   ================================================
  *   X    | X       | X        | X  | X    | X
  *   ...
  */
PRINTER_TABLE_ATTRIB ShowEventsTableAttributes =
{
  {
    {
      EVENT_ENTRY_TIME_STR,                                 //COLUMN HEADER
      EVENT_ENTRY_TIME_MAX_STR_WIDTH,                       //COLUMN MAX STR WIDTH
      DS_EVENT_PATH PATH_KEY_DELIM EVENT_ENTRY_TIME_STR     //COLUMN DATA PATH
    },
    {
      EVENT_ENTRY_ID_STR,                                    //COLUMN HEADER
      EVENT_ENTRY_ID_MAX_STR_WIDTH,                          //COLUMN MAX STR WIDTH
      DS_EVENT_PATH PATH_KEY_DELIM EVENT_ENTRY_ID_STR        //COLUMN DATA PATH
    },
    {
      EVENT_ENTRY_SEVERITY_STR,                               //COLUMN HEADER
      EVENT_ENTRY_SEVERITY_MAX_STR_WIDTH,                     //COLUMN MAX STR WIDTH
      DS_EVENT_PATH PATH_KEY_DELIM EVENT_ENTRY_SEVERITY_STR   //COLUMN DATA PATH
    },
    {
      EVENT_ACTION_REQ_STR,                                   //COLUMN HEADER
      AR_MAX_STR_WIDTH,                                       //COLUMN MAX STR WIDTH
      DS_EVENT_PATH PATH_KEY_DELIM EVENT_ACTION_REQ_STR       //COLUMN DATA PATH
    },
    {
      EVENT_ENTRY_CODE_STR,                                   //COLUMN HEADER
      EVENT_ENTRY_CODE_MAX_STR_WIDTH,                         //COLUMN MAX STR WIDTH
      DS_EVENT_PATH PATH_KEY_DELIM EVENT_ENTRY_CODE_STR       //COLUMN DATA PATH
    },
    {
      EVENT_MESG_STR,                                         //COLUMN HEADER
      EVENT_MESG_MAX_STR_WIDTH,                               //COLUMN MAX STR WIDTH
      DS_EVENT_PATH PATH_KEY_DELIM EVENT_MESG_STR             //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowEventsDataSetAttribs =
{
  NULL,
  &ShowEventsTableAttributes
};


EFI_STATUS
ShowEvent(IN struct Command *pCmd);

/**
Command syntax definition
**/
struct Command ShowEventCommand =
{
    SHOW_VERB,                                                          //!< verb
    {                                                                   //!< options
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}                         //!< options
#endif
    },
    {                                                                   //!< targets
        { EVENT_TARGET, L"", L"", TRUE, ValueEmpty },
        { DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional }
    },
    {																	//!< properties
        { SEVERITY_PROPERTY, L"", HELP_TEXT_EVENT_SEVERITY_PROPERTY, FALSE, ValueRequired },
        { CATEGORY_PROPERTY, L"", HELP_TEXT_EVENT_CATEGORY_PROPERTY, FALSE, ValueRequired },
        { ACTION_REQ_PROPERTY, L"", HELP_TEXT_EVENT_ACTION_REQ_PROPERTY, FALSE, ValueRequired },
        { COUNT_PROPERTY, L"", HELP_TEXT_EVENT_COUNT_PROPERTY, FALSE, ValueRequired }
    },
    L"Show event stored on one in the system log",         //!< help
    ShowEvent,
    TRUE,                                               //!< enable print control support
};

CHAR8 g_ascii_str[1024];
#define TO_ASCII(x) (UnicodeStrToAsciiStrS(x, g_ascii_str, 1024) == RETURN_SUCCESS) ? g_ascii_str : ""
static EFI_STATUS AddEventData(PRINT_CONTEXT *pPrinterCtx, CHAR16 *EventEntry);
static EFI_STATUS ProcessEvents(PRINT_CONTEXT *pPrinterCtx, CHAR16 *EventLog);

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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 *pTargetValue = NULL;
  char DimmUid[MAX_DIMM_UID_LENGTH]= { 0 };
  BOOLEAN IsDimmUidConfigured = FALSE;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

	ReturnCode = InitializeCommandStatus(&pCommandStatus);
	if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
		NVDIMM_DBG("Failed on InitializeCommandStatus");
		goto Finish;
	}

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    // Allow show event to run even if there are no DIMMs
    if (ReturnCode != EFI_NOT_FOUND) {
      goto Finish;
    }
  }

  // check targets
  if ((ContainTarget(pCmd, DIMM_TARGET) && (DimmCount > 0))) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmUidFromString(pTargetValue, pDimms, DimmCount, DimmUid);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
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
    }
    else if (StrICmp(pPropertyValue, PROPERTY_SEVERITY_VALUE_WARN) == 0) {
      SeverityType = SYSTEM_EVENT_TYPE_WARNING;
      IsSeverityTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_SEVERITY_VALUE_ERROR) == 0) {
      SeverityType = SYSTEM_EVENT_TYPE_ERROR;
      IsSeverityTypeConfigured = TRUE;
    }
    else {
      ReturnCode = EFI_INVALID_PARAMETER;
      NVDIMM_WARN("Invalid Event Severity. Error Level can be %s ", TO_ASCII(HELP_TEXT_EVENT_SEVERITY_PROPERTY));
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_LEVEL);
      goto Finish;
    }
  }
  else {
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
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_FW) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_FW;
      IsCategoryTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_PLATCONF) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_CONFIG;
      IsCategoryTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_PM) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_PM;
      IsCategoryTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_QUICK) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_QUICK;
      IsCategoryTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_SECURITY) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_SECURITY;
      IsCategoryTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_HEALTH) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_HEALTH;
      IsCategoryTypeConfigured = TRUE;
    }
    else if (StrICmp(pPropertyValue, PROPERTY_CATEGORY_VALUE_MGMT) == 0) {
      CategoryType = SYSTEM_EVENT_CAT_MGMT;
      IsCategoryTypeConfigured = TRUE;
    }
    else {
      ReturnCode = EFI_INVALID_PARAMETER;
      NVDIMM_WARN("Invalid Event Category. Supported categories %s ", TO_ASCII(HELP_TEXT_EVENT_CATEGORY_PROPERTY));
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_CATEGORY);
      goto Finish;
    }
  }
  else {
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
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_ACTION_REQUIRED);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    else if (ParsedNumber == 0) {
      ActrionReq = FALSE;
      IsActionReqTypeConfigured = TRUE;
    }
    else if (ParsedNumber == 1) {
      ActrionReq = TRUE;
      IsActionReqTypeConfigured = TRUE;
    }
    else {
      NVDIMM_WARN("Action required value %d is invalid", ParsedNumber);
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_ACTION_REQUIRED);
      goto Finish;
    }
  }
  else {
    // If count property doesn't exists is ok, it is optional param, using default value
    ReturnCode = EFI_SUCCESS;
  }

  ReturnCode = GetPropertyValue(pCmd, COUNT_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If Count property exists, check it validity
    IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
    if (!IsNumber) {
      NVDIMM_WARN("Count value is not a number");
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
      goto Finish;
    }
    else if ((ParsedNumber > EVENT_LOG_MAX_COUNT) || (ParsedNumber < EVENT_LOG_MIN_COUNT)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      NVDIMM_WARN("Count value %d doesn't fit into the range %d to %d", ParsedNumber, EVENT_LOG_MIN_COUNT, EVENT_LOG_MAX_COUNT);
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
      goto Finish;
    }
    RequestedCount = (INT32)ParsedNumber;
  }
  else {
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
		size_t StringLen = (size_t)(AsciiStrSize(StringBuffer) + 1);
    WStringBuffer = (CHAR16 *)AllocateZeroPool(StringLen * sizeof(CHAR16));
    if (NULL != WStringBuffer) {
      if (RETURN_SUCCESS != AsciiStrToUnicodeStrS(StringBuffer, WStringBuffer, StringLen)) {
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }
    else {
      ReturnCode = EFI_BUFFER_TOO_SMALL;
      goto Finish;
    }
    if (EFI_SUCCESS != (ReturnCode = ProcessEvents(pPrinterCtx, WStringBuffer))) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
  }
  else {
    //WA, to ensure ESX prints a message when no entries are found.
    if (PRINTER_ESX_FORMAT_ENABLED(pPrinterCtx)) {
      ResetCmdStatus(pCommandStatus, NVM_SUCCESS_NO_EVENT_FOUND);
      ReturnCode = EFI_NOT_FOUND;
    }
  }

  //Switch text output type to display as a table
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(pPrinterCtx);
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowEventsDataSetAttribs);
Finish:
  PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, L"Show event", L" on", pCommandStatus);
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
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

static EFI_STATUS
ProcessEvents(PRINT_CONTEXT *pPrinterCtx, CHAR16 *EventLog) {
  CHAR16 **ppSplitEventLogLines = NULL;
  UINT32 NumTokens = 0;
  UINT32 Index = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (NULL == pPrinterCtx || NULL == EventLog) {
    return EFI_INVALID_PARAMETER;
  }

  ppSplitEventLogLines = StrSplit(EventLog, EVENT_ENTRY_EOL, &NumTokens);
  if (ppSplitEventLogLines == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Index = 0; Index < NumTokens; ++Index) {
    if (EFI_SUCCESS != (ReturnCode = AddEventData(pPrinterCtx, ppSplitEventLogLines[Index]))) {
      goto Finish;
    }
  }

Finish:
  FreeStringArray(ppSplitEventLogLines, NumTokens);
  return ReturnCode;
}

static EFI_STATUS
AddEventData(PRINT_CONTEXT *pPrinterCtx, CHAR16 *EventEntry) {
  CHAR16 **ppSplitEventEntryToken = NULL;
  UINT32 NumTokens = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pPath = NULL;
  static UINT32 EventCnt = 0;

  if (NULL == pPrinterCtx || NULL == EventEntry) {
    return EFI_INVALID_PARAMETER;
  }

  PRINTER_BUILD_KEY_PATH(pPath, DS_EVENT_INDEX_PATH, EventCnt);

  ppSplitEventEntryToken = StrSplit(EventEntry, EVENT_ENTRY_TOKEN_DELIM, &NumTokens);
  if (ppSplitEventEntryToken == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (EVENT_ENTRY_TOKEN_CNT != NumTokens) {
    ReturnCode = EFI_LOAD_ERROR;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EVENT_ENTRY_TIME_STR, ppSplitEventEntryToken[0]);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EVENT_ENTRY_ID_STR, ppSplitEventEntryToken[1]);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EVENT_ENTRY_SEVERITY_STR, ppSplitEventEntryToken[2]);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EVENT_ACTION_REQ_STR, ppSplitEventEntryToken[3]);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EVENT_ENTRY_CODE_STR, ppSplitEventEntryToken[4]);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, EVENT_MESG_STR, ppSplitEventEntryToken[5]);

  EventCnt++;
Finish:
  FREE_POOL_SAFE(pPath);
  FreeStringArray(ppSplitEventEntryToken, NumTokens);
  return ReturnCode;
}