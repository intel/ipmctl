/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVMHEALTH_H_
#define _NVMHEALTH_H_

#include "NvmInterface.h"

#define SPARE_CAPACITY_STR              L"PercentageRemaining"
#define CONTROLLER_TEMPERATURE_STR      L"ControllerTemperature"
#define MEDIA_TEMPERATURE_STR           L"MediaTemperature"
#define WEAR_LEVEL_STR                  L"WearLevel"
#define POWER_ON_TIME_STR               L"PowerOnTime"
#define DIRTY_SHUTDOWNS_STR            L"DirtyShutdowns"
#define POWER_CYCLES_STR                L"PowerCycles"
#define UPTIME_STR                      L"UpTime"
#define FW_ERROR_COUNT_STR              L"FwErrorCount"
#define DIMM_HEALTH_STR                 L"Health"

#define SENSORS_COMBINED \
  L"Health|MediaTemperature|ControllerTemperature|PercentageRemaining|WearLevel|DirtyShutdowns|PowerOnTime|" \
  L"UpTime|PowerCycles|FwErrorCount"

#define TEMPERATURE_MSR     L"C"
#define SPARE_CAPACITY_MSR  L"%"
#define WEAR_LEVEL_MSR      L"%"
#define TIME_MSR            L"s"

#define STATE_NORMAL_STR        L"Normal"
#define STATE_NON_CRITICAL_STR  L"NonCritical"
#define STATE_CRITICAL_STR      L"Critical"
#define STATE_FATAL_STR         L"Fatal"
#define STATE_UNKNOWN_STR       L"Unknown"

#define THRESHOLD_NONE_STR                L"None"
#define THRESHOLD_LOWER_NON_CRITICAL_STR  L"LowerThresholdNonCritical"
#define THRESHOLD_UPPER_NON_CRITICAL_STR  L"UpperThresholdNonCritical"
#define THRESHOLD_LOWER_CRITICAL_STR      L"LowerThresholdCritical"
#define THRESHOLD_UPPER_CRITICAL_STR      L"UpperThresholdCritical"
#define THRESHOLD_UPPER_FATAL_STR         L"UpperThresholdFatal"

typedef enum {
  ThresholdNone = 0,
  ThresholdLowerNonCritical = BIT0,
  ThresholdUpperNonCritical = BIT1,
  ThresholdLowerCritical = BIT2,
  ThresholdUpperCritical = BIT3,
  ThresholdUpperFatal  = BIT4
} SensorThresholds;


/** health states **/
#define HEALTHY_STATE_STR               L"Healthy"
#define NON_CRITICAL_FAILURE_STATE_STR  L"Minor Failure"
#define CRITICAL_FAILURE_STATE_STR      L"Critical Failure"
#define FATAL_ERROR_STATE_STR           L"Non-recoverable error"
#define UNKNOWN_STR                     L"Unknown"
#define UNMANAGEABLE_STR                L"Unmanageable"
#define NONFUNCTIONAL_STR               L"Non-functional"

#define SENSOR_ENABLED_STATE_ENABLED_STR    L"1"
#define SENSOR_ENABLED_STATE_DISABLED_STR   L"0"

#define NOT_APPLICABLE_SHORT_STR                              L"N/A"

/**
  Namespace PM Capable values
**/
#define NAMESPACE_PM_CAPABLE_BLOCK_MODE_ONLY 0
#define NAMESPACE_PM_CAPABLE_PERSISTENT_MEM  1

#define TEMPERATURE_POSITIVE 0
#define TEMPERATURE_NEGATIVE 1

#define CONTROLLER_HEALTH_NORMAL 0

enum CONTROLLER_HEALTH_STATE {
  ControllerHealthStatusNoncritical = BIT0,  //!< maintenance required
  ControllerHealthStatusCritical = BIT1,     //!< features or performance degraded due to failure
  ControllerHealthStatusFatal = BIT2         //!< data loss has occurred or is imminent
};

