/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowDimmsCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <Convert.h>
#include <NvmInterface.h>
#include "Common.h"
#include "NvmDimmCli.h"
#include <NvmWorkarounds.h>
#include <ShowTopologyCommand.h>

/* Command syntax definition */
struct Command ShowDimmsCommand =
{
  SHOW_VERB,                                        //!< verb
  /**
    options
  **/
  {
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
  },
  /**
    targets
  **/
  {
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},             //!< properties
  L"Show information about one or more DCPMEM DIMMs.", //!< help
  ShowDimms                                            //!< run function
};

CHAR16 *mppAllowedShowDimmsDisplayValues[] =
{
  DIMM_ID_STR,
  SOCKET_ID_STR,
  FW_VER_STR,
  FW_API_VER_STR,
  INTERFACE_FORMAT_CODE_STR,
  CAPACITY_STR,
  MANAGEABILITY_STR,
  SECURITY_STR,
  HEALTH_STR,
  HEALTH_STATE_REASON_STR,
  FORM_FACTOR_STR,
  VENDOR_ID_STR,
  MANUFACTURER_ID_STR,
  DEVICE_ID_STR,
  REVISION_ID_STR,
  SUBSYSTEM_VENDOR_ID_STR,
  SUBSYSTEM_DEVICE_ID_STR,
  SUBSYSTEM_REVISION_ID_STR,
  MANUFACTURING_INFO_VALID,
  MANUFACTURING_LOCATION,
  MANUFACTURING_DATE,
  PART_NUMBER_STR,
  SERIAL_NUMBER_STR,
  DEVICE_LOCATOR_STR,
  MEMORY_CONTROLLER_STR,
  DATA_WIDTH_STR,
  TOTAL_WIDTH_STR,
  SPEED_STR,
  MEMORY_MODE_CAPACITY_STR,
  APPDIRECT_MODE_CAPACITY_STR,
  UNCONFIGURED_CAPACITY_STR,
  PACKAGE_SPARING_ENABLED_STR,
  PACKAGE_SPARING_LEVEL_STR,
  PACKAGE_SPARING_CAPABLE_STR,
  PACKAGE_SPARES_AVAILABLE_STR,
  IS_NEW_STR,
  FW_LOG_LEVEL_STR,
  BANK_LABEL_STR,
  MEMORY_TYPE_STR,
  FIRST_FAST_REFRESH_PROPERTY,
  MANUFACTURER_STR,
  CHANNEL_ID_STR,
  SLOT_ID_STR,
  CHANNEL_POS_STR,
  POWER_MANAGEMENT_ON_STR,
  PEAK_POWER_BUDGET_STR,
  AVG_POWER_BUDGET_STR,
  LAST_SHUTDOWN_STATUS_STR,
  DIMM_HANDLE_STR,
  DIMM_UID_STR,
  MODES_SUPPORTED_STR,
  SECURITY_CAPABILITIES_STR,
  DIMM_CONFIG_STATUS_STR,
  SKU_VIOLATION_STR,
  ARS_STATUS_STR,
  OVERWRITE_STATUS_STR,
  LAST_SHUTDOWN_TIME_STR,
  INACCESSIBLE_CAPACITY_STR,
  RESERVED_CAPACITY_STR,
  VIRAL_POLICY_STR,
  VIRAL_STATE_STR,
  AIT_DRAM_ENABLED_STR,
  BOOT_STATUS_STR,
  PHYSICAL_ID_STR,
  ERROR_INJECT_ENABLED_STR,
  MEDIA_TEMP_INJ_ENABLED_STR,
  SW_TRIGGERS_ENABLED_STR,
  POISON_ERR_INJ_CTR_STR,
  POISON_ERR_CLR_CTR_STR,
  MEDIA_TEMP_INJ_CTR_STR,
  SW_TRIGGER_CTR_STR,
#ifdef OS_BUILD
  ACTION_REQUIRED_STR,
  ACTION_REQUIRED_EVENTS_STR
#endif
};

CHAR16 *mppAllowedShowDimmsConfigStatuses[] = {
  CONFIG_STATUS_VALUE_VALID,
  CONFIG_STATUS_VALUE_NOT_CONFIG,
  CONFIG_STATUS_VALUE_BAD_CONFIG,
  CONFIG_STATUS_VALUE_BROKEN_INTERLEAVE,
  CONFIG_STATUS_VALUE_REVERTED,
  CONFIG_STATUS_VALUE_UNSUPPORTED,
};

/* local functions */
STATIC CHAR16 *HealthToString(UINT8 HealthState);
STATIC CHAR16 *ManageabilityToString(UINT8 ManageabilityState);
STATIC CHAR16 *FormFactorToString(UINT8 FormFactor);
STATIC CHAR16 *FwLogLevelToStr(UINT8 FwLogLevel);
STATIC CHAR16 *OverwriteDimmStatusToStr(UINT8 OverwriteDimmStatus);

/*
 * Register the show dimms command
 */
