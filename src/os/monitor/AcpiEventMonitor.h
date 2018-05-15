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
#include "nvm_management.h"
#include <string>
#include <map>
#include <vector>
#include <Types.h>

#ifndef _MONITOR_ACPIEVENTMONITOR_H_
#define _MONITOR_ACPIEVENTMONITOR_H_
#define ACPI_MONITOR_LOG_SRC "Ixpdimm-monitor ACPI monitor thread"
#define ACPI_MONITOR_INIT_MSG "ACPI MONITOR INIT\n"
#define ACPI_MONITOR_INIT_EXCEPTION_PRE_MSG "Issue during ACPI Monitor init, error: "
#define ACPI_MONITOR_GEN_EVENTS_EXCEPTION_PRE_MSG "Error getting FW error log entry, error: "
#define ACPI_MONITOR_GEN_NVM_EVENTS_EXCEPTION_PRE_MSG "Error generating Nvm events, error: "
#define ACPI_MONITOR_EXCEPTION_PRE_MSG "Couldn't get devices - error: "
#define ACPI_WAIT_FOR_API_TIMED_OUT_MSG	"Timed out waiting for an ACPI event\n"
#define ACPI_WAIT_FOR_API_UNKNOWN_MSG "Unknown error occured while waiting for an ACPI event\n"
#define ACPI_CREATE_CTX_GENERAL_ERROR_MSG "Error creating acpi context\n"
#define ACPI_SMART_HEALTH_EVENT_THERM_LOW_HEADER "ThermLow"
#define ACPI_SMART_HEALTH_EVENT_THERM_HIGH_HEADER "ThermHigh"
#define ACPI_SMART_HEALTH_EVENT_MEDIA_LOW_HEADER "MediaLow"
#define ACPI_SMART_HEALTH_EVENT_MEDIA_HIGH_HEADER "MediaHigh"
#define ACPI_SMART_HEALTH_EVENT_TS_HEADER "TS: 0x"
#define ACPI_SMART_HEALTH_EVENT_TEMP_HEADER " TEMP: "
#define ACPI_SMART_HEALTH_EVENT_SEQ_NUM_HEADER " SEQ NUM: "
#define ACPI_SMART_HEALTH_EVENT_DPA_HEADER " DPA: 0x"
#define ACPI_SMART_HEALTH_EVENT_PDA_HEADER " PDA: 0x"
#define ACPI_SMART_HEALTH_EVENT_RANGE_HEADER " RANGE: 0x"
#define ACPI_SMART_HEALTH_EVENT_ERROR_TYPE_HEADER " ERROR TYPE: 0x"
#define ACPI_SMART_HEALTH_EVENT_ERROR_FLAGS_HEADER " ERROR FLAGS: 0x"
#define ACPI_SMART_HEALTH_EVENT_TRANS_TYPE_HEADER " TRANS TYPE: 0x"
#define ACPI_SMART_HEALTH_EVENT_TEMP_CORE_STR "core temp : "
#define ACPI_SMART_HEALTH_EVENT_TEMP_MEDIA_STR "media temp : "
#define ACPI_SMART_HEALTH_EVENT_USER_ALARM_TRIP_STR "user alarm trip : "
#define ACPI_SMART_HEALTH_EVENT_TEMP_LOW_STR "low : "
#define ACPI_SMART_HEALTH_EVENT_TEMP_HIGH_STR "high : "
#define ACPI_SMART_HEALTH_EVENT_TEMP_CRITICAL_STR "critical : "
#define ACPI_SMART_HEALTH_EVENT_TEMP_NEG_STR "- : "
#define ACPI_SMART_HEALTH_EVENT_TEMP_RAW_STR "raw value: "
#define ACPI_SMART_HEALTH_EVENT_TEMP_STR "celcius value: "
#define ACPI_WAIT_FOR_TIMEOUT_SEC	30
#define ACPI_EVENT_MSG_FW_ERROR_CNT_INCREASED "FW error count increased to: %d"
#define ACPI_EVENT_MSG_SMART_HEALTH_EVENT "%s: %s"
#define DEV_FW_ERR_LOG_LOW (0)
#define DEV_FW_ERR_LOG_HIGH (1)
#define DEV_FW_ERR_LOG_MEDIA (0 << 1)
#define DEV_FW_ERR_LOG_THERMAL (1 << 1)
namespace monitor
{
	typedef struct _dev_event_history
	{
		NVM_UID device_uid;
		unsigned int dimm_id;
		unsigned int dimm_handle;
		struct device_error_log_status stats;
	}dev_event_history;

	static const std::string MONITOR_NAME = "ACPI-EVENTS";

	class AcpiMonitor : public NvmMonitorBase
	{
		public:
			AcpiMonitor();
			virtual ~AcpiMonitor();
			virtual void monitor();
			virtual void init();
		private:
			std::vector<dev_event_history> last_dev_details;
			void processNvmEvents(const unsigned int device_handle);
			int processNewEvents(NVM_UID uid,
								unsigned char log_type,
								unsigned char log_level,
								struct fw_error_log_sequence_numbers last_numbers,
								struct fw_error_log_sequence_numbers cur_numbers);
			void generateSystemEventEntry(NVM_UID uid, unsigned char log_type, unsigned char log_level, void * log_entry);
			std::string formatThermalSystemEventEntryDescription(NVM_UID uid, void * log_entry);
			std::string formatMediaSystemEventEntryDescription(NVM_UID uid, void * log_entry);
			void sendFwErrCntSystemEventEntry(const NVM_UID device_uid, const NVM_UINT64 error_count);
			void sendFwErrLogSystemEventEntry(const NVM_UID device_uid, std::string log_type_level, std::string log_details);
			std::string createThermStr(THERMAL_ERROR_LOG_INFO * td);
			std::string m_event_log_src;
			void **acpi_contexts;
	};
}

#endif /* _MONITOR_ACPIEVENTMONITOR_H_ */
