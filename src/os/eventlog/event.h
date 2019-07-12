/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the definition of the common eventing functionality.
 */

#ifndef	_EVENT_H_
#define	_EVENT_H_

#include <os_types.h>
#include <export_api.h>
#include <NvmSharedDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
* The list returned by the nvm_get_events_form_file function.
* Each entry contains the event type value and an offset to the message in the event buffer
*/
typedef struct _log_entry
{
    struct _log_entry *p_next;
    struct _log_entry *p_last;
    unsigned int event_type;
    unsigned long long message_offset;
} log_entry;

/*
* @deprecated
*
* Retrive a defined number of event log entries specified by the mask from the system event log
*
* @return The size of the event_buffer
*/
NVM_API size_t nvm_get_system_entries(const char *source, unsigned int event_type_mask, int count, char **event_buffer);

/*
* @deprecated
*
* Store an event log entry in the system event log
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_store_system_entry(const char *source, unsigned int event_type, const char *device_uid, const char  *message, ...);

/*
* @deprecated
*
* Store an event log entry in the system event log, wide character message support
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_store_system_entry_widechar(const wchar_t *source, unsigned int event_type, const wchar_t *device_uid, const wchar_t  *message, ...);

/*
* @deprecated
*
* Return action required status for defined DIMM UID
*
* @return The action required status; 1 or 0
*/
NVM_API char nvm_get_action_required(const char* dimm_uid);

/*
* @deprecated
*
* Clear action required status for defined event ID
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_clear_action_required(unsigned int event_id);

/*
* @deprecated
*
* Remove all events matching the criteria form the log file
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_remove_events_from_file(unsigned int event_type_mask, const char *dimm_uid, unsigned int event_id);

/*
* @deprecated
*
* Get all events matching the criteria form the log file
*
* @return The event_buffer size
*/
NVM_API size_t nvm_get_events_from_file(unsigned int event_type_mask, const char *dimm_uid, unsigned int event_id, int count, log_entry **pp_log_entry, char **event_buffer);

/*
* @deprecated
*
* Get all log entries matching the criteria form the log file
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_log_entries_from_file(unsigned int event_type_mask, const char *dimm_uid, unsigned int event_id, int count, log_entry **pp_log_entry);

/*
* @deprecated
*
* Get event id form the event entry
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_event_id_form_entry(char *event_entry, unsigned int *event_id);

/*
* @deprecated
*
* Get dimm uid form the event entry
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_uid_form_entry(char *event_entry, unsigned int size, char *dimm_uid);

void write_system_event_to_stdout(const char *source, const char *message);

#ifdef __cplusplus
}
#endif

#endif  /* _EVENT_H_ */
