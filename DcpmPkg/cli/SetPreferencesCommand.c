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
#include <ReadRunTimePreferences.h>

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
  {                                                                  //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", L"", FALSE, ValueOptional}
#endif
  },
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
  SetPreferences,
  TRUE
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
EFI_STATUS ValidateAndConvertInput(CHAR16 *InputString, UINT64 MinVal, UINT64 MaxValue, UINT64 *IntegerEq) {

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

EFI_STATUS SetPreferenceStr(IN struct Command *pCmd, IN CONST CHAR16 * pName, IN CONST CHAR8 *pIfNotFoundWarning, IN UINT64 MinVal, IN UINT64 MaxValue, OUT COMMAND_STATUS* pCommandStatus)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pTypeValue = NULL;
  UINT64 IntegerValue;

  if ((ReturnCode = ContainsProperty(pCmd, pName)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    ReturnCode = GetPropertyValue(pCmd, pName, &pTypeValue);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }
    ReturnCode = ValidateAndConvertInput(pTypeValue, MinVal, MaxValue, &IntegerValue);
    if (EFI_ERROR(ReturnCode) || ((StrCmp(pName, DBG_LOG_LEVEL) == 0) && IntegerValue > 4)) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, pName, pTypeValue, ReturnCode, PROPERTY_ERROR_INVALID_OUT_OF_RANGE);
      SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_INVALID_PARAMETER);
      goto Finish;
    } else {
      if (ReturnCode == EFI_SUCCESS) {
        ReturnCode = SET_STR_VARIABLE_NV(pName, gNvmDimmVariableGuid, pTypeValue);
        if (!EFI_ERROR(ReturnCode)) {
          PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_SET_PREFERENCE_SUCCESS, pName, pTypeValue);
          SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_SUCCESS);
        } else {
          PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, pName, pTypeValue, ReturnCode, PROPERTY_ERROR_SET_FAILED_UNKNOWN);
          SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_ERR_OPERATION_FAILED);
        }
      }
    }
  }
