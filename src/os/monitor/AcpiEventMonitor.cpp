/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "AcpiEventMonitor.h"
#include <string.h>
#include "EventMonitor.h"
#include <iostream>
#include <sstream>
#include <NvmDimmPassThru.h>
#include <event.h>

monitor::AcpiMonitor::AcpiMonitor() :
	NvmMonitorBase(MONITOR_NAME)
{
	m_event_log_src = std::string(ACPI_MONITOR_LOG_SRC);
	//minimal delay in monitor execution
	m_intervalSeconds = 1;
  acpi_contexts = NULL;
}

monitor::AcpiMonitor::~AcpiMonitor()
{
}

/*
* Called once on daemon startup.  Captures the health state
* of each dimm, which includes a snapshot of the error log.
* Error log entries found during init will not trigger future health events.
*
* @param[in] logger - logging interface.
*/
void monitor::AcpiMonitor::init()
{
	int dev_cnt = 0;

	nvm_sync_lock_api();

	nvm_store_system_entry(LOG_SRC,
        SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_CAT_MGMT_NUMB_2, false, false, true, false, 0),
        NULL,
		"ACPI Monitor starting initialization...");

	std::vector<std::string> dimmList = getDimmList();
	for (std::vector<std::string>::const_iterator dimmUidIter = dimmList.begin();
		dimmUidIter != dimmList.end(); dimmUidIter++)
	{
		dev_event_history e_hist;
		memset(&e_hist, 0, sizeof(e_hist));
		std::string dimmUidStr = *dimmUidIter;
		strncpy(e_hist.device_uid, dimmUidStr.c_str(), NVM_MAX_UID_LEN);
		// get performance data for the dimm
		if (NVM_SUCCESS != (nvm_get_fw_err_log_stats(e_hist.device_uid, &e_hist.stats)))
		{
			continue;
		}
		nvm_get_dimm_id(e_hist.device_uid, &e_hist.dimm_id, &e_hist.dimm_handle);

		last_dev_details.push_back(e_hist);
		++dev_cnt;
	}

	// clean up
	dimmList.clear();
	int rc;
	acpi_contexts = new void*[dev_cnt];
	for (int i = 0; i < dev_cnt; i++)
	{
		if (NVM_SUCCESS != (rc = nvm_acpi_event_create_ctx(last_dev_details[i].dimm_handle, &acpi_contexts[i])))
		{
			if (acpi_contexts)
				free(acpi_contexts);

			goto Finish;
		}
		else
		{
			nvm_acpi_event_set_monitor_mask(acpi_contexts[i], DIMM_ACPI_EVENT_SMART_HEALTH_MASK);
		}
	}

	nvm_store_system_entry(LOG_SRC,
        SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, SYSTEM_EVENT_CAT_MGMT_NUMB_3, false, false, true, false, 0),
        NULL,
		"ACPI Monitor init complete.  Will be monitoring %d number of dimms",
		dev_cnt);

Finish:
	nvm_sync_unlock_api();
}

/*
* The main entry point for monitoring asynchronous smart health
* ACPI notifications.
*/
void monitor::AcpiMonitor::monitor()
{
	unsigned int dev_cnt = (unsigned int)last_dev_details.size();
	enum acpi_get_event_result result;
	nvm_acpi_wait_for_event(acpi_contexts, dev_cnt, ACPI_WAIT_FOR_TIMEOUT_SEC, &result);
	switch (result)
	{
	case ACPI_EVENT_SIGNALLED_RESULT:
		for (unsigned int i = 0; i < dev_cnt; ++i)
		{
			enum acpi_event_state r;
			nvm_acpi_event_get_event_state(acpi_contexts[i], ACPI_SMART_HEALTH, &r);
			if (ACPI_EVENT_SIGNALLED == r)
			{
				unsigned int dimm;
				nvm_acpi_event_ctx_get_dimm_handle(acpi_contexts[i], &dimm);
				//received an async ACPI event notification
				//now figure out what happened and generate system level events and messages
				nvm_sync_lock_api();
				processNvmEvents(dimm);
				nvm_sync_unlock_api();
			}
		}
		break;
	case ACPI_EVENT_TIMED_OUT_RESULT:
		//no log msg, happens frequently
		break;
	case ACPI_EVENT_UNKNOWN_RESULT:
		break;
	}
}

