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

#endif //NVM_API_TESTS_H
