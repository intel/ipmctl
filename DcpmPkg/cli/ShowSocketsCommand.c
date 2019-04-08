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
#include "ShowSocketsCommand.h"
#include "Common.h"
#include "Convert.h"

#define DS_ROOT_PATH                        L"/SocketList"
#define DS_SOCKET_PATH                      L"/SocketList/Socket"
#define DS_SOCKET_INDEX_PATH                L"/SocketList/Socket[%d]"

 /*
  *  PRINT LIST ATTRIBUTES
  *  ---SocketId=0x0001---
  *     MappedMemoryLimit=X
  *     TotalMappedMemory=X
  *     ...
  */
PRINTER_LIST_ATTRIB ShowSocketListAttributes =
{
 {
    {
      SOCKET_NODE_STR,                                        //GROUP LEVEL TYPE
      L"---" SOCKET_ID_STR L"=$(" SOCKET_ID_STR L")---",      //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                             //NULL or KEY VAL FORMAT STR
      SOCKET_ID_STR                                           //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

 /*
 *  PRINTER TABLE ATTRIBUTES (3 columns)
 *   SocketID | MappedMemoryLimit | TotalMappedMemory
 *   ================================================
 *   0x0001   | X                 | X
 *   ...
 */
PRINTER_TABLE_ATTRIB ShowSocketTableAttributes =
{
  {
    {
      SOCKET_ID_STR,                                        //COLUMN HEADER
      SOCKET_MAX_STR_WIDTH,                                 //COLUMN MAX STR WIDTH
      DS_SOCKET_PATH PATH_KEY_DELIM SOCKET_ID_STR           //COLUMN DATA PATH
    },
    {
      MAPPED_MEMORY_LIMIT_STR,                               //COLUMN HEADER
      MAPPED_MEMORY_LIMIT_MAX_STR_WIDTH,                     //COLUMN MAX STR WIDTH
      DS_SOCKET_PATH PATH_KEY_DELIM MAPPED_MEMORY_LIMIT_STR  //COLUMN DATA PATH
    },
    {
      TOTAL_MAPPED_MEMORY_STR,                                //COLUMN HEADER
      TOTAL_MAPPED_MEMORY_MAX_STR_WIDTH,                      //COLUMN MAX STR WIDTH
      DS_SOCKET_PATH PATH_KEY_DELIM TOTAL_MAPPED_MEMORY_STR   //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowSocketDataSetAttribs =
{
  &ShowSocketListAttributes,
  &ShowSocketTableAttributes
};

/**
  Command syntax definition
**/
struct Command ShowSocketsCommand =
{
  SHOW_VERB,                                        //!< verb
  /**
   options
  **/
  {
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_DDRT, L"", L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_SMBUS, L"", L"", L"", FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  /**
   targets
  **/
  {
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},            //!< properties
  L"Show basic information about the physical \
    processors in the host server.",                  //!< help
  ShowSockets,                                        //!< run function
  TRUE,                                               //!< enable print control support
};

CHAR16 *mppAllowedShowSocketsDisplayValues[] =
{
  SOCKET_ID_STR,
  MAPPED_MEMORY_LIMIT_STR,
  TOTAL_MAPPED_MEMORY_STR
};

/**
  Register the show sockets command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowSocketsCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowSocketsCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the show sockets command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND failed to open Config protocol, or run-time preferences could
                        not be retrieved, or user-specified socket ID is invalid
  @retval Other errors returned by the driver
**/
EFI_STATUS
ShowSockets(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT32 SocketCount = 0;
  SOCKET_INFO *pSockets = NULL;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketIdsNum = 0;
  CHAR16 *pSocketsValue = NULL;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  DISPLAY_PREFERENCES DisplayPreferences;
  CHAR16 *pMappedMemLimitStr = NULL;
  CHAR16 *pTotalMappedMemStr = NULL;
  BOOLEAN SocketIdFound = FALSE;
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;
  BOOLEAN ShowAll = FALSE;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;

  NVDIMM_ENTRY();

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

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowSocketsDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowSocketsDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  ShowAll = (!pDispOptions->AllOptionSet && !pDispOptions->DisplayOptionSet) || pDispOptions->AllOptionSet;

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  /**
    if Socket IDs were passed in, read them
  **/
  if (pCmd->targets[0].pTargetValueStr && StrLen(pCmd->targets[0].pTargetValueStr) > 0) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSocketIds, &SocketIdsNum);

    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
  }

  /**
    Determine the units to display the sizes in
  **/
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
  /**
    Any valid units option will override the preferences
  **/
  if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
    UnitsToDisplay = UnitsOption;
  }

  /**
    Retrieve the list of sockets on the platform
  **/
  ReturnCode = pNvmDimmConfigProtocol->GetSockets(pNvmDimmConfigProtocol, &SocketCount, &pSockets);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  } else if (pSockets == NULL) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_SOCKET_SKU_SUPPORT);
    goto Finish;
  }

  /**
   Check if proper -socket target is given
  **/
  for (Index = 0; Index < SocketIdsNum; Index++) {
    SocketIdFound = FALSE;

    /**
      Checking if the specified socket exist
    **/
    for (Index2 = 0; Index2 < SocketCount; Index2++) {
      if (pSockets[Index2].SocketId == pSocketIds[Index]) {
        SocketIdFound = TRUE;
        break;
      }
    }
    if (!SocketIdFound) {
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_SOCKET_NOT_FOUND CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET, pSocketIds[Index]);
      goto Finish;
    }
  }

  for (Index = 0; Index < SocketCount; Index++) {
    if (SocketIdsNum > 0 && !ContainUint(pSocketIds, SocketIdsNum, pSockets[Index].SocketId)) {
      continue;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_SOCKET_INDEX_PATH, Index);

    /** SocketID **/
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pSockets[Index].SocketId);

    /** MappedMemoryLimit **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MAPPED_MEMORY_LIMIT_STR))) {
      ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pSockets[Index].MappedMemoryLimit, UnitsToDisplay, TRUE, &pMappedMemLimitStr);
        if (EFI_ERROR(ReturnCode)) {
            PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_CAPACITY_STRING);
            goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MAPPED_MEMORY_LIMIT_STR, pMappedMemLimitStr);
        FREE_POOL_SAFE(pMappedMemLimitStr);
    }

    /** TotalMappedMemory **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, TOTAL_MAPPED_MEMORY_STR))) {
      ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pSockets[Index].TotalMappedMemory, UnitsToDisplay, TRUE, &pTotalMappedMemStr);
        if (EFI_ERROR(ReturnCode)) {
            PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_CAPACITY_STRING);
            goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TOTAL_MAPPED_MEMORY_STR, pTotalMappedMemStr);
        FREE_POOL_SAFE(pTotalMappedMemStr);
    }
  }
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowSocketDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_CMD_DISPLAY_OPTIONS_SAFE(pDispOptions);
  FREE_POOL_SAFE(pSockets);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pMappedMemLimitStr);
  FREE_POOL_SAFE(pTotalMappedMemStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

