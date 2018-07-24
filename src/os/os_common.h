/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OS_COMMON_H_
#define	OS_COMMON_H_

extern VOID
EFIAPI
DebugPrint(
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  ...
);

#define OS_DEBUG_VERBOSE   0x00400000
#define OS_DEBUG_INFO      0x00000040
#define OS_DEBUG_WARN      0x00000002
#define OS_DEBUG_ERROR     0x80000000

#define OS_NVDIMM_VERB(fmt, ...)  \
  DebugPrint(OS_DEBUG_VERBOSE, "NVDIMM-VERB:%s::%s:%d: " fmt "\n", \
    FileFromPath(__FILE__), __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define OS_NVDIMM_DBG(fmt, ...)  \
  DebugPrint(OS_DEBUG_INFO, "NVDIMM-DBG:%s::%s:%d: " fmt "\n", \
    FileFromPath(__FILE__), __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define OS_NVDIMM_DBG_CLEAN(fmt, ...)  \
  DebugPrint(OS_DEBUG_INFO, fmt, ## __VA_ARGS__)

#define OS_NVDIMM_WARN(fmt, ...) \
  DebugPrint(OS_DEBUG_WARN, "NVDIMM-WARN:%s::%s:%d: " fmt "\n", \
    FileFromPath(__FILE__), __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define OS_NVDIMM_ERR(fmt, ...)  \
  DebugPrint(OS_DEBUG_ERROR, "NVDIMM-ERR:%s::%s:%d: " fmt "\n", \
    FileFromPath(__FILE__), __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define OS_NVDIMM_CRIT(fmt, ...) \
  DebugPrint(OS_DEBUG_ERROR, "NVDIMM-ERR:%s::%s:%d: " fmt "\n", \
    FileFromPath(__FILE__), __FUNCTION__, __LINE__, ## __VA_ARGS__)

#endif //OS_COMMON_H_