EFI_STATUS
RegisterShowDimmsCommand(
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVDIMM_ENTRY();
  Rc = RegisterCommand(&ShowDimmsCommand);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Execute the show dimms command
**/
EFI_STATUS
ShowDimms(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT32 DimmCount = 0;
  UINT32 UninitializedDimmCount = 0;
  DIMM_INFO *pDimms = NULL;
  DIMM_INFO *pUninitializedDimms = NULL;
  DIMM_INFO *pAllDimms = NULL;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketsNum = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pSocketsValue = NULL;
  CHAR16 *pSecurityStr = NULL;
  CHAR16 *pHealthStr = NULL;
  CHAR16 *pHealthStateReasonStr = NULL;
  CHAR16 *pManageabilityStr = NULL;
  CHAR16 *pFormFactorStr = NULL;
  CHAR16 *pDimmsValue = NULL;
  CHAR16 TmpFwVerString[MAX(FW_VERSION_LEN, FW_API_VERSION_LEN)];
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  CHAR16 *pDisplayValues = NULL;
  BOOLEAN Found = FALSE;
  BOOLEAN ShowAll = FALSE;
  BOOLEAN ContainSocketTarget = FALSE;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pAttributeStr =  NULL;
  CHAR16 *pCapacityStr = NULL;
  CHAR16 *pDimmErrStr = NULL;
  LAST_SHUTDOWN_STATUS_COMBINED LastShutdownStatus;
  DISPLAY_PREFERENCES DisplayPreferences;
  CHAR16 *pFormat = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  BOOLEAN ByteAddressable = FALSE;
  BOOLEAN BlockAddressable = FALSE;
#ifdef OS_BUILD
  CHAR16 *pActionReqStr = NULL;
#endif // OS_BUILD

  NVDIMM_ENTRY();
  ZeroMem(TmpFwVerString, sizeof(TmpFwVerString));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(&LastShutdownStatus, sizeof(LastShutdownStatus));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }
  ContainSocketTarget = ContainTarget(pCmd, SOCKET_TARGET);

  /**
    if sockets were specified
  **/
  if (ContainSocketTarget) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSocketIds, &SocketsNum);
    if (EFI_ERROR(ReturnCode)) {
      /** Error Code returned by function above **/
      NVDIMM_DBG("GetUintsFromString returned error");
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
  }

  /** if the all option was specified **/
  if (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT)) {
    AllOptionSet = TRUE;
  }
  /** if the display option was specified **/
  pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION);
  if (pDisplayValues) {
    DisplayOptionSet = TRUE;
  } else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      DisplayOptionSet = TRUE;
    }
  }

  /** make sure they didn't specify both the all and display options **/
  if (AllOptionSet && DisplayOptionSet) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Options used together");
    Print(FORMAT_STR, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
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

  /** make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  /** check that the display parameters are correct (if display option is set) **/
  if (DisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowDimmsDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowDimmsDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimmCount(pNvmDimmConfigProtocol, &UninitializedDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
  pUninitializedDimms = AllocateZeroPool(sizeof(*pUninitializedDimms) * UninitializedDimmCount);

  if (pDimms == NULL || pUninitializedDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  /** retrieve the DIMM list, populated for the minimal show -dimm call for now **/
  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
      DIMM_INFO_CATEGORY_SECURITY | DIMM_INFO_CATEGORY_SMART_AND_HEALTH, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, UninitializedDimmCount,
      pUninitializedDimms);

  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found thru SMBUS");
    goto Finish;
  }

  /** if a specific DIMM pid was passed in, set it **/
  if (pCmd->targets[0].pTargetValueStr && StrLen(pCmd->targets[0].pTargetValueStr) > 0) {
    pAllDimms = AllocateZeroPool(sizeof(*pAllDimms) * (DimmCount + UninitializedDimmCount));
    if (NULL == pAllDimms) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
    }
    CopyMem(pAllDimms, pDimms, sizeof(*pDimms) * DimmCount);
    CopyMem(&pAllDimms[DimmCount], pUninitializedDimms, sizeof(*pUninitializedDimms) * UninitializedDimmCount);
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pDimmsValue, pAllDimms, DimmCount + UninitializedDimmCount, &pDimmIds,
        &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
  }

  if (SocketsNum > 0) {
    Found = FALSE;
    for (Index = 0; Index < DimmCount; Index++) {
      if (ContainUint(pSocketIds, SocketsNum, pDimms[Index].SocketId)) {
        Found = TRUE;
        break;
      }
    }

    if (!Found) {
      Print(FORMAT_STR_NL, CLI_ERR_NO_DIMMS_ON_SOCKET);
      ReturnCode = EFI_NOT_FOUND;
      NVDIMM_DBG("No DIMMs on provided Socket");
      goto Finish;
    }
  }

  /** display a summary table of all dimms **/
  if (!AllOptionSet && !DisplayOptionSet) {
    SetDisplayInfo(L"DIMM", TableView);

    pFormat = FORMAT_SHOW_DIMM_HEADER;
#ifdef OS_BUILD
    Print(pFormat,
        DIMM_ID_STR,
        CAPACITY_STR,
        HEALTH_STR,
        ACTION_REQ_STR,
        SECURITY_STR,
        FW_VER_STR);
#else // OS_BUILD
    Print(pFormat,
        DIMM_ID_STR,
        CAPACITY_STR,
        SECURITY_STR,
        HEALTH_STR,
        FW_VER_STR);
#endif // OS_BUILD

    for (Index = 0; Index < DimmCount; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pDimms[Index].SocketId)) {
        continue;
      }

      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }

      ReturnCode = MakeCapacityString(pDimms[Index].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
      pHealthStr = HealthToString(pDimms[Index].HealthState);

      if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_SECURITY_INFO) {
        pSecurityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
      } else {
        pSecurityStr = SecurityToString(gNvmDimmCliHiiHandle, pDimms[Index].SecurityState);
      }

      ConvertFwVersion(TmpFwVerString, pDimms[Index].FwVer.FwProduct,
          pDimms[Index].FwVer.FwRevision, pDimms[Index].FwVer.FwSecurityVersion, pDimms[Index].FwVer.FwBuild);

      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      pDimmErrStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);

