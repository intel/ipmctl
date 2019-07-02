/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/ShellLib.h>
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include "Common.h"
#include "StopSessionCommand.h"
#include <PbrDcpmm.h>
#include <Convert.h>

#define NO_ACTIVE_SESSION_MSG     L"No session running.\n"
#define STOP_SESSION_MSG          L"Stopped PBR session.\n"
#define NORMAL_MODE               L"Normal"
#define BUFFERED_FREED_MSG        L"Stopping a session will free all recording content.\n"

/**
  Command syntax definition
**/
struct Command StopSessionCommand =
{
  STOP_VERB,                                                            //!< verb
  {                                                                      //!< options
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"",HELP_FORCE_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired },
#endif
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", L"", L"", L"",FALSE, ValueOptional}
  },
  {                                                                      //!< targets
    {SESSION_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                               //!< properties
  L"Stops the active playback or recording session.", //!< help
  StopSession,
  TRUE,
  TRUE
};

/**
  Main cmd handler
**/
EFI_STATUS
StopSession(
  IN     struct Command *pCmd
  )
{
  BOOLEAN Force = FALSE;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  UINT32 CurPbrMode = PBR_NORMAL_MODE;
  BOOLEAN Confirmation = FALSE;

  NVDIMM_ENTRY();

  pPrinterCtx = pCmd->pPrintCtx;

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmPbrProtocolGuid, (VOID **)&pNvmDimmPbrProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  ReturnCode = pNvmDimmPbrProtocol->PbrGetMode(&CurPbrMode);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_GET_PBR_MODE);
    goto Finish;
  }

  //if already normal, print a message and exit
  if (PBR_NORMAL_MODE == CurPbrMode) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, NO_ACTIVE_SESSION_MSG);
    goto Finish;
  }

  if (!Force) {
    PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, BUFFERED_FREED_MSG);
    ReturnCode = PromptYesNo(&Confirmation);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
      NVDIMM_DBG("Failed on PromptedInput");
    }

    if (!Confirmation) {
      goto Finish;
    }
  }

  ReturnCode = pNvmDimmPbrProtocol->PbrSetMode(PBR_NORMAL_MODE);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_SET_PBR_MODE, NORMAL_MODE);
    goto Finish;
  }
  //reset the tagid to 0 (first tag)
  PbrDcpmmSerializeTagId(0);

  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, STOP_SESSION_MSG);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Recover Format command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterStopSessionCommand()
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&StopSessionCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
