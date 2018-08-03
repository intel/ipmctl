/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef NVM_API_TESTS_H
#define NVM_API_TESTS_H


#include <gtest/gtest.h>
#include <nvm_management.h>
#include <wchar.h> 

class NvmApi_Tests : public ::testing::Test
{
public:
};

TEST_F(NvmApi_Tests, GetPmonRegs)
{
  device_discovery *p_devices = (device_discovery *)malloc(sizeof(device_discovery));
  NVM_UINT8 SmartDataMask;

  nvm_get_devices(p_devices, 1);
  //Valid SmartDataMask 0x0 to 0x3
  for (SmartDataMask = 0; SmartDataMask < 4; SmartDataMask++)
  {
    PMON_REGISTERS *p_output_payload = (PMON_REGISTERS *)malloc(sizeof(PMON_REGISTERS));

    EXPECT_EQ(nvm_get_pmon_registers(p_devices->uid, SmartDataMask, p_output_payload), NVM_SUCCESS);

    free(p_output_payload);
  }

  free(p_devices);
}

/*In order to run this test enable error injection in BIOS settings PlatformConfiguration->system event log->error injection*/
//TEST_F(NvmApi_Tests, SetAndClearErrorInjection)
//{
//  device_discovery *p_devices = (device_discovery *)malloc(sizeof(device_discovery));
//  nvm_get_devices(p_devices, 1);
//  device_error de;
//  de.type = ERROR_TYPE_TEMPERATURE;
//  de.temperature = 34;
//  de.memory_type = POISON_MEMORY_TYPE_APPDIRECT;
//  EXPECT_EQ(nvm_inject_device_error(p_devices->uid, &de), NVM_SUCCESS);
//  de.temperature = 50;
//  EXPECT_EQ(nvm_clear_injected_device_error(p_devices->uid, &de), NVM_SUCCESS);
//  free(p_devices);
//}

TEST_F(NvmApi_Tests, SetPmonRegs)
{
  device_discovery *p_devices = (device_discovery *)malloc(sizeof(device_discovery));
  NVM_UINT8 PMONGroupEnable;

  nvm_get_devices(p_devices, 1);
  //Valid PMON groups from 0xA to 0xF
  for (PMONGroupEnable = 10; PMONGroupEnable < 16; PMONGroupEnable++)
  {
    EXPECT_EQ(nvm_set_pmon_registers(p_devices->uid, PMONGroupEnable), NVM_SUCCESS);
  }

  free(p_devices);

}

TEST_F(NvmApi_Tests, GetDeviceStatus)
{
  device_discovery *p_devices = (device_discovery *)malloc(sizeof(device_discovery));

  nvm_get_devices(p_devices, 1);
  device_status *p_status = (device_status *)malloc(sizeof(device_status));

  EXPECT_EQ(nvm_get_device_status(p_devices->uid, p_status), NVM_SUCCESS);

  free(p_status);

  free(p_devices);
}

TEST_F(NvmApi_Tests, GetDimmIdPassThru)
{
  struct device_pt_cmd get_dimm_id_pt;

  device_discovery *p_devices = (device_discovery *)malloc(sizeof(device_discovery));

  nvm_get_devices(p_devices, 1);
  get_dimm_id_pt.opcode = 0x1;
  get_dimm_id_pt.sub_opcode = 0x0;
  get_dimm_id_pt.output_payload_size = 128;
  get_dimm_id_pt.input_payload_size = 0;
  get_dimm_id_pt.large_input_payload_size = 0;
  get_dimm_id_pt.large_output_payload_size = 0;

  get_dimm_id_pt.output_payload = malloc(128);
  int status = nvm_send_device_passthrough_cmd(p_devices->uid, &get_dimm_id_pt);
}

TEST_F(NvmApi_Tests, GetRegions)
{
  NVM_UINT8 count;
  nvm_get_number_of_regions(&count);
  region *p_region = (region *)malloc(sizeof(region)*count);

  EXPECT_EQ(nvm_get_regions(p_region, &count), NVM_SUCCESS);

  free(p_region);

}

TEST_F(NvmApi_Tests, GetFwErrorLogEntry)
{
  struct device_pt_cmd get_dimm_id_pt;
  ERROR_LOG entry;

  device_discovery *p_devices = (device_discovery *)malloc(sizeof(device_discovery));

  nvm_get_devices(p_devices, 1);
  //media/low
  int status = nvm_get_fw_error_log_entry_cmd(p_devices->uid, 1, 0, 0, &entry);
  if(status == NVM_SUCCESS)
  { 
    MEDIA_ERROR_LOG *media_log = (MEDIA_ERROR_LOG*)entry.OutputData;
  }
  else if (status == NVM_SUCCESS_NO_ERROR_LOG_ENTRY)
  {
    //no media/low entry
  }

  //media/high
  status = nvm_get_fw_error_log_entry_cmd(p_devices->uid, 1, 1, 0, &entry);
  if (status == NVM_SUCCESS)
  {
    MEDIA_ERROR_LOG *media_log = (MEDIA_ERROR_LOG*)entry.OutputData;
  }
  else if (status == NVM_SUCCESS_NO_ERROR_LOG_ENTRY)
  {
    //no media/high entry
  }


  //theraml/low
  status = nvm_get_fw_error_log_entry_cmd(p_devices->uid, 1, 0, 1, &entry);
  if (status == NVM_SUCCESS)
  {
    THERMAL_ERROR_LOG *thermal_log = (THERMAL_ERROR_LOG*)entry.OutputData;
  }
  else if (status == NVM_SUCCESS_NO_ERROR_LOG_ENTRY)
  {
    //no thermal/low entry
  }

  //theraml/high
  status = nvm_get_fw_error_log_entry_cmd(p_devices->uid, 1, 1, 1, &entry);
  if (status == NVM_SUCCESS)
  {
    THERMAL_ERROR_LOG *thermal_log = (THERMAL_ERROR_LOG*)entry.OutputData;
  }
  else if (status == NVM_SUCCESS_NO_ERROR_LOG_ENTRY)
  {
    //no thermal/high entry
  }

  free(p_devices);
}

TEST_F(NvmApi_Tests, VerifyGetFwErrLogStatsReturnsErrorWithInvalidParam)
{
  struct device_error_log_status error_log_stats;
  int retval = nvm_get_fw_err_log_stats("Asdfg", &error_log_stats);
  EXPECT_NE(retval, NVM_SUCCESS);
}

TEST_F(NvmApi_Tests, VerifyMemTopology)
{
  int count;
  int retval;
  struct memory_topology * mem_topo;

  retval = nvm_get_number_of_memory_topology_devices(&count);

  mem_topo = (struct memory_topology *)malloc((sizeof(struct memory_topology) * count));
  retval = nvm_get_memory_topology(mem_topo, count);


}

TEST_F(NvmApi_Tests, SetPreferences)
{
  int retval = nvm_set_user_preference("PERFORMANCE_MONITOR_INTERVAL_MINUTES","2");
}
#endif //NVM_API_TESTS_H
