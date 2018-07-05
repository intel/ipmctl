/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of the performance monitoring class
 * of the NvmMonitor service which periodically polls and stores performance metrics
 * for each manageable NVM-DIMM in the system.
 */

#include <string.h>
#include "PerformanceMonitor.h"
#include "LogEnterExit.h"
#include <event.h>

#if defined(__LINUX__)
#include <safe_str_lib.h>
#endif

monitor::PerformanceMonitor::PerformanceMonitor()
	: NvmMonitorBase(PERFORMANCE_MONITOR_NAME)
{
}

monitor::PerformanceMonitor::~PerformanceMonitor()
{
}

/*
 * Thread callback on monitor interval timer
 */
void monitor::PerformanceMonitor::monitor()
{
	nvm_sync_lock_api();
	// get list of manageable dimms
	std::vector<std::string> dimmList = monitor::NvmMonitorBase::getDimmList();
	for (std::vector<std::string>::const_iterator dimmUidIter = dimmList.begin();
			dimmUidIter != dimmList.end(); dimmUidIter++)
	{
		std::string dimmUidStr = *dimmUidIter;
		NVM_UID dimmUid;
		strncpy_s(dimmUid, NVM_MAX_UID_LEN, dimmUidStr.c_str(), NVM_MAX_UID_LEN);
    dimmUid[NVM_MAX_UID_LEN-1] = '\0';

		// get performance data for the dimm
		struct device_performance devPerformance;
		memset(&devPerformance, 0, sizeof (devPerformance));
		if (NVM_SUCCESS != (nvm_get_device_performance(dimmUid, &devPerformance)))
		{
			goto finish;
			return;
		}

		storeDimmPerformanceData(dimmUidStr, devPerformance);
	}

	// clean up
	dimmList.clear();
finish:
	nvm_sync_unlock_api();
}


bool monitor::PerformanceMonitor::storeDimmPerformanceData(const std::string &dimmUidStr,
		struct device_performance &performance)
{
	nvm_store_system_entry(LOG_SRC,
            SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_CAT_MGMT_NUMB_8, false, true, true, false, 0),
			dimmUidStr.c_str(),
			"BYTES READ - %llu, "\
			"BYTES WRITTEN - %llu, "\
			"READ REQS - %llu, "\
			"WRITE REQS - %llu, "\
			"BLOCK READS - %llu, "\
			"BLOCK WRITES - %llu",
			performance.bytes_read,
			performance.bytes_written,
			performance.host_reads,
			performance.host_writes,
			performance.block_reads,
			performance.block_writes);

	return true;
}

