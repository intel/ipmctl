/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
  * @file NvmHealth.h
  * @brief Health Types for EFI_NVMDIMMS_CONFIG_PROTOCOL to configure and manage DCPMMs.
  */

#ifndef _NVMHEALTH_H_
#define _NVMHEALTH_H_

#include "NvmInterface.h"

#define SPARE_CAPACITY_STR                    L"PercentageRemaining"
#define CONTROLLER_TEMPERATURE_STR            L"ControllerTemperature"
#define MEDIA_TEMPERATURE_STR                 L"MediaTemperature"
#define POWER_ON_TIME_STR                     L"PowerOnTime"
#define LATCHED_DIRTY_SHUTDOWN_COUNT_STR      L"LatchedDirtyShutdownCount"
#define POWER_CYCLES_STR                      L"PowerCycles"
#define UPTIME_STR                            L"UpTime"
#define FW_ERROR_COUNT_STR                    L"FwErrorCount"
#define DIMM_HEALTH_STR                       L"Health"
#define UNLATCHED_DIRTY_SHUTDOWN_COUNT_STR    L"UnlatchedDirtyShutdownCount"

#define SENSORS_COMBINED \
  L"Health|MediaTemperature|ControllerTemperature|PercentageRemaining|LatchedDirtyShutdownCount|PowerOnTime|" \
  L"UpTime|PowerCycles|FwErrorCount|UnlatchedDirtyShutdownCount" \


#define TEMPERATURE_MSR     L"C"
#define SPARE_CAPACITY_MSR  L"%"
#define TIME_MSR            L"s"

#define THRESHOLD_NONE_STR                L"None"
#define THRESHOLD_ALARM_STR               L"AlarmThreshold"
#define THRESHOLD_THROTTLING_STOP_STR     L"ThrottlingStopThreshold"
#define THRESHOLD_THROTTLING_START_STR    L"ThrottlingStartThreshold"
#define THRESHOLD_SHUTDOWN_STR            L"ShutdownThreshold"

typedef enum {
  ThresholdNone = 0,
  AlarmThreshold = BIT0,
  ThrottlingStopThreshold = BIT1,
  ThrottlingStartThreshold = BIT2,
  ShutdownThreshold = BIT3
} SensorThresholds;

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

/** Overall DIMM Health Status */
enum HEALTH_STATUS {
  HealthStatusNoncritical = BIT0,  //!< Non-Critical (maintenance required)
  HealthStatusCritical = BIT1,     //!< Critcial (features or performance degraded due to failure)
  HealthStatusFatal = BIT2         //!< Fatal (data loss has occurred or is imminent)
};

#pragma pack(push)
#pragma pack(1)
/**
  Last shutdown status struct
**/
typedef union {
  UINT8 AllFlags;
  struct {
    UINT8 PmAdr                     : 1; //!< PM ADR Command received
    UINT8 PmS3                      : 1; //!< PM Se received
    UINT8 PmS5                      : 1; //!< PM S5 received
    UINT8 DdrtPowerFailure          : 1; //!< DDRT Power Fail Command received
    UINT8 PmicPowerLoss             : 1; //!< PMIC Power Loss
    UINT8 PmWarmReset               : 1; //!< PM Warm Reset received
    UINT8 ThermalShutdown           : 1; //!< Thermal Shutdown received
    UINT8 FwFlushComplete           : 1; //!< Flush Complete
  } Separated;
} LAST_SHUTDOWN_STATUS_DETAILS;

/**
  Last shutdown status extended struct
**/
typedef union {
  UINT8 Raw[3];
  struct {
    UINT16 ViralInterrupt              : 1; //!< Viral interrupt received
    UINT16 SurpriseClockStopInterrupt  : 1; //!< Surprise clock stop interrupt received
    UINT16 WriteDataFlushComplete      : 1; //!< Write Data Flush Complete
    UINT16 S4PowerState                : 1; //!< S4 Power State received
    UINT16 PMIdle                      : 1; //!< PM Idle received
    UINT16 DdrtSurpriseReset           : 1; //!< Surprise Reset received
    UINT16 EnhancedAdrFlushStatus      : 4; //!< eADR Flush Status
  } Separated;
} LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED;

/**
  Last shutdown status combined struct
  NvmDimmConfig combines the structs into one uint32 for CLI/HII
**/
typedef union {
  UINT32 AsUint32;
  struct {
    LAST_SHUTDOWN_STATUS_DETAILS LastShutdownStatus;
    LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED LastShutdownStatusExtended;
  } Combined;
} LAST_SHUTDOWN_STATUS_DETAILS_COMBINED;

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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
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
  Convert dimm or sensor health state to a string. The caller is responsible for
  freeing the returned string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] HealthState - Numeric Value of the Health State.
      Defined in NvmTypes.h

  @retval String representation of the health state
**/
EFI_STRING
HealthToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 HealthState
  );
#endif /** _NVMHEALTH_H_ **/
