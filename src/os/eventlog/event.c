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

// Static values and definitions used locally only
#define MAX_EVENT_TYPE_STRING_LENGTH 15

// Common strings and formatting string
#define EVENT_MESSAGE_UID_PREFIX_CHAR ':' // Last char of the EVENT_MESSAGE_UID_PREFIX string
#define EVENT_MESSAGE_UID_PREFIX " UID:"
#define EVENT_MESSAGE_CONTROL_CHARACTERS "\n<>\t"
#define EVENT_MESSAGE_CONTROL_CHAR_START '>'
#define EVENT_MESSAGE_CONTROL_CHAR_STOP  '<'
#define ACTION_REQUIRED_FILE_PARSING_STRING "%s.ar"

/*
* Log file structure local definiton, used as a cache so the file name
* needs to be read only on first used
*/
log_file_struct g_log_file_table[SYSTEM_LOG_FILE_STRUCT_SIZE] =
{
    {FALSE, FALSE, SYSTEM_LOG_EVENT_FILE_NAME, "", SYSTEM_LOG_EVENT_LIMIT, SYSTEM_EVENT_NOT_APPLICABLE, 0, 0},
    {FALSE, FALSE, SYSTEM_LOG_DEBUG_FILE_NAME, "", SYSTEM_LOG_DEBUG_LIMIT, SYSTEM_EVENT_NOT_APPLICABLE, 0, 0}
};
#define PTR_LAST_EVENT_ID(log_type) &g_log_file_table[log_type].last_event_id
#define PTR_FILE_SIZE(log_type)     &g_log_file_table[log_type].number_of_lines
#define PTR_MAX_FILE_SIZE(log_type) &g_log_file_table[log_type].limit_value
#define PTR_VALUES_INIT(log_type)   &g_log_file_table[log_type].value_initialized

/*
* An enumeration set describing system event types represented by strings
* It is used to trasform the system_event_type enum value to the string
*/
const char *entry_type_string_table[] = { "Information", "Warning", "Error", "Debug" };

/*
* Find and return the event_type value form the event entry stored in the log
* Returns 0 in case of success
*/
static UINT32 get_event_type_form_event_entry(CHAR8 *event_message);

/*
* Function returns event id form the event message string
*/
static UINT32 get_event_id_form_entry(CHAR8* event_message);

/*
* Strip out the end of line chars form the string
*/
static void remove_control_characters(CHAR8 *string)
{
    while (string != NULL)
    {
        string = strpbrk(string, EVENT_MESSAGE_CONTROL_CHARACTERS);
        if (string != NULL)
        {
            *string = ' ';
            string++;
        }
    }
}

/*
* Get the file name form the ini file
* If the g_log_file_table is not initialized funciton initializes file limits based on 
* the current ini file configuration
* The return code other than SUCCESS indicates the file is not configured or there
* the buffer is too small which emans the file cannot be used
*/
static EFI_STATUS get_the_system_log_file_name(log_file_type file, UINTN file_size, CHAR8 *file_name)
{
    EFI_STATUS efi_status = EFI_SUCCESS;
    EFI_GUID guid = { 0 };

    if (FALSE == g_log_file_table[file].name_initialized) {
        // The system log file name not configured yet, check the preferences
        efi_status = preferences_get_string_ascii(g_log_file_table[file].ini_entry_name, guid, SYSTEM_LOG_FILE_NAME_MAX_LEN, g_log_file_table[file].file_name);
        if (EFI_SUCCESS == efi_status) {
            g_log_file_table[file].name_initialized = TRUE;
        }
    }
    if (FALSE == g_log_file_table[file].value_initialized) {
        // Initialize the file limits
        // Don't worry about the returns status, the limit_value doesn't have to be configured
        UINTN limit_value_size = sizeof(g_log_file_table[file].limit_value);
        preferences_get_var_ascii(g_log_file_table[file].ini_entry_limit, guid, (void *)&g_log_file_table[file].limit_value, &limit_value_size);
        g_log_file_table[file].value_initialized = TRUE;
    }
    if (EFI_SUCCESS == efi_status) {
        if (file_size < sizeof(g_log_file_table[file].file_name))
            efi_status = EFI_BUFFER_TOO_SMALL;
        else {
            // the name has been read successfuly from the ini file
            memcpy(file_name, g_log_file_table[file].file_name, SYSTEM_LOG_FILE_NAME_MAX_LEN);
        }
    }
    return efi_status;
}

/*
* Convert entry type string to the type mask
* In case of error function returns 0 mask
*/
static CHAR8 get_type_value(CHAR8 *type_string)
{
    int index;

    for (index = 0; index < sizeof(*entry_type_string_table); index++)
    {
        if (strcmp(type_string, entry_type_string_table[index]) == 0)
            return SYSTEM_EVENT_TO_MASK(index);
    }

    return 0;
}

