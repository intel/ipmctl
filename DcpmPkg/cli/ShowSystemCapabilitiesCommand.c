/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowSystemCapabilitiesCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include "Common.h"
#include "NvmDimmCli.h"
#include <ReadRunTimePreferences.h>

#define DS_ROOT_PATH                        L"/SystemCapabilities"


/**
  Command syntax definition
**/
struct Command ShowSystemCapabilitiesCommand = {
  SHOW_VERB,                                                    //!< verb
  {                                                             //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"",HELP_ALL_DETAILS_TEXT, FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES,HELP_DISPLAY_DETAILS_TEXT, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP,HELP_UNIT_DETAILS_TEXT, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {                                                             //!< targets
    {SYSTEM_TARGET, L"", L"", TRUE, ValueEmpty},
    {CAPABILITIES_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                      //!< properties
  L"Show the platform supported " PMEM_MODULE_STR L" capabilities.",
  ShowSystemCapabilities,
  TRUE
};

CHAR16 *mppAllowedShowSystemCapabilitiesDisplayValues[] = {
  PLATFORM_CONFIG_SUPPORT_STR,
  MEMORY_ALIGNMENT_STR,
  VOLATILE_MODE_ALLOWED_STR,
  VOLATILE_MODE_CURRENT_STR,
  APPDIRECT_MODE_ALLOWED_STR,
  OPERATING_MODE_SUPPORT_STR,
  APPDIRECT_SETTINGS_SUPPORTED_STR,
  APPDIRECT_SETTINGS_RECCOMENDED_STR,
  MIN_NAMESPACE_SIZE_STR,
  APPDIRECT_MIRROR_SUPPORTED_STR,
  DIMM_SPARE_SUPPORTED_STR,
  APPDIRECT_MIGRATION_SUPPORTED_STR,
  RENAME_NAMESPACE_SUPPORTED_STR,
  GROW_APPDIRECT_NAMESPACE_SUPPORTED_STR,
  SHRINK_APPDIRECT_NAMESPACE_SUPPORTED_STR,
  INITIATE_SCRUB_SUPPORTED,
  ASYNCHRONOUS_DRAM_REFRESH_SUPPORTED_STR,
  ERASE_DEVICE_DATA_SUPPORTED_STR,
  ENABLE_DEVICE_SECURITY_SUPPORTED_STR,
  DISABLE_DEVICE_SECURITY_SUPPORTED_STR,
  UNLOCK_DEVICE_SECURITY_SUPPORTED_STR,
  FREEZE_DEVICE_SECURITY_SUPPORTED_STR,
  CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR,
  CHANGE_MASTER_PASSPHRASE_SUPPORTED_STR,
  MASTER_ERASE_DEVICE_DATA_SUPPORTED_STR
};

/**
  Prints allowed volatile mode

  @param[in] MemoryMode Byte describing allowed memory mode
**/
STATIC
VOID
PrintAllowedVolatileMode(
  IN    PRINT_CONTEXT *pPrinterCtx,
  IN    CHAR16 *pPath,
  IN    CURRENT_MEMORY_MODE MemoryMode
  )
{
  switch (MemoryMode.MemoryModeSplit.AllowedVolatileMode) {
  case VOLATILE_MODE_1LM:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VOLATILE_MODE_ALLOWED_STR, ONE_LM_STR);
    break;
  case VOLATILE_MODE_2LM:
    // Memory Mode is the volatile implementation of 2LM
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VOLATILE_MODE_ALLOWED_STR, MEMORY_STR);
    break;
  default:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VOLATILE_MODE_ALLOWED_STR, UNKNOWN_STR);
    break;
  }
}

/**
  Prints current volatile mode

  @param[in] MemoryMode Byte describing current memory mode
**/
STATIC
VOID
PrintCurrentVolatileMode(
  IN    PRINT_CONTEXT *pPrinterCtx,
  IN    CHAR16 *pPath,
  IN    CURRENT_MEMORY_MODE MemoryMode
  )
{
  switch (MemoryMode.MemoryModeSplit.CurrentVolatileMode) {
  case VOLATILE_MODE_1LM:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VOLATILE_MODE_CURRENT_STR, ONE_LM_STR);
    break;
  case VOLATILE_MODE_2LM:
    // Memory Mode is the volatile implementation of 2LM
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VOLATILE_MODE_CURRENT_STR, MEMORY_STR);
    break;
  default:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VOLATILE_MODE_CURRENT_STR, UNKNOWN_STR);
    break;
  }
}

