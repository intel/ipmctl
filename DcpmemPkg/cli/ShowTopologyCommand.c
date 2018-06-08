/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include "ShowTopologyCommand.h"
#include "ShowDimmsCommand.h"
#include "NvmDimmCli.h"
#include "Uefi.h"
#include "Common.h"
#include <Convert.h>
#include <NvmHealth.h>

/** Command syntax definition **/
struct Command ShowTopologyCommand =
{
  SHOW_VERB,                                                             //!< verb
  {                                                                      //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
  },
  {                                                                      //!< targets
    {TOPOLOGY_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                 //!< properties
  L"Show the topology of the DCPMEM DIMMs installed in the host server"  , //!< help
  ShowTopology
};

CHAR16 *mppAllowedShowTopologyDisplayValues[] = {
  MEMORY_TYPE_STR,
  CAPACITY_STR,
  PHYSICAL_ID_STR,
  DIMM_ID_STR,
  DEVICE_LOCATOR_STR,
  SOCKET_ID_STR,
  MEMORY_CONTROLLER_STR,
  CHANNEL_ID_STR,
  CHANNEL_POS_STR,
  NODE_CONTROLLER_ID_STR,
  BANK_LABEL_STR
};

/** Register the command **/
EFI_STATUS
RegisterShowTopologyCommand(
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVDIMM_ENTRY();
  Rc = RegisterCommand(&ShowTopologyCommand);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/** Execute the command **/
EFI_STATUS
ShowTopology(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS TempReturnCode = EFI_INVALID_PARAMETER;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pSockets = NULL;
  UINT32 SocketsNum = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  UINT32 DimmCount = 0;
  UINT16 Index = 0;
  UINT16 Index2 = 0;
  UINT16 TopologyDimmsNumber = 0;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  BOOLEAN Found = FALSE;
  BOOLEAN ShowAll = FALSE;
  CHAR16 *pDisplayValues = NULL;
  CHAR16 *pSocketsValue = NULL;
  CHAR16 *pDimmsValue = NULL;
  CHAR16 *pMemoryType = NULL;
  CHAR16 *pTempString = NULL;
  CHAR16 *pCapacityStr = NULL;
  DIMM_INFO *pDimms = NULL;
  TOPOLOGY_DIMM_INFO  *pTopologyDimms = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  DISPLAY_PREFERENCES DisplayPreferences;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSockets, &SocketsNum);
    if (EFI_ERROR(ReturnCode)) {
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
  if (pDisplayValues != NULL) {
    DisplayOptionSet = TRUE;
  } else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues != NULL) {
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

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode) || (pDimms == NULL)) {
    goto Finish;
  }

  /** check that the display parameters are correct (if display option is set) **/
  if (DisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowTopologyDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowTopologyDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  ReturnCode = pNvmDimmConfigProtocol->GetSystemTopology(pNvmDimmConfigProtocol, &pTopologyDimms, &TopologyDimmsNumber);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  /** Check if proper -dimm target is given **/
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pDimmsValue, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode) || pDimmIds == NULL) {
      goto Finish;
    }
  }

  /** Check if proper -socket target is given **/
  for (Index = 0; Index < SocketsNum; Index++) {
    Found = FALSE;
    for (Index2 = 0; Index2 < DimmCount; Index2++) {
      if (pSockets[Index] == pDimms[Index2].SocketId) {
        Found = TRUE;
        break;
      }
    }
    if (!Found) {
      Print(FORMAT_STR_NL, CLI_ERR_INVALID_SOCKET_ID);
      ReturnCode = EFI_NOT_FOUND;
      NVDIMM_WARN("Invalid Socket ID");
      goto Finish;
    }
  }

  /** display a summary table of all dimms **/
  if (!AllOptionSet && !DisplayOptionSet) {

    SetDisplayInfo(L"DimmTopology", TableTabView);

    Print(FORMAT_SHOW_TOPO_HEADER,
        DIMM_ID_STR,
        MEMORY_TYPE_STR,
        CAPACITY_STR,
        PHYSICAL_ID_STR,
        DEVICE_LOCATOR_STR);

    for (Index = 0; Index < DimmCount; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSockets, SocketsNum, pDimms[Index].SocketId)) {
        continue;
      }
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }
      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      pMemoryType = MemoryTypeToStr(pDimms[Index].MemoryType);
      ReturnCode = MakeCapacityString(pDimms[Index].CapacityFromSmbios, UnitsToDisplay, TRUE, &pCapacityStr);
      Print(FORMAT_SHOW_TOPO,
          DimmStr,
          pMemoryType,
          pCapacityStr,
          pDimms[Index].DimmID,
          pDimms[Index].DeviceLocator);
      FREE_POOL_SAFE(pMemoryType);
      FREE_POOL_SAFE(pCapacityStr);
    }
    //Print topology for DDR4 entries if no dimm target specified
    if (!ContainTarget(pCmd, DIMM_TARGET)) {
      for (Index = 0; Index < TopologyDimmsNumber; Index++){
        pMemoryType = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
        TempReturnCode = MakeCapacityString(pTopologyDimms[Index].VolatileCapacity,
            UnitsToDisplay, TRUE, &pCapacityStr);
        KEEP_ERROR(ReturnCode, TempReturnCode);
        Print(FORMAT_SHOW_TOPO,
            NOT_APPLICABLE_SHORT_STR,
            pMemoryType,
            pCapacityStr,
            pTopologyDimms[Index].DimmID,
            pTopologyDimms[Index].DeviceLocator);
        FREE_POOL_SAFE(pMemoryType);
        FREE_POOL_SAFE(pCapacityStr);
      }
    }
  }

  /** display detailed view **/
  else {

    SetDisplayInfo(L"DimmTopology", ListView);

    ShowAll = (!AllOptionSet && !DisplayOptionSet) || AllOptionSet;

    for (Index = 0; Index < DimmCount; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSockets, SocketsNum, pDimms[Index].SocketId)) {
        continue;
      }

      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }

      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      /** always print the DimmlID **/
      Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, DimmStr);

      /** MemoryType **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_TYPE_STR))) {
        pTempString = MemoryTypeToStr(pDimms[Index].MemoryType);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MEMORY_TYPE_STR, pTempString);
        FREE_POOL_SAFE(pTempString);
      }

      /** Capacity **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CAPACITY_STR))) {
        TempReturnCode = MakeCapacityString(pDimms[Index].CapacityFromSmbios, UnitsToDisplay, TRUE, &pCapacityStr);
        KEEP_ERROR(ReturnCode, TempReturnCode);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /** PhysicalID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PHYSICAL_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, PHYSICAL_ID_STR, pDimms[Index].DimmID);
      }

      /** DeviceLocator **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DEVICE_LOCATOR_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, DEVICE_LOCATOR_STR, pDimms[Index].DeviceLocator);
      }

      /** SocketID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, SOCKET_ID_STR, pDimms[Index].SocketId);
      }

      /** MemControllerID **/
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

      /** NodeControllerID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, NODE_CONTROLLER_ID_STR))) {
        Print(FORMAT_3SPACE_EQ_0X04HEX_NL, NODE_CONTROLLER_ID_STR, pDimms[Index].NodeControllerID);
      }

      /** BankLabel **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BANK_LABEL_STR))) {
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, BANK_LABEL_STR, pDimms[Index].BankLabel);
      }
    }
    //Print detailed topology for DDR4 entries if no dimm target specified
    if (!ContainTarget(pCmd, DIMM_TARGET)) {
      for (Index = 0; Index < TopologyDimmsNumber; Index++) {
        /** Always Print DimmIDs **/
        Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, NOT_APPLICABLE_SHORT_STR);

        /** MemoryType **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_TYPE_STR))) {
          pTempString = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MEMORY_TYPE_STR, pTempString);
          FREE_POOL_SAFE(pTempString);
        }

        /** Capacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CAPACITY_STR))) {
          //Convert Megabytes to Gigabytes and get digits after point from number
          TempReturnCode = MakeCapacityString(pTopologyDimms[Index].VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
          KEEP_ERROR(ReturnCode, TempReturnCode);
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** PhysicalID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PHYSICAL_ID_STR))) {
          Print(FORMAT_3SPACE_EQ_0X04HEX_NL, PHYSICAL_ID_STR, pTopologyDimms[Index].DimmID);
        }

        /** DeviceLocator **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DEVICE_LOCATOR_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, DEVICE_LOCATOR_STR, pTopologyDimms[Index].DeviceLocator);
        }

        /** SocketID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, SOCKET_ID_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** MemControllerID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_CONTROLLER_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MEMORY_CONTROLLER_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** ChannelID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_ID_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, CHANNEL_ID_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** ChannelPos **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_POS_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, CHANNEL_POS_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** NodeControllerID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, NODE_CONTROLLER_ID_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, NODE_CONTROLLER_ID_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** BankLabel **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BANK_LABEL_STR))) {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, BANK_LABEL_STR, pTopologyDimms[Index].BankLabel);
        }
      }
    }
  }

Finish:
  FREE_POOL_SAFE(pDisplayValues);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pMemoryType);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pSockets);
  FREE_POOL_SAFE(pTopologyDimms);
  FREE_POOL_SAFE(pCapacityStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
