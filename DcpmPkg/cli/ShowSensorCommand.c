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
#include <DataSet.h>
#include <Printer.h>

#define DIMM_ID_STR                       L"DimmID"
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

#define DS_ROOT_PATH                      L"/SensorList"
#define DS_DIMM_PATH                      L"/SensorList/Dimm"
#define DS_DIMM_INDEX_PATH                L"/SensorList/Dimm[%d]"
#define DS_SENSOR_PATH                    L"/SensorList/Dimm/Sensor"
#define DS_SENSOR_INDEX_PATH              L"/SensorList/Dimm[%d]/Sensor[%d]"
 /*
 *  PRINT LIST ATTRIBUTES (2 levels: Dimm-->Sensor)
 *  ---DimmId=0x0001---
 *     ---Type=Health
 *        CurrentVal=Healthy
 *        CurrentState=Normal
 *        ...
 */
PRINTER_LIST_ATTRIB ShowSensorListAttributes =
{
 {
    {
      DIMM_NODE_STR,                                                        //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",                        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,                           //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                                           //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      SENSOR_NODE_STR,                                                      //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" SENSOR_TYPE_STR L"=$(" SENSOR_TYPE_STR L")",   //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,           //NULL or KEY VAL FORMAT STR
      SENSOR_TYPE_STR                                                       //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

/*
*  PRINTER TABLE ATTRIBUTES (4 columns)
*   DimmID | Type | CurrentValue | CurrentState
*   ===========================================
*   0x0001 | X    | X            | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowSensorTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                                                  //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                                           //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR                                       //COLUMN DATA PATH
    },
    {
      SENSOR_TYPE_STR,                                                              //COLUMN HEADER
      SENSOR_TYPE_MAX_STR_WIDTH,                                                    //COLUMN MAX STR WIDTH
      DS_SENSOR_PATH PATH_KEY_DELIM SENSOR_TYPE_STR                                 //COLUMN DATA PATH
    },
    {
      CURRENT_VALUE_STR,                                                            //COLUMN HEADER
      SENSOR_VALUE_MAX_STR_WIDTH,                                                   //COLUMN MAX STR WIDTH
      DS_SENSOR_PATH PATH_KEY_DELIM CURRENT_VALUE_STR                               //COLUMN DATA PATH
    },
    {
      CURRENT_STATE_STR,                                                            //COLUMN HEADER
      SENSOR_STATE_MAX_STR_WIDTH,                                                   //COLUMN MAX STR WIDTH
      DS_SENSOR_PATH PATH_KEY_DELIM CURRENT_STATE_STR                               //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowSensorDataSetAttribs =
{
  &ShowSensorListAttributes,
  &ShowSensorTableAttributes
};

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
  {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional}        //!< targets
  },
  {
    {L"", L"", L"", FALSE, ValueOptional},
  },                                                                  //!< properties
  L"Show health statistics ",                                         //!< help
  ShowSensor,
  TRUE,                                                               //!< enable print control support
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
  UINT32 DimmIndex = 0;
  BOOLEAN Found = FALSE;
  UINT32 SensorIndex = 0;
  CHAR16 *pTempBuff = NULL;
  UINT32 SensorToDisplay = SENSOR_TYPE_ALL;
  COMMAND_STATUS *pCommandStatus = NULL;
  DIMM_SENSOR DimmSensorsSet[SENSOR_TYPE_COUNT];
  CHAR16 *pTargetValue = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;

  struct {
      CHAR16 *pSensorStr;
      UINT32 Sensor;
  } Sensors[] = {
      {CONTROLLER_TEMPERATURE_STR, SENSOR_TYPE_CONTROLLER_TEMPERATURE},
      {MEDIA_TEMPERATURE_STR, SENSOR_TYPE_MEDIA_TEMPERATURE},
      {SPARE_CAPACITY_STR, SENSOR_TYPE_PERCENTAGE_REMAINING},
      {POWER_CYCLES_STR, SENSOR_TYPE_POWER_CYCLES},
      {POWER_ON_TIME_STR, SENSOR_TYPE_POWER_ON_TIME},
      {LATCHED_DIRTY_SHUTDOWN_COUNT_STR, SENSOR_TYPE_LATCHED_DIRTY_SHUTDOWN_COUNT},
      {UPTIME_STR, SENSOR_TYPE_UP_TIME},
      {FW_ERROR_COUNT_STR, SENSOR_TYPE_FW_ERROR_COUNT},
      {DIMM_HEALTH_STR, SENSOR_TYPE_DIMM_HEALTH},
      {UNLATCHED_DIRTY_SHUTDOWN_COUNT_STR, SENSOR_TYPE_UNLATCHED_DIRTY_SHUTDOWN_COUNT}
  };
  UINT32 SensorsNum = ARRAY_SIZE(Sensors);
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  BOOLEAN ShowAllManageableDimmFound = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(DimmSensorsSet, sizeof(DimmSensorsSet));
  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  pDispOptions = AllocateZeroPool(sizeof(CMD_DISPLAY_OPTIONS));
  if (NULL == pDispOptions) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowSensorDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowSensorDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed on InitializeCommandStatus");
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmsCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pDimmsValue, pDimms, DimmsCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmsCount, pDimmIds, DimmIdsNum)){
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  if (DimmIdsNum == 0) {
    for (DimmIndex = 0; DimmIndex < DimmsCount; DimmIndex++) {
      if (pDimms[DimmIndex].ManageabilityState == MANAGEMENT_VALID_CONFIG) {
        ShowAllManageableDimmFound = TRUE;
        break;
      }
    }
    if (ShowAllManageableDimmFound == FALSE) {
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_MANAGEABLE_DIMMS);
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
    for (DimmIndex = 0; DimmIndex < SensorsNum; DimmIndex++) {
      if (StrICmp(pTargetValue, Sensors[DimmIndex].pSensorStr) == 0) {
        SensorToDisplay = Sensors[DimmIndex].Sensor;
        Found = TRUE;
        break;
      }
    }

    if (!Found) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"The provided sensor: " FORMAT_STR L" is not valid.\n", pTargetValue);
      goto Finish;
    }
  }

  for (DimmIndex = 0; DimmIndex < DimmsCount; DimmIndex++) {
    if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[DimmIndex].DimmID)) {
      continue;
    }

    if (pDimms[DimmIndex].ManageabilityState != MANAGEMENT_VALID_CONFIG) {
      continue;
    }

    ReturnCode = GetSensorsInfo(pNvmDimmConfigProtocol, pDimms[DimmIndex].DimmID, DimmSensorsSet);
    if (EFI_ERROR(ReturnCode)) {
      /**
        We do not return on error. Just inform the user and skip to the next DIMM or end.
      **/
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode,L"Failed to read the sensors or thresholds values from DIMM %d. Code: " FORMAT_EFI_STATUS "\n",
        pDimms[DimmIndex].DimmID, ReturnCode);
      continue;
    }

    ReturnCode = GetPreferredDimmIdAsString(pDimms[DimmIndex].DimmHandle, pDimms[DimmIndex].DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to translate DIMM identifier to string\n");
      goto Finish;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

    for (SensorIndex = 0; SensorIndex < SENSOR_TYPE_COUNT; SensorIndex++) {
      if ((SensorToDisplay != SENSOR_TYPE_ALL
          && DimmSensorsSet[SensorIndex].Type != SensorToDisplay)) {
        continue;
      }

      PRINTER_BUILD_KEY_PATH(pPath, DS_SENSOR_INDEX_PATH, DimmIndex, SensorIndex);

      /**
        Type
      **/
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SENSOR_TYPE_STR, SensorTypeToString(DimmSensorsSet[SensorIndex].Type));
      /**
        Value
      **/
      if (!pDispOptions->DisplayOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CURRENT_VALUE_STR))) {
        /**
          Only for Health State
        **/
        if (ContainsValue(SensorTypeToString(DimmSensorsSet[SensorIndex].Type), DIMM_HEALTH_STR)) {
            pTempBuff = HealthToString(gNvmDimmCliHiiHandle, (UINT8)DimmSensorsSet[SensorIndex].Value);
            if (pTempBuff == NULL) {
              ReturnCode = EFI_OUT_OF_RESOURCES;
              PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
              goto Finish;
            }
        }
        else {
          pTempBuff = GetSensorValue(DimmSensorsSet[SensorIndex].Value, DimmSensorsSet[SensorIndex].Type);
        }

        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CURRENT_VALUE_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }
      /**
        State
      **/
      if (!pDispOptions->DisplayOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CURRENT_STATE_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CURRENT_STATE_STR, SensorStateToString(DimmSensorsSet[SensorIndex].State));
      }

      /**
        LowerThresholdNonCritical
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, LOWER_THRESHOLD_NON_CRITICAL_STR))) {
        switch (SensorIndex) {
        case SENSOR_TYPE_PERCENTAGE_REMAINING:
          // Only spare capacity sensor got lower non-critical threshold
          pTempBuff = GetSensorValue(DimmSensorsSet[SensorIndex].NonCriticalThreshold, DimmSensorsSet[SensorIndex].Type);
          break;
        default:
          pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
          break;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, LOWER_THRESHOLD_NON_CRITICAL_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        UpperThresholdNonCritical
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, UPPER_THRESHOLD_NON_CRITICAL_STR))) {
        switch (SensorIndex) {
        case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
        case SENSOR_TYPE_MEDIA_TEMPERATURE:
          // Only Controller/Media temperature sensor got upper non-critical threshold
          pTempBuff = GetSensorValue(DimmSensorsSet[SensorIndex].NonCriticalThreshold, DimmSensorsSet[SensorIndex].Type);
          break;
        default:
          pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
          break;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, UPPER_THRESHOLD_NON_CRITICAL_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        LowerThresholdCritical
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, LOWER_THRESHOLD_CRITICAL_STR))) {
        switch (SensorIndex) {
		case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
        case SENSOR_TYPE_MEDIA_TEMPERATURE:
          // Only Media temperature sensor got lower critical threshold
          pTempBuff = GetSensorValue(DimmSensorsSet[SensorIndex].CriticalLowerThreshold, DimmSensorsSet[SensorIndex].Type);
          break;
        default:
          pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
          break;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, LOWER_THRESHOLD_CRITICAL_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        UpperThresholdCritical
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, UPPER_THRESHOLD_CRITICAL_STR))) {
        switch (SensorIndex) {
		    case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
        case SENSOR_TYPE_MEDIA_TEMPERATURE:
          // Only Media temperature sensor got upper critical threshold
          pTempBuff = GetSensorValue(DimmSensorsSet[SensorIndex].CriticalUpperThreshold, DimmSensorsSet[SensorIndex].Type);
          break;
        default:
          pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
          break;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, UPPER_THRESHOLD_CRITICAL_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        UpperThresholdFatal
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, UPPER_THRESHOLD_FATAL_STR))) {
        switch (SensorIndex) {
        case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
        case SENSOR_TYPE_MEDIA_TEMPERATURE:
          // Only Controller/Media temperature sensor got upper fatal threshold
          pTempBuff = GetSensorValue(DimmSensorsSet[SensorIndex].FatalThreshold, DimmSensorsSet[SensorIndex].Type);
          break;
        default:
          pTempBuff = CatSPrint(NULL, FORMAT_STR, NOT_APPLICABLE_SHORT_STR);
          break;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, UPPER_THRESHOLD_FATAL_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        SettableThresholds
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SETABLE_THRESHOLDS_STR))) {
        pTempBuff = SensorThresholdsToString(DimmSensorsSet[SensorIndex].SettableThresholds);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SETABLE_THRESHOLDS_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        SupportedThresholds
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SUPPORTED_THRESHOLDS_STR))) {
        pTempBuff = SensorThresholdsToString(DimmSensorsSet[SensorIndex].SupportedThresholds);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SUPPORTED_THRESHOLDS_STR, pTempBuff);
        FREE_POOL_SAFE(pTempBuff);
      }

      /**
        Enabled
      **/
      if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ENABLED_STATE_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ENABLED_STATE_STR, SensorEnabledStateToString(DimmSensorsSet[SensorIndex].Enabled));
      }
      }
  }
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowSensorDataSetAttribs);

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_CMD_DISPLAY_OPTIONS_SAFE(pDispOptions);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDimmIds);
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
