/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of Windows system service utility methods.
 */

#include <windows.h>
#include <string>
#include "win_service.h"
#include "NvmMonitorBase.h"
#include <event.h>

#define	MESSAGE_LIB	"win_msgs.dll"
#define EVENT_PATH "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
int install_event_source(const char *service_name);
int uninstall_event_source(const char *service_name);
void logMsg(enum system_event_type msg_type, std::string src, std::string msg);
void logStartError(std::string msg);

static const int MAX_SERVICE_NAME = 256;
char g_serviceName[MAX_SERVICE_NAME];
SERVICE_STATUS_HANDLE gServiceHandle;
HANDLE g_serviceStopEvent;

bool serviceInstall(std::string serviceName, std::string displayName)
{
	bool result = true;

	// Open Service Control Manager
	SC_HANDLE hSCM;
	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCM == NULL)
	{
		result = false;
	}
	else
	{
		// Get full path of executable
		TCHAR szPath[MAX_PATH];
		int num_bytes = GetModuleFileName(NULL, szPath, MAX_PATH);
		if (num_bytes == 0)
		{
			result = false;
		}
		else
		{
			strcat_s(szPath, MAX_PATH, " ");

			// Call create service
			SC_HANDLE hService;
			hService = CreateService(
					hSCM,
					serviceName.c_str(),
					displayName.c_str(),
					SERVICE_ALL_ACCESS,
					SERVICE_WIN32_OWN_PROCESS,
					SERVICE_AUTO_START,
					SERVICE_ERROR_NORMAL,
					szPath,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);

			if (hService == NULL)
			{
				result = false;
			}
			else
			{
				// register the service as an event source
				install_event_source(g_serviceName);
				CloseServiceHandle(hService);
			}
		}
		CloseServiceHandle(hSCM);
	}
	return result;
}

/*
 * Delete a windows service
 */
bool serviceUninstall(const char *service_name)
{
	bool result = true;

	// Open Service Control Manager
	SC_HANDLE hSCM;
	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (hSCM == NULL)
	{
		result = false;
	}
	else
	{
		// Call delete service
		SC_HANDLE hService;
		hService = OpenService(
				hSCM,
				service_name,
				DELETE);

		if (hService == NULL)
		{
			result = false;
		}
		else
		{
			if (!DeleteService(hService))
			{
				result = false;
			}
			else
			{
				// remove service from being event source
				uninstall_event_source(g_serviceName);
				CloseServiceHandle(hService);
			}
		}
		CloseServiceHandle(hSCM);
	}
	return result;
}

/*
 * This gets called from int main() when the SCM is starting the service
 */