/*
* Function returns action required status stored in the action required file
*/
static char get_action_req_state_form_file(CHAR8 *device_uid)
{
    char ar_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN];
    FILE     *h_file = NULL;
    char action_req = 0;
    NVM_EVENT_MSG event_type_str = { 0 };
    UINTN read_event_type = 0;

    // The action required file configured
    sprintf(ar_file_name, ACTION_REQUIRED_FILE_PARSING_STRING, device_uid);
    // Check if file exists
    h_file = fopen(ar_file_name, "r");
    if (NULL != h_file)
    {
        // Read a first line and check if it makes any sense
        if (fgets(event_type_str, sizeof(event_type_str), h_file) != NULL)
        {
            read_event_type = AsciiStrHexToUintn(event_type_str);
            if ((read_event_type & ~(SYSTEM_EVENT_TYPE_CATEGORY_MASK | SYSTEM_EVENT_TYPE_SEVERITY_MASK |
                SYSTEM_EVENT_TYPE_NUMBER_MASK | SYSTEM_EVENT_TYPE_SOUT_MASK | SYSTEM_EVENT_TYPE_SYSLOG_MASK |
                SYSTEM_EVENT_TYPE_SYSLOG_FILE_MASK)) == (SYSTEM_EVENT_TYPE_AR_STATUS_MASK | SYSTEM_EVENT_TYPE_AR_EVENT_MASK))
            {
                action_req = 1;
            }
        }
    }

    return action_req;
}

/*
* Get the UID form the event entry line
*/
static size_t get_unified_id_form_event_entry(char *event_entry, size_t uid_buff_size, char *uid_buff)
{
    char * p_uid_string = NULL;
    size_t index = 0;

    if (NULL != event_entry)
    {
        // Locate last occurance of special character
        p_uid_string = strrchr(event_entry, EVENT_MESSAGE_UID_PREFIX_CHAR);
        // Skip to the first UID char
        p_uid_string++;
        // Copy the UID string, we have to remove the end of line char
        for (index = 0; (*p_uid_string != '\n') && (*p_uid_string != 0) && (uid_buff_size > index); p_uid_string++, index++)
        {
            uid_buff[index] = *p_uid_string;
        }
        if (uid_buff_size > index)
        {
            // There is a room to terminate the string
            uid_buff[index] = 0;
        }
    }
    return index;
}

/*
* Get the current action required state for the DIMM specified int the string
* entry
*/
static char get_action_required_status(char *uid_string)
{
    char log_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN] = { 0 };
    UINTN index;

    if (NULL == uid_string)
    {
        return 0;
    }
    // Copy the UID string, we have to remove the end of line char
    for (index = 0; (uid_string[index] != '\n') && (index < AsciiStrLen(uid_string)); index++)
    {
        log_file_name[index] = uid_string[index];
    }
    return get_action_req_state_form_file(log_file_name);
}

/*
* Stores entry in the buffer, it extends the buffer size if it is required
*/
static void store_entry_in_buffer(char *event_entry, size_t *p_event_buff_size, CHAR8 **event_buffer)
{
    size_t end_of_event_buffer = *p_event_buff_size;
    char *temp_string_ptr = NULL;
    size_t str_size;

    // Find the control char and estimate the fist section size
    temp_string_ptr = strchr(event_entry, EVENT_MESSAGE_CONTROL_CHAR_START);
    if (temp_string_ptr != NULL)
    {
        str_size = (size_t)(temp_string_ptr - event_entry);
        str_size--; // Remove last tabulator
        // Find the second control char and skip to the next char
        temp_string_ptr = strchr(event_entry, EVENT_MESSAGE_CONTROL_CHAR_STOP);
        if (NULL == temp_string_ptr)
            return;  // Improper entry format
        temp_string_ptr++;
        // Increase buffer size
        *p_event_buff_size += str_size + AsciiStrLen(temp_string_ptr) + 1; // + new line marker
        *event_buffer = realloc(*event_buffer, *p_event_buff_size);
        if (NULL != *event_buffer) {
            ((char*)*event_buffer)[end_of_event_buffer] = 0;
            // Coppy strings to the buffer
            s_strncat(*event_buffer, *p_event_buff_size, event_entry, str_size);
            s_strcat(*event_buffer, *p_event_buff_size, temp_string_ptr);
        }
    }
    else
    {
        // No cotrol characters in the entry
        // Increase buffer size
        *p_event_buff_size += AsciiStrLen(event_entry) + 1; // + new line marker
        *event_buffer = realloc(*event_buffer, *p_event_buff_size);
        if (NULL != *event_buffer) {
            ((char*)*event_buffer)[end_of_event_buffer] = 0;
            // Coppy strings to the buffer
            s_strcat(*event_buffer, *p_event_buff_size, event_entry);
        }
    }
}

static void store_log_entry(CHAR8 *event_message, UINTN offset, log_entry **pp_log_entry)
{
    log_entry *p_current = NULL;

    // Allocate new entry
    p_current = AllocateZeroPool(sizeof(log_entry));
    if (NULL != p_current) {
        // Fill with data
        p_current->event_type = get_event_type_form_event_entry(event_message);
        p_current->message_offset = offset;
        // Add it to the log entry list
        if (NULL == *pp_log_entry)
        {
            // Allocate first entry
            *pp_log_entry = p_current;
        }
        else
        {
            ((log_entry *)*pp_log_entry)->p_last->p_next = p_current;
        }
        ((log_entry *)*pp_log_entry)->p_last = p_current;
    }
}

