/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the definition of the performance monitoring class
 * of the NvmMonitor service which periodically polls and stores performance metrics
 * for each manageable NVM-DIMM in the system.
 */

#include "NvmMonitorBase.h"
#include <nvm_management.h>

#ifndef _MONITOR_PERFORMANCEMONITOR_H_
#define _MONITOR_PERFORMANCEMONITOR_H_


namespace monitor
{
	static const std::string PERFORMANCE_MONITOR_NAME = "PERFORMANCE";
	static const std::string PERFORMANCE_TABLE_NAME = "performance";

	/*
	 * Monitor class to periodically poll and store performance metrics for
	 * each manageable NVM-DIMM in the system.
	 */
	class PerformanceMonitor : public NvmMonitorBase
	{
		public:
			PerformanceMonitor();
			virtual ~PerformanceMonitor();
			virtual void monitor();

		private:
			std::vector<std::string> getDimmList();
			bool storeDimmPerformanceData(const std::string &dimmUidStr, struct device_performance &performance);
			void trimPerformanceData();
			//PersistentStore *m_pStore;
	};
}

#endif /* _MONITOR_PERFORMANCEMONITOR_H_ */
