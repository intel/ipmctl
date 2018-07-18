/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ShowSensorCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include "Common.h"
#include <Convert.h>
#include "NvmDimmCli.h"
#include <Library/BaseMemoryLib.h>
#include <NvmHealth.h>

#define SENSOR_TYPE_STR                   L"Type"
#define CURRENT_VALUE_STR                 L"CurrentValue"
#define CURRENT_STATE_STR                 L"CurrentState"
#define LOWER_THRESHOLD_NON_CRITICAL_STR  L"LowerThresholdNonCritical"
#define UPPER_THRESHOLD_NON_CRITICAL_STR  L"UpperThresholdNonCritical"
#define LOWER_THRESHOLD_CRITICAL_STR      L"LowerThresholdCritical"
#define UPPER_THRESHOLD_CRITICAL_STR      L"UpperThresholdCritical"
#define UPPER_THRESHOLD_FATAL_STR         L"UpperThresholdFatal"
#define SETABLE_THRESHOLDS_STR            L"SettableThresholds"
#define SUPPORTED_THRESHOLDS_STR          L"SupportedThresholds"
#define ENABLED_STATE_STR                 L"EnabledState"

/** Command syntax definition **/
struct Command ShowSensorCommand =
{
  SHOW_VERB,                                                          //!< verb
  {                                                                   //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {
  {SENSOR_TARGET, L"", SENSORS_COMBINED, TRUE, ValueOptional},
  {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired}        //!< targets
  },
  {
    {L"", L"", L"", FALSE, ValueOptional},
  },                                                                  //!< properties
  L"Show health statistics ",                                         //!< help
  ShowSensor
};

CHAR16 *mppAllowedShowSensorDisplayValues[] =
{
  DIMM_ID_STR,
  SENSOR_TYPE_STR,
  CURRENT_VALUE_STR,
  CURRENT_STATE_STR,
  LOWER_THRESHOLD_NON_CRITICAL_STR,
  UPPER_THRESHOLD_NON_CRITICAL_STR,
  LOWER_THRESHOLD_CRITICAL_STR,
  UPPER_THRESHOLD_CRITICAL_STR,
  UPPER_THRESHOLD_FATAL_STR,
  SETABLE_THRESHOLDS_STR,
  SUPPORTED_THRESHOLDS_STR,
  ENABLED_STATE_STR
};

/**
  Create the string from value for a sensor.

  param[in] Value is the value to be printed.
  param[in] SensorType - type of sensor
**/
STATIC
CHAR16 *
GetSensorValue(
  IN     INT64 Value,
  IN     UINT8 SensorType
  )
{
  CHAR16 *pReturnBuffer = NULL;

  pReturnBuffer = CatSPrintClean(pReturnBuffer, L"%lld" FORMAT_STR L"", Value, SensorValueMeasure(SensorType));

  return pReturnBuffer;
}