/*
* The function finds the last line in the stream/file
*/
static long int find_last_line_in_file(FILE * stream)
{
    long int pos = 0;

    // Find the end of the file
    fseek(stream, pos, SEEK_END);
    // Current possition in the stream
    pos = ftell(stream);
    // Jump over the previous EOL char
    pos -= 2;
    // Find the previous line
    while (fgetc(stream) != '\n')
    {
        if (fseek(stream, --pos, SEEK_SET) != 0)
            return 0;
    }
    return pos;
}

/*
* Get the string form the stream and move pointer to the string before - reversed read order
*/
static char* fgetsrev(char* str, int num, FILE * stream)
{
    static BOOLEAN b_o_f = TRUE;
    char * rc = NULL;
    long int pos = ftell(stream); // current possition in the stream, pointer cannot be set to 0
 
    if (pos > 0)
        b_o_f = FALSE;
    // Check if we reached begin of file
    if (b_o_f)
        return NULL;
    // Get the line
    rc = fgets(str, num, stream);
    if (NULL == rc)
        return rc;
    // Stor information about reached begin of file
    if (pos == 0)
    {
        b_o_f = TRUE;
        fseek(stream, pos, SEEK_SET); // Reset the pointer after last gets
    }
    // Jump over the previous EOL char
    pos -= 2;
    // Find the previous line
    while ((pos > 0) && (fgetc(stream) != '\n'))
    {
        if (fseek(stream, --pos, SEEK_SET) != 0)
            return NULL;
    }
    return str;
}

/*
* Find and return the event_type value form the event entry stored in the log
* Returns 0 in case of success
*/
static UINT32 get_event_type_form_event_entry(CHAR8 *event_message)
{
    char *p_ctrl_str_start = NULL;
    char *p_ctrl_str_stop = NULL;
    size_t str_size;
    char event_type_string[MAX_EVENT_TYPE_STRING_LENGTH] = { 0 };
    UINTN event_type_value = 0;

    // Find the control char and estimate the fist section size
    p_ctrl_str_start = strchr(event_message, EVENT_MESSAGE_CONTROL_CHAR_START);
    // Skip the control char
    p_ctrl_str_start++;
    if (p_ctrl_str_start != NULL)
    {
        p_ctrl_str_stop = strchr(event_message, EVENT_MESSAGE_CONTROL_CHAR_STOP);
        // Calculate event type value size
        str_size = (size_t)(p_ctrl_str_stop - p_ctrl_str_start);
        // Copy the event type string only
        AsciiStrnCpy(event_type_string, p_ctrl_str_start, str_size);
        // Conver it to the value
        event_type_value = AsciiStrHexToUintn(event_type_string);
    }
    return (UINT32)event_type_value;
}

/*
* Function validates the entry type with the requested mask and returns not_matching flag value if they match.
* The not_matching flag value defines the logic on the check, if it is set as FALSE, function returns FALSE
* everytime it finds a match
*/
static BOOLEAN check_skip_entry_status_for_type(BOOLEAN not_matching, CHAR8 type_mask, CHAR8 *event_message)
{
    char time_stamp[MAX_TIMESTAMP_LEN];
    UINT32 event_id = 0;
    char type_string[MAX_EVENT_TYPE_STRING_LENGTH];
    CHAR8 tmp_type_mask;
    BOOLEAN skip_entry = not_matching;

    if (SYSTEM_EVENT_NOT_APPLICABLE != type_mask)
    {
        // Get event type and compare with requested event type mask
        // The single line format is "%s %s %d %s %d %s\n" time_stamp date, time_stamp time, event_id, event_type, action_req, event_message
        sscanf(event_message, "%s %s\t%d\t%s", time_stamp, time_stamp, &event_id, type_string);
        // Convert the event type string to the event type value
        tmp_type_mask = get_type_value(type_string);
        if (tmp_type_mask & type_mask)
        {
            skip_entry = not_matching;
        }
        else
        {
            skip_entry = !not_matching;
        }
    }

    return skip_entry;
}

/*
* Function validates the entry category with the requested mask and returns not_matching flag value if they match.
* The not_matching flag value defines the logic on the check, if it is set as FALSE, function returns FALSE
* everytime it finds a match
*/
static BOOLEAN check_skip_entry_status_for_event_category(BOOLEAN not_matching, CHAR8 cat_mask, CHAR8 *event_message)
{
    BOOLEAN skip_entry = not_matching;
    UINT32 event_type = 0;
    CHAR8 event_cat_mask = 0;

    if (SYSTEM_EVENT_NOT_APPLICABLE != cat_mask)
    {
        // Get the event type value
        event_type = get_event_type_form_event_entry(event_message);
        // Get the category form it
        event_cat_mask = SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_CATEGORY_GET(event_type));
        if (event_cat_mask & cat_mask)
        {
            skip_entry = not_matching;
        }
        else
        {
            skip_entry = !not_matching;
        }
    }

    return skip_entry;
}

