/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/HiiLib.h>
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
#define DS_MEMORY_RESOURCES_DATA_PATH               L"/MemoryResources/data"
#define DS_MEMORY_RESOURCES_DATA_INDEX_PATH         L"/MemoryResources/data[%d]"

 /*
 *  PRINTER TABLE ATTRIBUTES (5 columns)
 *                |     DDR    |   DCPMM  |    Total   |
 *   ===================================================
 *   Volatile     | Volatile DDR Mem  | Volatile DCPMM Mem  | Volatile Mem     |
 *   AppDirect    | N/A               | AppDirect Mem       | AppDirect Mem    |
 *   Cache        | DDR Cache Mem     | N/A                 | Cache Mem        |
 *   Inaccessible | N/A               | Inaccessible Mem    | Inaccessible Mem |
 *   Physical     | Total DDR Mem     | Total DCPMM Mem     | Total Mem        |
 */
PRINTER_TABLE_ATTRIB ShowMemoryResourcesTableAttributes =
{
  {
    {
      MEMORY_TYPE_STR,                                                    //COLUMN HEADER
      MEMORY_TYPE_MAX_STR_WIDTH,                                          //COLUMN MAX STR WIDTH
      DS_MEMORY_RESOURCES_DATA_PATH PATH_KEY_DELIM MEMORY_TYPE_STR        //COLUMN DATA PATH
    },
    {
      DDR_STR,                                                            //COLUMN HEADER
      DDR_MAX_STR_WIDTH,                                                  //COLUMN MAX STR WIDTH
      DS_MEMORY_RESOURCES_DATA_PATH PATH_KEY_DELIM DDR_STR                //COLUMN DATA PATH
    },
    {
      DCPMM_STR,                                                          //COLUMN HEADER
      DCPMM_MAX_STR_WIDTH,                                                //COLUMN MAX STR WIDTH
      DS_MEMORY_RESOURCES_DATA_PATH PATH_KEY_DELIM DCPMM_STR              //COLUMN DATA PATH
    },
    {
      TOTAL_STR,                                                          //COLUMN HEADER
      TOTAL_STR_WIDTH,                                                    //COLUMN MAX STR WIDTH
      DS_MEMORY_RESOURCES_DATA_PATH PATH_KEY_DELIM TOTAL_STR              //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowMemoryResourcesDataSetAttribsPmtt3 =
{
  NULL,
  &ShowMemoryResourcesTableAttributes
};

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
  L"Show memory allocation information for this platform.",                           //!< help
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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  MEMORY_RESOURCES_INFO MemoryResourcesInfo;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  UINT64 DcpmmInaccessibleCapacity = 0;
  UINT64 TotalCapacity = 0;
  CHAR16 *pCapacityStr = NULL;
  CHAR16 *pPcdMissingStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  UINT32 Index = 0;

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
  if (EFI_LOAD_ERROR == ReturnCode) {
    pPcdMissingStr = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_CURR_CONF_MISSING), NULL);
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, pPcdMissingStr);
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: GetMemoryResourcesInfo Failed\n");
    goto Finish;
  }

  /* Print Volatile Capacities */
  Index = 0;
  PRINTER_BUILD_KEY_PATH(pPath, DS_MEMORY_RESOURCES_DATA_INDEX_PATH, Index);
  // Print Header
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, L"Volatile");
  // Print DDR Memory/Volatile Capacity
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.DDRVolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DDR_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);
  // Print DCPMM Volatile Capacity
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DCPMM_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);
  // Print Total Volatile Capacity
  TotalCapacity = MemoryResourcesInfo.DDRVolatileCapacity + MemoryResourcesInfo.VolatileCapacity;
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, TotalCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TOTAL_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  /* Print App Direct Capacities */
  Index = 1;
  PRINTER_BUILD_KEY_PATH(pPath, DS_MEMORY_RESOURCES_DATA_INDEX_PATH, Index);
  // Print Header
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, L"AppDirect");
  // Print DDR App Direct Capacities (as of now, this is N/A)
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DDR_STR, DASH_STR);
  // Print DCPMM DDR App Direct Capacities
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.AppDirectCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DCPMM_STR, pCapacityStr);
  // Print Total App Direct Capacities
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TOTAL_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  /* Print DDR Cache Capacities */
  Index = 2;
  PRINTER_BUILD_KEY_PATH(pPath, DS_MEMORY_RESOURCES_DATA_INDEX_PATH, Index);
  // Print header
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, L"Cache");
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.DDRCacheCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  // Print DDR Cache Capacity
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DDR_STR, pCapacityStr);
  // Print DCPMM Cache Capacity
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DCPMM_STR, DASH_STR);
  // Print Total Cache Capacity
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TOTAL_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  Index = 3;
  PRINTER_BUILD_KEY_PATH(pPath, DS_MEMORY_RESOURCES_DATA_INDEX_PATH, Index);
  // Print Header
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, L"Inaccessible");
  // Print DDR Inaccessible Capacity
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.DDRInaccessibleCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DDR_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);
  // Print DCPMM Inaccessible Capacity
  DcpmmInaccessibleCapacity = MemoryResourcesInfo.InaccessibleCapacity + MemoryResourcesInfo.ReservedCapacity + MemoryResourcesInfo.UnconfiguredCapacity;
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, DcpmmInaccessibleCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DCPMM_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);
  // Print Total Inaccessible Capacity
  TotalCapacity = MemoryResourcesInfo.DDRInaccessibleCapacity + DcpmmInaccessibleCapacity;
  TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, TotalCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TOTAL_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);

  /* Print Physical Capacities */
  Index = 4;
  PRINTER_BUILD_KEY_PATH(pPath, DS_MEMORY_RESOURCES_DATA_INDEX_PATH, Index);
  // Print header
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, L"Physical");
  // Print DDR Physical Capacity
  ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.DDRRawCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DDR_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);
  // Print DCPMM Physical Capacity
  ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, MemoryResourcesInfo.RawCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DCPMM_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);
  // Print Total Physical Capacity
  TotalCapacity = MemoryResourcesInfo.DDRRawCapacity + MemoryResourcesInfo.RawCapacity;
  ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, TotalCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
  KEEP_ERROR(ReturnCode, TempReturnCode);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TOTAL_STR, pCapacityStr);
  FREE_POOL_SAFE(pCapacityStr);


Finish:
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(pPrinterCtx);
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_MEMORY_RESOURCES_PATH, &ShowMemoryResourcesDataSetAttribsPmtt3);
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pPcdMissingStr);
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
