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
#define PRODUCT_NAME       L"Intel(R) Optane(TM) DC Persistent Memory"

#define GENERAL_RELEASE             0
#define ALPHA_RELEASE               1
#define BETA_RELEASE                2
#define TEST_RELEASE                3

#define NVMDIMM_MAJOR_VERSION       1
#define NVMDIMM_MINOR_VERSION       0
#define NVMDIMM_MAJOR_API_VERSION   1
#define NVMDIMM_MINOR_API_VERSION   0

#define NVMDIMM_RELEASE_TYPE        TEST_RELEASE
#define NVMDIMM_RELEASE_NUMBER      0

#define NVMDIMM_VERSION NVMDIMM_MAJOR_VERSION<<16 | \
  NVMDIMM_MINOR_VERSION<<8 | NVMDIMM_RELEASE_TYPE<<4 | \
  NVMDIMM_RELEASE_NUMBER

#define STRING_TO_WIDE(A, B) A##B
#define WIDEN_UP_STRING(A) STRING_TO_WIDE(L,#A)
#define DEFINE_TO_STRING(A) WIDEN_UP_STRING(A)

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

#ifndef OS_BUILD
#define NVMDIMM_VERSION_STRING \
  DEFINE_TO_STRING(NVMDIMM_MAJOR_VERSION) L"." \
  DEFINE_TO_STRING(NVMDIMM_MINOR_VERSION) L"." \
  DEFINE_TO_STRING(NVMDIMM_RELEASE_TYPE) L"." \
  DEFINE_TO_STRING(NVMDIMM_RELEASE_NUMBER)
#else
#define NVMDIMM_VERSION_STRING DEFINE_TO_STRING(__VERSION_NUMBER__)
#define NVMDIMM_VERSION_STRING_A STRINGIZE(__VERSION_NUMBER__)
#endif
/**
  Version number is BCD with format:
  MMmmRr
  M = Major version number
  m = Minor version number
  R = Release type (Alpha, Beta, Test, etc)
  r = Release version
**/
#define NVMDIMM_CONFIG_MAJOR_VERSION  1
#define NVMDIMM_CONFIG_MINOR_VERSION  0

/**
  Setting the proper release type based on the DEBUG/RELEASE build type.
**/
#define NVMDIMM_CONFIG_NORMAL_RELEASE 0
#define NVMDIMM_CONFIG_DEBUG_RELEASE   1
#ifndef MDEPKG_NDEBUG // If this is the debug build
#define NVMDIMM_CONFIG_RELEASE_TYPE   NVMDIMM_CONFIG_DEBUG_RELEASE
#else // Otherwise we have a release build
#define NVMDIMM_CONFIG_RELEASE_TYPE   NVMDIMM_CONFIG_NORMAL_RELEASE
#endif /**MDEPKG_NDEBUG **/

#define NVMD_CONFIG_PROTOCOL_VERSION (UINT32)(((NVMDIMM_RELEASE_TYPE == TEST_RELEASE ? 1 : 0) << 31 | \
  (NVMDIMM_MINOR_VERSION & 0x7FFF) << 16 | (NVMDIMM_MAJOR_VERSION & 0xFFFF)))

#endif /** _VERSION_H_ **/
