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
#include "Nlog.h"

 /**
   Get FW debug log syntax definition
 **/
struct Command DumpDebugCommandSyntax =
{
  DUMP_VERB,                                                        //!< verb
  {                                                                 //!< options
    { L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired },
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired },
#endif
    { L"", DICTIONARY_OPTION, L"", DICTIONARY_OPTION_HELP, FALSE, ValueOptional }
  },
  {
    {DEBUG_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_ID, TRUE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                          //!< properties
  L"Dump firmware debug log",                                       //!< help
  DumpDebugCommand, TRUE                                                  //!< run function
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 DimmCount = 0;
  UINT16 *pDimmIdsFilter = NULL;
  CHAR16 *pTargetValue = NULL;
  UINT32 DimmIdsFilterNum = 0;
  VOID *pDebugBuffer = NULL;
  UINT64 CurrentDebugBufferSize = 0;
  UINT64 BytesWritten = 0;
  UINT64 Length = 0;
  UINT64 Index = 0;
  BOOLEAN Found = FALSE;
  CHAR16 *pDumpUserPath = NULL;
  DIMM_INFO *pDimms = NULL;
  nlog_dict_entry * next;
  BOOLEAN dictExists = FALSE;
  CHAR16 *pDictUserPath = NULL;
  CHAR16 *decoded_file_name = NULL;
  nlog_dict_entry* dict_head = NULL;
  UINT32 dict_version;
  UINT64 dict_entries;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
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
    goto Finish;
  }

  /** get specific DIMM pid passed in, set it **/
  pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
  ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIdsFilter, &DimmIdsFilterNum);
  if (EFI_ERROR(ReturnCode) || (pDimmIdsFilter == NULL)) {
    NVDIMM_WARN("Target value is not a valid Dimm ID");
    goto Finish;
  }
  if (DimmIdsFilterNum > 1) {
    NVDIMM_WARN("Target value is not a valid Dimm ID");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_DIMM);
    goto Finish;
  }

  if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIdsFilter, DimmIdsFilterNum)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
    goto Finish;
  }

  // Check -destination option
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory.");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }
  else {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_PARSER_DETAILED_ERR_OPTION_REQUIRED, DESTINATION_OPTION);
    goto Finish;
  }

  if (containsOption(pCmd, DICTIONARY_OPTION)) {
    pDictUserPath = getOptionValue(pCmd, DICTIONARY_OPTION);
    if (pDictUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -dict value. Out of memory.");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    if (EFI_ERROR(FileExists(pDictUserPath, &dictExists)))
    {
      ReturnCode = EFI_END_OF_FILE;
      NVDIMM_ERR("Could not check for existence of the dictionary file.");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (!dictExists)
    {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"The passed dictionary file doesn't exist.\n");
    }
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_DEVICE_ERROR;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  BytesWritten = 0;

  ReturnCode = pNvmDimmConfigProtocol->DumpFwDebugLog(pNvmDimmConfigProtocol,
    pDimmIdsFilter[0], &pDebugBuffer, &BytesWritten, pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
      ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
      PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CLI_INFO_DUMP_DEBUG_LOG, L"", pCommandStatus);
      goto Finish;
    }
  }

  /** Get Fw debug log  **/
  ReturnCode = DumpToFile(pDumpUserPath, BytesWritten, pDebugBuffer, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_VOLUME_FULL) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Not enough space to save file " FORMAT_STR L" with size %lu\n", pDumpUserPath, CurrentDebugBufferSize);
    }
    else {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to dump FW Debug logs to file (" FORMAT_STR L")\n", pDumpUserPath);
    }
  }
  else {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Successfully dumped FW Debug logs to file (" FORMAT_STR L"). (%lu) MiB were written.\n",
      pDumpUserPath, BYTES_TO_MIB(BytesWritten));

    if (dictExists)
    {
      dict_head = load_nlog_dict(pCmd, pDictUserPath, &dict_version, &dict_entries);
      if (!dict_head)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to load the dictionary file " FORMAT_STR L"\n", pDictUserPath);
        goto Finish;
      }

      Length = StrLen(pDumpUserPath);
      //find the extention value
      for (Index = Length-1; Index >= 0; Index--)
      {
        if (pDumpUserPath[Index] == L'.')
        {
          Found = TRUE;
          break;
        }
      }

      if (FALSE == Found)
      {
        //no extention was found, just append
        decoded_file_name = CatSPrint(pDumpUserPath, L".txt");
      }
      else
      {
        //append .txt to the extentionless string
        Length = Index;
        decoded_file_name = AllocateZeroPool((sizeof(CHAR16) * Length) + sizeof(CHAR16));
        if (decoded_file_name) {
          for (Index = 0; Index < Length; Index++)
          {
            decoded_file_name[Index] = pDumpUserPath[Index];
          }
          decoded_file_name = CatSPrintClean(decoded_file_name, L".txt");
        }
      }

      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Loaded %d dictionary entries.\n", dict_entries);
      if (decoded_file_name) {
        decode_nlog_binary(pCmd, decoded_file_name, pDebugBuffer, BytesWritten, dict_version, dict_head);
      }
    }
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  while (dict_head)
  {
    next = dict_head->next;
    FREE_POOL_SAFE(dict_head->LogLevel);
    FREE_POOL_SAFE(dict_head->LogString);
    FREE_POOL_SAFE(dict_head->FileName);
    FREE_POOL_SAFE(dict_head);
    dict_head = next;
  }

  FREE_POOL_SAFE(decoded_file_name);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDictUserPath);
  FREE_POOL_SAFE(pDimmIdsFilter);
  FREE_POOL_SAFE(pDebugBuffer);
  FREE_POOL_SAFE(pDumpUserPath);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
