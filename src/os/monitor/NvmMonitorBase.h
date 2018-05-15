/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the definitino of the NvmMonitor service.
 */

#include <vector>
//#include <persistence/lib_persistence.h>
#include <sstream>
//#include <os/os_adapter.h>
//#include <persistence/logging.h>
//#include <nvm_types.h>
#include <os_types.h>

#ifndef _MONITOR_NVMMONITOR_H_
#define _MONITOR_NVMMONITOR_H_

#define LOG_SRC "ixpdimm-monitor"

namespace monitor
{
	typedef void(*SYSTEM_LOGGER)(enum system_event_type, std::string, std::string);

	static const std::string MONITOR_INTERVAL_SUFFIX_KEY = "_MONITOR_INTERVAL_MINUTES";
	static const std::string MONITOR_ENABLED_SUFFIX_KEY = "_MONITOR_ENABLED";
	static const size_t DEFAULT_INTERVAL_SECONDS = 60; // 1 minute
	static const bool DEFAULT_MONITOR_ENABLED = true;

	/*
	 * Base class for NvmMonitors.
	 * Note: Any class inheriting from NvmMonitorBase should add the appropriate
	 * entries to the apss.dat DB by adding to the create_default_config(char *path)
	 * function in lib_persistence.c. Also see MONITOR_INTERVAL_SUFFIX_KEY,
	 * MONITOR_ENABLED_SUFFIX_KEY, and the NvmMonitorBase constructor for reference.
	 */
	class NvmMonitorBase
	{

	public:
		/*
		 * Main Constructor
		 */
		NvmMonitorBase(std::string const &name);

		virtual ~NvmMonitorBase();

		virtual void init() {}
		virtual void monitor() = 0;
		virtual void cleanup() {}
		virtual void abort();
		std::string const & getName() const;

		size_t getIntervalSeconds() const;
		bool isEnabled() const;
		bool m_abort;
		/*
		 * Static function to get the collection of enabled monitors
		 */
		static void getMonitors(std::vector<NvmMonitorBase *> &monitors);
		static void deleteMonitors(std::vector<NvmMonitorBase *> &monitors);
		static void log(enum system_event_type, std::string, std::string);
		static std::vector<std::string> getDimmList();
	protected:
		std::string m_name;
		size_t m_intervalSeconds;
		bool m_enabled;
		SYSTEM_LOGGER m_logger;
	};



};

#endif /* _MONITOR_NVMMONITOR_H_ */
