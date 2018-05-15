/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of the event monitoring class
 * of the NvmMonitor service which periodically detects and stores interesting events.
 */

#include <string.h>
#include "EventMonitor.h"
#include <nvm_management.h>
#include <nvm_types.h>

monitor::EventMonitor::EventMonitor() :
	NvmMonitorBase(EVENT_MONITOR_NAME)
{
}

monitor::EventMonitor::~EventMonitor()
{
}

void monitor::EventMonitor::init()
{
	startOfDay();
}

void monitor::EventMonitor::cleanup()
{
	NvmMonitorBase::cleanup();
}

void monitor::EventMonitor::startOfDay()
{
	runPlatformConfigDiagnostic();
}

void monitor::EventMonitor::runPlatformConfigDiagnostic()
{
	runDiagnostic(DIAG_TYPE_PLATFORM_CONFIG);
}

void monitor::EventMonitor::runDiagnostic(const diagnostic_test diagType)
{
	diagnostic diag;
	memset(&diag, 0, sizeof (diag));
	diag.test = diagType;
	nvm_sync_lock_api();
	nvm_run_diagnostic(NULL, &diag, NULL);
	nvm_sync_unlock_api();
}

void monitor::EventMonitor::monitor()
{
	monitorDevices();
}

void monitor::EventMonitor::monitorDevices()
{
	runQuickHealthDiagnosticForDevice();
}

void monitor::EventMonitor::runQuickHealthDiagnosticForDevice()
{
	runDiagnostic(DIAG_TYPE_QUICK);
}
