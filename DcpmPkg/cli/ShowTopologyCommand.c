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
#include <ReadRunTimePreferences.h>

#define DS_ROOT_PATH                        L"/DimmTopologyList"
#define DS_DIMM_TOPOLOGY_PATH               L"/DimmTopologyList/DimmTopology"
#define DS_DIMM_TOPOLOGY_INDEX_PATH         L"/DimmTopologyList/DimmTopology[%d]"
#define DS_DIMM_SOCKET_INDEX_PATH           L"/DimmTopologyList/Socket[%d]"
#define DS_DIMM_DIE_INDEX_PATH              L"/DimmTopologyList/Socket[%d]/Die[%d]"
#define DS_DIMM_IMC_INDEX_PATH              L"/DimmTopologyList/Socket[%d]/Die[%d]/Imc[%d]"
#define DS_DIMM_CHANNEL_INDEX_PATH          L"/DimmTopologyList/Socket[%d]/Die[%d]/Imc[%d]/Channel[%d]"
#define DS_DIMM_SLOT_INDEX_PATH             L"/DimmTopologyList/Socket[%d]/Die[%d]/Imc[%d]/Channel[%d]/Slot[%d]"
#define DS_DIMM_DIMM_INDEX_PATH             L"/DimmTopologyList/Socket[%d]/Die[%d]/Imc[%d]/Channel[%d]/Slot[%d]/Dimm[%d]"

 /*
   *  PRINT LIST ATTRIBUTES
   *  ---DimmId=0x0001---
   *     MemoryType=DDR4
   *     Capacity=16.0 GiB
   *     PhysicalID=0x0051
   *     ...
   */
PRINTER_LIST_ATTRIB ShowTopoListAttributesPmtt1 =
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