/*
* Function validates the entry category with the requested mask and returns not_matching flag if they match.
* The not_matching flag value defines the logic on the check, if it is set as FALSE, function returns FALSE
* everytime it finds a match
*/
static BOOLEAN check_skip_entry_status_for_event_actionreq_set(BOOLEAN not_matching, CHAR8 ar_mask, CHAR8 *event_message)
{
    BOOLEAN skip_entry = not_matching;
    UINT32 event_type = 0;
    CHAR8 event_cat = 0;
    char log_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN] = { 0 };
    FILE     *h_file = NULL;
    NVM_EVENT_MSG event_type_str = { 0 };
    UINT32 read_event_type = 0;

    // Set the starting conditions for AR set and not set
    if (ar_mask == 0)
        skip_entry = not_matching;
    else
        skip_entry = !not_matching;
    // Get the event type value
    event_type = get_event_type_form_event_entry(event_message);
    // Get the UID file name
    get_unified_id_form_event_entry(event_message, sizeof(log_file_name), log_file_name);
    // The action required file configured
    snprintf(log_file_name, SYSTEM_LOG_FILE_NAME_MAX_LEN, ACTION_REQUIRED_FILE_PARSING_STRING, log_file_name);
    // Event type cannot equal 0, it is stored in the log file means at least one bit needs to be set
    h_file = fopen(log_file_name, "r+");
    if (NULL != h_file)
    {
        // Remove the event type from the action required file
        while (fgets(event_type_str, sizeof(event_type_str), h_file) != NULL)
        {
            read_event_type = (UINT32)AsciiStrHexToUintn(event_type_str);
            if (read_event_type == event_type)
            {
                event_cat = SYSTEM_EVENT_TYPE_AR_EVENT_GET(event_type);
                if (event_cat == ar_mask)
                {
                    // We found the event in the action required file
                    skip_entry = not_matching;
                }
                else
                {
                    // Skip that entry
                    skip_entry = !not_matching;
                }
                break;
            }
        }
        fclose(h_file);
    }

    return skip_entry;
}

/*
* Function validates the entry UID with the requested one and returns not_matching flag if they match.
* The not_matching flag value defines the logic on the check, if it is set as FALSE, function returns FALSE
* everytime it finds a match
*/
static BOOLEAN check_skip_entry_status_for_dimm_id(BOOLEAN not_matching, CONST CHAR8* dimm_uid, CHAR8 *event_message)
{
    BOOLEAN skip_entry = not_matching;
    char uid_buffer[MAX_DIMM_UID_LENGTH] = { 0 };

    // Get UID form the event message
    get_unified_id_form_event_entry(event_message, MAX_DIMM_UID_LENGTH, uid_buffer);
    if (AsciiStrCmp(dimm_uid, uid_buffer) == 0)
    {
        skip_entry = not_matching;
    }
    else
    {
        skip_entry = !not_matching;
    }

    return skip_entry;
}

/*
* Function validates the entry id with the requested one and returns not_matching value if they match.
* The not_matching flag value defines the logic on the check, if it is set as FALSE, function returns FALSE
* everytime it finds a match
*/
static BOOLEAN check_skip_entry_status_for_event_id(BOOLEAN not_matching, UINT32 event_id, CHAR8 *event_message)
{
    BOOLEAN skip_entry = not_matching;
    UINT32 temp_event_id = 0;

    // Get event entry for event id
    temp_event_id = get_event_id_form_entry(event_message);
    if (temp_event_id == event_id)
    {
        skip_entry = not_matching;
    }
    else
    {
        skip_entry = !not_matching;
    }

    return skip_entry;
}

