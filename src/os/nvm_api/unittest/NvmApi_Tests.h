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

TEST_F(NvmApi_Tests, Dupa)
{
  NVM_UID uid;
  nvm_freezelock_device(uid);
}

#endif //NVM_API_TESTS_H
