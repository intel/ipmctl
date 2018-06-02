/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "ShowPreferencesCommand.h"
#include "Common.h"
#include "Convert.h"

/**
  Command syntax definition
**/
struct Command ShowPreferencesCommand =
{
  SHOW_VERB,                                                          //!< verb
  {{L"", L"", L"", L"", FALSE, ValueOptional}},                       //!< options
  {                                                                   //!< targets
    {PREFERENCES_TARGET, L"", L"", TRUE, ValueEmpty},
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Show user preferences and their current values",                  //!< help
  ShowPreferences
};

/**
  Execute the Show Preferences command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
  @retval EFI_DEVICE_ERROR Communications failure with driver
  @retval EFI_NOT_FOUND Cli display preferences could not be retrieved successfully
**/
EFI_STATUS
ShowPreferences(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DRIVER_PREFERENCES DriverPreferences;
  DISPLAY_PREFERENCES DisplayPreferences;
  CONST CHAR16 *pImcInterleaving = NULL;
  CONST CHAR16 *pChannelInterleaving = NULL;
  CONST CHAR16 *pAppDirectGranularity = NULL;
#ifdef OS_BUILD
  CHAR16 tempStr[PROPERTY_VALUE_LEN];
#endif
  NVDIMM_ENTRY();

  SetDisplayInfo(L"Preferences", ListView);

  ZeroMem(&DriverPreferences, sizeof(DriverPreferences));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
   ReturnCode = EFI_INVALID_PARAMETER;
   Print(FORMAT_STR_NL,CLI_ERR_NO_COMMAND);
   goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
   Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
  }

  /** Need NvmDimmConfigProtocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
   Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
   ReturnCode = EFI_NOT_FOUND;
   goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(L"Show preferences", L" on", pCommandStatus);
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  Print(FORMAT_STR L"=" FORMAT_STR_NL, CLI_DEFAULT_DIMM_ID_PROPERTY, GetDimmIDStr(DisplayPreferences.DimmIdentifier));
  Print(FORMAT_STR L"=" FORMAT_STR_NL, CLI_DEFAULT_SIZE_PROPERTY, GetDisplaySizeStr(DisplayPreferences.SizeUnit));

  if (DriverPreferences.ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE &&
     DriverPreferences.ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE)
  {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      APP_DIRECT_SETTINGS_PROPERTY,
      PROPERTY_VALUE_RECOMMENDED);
  } else {
    pChannelInterleaving = ParseChannelInterleavingValue(DriverPreferences.ChannelInterleaving);
    pImcInterleaving = ParseImcInterleavingValue(DriverPreferences.ImcInterleaving);

    if (pChannelInterleaving == NULL || pImcInterleaving == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
      ReturnCode = EFI_DEVICE_ERROR;
    } else {
      Print(FORMAT_STR L"=" FORMAT_STR L"_" FORMAT_STR_NL,
        APP_DIRECT_SETTINGS_PROPERTY,
        pImcInterleaving,
        pChannelInterleaving);
    }
  }

  switch (DriverPreferences.AppDirectGranularity) {
  case APPDIRECT_GRANULARITY_1GIB:
    pAppDirectGranularity = L"1";
    break;
  case APPDIRECT_GRANULARITY_DEFAULT:
    pAppDirectGranularity = PROPERTY_VALUE_RECOMMENDED;
    break;
  default:
    pAppDirectGranularity = NULL;
  }

  if (pAppDirectGranularity == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_DEVICE_ERROR;
  } else {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      APP_DIRECT_GRANULARITY_PROPERTY,
      pAppDirectGranularity);
  }

#ifdef OS_BUILD
  ReturnCode = GET_VARIABLE_STR(PERFORMANCE_MONITOR_ENABLED, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      PERFORMANCE_MONITOR_ENABLED,
      tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(PERFORMANCE_MONITOR_INTERVAL_MINUTES, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      PERFORMANCE_MONITOR_INTERVAL_MINUTES,
      tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(EVENT_MONITOR_ENABLED, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      EVENT_MONITOR_ENABLED,
      tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(EVENT_MONITOR_INTERVAL_MINUTES, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      EVENT_MONITOR_INTERVAL_MINUTES,
      tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(EVENT_LOG_MAX, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      EVENT_LOG_MAX,
      tempStr);
  }
  ReturnCode = GET_VARIABLE_STR(DBG_LOG_MAX, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
      DBG_LOG_MAX,
      tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(DBG_LOG_LEVEL, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR L"=" FORMAT_STR_NL,
       DBG_LOG_LEVEL,
       tempStr);
  }

#endif

Finish:
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Show Preferences command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowPreferencesCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowPreferencesCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