/**
  Execute the show sensor command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowSensor(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pDimmsValue = NULL;
  UINT32 DimmsCount = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 Index = 0;
  BOOLEAN DisplayOptionSet = FALSE;
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN Found = FALSE;
  CHAR16 *pDisplayValues = NULL;
  UINT32 Index2 = 0;
  CHAR16 *pTempBuff = NULL;
  UINT32 SensorToDisplay = SENSOR_TYPE_ALL;
  COMMAND_STATUS *pCommandStatus = NULL;
  DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT];
  CHAR16 *pTargetValue = NULL;
  struct {
      CHAR16 *pSensorStr;
      UINT32 Sensor;
  } Sensors[] = {
      {CONTROLLER_TEMPERATURE_STR, SENSOR_TYPE_CONTROLLER_TEMPERATURE},
      {MEDIA_TEMPERATURE_STR, SENSOR_TYPE_MEDIA_TEMPERATURE},
      {SPARE_CAPACITY_STR, SENSOR_TYPE_PERCENTAGE_REMAINING},
      {POWER_CYCLES_STR, SENSOR_TYPE_POWER_CYCLES},
      {POWER_ON_TIME_STR, SENSOR_TYPE_POWER_ON_TIME},
      {DIRTY_SHUTDOWNS_STR, SENSOR_TYPE_DIRTY_SHUTDOWNS},
      {UPTIME_STR, SENSOR_TYPE_UP_TIME},
      {FW_ERROR_COUNT_STR, SENSOR_TYPE_FW_ERROR_COUNT},
      {DIMM_HEALTH_STR, SENSOR_TYPE_DIMM_HEALTH}
  };
  UINT32 SensorsNum = ARRAY_SIZE(Sensors);
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  BOOLEAN ShowAllManageableDimmFound = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(DimmSensorsSet, sizeof(DimmSensorsSet));
  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmsCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pDimmsValue, pDimms, DimmsCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmsCount, pDimmIds, DimmIdsNum)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  if (DimmIdsNum == 0) {
    for (Index = 0; Index < DimmsCount; Index++) {
      if (pDimms[Index].ManageabilityState == MANAGEMENT_VALID_CONFIG) {
        ShowAllManageableDimmFound = TRUE;
        break;
      }
    }
    if (ShowAllManageableDimmFound == FALSE) {
      Print(FORMAT_STR_NL, CLI_INFO_NO_MANAGEABLE_DIMMS);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
  }

  /**
    The user has provided a sensor. Try to match it to our list.
    Return an error if the sensor name is invalid.
  **/
  pTargetValue = GetTargetValue(pCmd, SENSOR_TARGET);

  if (pTargetValue != NULL && StrLen(pTargetValue) > 0) {
    Found = FALSE;
    for (Index = 0; Index < SensorsNum; Index++) {
      if (StrICmp(pTargetValue, Sensors[Index].pSensorStr) == 0) {
        SensorToDisplay = Sensors[Index].Sensor;
        Found = TRUE;
        break;
      }
    }

    if (!Found) {
      Print(L"The provided sensor: " FORMAT_STR L" is not valid.\n", pTargetValue);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowSensorDisplayValues,
      ALLOWED_DISP_VALUES_COUNT(mppAllowedShowSensorDisplayValues),
      &AllOptionSet,
      &DisplayOptionSet,
      &pDisplayValues);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  // Print the table header
  if (!AllOptionSet && !DisplayOptionSet) {
    SetDisplayInfo(L"Sensor", TableView);
    Print(FORMAT_SHOW_SENSOR_HEADER,
      DIMM_ID_STR,
      SENSOR_TYPE_STR,
      CURRENT_VALUE_STR,
      CURRENT_STATE_STR);
  }
  else {
     SetDisplayInfo(L"Sensor", ListView2L);
  }

  for (Index = 0; Index < DimmsCount; Index++) {
    if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
      continue;
    }

    if (pDimms[Index].ManageabilityState != MANAGEMENT_VALID_CONFIG) {
      continue;
    }

    ReturnCode = GetSensorsInfo(pNvmDimmConfigProtocol, pDimms[Index].DimmID, DimmSensorsSet);
    if (EFI_ERROR(ReturnCode)) {
      /**
        We do not return on error. Just inform the user and skip to the next DIMM or end.
      **/
      Print(L"Failed to read the sensors or thresholds values from DIMM %d. Code: " FORMAT_EFI_STATUS "\n",
          pDimms[Index].DimmID, ReturnCode);
      continue;
    }

    ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    /**
        Always print the DimmID for each DIMM.
    **/
    if (AllOptionSet || DisplayOptionSet) {
      Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, DimmStr);
    }

    for (Index2 = 0; Index2 < SENSOR_TYPE_COUNT; Index2++) {
      if ((SensorToDisplay != SENSOR_TYPE_ALL
          && DimmSensorsSet[Index2].Type != SensorToDisplay)) {
        continue;
      }

      if (!AllOptionSet && !DisplayOptionSet) {
        pTempBuff = GetSensorValue(DimmSensorsSet[Index2].Value, DimmSensorsSet[Index2].Type);
        if (pTempBuff == NULL) {
          Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
          ReturnCode = EFI_OUT_OF_RESOURCES;
          goto Finish;
        }

        /**
         Health State
        **/
        if (ContainsValue(SensorTypeToString(DimmSensorsSet[Index2].Type), DIMM_HEALTH_STR)) {
            FREE_POOL_SAFE(pTempBuff);
            pTempBuff = HealthToString(gNvmDimmCliHiiHandle, (UINT8)DimmSensorsSet[Index2].Value);
            if (pTempBuff == NULL) {
              Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
              ReturnCode = EFI_OUT_OF_RESOURCES;
              goto Finish;
            }
        }

        Print(FORMAT_SHOW_SENSOR_HEADER,
            DimmStr,
            SensorTypeToString(DimmSensorsSet[Index2].Type),
            pTempBuff,
            SensorStateToString(DimmSensorsSet[Index2].State)
            );

        FREE_POOL_SAFE(pTempBuff);
      } else {
        /**
          Always print the Type for each selected sensor.
        **/
        Print(L"   ---" FORMAT_STR L"=" FORMAT_STR_NL,
            SENSOR_TYPE_STR,
            SensorTypeToString(DimmSensorsSet[Index2].Type));

        /**
          Value
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, CURRENT_VALUE_STR))) {
          pTempBuff = GetSensorValue(DimmSensorsSet[Index2].Value, DimmSensorsSet[Index2].Type);
         /**
           Only for Health State
          **/
          if (ContainsValue(SensorTypeToString(DimmSensorsSet[Index2].Type), DIMM_HEALTH_STR)) {
              FREE_POOL_SAFE(pTempBuff);
              pTempBuff = HealthToString(gNvmDimmCliHiiHandle, (UINT8)DimmSensorsSet[Index2].Value);
              if (pTempBuff == NULL) {
                Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
                ReturnCode = EFI_OUT_OF_RESOURCES;
                goto Finish;
              }
          }
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, CURRENT_VALUE_STR, pTempBuff);

          FREE_POOL_SAFE(pTempBuff);
        }
        /**
          State
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, CURRENT_STATE_STR))) {
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL,
              CURRENT_STATE_STR,
              SensorStateToString(DimmSensorsSet[Index2].State));
        }

        /**
          LowerThresholdNonCritical
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, LOWER_THRESHOLD_NON_CRITICAL_STR))) {
          switch (Index2) {
          case SENSOR_TYPE_PERCENTAGE_REMAINING:
            // Only percentage remaining sensor got lower non-critical threshold
            pTempBuff = GetSensorValue(DimmSensorsSet[Index2].NonCriticalThreshold, DimmSensorsSet[Index2].Type);
            break;
          default:
            pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
            break;
          }
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, LOWER_THRESHOLD_NON_CRITICAL_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          UpperThresholdNonCritical
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, UPPER_THRESHOLD_NON_CRITICAL_STR))) {
          switch (Index2) {
          case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
          case SENSOR_TYPE_MEDIA_TEMPERATURE:
            // Only Controller/Media temperature sensor got upper non-critical threshold
            pTempBuff = GetSensorValue(DimmSensorsSet[Index2].NonCriticalThreshold, DimmSensorsSet[Index2].Type);
            break;
          default:
            pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
            break;
          }
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, UPPER_THRESHOLD_NON_CRITICAL_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          LowerThresholdCritical
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, LOWER_THRESHOLD_CRITICAL_STR))) {
          switch (Index2) {
          case SENSOR_TYPE_MEDIA_TEMPERATURE:
            // Only Media temperature sensor got lower critical threshold
            pTempBuff = GetSensorValue(DimmSensorsSet[Index2].CriticalLowerThreshold, DimmSensorsSet[Index2].Type);
            break;
          default:
            pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
            break;
          }
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, LOWER_THRESHOLD_CRITICAL_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          UpperThresholdCritical
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, UPPER_THRESHOLD_CRITICAL_STR))) {
          switch (Index2) {
          case SENSOR_TYPE_MEDIA_TEMPERATURE:
            // Only Media temperature sensor got upper critical threshold
            pTempBuff = GetSensorValue(DimmSensorsSet[Index2].CriticalUpperThreshold, DimmSensorsSet[Index2].Type);
            break;
          default:
            pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
            break;
          }
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, UPPER_THRESHOLD_CRITICAL_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          UpperThresholdFatal
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, UPPER_THRESHOLD_FATAL_STR))) {
          switch (Index2) {
          case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
          case SENSOR_TYPE_MEDIA_TEMPERATURE:
            // Only Controller/Media temperature sensor got upper fatal threshold
            pTempBuff = GetSensorValue(DimmSensorsSet[Index2].FatalThreshold, DimmSensorsSet[Index2].Type);
            break;
          default:
            pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
            break;
          }
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, UPPER_THRESHOLD_FATAL_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          SettableThresholds
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, SETABLE_THRESHOLDS_STR))) {
          pTempBuff = SensorThresholdsToString(DimmSensorsSet[Index2].SettableThresholds);
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, SETABLE_THRESHOLDS_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          SupportedThresholds
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, SUPPORTED_THRESHOLDS_STR))) {
          pTempBuff = SensorThresholdsToString(DimmSensorsSet[Index2].SupportedThresholds);
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL, SUPPORTED_THRESHOLDS_STR, pTempBuff);
          FREE_POOL_SAFE(pTempBuff);
        }

        /**
          Enabled
        **/
        if (!DisplayOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, ENABLED_STATE_STR))) {
          Print(FORMAT_6SPACE_STR_EQ FORMAT_STR_NL,
              ENABLED_STATE_STR,
              SensorEnabledStateToString(DimmSensorsSet[Index2].Enabled));
        }
      }

      if (SensorToDisplay != SENSOR_TYPE_ALL) {
        break;
      }
    }
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDisplayValues);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the set sensor command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowSensorCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowSensorCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
