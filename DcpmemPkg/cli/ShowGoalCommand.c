/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "ShowGoalCommand.h"
#include "Common.h"
#include "Convert.h"
#ifdef OS_BUILD
#include "event.h"
#endif // OS_BUILD

/**
  Command syntax definition
**/
struct Command ShowGoalCommand =
{
  SHOW_VERB,                                                          //!< verb
  {                                                                   //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
#ifdef OS_BUILD
    , {ACTION_REQ_OPTION_SHORT, ACTION_REQ_OPTION, L"", L"", FALSE, ValueEmpty}
#endif // OS_BUILD
  },
  {                                                                   //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Show region configuration goal stored on one or more AEPs",         //!< help
  ShowGoal
};


CHAR16 *mppAllowedShowGoalDisplayValues[] =
{
  SOCKET_ID_STR,
  DIMM_ID_STR,
  MEMORY_SIZE_PROPERTY,
  APPDIRECT_SIZE_PROPERTY,
  APPDIRECT_INDEX_PROPERTY,
  APPDIRECT_1_SIZE_PROPERTY,
  APPDIRECT_1_SETTINGS_PROPERTY,
  APPDIRECT_1_INDEX_PROPERTY,
  APPDIRECT_2_SIZE_PROPERTY,
  APPDIRECT_2_SETTINGS_PROPERTY,
  APPDIRECT_2_INDEX_PROPERTY,
  STATUS_STR
#ifdef OS_BUILD
  ,ACTION_REQ_PROPERTY,
  ACTION_REQ_EVENTS_PROPERTY
#endif // OS_BUILD
};

/**
  Print results of show goal according to table view

  @param[in] pRegionConfigsInfo - Region Config table to be printed
  @param[in] CurrentUnits The requested type of units to convert the capacity into
  @param[in] RegionConfigsCount - Number of elements in array
  #ifdef OS_BUILD
  @param[in] pDimmInfo - pointer to the dimm info list, if not NULL action required
  will be displayed
  #endif // OSBUILD

  @retval EFI_SUCCESS if printing is successful
  @retval EFI_INVALID_PARAMETER if input parameter is incorrect
**/
#ifdef OS_BUILD
EFI_STATUS
ShowGoalPrintTableView(
  IN    REGION_GOAL_PER_DIMM_INFO *pRegionConfigsInfo,
  IN    UINT16 CurrentUnits,
  IN    UINT32 RegionConfigsCount,
  IN    DIMM_INFO *pDimmInfo,
  IN    BOOLEAN Buffered
  )
#else // OS_BUILD
EFI_STATUS
ShowGoalPrintTableView(
    IN    REGION_GOAL_PER_DIMM_INFO *pRegionConfigsInfo,
    IN    UINT16 CurrentUnits,
    IN    UINT32 RegionConfigsCount,
    IN    BOOLEAN Buffered
)
#endif // OSBUILD
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *pVolatileCapacityStr = NULL;
  CHAR16 *pAppDirect1CapacityStr = NULL;
  CHAR16 *pAppDirect2CapacityStr = NULL;
  REGION_GOAL_PER_DIMM_INFO *pCurrentGoal = NULL;
#ifdef OS_BUILD
  CHAR16 *pActionReqStr = NULL;
#endif // OS_BUILD

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pRegionConfigsInfo == NULL) {
    goto Finish;
  }

  SetDisplayInfo(L"ConfigGoal", TableView);

#ifdef OS_BUILD
  if (pDimmInfo != NULL) {
      // Print the action required status
     NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, FORMAT_SHOW_GOAL_AR_HEADER, SOCKET_ID_TABLE_HEADER, DIMM_ID_TABLE_HEADER,
          MEMORY_SIZE_TABLE_HEADER, APPDIRECT_1_SIZE_TABLE_HEADER,
          APPDIRECT_2_SIZE_TABLE_HEADER, ACTION_REQUIRED_HEADER);
  } else {
     NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, FORMAT_SHOW_GOAL_HEADER, SOCKET_ID_TABLE_HEADER, DIMM_ID_TABLE_HEADER,
          MEMORY_SIZE_TABLE_HEADER, APPDIRECT_1_SIZE_TABLE_HEADER,
          APPDIRECT_2_SIZE_TABLE_HEADER);
  }
#else // OS_BUILD
  NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, FORMAT_SHOW_GOAL_HEADER, SOCKET_ID_TABLE_HEADER, DIMM_ID_TABLE_HEADER,
    MEMORY_SIZE_TABLE_HEADER, APPDIRECT_1_SIZE_TABLE_HEADER,
    APPDIRECT_2_SIZE_TABLE_HEADER);