bool serviceInit(std::string serviceName)
{
	strcat_s(g_serviceName, MAX_SERVICE_NAME, serviceName.c_str());
	bool result = true;

	SERVICE_TABLE_ENTRY service_table[] = {
			{g_serviceName, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
			{NULL, NULL}
	};

	if (StartServiceCtrlDispatcher(service_table) == 0)
	{
		logStartError("StartServiceCtrlDispatcher start failed.");
		result = false;
	}
	return result;
}

/*
 * Helper function to set the Windows service state
 */
BOOL setServiceStatus(DWORD state, BOOL block_controls, DWORD win32_exit_code, DWORD timeout_ms)
{

	SERVICE_STATUS status = {0};

	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status.dwCurrentState = state;
	status.dwWin32ExitCode = win32_exit_code;
	status.dwWaitHint = timeout_ms;

	status.dwControlsAccepted = block_controls ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	return SetServiceStatus(gServiceHandle, &status);
}

/*
* Service Control Handler. This is called when the SCM sends the "Stop" signal
*/
void ServiceCtrlHandler(DWORD ctrl)
{
	BOOL stop_service = FALSE;
	TCHAR szMsg[1024];

	snprintf(szMsg, sizeof(szMsg), "%s service stopping.", g_serviceName);
	switch (ctrl)
	{
		case SERVICE_CONTROL_STOP:
			/*log_system_event(SYSTEM_EVENT_TYPE_INFO, szMsg,
					"Received SERVICE_CONTROL_STOP.");*/
			stop_service = TRUE;
			break;
		case SERVICE_CONTROL_SHUTDOWN:
			/*log_system_event(SYSTEM_EVENT_TYPE_INFO, szMsg,
					"Received SERVICE_CONTROL_SHUTDOWN.");*/
			stop_service = TRUE;
			break;
		case SERVICE_CONTROL_INTERROGATE:
			break;
		default:
			break;
	}

	if (stop_service == TRUE)
	{

		setServiceStatus(SERVICE_STOP_PENDING, true, NO_ERROR, 0);

		// signal threads to stop
		SetEvent(g_serviceStopEvent);
	}
}

/*
 * Background worker thread. Loops until it's time to stop, calling the monitor
 * on the appropriate interval.
 */
DWORD WINAPI WorkerThread(LPVOID arg)
{
	if (arg != NULL)
	{
		monitor::NvmMonitorBase *pMonitor = (monitor::NvmMonitorBase *) arg;

		int milliseconds = (int)pMonitor->getIntervalSeconds() * 1000;
		pMonitor->init();
		//  Wait for the service stop signal until it's time to run the monitor callback
		while (WaitForSingleObject(g_serviceStopEvent, milliseconds) != WAIT_OBJECT_0)
		{
			pMonitor->monitor();
		}
		pMonitor->cleanup();
	}

	return ERROR_SUCCESS;
}

/*
* Service Main function - Called by the SCM after the service has started
*/
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	gServiceHandle = RegisterServiceCtrlHandler(g_serviceName, ServiceCtrlHandler);
	if (gServiceHandle == 0)
	{
		logStartError("RegisterServiceCtrlHandler failed.");
	}
	else
	{
		// signal service is starting
		setServiceStatus(SERVICE_START_PENDING, TRUE, NO_ERROR, 3000);

		// create event for when service is signaled to stop
		g_serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (g_serviceStopEvent)
		{
			TCHAR szPath[MAX_PATH];
			char *last_slash;

			GetModuleFileName(NULL, szPath, MAX_PATH);
			last_slash = strrchr(szPath, '\\');
			if (NULL != last_slash)
				*last_slash = '\0';

			if (SetCurrentDirectory(szPath))
			{
				//open_default_lib_store();

				std::vector<monitor::NvmMonitorBase *> monitors;
				monitor::NvmMonitorBase::getMonitors(monitors);

				size_t handleCount = monitors.size() + 1; // +1 to also add g_serviceStopEvent
				HANDLE *handles = new HANDLE[handleCount];
				for (size_t i = 0; i < monitors.size(); i++)
				{
					handles[i] =
							CreateThread(NULL, 0, WorkerThread, (LPVOID) (monitors[i]), 0, NULL);
				}

				// In the case there are no monitor items, adding the stop event will keep the
				// service running until a Service Stop Event occurs.
				handles[handleCount - 1] = g_serviceStopEvent;

				// now running
				setServiceStatus(SERVICE_RUNNING, false, NO_ERROR, 0);

				// wait for all threads to end and Service Stop Event
				WaitForMultipleObjects((DWORD)handleCount, handles, true, INFINITE);

				// clean up
				for (size_t i = 0; i < handleCount; i++)
				{
					CloseHandle(handles[i]);
				}

                delete handles;

				monitor::NvmMonitorBase::deleteMonitors(monitors);
				//close_lib_store();

				setServiceStatus(SERVICE_STOPPED, FALSE, NO_ERROR, 0);
			}
			else
			{
				logStartError("Failed to set working directory.");
				setServiceStatus(SERVICE_STOPPED, FALSE, NO_ERROR, 0);
			}

		}
		else
		{
			logStartError("CreateEvent Failed");
			setServiceStatus(SERVICE_STOPPED, FALSE, NO_ERROR, 0);
		}


	}
}

void logMsg(unsigned int msg_type, std::string src, std::string msg)
{
	//log_system_event(msg_type, src.c_str(),
	//	msg.c_str());
}

void logStartError(std::string msg)
{
	std::stringstream message;
	message << "Failed to start " << g_serviceName << " service.";
	logMsg(SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_ERROR, SYSTEM_EVENT_CAT_MGMT_NUMB_9, false, true, true, false, 0), message.str().c_str(), msg);
}



/*
 * Create registry entries for installing service as an event source
 */
int install_event_source(const char *service_name)
{
	HKEY regkey;
	char path[MAX_PATH] = EVENT_PATH;

	strcat_s(path, MAX_PATH, service_name);

	int ret = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
			path,
			0,
			0,
			REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE,
			0,
			&regkey,
			0);
	if (ret == ERROR_SUCCESS)
	{
		DWORD types_supported = EVENTLOG_ERROR_TYPE |
				EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

		TCHAR message_lib[MAX_PATH];
		char *last_slash;
		GetModuleFileName(NULL, message_lib, MAX_PATH);
		last_slash = strrchr(message_lib, '\\');
		if (NULL != last_slash)
			*last_slash = '\0';
		strcat_s(message_lib, MAX_PATH, "\\");
		strcat_s(message_lib, MAX_PATH, MESSAGE_LIB);

		ret = RegSetValueEx(regkey,
				"EventMessageFile",
				0,
				REG_SZ,
				(BYTE *) message_lib,
				(DWORD)strlen(message_lib));

		if (ret == ERROR_SUCCESS)
		{
			ret = RegSetValueEx(regkey,
					"TypesSupported",
					0,
					REG_DWORD,
					(LPBYTE) &types_supported,
					sizeof (types_supported));
		}

		if (ret != ERROR_SUCCESS) // Failed to create either of the values
		{
			ret = -6;
		}
		else
		{
			ret = 0;
		}

		RegCloseKey(regkey);
	}

	return ret;
}

/*
 * Delete registry entries to remove event source
 */
int uninstall_event_source(const char *service_name)
{
	int ret = 0;
	char path[MAX_PATH] = EVENT_PATH;

	strcat_s(path, MAX_PATH, service_name);
	ret = RegDeleteKey(HKEY_LOCAL_MACHINE, path);
	if (ret != ERROR_SUCCESS)
	{
		ret = -6;
	}

	return ret;
}

