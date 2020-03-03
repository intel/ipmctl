/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include <ReadRunTimePreferences.h>
#include "Utility.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "ShowGoalCommand.h"
#include "Common.h"
#include "Convert.h"

#define DS_ROOT_PATH                        L"/ConfigGoalList"
#define DS_CONFIG_GOAL_PATH                 L"/ConfigGoalList/ConfigGoal"
#define DS_CONFIG_GOAL_INDEX_PATH           L"/ConfigGoalList/ConfigGoal[%d]"

 /*
   *  PRINT LIST ATTRIBUTES
   *  ---DimmID=0x0001---
   *     SocketID=X
   *     MemorySize=X
   *     ...
   */
PRINTER_LIST_ATTRIB ShowGoalListAttributes =
{
 {
    {
      CONFIG_GOAL_NODE_STR,                                   //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",          //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                             //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                             //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

/*
*  PRINTER TABLE ATTRIBUTES ( columns)
*   SocketID | DimmID | MemorySize | AppDirect1Size | AppDirect2Size
*   ================================================================
*   0x0001   | X      |X           | X              | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowGoalTableAttributes =
{
  {
    {
      SOCKET_ID_STR,                                                //COLUMN HEADER
      SOCKET_MAX_STR_WIDTH,                                         //COLUMN MAX STR WIDTH
      DS_CONFIG_GOAL_PATH PATH_KEY_DELIM SOCKET_ID_STR              //COLUMN DATA PATH
    },
    {
      DIMM_ID_STR,                                                  //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                           //COLUMN MAX STR WIDTH
      DS_CONFIG_GOAL_PATH PATH_KEY_DELIM DIMM_ID_STR                //COLUMN DATA PATH
    },
    {
      MEMORY_SIZE_PROPERTY,                                         //COLUMN HEADER
      MEMORY_SIZE_MAX_STR_WIDTH,                                    //COLUMN MAX STR WIDTH
      DS_CONFIG_GOAL_PATH PATH_KEY_DELIM MEMORY_SIZE_PROPERTY       //COLUMN DATA PATH
    },
    {
      APPDIRECT_1_SIZE_PROPERTY,                                    //COLUMN HEADER
      MEMORY_SIZE_MAX_STR_WIDTH,                                    //COLUMN MAX STR WIDTH
      DS_CONFIG_GOAL_PATH PATH_KEY_DELIM APPDIRECT_1_SIZE_PROPERTY  //COLUMN DATA PATH
    },
    {
      APPDIRECT_2_SIZE_PROPERTY,                                    //COLUMN HEADER
      MEMORY_SIZE_MAX_STR_WIDTH,                                    //COLUMN MAX STR WIDTH
      DS_CONFIG_GOAL_PATH PATH_KEY_DELIM APPDIRECT_2_SIZE_PROPERTY  //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowGoalDataSetAttribs =
{
  &ShowGoalListAttributes,
  &ShowGoalTableAttributes
};

/**
  Command syntax definition
**/
struct Command ShowGoalCommand =
{
  SHOW_VERB,                                                          //!< verb
  {                                                                   //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"",HELP_ALL_DETAILS_TEXT, FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES,HELP_DISPLAY_DETAILS_TEXT, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP,HELP_UNIT_DETAILS_TEXT, FALSE, ValueRequired}
#ifdef OS_BUILD
    , {OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired}
#endif // OS_BUILD
  },
  {                                                                     //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                              //!< properties
  L"Show a pending memory allocation goal on one or more " PMEM_MODULES_STR L" to be applied on reboot.",            //!< help
  ShowGoal,
  TRUE,                                                                 //!< enable print control support
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
};


/**
  Print results of show goal according to table view

  @param[in] pCmd command from CLI
  @param[in] pRegionConfigsInfo - Region Config table to be printed
  @param[in] CurrentUnits The requested type of units to convert the capacity into
  @param[in] RegionConfigsCount - Number of elements in array

  @retval EFI_SUCCESS if printing is successful
  @retval EFI_INVALID_PARAMETER if input parameter is incorrect
**/

EFI_STATUS
ShowGoalPrintTableView(
    IN     struct Command *pCmd,
    IN    REGION_GOAL_PER_DIMM_INFO *pRegionConfigsInfo,
    IN    UINT16 CurrentUnits,
    IN    UINT32 RegionConfigsCount,
    IN    BOOLEAN Buffered
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *pVolatileCapacityStr = NULL;
  CHAR16 *pAppDirect1CapacityStr = NULL;
  CHAR16 *pAppDirect2CapacityStr = NULL;
  REGION_GOAL_PER_DIMM_INFO *pCurrentGoal = NULL;
  CHAR16 *pPath = NULL;

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pRegionConfigsInfo == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < RegionConfigsCount; ++Index) {
    pCurrentGoal = &pRegionConfigsInfo[Index];
    PRINTER_BUILD_KEY_PATH(pPath, DS_CONFIG_GOAL_INDEX_PATH, Index);

    ReturnCode = GetPreferredDimmIdAsString(pCurrentGoal->DimmID, pCurrentGoal->DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, ROUNDDOWN(pCurrentGoal->VolatileSize, SIZE_1GB), CurrentUnits,
        TRUE, &pVolatileCapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);

    TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pCurrentGoal->AppDirectSize[0],
        CurrentUnits, TRUE, &pAppDirect1CapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);

    TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pCurrentGoal->AppDirectSize[1],
        CurrentUnits, TRUE, &pAppDirect2CapacityStr);
    KEEP_ERROR(ReturnCode, TempReturnCode);

    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pCmd->pPrintCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pCurrentGoal->SocketId);
    PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, DIMM_ID_STR, DimmStr);
    PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, MEMORY_SIZE_PROPERTY, pVolatileCapacityStr);
    PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, APPDIRECT_1_SIZE_PROPERTY, pAppDirect1CapacityStr);
    PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, APPDIRECT_2_SIZE_PROPERTY, pAppDirect2CapacityStr);

    FREE_POOL_SAFE(pVolatileCapacityStr);
    FREE_POOL_SAFE(pAppDirect1CapacityStr);
    FREE_POOL_SAFE(pAppDirect2CapacityStr);
  }

