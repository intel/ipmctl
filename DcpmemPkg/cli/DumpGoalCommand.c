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
  {{L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired}}, //!< options
  {                                                                               //!< targets
    {SYSTEM_TARGET, L"", L"", TRUE, ValueEmpty},
    {CONFIG_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                        //!< properties
  L"Store the region configuration goal from one or more DIMMs to a file",        //!< help
  DumpGoal
};


/**
  Execute the Dump Goal command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
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

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  pDumpFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDumpFilePath));
  if (pDumpFilePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Check -destination option
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory");
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }

  ReturnCode = GetDeviceAndFilePath(pDumpUserPath, pDumpFilePath, &pDevicePath);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get file path (%r)", ReturnCode);
    goto Finish;
  }

  // Initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->DumpGoalConfig(pNvmDimmConfigProtocol, pDumpFilePath, pDevicePath,
      pCommandStatus);

  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  if (EFI_ERROR(ReturnCode)) {
    DisplayCommandStatus(L"Dump pool configuration goal", L" on", pCommandStatus);
  } else {
     SetDisplayInfo(L"DumpGoal", ResultsView);
    Print(L"Successfully dumped system configuration to file: " FORMAT_STR_NL, pDumpUserPath);
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDumpFilePath);
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