#ifdef OS_BUILD
      pActionReqStr = CatSPrint(NULL, FORMAT_DEC, pDimms[Index].ActionRequired);
      pFormat = FORMAT_SHOW_DIMM_CONTENT;
      if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_UID) {
        Print(pFormat,
          pDimmErrStr,
          pCapacityStr,
          pHealthStr,
          pActionReqStr,
          pSecurityStr,
          TmpFwVerString);
      } else {
        Print(pFormat,
          DimmStr,
          pCapacityStr,
          pHealthStr,
          pActionReqStr,
          pSecurityStr,
          TmpFwVerString);
      }
#else // OS_BUILD
      if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_UID) {
        Print(pFormat,
          pDimmErrStr,
          pCapacityStr,
          pSecurityStr,
          pHealthStr,
          TmpFwVerString);
      } else {
        Print(pFormat,
          DimmStr,
          pCapacityStr,
          pSecurityStr,
          pHealthStr,
          TmpFwVerString);
      }
#endif // OS_BUILD
      FREE_POOL_SAFE(pDimmErrStr);
      FREE_POOL_SAFE(pHealthStr);
      FREE_POOL_SAFE(pSecurityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }

    /** show dimms from Uninitialized list **/

    for (Index = 0; Index < UninitializedDimmCount; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pUninitializedDimms[Index].SmbusAddress.Cpu)) {
        continue;
      }

      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pUninitializedDimms[Index].DimmID)) {
        continue;
      }

      pHealthStr = HealthToString(pUninitializedDimms[Index].HealthState);

      ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmCliHiiHandle,
        pUninitializedDimms[Index].HealthStausReason, &pHealthStateReasonStr);
      if (pHealthStateReasonStr == NULL || EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      ReturnCode = GetPreferredDimmIdAsString(pUninitializedDimms[Index].DimmHandle, NULL, DimmStr,
          MAX_DIMM_UID_LENGTH);
      pDimmErrStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);

      ConvertFwVersion(TmpFwVerString, pUninitializedDimms[Index].FwVer.FwProduct,
          pUninitializedDimms[Index].FwVer.FwRevision, pUninitializedDimms[Index].FwVer.FwSecurityVersion,
          pUninitializedDimms[Index].FwVer.FwBuild);

      TempReturnCode = MakeCapacityString(pUninitializedDimms[Index].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
#ifdef OS_BUILD
      if (pUninitializedDimms[Index].ErrorMask & DIMM_INFO_ERROR_UID) {
        Print(pFormat,
          pDimmErrStr,
          pCapacityStr,
          pHealthStr,
          L"N/A",
          L"",
          TmpFwVerString);
      } else {
        Print(pFormat,
          DimmStr,
          pCapacityStr,
          pHealthStr,
          L"N/A",
          L"",
          TmpFwVerString);
      }
#else // OS_BUILD
      if (pUninitializedDimms[Index].ErrorMask & DIMM_INFO_ERROR_UID) {
        Print(pFormat,
          pDimmErrStr,
          pCapacityStr,
          L"",
          pHealthStr,
          TmpFwVerString);
      } else {
        Print(pFormat,
          DimmStr,
          pCapacityStr,
          L"",
          pHealthStr,
          TmpFwVerString);
      }
#endif // OS_BUILD
      FREE_POOL_SAFE(pDimmErrStr);
      FREE_POOL_SAFE(pHealthStr);
      FREE_POOL_SAFE(pCapacityStr);
      FREE_POOL_SAFE(pHealthStateReasonStr);
    }
  }

  /** display detailed view **/
  else {
    SetDisplayInfo(L"DIMM", ListView);

    // Collect all properties if the user calls "show -a -dimm"
    ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
        DIMM_INFO_CATEGORY_ALL, pDimms);
    ShowAll = (!AllOptionSet && !DisplayOptionSet) || AllOptionSet;

    /** show dimms from Initialized list **/
    for (Index = 0; Index < DimmCount; Index++) {
      /** matching pid **/
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }

      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pDimms[Index].SocketId)) {
        continue;
      }

      /** always print the DimmID **/
      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid, DimmStr,
          MAX_DIMM_UID_LENGTH);
      if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_UID) {
        Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, UNKNOWN_ATTRIB_VAL);
      } else {
        Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, DimmStr);
      }

      /** Capacity **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CAPACITY_STR))) {
        ReturnCode = MakeCapacityString(pDimms[Index].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /** Security State **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SECURITY_STR))) {
        if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_SECURITY_INFO) {
          pSecurityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
        } else {
          pSecurityStr = SecurityToString(gNvmDimmCliHiiHandle, pDimms[Index].SecurityState);
        }
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, SECURITY_STR, pSecurityStr);
        FREE_POOL_SAFE(pSecurityStr);
      }

      /** Health State **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, HEALTH_STR))) {
        pHealthStr = HealthToString(pDimms[Index].HealthState);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, HEALTH_STR, pHealthStr);
        FREE_POOL_SAFE(pHealthStr);
      }

      /** Health State Reason**/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, HEALTH_STATE_REASON_STR))) {
        ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmCliHiiHandle,
          pDimms[Index].HealthStausReason, &pHealthStateReasonStr);
        if (pHealthStateReasonStr == NULL || EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, HEALTH_STATE_REASON_STR, pHealthStateReasonStr);
        FREE_POOL_SAFE(pHealthStateReasonStr);
      }

      /** FwVersion **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FW_VER_STR))) {
        ConvertFwVersion(TmpFwVerString, pDimms[Index].FwVer.FwProduct, pDimms[Index].FwVer.FwRevision,
          pDimms[Index].FwVer.FwSecurityVersion, pDimms[Index].FwVer.FwBuild);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FW_VER_STR, TmpFwVerString);
      }

      /** FwApiVersion **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FW_API_VER_STR))) {
        ConvertFwApiVersion(TmpFwVerString, pDimms[Index].FwVer.FwApiMajor, pDimms[Index].FwVer.FwApiMinor);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FW_API_VER_STR, TmpFwVerString);
      }

      /** InterfaceFormatCode **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, INTERFACE_FORMAT_CODE_STR))) {
        Print(L"   " FORMAT_STR L"=", INTERFACE_FORMAT_CODE_STR);
        if (pDimms[Index].InterfaceFormatCodeNum <= MAX_IFC_NUM) {
          for (Index2 = 0; Index2 < pDimms[Index].InterfaceFormatCodeNum; Index2++) {
            if (pDimms[Index].InterfaceFormatCode[Index2] == DCPMM_FMT_CODE_APP_DIRECT) {
              ByteAddressable = TRUE;
            } else if (pDimms[Index].InterfaceFormatCode[Index2] == DCPMM_FMT_CODE_STORAGE) {
              BlockAddressable = TRUE;
            }
          }

          if (ByteAddressable) {
            Print(L"0x%04x ", DCPMM_FMT_CODE_APP_DIRECT);
            Print(FORMAT_CODE_APP_DIRECT_STR);
          }

          if (pDimms[Index].InterfaceFormatCodeNum > 1) {
            Print(L", ");
          }

          if (BlockAddressable) {
            Print(L"0x%04x ", FORMAT_CODE_STORAGE_STR);
            Print(FORMAT_CODE_STORAGE_STR);
          }
        }
        Print(L"\n");
      }

      /** Manageability **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MANAGEABILITY_STR))) {
        pManageabilityStr = ManageabilityToString(pDimms[Index].ManageabilityState);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MANAGEABILITY_STR, pManageabilityStr);
        FREE_POOL_SAFE(pManageabilityStr);
      }

      /** PhysicalID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PHYSICAL_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, PHYSICAL_ID_STR, pDimms[Index].DimmID);
      }

      /** DimmHandle **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DIMM_HANDLE_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, DIMM_HANDLE_STR, pDimms[Index].DimmHandle);
      }

      /** DimmUID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DIMM_UID_STR))) {
        if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_UID) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, DIMM_UID_STR, UNKNOWN_ATTRIB_VAL);
        } else {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, DIMM_UID_STR, pDimms[Index].DimmUid);
        }
      }

      /** SocketId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SOCKET_ID_STR, pDimms[Index].SocketId);
      }

      /** MemoryControllerId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_CONTROLLER_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, MEMORY_CONTROLLER_STR, pDimms[Index].ImcId);
      }

      /** ChannelID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, CHANNEL_ID_STR, pDimms[Index].ChannelId);
      }

      /** ChannelPos **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_POS_STR))) {
        Print(FORMAT_3SPACE_STR_EQ_DEC_NL, CHANNEL_POS_STR, pDimms[Index].ChannelPos);
      }

      /** MemoryType **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_TYPE_STR))) {
        pAttributeStr = MemoryTypeToStr(pDimms[Index].MemoryType);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MEMORY_TYPE_STR, pAttributeStr);
        FREE_POOL_SAFE(pAttributeStr);
      }

      /** ManufacturerStr **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MANUFACTURER_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MANUFACTURER_STR, pDimms[Index].ManufacturerStr);
      }

      /** VendorId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, VENDOR_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, VENDOR_ID_STR, EndianSwapUint16(pDimms[Index].VendorId));
      }

      /** DeviceId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DEVICE_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, DEVICE_ID_STR, EndianSwapUint16(pDimms[Index].DeviceId));
      }

      /** RevisionId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, REVISION_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, REVISION_ID_STR, pDimms[Index].Rid);
      }

      /** SubsytemVendorId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SUBSYSTEM_VENDOR_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SUBSYSTEM_VENDOR_ID_STR,
          EndianSwapUint16(pDimms[Index].SubsystemVendorId));
      }

      /** SubsytemDeviceId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SUBSYSTEM_DEVICE_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SUBSYSTEM_DEVICE_ID_STR,  pDimms[Index].SubsystemDeviceId);
      }

      /** SubsytemRevisionId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SUBSYSTEM_REVISION_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SUBSYSTEM_REVISION_ID_STR, pDimms[Index].SubsystemRid);
      }

      /** DeviceLocator **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DEVICE_LOCATOR_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, DEVICE_LOCATOR_STR, pDimms[Index].DeviceLocator);
      }

      /** ManufacturingInfoValid **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MANUFACTURING_INFO_VALID))) {
        Print(FORMAT_3SPACE_STR_EQ_DEC_NL, MANUFACTURING_INFO_VALID, pDimms[Index].ManufacturingInfoValid);
      }

      /** ManufacturingLocation **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MANUFACTURING_LOCATION))) {
        if (pDimms[Index].ManufacturingInfoValid) {
          Print(L"   " FORMAT_STR L"=0x%02x\n", MANUFACTURING_LOCATION, pDimms[Index].ManufacturingLocation);
        } else {
          Print(L"   " FORMAT_STR L"=N/A\n", MANUFACTURING_LOCATION);
        }
      }

      /** ManufacturingDate **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MANUFACTURING_DATE))) {
        if (pDimms[Index].ManufacturingInfoValid) {
          Print(L"   " FORMAT_STR L"=%02x-%02x\n", MANUFACTURING_DATE, pDimms[Index].ManufacturingDate & 0xFF,
              (pDimms[Index].ManufacturingDate >> 8) & 0xFF);
        } else {
          Print(L"   " FORMAT_STR L"=N/A\n", MANUFACTURING_DATE);
        }
      }

      /** SerialNumber **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SERIAL_NUMBER_STR))) {
        Print(L"   " FORMAT_STR L"=0x%08x\n", SERIAL_NUMBER_STR, EndianSwapUint32(pDimms[Index].SerialNumber));
      }

      /** PartNumber **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PART_NUMBER_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, PART_NUMBER_STR, pDimms[Index].PartNumber);
      }

      /** BankLabel **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BANK_LABEL_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, BANK_LABEL_STR, pDimms[Index].BankLabel);
      }

      /** DataWidth **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DATA_WIDTH_STR))) {
        Print(L"   " FORMAT_STR L"=%d b\n", DATA_WIDTH_STR, pDimms[Index].DataWidth);
      }

      /** TotalWidth **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, TOTAL_WIDTH_STR))) {
        Print(L"   " FORMAT_STR L"=%d b\n", TOTAL_WIDTH_STR, pDimms[Index].TotalWidth);
      }

      /** Speed **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SPEED_STR))) {
        Print(L"   " FORMAT_STR L"=%d MHz\n", SPEED_STR, pDimms[Index].Speed);
      }

      /** FormFactor **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FORM_FACTOR_STR))) {
        pFormFactorStr = FormFactorToString(pDimms[Index].FormFactor);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FORM_FACTOR_STR, pFormFactorStr);
        FREE_POOL_SAFE(pFormFactorStr);
      }

      /** If Dimm is Manageable, print rest of the attributes **/
      if (pDimms[Index].ManageabilityState) {
        /** ManufacturerId **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MANUFACTURER_ID_STR))) {
          Print(FORMAT_3SPACE_EQ_0X04HEX_NL, MANUFACTURER_ID_STR,
            EndianSwapUint16(pDimms[Index].ManufacturerId));
        }

        /** VolatileCapacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_MODE_CAPACITY_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            TempReturnCode = MakeCapacityString(pDimms[Index].VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MEMORY_MODE_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** AppDirectCapacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_MODE_CAPACITY_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            TempReturnCode = MakeCapacityString(pDimms[Index].AppDirectCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, APPDIRECT_MODE_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** UnconfiguredCapacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, UNCONFIGURED_CAPACITY_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            TempReturnCode = MakeCapacityString(pDimms[Index].UnconfiguredCapacity, UnitsToDisplay, TRUE,
                &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, UNCONFIGURED_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** InaccessibleCapacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, INACCESSIBLE_CAPACITY_STR))) {
          KEEP_ERROR(ReturnCode, TempReturnCode);
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            TempReturnCode = MakeCapacityString(pDimms[Index].InaccessibleCapacity, UnitsToDisplay, TRUE,
                &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, INACCESSIBLE_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** ReservedCapacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, RESERVED_CAPACITY_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            TempReturnCode = MakeCapacityString(pDimms[Index].ReservedCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, RESERVED_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** PackageSparingCapable **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARING_CAPABLE_STR))) {
          Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARING_CAPABLE_STR, pDimms[Index].PackageSparingCapable);
        }

        if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_PACKAGE_SPARING) {
          /** PackageSparingEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARING_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARING_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PackageSparingLevel **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARING_LEVEL_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARING_LEVEL_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PackageSparesAvailable **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARES_AVAILABLE_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARES_AVAILABLE_STR, UNKNOWN_ATTRIB_VAL);
          }
        } else {
          /** PackageSparingEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARING_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARING_ENABLED_STR, pDimms[Index].PackageSparingEnabled);
          }

          /** PackageSparingLevel **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARING_LEVEL_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARING_LEVEL_STR, pDimms[Index].PackageSparingLevel);
          }

          /** PackageSparesAvailable **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PACKAGE_SPARES_AVAILABLE_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, PACKAGE_SPARES_AVAILABLE_STR, pDimms[Index].PackageSparesAvailable);
          }
        }

        /** IsNew **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, IS_NEW_STR))) {
          Print(FORMAT_3SPACE_STR_EQ_DEC_NL, IS_NEW_STR, pDimms[Index].IsNew);
        }

        /** FWLogLevel **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FW_LOG_LEVEL_STR))) {
          pAttributeStr = FwLogLevelToStr(pDimms[Index].FWLogLevel);
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FW_LOG_LEVEL_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_OPTIONAL_CONFIG_DATA) {
          /** First fast refresh **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FIRST_FAST_REFRESH_PROPERTY))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, FIRST_FAST_REFRESH_PROPERTY, UNKNOWN_ATTRIB_VAL);
          }

          /** ViralPolicyEnable **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, VIRAL_POLICY_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, VIRAL_POLICY_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** ViralStatus **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, VIRAL_STATE_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, VIRAL_STATE_STR, UNKNOWN_ATTRIB_VAL);
          }
        } else {
          /** First fast refresh **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FIRST_FAST_REFRESH_PROPERTY))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, FIRST_FAST_REFRESH_PROPERTY, pDimms[Index].FirstFastRefresh);
          }

          /** ViralPolicyEnable **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, VIRAL_POLICY_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, VIRAL_POLICY_STR, pDimms[Index].ViralPolicyEnable);
          }

          /** ViralStatus **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, VIRAL_STATE_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, VIRAL_STATE_STR, pDimms[Index].ViralStatus);
          }
        }

        if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_POWER_MGMT) {
          /** PowerManagementEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, POWER_MANAGEMENT_ON_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, POWER_MANAGEMENT_ON_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PeakPowerBudget **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PEAK_POWER_BUDGET_STR))) {
            Print(L"   " FORMAT_STR L"=%d mW\n", PEAK_POWER_BUDGET_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** AvgPowerBudget **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, AVG_POWER_BUDGET_STR))) {
            Print(L"   " FORMAT_STR L"=%d mW\n", AVG_POWER_BUDGET_STR, UNKNOWN_ATTRIB_VAL);
          }
        } else {
          /** PowerManagementEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, POWER_MANAGEMENT_ON_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, POWER_MANAGEMENT_ON_STR, pDimms[Index].PowerManagementEnabled);
          }

          /** PeakPowerBudget **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PEAK_POWER_BUDGET_STR))) {
            Print(L"   " FORMAT_STR L"=%d mW\n", PEAK_POWER_BUDGET_STR, pDimms[Index].PeakPowerBudget);
          }

          /** AvgPowerBudget **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, AVG_POWER_BUDGET_STR))) {
            Print(L"   " FORMAT_STR L"=%d mW\n", AVG_POWER_BUDGET_STR, pDimms[Index].AvgPowerBudget);
          }
        }

        /** LastShutdownStatus **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, LAST_SHUTDOWN_STATUS_STR))) {
          LastShutdownStatus.AsUint32 = pDimms[Index].LastShutdownStatus;
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            pAttributeStr = LastShutdownStatusToStr(LastShutdownStatus);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, LAST_SHUTDOWN_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** LastShutdownTime **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, LAST_SHUTDOWN_TIME_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            pAttributeStr = GetTimeFormatString(pDimms[Index].LastShutdownTime);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, LAST_SHUTDOWN_TIME_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** ModesSupported **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MODES_SUPPORTED_STR))) {
          pAttributeStr = ModesSupportedToStr(pDimms[Index].ModesSupported);
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MODES_SUPPORTED_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** SecurityCapabilities **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SECURITY_CAPABILITIES_STR))) {
          pAttributeStr = SecurityCapabilitiesToStr(pDimms[Index].SecurityCapabilities);
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, SECURITY_CAPABILITIES_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** ConfigurationStatus **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DIMM_CONFIG_STATUS_STR))) {
          pAttributeStr = mppAllowedShowDimmsConfigStatuses[pDimms[Index].ConfigStatus];
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, DIMM_CONFIG_STATUS_STR, pAttributeStr);
        }

        /** SKUViolation **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SKU_VIOLATION_STR))) {
          pAttributeStr = mppAllowedShowDimmsConfigStatuses[pDimms[Index].ConfigStatus];
          Print(FORMAT_3SPACE_STR_EQ_DEC_NL, SKU_VIOLATION_STR, pDimms[Index].SKUViolation);
        }

        /** ARSStatus **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, ARS_STATUS_STR))) {
          pAttributeStr = ARSStatusToStr(pDimms[Index].ARSStatus);
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, ARS_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** OverwriteDimmStatus **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, OVERWRITE_STATUS_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_OVERWRITE_STATUS) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            pAttributeStr = OverwriteDimmStatusToStr(pDimms[Index].OverwriteDimmStatus);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, OVERWRITE_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** AitDramEnabled **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, AIT_DRAM_ENABLED_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, AIT_DRAM_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, AIT_DRAM_ENABLED_STR, pDimms[Index].AitDramEnabled);
          }
        }

        /** Boot Status **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BOOT_STATUS_STR))) {
          if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_BSR) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          } else {
            pAttributeStr = BootStatusBitmaskToStr(gNvmDimmCliHiiHandle, pDimms[Index].BootStatusBitmask);
          }
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, BOOT_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        if (pDimms[Index].ErrorMask & DIMM_INFO_ERROR_MEM_INFO_PAGE) {
          /** ErrorInjectionEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, ERROR_INJECT_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, ERROR_INJECT_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** MediaTemperatureInjectionEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEDIA_TEMP_INJ_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, MEDIA_TEMP_INJ_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** SoftwareTriggersEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SW_TRIGGERS_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, SW_TRIGGERS_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PoisonErrorInjectionsCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, POISON_ERR_INJ_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, POISON_ERR_INJ_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PoisonErrorClearCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, POISON_ERR_CLR_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, POISON_ERR_CLR_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** MediaTemperatureInjectionsCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEDIA_TEMP_INJ_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, MEDIA_TEMP_INJ_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** SoftwareTriggersCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SW_TRIGGER_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, SW_TRIGGER_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }
        } else {
          /** ErrorInjectionEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, ERROR_INJECT_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, ERROR_INJECT_ENABLED_STR, pDimms[Index].ErrorInjectionEnabled);
          }

          /** MediaTemperatureInjectionEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEDIA_TEMP_INJ_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, MEDIA_TEMP_INJ_ENABLED_STR,
              pDimms[Index].MediaTemperatureInjectionEnabled);
          }

          /** SoftwareTriggersEnabled **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SW_TRIGGERS_ENABLED_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, SW_TRIGGERS_ENABLED_STR, pDimms[Index].SoftwareTriggersEnabled);
          }

          /** PoisonErrorInjectionsCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, POISON_ERR_INJ_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, POISON_ERR_INJ_CTR_STR, pDimms[Index].PoisonErrorInjectionsCounter);
          }

          /** PoisonErrorClearCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, POISON_ERR_CLR_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, POISON_ERR_CLR_CTR_STR, pDimms[Index].PoisonErrorClearCounter);
          }

          /** MediaTemperatureInjectionsCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEDIA_TEMP_INJ_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, MEDIA_TEMP_INJ_CTR_STR, pDimms[Index].MediaTemperatureInjectionsCounter);
          }

          /** SoftwareTriggersCounter **/
          if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SW_TRIGGER_CTR_STR))) {
            Print(FORMAT_3SPACE_STR_EQ_DEC_NL, SW_TRIGGER_CTR_STR, pDimms[Index].SoftwareTriggersCounter);
          }
        }