/*
* Function returns event id form the event message string
*/
static UINT32 get_event_id_form_entry(CHAR8* event_message)
{
    char time_stamp[MAX_TIMESTAMP_LEN];
    UINT32 event_id = 0;

    sscanf(event_message, "%s\t%s\t%d", time_stamp, time_stamp, &event_id);
    return event_id;
}
/*
* Get the requested events form the system log file and prepare the string.
* Function returns the event_buffer size.
* The negative value indicates either the system log file access problem
* or the system log file is not configured at all.
* Function can return those entries in reversed order in case the
* reversed bool flag is being set.
* The not_matching bool flag controls the logic, if it is TRUE, only not
* events which ARE NOT maching criteria are going to be returned, if FALSE
* all events which ARE matching criteria are goint to be returned.
*/
static int get_system_events_from_file(BOOLEAN reversed, BOOLEAN not_matching, UINT32 event_type_mask, INT32 count, CONST CHAR8* dimm_uid, UINT32 event_id, log_entry **pp_log_entry, CHAR8 **event_buffer)
{
    EFI_STATUS efi_status = EFI_NOT_FOUND;
    char log_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN];
    FILE     *h_file=NULL;
    NVM_EVENT_MSG event_message = { 0 };
    BOOLEAN skip_entry = FALSE;
    INT32 event_count = 0;
    size_t event_buff_size = 0;
    char *ret_ptr = NULL;

    // Get the log file name if it is necessary
    if (SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type_mask) & SYSTEM_EVENT_DEBUG_MASK)
        efi_status = get_the_system_log_file_name(SYSTEM_LOG_DEBUG_FILE, sizeof(log_file_name), log_file_name);
    else
        efi_status = get_the_system_log_file_name(SYSTEM_LOG_EVENT_FILE, sizeof(log_file_name), log_file_name);
    if (EFI_SUCCESS != efi_status)
        return -1;

    // The system event log file configured
    h_file = fopen(log_file_name, "r+");
    if (NULL == h_file)
        return 0;
    if (TRUE == reversed)
    {
        // Set the read pointer to the last line
        find_last_line_in_file(h_file);
        // Find the last entry
        ret_ptr= fgetsrev(event_message, sizeof(event_message), h_file);
    }
    else
    {
        ret_ptr = fgets(event_message, sizeof(event_message), h_file);
    }
    while (ret_ptr != NULL)
    {
        skip_entry = FALSE;
        // Check event entry filters
        if (event_type_mask & SYSTEM_EVENT_TYPE_CATEGORY_MASK)
        {
            skip_entry |= check_skip_entry_status_for_event_category(not_matching, SYSTEM_EVENT_TYPE_CATEGORY_GET(event_type_mask), event_message);
        }
        if (event_type_mask & SYSTEM_EVENT_TYPE_SEVERITY_MASK)
        {
            skip_entry |= check_skip_entry_status_for_type(not_matching, SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type_mask), event_message);
        }
        if (event_type_mask & SYSTEM_EVENT_TYPE_AR_STATUS_MASK)
        {
            skip_entry |= check_skip_entry_status_for_event_actionreq_set(not_matching, SYSTEM_EVENT_TYPE_AR_EVENT_GET(event_type_mask), event_message);
        }
        if (dimm_uid != NULL)
        {
            skip_entry |= check_skip_entry_status_for_dimm_id(not_matching, dimm_uid, event_message);
        }
        if (event_id != SYSTEM_EVENT_NOT_APPLICABLE)
        {
            skip_entry |= check_skip_entry_status_for_event_id(not_matching, event_id, event_message);
        }

        if (FALSE == skip_entry)
        {
            if (pp_log_entry != NULL)
            {
                // Add log entry to the list
                store_log_entry(event_message, event_buff_size, pp_log_entry);
            }
            if (event_buffer != NULL)
            {
                // Store entry in the buffer as a string
                store_entry_in_buffer(event_message, &event_buff_size, event_buffer);
            }

            if (SYSTEM_EVENT_NOT_APPLICABLE != count)
            {
                event_count++;
                if (event_count >= count)
                {
                    // We have reached number of events we want to collect
                    break;
                }
            }
        }
        // Get next entry
        if (TRUE == reversed)
        {
            ret_ptr = fgetsrev(event_message, sizeof(event_message), h_file);
        }
        else
        {
            ret_ptr = fgets(event_message, sizeof(event_message), h_file);
        }
    }
    // Close the file
    fclose(h_file);

    return (int) event_buff_size;
}

/*
* Check the file size and allocate the buffer for it
*/
static inline void *allocate_buffer_for_file(FILE *h_file, size_t *buff_size)
{
    UINTN file_size = 0;
    void *file_buffer = NULL;

    // Check the file size
    fseek(h_file, 0, SEEK_END);
    file_size = ftell(h_file);
    fseek(h_file, 0, SEEK_SET);
    if (file_size > 0) {
        file_buffer = AllocateZeroPool(file_size);
    }
    if (NULL != buff_size) {
        *buff_size = file_size;
    }
    return file_buffer;
}

/*
* Logs a message in the system event log file.
*/
static void log_system_event_to_file(UINT32 event_type, const char *event_message)
{
    char log_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN];
    EFI_STATUS efi_status = EFI_SUCCESS;
    FILE  *h_file = NULL;
    NVM_EVENT_MSG last_event_message = { 0 };
    char time_stamp[MAX_TIMESTAMP_LEN] = { 0 };
    UINT32 *p_event_id = NULL;
    UINT32  *p_max_log_level = NULL;
    UINT32 *p_number_of_lines = NULL;
    time_t raw_time_stamp;
    char* p_file_buffer = NULL;
    size_t file_buffer_size = 0;

    // Get the log file name if it is necessary
    if (SYSTEM_EVENT_TYPE_DEBUG == SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type)) {
        efi_status = get_the_system_log_file_name(SYSTEM_LOG_DEBUG_FILE, sizeof(log_file_name), log_file_name);
        p_event_id = PTR_LAST_EVENT_ID(SYSTEM_LOG_DEBUG_FILE);
        p_max_log_level = PTR_MAX_FILE_SIZE(SYSTEM_LOG_DEBUG_FILE);
        p_number_of_lines = PTR_FILE_SIZE(SYSTEM_LOG_DEBUG_FILE);
    }
    else {
        efi_status = get_the_system_log_file_name(SYSTEM_LOG_EVENT_FILE, sizeof(log_file_name), log_file_name);
        p_event_id = PTR_LAST_EVENT_ID(SYSTEM_LOG_EVENT_FILE);
        p_max_log_level = PTR_MAX_FILE_SIZE(SYSTEM_LOG_EVENT_FILE);
        p_number_of_lines = PTR_FILE_SIZE(SYSTEM_LOG_EVENT_FILE);
    }

    if (EFI_SUCCESS == efi_status)
    {
        // The system event log file configured
        h_file = fopen(log_file_name, "a+");
        if (NULL != h_file) {
            if (*p_event_id == 0) {
                // Find the last entry
                while (fgets(last_event_message, sizeof(last_event_message), h_file) != NULL) {
                    // Calculate current file size
                    *p_number_of_lines += 1;
                }
                *p_event_id = get_event_id_form_entry(last_event_message);
            }
            // Store the new entry in the file
            *p_event_id += 1;
            *p_number_of_lines += 1;
            time(&raw_time_stamp);
			      struct tm *local_time_stamp = localtime(&raw_time_stamp);
			      if (local_time_stamp) {
				        strftime(time_stamp, sizeof(time_stamp), "%m/%d/%Y %T ", local_time_stamp);
			      }
            // Check the file limit and remove one entry if it is necessary
            if ((*p_max_log_level != SYSTEM_EVENT_NOT_APPLICABLE ) && (*p_number_of_lines > *p_max_log_level)) {
                p_file_buffer = allocate_buffer_for_file(h_file, &file_buffer_size);
                if (NULL != p_file_buffer) {
                    // Copy the file to the buffer
                    while (fgets(last_event_message, sizeof(last_event_message), h_file) != NULL)
                    {
                        // Remove all entries above the limit
                        if (*p_number_of_lines > *p_max_log_level) {
                            // One entry has been removed
                            *p_number_of_lines -= 1;
                        }
                        else {
                            // Copyt all other entries to the buffer
                            strcat(p_file_buffer, last_event_message);
                        }
                    }
                    // Reopen and truncate the file
                    h_file = freopen(log_file_name, "w", h_file);
                    if (NULL != h_file) {
                        // Find the first end of line char and move to the next line
                        fprintf(h_file, "%s", p_file_buffer);
                    }
                    free(p_file_buffer);
                    // Reopen and truncate the file in append mode again
                    h_file = freopen(log_file_name, "a+", h_file);
                }
            }
            // Append the new entry
            if (NULL != h_file) {
                fprintf(h_file, "%s\t%d\t%s\t%d\t%c%08x%c\t%s\n", time_stamp, *p_event_id, entry_type_string_table[SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type)],
                    SYSTEM_EVENT_TYPE_AR_EVENT_GET(event_type), EVENT_MESSAGE_CONTROL_CHAR_START, event_type, EVENT_MESSAGE_CONTROL_CHAR_STOP, event_message);
            }
            // Close the file
            fclose(h_file);
        }
    }
}

