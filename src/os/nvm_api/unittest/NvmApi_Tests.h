/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef NVM_API_TESTS_H
#define NVM_API_TESTS_H


#include <gtest/gtest.h>
#include <nvm_management.h>

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

    nvm_get_pmon_registers(p_devices->uid, SmartDataMask, p_output_payload);

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
    nvm_set_pmon_registers(p_devices->uid, PMONGroupEnable);
  }

  free(p_devices);

}

#endif //NVM_API_TESTS_H