#ifdef OS_BUILD
    /** ActionRequired **/
    if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, ACTION_REQUIRED_STR))) {
      Print(FORMAT_3SPACE_STR_EQ_DEC_NL, ACTION_REQUIRED_STR, pDimms[Index].ActionRequired);
    }

    /** ActionRequiredEvents **/
    if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, ACTION_REQUIRED_EVENTS_STR))) {
      Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, ACTION_REQUIRED_EVENTS_STR, L"N/A");
    }
#endif
      }
    }

    /** show dimms from Uninitialized list **/

    for (Index = 0; Index < UninitializedDimmCount; Index++) {
      /** matching pid **/
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pUninitializedDimms[Index].DimmID)) {
        continue;
      }

      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pUninitializedDimms[Index].SmbusAddress.Cpu)) {
        continue;
      }

      /** always print the Dimm Handle **/
      Print(L"---" FORMAT_STR L"=0x%04x---\n", DIMM_ID_STR, pUninitializedDimms[Index].DimmHandle);

      /** Capacity **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CAPACITY_STR))) {
        ReturnCode = MakeCapacityString(pUninitializedDimms[Index].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /** Health State **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, HEALTH_STR))) {
        pHealthStr = HealthToString(pUninitializedDimms[Index].HealthState);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, HEALTH_STR, pHealthStr);
        FREE_POOL_SAFE(pHealthStr);
      }
      /** Health State reason**/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, HEALTH_STATE_REASON_STR))) {
        ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmCliHiiHandle,
          pUninitializedDimms[Index].HealthStausReason, &pHealthStateReasonStr);
        if (pHealthStateReasonStr == NULL || EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, HEALTH_STATE_REASON_STR, pHealthStateReasonStr);
        FREE_POOL_SAFE(pHealthStateReasonStr);
      }

      // TODO: Order of Attributes need to be defined in spec still
      /** SubsytemDeviceId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SUBSYSTEM_DEVICE_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SUBSYSTEM_DEVICE_ID_STR, pUninitializedDimms[Index].SubsystemDeviceId);
      }

      /** SubsytemRevisionId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SUBSYSTEM_REVISION_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SUBSYSTEM_REVISION_ID_STR, pUninitializedDimms[Index].SubsystemRid);
      }

      /** SocketId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SOCKET_ID_STR, pUninitializedDimms[Index].SocketId);
      }

      /** MemoryControllerId **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_CONTROLLER_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, MEMORY_CONTROLLER_STR, pUninitializedDimms[Index].ImcId);
      }

      /** ChannelID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, CHANNEL_ID_STR, pUninitializedDimms[Index].ChannelId);
      }

      /** ChannelPos **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_POS_STR))) {
        Print(FORMAT_3SPACE_STR_EQ_DEC_NL, CHANNEL_POS_STR, pUninitializedDimms[Index].ChannelPos);
      }

      /** Boot Status **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BOOT_STATUS_STR))) {
        if (pUninitializedDimms[Index].ErrorMask & DIMM_INFO_ERROR_BSR) {
          pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
        } else {
          pAttributeStr = BootStatusBitmaskToStr(gNvmDimmCliHiiHandle, pUninitializedDimms[Index].BootStatusBitmask);
        }
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, BOOT_STATUS_STR, pAttributeStr);
        FREE_POOL_SAFE(pAttributeStr);
      }

      /** SerialNumber **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SERIAL_NUMBER_STR))) {
        Print(L"   " FORMAT_STR L"=0x%08x\n", SERIAL_NUMBER_STR, pUninitializedDimms[Index].SerialNumber);
      }

      /** FwVersion **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FW_VER_STR))) {
        ConvertFwVersion(TmpFwVerString, pUninitializedDimms[Index].FwVer.FwProduct,
        pUninitializedDimms[Index].FwVer.FwRevision, pUninitializedDimms[Index].FwVer.FwSecurityVersion,
        pUninitializedDimms[Index].FwVer.FwBuild);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FW_VER_STR, TmpFwVerString);
      }

      /** FwApiVersion **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, FW_API_VER_STR))) {
        ConvertFwApiVersion(TmpFwVerString, pUninitializedDimms[Index].FwVer.FwApiMajor,
            pUninitializedDimms[Index].FwVer.FwApiMinor);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FW_API_VER_STR, TmpFwVerString);
      }

      /** PartNumber **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PART_NUMBER_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, PART_NUMBER_STR, pUninitializedDimms[Index].PartNumber);
      }
    }
  }

Finish:
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pAllDimms);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDisplayValues);
  FREE_POOL_SAFE(pUninitializedDimms);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert health state to a string
**/
STATIC
CHAR16*
HealthToString(
  IN     UINT8 HealthState
  )
{
  CHAR16 *pHealthString = NULL;
  switch(HealthState) {
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
    case HEALTH_UNMANAGEABLE:
      pHealthString = CatSPrint(NULL, FORMAT_STR, UNMANAGEABLE_STR);
      break;
    case HEALTH_NONFUNCTIONAL:
      pHealthString = CatSPrint(NULL, FORMAT_STR, NONFUNCTIONAL_STR);
      break;
    case HEALTH_UNKNOWN:
    default:
      pHealthString = CatSPrint(NULL, FORMAT_STR, UNKNOWN_STR);
      break;
  }
  return pHealthString;
}