/*
* Sends system event entry to standard output.
*/
void write_system_event_to_stdout(enum system_event_type type, const char *source, const char *message)
{
    NVM_EVENT_MSG ascii_event_message = { 0 };
    CHAR16 w_event_message[sizeof(ascii_event_message)] = { 0 };
 
    // Prepare string
    s_strcat(ascii_event_message, sizeof(ascii_event_message), source);
    s_strcat(ascii_event_message, sizeof(ascii_event_message), " ");
    s_strcat(ascii_event_message, sizeof(ascii_event_message), entry_type_string_table[type]);
    s_strcat(ascii_event_message, sizeof(ascii_event_message), " ");
    s_strcat(ascii_event_message, sizeof(ascii_event_message), message);
    s_strcat(ascii_event_message, sizeof(ascii_event_message), "\n");
    // Convert to the unicode
    AsciiStrToUnicodeStr(ascii_event_message, w_event_message);

    // Send it to standard output
    Print(FORMAT_STR, w_event_message);
}

/*
* Logs a message in the system event log file.
*/
static void add_event_to_action_req_file(UINT32 type, const CHAR8 *device_uid)
{
    char ar_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN];
    FILE     *h_file = NULL;
    NVM_EVENT_MSG event_type_str = { 0 };
    char* new_file_buffer;
    UINT32 read_event_type = 0;

    // The action required file configured
    sprintf(ar_file_name, ACTION_REQUIRED_FILE_PARSING_STRING, device_uid);

    if (SYSTEM_EVENT_TYPE_AR_EVENT_GET(type))
    {
		    if (NULL == (h_file = fopen(ar_file_name, "a")))
		    {
			    return;
		    }
        // Create the event type string
        sprintf(event_type_str, "%08x\n", type);
        // Add the event type to the action required file
        fputs(event_type_str, h_file);
        fclose(h_file);
    }
    else
    {
        h_file = fopen(ar_file_name, "r+");
        if (NULL != h_file)
        {
            new_file_buffer = allocate_buffer_for_file(h_file, NULL);
            if (NULL != new_file_buffer) {
                // Remove the event type from the action required file
                while (fgets(event_type_str, sizeof(event_type_str), h_file) != NULL)
                {
                    read_event_type = (UINT32)AsciiStrHexToUintn(event_type_str);
                    if (((read_event_type ^ type) & (SYSTEM_EVENT_TYPE_CATEGORY_MASK | SYSTEM_EVENT_TYPE_NUMBER_MASK | SYSTEM_EVENT_TYPE_AR_STATUS_MASK)) != 0)
                    {
                        strcat(new_file_buffer, event_type_str);
                    }
                }
		            FILE *file = freopen(ar_file_name, "w", h_file);
		            if (file == NULL)
			            return;
                fprintf(h_file, "%s", new_file_buffer);
                free(new_file_buffer);
            }
            fclose(h_file);
        }
    }
}

/*
* Logs a message in the operating system event log.
*/
void log_system_event(enum system_event_type type, const char *source, const char *message);

/*
* Retrive an event log entry from the system event log
*/
int get_system_events(char event_type_mask, int count, const char *source, char **event_buffer);

