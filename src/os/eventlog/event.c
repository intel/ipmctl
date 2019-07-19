/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of the common eventing functionality.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "event.h"
#include "NvmStatus.h"
#include <NvmTypes.h>
#include <os_efi_preferences.h>
#include <Debug.h>
#include <PrintLib.h>
#include "s_str.h"
#include "os.h"
#include <os_efi_api.h>
#include "os_str.h"

/*
* Retrive a defined number of event log entries specified by the mask from the system event log
*
* @return The size of the event_buffer
*/
NVM_API size_t nvm_get_system_entries(CONST CHAR8 *source, UINT32 event_type_mask, INT32 count, CHAR8 **event_buffer)
{
    return 0; // deprecated
}

/*
* @deprecated
*
* Store an event log entry in the system event log
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_store_system_entry (CONST CHAR8 *source,  UINT32 event_type, const CHAR8 *device_uid, CONST CHAR8  *message, ...)
{
    return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* @deprecated
*
* Return action required status for defined DIMM UID
*
* @return The action required status; 1 or 0
*/
NVM_API char nvm_get_action_required(CONST CHAR8* dimm_uid)
{
    return 0; // deprecated
}

/*
* @deprecated
*
* Clear action required status for defined event ID
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_clear_action_required(UINT32 event_id)
{
    return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* @deprecated
*
* Remove all events matching the criteria form the log file
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_remove_events_from_file(UINT32 event_type_mask, CONST CHAR8* dimm_uid, UINT32 event_id)
{
  return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* @deprecated
*
* Get all events matching the criteria form the log file
*
* @return The event_buffer size
*/
NVM_API size_t nvm_get_events_from_file(UINT32 event_type_mask, CONST CHAR8 *dimm_uid, UINT32 event_id, INT32 count, log_entry **pp_log_entry, CHAR8 **event_buffer)
{
    return 0; // deprecated
}

/*
* @deprecated
*
* Get all log entries matching the criteria form the log file
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_log_entries_from_file(UINT32 event_type_mask, CONST CHAR8 *dimm_uid, UINT32 event_id, INT32 count, log_entry **pp_log_entry)
{
    return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* @deprecated
*
* Get event id form the event entry
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_event_id_form_entry(CHAR8 *event_entry, UINT32 *event_id)
{
    return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* @deprecated
*
* Get dimm uid form the event entry
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_uid_form_entry(CHAR8 *event_entry, UINT32 size, CHAR8 *dimm_uid)
{
    return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* @deprecated
*
* Store an event log entry in the system event log, wide character message support
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_store_system_entry_widechar(CONST CHAR16 *source, UINT32 event_type, CONST CHAR16 *device_uid, CONST CHAR16 *message, ...)
{
  return NVM_ERR_API_NOT_SUPPORTED; // deprecated
}

/*
* Sends system event entry to standard output.
*/
void write_system_event_to_stdout(const char *source, const char *message)
{
    NVM_EVENT_MSG ascii_event_message = { 0 };
    CHAR16 w_event_message[sizeof(ascii_event_message)] = { 0 };

    // Prepare string
    os_strcat(ascii_event_message, sizeof(ascii_event_message), source);
    os_strcat(ascii_event_message, sizeof(ascii_event_message), " ");
    os_strcat(ascii_event_message, sizeof(ascii_event_message), message);
    os_strcat(ascii_event_message, sizeof(ascii_event_message), "\n");
    // Convert to the unicode
    AsciiStrToUnicodeStr(ascii_event_message, w_event_message);

    // Send it to standard output
    Print(FORMAT_STR, w_event_message);
}