/**
  Convert manageability state to a string
**/
STATIC
CHAR16*
ManageabilityToString(
  IN     UINT8 ManageabilityState
  )
{
  CHAR16 *pManageabilityString = NULL;

  switch(ManageabilityState) {
    case MANAGEMENT_VALID_CONFIG:
      pManageabilityString = CatSPrint(NULL, FORMAT_STR, L"Manageable");
      break;
    case MANAGEMENT_INVALID_CONFIG:
    default:
      pManageabilityString = CatSPrint(NULL, FORMAT_STR, L"Unmanageable");
      break;
  }
  return pManageabilityString;
}

/**
  Convert type to string
**/
STATIC
CHAR16*
FormFactorToString(
  IN     UINT8 FormFactor
  )
{
  CHAR16 *pFormFactorStr = NULL;
  switch(FormFactor) {
    case FORM_FACTOR_DIMM:
      pFormFactorStr = CatSPrint(NULL, FORMAT_STR, L"DIMM");
      break;
    case FORM_FACTOR_SODIMM:
      pFormFactorStr = CatSPrint(NULL, FORMAT_STR, L"SODIMM");
      break;
    default:
      pFormFactorStr = CatSPrint(NULL, FORMAT_STR, L"Other");
      break;
  }
  return pFormFactorStr;
}

