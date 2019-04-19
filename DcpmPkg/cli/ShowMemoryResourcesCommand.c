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
#include <DataSet.h>
#include <Printer.h>
#include "Common.h"
#include "NvmDimmCli.h"
#include <ReadRunTimePreferences.h>

#define DS_MEMORY_RESOURCES_PATH                    L"/MemoryResources"

/**
  Command syntax definition
**/
struct Command ShowMemoryResourcesCommand = {
  SHOW_VERB,                                                                          //!< verb
  {                                                                                   //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired}
#ifdef OS_BUILD
  ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_UNIT_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {{MEMORY_RESOURCES_TARGET, L"", L"", TRUE, ValueEmpty}},                            //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                                            //!< properties
  L"Show information about total DIMM resource allocation.",                          //!< help
  ShowMemoryResources,
  TRUE,                                                                               //!< enable print control support
};

PRINTER_DATA_SET_ATTRIBS ShowMemResourcesDataSetAttribs =
{
  NULL,
  NULL
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
  PRINT_CONTEXT *pPrinterCtx = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  SetMem(&MemoryResourcesInfo, sizeof(MemoryResourcesInfo), 0x0);

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

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

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetMemoryResourcesInfo(pNvmDimmConfigProtocol, &MemoryResourcesInfo);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: GetMemoryResourcesInfo Failed\n");
    goto Finish;
  }

  ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.RawCapacity, UnitsToDisplay, TRUE, &pCapacityStr);

  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, DISPLAYED_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, DISPLAYED_MEMORY_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.AppDirectCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, DISPLAYED_APPDIRECT_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.UnconfiguredCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, DISPLAYED_UNCONFIGURED_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.InaccessibleCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, DISPLAYED_INACCESSIBLE_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.ReservedCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, DISPLAYED_RESERVED_CAPACITY_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

Finish:
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, &ShowMemResourcesDataSetAttribs);
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
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
