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

// Do not parse args marker for the nvm_store_system_entry function
#define DO_NOT_PARSE_ARGS 0xDEADBABE

// SYSTEM_EVENT_CAT_MGMT
#define SYSTEM_EVENT_CAT_MGMT_NUMB_1        1 //call to void logStartError(std::string msg)
#define SYSTEM_EVENT_CAT_MGMT_NUMB_2        2 // 
#define SYSTEM_EVENT_CAT_MGMT_NUMB_3        3 //"ACPI Monitor init complete.  Will be monitoring %d number of dimms"
#define SYSTEM_EVENT_CAT_MGMT_NUMB_4        4 //ACPI_EVENT_MSG_FW_ERROR_CNT_INCREASED "FW error count increased to: %d"
#define SYSTEM_EVENT_CAT_MGMT_NUMB_5        5 //call to void monitor::AcpiMonitor::sendFwErrLogSystemEventEntry(const NVM_UID device_uid, std::string log_type_level, std::string log_details)
#define SYSTEM_EVENT_CAT_MGMT_NUMB_6        6 //"nvm_get_device_count failed with error %d"
#define SYSTEM_EVENT_CAT_MGMT_NUMB_7        7 //"nvm_get_devices failed with error %d"
#define SYSTEM_EVENT_CAT_MGMT_NUMB_8        8 //call to bool monitor::PerformanceMonitor::storeDimmPerformanceData(const std::string &dimmUidStr, struct device_performance &performance)

#define EVENT_CONFIG_CHANGE_300        300 //STR_CONFIG_CHANGE_NEW_GOAL
#define EVENT_CONFIG_CHANGE_301        301 //STR_CONFIG_CHANGE_DELETE_GOAL
#define EVENT_CONFIG_CHANGE_305        305 //STR_CONFIG_SENSOR_SET_CHANGED 
#define EVENT_CONFIG_CHANGE_312        312 //STR_CONFIG_MONITOR_HAS_STARTED 

#define SYSTEM_EVENT_TYPE_CATEGORY_MASK     0xFF000000
#define SYSTEM_EVENT_TYPE_SEVERITY_MASK     0x00F00000
#define SYSTEM_EVENT_TYPE_NUMBER_MASK       0x000FFF00
#define SYSTEM_EVENT_TYPE_SOUT_MASK         0x00000080
#define SYSTEM_EVENT_TYPE_SYSLOG_MASK       0x00000040
#define SYSTEM_EVENT_TYPE_SYSLOG_FILE_MASK  0x00000020
#define SYSTEM_EVENT_TYPE_AR_STATUS_MASK    0x00000010
#define SYSTEM_EVENT_TYPE_AR_EVENT_MASK     0x00000001
#define SYSTEM_EVENT_TYPE_CATEGORY_POS      24
#define SYSTEM_EVENT_TYPE_SEVERITY_POS      20
#define SYSTEM_EVENT_TYPE_NUMBER_POS        8
#define SYSTEM_EVENT_TYPE_SOUT_POS          7
#define SYSTEM_EVENT_TYPE_SYSLOG_POS        6
#define SYSTEM_EVENT_TYPE_SYSLOG_FILE_POS   5
#define SYSTEM_EVENT_TYPE_AR_STATUS_POS     4
#define SYSTEM_EVENT_TYPE_AR_EVENT_POS      0

#define SYSTEM_EVENT_TYPE_CATEGORY_SET(x)      (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_CATEGORY_POS)&SYSTEM_EVENT_TYPE_CATEGORY_MASK)
#define SYSTEM_EVENT_TYPE_SEVERITY_SET(x)      (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_SEVERITY_POS)&SYSTEM_EVENT_TYPE_SEVERITY_MASK)
#define SYSTEM_EVENT_TYPE_NUMBER_SET(x)        (unsigned int)((((unsigned short) x)<<SYSTEM_EVENT_TYPE_NUMBER_POS)&SYSTEM_EVENT_TYPE_NUMBER_MASK)
#define SYSTEM_EVENT_TYPE_SOUT_SET(x)          (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_SOUT_POS)&SYSTEM_EVENT_TYPE_SOUT_MASK)
#define SYSTEM_EVENT_TYPE_SYSLOG_SET(x)        (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_SYSLOG_POS)&SYSTEM_EVENT_TYPE_SYSLOG_MASK)
#define SYSTEM_EVENT_TYPE_SYSLOG_FILE_SET(x)   (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_SYSLOG_FILE_POS)&SYSTEM_EVENT_TYPE_SYSLOG_FILE_MASK)
#define SYSTEM_EVENT_TYPE_AR_STATUS_SET(x)     (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_AR_STATUS_POS)&SYSTEM_EVENT_TYPE_AR_STATUS_MASK)
#define SYSTEM_EVENT_TYPE_AR_EVENT_SET(x)      (unsigned int)((((unsigned char) x)<<SYSTEM_EVENT_TYPE_AR_EVENT_POS)&SYSTEM_EVENT_TYPE_AR_EVENT_MASK)

