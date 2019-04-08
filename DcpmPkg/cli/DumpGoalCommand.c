/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "DumpGoalCommand.h"
#include "Common.h"

/**
  Command syntax definition
**/
struct Command DumpGoalCommand =
{
  DUMP_VERB,                                                                      //!< verb
  {                                                                               //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_DDRT, L"", L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_SMBUS, L"", L"", L"", FALSE, ValueEmpty},
    {LARGE_PAYLOAD_OPTION, L"", L"", L"", FALSE, ValueEmpty},
    {SMALL_PAYLOAD_OPTION, L"", L"", L"", FALSE, ValueEmpty},
    {L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired}
#ifdef OS_BUILD
  ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {                                                                               //!< targets
    {SYSTEM_TARGET, L"", L"", TRUE, ValueEmpty},
    {CONFIG_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                        //!< properties
  L"Store the region configuration goal from one or more DIMMs to a file",        //!< help
  DumpGoal,
  TRUE
};


/**
  Execute the Dump Goal command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
EFI_STATUS
DumpGoal(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pDumpUserPath = NULL;
  CHAR16 *pDumpFilePath = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePath = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  pDumpFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDumpFilePath));
  if (pDumpFilePath == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  // Check -destination option
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }

  ReturnCode = GetDeviceAndFilePath(pDumpUserPath, pDumpFilePath, &pDevicePath);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_WRONG_FILE_PATH);
    goto Finish;
  }

  // Initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->DumpGoalConfig(pNvmDimmConfigProtocol, pDumpFilePath, pDevicePath,
      pCommandStatus);

  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CLI_DUMP_GOAL_MSG, CLI_DUMP_GOAL_ON_MSG, pCommandStatus);
  } else {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_DUMP_CONFIG_SUCCESS, pDumpUserPath);
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDumpFilePath);
  FREE_POOL_SAFE(pDumpUserPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
 }

/**
  Register the Dump Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDumpGoalCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DumpGoalCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

