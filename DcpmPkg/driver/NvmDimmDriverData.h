/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVMDIMM_DRIVER_DATA_H_
#define _NVMDIMM_DRIVER_DATA_H_

#include <Guid/HiiPlatformSetupFormset.h>
#include <Guid/HiiFormMapMethodGuid.h>
#include <NvmTypes.h>
#include <NvmLimits.h>
#include <Version.h>
#include <NvmHealth.h>

#define SKU_SECURITY_FEATURE_SET        0
#define SKU_OVERWRITE_DIMM_FEATURE_SET  1
#define SKU_CONTROLLED_COUNTRY          2

/**
  Security states bitmask
**/
#define SECURITY_MASK_ENABLED             BIT1
#define SECURITY_MASK_LOCKED              BIT2
#define SECURITY_MASK_FROZEN              BIT3
#define SECURITY_MASK_COUNTEXPIRED        BIT4
#define SECURITY_MASK_NOT_SUPPORTED       BIT5
#define SECURITY_MASK_MASTER_ENABLED      BIT8
#define SECURITY_MASK_MASTER_COUNTEXPIRED BIT9

/**
  Persistent Partition Settings masks
**/
#define PARTITION_ENABLE_MASK BIT0

extern EFI_GUID gNvmDimmDevicePathGuid;

/**
  DIMM GUID base.
  The GUID changes on each DIMM
**/
#define NVMDIMM_DRIVER_NGNVM_GUID \
  { 0xca5a7c11, 0x7278, 0x4bc3, {0x80, 0xcc, 0x40, 0x42, 0x70, 0x8d, 0x48, 0x6f }}

extern EFI_GUID gNvmDimmNgnvmGuid;

/**
  GUID for NvmDimmDriver Driver Variables for Get/Set via runtime services.
**/
#define NVMDIMM_DRIVER_NGNVM_VARIABLE_GUID \
  { 0x8986be7a, 0x212f, 0x427e, {0x81, 0xa5, 0x42, 0x0d, 0xab, 0xc7, 0x92, 0xdf}}

extern EFI_GUID gNvmDimmNgnvmVariableGuid;

extern EFI_GUID gIntelDimmConfigVariableGuid;

#endif /** _NVMDIMM_DRIVER_DATA_H_ **/
