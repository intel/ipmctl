/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "SetPreferencesCommand.h"
#include "Common.h"
#include "Convert.h"
#include "Utility.h"

 /**
 Local definitions
 **/
#define MIN_BOOLEAN_VALUE 0
#define MAX_BOOLEAN_VALUE 1
#define MIN_LOG_VALUE 0x0
#define MAX_LOG_VALUE 0x7FFFFFFF
#define MIN_LOG_LEVEL_VALUE 0
#define MAX_LOG_LEVEL_VALUE 4
#define MIN_PERFORMANCE_MONITOR_INTERVAL_MINUTES 1
#define MIN_EVENT_MONITOR_INTERVAL_MINUTES 1

/**
  Command syntax definition
**/

struct Command SetPreferencesCommand =
{
  SET_VERB,                                                          //!< verb
  {
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}                         //!< options
#endif
  },                      //!< options
  {                                                                  //!< targets
    {PREFERENCES_TARGET, L"", L"", TRUE, ValueEmpty},
  },
  {                                                                  //!< properties
    {CLI_DEFAULT_DIMM_ID_PROPERTY, L"", PROPERTY_VALUE_HANDLE L"|" PROPERTY_VALUE_UID, FALSE, ValueRequired},
    {CLI_DEFAULT_SIZE_PROPERTY, L"", HELP_TEXT_DEFAULT_SIZE, FALSE, ValueRequired},
    {APP_DIRECT_SETTINGS_PROPERTY, L"", HELP_TEXT_APPDIRECT_SETTINGS, FALSE, ValueRequired},
    {APP_DIRECT_GRANULARITY_PROPERTY, L"", HELP_TEXT_APPDIRECT_GRANULARITY, FALSE, ValueRequired},
#ifdef OS_BUILD
    {PERFORMANCE_MONITOR_ENABLED, L"", HELP_PERFORMANCE_MONITOR_ENABLED, FALSE, ValueRequired },
    {PERFORMANCE_MONITOR_INTERVAL_MINUTES, L"", HELP_PERFORMANCE_MONITOR_INTERVAL_MINUTES, FALSE, ValueRequired },
    {EVENT_MONITOR_ENABLED, L"", HELP_EVENT_MONITOR_ENABLED, FALSE, ValueRequired },
    {EVENT_MONITOR_INTERVAL_MINUTES, L"", HELP_EVENT_MONITOR_INTERVAL_MINUTES, FALSE, ValueRequired },
    {EVENT_LOG_MAX, L"", HELP_EVENT_LOG_MAX, FALSE, ValueRequired },
    {DBG_LOG_MAX, L"", HELP_DBG_LOG_MAX, FALSE, ValueRequired },
    {DBG_LOG_LEVEL, L"", HELP_DBG_LOG_LEVEL, FALSE, ValueRequired},
#endif
  },
  L"Set user preferences",                  //!< help
  SetPreferences
};

STATIC
UINT8
GetBitFieldForInterleaveChannelSize(
  IN     CHAR16 *pStringValue
  )
{
  UINT8 BitField = CHANNEL_INTERLEAVE_SIZE_INVALID;

  if (pStringValue == NULL) {
    return BitField;
  }

  if (StrICmp(pStringValue, L"64B") == 0) {
    BitField = CHANNEL_INTERLEAVE_SIZE_64B;
  } else if (StrICmp(pStringValue, L"128B") == 0) {
    BitField = CHANNEL_INTERLEAVE_SIZE_128B;
  } else if (StrICmp(pStringValue, L"256B") == 0) {
    BitField = CHANNEL_INTERLEAVE_SIZE_256B;
  } else if (StrICmp(pStringValue, L"4KB") == 0) {
    BitField = CHANNEL_INTERLEAVE_SIZE_4KB;
  } else if (StrICmp(pStringValue, L"1GB") == 0) {
    BitField = CHANNEL_INTERLEAVE_SIZE_1GB;
  }

  return BitField;
}

STATIC
UINT8
GetBitFieldForInterleaveiMCSize(
  IN     CHAR16 *pStringValue
  )
{
  UINT8 BitField = IMC_INTERLEAVE_SIZE_INVALID;

  if (pStringValue == NULL) {
    return BitField;
  }

  if (StrICmp(pStringValue, L"64B") == 0) {
    BitField = IMC_INTERLEAVE_SIZE_64B;
  } else if (StrICmp(pStringValue, L"128B") == 0) {
    BitField = IMC_INTERLEAVE_SIZE_128B;
  } else if (StrICmp(pStringValue, L"256B") == 0) {
    BitField = IMC_INTERLEAVE_SIZE_256B;
  } else if (StrICmp(pStringValue, L"4KB") == 0) {
    BitField = IMC_INTERLEAVE_SIZE_4KB;
  } else if (StrICmp(pStringValue, L"1GB") == 0) {
    BitField = IMC_INTERLEAVE_SIZE_1GB;
  }

  return BitField;
}

