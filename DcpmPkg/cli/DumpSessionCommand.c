/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/PrintLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "DumpSessionCommand.h"
#include "Common.h"
#include <PbrDcpmm.h>
#ifdef OS_BUILD
#include <os.h>
#endif
#define SUCCESSFULLY_DUMPED_BUFFER_MSG    L"Successfully dumped %d bytes to file."

/**
  Command syntax definition
**/
struct Command DumpSessionCommand =
{
  DUMP_VERB,                                                                      //!< verb
  { 
    {L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, L"Destination to dump the recorded session", FALSE, ValueRequired},   //!< options
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty}
#ifdef OS_BUILD
  ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {                                                                               //!< targets
    {SESSION_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                        //!< properties
  L"Dump the PBR session buffer to a file",                                       //!< help
  DumpSession,
  TRUE
};

/**
  Execute the Dump Session command

**/
EFI_STATUS
DumpSession(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pDumpUserPath = NULL;
  CHAR16 *pDumpFilePath = NULL;
  UINT32 BufferSz = 0;
  VOID *pBuffer = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  PbrHeader *pHeader = NULL;

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

  pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
  if (pDumpUserPath == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    NVDIMM_ERR("Could not get -destination value. Out of memory");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  //get the contents of the pbr session buffer
  ReturnCode = pNvmDimmConfigProtocol->PbrGetSession(&pBuffer, &BufferSz);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_GET_SESSION_BUFFER);
    goto Finish;
  }

  //fill in run-time versioning info
  if (NULL != pBuffer) {
    pHeader = (PbrHeader*)pBuffer;
#ifdef OS_BUILD
    os_get_os_name(pHeader->OsName, PBR_OS_NAME_MAX);
    os_get_os_version(pHeader->OsVersion, PBR_OS_VERSION_MAX);
    AsciiSPrint(pHeader->SwVersion, PBR_SW_VERSION_MAX, NVMDIMM_VERSION_STRING_A);
#else
    AsciiSPrint(pHeader->OsName, PBR_OS_NAME_MAX, "UEFI");
    UnicodeStrToAsciiStrS(NVMDIMM_VERSION_STRING, pHeader->SwVersion, PBR_SW_VERSION_MAX);
#endif
  }

  //dump the buffer to a file
  ReturnCode = DumpToFile(pDumpUserPath, BufferSz, pBuffer, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_DUMP_SESSION_TO_FILE);
    goto Finish;
  }

  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, SUCCESSFULLY_DUMPED_BUFFER_MSG, BufferSz);

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pBuffer);
  FREE_POOL_SAFE(pDumpFilePath);
  FREE_POOL_SAFE(pDumpUserPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
 }

/**
  Register the Dump PBR command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDumpSessionCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DumpSessionCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}