/**
  Prints allowed appdirect mode

  @param[in] MemoryMode Byte describing allowed memory mode
**/
STATIC
VOID
PrintAllowedAppDirectMode(
  IN    PRINT_CONTEXT *pPrinterCtx,
  IN    CHAR16 *pPath,
  IN    CURRENT_MEMORY_MODE MemoryMode
  )
{
  switch (MemoryMode.MemoryModeSplit.PersistentMode) {
  case PERSISTENT_MODE_DISABLED:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, APPDIRECT_MODE_ALLOWED_STR, DISABLED_STR);
    break;
  case PERSISTENT_MODE_APP_DIRECT:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, APPDIRECT_MODE_ALLOWED_STR, APPDIRECT_STR);
    break;
  default:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, APPDIRECT_MODE_ALLOWED_STR, UNKNOWN_STR);
    break;
  }
}

/**
  Prints supported memory modes

  @param[in] MemoryModes Byte describing supported memory modes
**/
STATIC
VOID
PrintSupportedMemoryModes(
  IN    PRINT_CONTEXT *pPrinterCtx,
  IN    CHAR16 *pPath,
  IN    SUPPORTED_MEMORY_MODE MemoryModes
  )
{
  BOOLEAN First = TRUE;
  CHAR16 *Val = NULL;

  if (MemoryModes.MemoryModesFlags.OneLm) {
    Val = CatSPrint(Val, ONE_LM_STR);
    First = FALSE;
  }
  if (MemoryModes.MemoryModesFlags.Memory) {
    if (First) {
      First = FALSE;
    } else {
      Val = CatSPrintClean(Val, L", ");
    }
    Val = CatSPrintClean(Val, MEMORY_STR);
  }
  if (MemoryModes.MemoryModesFlags.AppDirect) {
    if (First) {
      First = FALSE;
    }
    else {
      Val = CatSPrintClean(Val, L", ");
    }
    Val = CatSPrintClean(Val, APPDIRECT_STR);
  }
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, OPERATING_MODE_SUPPORT_STR, Val);
  FREE_POOL_SAFE(Val);
}