/*
* Start the process of dispositioning and generating system level events
* based on information gathered from the target device.
*
* @param[in] device_handle - target device in question
*/
void  monitor::AcpiMonitor::processNvmEvents(const unsigned int device_handle)
{
	int total_events = 0;
	unsigned int dev_cnt = (unsigned int)last_dev_details.size();
	for (size_t i = 0; i < dev_cnt; i++)
	{
		if (last_dev_details[i].dimm_handle == device_handle)
		{
			dev_event_history cur_stats;
			memset(&cur_stats, 0, sizeof(cur_stats));
			nvm_get_fw_err_log_stats(last_dev_details[i].device_uid, &cur_stats.stats);

			total_events = processNewEvents(last_dev_details[i].device_uid,
				DEV_FW_ERR_LOG_THERMAL,
				DEV_FW_ERR_LOG_LOW,
				last_dev_details[i].stats.therm_low,
				cur_stats.stats.therm_low);

			total_events += processNewEvents(last_dev_details[i].device_uid,
				DEV_FW_ERR_LOG_THERMAL,
				DEV_FW_ERR_LOG_HIGH,
				last_dev_details[i].stats.therm_high,
				cur_stats.stats.therm_high);

			total_events += processNewEvents(last_dev_details[i].device_uid,
				DEV_FW_ERR_LOG_MEDIA,
				DEV_FW_ERR_LOG_LOW,
				last_dev_details[i].stats.media_low,
				cur_stats.stats.media_low);

			total_events += processNewEvents(last_dev_details[i].device_uid,
				DEV_FW_ERR_LOG_MEDIA,
				DEV_FW_ERR_LOG_HIGH,
				last_dev_details[i].stats.media_high,
				cur_stats.stats.media_high);

			//sendFwErrCntSystemEventEntry(last_dev_details[i].device_uid, total_events);
			memcpy(cur_stats.device_uid, last_dev_details[i].device_uid, sizeof(cur_stats.device_uid));
			cur_stats.dimm_id = last_dev_details[i].dimm_id;
			cur_stats.dimm_handle = last_dev_details[i].dimm_handle;
			last_dev_details[i] = cur_stats;

			return;
		}
	}
}

/*
* Generate a new system event for any new fw error log entries.
* A system event includes Windows event, or msg in syslog depending on
* underlying OS.  This will also add entries into the CR MGMT DB.
*
* @param[in] uid - dimm that generated the event
* @param[in] log_type - DEV_FW_ERR_LOG_MEDIA or DEV_FW_ERR_LOG_THERM
* @param[in] log_level - DEV_FW_ERR_LOG_LOW or DEV_FW_ERR_LOG_HIGH
* @param[in] last_numbers - the fw log sequence numbers obtained by either monitor init or
*							the last time an event of this type happened.
* @param[in] cur_numbers - the current fw log sequence numbers (see FIS for details)
*/
int monitor::AcpiMonitor::processNewEvents(NVM_UID uid,
	unsigned char log_type,
	unsigned char log_level,
	struct fw_error_log_sequence_numbers last_numbers,
	struct fw_error_log_sequence_numbers cur_numbers)
{
	unsigned char buffer[128];
	int new_log_cnt = 0;

	for (int index = cur_numbers.oldest; index <= cur_numbers.current; index++)
	{
		if (!(index >= last_numbers.oldest &&
			index <= last_numbers.current))
		{
			int rc;
			//get the log via the seq number and craft an appropriate event.
			if (NVM_SUCCESS == (rc = nvm_get_fw_error_log_entry_cmd(uid,
				index, log_level, log_type, buffer, 128)))
			{
				generateSystemEventEntry(uid, log_type, log_level, (void *)buffer);
				new_log_cnt++;
			}
		}
	}
	return new_log_cnt;
}

/*
* Start the process of dispositioning and generating system level events
* based on information gathered from the target device.
*
* @param[in] uid - target device in question
* @param[in] log_type - DEV_FW_ERR_LOG_THERMAL or DEV_FW_ERR_LOG_MEDIA
* @param[in] log_level - DEV_FW_ERR_LOG_LOW or DEV_FW_ERR_LOG_HIGH
* @param[in] log_entry - raw log entry from the device FW.
*/
void monitor::AcpiMonitor::generateSystemEventEntry(NVM_UID uid, unsigned char log_type, unsigned char log_level, void * log_entry)
{
	std::string description;
	std::string header;

	if (DEV_FW_ERR_LOG_THERMAL == log_type && DEV_FW_ERR_LOG_LOW == log_level)
	{
		header = ACPI_SMART_HEALTH_EVENT_THERM_LOW_HEADER;
		description = formatThermalSystemEventEntryDescription(uid, log_entry);
	}
	else if (DEV_FW_ERR_LOG_THERMAL == log_type && DEV_FW_ERR_LOG_HIGH == log_level)
	{
		header = ACPI_SMART_HEALTH_EVENT_THERM_HIGH_HEADER;
		description = formatThermalSystemEventEntryDescription(uid, log_entry);
	}
	else if (DEV_FW_ERR_LOG_MEDIA == log_type && DEV_FW_ERR_LOG_LOW == log_level)
	{
		header = ACPI_SMART_HEALTH_EVENT_MEDIA_LOW_HEADER;
		description = formatMediaSystemEventEntryDescription(uid, log_entry);
	}
	else if (DEV_FW_ERR_LOG_MEDIA == log_type && DEV_FW_ERR_LOG_HIGH == log_level)
	{
		header = ACPI_SMART_HEALTH_EVENT_MEDIA_HIGH_HEADER;
		description = formatMediaSystemEventEntryDescription(uid, log_entry);
	}
	sendFwErrLogSystemEventEntry(uid, header, description);
}

