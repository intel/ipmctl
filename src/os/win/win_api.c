/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the Windows implementation of the os_adapter.h
 * system call wrappers.
 */

#include <windows.h>
#include <winnt.h>
#include <stdio.h>
#include <strsafe.h>
#include "os_types.h"
#include "win_msgs.h"

#define MAX_EVENT_TYPE_LEN		15
#define MAX_RECORD_BUFFER_SIZE  0x10000  // 64K

CONST char p_system_event_type_names[][MAX_EVENT_TYPE_LEN] = { "Informational", "Warning", "Error", "Debug" };

/*
 * Blocks for the specified number of msecs.
 */
void nvm_sleep(unsigned long time)
{
	Sleep(time);
}

/*
 * Map event type enum to event type defined in message file
 */
WORD get_windows_event_type(enum system_event_type type)
{
	WORD ret;
	switch (type)
	{

		case SYSTEM_EVENT_TYPE_WARNING:
			ret = WARNING_EVENT;
			break;
		case SYSTEM_EVENT_TYPE_ERROR:
			ret = ERROR_EVENT;
			break;
		case SYSTEM_EVENT_TYPE_DEBUG:
		case SYSTEM_EVENT_TYPE_INFO:
		default:
			ret = INFORMATIONAL_EVENT;
			break;
	}

	return ret;
}

/*
* Map event type enum to event type defined in message file
*/
enum system_event_type get_system_event_type(WORD type)
{
	enum system_event_type ret;

	switch (type)
	{

	case WARNING_EVENT:
		ret = SYSTEM_EVENT_TYPE_WARNING;
		break;
	case ERROR_EVENT:
		ret = SYSTEM_EVENT_TYPE_ERROR;
		break;
	case INFORMATIONAL_EVENT:
	default:
		ret = SYSTEM_EVENT_TYPE_INFO;
		break;
	}

	return ret;
}

/*
 * Map event type enum to event id defined in message file
 */
DWORD get_event_id(enum system_event_type type)
{
	DWORD ret;

	switch (type)
	{
		case SYSTEM_EVENT_TYPE_WARNING:
			ret = NVMDIMM_WARNING;
			break;
		case SYSTEM_EVENT_TYPE_ERROR:
			ret = NVMDIMM_ERROR;
			break;
		case SYSTEM_EVENT_TYPE_DEBUG:
		case SYSTEM_EVENT_TYPE_INFO:
		default:
			ret = NVMDIMM_INFORMATIONAL;
			break;
	}

	return ret;
}

// Get a string that contains the time stamp of when the event
// was generated.
void get_time_stamp(const DWORD time, char *string)
{
	ULONGLONG ull_time_stamp = 0;
	ULONGLONG secs_to_1970 = 116444736000000000;
	SYSTEMTIME st;
	FILETIME ft, ftLocal;

	ull_time_stamp = Int32x32To64(time, 10000000) + secs_to_1970;
	ft.dwHighDateTime = (DWORD)((ull_time_stamp >> 32) & 0xFFFFFFFF);
	ft.dwLowDateTime = (DWORD)(ull_time_stamp & 0xFFFFFFFF);

	FileTimeToLocalFileTime(&ft, &ftLocal);
	FileTimeToSystemTime(&ftLocal, &st);
	snprintf(string, MAX_TIMESTAMP_LEN, "%d/%d/%d %.2d:%.2d:%.2d ",
		st.wMonth, st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond);
}

static void store_entry_in_buffer(PEVENTLOGRECORD p_entry, size_t *p_event_buff_size, char **event_buffer)
{
	char time_stamp[MAX_TIMESTAMP_LEN];
	char event_type[MAX_EVENT_TYPE_LEN];
	size_t end_of_event_buffer = *p_event_buff_size;

	// Get the timestamp string
	get_time_stamp(p_entry->TimeGenerated, time_stamp);
	// Get the entry time string
	snprintf(event_type, MAX_EVENT_TYPE_LEN, "%s ", p_system_event_type_names[get_system_event_type(p_entry->EventType)]);
	// Increase buffer size
	*p_event_buff_size += strlen(time_stamp) + strlen(event_type) + p_entry->DataOffset - p_entry->StringOffset + 1; // + new line marker
	*event_buffer = realloc(*event_buffer, *p_event_buff_size);
    if (NULL != *event_buffer) {
        ((char*)*event_buffer)[end_of_event_buffer] = 0;
        // Coppy strings to the buffer
        strcat_s(*event_buffer, *p_event_buff_size, time_stamp);
        strcat_s(*event_buffer, *p_event_buff_size, event_type);
        strcat_s(*event_buffer, *p_event_buff_size, (char*)(((char*)p_entry) + p_entry->StringOffset));
        strcat_s(*event_buffer, *p_event_buff_size, "\n");
    }
}

