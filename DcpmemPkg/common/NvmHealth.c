/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <NvmTypes.h>
#include <Library/UefiLib.h>
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
    DimmSensorsSet[Index].State = SENSOR_STATE_NORMAL;
    DimmSensorsSet[Index].Enabled = SENSOR_NA_ENABLED;
    DimmSensorsSet[Index].SettableThresholds = ThresholdNone;
    DimmSensorsSet[Index].SupportedThresholds = ThresholdNone;
  }

  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].SettableThresholds = ThresholdUpperNonCritical;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].SupportedThresholds =
      ThresholdUpperNonCritical | ThresholdUpperFatal;

  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].SettableThresholds = ThresholdUpperNonCritical;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].SupportedThresholds =
      ThresholdUpperNonCritical | ThresholdLowerCritical | ThresholdUpperCritical | ThresholdUpperFatal;

  DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].SettableThresholds = ThresholdLowerNonCritical;
  DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].SupportedThresholds = ThresholdLowerNonCritical;
}

EFI_STATUS
GetSensorsInfo(
  IN     EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 DimmID,
  IN OUT DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT]
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT8 Index = 0;
  SENSOR_INFO SensorInfo;
  INT16 Threshold = 0;
  UINT8 DimmHealthState = 0;

  ZeroMem(&SensorInfo, sizeof(SensorInfo));

  /**
    Driver fills the data partially, so the initializer stays with the proper
    sensor types and default data.
  **/
  InitSensorsSet(DimmSensorsSet);

  ReturnCode = pNvmDimmConfigProtocol->GetSmartAndHealth(pNvmDimmConfigProtocol, DimmID, &SensorInfo, NULL, NULL, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Copy SMART & Health values **/
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].Value = SensorInfo.MediaTemperature;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].Value = SensorInfo.ControllerTemperature;
  DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].Value = SensorInfo.PercentageRemaining;
  DimmSensorsSet[SENSOR_TYPE_WEAR_LEVEL].Value = SensorInfo.WearLevel;
  DimmSensorsSet[SENSOR_TYPE_POWER_CYCLES].Value = SensorInfo.PowerCycles;
  DimmSensorsSet[SENSOR_TYPE_POWER_ON_TIME].Value = SensorInfo.PowerOnTime;
  DimmSensorsSet[SENSOR_TYPE_DIRTY_SHUTDOWNS].Value = SensorInfo.DirtyShutdowns;
  DimmSensorsSet[SENSOR_TYPE_FW_ERROR_COUNT].Value = SensorInfo.MediaErrorCount + SensorInfo.ThermalErrorCount;
  DimmSensorsSet[SENSOR_TYPE_UP_TIME].Value = SensorInfo.UpTime;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].CriticalLowerThreshold = SensorInfo.MediaThrottlingStopThresh;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].CriticalUpperThreshold = SensorInfo.MediaThrottlingStartThresh;
  DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].FatalThreshold = SensorInfo.MediaTempShutdownThresh;
  DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].FatalThreshold = SensorInfo.ContrTempShutdownThresh;

  /** Determine Health State based on Health Status Bit Mask **/
  ConvertHealthBitmask(SensorInfo.HealthStatus, &DimmHealthState);
  DimmSensorsSet[SENSOR_TYPE_DIMM_HEALTH].Value = DimmHealthState;

  /** Determine sensor state **/

  if (!SensorInfo.MediaTemperatureValid) {
    DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].State = SENSOR_STATE_UNKNOWN;
  } else if (SensorInfo.MediaTemperature >= SensorInfo.MediaTempShutdownThresh) {
    DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].State = SENSOR_STATE_FATAL;
  } else if (SensorInfo.MediaTemperature >= SensorInfo.MediaThrottlingStartThresh) {
    DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].State = SENSOR_STATE_CRITICAL;
  } else if (SensorInfo.MediaTemperatureTrip) {
    DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].State = SENSOR_STATE_NON_CRITICAL;
  } else {
    DimmSensorsSet[SENSOR_TYPE_MEDIA_TEMPERATURE].State = SENSOR_STATE_NORMAL;
  }

  if (!SensorInfo.ControllerTemperatureValid) {
    DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].State = SENSOR_STATE_UNKNOWN;
  } else if (SensorInfo.ControllerTemperature >= SensorInfo.ContrTempShutdownThresh) {
    DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].State = SENSOR_STATE_FATAL;
  } else if (SensorInfo.ControllerTemperatureTrip) {
    DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].State = SENSOR_STATE_NON_CRITICAL;
  } else {
    DimmSensorsSet[SENSOR_TYPE_CONTROLLER_TEMPERATURE].State = SENSOR_STATE_NORMAL;
  }

  if (!SensorInfo.SpareBlocksValid) {
    DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].State = SENSOR_STATE_UNKNOWN;
  } else if (SensorInfo.PercentageRemainingTrip) {
    DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].State = SENSOR_STATE_NON_CRITICAL;
  } else {
    DimmSensorsSet[SENSOR_TYPE_PERCENTAGE_REMAINING].State = SENSOR_STATE_NORMAL;
  }

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

    DimmSensorsSet[Index].NonCriticalThreshold = Threshold;
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
    case SENSOR_TYPE_WEAR_LEVEL:
      return WEAR_LEVEL_STR;
    case SENSOR_TYPE_POWER_CYCLES:
      return POWER_CYCLES_STR;
    case SENSOR_TYPE_POWER_ON_TIME:
      return POWER_ON_TIME_STR;
    case SENSOR_TYPE_DIRTY_SHUTDOWNS:
      return DIRTY_SHUTDOWNS_STR;
    case SENSOR_TYPE_FW_ERROR_COUNT:
      return FW_ERROR_COUNT_STR;
    case SENSOR_TYPE_UP_TIME:
      return UPTIME_STR;
    case SENSOR_TYPE_DIMM_HEALTH:
      return DIMM_HEALTH_STR;
    default:
      return L"Unknown";
  }
}

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
  )
{
  switch (SensorState) {
    case SENSOR_STATE_NORMAL:
      return STATE_NORMAL_STR;
    case SENSOR_STATE_NON_CRITICAL:
      return STATE_NON_CRITICAL_STR;
    case SENSOR_STATE_CRITICAL:
      return STATE_CRITICAL_STR;
    case SENSOR_STATE_FATAL:
      return STATE_FATAL_STR;
    case SENSOR_STATE_UNKNOWN:
    default:
      return STATE_UNKNOWN_STR;
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
      return TEMPERATURE_MSR;
    case SENSOR_TYPE_PERCENTAGE_REMAINING:
      return SPARE_CAPACITY_MSR;
    case SENSOR_TYPE_WEAR_LEVEL:
      return WEAR_LEVEL_MSR;
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
  } else {
    if ((SensorThresholdsType & ThresholdLowerNonCritical) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_LOWER_NON_CRITICAL_STR);
    }

    if ((SensorThresholdsType & ThresholdUpperNonCritical) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_UPPER_NON_CRITICAL_STR);
    }

    if ((SensorThresholdsType & ThresholdLowerCritical) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_LOWER_CRITICAL_STR);
    }

    if ((SensorThresholdsType & ThresholdUpperCritical) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_UPPER_CRITICAL_STR);
    }

    if ((SensorThresholdsType & ThresholdUpperFatal) != 0) {
      pFormat = (pStr == NULL) ? FORMAT_STR : L"," FORMAT_STR;
      pStr = CatSPrintClean(pStr, pFormat, THRESHOLD_UPPER_FATAL_STR);
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
  if (HealthMask & ControllerHealthStatusFatal) {
    *pHealthState = HEALTH_FATAL_FAILURE;
  } else if (HealthMask & ControllerHealthStatusCritical) {
    *pHealthState = HEALTH_CRITICAL_FAILURE;
  } else if (HealthMask & ControllerHealthStatusNoncritical) {
    *pHealthState = HEALTH_NON_CRITICAL_FAILURE;
  } else if (HealthMask == CONTROLLER_HEALTH_NORMAL) {
    *pHealthState = HEALTH_HEALTHY;
  } else {
    *pHealthState = HEALTH_UNKNOWN;
  }
}

/**
  Convert health state to a string

  @param[in] Health State - Numeric Value of the Health State.
      Defined in NvmTypes.h
**/
CHAR16*
SensorHealthToString(
  IN     UINT8 HealthState
  )
{
  CHAR16 *pHealthString = NULL;
  switch (HealthState) {
    case HEALTH_HEALTHY:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHY_STATE_STR);
      break;
    case HEALTH_NON_CRITICAL_FAILURE:
      pHealthString = CatSPrint(NULL, FORMAT_STR, NON_CRITICAL_FAILURE_STATE_STR);
      break;
    case HEALTH_CRITICAL_FAILURE:
      pHealthString = CatSPrint(NULL, FORMAT_STR, CRITICAL_FAILURE_STATE_STR);
      break;
    case HEALTH_FATAL_FAILURE:
      pHealthString = CatSPrint(NULL, FORMAT_STR, FATAL_ERROR_STATE_STR);
      break;
    case HEALTH_UNKNOWN:
    default:
      pHealthString = CatSPrint(NULL, FORMAT_STR, UNKNOWN_STR);
      break;
  }
  return pHealthString;
}