Finish:
  return ReturnCode;
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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  DRIVER_PREFERENCES DriverPreferences;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT8 Index = 0;
  CHAR16 *pTypeValue = NULL;
  UINTN VariableSize = 0;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  UINT16 PropertyCnt = 0;

  NVDIMM_ENTRY();

  ZeroMem(&DriverPreferences, sizeof(DriverPreferences));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  //verify we have atleast one preference to set.
  ReturnCode = GetPropertyCount(pCmd, &PropertyCnt);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }
  else if (0 == PropertyCnt) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Preference not specified\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  /** Need NvmDimmConfigProtocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Retrieve current settings
  ReturnCode = pNvmDimmConfigProtocol->GetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, L"Set preferences", L" on", pCommandStatus);
    goto Finish;
  }

  ReturnCode = ReadRunTimePreferences(&DisplayPreferences, DISPLAY_CLI_INFO);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }

  if ((TempReturnCode = ContainsProperty(pCmd, CLI_DEFAULT_DIMM_ID_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(TempReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }
    TempReturnCode = GetPropertyValue(pCmd, CLI_DEFAULT_DIMM_ID_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Default DimmID Type not provided");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, CLI_DEFAULT_DIMM_ID_PROPERTY, L"", ReturnCode, PROPERTY_ERROR_DEFAULT_DIMM_NOT_PROVIDED);
    } else if ((Index = GetDimmIDIndex(pTypeValue)) >= DISPLAY_DIMM_ID_MAX_SIZE) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Incorrect default DimmID type");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, CLI_DEFAULT_DIMM_ID_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_INCORRECT_DEFAULT_DIMM_TYPE);
    } else {
      DisplayPreferences.DimmIdentifier = Index;
      VariableSize = sizeof(DisplayPreferences.DimmIdentifier);
      TempReturnCode = SET_VARIABLE_NV(
        CLI_DEFAULT_DIMM_ID_PROPERTY,
        gNvmDimmVariableGuid,
        VariableSize,
        &DisplayPreferences.DimmIdentifier);
      if (!EFI_ERROR(TempReturnCode)) {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_SUCCESS, CLI_DEFAULT_DIMM_ID_PROPERTY, pTypeValue);
      } else {
        KEEP_ERROR(ReturnCode,TempReturnCode);
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, CLI_DEFAULT_DIMM_ID_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_UNKNOWN);
      }
    }
  }

  if ((TempReturnCode = ContainsProperty(pCmd, CLI_DEFAULT_SIZE_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    TempReturnCode = GetPropertyValue(pCmd, CLI_DEFAULT_SIZE_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Display default size type not provided");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, CLI_DEFAULT_SIZE_PROPERTY, L"", ReturnCode, PROPERTY_ERROR_DISPLAY_DEFAULT_NOT_PROVIDED);
    } else if ((Index = GetDisplaySizeIndex(pTypeValue)) >= DISPLAY_SIZE_MAX_SIZE) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("Incorrect default size type");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, CLI_DEFAULT_SIZE_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_DEFAULT_INCORRECT_SIZE_TYPE);
    } else {
      DisplayPreferences.SizeUnit = Index;
      VariableSize = sizeof(DisplayPreferences.SizeUnit);
      TempReturnCode = SET_VARIABLE_NV(
        CLI_DEFAULT_SIZE_PROPERTY,
        gNvmDimmVariableGuid,
        VariableSize,
        &DisplayPreferences.SizeUnit);
      if (TempReturnCode == EFI_SUCCESS) {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_SUCCESS, CLI_DEFAULT_SIZE_PROPERTY, pTypeValue);
      } else {
        KEEP_ERROR(ReturnCode,TempReturnCode);
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, CLI_DEFAULT_SIZE_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_UNKNOWN);
      }
    }
  }

  if ((TempReturnCode = ContainsProperty(pCmd, APP_DIRECT_SETTINGS_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(ReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    TempReturnCode = GetPropertyValue(pCmd, APP_DIRECT_SETTINGS_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("AppDirect interleave setting type not provided");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, APP_DIRECT_SETTINGS_PROPERTY, L"", ReturnCode, PROPERTY_ERROR_INTERLEAVE_TYPE_NOT_PROVIDED);
    } else {
      if (StrICmp(pTypeValue, PROPERTY_VALUE_RECOMMENDED) == 0) {
        DriverPreferences.ChannelInterleaving = DEFAULT_CHANNEL_INTERLEAVE_SIZE;
        DriverPreferences.ImcInterleaving = DEFAULT_IMC_INTERLEAVE_SIZE;
      } else if ((TempReturnCode = GetAppDirectSettingsBitFields(pTypeValue, &DriverPreferences.ImcInterleaving, &DriverPreferences.ChannelInterleaving)) != EFI_SUCCESS) {
        KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
        NVDIMM_WARN("Incorrect AppDirect interleave setting type");
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, APP_DIRECT_SETTINGS_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_APPDIR_INTERLEAVE_TYPE);
      }

      if (TempReturnCode == EFI_SUCCESS) {
        TempReturnCode = pNvmDimmConfigProtocol->SetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
        if (TempReturnCode == EFI_SUCCESS) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_SUCCESS, APP_DIRECT_SETTINGS_PROPERTY, pTypeValue);
        } else {
          TempReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
          KEEP_ERROR(ReturnCode, TempReturnCode);
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, APP_DIRECT_SETTINGS_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_UNKNOWN);
        }
      }
    }
  }

  if ((TempReturnCode = ContainsProperty(pCmd, APP_DIRECT_GRANULARITY_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }

    TempReturnCode = GetPropertyValue(pCmd, APP_DIRECT_GRANULARITY_PROPERTY, &pTypeValue);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
      NVDIMM_WARN("AppDirect Granularity setting type not provided");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, APP_DIRECT_GRANULARITY_PROPERTY, L"", ReturnCode, PROPERTY_ERROR_GRANULARITY_NOT_PROVIDED);
    } else {
      if (StrICmp(pTypeValue, PROPERTY_VALUE_RECOMMENDED) == 0) {
        DriverPreferences.AppDirectGranularity = APPDIRECT_GRANULARITY_DEFAULT;
      } else if (StrICmp(pTypeValue, L"1") == 0) {
        DriverPreferences.AppDirectGranularity = APPDIRECT_GRANULARITY_1GIB;
      } else {
        TempReturnCode = EFI_INVALID_PARAMETER;
        KEEP_ERROR(ReturnCode, TempReturnCode);
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, APP_DIRECT_GRANULARITY_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_INVALID_GRANULARITY);
      }

      if (TempReturnCode == EFI_SUCCESS) {
        TempReturnCode = pNvmDimmConfigProtocol->SetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
        if (TempReturnCode == EFI_SUCCESS) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_SUCCESS, APP_DIRECT_GRANULARITY_PROPERTY, pTypeValue);
        } else {
          TempReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
          KEEP_ERROR(ReturnCode, TempReturnCode);
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_SET_PREFERENCE_ERROR, APP_DIRECT_GRANULARITY_PROPERTY, pTypeValue, ReturnCode, PROPERTY_ERROR_UNKNOWN);
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
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
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

