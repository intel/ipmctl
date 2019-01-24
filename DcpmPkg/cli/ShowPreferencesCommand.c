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

#define DS_ROOT_PATH                      L"/Preferences"

/**
  Command syntax definition
**/
struct Command ShowPreferencesCommand =
{
  SHOW_VERB,                                                          //!< verb
  {                                                                   //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}
#endif
  },
  {                                                                   //!< targets
    {PREFERENCES_TARGET, L"", L"", TRUE, ValueEmpty},
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Show user preferences and their current values",                  //!< help
  ShowPreferences,
  TRUE
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
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;

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
   PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
   goto Finish;
  }

  /** Need NvmDimmConfigProtocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
   ReturnCode = EFI_NOT_FOUND;
   PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
   goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDriverPreferences(pNvmDimmConfigProtocol, &DriverPreferences, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    PRINTER_SET_COMMAND_STATUS(pCmd->pPrintCtx, ReturnCode, L"Show preferences", L" on", pCommandStatus);
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }

  PRINTER_BUILD_KEY_PATH(pPath, DS_ROOT_PATH);

  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CLI_DEFAULT_DIMM_ID_PROPERTY, GetDimmIDStr(DisplayPreferences.DimmIdentifier));
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CLI_DEFAULT_SIZE_PROPERTY, GetDisplaySizeStr(DisplayPreferences.SizeUnit));

  if (DriverPreferences.ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE &&
     DriverPreferences.ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE)
  {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  APP_DIRECT_SETTINGS_PROPERTY, PROPERTY_VALUE_RECOMMENDED);
  } else {
    pChannelInterleaving = ParseChannelInterleavingValue(DriverPreferences.ChannelInterleaving);
    pImcInterleaving = ParseImcInterleavingValue(DriverPreferences.ImcInterleaving);

    if (pChannelInterleaving == NULL || pImcInterleaving == NULL) {
      ReturnCode = EFI_DEVICE_ERROR;
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    } else {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, APP_DIRECT_SETTINGS_PROPERTY, FORMAT_STR L"_" FORMAT_STR_NL, pImcInterleaving, pChannelInterleaving);
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
    ReturnCode = EFI_DEVICE_ERROR;
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
  } else {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  APP_DIRECT_GRANULARITY_PROPERTY, pAppDirectGranularity);
  }

#ifdef OS_BUILD
  ReturnCode = GET_VARIABLE_STR(PERFORMANCE_MONITOR_ENABLED, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  PERFORMANCE_MONITOR_ENABLED, tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(PERFORMANCE_MONITOR_INTERVAL_MINUTES, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  PERFORMANCE_MONITOR_INTERVAL_MINUTES, tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(EVENT_MONITOR_ENABLED, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  EVENT_MONITOR_ENABLED, tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(EVENT_MONITOR_INTERVAL_MINUTES, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  EVENT_MONITOR_INTERVAL_MINUTES, tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(EVENT_LOG_MAX, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  EVENT_LOG_MAX, tempStr);
  }
  ReturnCode = GET_VARIABLE_STR(DBG_LOG_MAX, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  DBG_LOG_MAX, tempStr);
  }

  ReturnCode = GET_VARIABLE_STR(DBG_LOG_LEVEL, gNvmDimmConfigProtocolGuid, 0, tempStr);
  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath,  DBG_LOG_LEVEL, tempStr);
  }
#endif

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
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