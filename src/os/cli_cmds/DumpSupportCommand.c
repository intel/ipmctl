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
    { L"", DESTINATION_PREFIX_OPTION, L"", L"",L"Destination to Dump the Support", FALSE, ValueRequired },
    { VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty },
    { L"",PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    { L"",PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},

#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired },
#endif
    { L"", DICTIONARY_OPTION, L"", L"", L"Dictionary File", FALSE, ValueOptional }
  },
  {                                                                 //!< targets
    {SUPPORT_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {                                                                 //!< properties
    { L"", L"", L"", FALSE, ValueOptional }
  },
  L"Capture a snapshot of the system state for support purposes",   //!< help
  DumpSupportCommand,                                                //!< run function
  TRUE
};

typedef struct _DUMP_SUPPORT_CMD
{
  CHAR16 cmd[100];
} DUMP_SUPPORT_CMD;

#define MAX_PLAFORM_SUPPORT_CMDS 7
#define MAX_DIMM_SPECIFIC_CMDS 5
#define APPEND_TO_FILE_NAME L"platform_support_info"
#define PLATFORM_INFO_STR L"Platform information"
#define DIMM_SPECIFIC_INFO L"Dimm Specific information - UUID: "
#define WITH_DIC_OPTION  DICTIONARY_OPTION SPACE_FORMAT_STR_SPACE DEBUG_TARGET L" " DIMM_TARGET SPACE_FORMAT_HEX
#define WITHOUT_DICT_OPTION DEBUG_TARGET L" " DIMM_TARGET SPACE_FORMAT_HEX
#define STR_DUMP_DEST L"dump -destination %ls "

DUMP_SUPPORT_CMD DumpPlatformLevelCmds[MAX_PLAFORM_SUPPORT_CMDS] = {
{L"version" },
{L"show -memoryresources"},
{L"show -a -system -capabilities"},
{L"show -a -topology" },
{L"start -diagnostic"},
{L"show -system"},
};

DUMP_SUPPORT_CMD DumpCmdsPerDimm[MAX_DIMM_SPECIFIC_CMDS] = {
  {L"show -a -dimm 0x%04x"},
  {L"show -a -sensor -dimm 0x%04x"},
  {L"show -pcd -dimm 0x%04x"},
  {L"show -error Media -dimm 0x%04x"},
  {L"show -error Thermal -dimm 0x%04x"},
};

#define NEW_DUMP_ENTRY_HEADER L"/*\n* %ls\n*/\n"
#define PER_DIMM_HEADER L"/*\n* %ls 0x%04x\n*/\n"
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

STATIC VOID PrintHeaderInfo(CHAR16 *printLine) {
  Print(L"\n/*************************************************************************************************/\n");
  Print(L"/******************************** %ls ********************************/", printLine);
  Print(L"\n/*************************************************************************************************/\n");
}

STATIC VOID PrintAndExecuteCommand(CHAR16 *pCmdInputWithDimmId) {
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  struct CommandInput Input;
  struct Command Command;
  if (NULL == pCmdInputWithDimmId) {
    NVDIMM_DBG("pCmdInputWithDimmId value is NULL");
    return;
  }
  Print(NEW_DUMP_ENTRY_HEADER, pCmdInputWithDimmId);
  FillCommandInput(pCmdInputWithDimmId, &Input);
  ReturnCode = Parse(&Input, &Command);

  if (!EFI_ERROR(ReturnCode)) {
    /* parse success, now run the command */
    ReturnCode = ExecuteCmd(&Command);
  }
  FreeCommandInput(&Input);
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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pDumpUserPath = NULL;
  CHAR16 *pPlatformSupportFileName = NULL;
  CHAR16 *pPrintDIMMHeaderInfo = NULL;
  UINT32 Index = 0;
  UINT32 DimmIndex = 0;

  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR8 *pPlatformSupportFilenameAscii = NULL;
  FILE *hFile = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pDictUserPath = NULL;
  CHAR16 *pCmdInputWithDimmId = NULL;
  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;
  /** Open Config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  if (containsOption(pCmd, DICTIONARY_OPTION)) {
    pDictUserPath = getOptionValue(pCmd, DICTIONARY_OPTION);
    if (pDictUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -dict value. Out of memory");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }

  /* Check -destination  prefix option */
  if (containsOption(pCmd, DESTINATION_PREFIX_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_PREFIX_OPTION);

    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory.");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_PARSER_ERR_INVALID_OPTION_VALUES);
    goto Finish;
  }
  pPlatformSupportFileName = CatSPrint(pDumpUserPath, L"_" FORMAT_STR L".txt",
    APPEND_TO_FILE_NAME);
  if(NULL == pPlatformSupportFileName || NULL == (pPlatformSupportFilenameAscii = AllocatePool((StrLen(pPlatformSupportFileName) + 1) * sizeof(CHAR8))))
  {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }
  UnicodeStrToAsciiStr(pPlatformSupportFileName, pPlatformSupportFilenameAscii);
  if(NULL == (hFile = fopen(pPlatformSupportFilenameAscii, "w+")))
  {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }
  gOsShellParametersProtocol.StdOut = (SHELL_FILE_HANDLE) hFile;
  PrintHeaderInfo(PLATFORM_INFO_STR);
  for(Index = 0; Index < MAX_PLAFORM_SUPPORT_CMDS; ++Index) {
	PrintAndExecuteCommand(DumpPlatformLevelCmds[Index].cmd);
  }

  for (DimmIndex = 0; DimmIndex < DimmCount; ++DimmIndex) {
    pPrintDIMMHeaderInfo = CatSPrint(NULL, DIMM_SPECIFIC_INFO FORMAT_STR, pDimms[DimmIndex].DimmUid);
    PrintHeaderInfo(pPrintDIMMHeaderInfo);
    FREE_POOL_SAFE(pPrintDIMMHeaderInfo);
    for (Index = 0; Index < MAX_DIMM_SPECIFIC_CMDS; ++Index) {
      pCmdInputWithDimmId = CatSPrint(NULL, DumpCmdsPerDimm[Index].cmd, pDimms[DimmIndex].DimmHandle);
      PrintAndExecuteCommand(pCmdInputWithDimmId);
    }
    if (pDictUserPath != NULL) {
      pCmdInputWithDimmId = CatSPrintClean(NULL, STR_DUMP_DEST, pDumpUserPath);
      pCmdInputWithDimmId = CatSPrintClean(pCmdInputWithDimmId, WITH_DIC_OPTION, pDictUserPath, pDimms[DimmIndex].DimmHandle);
    } else {
      pCmdInputWithDimmId = CatSPrintClean(NULL, STR_DUMP_DEST, pDumpUserPath);
      pCmdInputWithDimmId = CatSPrintClean(pCmdInputWithDimmId, WITHOUT_DICT_OPTION, pDimms[DimmIndex].DimmHandle);
    }
    PrintAndExecuteCommand(pCmdInputWithDimmId);
    FREE_POOL_SAFE(pCmdInputWithDimmId);
  } //end for dimmIndex

  fclose(gOsShellParametersProtocol.StdOut);
  gOsShellParametersProtocol.StdOut = stdout;

  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_DUMP_SUPPORT_SUCCESS L"\n", pPlatformSupportFileName);

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pPlatformSupportFileName);
  FREE_POOL_SAFE(pPlatformSupportFilenameAscii);
  FREE_POOL_SAFE(pDumpUserPath);
  FREE_POOL_SAFE(pDictUserPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