/**
  Convert type to string
**/
STATIC
CHAR16*
FwLogLevelToStr(
  IN     UINT8 FwLogLevel
  )
{
  switch(FwLogLevel) {
    case FW_LOG_LEVEL_DISABLED:
      return CatSPrint(NULL, FORMAT_STR, FW_LOG_LEVEL_DISABLED_STR);
      break;
    case FW_LOG_LEVEL_ERROR:
      return CatSPrint(NULL, FORMAT_STR, FW_LOG_LEVEL_ERROR_STR);
      break;
    case FW_LOG_LEVEL_WARNING:
      return CatSPrint(NULL, FORMAT_STR, FW_LOG_LEVEL_WARNING_STR);
      break;
    case FW_LOG_LEVEL_INFO:
      return CatSPrint(NULL, FORMAT_STR, FW_LOG_LEVEL_INFO_STR);
      break;
    case FW_LOG_LEVEL_DEBUG:
      return CatSPrint(NULL, FORMAT_STR, FW_LOG_LEVEL_DEBUG_STR);
      break;
    default:
      return CatSPrint(NULL, FORMAT_STR, FW_LOG_LEVEL_UNKNOWN_STR);
      break;
  }
}


/**
  Convert overwrite DIMM status value to string
**/
STATIC
CHAR16 *
OverwriteDimmStatusToStr(
  IN     UINT8 OverwriteDimmStatus
  )
{
  CHAR16 *pOverwriteDimmStatusStr = NULL;

  NVDIMM_ENTRY();

  switch (OverwriteDimmStatus) {
    case OVERWRITE_DIMM_STATUS_COMPLETED:
      pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_COMPLETED_STR);
      break;
    case OVERWRITE_DIMM_STATUS_IN_PROGRESS:
      pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_IN_PROGRESS_STR);
      break;
    case OVERWRITE_DIMM_STATUS_NOT_STARTED:
      pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_NOT_STARTED_STR);
      break;
    case OVERWRITE_DIMM_STATUS_UNKNOWN:
    default:
      pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_UNKNOWN_STR);
      break;
  }

  NVDIMM_EXIT();
  return pOverwriteDimmStatusStr;
}