/*
* Retrive a defined number of event log entries specified by the mask from the system event log
*/
NVM_API int nvm_get_system_entries(CONST CHAR8 *source, UINT32 event_type_mask, INT32 count, CHAR8 **event_buffer)
{
    int ret_value;

    if ((event_buffer == NULL) || (*event_buffer != NULL))
    {
        // NULL pointer required
        return -1;
    }

    ret_value = get_system_events_from_file(TRUE, FALSE, event_type_mask, count, NULL, SYSTEM_EVENT_NOT_APPLICABLE, NULL, event_buffer);
    if (ret_value < 0)
    {
        // In case of the system file problem try to get those entries form
        // the system event log
        ret_value = get_system_events(SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type_mask), count, source, event_buffer);
    }
    return ret_value;
}

/*
* Store an event log entry in the system event log
*/
NVM_API int nvm_store_system_entry (CONST CHAR8 *source,  UINT32 event_type, const CHAR8 *device_uid, CONST CHAR8  *message, ...)
{
    VA_LIST args;
    NVM_EVENT_MSG event_message = { 0 };
    UINT32 size = sizeof(event_message);
    int ret_code;

    VA_START(args, message);
    ret_code = (int) AsciiVSPrint(event_message, size, message, args);
    VA_END(args); // Cleans up the list
    if (ret_code > -1 && ret_code < (int)size)
    {
        // Strip out all \n characters for the string
        remove_control_characters(event_message);
        if (device_uid != NULL)
        {
            s_strcat(event_message, sizeof(event_message), EVENT_MESSAGE_UID_PREFIX);
            s_strcat(event_message, sizeof(event_message), device_uid);
        }
        if (SYSTEM_EVENT_TYPE_SYSLOG_FILE_GET(event_type))
        {
            log_system_event_to_file(event_type, event_message);
        }
        if (SYSTEM_EVENT_TYPE_SYSLOG_GET(event_type))
        {
            log_system_event(SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type), source, event_message);
        }
        if (SYSTEM_EVENT_TYPE_SOUT_GET(event_type))
        {
            write_system_event_to_stdout(SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type), source, event_message);
        }
        if ((device_uid != NULL) & (SYSTEM_EVENT_TYPE_AR_FILE_GET(event_type)))
        {
            add_event_to_action_req_file(event_type, device_uid);
        }
    }

    return ret_code;
}

/*
* Return action required status for defined DIMM UID
*/
NVM_API char nvm_get_action_required(CONST CHAR8* dimm_uid)
{
    return get_action_required_status((char *) dimm_uid);
}

/*
* Clear action required status for defined event ID
*/
NVM_API int nvm_clear_action_required(UINT32 event_id)
{
    char log_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN] = { 0 };
    EFI_STATUS efi_status = EFI_SUCCESS;
    FILE     *h_file = NULL;
    NVM_EVENT_MSG event_message = { 0 };
    UINT32 temp_event_id = 0;
    UINT32 event_type = 0;
    NVM_EVENT_MSG event_type_str = { 0 };
    char* new_file_buffer;
    UINT32 read_event_type = 0;

    // Get the log file name if it is necessary
    efi_status = get_the_system_log_file_name(SYSTEM_LOG_EVENT_FILE, sizeof(log_file_name), log_file_name);
    if (EFI_SUCCESS == efi_status)
    {
        // The system event log file configured
        h_file = fopen(log_file_name, "r");
        if (NULL != h_file)
        {
            // Find the last entry
            while (fgets(event_message, sizeof(event_message), h_file) != NULL)
            {
                temp_event_id = get_event_id_form_entry(event_message);
                if (temp_event_id == event_id)
                {
                    // We found event, now get the event type
                    event_type = get_event_type_form_event_entry(event_message);
                    // Get the UID file name
                    get_unified_id_form_event_entry(event_message, sizeof(log_file_name), log_file_name);
                    // The action required file configured
                    snprintf(log_file_name, SYSTEM_LOG_FILE_NAME_MAX_LEN, ACTION_REQUIRED_FILE_PARSING_STRING, log_file_name);
                    break;
                }
            }
            // Close the file
            fclose(h_file);
        }
    }
    else
    {
        return -1;
    }
    // Event type cannot equal 0, it is stored in the log file means at least one bit needs to be set
    if (event_type)
    {
        h_file = fopen(log_file_name, "r+");
        if (NULL != h_file)
        {
            new_file_buffer = allocate_buffer_for_file(h_file, NULL);
            if (NULL != new_file_buffer) {
                // Remove the event type from the action required file
                while (fgets(event_type_str, sizeof(event_type_str), h_file) != NULL)
                {
                    read_event_type = (UINT32)AsciiStrHexToUintn(event_type_str);
                    if (((read_event_type ^ event_type) & (SYSTEM_EVENT_TYPE_CATEGORY_MASK | SYSTEM_EVENT_TYPE_NUMBER_MASK)) != 0)
                    {
                        strcat(new_file_buffer, event_type_str);
                    }
                }
                FILE *file = freopen(log_file_name, "w", h_file);
                if (file == NULL)
                    return -1;
                fprintf(h_file, "%s", new_file_buffer);
                fclose(h_file);
                free(new_file_buffer);
            } else {
                return -1;
            }
        }
    }

    return 0;
}