PRINTER_LIST_ATTRIB ShowTopoListAttributesPmtt2 =
{
 {
    {
      SOCKET_NODE_STR,                                                                                                               //GROUP LEVEL TYPE
      L"---" SOCKET_ID_STR L"=$(" SOCKET_ID_STR L")---",                                                                             //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                                                                                                    //NULL or KEY VAL FORMAT STR
      SOCKET_ID_STR                                                                                                                  //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      DIE_NODE_STR,                                                                                                                  //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" DIE_ID_STR L"=$(" DIE_ID_STR L")---",                                                                   //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                                                                                                    //NULL or KEY VAL FORMAT STR
      DIE_ID_STR                                                                                                                     //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      IMC_NODE_STR,                                                                                                                  //GROUP LEVEL TYPE
      SHOW_LIST_IDENT SHOW_LIST_IDENT L"---" MEMORY_CONTROLLER_STR L"=$(" MEMORY_CONTROLLER_STR L")---",                             //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                                                                                                    //NULL or KEY VAL FORMAT STR
      MEMORY_CONTROLLER_STR                                                                                                          //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      CHANNEL_NODE_STR,                                                                                                              //GROUP LEVEL TYPE
      SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT L"---" CHANNEL_ID_STR L"=$(" CHANNEL_ID_STR L")---",                           //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                                                                                                    //NULL or KEY VAL FORMAT STR
      CHANNEL_ID_STR                                                                                                                 //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      SLOT_NODE_STR,                                                                                                                 //GROUP LEVEL TYPE
      SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT L"---" CHANNEL_POS_STR L"=$(" CHANNEL_POS_STR L")---",         //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                                                                                                    //NULL or KEY VAL FORMAT STR
      CHANNEL_POS_STR                                                                                                                //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      DIMM_NODE_STR,                                                                                                                 //GROUP LEVEL TYPE
      SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---", //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,   //NULL or KEY VAL FORMAT STR,
      DIMM_ID_STR                                                                                                                    //NULL or IGNORE KEY LIST (K1;K2)
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


PRINTER_DATA_SET_ATTRIBS ShowTopoDataSetAttribsPmtt1 =
{
  &ShowTopoListAttributesPmtt1,
  &ShowTopoTableAttributes
};

PRINTER_DATA_SET_ATTRIBS ShowTopoDataSetAttribsPmtt2 =
{
  &ShowTopoListAttributesPmtt2,
  &ShowTopoTableAttributes
};

/** Command syntax definition **/
struct Command ShowTopologyCommand =
{
  SHOW_VERB,                                                             //!< verb
  {                                                                      //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT , FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", HELP_ALL_DETAILS_TEXT, FALSE, ValueEmpty},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, HELP_UNIT_DETAILS_TEXT, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
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

CHAR16 *mppAllowedShowTopologyDisplayValues[] = {0};

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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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
  BOOLEAN Found = FALSE;
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
  UINT16 SocketID = MAX_UINT16;
  UINT16 DieID = MAX_UINT16;
  UINT16 MemControllerID = MAX_UINT16;
  UINT16 ChannelID = MAX_UINT16;
  UINT16 SlotID = MAX_UINT16;
  ACPI_REVISION Revision;

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

  Revision.AsUint8 = pTopologyDimms[Index].PmttVersion;
  /** display a summary table of all dimms **/
  if (!AllOptionSet) {
    //Print topology for DDR4 entries if no dimm target specified
    for (Index = 0; Index < TopologyDimmsNumber; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSockets, SocketsNum, pTopologyDimms[Index].SocketID)) {
        continue;
      }
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }

      if (pTopologyDimms[Index].MemoryType != 2 && ContainTarget(pCmd, DIMM_TARGET)) {
        break;
      }
      pMemoryType = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
      TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pTopologyDimms[Index].VolatileCapacity,
        UnitsToDisplay, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_TOPOLOGY_INDEX_PATH, TopoCnt);
      if (pTopologyDimms[Index].MemoryType == MEMORYTYPE_DCPM) {
        ReturnCode = GetPreferredDimmIdAsString(pTopologyDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, NOT_APPLICABLE_SHORT_STR);
      }
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pMemoryType);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].DimmID);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pTopologyDimms[Index].DeviceLocator);
      ++TopoCnt;

      FREE_POOL_SAFE(pMemoryType);
      FREE_POOL_SAFE(pCapacityStr);
    }
  }
  /** display detailed view for PMTT 0.1 **/
  else if (IS_ACPI_REV_MAJ_0_MIN_1(Revision)) {
    SetDisplayInfo(L"DimmTopology", ListView, NULL);

    //Print detailed topology for DDR4 entries if no dimm target specified
    for (Index = 0; Index < TopologyDimmsNumber; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSockets, SocketsNum, pTopologyDimms[Index].SocketID)) {
        continue;
      }
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }

      if (pTopologyDimms[Index].MemoryType != 2 && ContainTarget(pCmd, DIMM_TARGET)) {
        break;
      }

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_TOPOLOGY_INDEX_PATH, TopoCnt);

      /** Always Print DimmIDs **/
      if (pTopologyDimms[Index].MemoryType == MEMORYTYPE_DCPM) {
        ReturnCode = GetPreferredDimmIdAsString(pTopologyDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, NOT_APPLICABLE_SHORT_STR);
      }

      /** MemoryType **/
      pTempString = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pTempString);
      FREE_POOL_SAFE(pTempString);

      /** Capacity **/
      //Convert Megabytes to Gigabytes and get digits after point from number
      TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle,
        pTopologyDimms[Index].VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);

      /** PhysicalID **/
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].DimmID);

      /** DeviceLocator **/
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pTopologyDimms[Index].DeviceLocator);

      /** SocketID, MemControllerID, ChannelID, ChannelPos, NodeControllerID **/
      if (pTopologyDimms[Index].MemoryType == MEMORYTYPE_DCPM) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pTopologyDimms[Index].SocketID);
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, FORMAT_HEX, pTopologyDimms[Index].MemControllerID);
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].ChannelID);
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_POS_STR, FORMAT_INT32, pTopologyDimms[Index].SlotID);
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, NODE_CONTROLLER_ID_STR, FORMAT_HEX, pTopologyDimms[Index].NodeControllerID);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SOCKET_ID_STR, NOT_APPLICABLE_SHORT_STR);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, NOT_APPLICABLE_SHORT_STR);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CHANNEL_ID_STR, NOT_APPLICABLE_SHORT_STR);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CHANNEL_POS_STR, NOT_APPLICABLE_SHORT_STR);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, NODE_CONTROLLER_ID_STR, NOT_APPLICABLE_SHORT_STR);
      }

      /** BankLabel **/
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BANK_LABEL_STR, pTopologyDimms[Index].BankLabel);

      ++TopoCnt;
    }
  }
  /** display detailed view for PMTT 0.2 **/
  else if (IS_ACPI_REV_MAJ_0_MIN_2(Revision)) {
    SetDisplayInfo(L"DimmTopology", ListView, NULL);

    //Print detailed topology for DDR4 entries if no dimm target specified
    for (Index = 0; Index < TopologyDimmsNumber; Index++) {
      if (SocketsNum > 0 && !ContainUint(pSockets, SocketsNum, pTopologyDimms[Index].SocketID)) {
        continue;
      }
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
        continue;
      }

      if (pTopologyDimms[Index].MemoryType != 2 && ContainTarget(pCmd, DIMM_TARGET)) {
        break;
      }

      /** SocketID **/
      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_SOCKET_INDEX_PATH, pTopologyDimms[Index].SocketID);
      SocketID = pTopologyDimms[Index].SocketID;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pTopologyDimms[Index].SocketID);

      /** DieID **/
      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_DIE_INDEX_PATH, SocketID, pTopologyDimms[Index].DieID);
      DieID = pTopologyDimms[Index].DieID;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DIE_ID_STR, FORMAT_HEX, pTopologyDimms[Index].DieID);

      /** MemControllerID **/
      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_IMC_INDEX_PATH, SocketID, DieID, pTopologyDimms[Index].MemControllerID);
      MemControllerID = pTopologyDimms[Index].MemControllerID;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, FORMAT_HEX, pTopologyDimms[Index].MemControllerID);

      /** ChannelID **/
      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_CHANNEL_INDEX_PATH, SocketID, DieID, MemControllerID, pTopologyDimms[Index].ChannelID);
      ChannelID = pTopologyDimms[Index].ChannelID;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].ChannelID);

      /** ChannelPos **/
      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_SLOT_INDEX_PATH, SocketID, DieID, MemControllerID, ChannelID, pTopologyDimms[Index].SlotID);
      SlotID = pTopologyDimms[Index].SlotID;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_POS_STR, FORMAT_HEX, pTopologyDimms[Index].SlotID);

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_DIMM_INDEX_PATH, SocketID, DieID, MemControllerID, ChannelID, SlotID, TopoCnt);
      /** Always Print DimmIDs **/
      if (pTopologyDimms[Index].MemoryType == MEMORYTYPE_DCPM) {
        ReturnCode = GetPreferredDimmIdAsString(pTopologyDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, NOT_APPLICABLE_SHORT_STR);
      }

      /** MemoryType **/
      pTempString = MemoryTypeToStr(pTopologyDimms[Index].MemoryType);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pTempString);
      FREE_POOL_SAFE(pTempString);

      /** Capacity **/
      //Convert Megabytes to Gigabytes and get digits after point from number
      TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle,
        pTopologyDimms[Index].VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
      FREE_POOL_SAFE(pCapacityStr);

      /** PhysicalID **/
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pTopologyDimms[Index].DimmID);

      /** DeviceLocator **/
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pTopologyDimms[Index].DeviceLocator);

      /** NodeControllerID **/
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, NODE_CONTROLLER_ID_STR, FORMAT_HEX, pTopologyDimms[Index].NodeControllerID);

      /** BankLabel **/
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BANK_LABEL_STR, pTopologyDimms[Index].BankLabel);

      ++TopoCnt;
    }
  }

  //Specify table attributes
  if (IS_ACPI_REV_MAJ_0_MIN_1(Revision)) {
    PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowTopoDataSetAttribsPmtt1);
  }
  else {
    PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowTopoDataSetAttribsPmtt2);
  }

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