#define SYSTEM_EVENT_TYPE_CATEGORY_GET(x)      (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_CATEGORY_MASK)>>SYSTEM_EVENT_TYPE_CATEGORY_POS)
#define SYSTEM_EVENT_TYPE_SEVERITY_GET(x)      (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_SEVERITY_MASK)>>SYSTEM_EVENT_TYPE_SEVERITY_POS)
#define SYSTEM_EVENT_TYPE_NUMBER_GET(x)        (unsigned short)((((unsigned int) x)&SYSTEM_EVENT_TYPE_NUMBER_MASK)>>SYSTEM_EVENT_TYPE_NUMBER_POS)
#define SYSTEM_EVENT_TYPE_SOUT_GET(x)          (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_SOUT_MASK)>>SYSTEM_EVENT_TYPE_SOUT_POS)
#define SYSTEM_EVENT_TYPE_SYSLOG_GET(x)        (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_SYSLOG_MASK)>>SYSTEM_EVENT_TYPE_SYSLOG_POS)
#define SYSTEM_EVENT_TYPE_SYSLOG_FILE_GET(x)   (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_SYSLOG_FILE_MASK)>>SYSTEM_EVENT_TYPE_SYSLOG_FILE_POS)
#define SYSTEM_EVENT_TYPE_AR_FILE_GET(x)       (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_AR_STATUS_MASK)>>SYSTEM_EVENT_TYPE_AR_STATUS_POS)
#define SYSTEM_EVENT_TYPE_AR_EVENT_GET(x)      (unsigned char)((((unsigned int) x)&SYSTEM_EVENT_TYPE_AR_EVENT_MASK)>>SYSTEM_EVENT_TYPE_AR_EVENT_POS)

#define SYSTEM_EVENT_CREATE_EVENT_TYPE(cat,sev,num,sout,slog,sfile,arf,ar) (unsigned int)(SYSTEM_EVENT_TYPE_CATEGORY_SET(cat) | \
                                                                                        SYSTEM_EVENT_TYPE_SEVERITY_SET(sev) | \
                                                                                        SYSTEM_EVENT_TYPE_NUMBER_SET(num) | \
                                                                                        SYSTEM_EVENT_TYPE_SOUT_SET(sout) | \
                                                                                        SYSTEM_EVENT_TYPE_SYSLOG_SET(slog) | \
                                                                                        SYSTEM_EVENT_TYPE_SYSLOG_FILE_SET(sfile) | \
                                                                                        SYSTEM_EVENT_TYPE_AR_STATUS_SET(arf) | \
                                                                                        SYSTEM_EVENT_TYPE_AR_EVENT_SET(ar))

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
* Supported log files enum
*/
typedef enum
{
  SYSTEM_LOG_EVENT_FILE = 0,
  SYSTEM_LOG_DEBUG_FILE = 1,
  SYSTEM_LOG_AR_FILE = 2,
  SYSTEM_LOG_FILE_STRUCT_SIZE
} log_file_type;

/*
* Log file names structure, used to store the log file names, needs to be initialized on
* first use
*/
typedef struct _log_file_struct
{
    char name_initialized;
    char value_initialized;
    char ini_entry_name[SYSTEM_LOG_MAX_INI_ENTRY_SIZE];
    char file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN];
    char ini_entry_limit[SYSTEM_LOG_MAX_INI_ENTRY_SIZE];
    unsigned int limit_value;
    unsigned int number_of_lines;
    unsigned int last_event_id;
} log_file_struct;

/*
* Retrive a defined number of event log entries specified by the mask from the system event log
*
* @return The size of the event_buffer
*/
NVM_API size_t nvm_get_system_entries(const char *source, unsigned int event_type_mask, int count, char **event_buffer);

/*
* Store an event log entry in the system event log
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_store_system_entry(const char *source, unsigned int event_type, const char *device_uid, const char  *message, ...);

/*
* Store an event log entry in the system event log, wide character message support
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_store_system_entry_widechar(const wchar_t *source, unsigned int event_type, const wchar_t *device_uid, const wchar_t  *message, ...);

/*
* Return action required status for defined DIMM UID
*
* @return The action required status; 1 or 0
*/
NVM_API char nvm_get_action_required(const char* dimm_uid);

/*
* Clear action required status for defined event ID
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_clear_action_required(unsigned int event_id);

/*
* Remove all events matching the criteria form the log file
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_remove_events_from_file(unsigned int event_type_mask, const char *dimm_uid, unsigned int event_id);

/*
* Get all events matching the criteria form the log file
*
* @return The event_buffer size
*/
NVM_API size_t nvm_get_events_from_file(unsigned int event_type_mask, const char *dimm_uid, unsigned int event_id, int count, log_entry **pp_log_entry, char **event_buffer);

/*
* Get all log entries matching the criteria form the log file
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_log_entries_from_file(unsigned int event_type_mask, const char *dimm_uid, unsigned int event_id, int count, log_entry **pp_log_entry);

/*
* Get event id form the event entry
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_event_id_form_entry(char *event_entry, unsigned int *event_id);

/*
* Get dimm uid form the event entry
*
* @return NvmStatusCode
*/
NVM_API NvmStatusCode nvm_get_uid_form_entry(char *event_entry, unsigned int size, char *dimm_uid);

#ifdef __cplusplus
}
#endif

#endif  /* _EVENT_H_ */