/*
* Make a human readable description from the raw fw thermal log entry.
*
* @param[in] uid - target device in question
* @param[in] log_entry - raw log entry from the device FW.
*/
std::string monitor::AcpiMonitor::formatThermalSystemEventEntryDescription(NVM_UID uid, void * log_entry)
{
	ERROR_LOG_INFO *p_error_log = (ERROR_LOG_INFO *)log_entry;
	THERMAL_ERROR_LOG_INFO *p_therm_log = (THERMAL_ERROR_LOG_INFO *)p_error_log->OutputData;
	std::stringstream log_details;
	log_details << ACPI_SMART_HEALTH_EVENT_TS_HEADER << std::hex << p_error_log->SystemTimestamp
		<< ACPI_SMART_HEALTH_EVENT_TEMP_HEADER << createThermStr(p_therm_log)
		<< ACPI_SMART_HEALTH_EVENT_SEQ_NUM_HEADER << std::dec << p_therm_log->SequenceNum;
	return log_details.str();
}

/*
* Make a human readable description from the raw fw media log entry.
*
* @param[in] uid - target device in question
* @param[in] log_entry - raw log entry from the device FW.
*/
std::string monitor::AcpiMonitor::formatMediaSystemEventEntryDescription(NVM_UID uid, void * log_entry)
{
	ERROR_LOG_INFO *p_error_log = (ERROR_LOG_INFO *)log_entry;
	MEDIA_ERROR_LOG_INFO *p_media_log = (MEDIA_ERROR_LOG_INFO*)p_error_log->OutputData;
	std::stringstream log_details;
	log_details << ACPI_SMART_HEALTH_EVENT_TS_HEADER << std::hex << p_error_log->SystemTimestamp
				<< ACPI_SMART_HEALTH_EVENT_DPA_HEADER << std::hex << p_media_log->Dpa
				<< ACPI_SMART_HEALTH_EVENT_PDA_HEADER << std::hex << p_media_log->Pda
				<< ACPI_SMART_HEALTH_EVENT_RANGE_HEADER << std::hex << (unsigned int)p_media_log->Range
				<< ACPI_SMART_HEALTH_EVENT_ERROR_TYPE_HEADER << std::hex << (unsigned int)p_media_log->ErrorType
				//<< ACPI_SMART_HEALTH_EVENT_ERROR_FLAGS_HEADER << std::hex << (unsigned int)p_error_log->
				<< ACPI_SMART_HEALTH_EVENT_TRANS_TYPE_HEADER << std::hex << (unsigned int)p_media_log->TransactionType
				<< ACPI_SMART_HEALTH_EVENT_SEQ_NUM_HEADER << std::dec << p_media_log->SequenceNum;
	return log_details.str();
}

/*
* Send a system event that describes how many new FW error log entries were found.
* Should be visiable in Windows event viewer or Linux syslog depending on the running OS.
*
* @param[in] uid - target device in question
* @param[in] error_count - how many new events were found.
*/
void monitor::AcpiMonitor::sendFwErrCntSystemEventEntry(const NVM_UID device_uid,
	const NVM_UINT64 error_count)
{
	std::stringstream new_error_count;
	new_error_count << error_count;

	nvm_store_system_entry(LOG_SRC,
        SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_ERROR, SYSTEM_EVENT_CAT_MGMT_NUMB_4, false, true, true, false, 0),
		device_uid,
		ACPI_EVENT_MSG_FW_ERROR_CNT_INCREASED,
		error_count);
}

/*
* Send a system event that describes a singular error log entry.
* Should be visiable in Windows event viewer or Linux syslog depending on the running OS.
*
* @param[in] uid - target device in question
*/
void monitor::AcpiMonitor::sendFwErrLogSystemEventEntry(const NVM_UID device_uid, std::string log_type_level, std::string log_details)
{
	std::stringstream msg;
	msg << log_type_level << " " << log_details;

	nvm_store_system_entry(LOG_SRC,
        SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_ERROR, SYSTEM_EVENT_CAT_MGMT_NUMB_5, false, true, true, false, 0),
		device_uid,
		msg.str().c_str());
}

/*
*
* Helper func for creating a human readable event message that describes a smart temp
* event.
*
* @param[in] temp_data - smart temp data read from the dimm.
*/
std::string monitor::AcpiMonitor::createThermStr(THERMAL_ERROR_LOG_INFO * td)
{
	std::stringstream temp_details;
	temp_details << ACPI_SMART_HEALTH_EVENT_TEMP_CORE_STR;

	if (TEMP_USER_ALARM == td->Reported)
	{
		temp_details << ACPI_SMART_HEALTH_EVENT_USER_ALARM_TRIP_STR;
	}
	else if (TEMP_LOW == td->Reported)
	{
		temp_details << ACPI_SMART_HEALTH_EVENT_TEMP_LOW_STR;
	}
	else if (TEMP_HIGH == td->Reported)
	{
		temp_details << ACPI_SMART_HEALTH_EVENT_TEMP_HIGH_STR;
	}
	else if (TEMP_CRIT == td->Reported)
	{
		temp_details << ACPI_SMART_HEALTH_EVENT_TEMP_CRITICAL_STR;
	}

	temp_details << ACPI_SMART_HEALTH_EVENT_TEMP_STR;

	temp_details << std::dec << td->Temperature;
	return temp_details.str().c_str();
}
