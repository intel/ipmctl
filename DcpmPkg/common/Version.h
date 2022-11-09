/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
  Version number is BCD with format:
  MMmmRr
  M = Major version number
  m = Minor version number
  R = Release type (Alpha, Beta, Test, etc)
  r = Release version
**/
#ifndef _VERSION_H_
#define _VERSION_H_

#define VENDOR_ID          0x8086
#define PRODUCT_NAME       L"Intel(R) Optane(TM) Persistent Memory"

#define GENERAL_RELEASE             0
#define ALPHA_RELEASE               1
#define BETA_RELEASE                2
#define TEST_RELEASE                3

#define NVMDIMM_MAJOR_VERSION       3
#define NVMDIMM_MINOR_VERSION       0
#define NVMDIMM_MAJOR_API_VERSION   1
#define NVMDIMM_MINOR_API_VERSION   1

#ifdef OS_BUILD
#include <Version_OS_build.h>
#else
#include <Version_UEFI_build.h>
#endif

#define STRING_TO_WIDE(A, B) A##B
#define WIDEN_UP_STRING(A) STRING_TO_WIDE(L,#A)
#define DEFINE_TO_STRING(A) WIDEN_UP_STRING(A)

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)


#define NVMD_CONFIG_PROTOCOL_VERSION (UINT32)(((NVMDIMM_RELEASE_TYPE == TEST_RELEASE ? 1 : 0) << 31 | \
  (NVMDIMM_MINOR_VERSION & 0x7FFF) << 16 | (NVMDIMM_MAJOR_VERSION & 0xFFFF)))

#endif /** _VERSION_H_ **/
