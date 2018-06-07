/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>

#include "ShowRegionsCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include "Common.h"
#ifdef OS_BUILD
#include "BaseMemoryLib.h"
#endif
/**
  Command syntax definition
**/
struct Command ShowRegionsCommand =
{
  SHOW_VERB,                                           //!< verb
  {                                                    //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
  },
  {                                                    //!< targets
    {REGION_TARGET, L"", L"RegionIDs", TRUE, ValueOptional},
    { SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired },
  },
  {{L"", L"", L"", FALSE, ValueOptional}},             //!< properties
  L"Show information about one or more Regions.",        //!< help
  ShowRegions                                            //!< run function
};

CHAR16 *mppAllowedShowRegionsDisplayValues[] =
{
  REGION_ID_STR,
  PERSISTENT_MEM_TYPE_STR,
  TOTAL_CAPACITY_STR,
  FREE_CAPACITY_STR,
  SOCKET_ID_STR,
  REGION_HEALTH_STATE_STR,
  DIMM_ID_STR,
  ISET_ID_STR,
};

/**
  Register the show regions command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowRegionsCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowRegionsCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Convert the health state to a health string

@param[in] Health - Region Health State

@retval Const Pointer to Region Health State string
**/
STATIC
CONST CHAR16 *
RegionHealthToString(
  IN     UINT16 Health
)
{
  switch (Health) {
  case RegionHealthStateNormal:
    return HEALTHY_STATE;
  case RegionHealthStateError:
    return ERROR_STATE;
  case RegionHealthStatePending:
    return PENDING_STATE;
  case RegionHealthStateLocked:
    return LOCKED_STATE;
  case RegionHealthStateUnknown:
  default:
    return UNKNOWN_STATE;
  }
}


