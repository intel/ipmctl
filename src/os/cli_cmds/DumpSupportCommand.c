/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Protocol/EfiShellParameters.h>
#include <Library/BaseMemoryLib.h>
#include "DumpSupportCommand.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "LoadCommand.h"
#include "Debug.h"
#include "Convert.h"
#include <stdio.h>

extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;

/**
  Get FW debug log syntax definition
**/
struct Command DumpSupportCommandSyntax = {
  DUMP_VERB,                                                      //!< verb
  {                                                                 //!< options
    {L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired},
  },
  {                                                                 //!< targets
    {SUPPORT_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {                                                                 //!< properties
    { L"", L"", L"", FALSE, ValueOptional }
  },
  L"Capture a snapshot of the system staet for support purposes",   //!< help
  DumpSupportCommand                                                //!< run function
};

typedef struct _DUMP_SUPPORT_CMD
{
  CHAR16 cmd[100];
} DUMP_SUPPORT_CMD;

#define MAX_CMDS 8

DUMP_SUPPORT_CMD DumpCmds[MAX_CMDS] = {
{L"version" },
{L"show -memoryresources"},
{L"show -a -dimm"},
{L"show -a -system -capabilities"},
{L"show -a -topology" },
{L"show -a -sensor"},
{L"start -diagnostic"},
{L"show -event"} };

#define NEW_DUMP_ENTRY_HEADER L"/*\n* %ls\n*/\n"

/**
  Register syntax of create -support
**/
EFI_STATUS
RegisterDumpSupportCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DumpSupportCommandSyntax);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Dump support command

  @param[in] pCmd Command from CLI

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pCmd NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL Function failure
**/
EFI_STATUS
DumpSupportCommand(
  IN    struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pDumpUserPath = NULL;
  UINT32 Index = 0;
  struct CommandInput Input;
  struct Command Command;
  CHAR8 *pDumpUserPathAscii = NULL;
  FILE *hFile = NULL;

  NVDIMM_ENTRY();
  SetDisplayInfo(L"DumpSupport", ResultsView);

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /* Check -destination option */
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory.");
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }
  else {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if(NULL == (pDumpUserPathAscii = AllocatePool((StrLen(pDumpUserPath) + 1) * sizeof(CHAR8))))
  {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  UnicodeStrToAsciiStr(pDumpUserPath, pDumpUserPathAscii);
  if(NULL == (hFile = fopen(pDumpUserPathAscii, "w+")))
  {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }
  gOsShellParametersProtocol.StdOut = (SHELL_FILE_HANDLE) hFile;

  for(Index = 0; Index < MAX_CMDS; ++Index)
  {
    Print(NEW_DUMP_ENTRY_HEADER, DumpCmds[Index].cmd);
    FillCommandInput(DumpCmds[Index].cmd, &Input);
    ReturnCode = Parse(&Input, &Command);

    if (!EFI_ERROR(ReturnCode)) {
      /* parse success, now run the command */
      ReturnCode = Command.run(&Command);
    }

    FreeCommandInput(&Input);
  }

  fclose(gOsShellParametersProtocol.StdOut);
  gOsShellParametersProtocol.StdOut = stdout;

  Print(CLI_INFO_DUMP_SUPPORT_SUCCESS, pDumpUserPath);

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDumpUserPath);
  FREE_POOL_SAFE(pDumpUserPathAscii);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