/**
  Execute the show system capabilities command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_ABORTED invoking CONFIG_PROTOGOL function failure
**/
EFI_STATUS
ShowSystemCapabilities(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;
  BOOLEAN FilterOutput = FALSE;
  BOOLEAN ShowAll = FALSE;
  CHAR16 *pDisplayValues = NULL;
  CHAR16 *pCapacityStr = NULL;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  CURRENT_MEMORY_MODE TempCurrentMode;
  SUPPORTED_MEMORY_MODE TempSupportedMode;
  DISPLAY_PREFERENCES DisplayPreferences;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  CHAR16 *TempAppDirSettings = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  SetMem(&SystemCapabilitiesInfo, sizeof(SystemCapabilitiesInfo), 0x0);

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = ReadRunTimePreferences(&DisplayPreferences, DISPLAY_CLI_INFO);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }

  UnitsToDisplay = DisplayPreferences.SizeUnit;

  ReturnCode = GetUnitsOption(pCmd, &UnitsOption);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Any valid units option will override the preferences **/
  if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
    UnitsToDisplay = UnitsOption;
  }

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetSystemCapabilitiesInfo(pNvmDimmConfigProtocol, &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION);
  if (pDisplayValues) {
    FilterOutput = TRUE;
  } else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      FilterOutput = TRUE;
    }
  }

  if (FilterOutput) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowSystemCapabilitiesDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowSystemCapabilitiesDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  ShowAll = (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT));

  if (FilterOutput && ShowAll) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    goto Finish;
  }

  PRINTER_BUILD_KEY_PATH(pPath, DS_ROOT_PATH);

  /** Values shown by default **/
  if (FilterOutput == ContainsValue(pDisplayValues, PLATFORM_CONFIG_SUPPORT_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PLATFORM_CONFIG_SUPPORT_STR, FORMAT_INT32,
        BIT_GET(SystemCapabilitiesInfo.PlatformConfigSupported, PLATFROM_CONFIG_SUPPORTED_BIT));
  }
  if (FilterOutput == ContainsValue(pDisplayValues, MEMORY_ALIGNMENT_STR)) {
    TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, Pow(2, SystemCapabilitiesInfo.InterleaveAlignmentSize),
                        UnitsToDisplay, TRUE, &pCapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_ALIGNMENT_STR, pCapacityStr);
    FREE_POOL_SAFE(pCapacityStr);
  }
  TempCurrentMode.MemoryMode = SystemCapabilitiesInfo.CurrentOperatingMode;
  if (FilterOutput == ContainsValue(pDisplayValues, VOLATILE_MODE_ALLOWED_STR)) {
    PrintAllowedVolatileMode(pPrinterCtx, pPath, TempCurrentMode);
  }
  if (FilterOutput == ContainsValue(pDisplayValues, VOLATILE_MODE_CURRENT_STR)) {
    PrintCurrentVolatileMode(pPrinterCtx, pPath, TempCurrentMode);
  }
  if (FilterOutput == ContainsValue(pDisplayValues, APPDIRECT_MODE_ALLOWED_STR)) {
    PrintAllowedAppDirectMode(pPrinterCtx, pPath, TempCurrentMode);
  }
  /** Values shown when -d/-a option specified **/
  if (ShowAll || ContainsValue(pDisplayValues, OPERATING_MODE_SUPPORT_STR)) {
    TempSupportedMode.MemoryModes = SystemCapabilitiesInfo.OperatingModeSupport;
    PrintSupportedMemoryModes(pPrinterCtx, pPath, TempSupportedMode);
  }

  if (ShowAll || ContainsValue(pDisplayValues, MIN_NAMESPACE_SIZE_STR)) {
    TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, SystemCapabilitiesInfo.MinNsSize, UnitsToDisplay, TRUE, &pCapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MIN_NAMESPACE_SIZE_STR, pCapacityStr);
    FREE_POOL_SAFE(pCapacityStr);
  }

  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_MIRROR_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, APPDIRECT_MIRROR_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.AppDirectMirrorSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, DIMM_SPARE_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DIMM_SPARE_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.DimmSpareSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_MIGRATION_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, APPDIRECT_MIGRATION_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.AppDirectMigrationSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, RENAME_NAMESPACE_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, RENAME_NAMESPACE_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.RenameNsSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, GROW_APPDIRECT_NAMESPACE_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, GROW_APPDIRECT_NAMESPACE_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.GrowPmNsSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, SHRINK_APPDIRECT_NAMESPACE_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SHRINK_APPDIRECT_NAMESPACE_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.ShrinkPmNsSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, INITIATE_SCRUB_SUPPORTED)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, INITIATE_SCRUB_SUPPORTED, FORMAT_INT32, SystemCapabilitiesInfo.InitiateScrubSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, ASYNCHRONOUS_DRAM_REFRESH_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ASYNCHRONOUS_DRAM_REFRESH_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.AdrSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, ERASE_DEVICE_DATA_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ERASE_DEVICE_DATA_SUPPORTED_STR, FORMAT_INT32, SystemCapabilitiesInfo.EraseDeviceDataSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, ENABLE_DEVICE_SECURITY_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ENABLE_DEVICE_SECURITY_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.EnableDeviceSecuritySupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, DISABLE_DEVICE_SECURITY_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DISABLE_DEVICE_SECURITY_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.DisableDeviceSecuritySupported); // supported for both OS and UEFI
  }

  if (ShowAll || ContainsValue(pDisplayValues, UNLOCK_DEVICE_SECURITY_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, UNLOCK_DEVICE_SECURITY_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.UnlockDeviceSecuritySupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, FREEZE_DEVICE_SECURITY_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, FREEZE_DEVICE_SECURITY_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.FreezeDeviceSecuritySupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.ChangeDevicePassphraseSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, CHANGE_MASTER_PASSPHRASE_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANGE_MASTER_PASSPHRASE_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.ChangeMasterPassphraseSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, MASTER_ERASE_DEVICE_DATA_SUPPORTED_STR)) {
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MASTER_ERASE_DEVICE_DATA_SUPPORTED_STR, FORMAT_INT32,
      SystemCapabilitiesInfo.MasterEraseDeviceDataSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_SETTINGS_SUPPORTED_STR)) {
    TempAppDirSettings = PrintAppDirectSettings(
      (VOID *)SystemCapabilitiesInfo.PtrInterleaveFormatsSupported,
      SystemCapabilitiesInfo.InterleaveFormatsSupportedNum,
      (INTERLEAVE_SIZE *)SystemCapabilitiesInfo.PtrInterleaveSize,
      FALSE, PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, APPDIRECT_SETTINGS_SUPPORTED_STR, TempAppDirSettings);
    FREE_POOL_SAFE(TempAppDirSettings);
  }

  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_SETTINGS_RECCOMENDED_STR)) {
    TempAppDirSettings = PrintAppDirectSettings(
      (VOID *)SystemCapabilitiesInfo.PtrInterleaveFormatsSupported,
      SystemCapabilitiesInfo.InterleaveFormatsSupportedNum,
      (INTERLEAVE_SIZE *)SystemCapabilitiesInfo.PtrInterleaveSize,
      TRUE, PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, APPDIRECT_SETTINGS_RECCOMENDED_STR, TempAppDirSettings);
    FREE_POOL_SAFE(TempAppDirSettings);
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveSize);
  FREE_POOL_SAFE(pDisplayValues);
  FREE_POOL_SAFE(pPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return  ReturnCode;
}

/**
  Register the show system capabilities command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowSystemCapabilitiesCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowSystemCapabilitiesCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
