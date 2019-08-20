/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <NvmTypes.h>
#include <Library/UefiLib.h>
#include <Library/HiiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include "NvmHealth.h"
#include <Protocol/DriverHealth.h>

/**
  Init sensors array with default values

  @param[in,out] DimmSensorsSet sensors array to fill with default values
**/
VOID
InitSensorsSet(
  IN OUT DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT]
  )
{
  UINT8 Index = 0;

  if (DimmSensorsSet == NULL) {
    return;
  }
  ZeroMem(DimmSensorsSet, sizeof(DIMM_SENSOR) * SENSOR_TYPE_COUNT);

  for (Index = 0; Index < SENSOR_TYPE_COUNT; ++Index) {
    DimmSensorsSet[Index].Type = Index;
    DimmSensorsSet[Index].Enabled = SENSOR_NA_ENABLED;
    DimmSensorsSet[Index].SettableThresholds = ThresholdNone;
    DimmSensorsSet[Index].SupportedThresholds = ThresholdNone;
  }

  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].SettableThresholds = AlarmThreshold;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].SupportedThresholds =
    AlarmThreshold | ShutdownThreshold;

  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].SettableThresholds = AlarmThreshold;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].SupportedThresholds =
    AlarmThreshold | ThrottlingStopThreshold | ThrottlingStartThreshold | ShutdownThreshold;

  DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].SettableThresholds = AlarmThreshold;
  DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].SupportedThresholds = AlarmThreshold;
}

EFI_STATUS
GetSensorsInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 DimmID,
  IN OUT DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT]
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT8 Index = 0;
  SMART_AND_HEALTH_INFO HealthInfo;
  INT16 Threshold = 0;
  UINT8 DimmHealthState = 0;

  ZeroMem(&HealthInfo, sizeof(HealthInfo));

  /**
    Driver fills the data partially, so the initializer stays with the proper
    sensor types and default data.
  **/
  InitSensorsSet(DimmSensorsSet);

  ReturnCode = pNvmDimmConfigProtocol->GetSmartAndHealth(pNvmDimmConfigProtocol, DimmID, &HealthInfo);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Copy SMART & Health values **/
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].Value = HealthInfo.MediaTemperature;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].ThrottlingStopThreshold = HealthInfo.MediaThrottlingStopThresh;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].ThrottlingStartThreshold = HealthInfo.MediaThrottlingStartThresh;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].ShutdownThreshold = HealthInfo.MediaTempShutdownThresh;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].Value = HealthInfo.ControllerTemperature;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].ShutdownThreshold = HealthInfo.ContrTempShutdownThresh;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].ThrottlingStopThreshold = HealthInfo.ControllerThrottlingStopThresh;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].ThrottlingStartThreshold = HealthInfo.ControllerThrottlingStartThresh;
  DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].Value = HealthInfo.PercentageRemaining;
  DimmSensorsSet[SENSOR_TYPE_POWER_CYCLES].Value = HealthInfo.PowerCycles;
  DimmSensorsSet[SENSOR_TYPE_POWER_ON_TIME].Value = HealthInfo.PowerOnTime;
  DimmSensorsSet[SENSOR_TYPE_LATCHED_DIRTY_SHUTDOWN_COUNT].Value = HealthInfo.LatchedDirtyShutdownCount;
  DimmSensorsSet[SENSOR_TYPE_UNLATCHED_DIRTY_SHUTDOWN_COUNT].Value = HealthInfo.UnlatchedDirtyShutdownCount;
  DimmSensorsSet[SENSOR_TYPE_FW_ERROR_COUNT].Value = HealthInfo.MediaErrorCount + HealthInfo.ThermalErrorCount;
  DimmSensorsSet[SENSOR_TYPE_UP_TIME].Value = HealthInfo.UpTime;

  DimmSensorsSet[SENSOR_TYPE_MAX_MEDIA_TEMPERATURE].Value = HealthInfo.MaxMediaTemperature;
  DimmSensorsSet[SENSOR_TYPE_MAX_CONTROLLER_TEMPERATURE].Value = HealthInfo.MaxControllerTemperature;

  /** Determine Health State based on Health Status Bit Mask **/
  ConvertHealthBitmask(HealthInfo.HealthStatus, &DimmHealthState);
  DimmSensorsSet[SENSOR_TYPE_DIMM_HEALTH].Value = DimmHealthState;

  for (Index = SENSOR_TYPE_MEDIA_TEMPERATURE; Index <= SENSOR_TYPE_PERCENTAGE_REMAINING; ++Index) {
    ReturnCode = pNvmDimmConfigProtocol->GetAlarmThresholds(
        pNvmDimmConfigProtocol,
        DimmID,
        Index,
        &Threshold,
        &DimmSensorsSet[Index].Enabled,
        NULL);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    DimmSensorsSet[Index].AlarmThreshold = Threshold;
  }
