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
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_DDRT, L"", L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_SMBUS, L"", L"", L"", FALSE, ValueEmpty},
    {LARGE_PAYLOAD_OPTION, L"", L"", L"", FALSE, ValueEmpty},
    {SMALL_PAYLOAD_OPTION, L"", L"", L"", FALSE, ValueEmpty},
    { L"", DESTINATION_PREFIX_OPTION, L"", DESTINATION_PREFIX_OPTION_HELP, TRUE, ValueRequired },
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired },
#endif
    { L"", DICTIONARY_OPTION, L"", DICTIONARY_OPTION_HELP, FALSE, ValueOptional }
  },
  {
    {DEBUG_TARGET, L"", L"", TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}
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
  UINT32 InitializedDimmCount = 0;
  UINT32 UninitializedDimmCount = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pTargetValue = NULL;
  CHAR16 *pDumpUserPath = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 Index = 0;
  nlog_dict_entry * next;
  BOOLEAN dictExists = FALSE;
  CHAR16 *pDictUserPath = NULL;
  CHAR16 *raw_file_name = NULL;
  CHAR16 *decoded_file_name = NULL;
  nlog_dict_entry* dict_head = NULL;
  UINT32 dict_version;
  UINT64 dict_entries;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *SourceNames[NUM_FW_DEBUG_LOG_SOURCES] = {L"media", L"sram", L"spi"};
  UINT8 IndexSource = 0;
  VOID *RawLogBuffer = NULL;
  UINT64 RawLogBufferSizeBytes = 0;
  INT8 SuccessesPerDimm[MAX_DIMMS];
  UINT32 Reserved = 0;

  NVDIMM_ENTRY();

  // Make sure SuccessesPerDimm is fully initialized to -1's.
  // -1 indicates a dimm that ends up to not be specified
  // so we don't care about its number of successes
  SetMem(SuccessesPerDimm, MAX_DIMMS * sizeof(SuccessesPerDimm[0]), (UINT8)(-1));

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

  ReturnCode = GetAllDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms,
      &DimmCount, &InitializedDimmCount, &UninitializedDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  /** get specific DIMM pid passed in, set it **/
  pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
  ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Target value is not a valid Dimm ID");
    goto Finish;
  }

  // Check -destination option
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory");
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
      NVDIMM_ERR("Could not get -dict value. Out of memory");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    if (EFI_ERROR(FileExists(pDictUserPath, &dictExists)))
    {
      ReturnCode = EFI_END_OF_FILE;
      NVDIMM_ERR("Could not check for existence of the dictionary file");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (!dictExists)
    {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"The passed dictionary file doesn't exist\n");
    }
  }

  // Only load the dictionary once
  if (dictExists)
  {
    dict_head = load_nlog_dict(pCmd, pDictUserPath, &dict_version, &dict_entries);
    if (!dict_head)
    {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to load the dictionary file " FORMAT_STR L"\n", pDictUserPath);
      goto Finish;
    }

    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Loaded %d dictionary entries\n", dict_entries);
  }

  for (Index = 0; Index < DimmCount; Index++) {
    // Initialize to -1 so we can ignore dimms that aren't specified
    SuccessesPerDimm[Index] = -1;

    // If a dimm was not specified, filter it out here
    if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
      continue;
    }

    // Initialize the successes of the specified dimm to 0
    // Used to calculate if we return an error or not to CLI
    SuccessesPerDimm[Index] = 0;

    // For easier reading
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"\n");

    // Collect logs from every debug log source on each dimm
    for (IndexSource = 0; IndexSource < NUM_FW_DEBUG_LOG_SOURCES; IndexSource++) {

      // Re-initialize pCommandStatus messages for each dimm and debug log source
      ReturnCode = InitializeCommandStatus(&pCommandStatus);
      if (EFI_ERROR(ReturnCode)) {
        ReturnCode = EFI_DEVICE_ERROR;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
        goto FreeAndContinue;
      }

      // Append dimm info, source, and .txt
      // We want to re-use pDumpUserPath, so use CatSPrint instead of
      // CatSPrintClean
      raw_file_name = CatSPrint(pDumpUserPath, L"_" FORMAT_STR L"_0x%04x_" FORMAT_STR L".bin",
          pDimms[Index].DimmUid, pDimms[Index].DimmHandle, SourceNames[IndexSource]);
      decoded_file_name = CatSPrint(pDumpUserPath, L"_" FORMAT_STR L"_0x%04x_" FORMAT_STR L".txt",
          pDimms[Index].DimmUid, pDimms[Index].DimmHandle, SourceNames[IndexSource]);
      if (raw_file_name == NULL || decoded_file_name == NULL) {
        goto FreeAndContinue;
      }


      ReturnCode = pNvmDimmConfigProtocol->GetFwDebugLog(pNvmDimmConfigProtocol,
          pDimms[Index].DimmID, IndexSource, Reserved, &RawLogBuffer, &RawLogBufferSizeBytes, pCommandStatus);

      if (EFI_ERROR(ReturnCode)) {
        if (ReturnCode == EFI_NOT_STARTED) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode,
            L"No " FORMAT_STR L" FW debug logs found\n",
            SourceNames[IndexSource]);
          goto FreeAndContinue;
        }
        if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
          ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode,
            L"Unexpected error in retrieving " FORMAT_STR L" FW debug logs\n",
            SourceNames[IndexSource]);
          PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CLI_INFO_DUMP_DEBUG_LOG, L" ", pCommandStatus);
          goto FreeAndContinue;
        }
      }

      /** Get FW debug log  **/
      ReturnCode = DumpToFile(raw_file_name, RawLogBufferSizeBytes, RawLogBuffer, TRUE);
      if (EFI_ERROR(ReturnCode)) {
        if (ReturnCode == EFI_VOLUME_FULL) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode,
              L"Not enough space to save file " FORMAT_STR L" with size %lu MiB\n",
              raw_file_name, BYTES_TO_MIB(RawLogBufferSizeBytes));
        }
        else {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode,
              L"Failed to dump " FORMAT_STR L" FW debug logs to file " FORMAT_STR L"\n",
              SourceNames[IndexSource], raw_file_name);
        }
        goto FreeAndContinue;
      }

      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Dumped " FORMAT_STR L" FW debug logs to file " FORMAT_STR L"\n",
            SourceNames[IndexSource], raw_file_name);

      /** Decode FW debug log **/
      if (dictExists) {
        decode_nlog_binary(pCmd, decoded_file_name, RawLogBuffer, RawLogBufferSizeBytes,
            dict_version, dict_head);
      }

      SuccessesPerDimm[Index]++;

FreeAndContinue:

      FREE_POOL_SAFE(raw_file_name);
      FREE_POOL_SAFE(decoded_file_name);
      FREE_POOL_SAFE(RawLogBuffer);
      FreeCommandStatus(&pCommandStatus);
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

  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDictUserPath);
  FREE_POOL_SAFE(pDumpUserPath);

  // Return success if any of 3 logs were retrieved on every specified dimm
  ReturnCode = EFI_SUCCESS;
  for (Index = 0; Index < DimmCount; Index++)
  {
    if (SuccessesPerDimm[Index] == 0)
    {
      // If any specified dimm (initialized with 0) had no successes, then return error
      ReturnCode = EFI_DEVICE_ERROR;
    }
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