#pragma pack(push)
#pragma pack(1)
/**
  Last shutdown status struct
**/
typedef union {
  UINT8 AllFlags;
  struct {
    UINT8 PmAdr                     : 1;
    UINT8 PmS3                      : 1;
    UINT8 PmS5                      : 1;
    UINT8 DdrtPowerFailure          : 1;
    UINT8 Pmic12VPowerFailure       : 1;
    UINT8 PmWarmReset               : 1;
    UINT8 ThermalShutdown           : 1;
    UINT8 FwFlushComplete           : 1;
  } Separated;
} LAST_SHUTDOWN_STATUS;

/**
  Last shutdown status extended struct
**/
typedef union {
  UINT8 Raw[3];
  struct {
    UINT8 ViralInterrupt              : 1;
    UINT8 SurpriseClockStopInterrupt  : 1;
    UINT8 WriteDataFlushComplete      : 1;
    UINT8 S4PowerState                : 1;
    UINT8                             : 4; //!< Reserved
    UINT8                             : 8; //!< Reserved
    UINT8                             : 8; //!< Reserved
  } Separated;
} LAST_SHUTDOWN_STATUS_EXTENDED;

/**
  Last shutdown status combined struct
  NvmDimmConfig combines the structs into one uint32 for CLI/HII
**/
typedef union {
  UINT32 AsUint32;
  struct {
    LAST_SHUTDOWN_STATUS LastShutdownStatus;
    LAST_SHUTDOWN_STATUS_EXTENDED LastShutdownStatusExtended;
  } Combined;
} LAST_SHUTDOWN_STATUS_COMBINED;

/**
  Temperature structure
**/
typedef union {
  UINT16 AsUint16;
  struct {
    UINT16 TemperatureValue  : 15; //< Unsigned, integer temperature in Celsius
    UINT16 Sign              : 1;  //< Sign Bit (1 - negative, 0 - positive)
  } Separated;
} TEMPERATURE;

#pragma pack(pop)

/**
  Init sensors array with default values

  @param[in,out] DimmSensorsSet sensors array to fill with default values
**/
VOID
InitSensorsSet(
  IN OUT DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT]
  );

EFI_STATUS
GetSensorsInfo(
  IN     EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 DimmID,
  IN OUT DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT]
  );

/**
  Translate the SensorType into its Unicode string representation.
  The string buffer is static and the returned string is const so the
  caller should not make changes to the returned buffer.

  @param[in] SensorType the enum sensor type.
    The SensorTypeAll will result in an "Unknown" return as this value
    is not translatable.
**/
CONST
CHAR16 *
SensorTypeToString(
  IN     UINT8 SensorType
  );

/**
  Translate the SensorState into its Unicode string representation.
  The string buffer is static and the returned string is const so the
  caller should not make changes to the returned buffer.

  @param[in] SensorState the enum sensor state.
**/
CONST
CHAR16 *
SensorStateToString(
  IN     UINT8 SensorState
  );

/**
  Assign unit of measure for each SensorType.

  @param[in] SensorType the enum sensor type.
    Default case provides scalar.
**/
CONST
CHAR16 *
SensorValueMeasure(
  IN     UINT8 SensorType
  );

/**
  Translate the SensorThresholdsType into its Unicode string representation.
  A returned string has to be freed by the caller.
**/
CHAR16 *
SensorThresholdsToString(
  IN     SensorThresholds SensorThresholdsType
  );

/**
  Translate the EnabledState into its Unicode string representation.
  The string buffer is static and the returned string is const so the
  caller should not make changes to the returned buffer.
**/
CONST
CHAR16 *
SensorEnabledStateToString(
  IN     UINT8 SensorState
  );

/**
  Convert Health state bitmask to a defined state

  @param[in] HealthMask - mask from DIMM structure
  @param[out] pHealthState - pointer to output with defined Health State
**/
VOID
ConvertHealthBitmask(
  IN     UINT8 HealthMask,
     OUT UINT8 *pHealthState
  );

/**
  Convert health state to a string

  @param[in] Health State - Numeric Value of the Health State.
      Defined in NvmTypes.h
**/
CHAR16*
SensorHealthToString(
  IN     UINT8 HealthState
  );
#endif /** _NVMHEALTH_H_ **/
