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

#define DS_ROOT_PATH                        L"/DimmTopologyList"
#define DS_DIMM_TOPOLOGY_PATH               L"/DimmTopologyList/DimmTopology"
#define DS_DIMM_TOPOLOGY_INDEX_PATH         L"/DimmTopologyList/DimmTopology[%d]"

 /*
   *  PRINT LIST ATTRIBUTES
   *  ---DimmId=0x0001---
   *     MemoryType=DDR4
   *     Capacity=16.0 GiB
   *     PhysicalID=0x0051
   *     ...
   */
PRINTER_LIST_ATTRIB ShowTopoListAttributes =
{
 {
    {
      TOPOLOGY_NODE_STR,                                  //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",      //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                         //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                         //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

/*
*  PRINTER TABLE ATTRIBUTES (5 columns)
*   DimmID | MemoryType | Capacity | PhysicalID | DeviceLocator
*   ==========================================================
*   0x0001 | X          | X        | X          | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowTopoTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                            //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                     //COLUMN MAX STR WIDTH
      DS_DIMM_TOPOLOGY_PATH PATH_KEY_DELIM DIMM_ID_STR        //COLUMN DATA PATH
    },
    {
      MEMORY_TYPE_STR,                                        //COLUMN HEADER
      MEMORY_TYPE_MAX_STR_WIDTH,                              //COLUMN MAX STR WIDTH
      DS_DIMM_TOPOLOGY_PATH PATH_KEY_DELIM MEMORY_TYPE_STR    //COLUMN DATA PATH
    },
    {
      CAPACITY_STR,                                           //COLUMN HEADER
      CAPACITY_MAX_STR_WIDTH,                                 //COLUMN MAX STR WIDTH
      DS_DIMM_TOPOLOGY_PATH PATH_KEY_DELIM CAPACITY_STR       //COLUMN DATA PATH
    },
    {
      PHYSICAL_ID_STR,                                        //COLUMN HEADER
      ID_MAX_STR_WIDTH,                                       //COLUMN MAX STR WIDTH
      DS_DIMM_TOPOLOGY_PATH PATH_KEY_DELIM PHYSICAL_ID_STR    //COLUMN DATA PATH
    },
    {
      DEVICE_LOCATOR_STR,                                      //COLUMN HEADER
      DEVICE_LOCATOR_MAX_STR_WIDTH,                            //COLUMN MAX STR WIDTH
      DS_DIMM_TOPOLOGY_PATH PATH_KEY_DELIM DEVICE_LOCATOR_STR  //COLUMN DATA PATH
    }
  }
};


PRINTER_DATA_SET_ATTRIBS ShowTopoDataSetAttribs =
{
  &ShowTopoListAttributes,
  &ShowTopoTableAttributes
};

/** Command syntax definition **/
struct Command ShowTopologyCommand =
{
  SHOW_VERB,                                                             //!< verb
  {                                                                      //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {                                                                      //!< targets
    {TOPOLOGY_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                   //!< properties
  L"Show the topology of the DCPMMs installed in the host server"  , //!< help
  ShowTopology,
  TRUE
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
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;
  UINT32 TopoCnt = 0;
  BOOLEAN volatile DimmIsOkToDisplay[MAX_DIMMS];

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;
  for (Index = 0; Index < MAX_DIMMS; Index++) {
    DimmIsOkToDisplay[Index] = FALSE;
  }

  pDispOptions = AllocateZeroPool(sizeof(CMD_DISPLAY_OPTIONS));
  if (NULL == pDispOptions) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowTopologyDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowTopologyDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSockets, &SocketsNum);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
  }

  AllOptionSet = pDispOptions->AllOptionSet;
  DisplayOptionSet = pDispOptions->DisplayOptionSet;
  pDisplayValues = pDispOptions->pDisplayValues;

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
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

  /** make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetAllDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode) || (pDimms == NULL)) {
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetSystemTopology(pNvmDimmConfigProtocol, &pTopologyDimms, &TopologyDimmsNumber);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  /** Check if proper -dimm target is given **/
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pDimmsValue, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    /*Mark each dimm as ok to display based on the dimms passed by the user*/
    for (Index = 0; Index < DimmCount; Index++) {
      for (Index2 = 0; Index2 < DimmIdsNum; Index2++) {
        if (pDimms[Index].DimmID == pDimmIds[Index2]) {
          DimmIsOkToDisplay[Index] = TRUE;
        }
      }
    }
  } else {
    /*Since no dimms were specified, mark them all as ok to display*/
    for (Index = 0; Index < MAX_DIMMS; Index++) {
      DimmIsOkToDisplay[Index] = TRUE;
    }
  }

  /** Check if proper -socket target is given **/
  for (Index = 0; Index < SocketsNum; Index++) {
    Found = FALSE;
    /*Only display dimms which match the socket(s) specified *and* that the user has indicated*/
    for (Index2 = 0; Index2 < DimmCount; Index2++) {
      if (DimmIsOkToDisplay[Index2] == TRUE &&
          pSockets[Index] == pDimms[Index2].SocketId) {
        Found = TRUE;
        break;
      }
    }
    if (!Found) {
      ReturnCode = EFI_NOT_FOUND;
      if (DimmIdsNum > 0)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_SPECIFIED_DIMMS_ON_SPECIFIED_SOCKET);
      } else {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INVALID_SOCKET_ID);
      }
      NVDIMM_WARN("Invalid Socket ID");
      goto Finish;
    }
  }

  /** display a summary table of all dimms **/
  if (!AllOptionSet && !DisplayOptionSet) {

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
      ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[Index].CapacityFromSmbios, UnitsToDisplay, TRUE, &pCapacityStr);

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_TOPOLOGY_INDEX_PATH, TopoCnt);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pMemoryType);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pDimms[Index].DimmID);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pDimms[Index].DeviceLocator);
      ++TopoCnt;

      FREE_POOL_SAFE(pMemoryType);
      FREE_POOL_SAFE(pCapacityStr);
    }

    //Print topology for DDR4 entries if no dimm target specified
    if (!ContainTarget(pCmd, DIMM_TARGET)) {
      for (Index = 0; Index < TopologyDimmsNumber; Index++){
        if (SocketsNum > 0 && !ContainUint(pSockets, SocketsNum, pTopologyDimms[Index].SocketID)) {
          continue;
        }
        pMemoryType = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
        TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pTopologyDimms[Index].VolatileCapacity,
            UnitsToDisplay, TRUE, &pCapacityStr);
        KEEP_ERROR(ReturnCode, TempReturnCode);

        PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_TOPOLOGY_INDEX_PATH, TopoCnt);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, NOT_APPLICABLE_SHORT_STR);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pMemoryType);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].DimmID);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pTopologyDimms[Index].DeviceLocator);
        ++TopoCnt;

        FREE_POOL_SAFE(pMemoryType);
        FREE_POOL_SAFE(pCapacityStr);
      }
    }
  }

  /** display detailed view **/
  else {

    SetDisplayInfo(L"DimmTopology", ListView, NULL);

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

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_TOPOLOGY_INDEX_PATH, TopoCnt);

      /** always print the DimmlID **/
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

      /** MemoryType **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_TYPE_STR))) {
        pTempString = MemoryTypeToStr(pDimms[Index].MemoryType);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pTempString);
        FREE_POOL_SAFE(pTempString);
      }

      /** Capacity **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CAPACITY_STR))) {
        TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[Index].CapacityFromSmbios, UnitsToDisplay, TRUE, &pCapacityStr);
        KEEP_ERROR(ReturnCode, TempReturnCode);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /** PhysicalID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PHYSICAL_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pDimms[Index].DimmID);
      }

      /** DeviceLocator **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DEVICE_LOCATOR_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pDimms[Index].DeviceLocator);
      }

      /** SocketID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pDimms[Index].SocketId);
      }

      /** MemControllerID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_CONTROLLER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, FORMAT_HEX, pDimms[Index].ImcId);
      }

      /** ChannelID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_ID_STR, FORMAT_HEX, pDimms[Index].ChannelId);
      }

      /** ChannelPos **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_POS_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_POS_STR, FORMAT_INT32, pDimms[Index].ChannelPos);
      }

      /** NodeControllerID **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, NODE_CONTROLLER_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, NODE_CONTROLLER_ID_STR, FORMAT_HEX, pDimms[Index].NodeControllerID);
      }

      /** BankLabel **/
      if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BANK_LABEL_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BANK_LABEL_STR, pDimms[Index].BankLabel);
      }

      ++TopoCnt;
    }
    //Print detailed topology for DDR4 entries if no dimm target specified
    if (!ContainTarget(pCmd, DIMM_TARGET)) {
      for (Index = 0; Index < TopologyDimmsNumber; Index++) {

        PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_TOPOLOGY_INDEX_PATH, TopoCnt);

        /** Always Print DimmIDs **/
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, NOT_APPLICABLE_SHORT_STR);

        /** MemoryType **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_TYPE_STR))) {
          pTempString = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pTempString);
          FREE_POOL_SAFE(pTempString);
        }

        /** Capacity **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CAPACITY_STR))) {
          //Convert Megabytes to Gigabytes and get digits after point from number
          TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle,
              pTopologyDimms[Index].VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
          KEEP_ERROR(ReturnCode, TempReturnCode);
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** PhysicalID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, PHYSICAL_ID_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].DimmID);
        }

        /** DeviceLocator **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DEVICE_LOCATOR_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pTopologyDimms[Index].DeviceLocator);
        }

        /** SocketID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, SOCKET_ID_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SOCKET_ID_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** MemControllerID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, MEMORY_CONTROLLER_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** ChannelID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_ID_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CHANNEL_ID_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** ChannelPos **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, CHANNEL_POS_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CHANNEL_POS_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** NodeControllerID **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, NODE_CONTROLLER_ID_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, NODE_CONTROLLER_ID_STR, NOT_APPLICABLE_SHORT_STR);
        }

        /** BankLabel **/
        if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, BANK_LABEL_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BANK_LABEL_STR, pTopologyDimms[Index].BankLabel);
        }

        ++TopoCnt;
      }
    }
  }

  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowTopoDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_CMD_DISPLAY_OPTIONS_SAFE(pDispOptions);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pMemoryType);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pSockets);
  FREE_POOL_SAFE(pTopologyDimms);
  FREE_POOL_SAFE(pCapacityStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