Finish:
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pCmd->pPrintCtx, DS_ROOT_PATH, &ShowGoalDataSetAttribs);
  FREE_POOL_SAFE(pPath);
  return ReturnCode;
}

/**
  Print results of show goal according to detailed view

  @param[in] pCmd command from CLI
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
  IN    struct Command *pCmd,
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
  CHAR16 *pPath = NULL;

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pRegionConfigsInfo == NULL || (DisplayOptionSet && pDisplayValues == NULL)) {
    goto Finish;
  }

  for (Index = 0; Index < RegionConfigsCount; ++Index) {
    pCurrentGoal = &pRegionConfigsInfo[Index];

    PRINTER_BUILD_KEY_PATH(pPath, DS_CONFIG_GOAL_INDEX_PATH, Index);

    /* always print dimmID */
    /** Dimm ID **/
    ReturnCode = GetPreferredDimmIdAsString(pCurrentGoal->DimmID, pCurrentGoal->DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, DIMM_ID_STR, DimmStr);

    /** Socket Id **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pCmd->pPrintCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pCurrentGoal->SocketId);
    }
    /** Volatile Size **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_SIZE_PROPERTY))) {
      ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, ROUNDDOWN(pCurrentGoal->VolatileSize, SIZE_1GB), CurrentUnits,
          TRUE, &pCapacityStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, MEMORY_SIZE_PROPERTY, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }
    /** AppDirect1Size **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_1_SIZE_PROPERTY))) {
      TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pCurrentGoal->AppDirectSize[0], CurrentUnits, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pCmd->pPrintCtx, pPath, APPDIRECT_1_SIZE_PROPERTY,
                                         pCurrentGoal->InterleaveSetType[0] == MIRRORED ? FORMAT_STR MIRRORED_STR : FORMAT_STR, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }
    /** AppDirect1Index **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_1_INDEX_PROPERTY))) {
      if (pCurrentGoal->AppDirectSize[0] == 0) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, APPDIRECT_1_INDEX_PROPERTY, NA_STR);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pCmd->pPrintCtx, pPath, APPDIRECT_1_INDEX_PROPERTY, FORMAT_INT32, pCurrentGoal->AppDirectIndex[0]);
      }
    }
    /** AppDirect1Settings **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_1_SETTINGS_PROPERTY))) {
      InterleaveSettingsToString(pCurrentGoal->AppDirectSize[0], pCurrentGoal->NumberOfInterleavedDimms[0],
          pCurrentGoal->ImcInterleaving[0], pCurrentGoal->ChannelInterleaving[0], &pSettingsString);
      PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, APPDIRECT_1_SETTINGS_PROPERTY, pSettingsString);
      FREE_POOL_SAFE(pSettingsString);
    }
    /** AppDirect2Size **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_2_SIZE_PROPERTY))) {
      TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pCurrentGoal->AppDirectSize[1], CurrentUnits, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pCmd->pPrintCtx, pPath, APPDIRECT_2_SIZE_PROPERTY,
        pCurrentGoal->InterleaveSetType[0] == MIRRORED ? FORMAT_STR MIRRORED_STR : FORMAT_STR, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }
    /** AppDirect2Index **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_2_INDEX_PROPERTY))) {
      if (pCurrentGoal->AppDirectSize[1] == 0) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, APPDIRECT_2_INDEX_PROPERTY, NA_STR);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pCmd->pPrintCtx, pPath, APPDIRECT_2_INDEX_PROPERTY, FORMAT_INT32, pCurrentGoal->AppDirectIndex[1]);
      }
    }
    /** AppDirect2Settings **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, APPDIRECT_2_SETTINGS_PROPERTY))) {
      InterleaveSettingsToString(pCurrentGoal->AppDirectSize[1], pCurrentGoal->NumberOfInterleavedDimms[1],
          pCurrentGoal->ImcInterleaving[1], pCurrentGoal->ChannelInterleaving[1], &pSettingsString);
      PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, APPDIRECT_2_SETTINGS_PROPERTY, pSettingsString);
      FREE_POOL_SAFE(pSettingsString);
    }
    /** Status **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayValues, STATUS_STR))) {
      pStatusString = GoalStatusToString(gNvmDimmCliHiiHandle, pCurrentGoal->Status);
      PRINTER_SET_KEY_VAL_WIDE_STR(pCmd->pPrintCtx, pPath, STATUS_STR, pStatusString);
      FREE_POOL_SAFE(pStatusString);
    }
  }
Finish:
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pCmd->pPrintCtx, DS_ROOT_PATH, &ShowGoalDataSetAttribs);
  FREE_POOL_SAFE(pPath);
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
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  UINT32 DimmIdsCount = 0;
  UINT32 SocketIdsCount = 0;
  UINT32 RegionConfigsCount = 0;
  CHAR16 *pSettingsString = NULL;
  CHAR16 *pDisplayValues = NULL;
  CHAR16 *pTargetValue = NULL;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  REGION_GOAL_PER_DIMM_INFO RegionConfigsInfo[MAX_DIMMS];
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  DISPLAY_PREFERENCES DisplayPreferences;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;

  NVDIMM_ENTRY();

  SetMem(RegionConfigsInfo, sizeof(RegionConfigsInfo), 0x0);
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  pDispOptions = AllocateZeroPool(sizeof(CMD_DISPLAY_OPTIONS));
  if (NULL == pDispOptions) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowGoalDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowGoalDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  pDisplayValues = pDispOptions->pDisplayValues;
  AllOptionSet = pDispOptions->AllOptionSet;
  DisplayOptionSet = pDispOptions->DisplayOptionSet;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  /** Need NvmDimmConfigProtocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  // check sockets
  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pTargetValue, &pSocketIds, &SocketIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      NVDIMM_DBG("Failed on GetUintsFromString");
      goto Finish;
    }
  }

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


  if (ContainsValue(pDisplayValues, APPDIRECT_1_INDEX_PROPERTY) &&
      ContainsValue(pDisplayValues, APPDIRECT_INDEX_PROPERTY)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Values used together");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_VALUES_APPDIRECT_INDECES_USED_TOGETHER);
    goto Finish;
  }

  if (ContainsValue(pDisplayValues, APPDIRECT_1_SIZE_PROPERTY) &&
      ContainsValue(pDisplayValues, APPDIRECT_SIZE_PROPERTY)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Values used together");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_VALUES_APPDIRECT_SIZE_USED_TOGETHER);
    goto Finish;
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
    if (EFI_VOLUME_CORRUPTED == ReturnCode) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_PCD_CORRUPTED);
    }
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    PrinterSetCommandStatus(pPrinterCtx, ReturnCode, CLI_GET_REGION_MSG, CLI_GET_REGION_ON_MSG, pCommandStatus);
    goto Finish;
  }

  if (RegionConfigsCount == 0) {
    //WA, to ensure ESX prints a message when no entries are found.
    if (PRINTER_ESX_FORMAT_ENABLED(pPrinterCtx)) {
      PRINTER_SET_MSG(pPrinterCtx, EFI_NOT_FOUND, CLI_NO_GOALS_MSG);
    }
    else {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_NO_GOALS_MSG);
    }
    goto Finish;
  }

  if (!DisplayOptionSet && !AllOptionSet) {
    /** Default table view **/
    ReturnCode = ShowGoalPrintTableView(pCmd, RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, TRUE);
  } else {
    /** Detailed view **/
    ReturnCode = ShowGoalPrintDetailedView(pCmd, RegionConfigsInfo, RegionConfigsCount, AllOptionSet,
                     DisplayOptionSet, UnitsToDisplay, pDisplayValues);
  }

  if (!EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_REBOOT_REQUIRED_MSG);
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pSettingsString);
  FREE_CMD_DISPLAY_OPTIONS_SAFE(pDispOptions);
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