#endif // OS_BUILD

  for (Index = 0; Index < RegionConfigsCount; ++Index) {
    pCurrentGoal = &pRegionConfigsInfo[Index];

    ReturnCode = GetPreferredDimmIdAsString(pCurrentGoal->DimmID, pCurrentGoal->DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    TempReturnCode = MakeCapacityString(ROUNDDOWN(pCurrentGoal->VolatileSize, SIZE_1GB), CurrentUnits,
        TRUE, &pVolatileCapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);

    TempReturnCode = MakeCapacityString(pCurrentGoal->AppDirectSize[0],
        CurrentUnits, TRUE, &pAppDirect1CapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);

    TempReturnCode = MakeCapacityString(pCurrentGoal->AppDirectSize[1],
        CurrentUnits, TRUE, &pAppDirect2CapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);

    /** Print single row **/
#ifdef OS_BUILD
    if (pDimmInfo != NULL) {
        pActionReqStr = CatSPrint(NULL, FORMAT_DEC, pDimmInfo[Index].ActionRequired);
        // Print the action required status
        NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, FORMAT_SHOW_GOAL_AR_SINGLE,
            pCurrentGoal->SocketId,
            DimmStr,
            pVolatileCapacityStr,
            pAppDirect1CapacityStr,
            pAppDirect2CapacityStr,
            pActionReqStr);
    } else {
       NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, FORMAT_SHOW_GOAL_SINGLE,
            pCurrentGoal->SocketId,
            DimmStr,
            pVolatileCapacityStr,
            pAppDirect1CapacityStr,
            pAppDirect2CapacityStr);
    }
#else // OS_BUILD
    NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, FORMAT_SHOW_GOAL_SINGLE,
        pCurrentGoal->SocketId,
        DimmStr,
        pVolatileCapacityStr,
        pAppDirect1CapacityStr,
        pAppDirect2CapacityStr);
#endif // OS_BUILD

    FREE_POOL_SAFE(pVolatileCapacityStr);
    FREE_POOL_SAFE(pAppDirect1CapacityStr);
    FREE_POOL_SAFE(pAppDirect2CapacityStr);
  }

Finish:
  return ReturnCode;
}