/*
* Remove all events matching the criteria form the log file
*/
NVM_API int nvm_remove_events_from_file(UINT32 event_type_mask, CONST CHAR8* dimm_uid, UINT32 event_id)
{
    CHAR8 *event_buffer = NULL;
    INTN event_buffer_size = 0;
    FILE *h_file = NULL;
    EFI_STATUS efi_status = EFI_SUCCESS;
    char log_file_name[SYSTEM_LOG_FILE_NAME_MAX_LEN] = { 0 };
    char *p_values_init = NULL;

    // Read all events matching the criteria
    event_buffer_size = get_system_events_from_file(FALSE, TRUE, event_type_mask, SYSTEM_EVENT_NOT_APPLICABLE, dimm_uid, event_id, NULL, &event_buffer);
	if (event_buffer_size > 0)
	{
		// Find the system log file name        
    if (SYSTEM_EVENT_TYPE_SEVERITY_GET(event_type_mask) & SYSTEM_EVENT_DEBUG_MASK) {
      efi_status = get_the_system_log_file_name(SYSTEM_LOG_DEBUG_FILE, sizeof(log_file_name), log_file_name);
      p_values_init = PTR_VALUES_INIT(SYSTEM_LOG_DEBUG_FILE);
    }
    else {
      efi_status = get_the_system_log_file_name(SYSTEM_LOG_EVENT_FILE, sizeof(log_file_name), log_file_name);
      p_values_init = PTR_VALUES_INIT(SYSTEM_LOG_EVENT_FILE);
    }
		if (EFI_SUCCESS == efi_status)
		{
			// Trunct the old log file and store the new value in it
			h_file = fopen(log_file_name, "w");
			if (NULL != h_file)
			{
				// Copy data only if there is anything in the buffer
				fprintf(h_file, "%s", event_buffer);
				// Close the file
				fclose(h_file);
        // Requires new value initialization
        *p_values_init = FALSE;
			}
			else
				return -1;
			// Free the buffer
			free(event_buffer);
		}
	}
    return 0;
}

/*
* Get all events matching the criteria form the log file
*/
NVM_API int nvm_get_events_from_file(UINT32 event_type_mask, CONST CHAR8 *dimm_uid, UINT32 event_id, INT32 count, log_entry **pp_log_entry, CHAR8 **event_buffer)
{
    if ((event_buffer == NULL) || (*event_buffer != NULL))
    {
        return -1;
    }

    return get_system_events_from_file(TRUE, FALSE, event_type_mask, count, dimm_uid, event_id, pp_log_entry, event_buffer);
}

/*
* Get all log entries matching the criteria form the log file
*/
NVM_API int nvm_get_log_entries_from_file(UINT32 event_type_mask, CONST CHAR8 *dimm_uid, UINT32 event_id, INT32 count, log_entry **pp_log_entry)
{
    if ((pp_log_entry == NULL) || (*pp_log_entry != NULL))
    {
        return -1;
    }

    return get_system_events_from_file(TRUE, FALSE, event_type_mask, count, dimm_uid, event_id, pp_log_entry, NULL);
}

/*
* Get event id form the event entry
*/
NVM_API int nvm_get_event_id_form_entry(CHAR8 *event_entry, UINT32 *event_id)
{
    if ((NULL == event_id) || (NULL == event_entry))
        return -1;

    *event_id = get_event_id_form_entry(event_entry);
    return 0;
}

/*
* Get dimm uid form the event entry
*/
NVM_API int nvm_get_uid_form_entry(CHAR8 *event_entry, UINT32 size, CHAR8 *dimm_uid)
{
    if ((NULL == dimm_uid) || (NULL == event_entry))
        return -1;

    if (get_unified_id_form_event_entry(event_entry, size, dimm_uid) == 0)
        return -2;

    return 0;
}

/*
* Store an event log entry in the system event log, wide character message support
*/
NVM_API int nvm_store_system_entry_widechar(CONST CHAR16 *source, UINT32 event_type, CONST CHAR16 *device_uid, CONST CHAR16 *message, ...)
{
  VA_LIST args;
  NVM_EVENT_MSG_W widechar_event_message = { 0 };
  NVM_EVENT_MSG ascii_event_message = { 0 };
  UINTN size = NVM_EVENT_MSG_LEN;
  UINTN ret_value = 0;
  CHAR8 ascii_source[MAX_SOURCE_STR_LENGTH] = { 0 };
  CHAR8 ascii_dimm_id[MAX_DIMM_UID_LENGTH] = { 0 };

  if ((NULL == source) || (NULL == message)) {
    return -1;
  }

  // Convert the event message wide character string to ascii event string
  VA_START(args, message);
  ret_value = UnicodeVSPrint(widechar_event_message, size, message, args);
  VA_END(args);
  if (ret_value < size) {
    UnicodeStrToAsciiStr(widechar_event_message, ascii_event_message);
    UnicodeStrToAsciiStr(source, ascii_source);
    if (NULL != device_uid) {
      UnicodeStrToAsciiStr(device_uid, ascii_dimm_id);
      nvm_store_system_entry(ascii_source, event_type, ascii_dimm_id, ascii_event_message);
    }
    else {
      nvm_store_system_entry(ascii_source, event_type, NULL, ascii_event_message);
    }
  }
  return -1;
}
