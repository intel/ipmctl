/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of the event monitoring class
 * of the NvmMonitor service which periodically detects and stores interesting events.
 */

#include "NvmMonitorBase.h"
#include "nvm_management.h"
//#include <persistence/schema.h>
#include <string>
#include <map>
#include <vector>
//#include <core/NvmLibrary.h>

#ifndef _MONITOR_EVENTMONITOR_H_
#define _MONITOR_EVENTMONITOR_H_

namespace monitor
{
	static const std::string EVENT_MONITOR_NAME = "EVENT";

	class EventMonitor : public NvmMonitorBase
	{
	public:
		class NoDeviceSavedState : public std::exception
		{};

		EventMonitor();
		virtual ~EventMonitor();
		virtual void init();
		virtual void cleanup();
		virtual void monitor();

	private:
		void startOfDay();
		void runPlatformConfigDiagnostic();
		void runDiagnostic(const diagnostic_test diagType);
		void monitorDevices();
		void runQuickHealthDiagnosticForDevice();
	};
}
#endif /* _MONITOR_EVENTMONITOR_H_ */
