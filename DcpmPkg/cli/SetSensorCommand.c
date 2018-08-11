/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include "SetSensorCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include "Common.h"
#include "NvmDimmCli.h"

/** Command syntax definition **/
struct Command SetSensorCommand =
{
  SET_VERB,                                                         //!< verb
  {                                                              //!< options
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {                                                                 //!< targets
    {SENSOR_TARGET, L"", SENSOR_TARGETS, TRUE, ValueRequired},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired}
  },
  {                                                                 //!< properties
    {NON_CRIT_THRESHOLD_PROPERTY, L"", HELP_TEXT_VALUE, FALSE},
    {ENABLED_STATE_PROPERTY, L"", PROPERTY_VALUE_0 L"|" PROPERTY_VALUE_1, FALSE}
  },
  L"Modify the alarm threshold(s) for one or more DIMMs.",          //!< help
  SetSensor
};

/**
  Execute the set sensor command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
SetSensor(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  UINT32 DimmIdsCount = 0;
  UINT64 TempValue = 0;
  CHAR16* pTargetValue = NULL;
  CHAR16* pCommandStatusMessage = NULL;
  INT16 NonCriticalThreshold = THRESHOLD_UNDEFINED;
  UINT16 Index = 0;
  UINT8 EnabledState = ENABLED_STATE_UNDEFINED;
  UINT8 SensorId = SENSOR_ID_UNDEFINED;
  BOOLEAN ValidPropertyAndValue = FALSE;
  BOOLEAN Force = FALSE;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN Confirmation = FALSE;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];

  NVDIMM_ENTRY();

  SetDisplayInfo(L"SetSensor", ResultsView, NULL);

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

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  if (DimmIdsCount == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(&DimmIdsCount, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    if (DimmIdsCount == 0) {
      Print(FORMAT_STR_NL, CLI_INFO_NO_MANAGEABLE_DIMMS);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
  }

  // check properties

  if (!EFI_ERROR(ContainsProperty(pCmd, NON_CRIT_THRESHOLD_PROPERTY))) {
    if (PropertyToUint64(pCmd, NON_CRIT_THRESHOLD_PROPERTY, &TempValue)) {
      NonCriticalThreshold = (INT16) TempValue;
      ValidPropertyAndValue = TRUE;
    } else {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_NONCRIT_THRESHOLD);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
  if (!EFI_ERROR(ContainsProperty(pCmd, ENABLED_STATE_PROPERTY))) {
    if (PropertyToUint64(pCmd, ENABLED_STATE_PROPERTY, &TempValue)) {
      EnabledState = (UINT8) TempValue;
      ValidPropertyAndValue = TRUE;
    } else {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_ENABLED_STATE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
  if (!ValidPropertyAndValue) {
    Print(FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (ContainTarget(pCmd, SENSOR_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, SENSOR_TARGET);
    if (StrICmp(pTargetValue, CONTROLLER_TEMPERATURE_TARGET_VALUE) == 0) {
      SensorId = SENSOR_TYPE_CONTROLLER_TEMPERATURE;
    } else if (StrICmp(pTargetValue, MEDIA_TEMPERATURE_TARGET_VALUE) == 0) {
      SensorId = SENSOR_TYPE_MEDIA_TEMPERATURE;
    } else if (StrICmp(pTargetValue, SPARE_CAPACITY_TARGET_VALUE) == 0) {
      SensorId = SENSOR_TYPE_PERCENTAGE_REMAINING;
    } else {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SENSOR);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  /** Check force option **/
  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  if (SensorId == SENSOR_TYPE_CONTROLLER_TEMPERATURE) {
    pCommandStatusMessage = CatSPrint(NULL, L"Modify controller temperature settings");
  } else if (SensorId == SENSOR_TYPE_MEDIA_TEMPERATURE) {
    pCommandStatusMessage = CatSPrint(NULL, L"Modify media temperature settings");
  } else {
    pCommandStatusMessage = CatSPrint(NULL, L"Modify percentage remaining settings");
  }

  if (!Force) {
    for (Index = 0; Index < DimmIdsCount; Index++) {
      ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      Print(L"Modifying settings on DIMM (" FORMAT_STR L").", DimmStr);
      ReturnCode = PromptYesNo(&Confirmation);
      if (!EFI_ERROR(ReturnCode) && Confirmation) {
        ReturnCode = pNvmDimmConfigProtocol->SetAlarmThresholds(pNvmDimmConfigProtocol, &pDimmIds[Index], 1,
              SensorId, NonCriticalThreshold, EnabledState, pCommandStatus);
        if (EFI_ERROR(ReturnCode)) {
          goto FinishCommandStatusSet;
        }
      } else {
        Print(L"Skipped modifying settings for DIMM (" FORMAT_STR L")\n", DimmStr);
        continue;
      }
    }
  } else {
    ReturnCode = pNvmDimmConfigProtocol->SetAlarmThresholds(pNvmDimmConfigProtocol, &pDimmIds[Index], DimmIdsCount,
          SensorId, NonCriticalThreshold, EnabledState, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishCommandStatusSet;
    }
  }

FinishCommandStatusSet:
  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  DisplayCommandStatus(pCommandStatusMessage, L" on", pCommandStatus);
Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pCommandStatusMessage);
  FREE_POOL_SAFE(pDimms);
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
RegisterSetSensorCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&SetSensorCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
