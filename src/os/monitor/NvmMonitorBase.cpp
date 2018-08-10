/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of the NvmMonitor service.
 */
#include <string>
#include <iostream>
#include "LogEnterExit.h"
#include "NvmMonitorBase.h"
#include "PerformanceMonitor.h"
#include "EventMonitor.h"
#include "AcpiEventMonitor.h"
#include <nvm_management.h>
#include <event.h>

#if defined(__LINUX__)
#include <safe_str_lib.h>
#endif

/*
 * Constructor
 * param:  name - used to look up monitor configurations in the config database.
 *If config keys aren't find, then default values are used
 */
monitor::NvmMonitorBase::NvmMonitorBase(std::string const &name)
		: m_name(name)
{
	m_abort = false;
	// get values from Database
	std::string intervalKey = m_name + MONITOR_INTERVAL_SUFFIX_KEY;
	std::string enabledKey = m_name + MONITOR_ENABLED_SUFFIX_KEY;

	m_intervalSeconds = DEFAULT_INTERVAL_SECONDS;
	m_enabled = DEFAULT_MONITOR_ENABLED;

	int configResult;
	configResult = nvm_get_config_int(intervalKey.c_str(), 1);
	m_intervalSeconds = (size_t)configResult * 60;

	configResult = nvm_get_config_int(enabledKey.c_str(), 0);
	m_enabled = configResult != 0;

  nvm_store_system_entry(LOG_SRC,
    SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, EVENT_CONFIG_CHANGE_312, false, true, true, false, 0),
    NULL, "The Intel(R) Optane (TM) DC persistent memory %s monitor service has started.", m_name.c_str());
}

monitor::NvmMonitorBase::~NvmMonitorBase()
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);
}

void monitor::NvmMonitorBase::abort()
{
	m_abort = true;
}

void monitor::NvmMonitorBase::getMonitors(std::vector<monitor::NvmMonitorBase *> &monitors)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	AcpiMonitor *acpiMon = new AcpiMonitor();
	monitors.push_back(acpiMon);

	EventMonitor *event = new EventMonitor();
	if (event && event->isEnabled())
	{
		monitors.push_back(event);
	}
	else if (event)
	{
		delete event;
	}

	PerformanceMonitor *performance = new PerformanceMonitor();
	if (performance && performance->isEnabled())
	{
		monitors.push_back(performance);
	}
	else
	{
		delete performance;
	}
}

/*
 * free the memory allocated for the monitors
 */
void monitor::NvmMonitorBase::deleteMonitors(std::vector<NvmMonitorBase *> &monitors)
{
	LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);
	for(size_t m = 0; m < monitors.size(); m++)
	{
		delete monitors[m];
	}
}

/*
 * Getters for private fields
 */

std::string const &monitor::NvmMonitorBase::getName() const
{
	return m_name;
}

size_t monitor::NvmMonitorBase::getIntervalSeconds() const
{
	return m_intervalSeconds;
}

bool monitor::NvmMonitorBase::isEnabled() const
{
	return m_enabled;
}

void monitor::NvmMonitorBase::log(enum system_event_type type, std::string src, std::string msg)
{
	std::cout << "SRC: " << src << " MSG:" << msg;
}


std::vector<std::string> monitor::NvmMonitorBase::getDimmList()
{
	//LogEnterExit logging(__FUNCTION__, __FILE__, __LINE__);

	std::vector<std::string> dimmList;
  unsigned int dimmCount = 0;
  int nvm_status = 0;
  if (NVM_SUCCESS == nvm_get_number_of_devices(&dimmCount)) {
    // error getting dimm count
    if (dimmCount < 0)
    {
      nvm_store_system_entry(LOG_SRC,
        SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_ERROR, SYSTEM_EVENT_CAT_MGMT_NUMB_6, false, true, true, false, 0),
        NULL,
        "nvm_get_device_count failed with error %d",
        dimmCount);
    }
    // at least one dimm
    else if (dimmCount > 0)
    {
      struct device_discovery *dimms = new device_discovery[dimmCount];
      nvm_status = nvm_get_devices(dimms, dimmCount);
      // error getting dimms
      if (nvm_status != NVM_SUCCESS)
      {
        nvm_store_system_entry(LOG_SRC,
          SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_ERROR, SYSTEM_EVENT_CAT_MGMT_NUMB_7, false, true, true, false, 0),
          NULL,
          "nvm_get_devices failed with error %d",
          dimmCount);
      }
      // at least one dimm
      else
      {
        for (unsigned int i = 0; i < dimmCount; i++)
        {
          // only looks at manageable NVM-DIMMs
          if (dimms[i].manageability == MANAGEMENT_VALIDCONFIG)
          {
            NVM_UID uidStr;
            strncpy_s(uidStr, NVM_MAX_UID_LEN, dimms[i].uid, NVM_MAX_UID_LEN);
            dimmList.push_back(std::string(uidStr));
          }
        }
      }

      delete[] dimms;
    }
  }
	return dimmList;
}
