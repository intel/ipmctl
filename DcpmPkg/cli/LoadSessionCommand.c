/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "LoadSessionCommand.h"
#include "Common.h"
#include "Convert.h"
#include "Utility.h"
#include <PbrDcpmm.h>

#define SUCCESSFULLY_LOADED_BUFFER_MSG    L"Successfully loaded %d bytes to session buffer."

/**
  Command syntax definition
**/
struct Command LoadSessionCommand =
{
  LOAD_VERB,                                                            //!< verb
  {                                                                     //!< options
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, L"Source of the Session to load", FALSE, ValueRequired},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {                                                                     //!< targets
    {SESSION_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                              //!< properties
  L"Load Recording into memory",                                         //!< help
  LoadSession,
  TRUE,
  TRUE
};


/**
  Execute the Load Session command

**/
EFI_STATUS
LoadSession(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  CHAR16 *pLoadFilePath = NULL;
  CHAR16 *pLoadUserPath = NULL;
  UINT64 FileBufferSize = 0;
  UINT8 *pFileBuffer = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  // Check -source option
  if (containsOption(pCmd, SOURCE_OPTION)) {
    pLoadUserPath = getOptionValue(pCmd, SOURCE_OPTION);
    if (pLoadUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -source value. Out of memory");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  pLoadFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pLoadFilePath));
  if (pLoadFilePath == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = GetDeviceAndFilePath(pLoadUserPath, pLoadFilePath, &pDevicePathProtocol);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_GET_FILE_PATH, ReturnCode);
    goto Finish;
  }

  ReturnCode = FileRead(pLoadFilePath, pDevicePathProtocol, 0, &FileBufferSize, (VOID **)&pFileBuffer);
  if (EFI_ERROR(ReturnCode) || pFileBuffer == NULL) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_READ_FILE);
    goto Finish;
  }

  //session module responsible for freeing buffer.
  ReturnCode = pNvmDimmConfigProtocol->PbrSetSession(pFileBuffer, (UINT32)FileBufferSize);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_SET_SESSION_BUFFER);
    goto Finish;
  }

  //reset the tagid to 0 (first tag)
  PbrDcpmmSerializeTagId(0);

  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, SUCCESSFULLY_LOADED_BUFFER_MSG, (UINT32)FileBufferSize);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pLoadFilePath);
  FREE_POOL_SAFE(pLoadUserPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Register the Load Session command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterLoadSessionCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&LoadSessionCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}