STATIC
EFI_STATUS
GetAppDirectSettingsBitFields(
  IN      CHAR16 *pAppDirectSettings,
      OUT UINT8  *pImcBitField,
      OUT UINT8  *pChannelBitField
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 **ppSplitSettings = NULL;
  UINT32 NumOfElements = 0;

  if (pAppDirectSettings == NULL || pImcBitField == NULL || pChannelBitField == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  // default values
  *pImcBitField = DEFAULT_IMC_INTERLEAVE_SIZE;
  *pChannelBitField = DEFAULT_CHANNEL_INTERLEAVE_SIZE;


  ppSplitSettings = StrSplit(pAppDirectSettings, L'_', &NumOfElements);
  if (ppSplitSettings == NULL || NumOfElements != 2) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pImcBitField = GetBitFieldForInterleaveiMCSize(ppSplitSettings[0]);
  *pChannelBitField = GetBitFieldForInterleaveChannelSize(ppSplitSettings[1]);

  if (*pImcBitField == IMC_INTERLEAVE_SIZE_INVALID ||
      *pChannelBitField == CHANNEL_INTERLEAVE_SIZE_INVALID)
  {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

Finish:
  if (ppSplitSettings != NULL) {
    FreeStringArray(ppSplitSettings, NumOfElements);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#ifdef OS_BUILD
EFI_STATUS ValidateAndConvertInput(CHAR16 *InputString, INT64 MinVal, UINT64 MaxValue, UINT64 *IntegerEq) {

  EFI_STATUS rc = EFI_INVALID_PARAMETER;
  if (!GetU64FromString(InputString, IntegerEq)) {
    goto Finish;
  }
  if (*IntegerEq > MaxValue || *IntegerEq < MinVal) {
      goto Finish;
  }
  rc = EFI_SUCCESS;

Finish:
  return rc;
}

EFI_STATUS SetPreferenceStr(IN struct Command *pCmd, IN CONST CHAR16 * pName, IN CONST CHAR8 *pIfNotFoundWarning, IN INT64 MinVal, IN UINT64 MaxValue, OUT COMMAND_STATUS* pCommandStatus)
{
  EFI_STATUS rc = EFI_SUCCESS;
  CHAR16 *pTypeValue = NULL;
  UINT64 IntegerValue;

  if ((rc = ContainsProperty(pCmd, pName)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(rc)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    rc = GetPropertyValue(pCmd, pName, &pTypeValue);
    if (EFI_ERROR(rc)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }
    rc = ValidateAndConvertInput(pTypeValue, MinVal, MaxValue, &IntegerValue);
    if (EFI_ERROR(rc) || ((StrCmp(pName, DBG_LOG_LEVEL) == 0) && IntegerValue > 4)) {
      PRINT_SET_PREFERENCES_EFI_ERR(pName, pTypeValue, EFI_INVALID_PARAMETER);
      SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_INVALID_PARAMETER);
      goto Finish;
    } else {
      if (rc == EFI_SUCCESS) {
        rc = SET_STR_VARIABLE_NV(pName, gNvmDimmCliVariableGuid, pTypeValue);
        if (!EFI_ERROR(rc)) {
          PRINT_SET_PREFERENCES_SUCCESS(pName, pTypeValue);
          SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_SUCCESS);
        } else {
          PRINT_SET_PREFERENCES_EFI_ERR(pName, pTypeValue, rc);
          SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_OPERATION_FAILED);
        }
      }
    }
  }
Finish:
  return rc;
}
#endif

/**
  Execute the Set Preferences command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
  @retval EFI_NOT_FOUND Cli display preferences could not be retrieved successfully
**/
EFI_STATUS
SetPreferences(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  DRIVER_PREFERENCES DriverPreferences;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT8 Index = 0;
  CHAR16 *pTypeValue = NULL;
  UINTN VariableSize = 0;

  NVDIMM_ENTRY();

  SetDisplayInfo(L"SetPreferences", ResultsView, NULL);

  ZeroMem(&DriverPreferences, sizeof(DriverPreferences));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
  }

  /** Need NvmDimmConfigProtocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Retrieve current settings
  ReturnCode = pNvmDimmConfigProtocol->GetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(L"Set preferences", L" on", pCommandStatus);
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
   Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
   ReturnCode = EFI_NOT_FOUND;
   goto Finish;
  }

  if ((TempReturnCode = ContainsProperty(pCmd, CLI_DEFAULT_DIMM_ID_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(TempReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }
    TempReturnCode = GetPropertyValue(pCmd, CLI_DEFAULT_DIMM_ID_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Default DimmID Type not provided");
      PRINT_SET_PREFERENCES_EFI_ERR(CLI_DEFAULT_DIMM_ID_PROPERTY, NULL, EFI_INVALID_PARAMETER);
    } else if ((Index = GetDimmIDIndex(pTypeValue)) >= DISPLAY_DIMM_ID_MAX_SIZE) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Incorrect default DimmID type");
      PRINT_SET_PREFERENCES_EFI_ERR(CLI_DEFAULT_DIMM_ID_PROPERTY, pTypeValue, EFI_INVALID_PARAMETER);
    } else {
      DisplayPreferences.DimmIdentifier = Index;
      VariableSize = sizeof(DisplayPreferences.DimmIdentifier);
      TempReturnCode = SET_VARIABLE_NV(
        CLI_DEFAULT_DIMM_ID_PROPERTY,
        gNvmDimmCliVariableGuid,
        VariableSize,
        &DisplayPreferences.DimmIdentifier);
      if (!EFI_ERROR(TempReturnCode)) {
        PRINT_SET_PREFERENCES_SUCCESS(CLI_DEFAULT_DIMM_ID_PROPERTY, pTypeValue);
      } else {
        KEEP_ERROR(ReturnCode,TempReturnCode);
        PRINT_SET_PREFERENCES_EFI_ERR(CLI_DEFAULT_DIMM_ID_PROPERTY, pTypeValue, TempReturnCode);
      }
    }
  }

  if ((TempReturnCode = ContainsProperty(pCmd, CLI_DEFAULT_SIZE_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(TempReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }
    TempReturnCode = GetPropertyValue(pCmd, CLI_DEFAULT_SIZE_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Display default size type not provided");
      PRINT_SET_PREFERENCES_EFI_ERR(CLI_DEFAULT_SIZE_PROPERTY, NULL, EFI_INVALID_PARAMETER);
    } else if ((Index = GetDisplaySizeIndex(pTypeValue)) >= DISPLAY_SIZE_MAX_SIZE) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Incorrect default size type");
      PRINT_SET_PREFERENCES_EFI_ERR(CLI_DEFAULT_SIZE_PROPERTY, pTypeValue, EFI_INVALID_PARAMETER);
    } else {
      DisplayPreferences.SizeUnit = Index;
      VariableSize = sizeof(DisplayPreferences.SizeUnit);
      TempReturnCode = SET_VARIABLE_NV(
        CLI_DEFAULT_SIZE_PROPERTY,
        gNvmDimmCliVariableGuid,
        VariableSize,
        &DisplayPreferences.SizeUnit);
      if (TempReturnCode == EFI_SUCCESS) {
        PRINT_SET_PREFERENCES_SUCCESS(CLI_DEFAULT_SIZE_PROPERTY, pTypeValue);
      } else {
        KEEP_ERROR(ReturnCode,TempReturnCode);
        PRINT_SET_PREFERENCES_EFI_ERR(CLI_DEFAULT_SIZE_PROPERTY, pTypeValue, TempReturnCode);
      }
    }
  }

  if ((TempReturnCode = ContainsProperty(pCmd, APP_DIRECT_SETTINGS_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }

    TempReturnCode = GetPropertyValue(pCmd, APP_DIRECT_SETTINGS_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("AppDirect interleave setting type not provided");
      PRINT_SET_PREFERENCES_EFI_ERR(APP_DIRECT_SETTINGS_PROPERTY, NULL, EFI_INVALID_PARAMETER);
    } else {
      if (StrICmp(pTypeValue, PROPERTY_VALUE_RECOMMENDED) == 0) {
        DriverPreferences.ChannelInterleaving = DEFAULT_CHANNEL_INTERLEAVE_SIZE;
        DriverPreferences.ImcInterleaving = DEFAULT_IMC_INTERLEAVE_SIZE;
      } else if ((TempReturnCode = GetAppDirectSettingsBitFields(pTypeValue, &DriverPreferences.ImcInterleaving, &DriverPreferences.ChannelInterleaving)) != EFI_SUCCESS) {
        KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
        NVDIMM_WARN("Incorrect AppDirect interleave setting type");
        PRINT_SET_PREFERENCES_EFI_ERR(APP_DIRECT_SETTINGS_PROPERTY, pTypeValue, EFI_INVALID_PARAMETER);
      }

      if (TempReturnCode == EFI_SUCCESS) {
        TempReturnCode = pNvmDimmConfigProtocol->SetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
        if (TempReturnCode == EFI_SUCCESS) {
          PRINT_SET_PREFERENCES_SUCCESS(APP_DIRECT_SETTINGS_PROPERTY, pTypeValue);
        } else {
          TempReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
          PRINT_SET_PREFERENCES_EFI_ERR(APP_DIRECT_SETTINGS_PROPERTY, pTypeValue, TempReturnCode);
          KEEP_ERROR(ReturnCode, TempReturnCode);
        }
      }
    }
  }

  if ((TempReturnCode = ContainsProperty(pCmd, APP_DIRECT_GRANULARITY_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }

    TempReturnCode = GetPropertyValue(pCmd, APP_DIRECT_GRANULARITY_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("AppDirect Granularity setting type not provided");
      PRINT_SET_PREFERENCES_EFI_ERR(APP_DIRECT_GRANULARITY_PROPERTY, NULL, EFI_INVALID_PARAMETER);
    } else {
      if (StrICmp(pTypeValue, PROPERTY_VALUE_RECOMMENDED) == 0) {
        DriverPreferences.AppDirectGranularity = APPDIRECT_GRANULARITY_DEFAULT;
      } else if (StrICmp(pTypeValue, L"1") == 0) {
        DriverPreferences.AppDirectGranularity = APPDIRECT_GRANULARITY_1GIB;
      } else {
        TempReturnCode = EFI_INVALID_PARAMETER;
        KEEP_ERROR(ReturnCode, TempReturnCode);
        PRINT_SET_PREFERENCES_EFI_ERR(APP_DIRECT_GRANULARITY_PROPERTY, pTypeValue, EFI_INVALID_PARAMETER);
      }

      if (TempReturnCode == EFI_SUCCESS) {
        TempReturnCode = pNvmDimmConfigProtocol->SetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
        if (TempReturnCode == EFI_SUCCESS) {
          PRINT_SET_PREFERENCES_SUCCESS(APP_DIRECT_GRANULARITY_PROPERTY, pTypeValue);
        } else {
          TempReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
          PRINT_SET_PREFERENCES_EFI_ERR(APP_DIRECT_GRANULARITY_PROPERTY, pTypeValue, TempReturnCode);
          KEEP_ERROR(ReturnCode, TempReturnCode);
        }
      }
    }
  }
#ifdef OS_BUILD
  SetPreferenceStr(pCmd, PERFORMANCE_MONITOR_ENABLED, "Performance monitor enable setting type not provided", MIN_BOOLEAN_VALUE, MAX_BOOLEAN_VALUE, pCommandStatus);
  SetPreferenceStr(pCmd, PERFORMANCE_MONITOR_INTERVAL_MINUTES, "Performance monitor interval minutes setting type not provided", MIN_PERFORMANCE_MONITOR_INTERVAL_MINUTES, MAX_UINT64_VALUE, pCommandStatus);
  SetPreferenceStr(pCmd, EVENT_MONITOR_ENABLED, "Event monitor enabled setting type not provided", MIN_BOOLEAN_VALUE, MAX_BOOLEAN_VALUE, pCommandStatus);
  SetPreferenceStr(pCmd, EVENT_MONITOR_INTERVAL_MINUTES, "event monitor interval minutes setting type not provided", MIN_EVENT_MONITOR_INTERVAL_MINUTES, MAX_UINT64_VALUE, pCommandStatus);
  SetPreferenceStr(pCmd, EVENT_LOG_MAX, "Event log max setting type not provided", MIN_LOG_VALUE, MAX_LOG_VALUE, pCommandStatus);
  SetPreferenceStr(pCmd, DBG_LOG_MAX, "Log max setting type not provided", MIN_LOG_VALUE, MAX_LOG_VALUE, pCommandStatus);
  SetPreferenceStr(pCmd, DBG_LOG_LEVEL, "Log level setting type not provided", MIN_LOG_LEVEL_VALUE, MAX_LOG_LEVEL_VALUE, pCommandStatus);

  TempReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  KEEP_ERROR(ReturnCode, TempReturnCode);
#endif

Finish:
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Set Preferences command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterSetPreferencesCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&SetPreferencesCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

