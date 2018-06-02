/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowMemoryResourcesCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include "Common.h"

/**
  Command syntax definition
**/
struct Command ShowMemoryResourcesCommand = {
  SHOW_VERB,                                                                          //!< verb
  {{UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}}, //!< options
  {{MEMORY_RESOURCES_TARGET, L"", L"", TRUE, ValueEmpty}},                            //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                                            //!< properties
  L"Show information about total DIMM resource allocation.",                          //!< help
  ShowMemoryResources
};

/**
  Execute the show memory resources command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOGOL function failure
**/
EFI_STATUS
ShowMemoryResources(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  MEMORY_RESOURCES_INFO MemoryResourcesInfo;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  CHAR16 *pCapacityStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  SetMem(&MemoryResourcesInfo, sizeof(MemoryResourcesInfo), 0x0);

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

  ReturnCode = pNvmDimmConfigProtocol->GetMemoryResourcesInfo(pNvmDimmConfigProtocol, &MemoryResourcesInfo);
  if (EFI_ERROR(ReturnCode)) {
    Print(L"Error: GetMemoryResourcesInfo Failed\n");
    goto Finish;
  }

  SetDisplayInfo(L"MemoryResources", ListView);

  ReturnCode = MakeCapacityString(MemoryResourcesInfo.RawCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  Print(FORMAT_STR L"=" FORMAT_STR_NL,DISPLAYED_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(MemoryResourcesInfo.VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  Print(FORMAT_STR L"=" FORMAT_STR_NL, DISPLAYED_MEMORY_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(MemoryResourcesInfo.AppDirectCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);

  Print(FORMAT_STR L"=" FORMAT_STR_NL, DISPLAYED_APPDIRECT_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(MemoryResourcesInfo.UnconfiguredCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);

  Print(FORMAT_STR L"=" FORMAT_STR_NL, DISPLAYED_UNCONFIGURED_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(MemoryResourcesInfo.InaccessibleCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);

  Print(FORMAT_STR L"=" FORMAT_STR_NL, DISPLAYED_INACCESSIBLE_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(MemoryResourcesInfo.ReservedCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);

  Print(FORMAT_STR L"=" FORMAT_STR_NL, DISPLAYED_RESERVED_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);

  return  ReturnCode;
}

/**
  Register the show memory resources command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowMemoryResourcesCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowMemoryResourcesCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
