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
  {{L"", L"", L"", FALSE, ValueOptional}},           //!< properties
  L"Show basic information about the physical \
    processors in the host server.",                 //!< help
  ShowSockets                                        //!< run function
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
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
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
  BOOLEAN DisplayOptionSet = FALSE;
  CHAR16 *pDisplayValues = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  CHAR16 *pMappedMemLimitStr = NULL;
  CHAR16 *pTotalMappedMemStr = NULL;
  BOOLEAN SocketIdFound = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  /**
    if Socket IDs were passed in, read them
  **/
  if (pCmd->targets[0].pTargetValueStr && StrLen(pCmd->targets[0].pTargetValueStr) > 0) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSocketIds, &SocketIdsNum);

    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
  }

  /**
    if the display option was specified
  **/
  pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION);
  if (pDisplayValues) {
    DisplayOptionSet = TRUE;
  }
  else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      DisplayOptionSet = TRUE;
    }
  }

  /**
    Check that the display parameters are correct (if display option is set)
  **/
  if (DisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowSocketsDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowSocketsDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  /**
    Determine the units to display the sizes in
  **/
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
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  } else if (pSockets == NULL) {
    Print(L"Platform does not support socket SKU limits.\n");
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
      Print(L"\nSocket not found. Invalid SocketID: %d\n", pSocketIds[Index]);
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
  }

  /**
    Display a summary table of all sockets
  **/
  if (!DisplayOptionSet) {

    SetDisplayInfo(L"Socket", TableView, NULL);

    Print(FORMAT_SHOW_SOCKET_HEADER,
	  SOCKET_ID_STR,
	  MAPPED_MEMORY_LIMIT_STR,
	  TOTAL_MAPPED_MEMORY_STR);

    for (Index = 0; Index < SocketCount; Index++) {
      if (SocketIdsNum > 0 && !ContainUint(pSocketIds, SocketIdsNum, pSockets[Index].SocketId)) {
        continue;
      }

      TempReturnCode = MakeCapacityString(pSockets[Index].MappedMemoryLimit, UnitsToDisplay, TRUE, &pMappedMemLimitStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);

      TempReturnCode = MakeCapacityString(pSockets[Index].TotalMappedMemory, UnitsToDisplay, TRUE, &pTotalMappedMemStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);

      Print(FORMAT_SHOW_SOCKET,
        pSockets[Index].SocketId,
        pMappedMemLimitStr,
        pTotalMappedMemStr);

      FREE_POOL_SAFE(pMappedMemLimitStr);
      FREE_POOL_SAFE(pTotalMappedMemStr);
    }
  } else {  /** display detailed view **/

    for (Index = 0; Index < SocketCount; Index++) {
      if (SocketIdsNum > 0 && !ContainUint(pSocketIds, SocketIdsNum, pSockets[Index].SocketId)) {
        continue;
      }

      /** always print the socket ID **/
      Print(L"---" FORMAT_STR L"=0x%04x---\n", SOCKET_ID_STR, pSockets[Index].SocketId);

      /** MappedMemoryLimit **/
      if (DisplayOptionSet && ContainsValue(pDisplayValues, MAPPED_MEMORY_LIMIT_STR)) {
        TempReturnCode = MakeCapacityString(pSockets[Index].MappedMemoryLimit, UnitsToDisplay, TRUE, &pMappedMemLimitStr);
        KEEP_ERROR(ReturnCode, TempReturnCode);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, MAPPED_MEMORY_LIMIT_STR, pMappedMemLimitStr);
        FREE_POOL_SAFE(pMappedMemLimitStr);
      }

      /** TotalMappedMemory **/
      if (DisplayOptionSet && ContainsValue(pDisplayValues, TOTAL_MAPPED_MEMORY_STR)) {
        ReturnCode = MakeCapacityString(pSockets[Index].TotalMappedMemory, UnitsToDisplay, TRUE, &pTotalMappedMemStr);
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, TOTAL_MAPPED_MEMORY_STR, pTotalMappedMemStr);
        FREE_POOL_SAFE(pTotalMappedMemStr);
      }
    }
  }

Finish:
  FREE_POOL_SAFE(pSockets);
  FREE_POOL_SAFE(pSocketIds);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