/**
  Print results of show goal according to detailed view

  @param[in] pRegionConfigsInfo - Region Config table to be printed
  @param[in] RegionConfigsCount - Number of elements in array
  @param[in] AllOptionSet - Print all display options
  @param[in] DisplayOptionSet - Print specified display options
  @param[in] CurrentUnits The requested type of units to convert the capacity into
  @param[in] pDisplayValues - Selected display options

  @retval EFI_SUCCESS if printing is successful
  @retval EFI_INVALID_PARAMETER if input parameter is incorrect
**/
EFI_STATUS
ShowGoalPrintDetailedView(
  IN    REGION_GOAL_PER_DIMM_INFO *pRegionConfigsInfo,
  IN    UINT32 RegionConfigsCount,
  IN    BOOLEAN AllOptionSet,
  IN    BOOLEAN DisplayOptionSet,
  IN    UINT16 CurrentUnits,
  IN    CHAR16 *pDisplayValues
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  REGION_GOAL_PER_DIMM_INFO *pCurrentGoal = NULL;
  CHAR16 *pSettingsString = NULL;
  CHAR16 *pStatusString = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *pCapacityStr = NULL;

  ZeroMem(DimmStr, sizeof(DimmStr));

  SetDisplayInfo(L"ConfigGoal", ListView);

  if (pRegionConfigsInfo == NULL || (DisplayOptionSet && pDisplayValues == NULL)) {
    goto Finish;
  }

  for (Index = 0; Index < RegionConfigsCount; ++Index) {
    pCurrentGoal = &pRegionConfigsInfo[Index];

    /* always print dimmID */
    /** Dimm ID **/
    ReturnCode = GetPreferredDimmIdAsString(pCurrentGoal->DimmID, pCurrentGoal->DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, DimmStr);

    /** Socket Id **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
      Print(L"   " FORMAT_STR L"=0x%x\n", SOCKET_ID_STR, pCurrentGoal->SocketId);
    }
    /** Volatile Size **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_SIZE_PROPERTY))) {
      ReturnCode = MakeCapacityString(ROUNDDOWN(pCurrentGoal->VolatileSize, SIZE_1GB), CurrentUnits,
          TRUE, &pCapacityStr);
      Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MEMORY_SIZE_PROPERTY, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }
    /** AppDirect1Size **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_1_SIZE_PROPERTY))) {
      TempReturnCode = MakeCapacityString(pCurrentGoal->AppDirectSize[0], CurrentUnits, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      Print((pCurrentGoal->InterleaveSetType[0] == MIRRORED ? L"   " FORMAT_STR L"=" FORMAT_STR L" Mirrored\n" : L"   " FORMAT_STR L"=" FORMAT_STR L"\n"),
          APPDIRECT_1_SIZE_PROPERTY, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }
    /** AppDirect1Index **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_1_INDEX_PROPERTY))) {
      Print(FORMAT_3SPACE_STR_EQ_DEC_NL, APPDIRECT_1_INDEX_PROPERTY, pCurrentGoal->AppDirectIndex[0]);
    }
    /** AppDirect1Settings **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_1_SETTINGS_PROPERTY))) {
      InterleaveSettingsToString(pCurrentGoal->AppDirectSize[0], pCurrentGoal->NumberOfInterleavedDimms[0],
          pCurrentGoal->ImcInterleaving[0], pCurrentGoal->ChannelInterleaving[0], &pSettingsString);
      Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, APPDIRECT_1_SETTINGS_PROPERTY, pSettingsString);
      FREE_POOL_SAFE(pSettingsString);
    }
    /** AppDirect2Size **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_2_SIZE_PROPERTY))) {
      TempReturnCode = MakeCapacityString(pCurrentGoal->AppDirectSize[1], CurrentUnits, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      Print((pCurrentGoal->InterleaveSetType[1] == MIRRORED ? L"   " FORMAT_STR L"=" FORMAT_STR L" Mirrored\n" : L"   " FORMAT_STR L"=" FORMAT_STR L"\n"),
          APPDIRECT_2_SIZE_PROPERTY, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }
    /** AppDirect2Index **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_2_INDEX_PROPERTY))) {
      Print(FORMAT_3SPACE_STR_EQ_DEC_NL, APPDIRECT_2_INDEX_PROPERTY, pCurrentGoal->AppDirectIndex[1]);
    }
    /** AppDirect2Settings **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_2_SETTINGS_PROPERTY))) {
      InterleaveSettingsToString(pCurrentGoal->AppDirectSize[1], pCurrentGoal->NumberOfInterleavedDimms[1],
          pCurrentGoal->ImcInterleaving[1], pCurrentGoal->ChannelInterleaving[1], &pSettingsString);
      Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, APPDIRECT_2_SETTINGS_PROPERTY, pSettingsString);
      FREE_POOL_SAFE(pSettingsString);
    }
    /** Status **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, STATUS_STR))) {
      pStatusString = GoalStatusToString(gNvmDimmCliHiiHandle, pCurrentGoal->Status);
      Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, STATUS_STR, pStatusString);
      FREE_POOL_SAFE(pStatusString);
    }
#ifdef OS_BUILD
    CHAR8 AsciiDimmUid[MAX_DIMM_UID_LENGTH];
    /** ActionRequired **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, ACTION_REQ_PROPERTY))) {
        UnicodeStrToAsciiStrS(pCurrentGoal->DimmUid, AsciiDimmUid, MAX_DIMM_UID_LENGTH);
        Print(FORMAT_3SPACE_STR_EQ_DEC_NL, ACTION_REQ_PROPERTY, (UINT8) nvm_get_action_required(AsciiDimmUid));
    }
    /** ActionRequiredEvents **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, ACTION_REQ_EVENTS_PROPERTY))) {
        CHAR8 *StringBuffer = NULL;
        UnicodeStrToAsciiStrS(pCurrentGoal->DimmUid, AsciiDimmUid, MAX_DIMM_UID_LENGTH);
        nvm_get_events_from_file(SYSTEM_EVENT_TYPE_AR_STATUS_SET(TRUE) | SYSTEM_EVENT_TYPE_AR_EVENT_SET(TRUE),
                                AsciiDimmUid, SYSTEM_EVENT_NOT_APPLICABLE,
                                SYSTEM_EVENT_NOT_APPLICABLE, NULL, &StringBuffer);
        if (NULL != StringBuffer) {
            size_t StringLen = AsciiStrSize(StringBuffer) + 1;
            CHAR16 *WStringBuffer = NULL;
            WStringBuffer = (CHAR16 *)AllocateZeroPool(StringLen * sizeof(CHAR16));
            if (NULL != WStringBuffer) {
                if (RETURN_SUCCESS != AsciiStrToUnicodeStrS(StringBuffer, WStringBuffer, StringLen)) {
                    ReturnCode = EFI_ABORTED;
                    FREE_POOL_SAFE(StringBuffer);
                    FREE_POOL_SAFE(WStringBuffer);
                    goto Finish;
                }
            }
            else {
                ReturnCode = EFI_BUFFER_TOO_SMALL;
                FREE_POOL_SAFE(StringBuffer);
                goto Finish;
            }
            FREE_POOL_SAFE(StringBuffer);
            Print(FORMAT_3SPACE_STR_NL, ACTION_REQ_EVENTS_PROPERTY L"=");
            Print(FORMAT_STR, WStringBuffer);
            FREE_POOL_SAFE(WStringBuffer);
        }
        else {
            Print(FORMAT_3SPACE_STR_NL, ACTION_REQ_EVENTS_PROPERTY L"=N/A");
        }
    }
    Print(FORMAT_STR_NL, L"");
#endif // OS_BUILD
  }
Finish:
  return ReturnCode;
}
/**
  Execute the Show Goal command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowGoal(
  IN     struct Command *pCmd
  )
{
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  UINT16 *pDimmIds = NULL;
  UINT16 *pSocketIds = NULL;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdAepCliDefaultCapacityUnit);
  UINT32 DimmIdsCount = 0;
  UINT32 SocketIdsCount = 0;
  UINT32 RegionConfigsCount = 0;
  CHAR16 *pSettingsString = NULL;
  CHAR16 *pDisplayValues = NULL;
  CHAR16 *pTargetValue = NULL;
  EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  REGION_GOAL_PER_DIMM_INFO RegionConfigsInfo[MAX_DIMMS];
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  DISPLAY_PREFERENCES DisplayPreferences;
#ifdef OS_BUILD
  BOOLEAN ActionReqSet = FALSE;
#endif // OS_BILD

  NVDIMM_ENTRY();

  SetMem(RegionConfigsInfo, sizeof(RegionConfigsInfo), 0x0);
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
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
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

  // check sockets
  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pTargetValue, &pSocketIds, &SocketIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      NVDIMM_DBG("Failed on GetUintsFromString");
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

  /** if the all option was specified **/
  if (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT)) {
    AllOptionSet = TRUE;
  }
#ifdef OS_BUILD
  /** if the action required option was specified **/
  if (containsOption(pCmd, ACTION_REQ_OPTION) || containsOption(pCmd, ACTION_REQ_OPTION_SHORT)) {
      ActionReqSet = TRUE;
  }
#endif // OS_BUILD
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
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    goto Finish;
  }

  if (ContainsValue(pDisplayValues, APPDIRECT_1_INDEX_PROPERTY) &&
      ContainsValue(pDisplayValues, APPDIRECT_INDEX_PROPERTY)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Values used together");
    Print(FORMAT_STR_NL, CLI_ERR_VALUES_APPDIRECT_INDECES_USED_TOGETHER);
    goto Finish;
  }

  if (ContainsValue(pDisplayValues, APPDIRECT_1_SIZE_PROPERTY) &&
      ContainsValue(pDisplayValues, APPDIRECT_SIZE_PROPERTY)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Values used together");
    Print(FORMAT_STR_NL, CLI_ERR_VALUES_APPDIRECT_SIZE_USED_TOGETHER);
    goto Finish;
  }

  /** check that the display parameters are correct (if display option is set) **/
  if (DisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowGoalDisplayValues,
      ALLOWED_DISP_VALUES_COUNT(mppAllowedShowGoalDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  /** Fetch goal configs from driver **/
  ReturnCode = pNvmDimmConfigProtocol->GetGoalConfigs(
    pNvmDimmConfigProtocol,
    pDimmIds,
    DimmIdsCount,
    pSocketIds,
    SocketIdsCount,
    MAX_DIMMS,
    RegionConfigsInfo,
    &RegionConfigsCount,
    pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(L"Get region configuration goal", L" on", pCommandStatus);
    goto Finish;
  }

  if (RegionConfigsCount == 0) {
    Print(FORMAT_STR_NL, L"There are no goal configs defined in the system.");
    goto Finish;
  }

  if (!DisplayOptionSet && !AllOptionSet) {
    /** Default table view **/
#ifdef OS_BUILD
    if (ActionReqSet) {
      ReturnCode = ShowGoalPrintTableView(RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, pDimms, TRUE);
    } else {
      ReturnCode = ShowGoalPrintTableView(RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, NULL, TRUE);
    }
#else // OS_BUILD
    ReturnCode = ShowGoalPrintTableView(RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, TRUE);
#endif // OS_BUILD
  } else {
    /** Detailed view **/
    ReturnCode = ShowGoalPrintDetailedView(RegionConfigsInfo, RegionConfigsCount, AllOptionSet,
                     DisplayOptionSet, UnitsToDisplay, pDisplayValues);
  }

  if (!EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, L"A reboot is required to process new memory allocation goals.");
  }

Finish:
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pSettingsString);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Show Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowGoalCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowGoalCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