Finish:
  return ReturnCode;
}

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
  )
{
  switch (SensorType) {
    case SENSOR_TYPE_MEDIA_TEMPERATURE:
      return MEDIA_TEMPERATURE_STR;
    case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
      return CONTROLLER_TEMPERATURE_STR;
    case SENSOR_TYPE_PERCENTAGE_REMAINING:
      return SPARE_CAPACITY_STR;
    case SENSOR_TYPE_POWER_CYCLES:
      return POWER_CYCLES_STR;
    case SENSOR_TYPE_POWER_ON_TIME:
      return POWER_ON_TIME_STR;
    case SENSOR_TYPE_LATCHED_DIRTY_SHUTDOWN_COUNT:
      return LATCHED_DIRTY_SHUTDOWN_COUNT_STR;
    case SENSOR_TYPE_FW_ERROR_COUNT:
      return FW_ERROR_COUNT_STR;
    case SENSOR_TYPE_UP_TIME:
      return UPTIME_STR;
    case SENSOR_TYPE_DIMM_HEALTH:
      return DIMM_HEALTH_STR;
    case SENSOR_TYPE_UNLATCHED_DIRTY_SHUTDOWN_COUNT:
      return UNLATCHED_DIRTY_SHUTDOWN_COUNT_STR;
    case SENSOR_TYPE_MAX_MEDIA_TEMPERATURE:
      return  MAX_MEDIA_TEMPERATURE_STR;
    case SENSOR_TYPE_MAX_CONTROLLER_TEMPERATURE:
      return MAX_CONTROLLER_TEMPERATURE_STR;
    default:
      return L"Unknown";
  }
}

/**
  Assign unit of measure for each SensorType.

  @param[in] SensorType the enum sensor type.
    Default case provides scalar.
**/
CONST
CHAR16 *
SensorValueMeasure(
  IN     UINT8 SensorType
  )
{
  switch(SensorType) {
    case SENSOR_TYPE_MEDIA_TEMPERATURE:
    case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
    case SENSOR_TYPE_MAX_CONTROLLER_TEMPERATURE:
    case SENSOR_TYPE_MAX_MEDIA_TEMPERATURE:
      return TEMPERATURE_MSR;
    case SENSOR_TYPE_PERCENTAGE_REMAINING:
      return SPARE_CAPACITY_MSR;
    case SENSOR_TYPE_POWER_ON_TIME:
    case SENSOR_TYPE_UP_TIME:
      return TIME_MSR;
    default:
      return L"";
  }
}

/**
  Translate the SensorThresholdsType into its Unicode string representation.
  A returned string has to be freed by the caller.
**/
CHAR16 *
SensorThresholdsToString(
  IN     SensorThresholds SensorThresholdsType
)
{
  CHAR16 *pStr = NULL;
  CHAR16 *pFormat = NULL;

  if (SensorThresholdsType == ThresholdNone) {
    pStr = CatSPrintClean(pStr, FORMAT_STR, THRESHOLD_NONE_STR);
  }
  else {
    if ((SensorThresholdsType & AlarmThreshold) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_ALARM_STR);
    }

    if ((SensorThresholdsType & ThrottlingStopThreshold) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_THROTTLING_STOP_STR);
    }

    if ((SensorThresholdsType & ThrottlingStartThreshold) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_THROTTLING_START_STR);
    }

    if ((SensorThresholdsType & ShutdownThreshold) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_SHUTDOWN_STR);
    }
  }

  return pStr;
}

/**
  Translate the Enabled into its Unicode string representation.
  The string buffer is static and the returned string is const so the
  caller should not make changes to the returned buffer.
**/
CONST
CHAR16 *
SensorEnabledStateToString(
  IN     UINT8 SensorState
  )
{
  switch (SensorState) {
    case SENSOR_ENABLED:
      return SENSOR_ENABLED_STATE_ENABLED_STR;
    case SENSOR_DISABLED:
      return SENSOR_ENABLED_STATE_DISABLED_STR;
    case SENSOR_NA_ENABLED:
      return NOT_APPLICABLE_SHORT_STR;
    default:
      return L"Unknown";
  }
}
/**
  Convert Health state bitmask to a defined state

  @param[in] HealthMask - mask from DIMM structure
  @param[out] pHealthState - pointer to output with defined Health State
**/
VOID
ConvertHealthBitmask(
  IN     UINT8 HealthMask,
     OUT UINT8 *pHealthState
  )
{
  if (HealthMask & HealthStatusFatal) {
    *pHealthState = HEALTH_FATAL_FAILURE;
  } else if (HealthMask & HealthStatusCritical) {
    *pHealthState = HEALTH_CRITICAL_FAILURE;
  } else if (HealthMask & HealthStatusNoncritical) {
    *pHealthState = HEALTH_NON_CRITICAL_FAILURE;
  }  else if (HealthMask == CONTROLLER_HEALTH_NORMAL) {
    *pHealthState = HEALTH_HEALTHY;
  } else {
    *pHealthState = HEALTH_UNKNOWN;
  }
}

/**
  Convert dimm or sensor health state to a string. The caller is responsible for
  freeing the returned string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Health State - Numeric Value of the Health State.
      Defined in NvmTypes.h

  @retval String representation of the health state
**/
EFI_STRING
HealthToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 HealthState
  )
{
  switch (HealthState) {
    case HEALTH_HEALTHY:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_HEALTHY), NULL);
    case HEALTH_NON_CRITICAL_FAILURE:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_NON_CRITICAL_FAILURE), NULL);
    case HEALTH_CRITICAL_FAILURE:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_CRITICAL_FAILURE), NULL);
    case HEALTH_FATAL_FAILURE:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_FATAL_FAILURE), NULL);
    case HEALTH_UNMANAGEABLE:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_UNMANAGEABLE), NULL);
    case HEALTH_NON_FUNCTIONAL:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_NON_FUNCTIONAL), NULL);
    case HEALTH_UNKNOWN:
    default:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_UNKNOWN), NULL);
  }
}
