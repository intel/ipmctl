/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include "NvmTypes.h"
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

/*This should match the error_type definition in nvm_management.h*/
#define ERROR_INJ_POISON          0X01
#define ERROR_INJ_TEMPERATURE     0X02
#define ERROR_INJ_PACKAGE_SPARING 0X03
#define ERROR_INJ_SPARE_CAPACITY  0X04
#define ERROR_INJ_FATAL_MEDIA_ERR 0X05
#define ERROR_INJ_DIRTY_SHUTDOWN  0X06
#define ERROR_INJ_TYPE_INVALID    0x08

/**
  The device path type for our driver and HII driver.
**/
typedef struct {
  VENDOR_DEVICE_PATH VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL End;
} VENDOR_END_DEVICE_PATH;

#define PMEMDEV_INITIALIZER(DEV) \
      InitializeListHead(DEV.Dimms); \
      InitializeListHead(DEV.UninitializedDimms); \
      InitializeListHead(DEV.ISs); \
      InitializeListHead(DEV.Namespaces);

#define DIMM_BSR_MAJOR_NO_POST_CODE 0x0
#define DIMM_BSR_MAJOR_CHECKPOINT_INIT_FAILURE 0xA1 // Unrecoverable FW error
#define DIMM_BSR_MAJOR_CHECKPOINT_CPU_EXCEPTION 0xE1 // CPU exception
#define DIMM_BSR_MAJOR_CHECKPOINT_INIT_COMPLETE 0xF0 // FW initialization complete
#define DIMM_BSR_MAILBOX_READY  0x1
#define DIMM_BSR_MAILBOX_NOT_READY 0x0
#define DIMM_BSR_MEDIA_NOT_TRAINED 0x0
#define DIMM_BSR_MEDIA_TRAINED 0x1
#define DIMM_BSR_MEDIA_ERROR 0x2
#define DIMM_BSR_MEDIA_DISABLED 0x1

// @todo Remove FIS 1.4 backwards compatibility workaround
// FIS <= 1.4
#define DIMM_BSR_AIT_DRAM_READY 0x1

// FIS >= 1.5
#define DIMM_BSR_AIT_DRAM_NOTTRAINED 0x0
#define DIMM_BSR_AIT_DRAM_TRAINED_NOTLOADED 0x1
#define DIMM_BSR_AIT_DRAM_ERROR 0x2
#define DIMM_BSR_AIT_DRAM_TRAINED_LOADED_READY 0x3

#define DIMM_BSR_OIE_ENABLED 0x1
#define DIMM_BSR_FW_ASSERT 0x1
#define DIMM_BSR_MEDIA_INTERFACE_ENGINE_STALLED 0x01

#define REGISTER_BSR_STR  L"BSR"
#define REGISTER_OS_STR   L"OS"

// @todo Remove FIS 1.4 backwards compatibility workaround
typedef union {
  UINT64 AsUint64;
  struct {
    UINT64 Major : 8;
    UINT64 Minor : 8;
    UINT64 MR : 2;
    UINT64 DT : 1;
    UINT64 PCR : 1;
    UINT64 MBR : 1;
    UINT64 WTS : 1;
    UINT64 FRCF : 1;
    UINT64 CR : 1;
    UINT64 MD: 1;
    UINT64 OIE: 1;
    UINT64 OIWE: 1;
    UINT64 Rsvd : 5;
    UINT64 Assertion : 1;
    UINT64 MI_Stalled: 1;
    UINT64 DR: 1;
    UINT64 Rsvd1 : 29;
  } Separated_FIS_1_4;
  struct {
    UINT64 Major : 8;
    UINT64 Minor : 8;
    UINT64 MR : 2;
    UINT64 DT : 1;
    UINT64 PCR : 1;
    UINT64 MBR : 1;
    UINT64 WTS : 1;
    UINT64 FRCF : 1;
    UINT64 CR : 1;
    UINT64 MD: 1;
    UINT64 OIE: 1;
    UINT64 OIWE: 1;
    UINT64 DR: 2;
    UINT64 Rsvd: 3;
    UINT64 Assertion : 1;
    UINT64 MI_Stalled: 1;
    UINT64 Rsvd1: 30;
  } Separated_Current_FIS;
} DIMM_BSR;

typedef struct _SENSOR_INFO {
  BOOLEAN SpareBlocksValid;
  BOOLEAN MediaTemperatureValid;
  BOOLEAN ControllerTemperatureValid;
  BOOLEAN PercentageUsedValid;
  BOOLEAN MediaTemperatureTrip;
  BOOLEAN ControllerTemperatureTrip;
  BOOLEAN SpareBlockTrip;
  INT16 MediaTemperature;
  INT16 ControllerTemperature;
  UINT8 SpareCapacity;
  UINT8 WearLevel;
  UINT32 DirtyShutdowns;
  UINT8 LastShutdownStatus;
  UINT32 PowerOnTime;
  UINT32 UpTime;
  UINT64 PowerCycles;
  UINT8 FwErrorCount;
  UINT8 HealthStatus;
  UINT8 PercentageUsed;
  UINT32 MediaErrorCount;
  UINT32 ThermalErrorCount;
  INT16 ContrTempShutdownThresh;
  INT16 MediaTempShutdownThresh;
  INT16 MediaThrottlingStartThresh;
  INT16 MediaThrottlingStopThresh;
} SENSOR_INFO;

typedef struct {
  UINT8 Type;
  UINT8 State;
  UINT8 Enabled;
  INT64 Value;
  UINT8 SettableThresholds;
  UINT8 SupportedThresholds;
  INT64 NonCriticalThreshold;
  INT64 CriticalLowerThreshold;
  INT64 CriticalUpperThreshold;
  INT64 FatalThreshold;
} DIMM_SENSOR;

typedef struct _THERMAL_ERROR_LOG_PER_DIMM_INFO {
  INT16   Temperature;        //!< In celsius
  UINT8   Reported;           //!< Temperature being reported
  UINT16  SequenceNum;
  UINT8   Reserved[2];
} THERMAL_ERROR_LOG_INFO;

typedef struct _MEDIA_ERROR_LOG_PER_DIMM_INFO {
  UINT64  Dpa;                //!< Specifies DPA address of error
  UINT64  Pda;                //!< Specifies PDA address of the failure
  UINT8   Range;              //!< Specifies the length in address space of this error.
  UINT8   ErrorType;          //!< Indicates what kind of error was logged.
  UINT8   PdaValid;           //!< Indicates the PDA address is valid.
  UINT8   DpaValid;           //!< Indicates the DPA address is valid.
  UINT8   Interrupt;          //!< Indicates this error generated an interrupt packet
  UINT8   Viral;              //!< Indicates Viral was signaled for this error
  UINT8   TransactionType;
  UINT16  SequenceNum;
  UINT8   Reserved[2];
} MEDIA_ERROR_LOG_INFO;

#define THERMAL_ERROR 0
#define MEDIA_ERROR 1

#define FW_MAX_DEBUG_LOG_LEVEL         4
#define FW_MIN_DEBUG_LOG_LEVEL	       1

#endif /** _TYPES_H_ **/