static int retrieve_events_form_buffer(const char *source, size_t *p_event_buff_size, char **event_buffer, char *p_buffer, DWORD bytes_read, int numb_of_events, char event_type_mask)
{
	char *p_entry = p_buffer;
	char *p_end_of_buffer = p_buffer + bytes_read;
	int  event_count = 0;
	enum system_event_type event_type;
	BOOL skip_entry = FALSE;

	while (p_entry < p_end_of_buffer)
	{
		// If the event was written by our provider, write the contents of the event.
		if (0 == strcmp(source, (char*)(p_entry + sizeof(EVENTLOGRECORD))))
		{
			if (SYSTEM_EVENT_NOT_APPLICABLE != event_type_mask)
			{
				// Get event type and compare with requested event type mask
				event_type = get_system_event_type(((PEVENTLOGRECORD)p_entry)->EventType);
				if (event_type_mask & SYSTEM_EVENT_TO_MASK(event_type))
				{
					skip_entry = FALSE;
				}
				else
				{
					skip_entry = TRUE;
				}
			}

			if (FALSE == skip_entry)
			{
				// Store entry in the buffer as a string
				store_entry_in_buffer((PEVENTLOGRECORD)p_entry, p_event_buff_size, event_buffer);

				if (SYSTEM_EVENT_NOT_APPLICABLE != numb_of_events)
				{
					event_count++;
					if (event_count >= numb_of_events)
					{
						// We have reached number of events we want to collect
						break;
					}
				}
			}
		}
		// Get another entry
		p_entry += ((PEVENTLOGRECORD)p_entry)->Length;
	}

	return event_count;
}

/*
* Retrive an event log entry from the system event log
*/
size_t get_system_events(char event_type_mask, int count, const char *source, char **event_buffer)
{
	HANDLE h_event_log;
	char * p_buffer = NULL;
	char * p_temp = NULL;
	DWORD status = ERROR_SUCCESS;
	DWORD buff_size = MAX_RECORD_BUFFER_SIZE;
	DWORD bytes_read = 0;
	DWORD bytes_to_read = 0;
	size_t event_buffer_size = 0;
	int check_the_count = count;

	// The source name (provider) must exist as a subkey of Application.
	h_event_log = OpenEventLog(NULL, source);
	if (NULL != h_event_log)
	{
		// Allocate an initial block of memory used to read event records. The number
		// of records read into the buffer will vary depending on the size of each event.
		// The size of each event will vary based on the size of the user-defined
		// data included with each event, the number and length of insertion
		// strings, and other data appended to the end of the event record.
		p_buffer = (char*)malloc(buff_size);
		if (NULL != p_buffer)
		{
			// Read blocks of records until you reach the end of the log or an
			// error occurs. The records are read from newest to oldest. If the buffer
			// is not big enough to hold a complete event record, reallocate the buffer.
			while (ERROR_SUCCESS == status)
			{
				if (!ReadEventLog(h_event_log,
					EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ,
					0,
					p_buffer,
					buff_size,
					&bytes_read,
					&bytes_to_read))
				{
					status = GetLastError();
					if (ERROR_INSUFFICIENT_BUFFER == status)
					{
						p_temp = (PBYTE)realloc(p_buffer, bytes_to_read);
						if (NULL != p_temp)
						{
							status = ERROR_SUCCESS;  // we want one more loop
							p_buffer = p_temp;
							buff_size = bytes_to_read;
						}
					}
				}
				else
				{
					count -= retrieve_events_form_buffer(source, &event_buffer_size, event_buffer, p_buffer, bytes_read, count, event_type_mask);
					if ((SYSTEM_EVENT_NOT_APPLICABLE != check_the_count) && (0 >= count))
					{
						break;
					}
				}
			}
		}
	}

	if (h_event_log)
		CloseEventLog(h_event_log);

	if (p_buffer)
		free(p_buffer);

	return event_buffer_size;
}

/*
 * Logs a message in the operating system event log.
 */
void log_system_event(enum system_event_type type, const char *source, const char *message)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[1];

	lpszStrings[0] = message;

	hEventSource = RegisterEventSource(NULL, source);

	if (NULL != hEventSource)
	{
		ReportEvent(hEventSource,			// event log handle
				get_windows_event_type(type),	// event type
				0,				// event category
				get_event_id(type),			// event identifier
				NULL,				// no security identifier
				1,				// size of lpszStrings array
				0,				// no binary data
				lpszStrings,			// array of strings
				NULL);				// no binary data

		DeregisterEventSource(hEventSource);
	}
}

