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

/**
  Command syntax definition
**/
struct Command ShowSystemCapabilitiesCommand = {
  SHOW_VERB,                                                    //!< verb
  {                                                             //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
  },
  {                                                             //!< targets
    {SYSTEM_TARGET, L"", L"", TRUE, ValueEmpty},
    {CAPABILITIES_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                      //!< properties
  L"Show information about BIOS memory management capabilities.",
  ShowSystemCapabilities
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
  CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR
};

/**
  Prints allowed volatile mode

  @param[in] MemoryMode Byte describing allowed memory mode
**/
STATIC
VOID
PrintAllowedVolatileMode(
  IN    CURRENT_MEMORY_MODE MemoryMode
  )
{
  Print(FORMAT_STR L"=", VOLATILE_MODE_ALLOWED_STR);
  switch (MemoryMode.MemoryModeSplit.AllowedVolatileMode) {
  case VOLATILE_MODE_1LM:
    Print(ONE_LM_STR);
    break;
  case VOLATILE_MODE_MEMORY:
    Print(MEMORY_STR);
    break;
  default:
    Print(UNKNOWN_STR);
    break;
  }
  Print(L"\n");
}

/**
  Prints current volatile mode

  @param[in] MemoryMode Byte describing current memory mode
**/
STATIC
VOID
PrintCurrentVolatileMode(
  IN    CURRENT_MEMORY_MODE MemoryMode
  )
{
  Print(FORMAT_STR L"=", VOLATILE_MODE_CURRENT_STR);
  switch (MemoryMode.MemoryModeSplit.CurrentVolatileMode) {
  case VOLATILE_MODE_1LM:
    Print(ONE_LM_STR);
    break;
  case VOLATILE_MODE_MEMORY:
    Print(MEMORY_STR);
    break;
  default:
    Print(UNKNOWN_STR);
    break;
  }
  Print(L"\n");
}

/**
  Prints allowed appdirect mode

  @param[in] MemoryMode Byte describing allowed memory mode
**/
STATIC
VOID
PrintAllowedAppDirectMode(
  IN    CURRENT_MEMORY_MODE MemoryMode
  )
{
  Print(FORMAT_STR L"=", APPDIRECT_MODE_ALLOWED_STR);
  switch (MemoryMode.MemoryModeSplit.PersistentMode) {
  case PERSISTENT_MODE_DISABLED:
    Print(DISABLED_STR);
    break;
  case PERSISTENT_MODE_APP_DIRECT:
    Print(APPDIRECT_STR);
    break;
  case PERSISTENT_MODE_APP_DIRECT_CACHE:
    Print(APPDIRECT_STR);
    break;
  default:
    Print(UNKNOWN_STR);
    break;
  }
  Print(L"\n");
}

/**
  Prints supported memory modes

  @param[in] MemoryModes Byte describing supported memory modes
**/
STATIC
VOID
PrintSupportedMemoryModes(
  IN    SUPPORTED_MEMORY_MODE MemoryModes
  )
{
  BOOLEAN First = TRUE;

  Print(FORMAT_STR L": ", OPERATING_MODE_SUPPORT_STR);
  if (MemoryModes.MemoryModesFlags.OneLm) {
    Print(ONE_LM_STR);
    First = FALSE;
  }
  if (MemoryModes.MemoryModesFlags.Memory) {
    if (First) {
      First = FALSE;
    } else {
      Print(L", ");
    }
    Print(MEMORY_STR);
  }
  if (MemoryModes.MemoryModesFlags.AppDirect) {
    if (First) {
      First = FALSE;
    }
    else {
      Print(L", ");
    }
    Print(APPDIRECT_STR);
  }
  Print(L"\n");
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  SetMem(&SystemCapabilitiesInfo, sizeof(SystemCapabilitiesInfo), 0x0);

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    ReturnCode = EFI_NOT_FOUND;
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
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetSystemCapabilitiesInfo(pNvmDimmConfigProtocol, &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    Print(L"Error: GetSystemCapabilitiesInfo Failed\n");
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
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  ShowAll = (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT));

  if (FilterOutput && ShowAll) {
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  SetDisplayInfo(L"SystemCapabilities", ListView);

  /** Values shown by default **/
  if (FilterOutput == ContainsValue(pDisplayValues, PLATFORM_CONFIG_SUPPORT_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, PLATFORM_CONFIG_SUPPORT_STR,
        BIT_GET(SystemCapabilitiesInfo.PlatformConfigSupported, PLATFROM_CONFIG_SUPPORTED_BIT));
  }
  if (FilterOutput == ContainsValue(pDisplayValues, MEMORY_ALIGNMENT_STR)) {
    TempReturnCode = MakeCapacityString(Pow(2, SystemCapabilitiesInfo.InterleaveAlignmentSize),
                        UnitsToDisplay, TRUE, &pCapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);
    Print(FORMAT_STR L"=" FORMAT_STR_NL, MEMORY_ALIGNMENT_STR, pCapacityStr);
    FREE_POOL_SAFE(pCapacityStr);
  }
  TempCurrentMode.MemoryMode = SystemCapabilitiesInfo.CurrentOperatingMode;
  if (FilterOutput == ContainsValue(pDisplayValues, VOLATILE_MODE_ALLOWED_STR)) {
    PrintAllowedVolatileMode(TempCurrentMode);
  }
  if (FilterOutput == ContainsValue(pDisplayValues, VOLATILE_MODE_CURRENT_STR)) {
    PrintCurrentVolatileMode(TempCurrentMode);
  }
  if (FilterOutput == ContainsValue(pDisplayValues, APPDIRECT_MODE_ALLOWED_STR)) {
    PrintAllowedAppDirectMode(TempCurrentMode);
  }
  /** Values shown when -d/-a option specified **/
  if (ShowAll || ContainsValue(pDisplayValues, OPERATING_MODE_SUPPORT_STR)) {
    TempSupportedMode.MemoryModes = SystemCapabilitiesInfo.OperatingModeSupport;
    PrintSupportedMemoryModes(TempSupportedMode);
  }
  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_SETTINGS_SUPPORTED_STR)) {
    PrintAppDirectSettings(
      (INTERLEAVE_FORMAT *)SystemCapabilitiesInfo.PtrInterleaveFormatsSupported,
      SystemCapabilitiesInfo.InterleaveFormatsSupportedNum,
      FALSE, PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD);
  }
  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_SETTINGS_RECCOMENDED_STR)) {
    PrintAppDirectSettings(
      (INTERLEAVE_FORMAT *)SystemCapabilitiesInfo.PtrInterleaveFormatsSupported,
      SystemCapabilitiesInfo.InterleaveFormatsSupportedNum,
      TRUE, PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD);
  }
  if (ShowAll || ContainsValue(pDisplayValues, MIN_NAMESPACE_SIZE_STR)) {
    TempReturnCode = MakeCapacityString(SystemCapabilitiesInfo.MinNsSize, UnitsToDisplay, TRUE, &pCapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);
    Print(FORMAT_STR L"=" FORMAT_STR_NL, MIN_NAMESPACE_SIZE_STR, pCapacityStr);
    FREE_POOL_SAFE(pCapacityStr);
  }

  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_MIRROR_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, APPDIRECT_MIRROR_SUPPORTED_STR, SystemCapabilitiesInfo.AppDirectMirrorSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, DIMM_SPARE_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, DIMM_SPARE_SUPPORTED_STR, SystemCapabilitiesInfo.DimmSpareSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, APPDIRECT_MIGRATION_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, APPDIRECT_MIGRATION_SUPPORTED_STR, SystemCapabilitiesInfo.AppDirectMigrationSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, RENAME_NAMESPACE_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, RENAME_NAMESPACE_SUPPORTED_STR, SystemCapabilitiesInfo.RenameNsSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, GROW_APPDIRECT_NAMESPACE_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, GROW_APPDIRECT_NAMESPACE_SUPPORTED_STR, SystemCapabilitiesInfo.GrowPmNsSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, SHRINK_APPDIRECT_NAMESPACE_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, SHRINK_APPDIRECT_NAMESPACE_SUPPORTED_STR, SystemCapabilitiesInfo.ShrinkPmNsSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, INITIATE_SCRUB_SUPPORTED)) {
    Print(FORMAT_STR_EQ_DEC_NL, INITIATE_SCRUB_SUPPORTED, SystemCapabilitiesInfo.InitiateScrubSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, ASYNCHRONOUS_DRAM_REFRESH_SUPPORTED_STR)) {
	  Print(FORMAT_STR_EQ_DEC_NL, ASYNCHRONOUS_DRAM_REFRESH_SUPPORTED_STR, SystemCapabilitiesInfo.AdrSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, ERASE_DEVICE_DATA_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, ERASE_DEVICE_DATA_SUPPORTED_STR, SystemCapabilitiesInfo.EraseDeviceDataSupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, ENABLE_DEVICE_SECURITY_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, ENABLE_DEVICE_SECURITY_SUPPORTED_STR,
      SystemCapabilitiesInfo.EnableDeviceSecuritySupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, DISABLE_DEVICE_SECURITY_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, DISABLE_DEVICE_SECURITY_SUPPORTED_STR,
      SystemCapabilitiesInfo.DisableDeviceSecuritySupported); // supported for both OS and UEFI
  }

  if (ShowAll || ContainsValue(pDisplayValues, UNLOCK_DEVICE_SECURITY_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, UNLOCK_DEVICE_SECURITY_SUPPORTED_STR,
      SystemCapabilitiesInfo.UnlockDeviceSecuritySupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, FREEZE_DEVICE_SECURITY_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, FREEZE_DEVICE_SECURITY_SUPPORTED_STR,
      SystemCapabilitiesInfo.FreezeDeviceSecuritySupported);
  }

  if (ShowAll || ContainsValue(pDisplayValues, CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR)) {
    Print(FORMAT_STR_EQ_DEC_NL, CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR,
      SystemCapabilitiesInfo.ChangeDevicePassphraseSupported);
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  FREE_POOL_SAFE(pDisplayValues);

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