/**
  Execute the show regions command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOGOL function failure
**/
EFI_STATUS
ShowRegions(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT32 RegionCount = 0;
  REGION_INFO *pRegions = NULL;
  UINT16 *pRegionsIds = NULL;
  UINT32 RegionIdsNum = 0;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketsNum = 0;
  CHAR16 *pSocketsValue = NULL;
  CHAR16 *pRegionsValue = NULL;
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  CHAR16 *pDisplayValues = NULL;
  UINT32 Index = 0;
  UINT32 DimmIdx = 0;
  BOOLEAN Found = FALSE;
  CHAR16 *pRegionTempStr = NULL;
  BOOLEAN HeaderPrinted = FALSE;
  INTERLEAVE_FORMAT *pInterleaveFormat = NULL;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  CHAR16 *pCapacityStr = NULL;
  CHAR16 *pFreeCapacityStr = NULL;
  CONST CHAR16 *pHealthStateStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT32 AppDirectRegionCount = 0;

  NVDIMM_ENTRY();
  ReturnCode = EFI_SUCCESS;

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

#ifdef OS_BUILD
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
#endif

  /** initialize status structure **/
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
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

  /**
    If sockets were specified
  **/
  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSocketIds, &SocketsNum);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
    }

  /**
    if Region IDs were passed in, read them
  **/
  if (pCmd->targets[0].pTargetValueStr && StrLen(pCmd->targets[0].pTargetValueStr) > 0) {
    pRegionsValue = GetTargetValue(pCmd, REGION_TARGET);
    ReturnCode = GetUintsFromString(pRegionsValue, &pRegionsIds, &RegionIdsNum);

    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_REGION);
      goto Finish;
    }
  }
  /**
    if the all option was specified
  **/
  if (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT)) {
    AllOptionSet = TRUE;
  }
  /**
    if the display option was specified
  **/
  pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION);
  if (pDisplayValues) {
    DisplayOptionSet = TRUE;
  } else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      DisplayOptionSet = TRUE;
    }
  }

  /**
    Make sure they didn't specify both the all and display options
  **/
  if (AllOptionSet && DisplayOptionSet) {
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Check that the display parameters are correct (if display option is set) **/
  if (DisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowRegionsDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowRegionsDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
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

  pNvmDimmConfigProtocol->GetRegionCount(pNvmDimmConfigProtocol, &RegionCount);

  pRegions = AllocateZeroPool(sizeof(REGION_INFO) * RegionCount);
  if (pRegions == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetRegions(pNvmDimmConfigProtocol, RegionCount, pRegions, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
      ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
      DisplayCommandStatus(CLI_INFO_SHOW_REGION, L"", pCommandStatus);
    } else {
      ReturnCode = EFI_ABORTED;
    }
    NVDIMM_WARN("Failed to retrieve the REGION list");
    goto Finish;
  }

  for (Index = 0; Index < RegionCount; Index++) {
    if (((pRegions[Index].RegionType & PM_TYPE_AD) != 0) ||
      ((pRegions[Index].RegionType & PM_TYPE_AD_NI) != 0)) {
      AppDirectRegionCount++;
    }
  }

  if (AppDirectRegionCount == 0) {
    Print(L"There are no Regions defined in the system.\n");
    goto Finish;
  }

  /**
    display a summary table of all regions
  **/
  if (!AllOptionSet && !DisplayOptionSet) {
    for (Index = 0; Index < RegionCount; Index++) {
      if (RegionIdsNum > 0 && !ContainUint(pRegionsIds, RegionIdsNum, pRegions[Index].RegionId)) {
        continue;
      }

      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pRegions[Index].SocketId)) {
        continue;
      }

      if (pRegions[Index].RegionType == PM_TYPE_STORAGE) {
        continue;
      }

      Found = TRUE;

      pRegionTempStr = RegionTypeToString(pRegions[Index].RegionType);

      if (!HeaderPrinted) {
        Print(FORMAT_SHOW_REGION_HEADER,
#ifndef OS_BUILD
              REGION_ID_STR,
#endif
              SOCKET_ID_STR,
#ifdef OS_BUILD
              ISET_ID_STR,
#endif
              PERSISTENT_MEM_TYPE_STR,
              TOTAL_CAPACITY_STR,
              FREE_CAPACITY_STR,
              REGION_HEALTH_STATE_STR);
        HeaderPrinted = TRUE;
      }

      ReturnCode = MakeCapacityString(pRegions[Index].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
      TempReturnCode = MakeCapacityString(pRegions[Index].FreeCapacity, UnitsToDisplay, TRUE, &pFreeCapacityStr);
      pHealthStateStr = RegionHealthToString(pRegions[Index].Health);

      Print(
          // Only print the 'RegionID' if on UEFI
#ifndef OS_BUILD
          L"%8d "
#endif
          L"%8d "
#ifdef OS_BUILD
          L"0x%016llx "
#endif
          L"%23ls %9ls %12ls %11ls\n",
#ifndef OS_BUILD
          pRegions[Index].RegionId,
#endif
          pRegions[Index].SocketId,
#ifdef OS_BUILD
          pRegions[Index].CookieId,
#endif
          pRegionTempStr,
        pCapacityStr,
          pFreeCapacityStr,
          pHealthStateStr);

      FREE_POOL_SAFE(pCapacityStr);
      FREE_POOL_SAFE(pFreeCapacityStr);
      FREE_POOL_SAFE(pRegionTempStr);
    }
  } else { // display detailed view
    for (Index = 0; Index < RegionCount; Index++) {
      /**
        Skip if the RegionId is not matching.
      **/
      if (RegionIdsNum > 0 && !ContainUint(pRegionsIds, RegionIdsNum, pRegions[Index].RegionId)) {
        continue;
      }

      /**
        Skip if the socket is not matching.
      **/
      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pRegions[Index].SocketId)) {
        continue;
      }

      if (pRegions[Index].RegionType == PM_TYPE_STORAGE) {
        continue;
      }

      Found = TRUE;

      // Add a space between region groups
      if (0 != Index) {
        Print(L"\n");
      }

      /**
        RegionId
      **/
      /* always print the region ID */
#ifdef OS_BUILD
      Print(L"---" FORMAT_STR L"=0x%016llx---\n", ISET_ID_STR, pRegions[Index].CookieId);
#else
      Print(L"---" FORMAT_STR L"=%04d---\n", REGION_ID_STR, pRegions[Index].RegionId);
#endif

      /**
      SocketId
      **/
      if (AllOptionSet ||
        (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
        Print(L"   " FORMAT_STR L"=0x%08x\n", SOCKET_ID_STR, pRegions[Index].SocketId);
      }

      /**
        Display all the persistent memory types supported by the region.
      **/
      if (AllOptionSet ||
          (DisplayOptionSet && ContainsValue(pDisplayValues, PERSISTENT_MEM_TYPE_STR))) {
        pRegionTempStr = RegionTypeToString(pRegions[Index].RegionType);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, PERSISTENT_MEM_TYPE_STR, pRegionTempStr);
        FREE_POOL_SAFE(pRegionTempStr);
      }

      /**
        Capacity
      **/
      if (AllOptionSet ||
          (DisplayOptionSet && ContainsValue(pDisplayValues, TOTAL_CAPACITY_STR))) {
        ReturnCode = MakeCapacityString(pRegions[Index].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, TOTAL_CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /**
        FreeCapacity
      **/
      if (AllOptionSet ||
          (DisplayOptionSet && ContainsValue(pDisplayValues, FREE_CAPACITY_STR))) {
        TempReturnCode = MakeCapacityString(pRegions[Index].FreeCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
        KEEP_ERROR(ReturnCode, TempReturnCode);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FREE_CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /**
      HealthState
      **/
      if (AllOptionSet ||
        (DisplayOptionSet && ContainsValue(pDisplayValues, REGION_HEALTH_STATE_STR))) {
        pHealthStateStr = RegionHealthToString(pRegions[Index].Health);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, REGION_HEALTH_STATE_STR, pHealthStateStr);
      }

      /**
      Dimms
      **/
      if (AllOptionSet ||
        (DisplayOptionSet && ContainsValue(pDisplayValues, DIMM_ID_STR))) {
        Print(L"   " FORMAT_STR L"=", DIMM_ID_STR);
        for (DimmIdx = 0; DimmIdx < pRegions[Index].DimmIdCount; DimmIdx++) {
          if (DimmIdx > 0) {
              Print(L", ");
      }
          Print(L"0x%04x", pRegions[Index].DimmId[DimmIdx]);
      }
        Print(L"\n");
      }

#ifndef OS_BUILD

      /**
      ISetID
      **/
      if (AllOptionSet ||
          (DisplayOptionSet && ContainsValue(pDisplayValues, ISET_ID_STR))) {
          Print(L"   " FORMAT_STR L"=0x%016llx\n", ISET_ID_STR, pRegions[Index].CookieId);
      }
#endif
      }
      }

  if (RegionIdsNum > 0 && !Found) {
    Print(FORMAT_STR_SPACE L"" FORMAT_STR_NL, CLI_ERR_INVALID_REGION_ID, pCmd->targets[0].pTargetValueStr);
    if (SocketsNum > 0) {
      Print(L"The specified region id might not exist on the specified Socket(s).\n");
    }
    ReturnCode = EFI_NOT_FOUND;
  }

Finish:
  if (pRegions != NULL) {
    for (Index = 0; Index < RegionCount; Index++) {
      pInterleaveFormat = (INTERLEAVE_FORMAT *) pRegions[Index].PtrInterlaveFormats;
      FREE_POOL_SAFE(pInterleaveFormat);
    }
  }
  FREE_POOL_SAFE(pRegions);
  FREE_POOL_SAFE(pDisplayValues);
  FREE_POOL_SAFE(pRegionsIds);
  FREE_POOL_SAFE(pSocketIds);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
