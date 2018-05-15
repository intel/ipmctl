/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the Windows implementation of the os_adapter.h
 * system call wrappers.
 */

#include <stdio.h>
#include <syslog.h>
#include <os_types.h>

 /*
 * Map event type enum to event type defined in message file
 */
int get_linux_event_type(enum system_event_type type)
{
	int ret;
	switch (type)
	{
		case SYSTEM_EVENT_TYPE_WARNING:
			ret = LOG_WARNING;
			break;
		case SYSTEM_EVENT_TYPE_ERROR:
			ret = LOG_ERR;
			break;
		case SYSTEM_EVENT_TYPE_DEBUG:
			ret = LOG_DEBUG;
			break;
		case SYSTEM_EVENT_TYPE_INFO:
		default:
			ret = LOG_INFO;
			break;
	}

	return ret;
}

/*
* Retrive an event log entry from the system event log
*/
size_t get_system_events(char event_type_mask, int count, const char *source, char **event_buffer)
{
	return 0;
}

/*
 * Logs a message in the operating system event log.
 */
void log_system_event(enum system_event_type type, const char *source, const char *message)
{
	openlog(source, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

	syslog(LOG_MAKEPRI(LOG_LOCAL1, get_linux_event_type(type)), "%s", message);

	closelog();
}

