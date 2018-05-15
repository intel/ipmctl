/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include "DumpDebugCommand.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "LoadCommand.h"
#include "Debug.h"
#include "Convert.h"

/**
  Get FW debug log syntax definition
**/
struct Command DumpDebugCommandSyntax =
{
  DUMP_VERB,                                                        //!< verb
  {                                                                 //!< options
    {L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired},
  },
  {
    {DEBUG_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_ID, TRUE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                          //!< properties
  L"Dump firmware debug log",                                       //!< help
  DumpDebugCommand                                                  //!< run function
};

/**
  Register syntax of dump -debug
**/
EFI_STATUS
RegisterDumpDebugCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DumpDebugCommandSyntax);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

 /**
  Dump debug log command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
DumpDebugCommand(
  IN    struct Command *pCmd
)
{
  EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 DimmCount = 0;
  UINT16 *pDimmIdsFilter = NULL;
  CHAR16 *pTargetValue = NULL;
  UINT32 DimmIdsFilterNum = 0;
  VOID *pDebugBuffer = NULL;
  UINT64 CurrentDebugBufferSize = 0;
  UINT64 BytesWritten = 0;
  CHAR16 *pDumpUserPath = NULL;
  DIMM_INFO *pDimms = NULL;
  BOOLEAN fExists = FALSE;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** Open Config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** get specific DIMM pid passed in, set it **/
  pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
  ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIdsFilter, &DimmIdsFilterNum);
  if (EFI_ERROR(ReturnCode) || (pDimmIdsFilter == NULL)) {
    NVDIMM_WARN("Target value is not a valid Dimm ID");
    goto Finish;
  }
  if (DimmIdsFilterNum > 1) {
    NVDIMM_WARN("Target value is not a valid Dimm ID");
    Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_DIMM);
    goto Finish;
  }

  if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIdsFilter, DimmIdsFilterNum)){
    Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Check -destination option
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory.");
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = FileExists(pDumpUserPath, &fExists);
  if (!EFI_ERROR(ReturnCode) && fExists){
     Print(L"Error: File (" FORMAT_STR L") already exists.\n", pDumpUserPath);
     ReturnCode = EFI_INVALID_PARAMETER;
     goto Finish;
   }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_DEVICE_ERROR;
    goto Finish;
  }

  BytesWritten = 0;

  ReturnCode = pNvmDimmConfigProtocol->DumpFwDebugLog(pNvmDimmConfigProtocol,
      pDimmIdsFilter[0], &pDebugBuffer, &BytesWritten, pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
      ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
      DisplayCommandStatus(CLI_INFO_DUMP_DEBUG_LOG, L"", pCommandStatus);
      goto Finish;
    }
  }

  /** Get Fw debug log  **/
  ReturnCode = DumpToFile(pDumpUserPath, BytesWritten, pDebugBuffer, FALSE);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_VOLUME_FULL) {
      Print(L"Not enough space to save file " FORMAT_STR L" with size %d\n", pDumpUserPath, CurrentDebugBufferSize);
    } else {
      Print(L"Failed to dump FW Debug logs to file (" FORMAT_STR L")\n", pDumpUserPath);
    }
  } else {
    Print(L"Successfully dumped FW Debug logs to file (" FORMAT_STR L"). (%d) MiB were written.\n",
        pDumpUserPath, BYTES_TO_MIB(BytesWritten));
  }

Finish:
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDumpUserPath);
  FREE_POOL_SAFE(pDimmIdsFilter);
  FREE_POOL_SAFE(pDebugBuffer);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
