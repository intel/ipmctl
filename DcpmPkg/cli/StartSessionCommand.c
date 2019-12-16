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
#include "StartSessionCommand.h"
#include <PbrDcpmm.h>
#include <Convert.h>
#ifdef OS_BUILD
#include <Protocol/Driver/DriverBinding.h>
#else
#include <Protocol/DriverBinding.h>
#endif
#define ACTION_SETTING_MODE           L"Setting to " FORMAT_STR L" mode.\n"
#define MANUAL_ACTION_REQUIRED        L"Please manually unload and load the driver\n"
#define END_OF_PLAYBACK_BUFFER        L"Session tag id not set or at the end of the playback buffer.  Setting playback pointer to the first tag.\n\n"
#define BUFFERED_FREED_MSG            L"Starting a new session will free previously recorded content.\n"

 /**
   Helper that creates a new pbr recording buffer

   @retval EFI_SUCCESS on success
 **/
STATIC EFI_STATUS CreatePbrBuffer(EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol);
STATIC EFI_STATUS SetPbrMode(EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol, UINT32 Mode);
STATIC EFI_STATUS ExecuteCommand(CHAR16 *pCmdInput);
STATIC EFI_STATUS ExecuteCommands(PRINT_CONTEXT *pPrinterCtx, EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol, UINT32 TagId);
STATIC EFI_STATUS ResetPbrSession(EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol, UINT32 Id);

extern EFI_DRIVER_BINDING_PROTOCOL gNvmDimmDriverDriverBinding;
/**
  Command syntax definition
**/
struct Command StartSessionCommand =
{
  START_VERB,                                                            //!< verb
  {                                                                      //!< options
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", HELP_FORCE_DETAILS_TEXT,FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired },
#endif
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", L"", L"",L"", FALSE, ValueOptional}
  },
  {                                                                      //!< targets
    {SESSION_TARGET, L"", L"", TRUE, ValueEmpty},
    {PBR_MODE_TARGET, L"", L"", TRUE, ValueRequired},
    {PBR_MODE_TAG, L"", L"", FALSE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                               //!< properties
  L"Starts a playback or record session",                                //!< help
  StartSession,
  TRUE,
  TRUE //exclude from PBR
};

/**
  Main cmd handler
**/
EFI_STATUS
StartSession(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol = NULL;
  CHAR16 *pModeValue = NULL;
  CHAR16 *pTagValue = NULL;
  UINT64 TagId64 = 0;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  BOOLEAN Force = FALSE;
  BOOLEAN Confirmation = FALSE;
  UINT32 PbrMode;

  pPrinterCtx = pCmd->pPrintCtx;

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmPbrProtocolGuid, (VOID **)&pNvmDimmPbrProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  pModeValue = GetTargetValue(pCmd, PBR_MODE_TARGET);
  if (NULL == pModeValue) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  pTagValue = GetTargetValue(pCmd, PBR_MODE_TAG);

  //start recording session
  if (0 == StrICmp(pModeValue, PBR_RECORD_MODE_VAL)) {

    ReturnCode = pNvmDimmPbrProtocol->PbrGetMode(&PbrMode);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_FAILED_TO_GET_PBR_MODE);
      goto Finish;
    }

    //if currently in a session, warn user that starting a new recording session
    //will clear previous recording data
    if (!Force && (PBR_RECORD_MODE == PbrMode || PBR_PLAYBACK_MODE == PbrMode)) {
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, BUFFERED_FREED_MSG);
      ReturnCode = PromptYesNo(&Confirmation);
      if (EFI_ERROR(ReturnCode)) {
        PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
        NVDIMM_DBG("Failed on PromptedInput");
      }
      //user does not want to proceed, exit command.
      if (!Confirmation) {
        goto Finish;
      }
    }

    //create a blank pbr buffer
    ReturnCode = CreatePbrBuffer(pNvmDimmPbrProtocol);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
      goto Finish;
    }

    //set to record mode
    ReturnCode = SetPbrMode(pNvmDimmPbrProtocol, PBR_RECORD_MODE);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_SET_PBR_MODE, pModeValue);
      goto Finish;
    }

    //reset the tagid to 0 (first tag)
    PbrDcpmmSerializeTagId(0);

    //print which mode we just configured
    PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, ACTION_SETTING_MODE, pModeValue);
  }
  //start playback session
  else if (StrICmp(pModeValue, PBR_PLAYBACK_MANUAL_MODE_VAL) == 0 ||
    StrICmp(pModeValue, PBR_PLAYBACK_MODE_VAL) == 0) {

    //only print if manual mode, otherwise obvious what mode was just executed.
    if (0 == StrICmp(pModeValue, PBR_PLAYBACK_MANUAL_MODE_VAL)) {
      PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, ACTION_SETTING_MODE, pModeValue);
      //default starting session tag is 0x0, unless specified by -tag option.
      ReturnCode = ResetPbrSession(pNvmDimmPbrProtocol, 0);
      if (EFI_ERROR(ReturnCode)) {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_SET_SESSION_TAG);
        goto Finish;
      }
    }

    //set to playback mode
    ReturnCode = SetPbrMode(pNvmDimmPbrProtocol, PBR_PLAYBACK_MODE);
    //no session loaded
    if (EFI_NOT_READY == ReturnCode) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_NO_PBR_SESSION_LOADED);
      goto Finish;
    }
    else if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_SET_PBR_MODE, pModeValue);
      goto Finish;
    }

    //user specified to playback starting from a specific session tag/id
    if (pTagValue) {
      if (GetU64FromString(pTagValue, &TagId64)) {
        ReturnCode = ResetPbrSession(pNvmDimmPbrProtocol, (UINT32)TagId64);
        if (EFI_ERROR(ReturnCode)) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_RESET_SESSION);
          goto Finish;
        }
        //user is specifying a tag to execute
        PbrDcpmmSerializeTagId((UINT32)TagId64);
      }
      else {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
        goto Finish;
      }
    }
    else {
      ReturnCode = ResetPbrSession(pNvmDimmPbrProtocol, 0);
      if (EFI_ERROR(ReturnCode)) {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_RESET_SESSION);
        goto Finish;
      }
      //reset the tagid to 0 (first tag)
      PbrDcpmmSerializeTagId(0);
    }

    //if playback mode, automatically execute pbr buffer
    if (StrICmp(pModeValue, PBR_PLAYBACK_MODE_VAL) == 0) {
      ReturnCode = ExecuteCommands(pPrinterCtx, pNvmDimmPbrProtocol, (UINT32)TagId64);
    }
  }
  else {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNKNOWN_MODE);
    goto Finish;
  }
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  return ReturnCode;
}

/**
  Register the Recover Format command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterStartSessionCommand()
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&StartSessionCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Helper that creates a new pbr recording buffer
**/
STATIC EFI_STATUS CreatePbrBuffer(EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol) {

  if (NULL == pNvmDimmPbrProtocol) {
    return EFI_INVALID_PARAMETER;
  }
  return pNvmDimmPbrProtocol->PbrSetSession(NULL, 0);
}

/**
  Helper for setting the playback/record mode
**/
STATIC EFI_STATUS SetPbrMode(EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol, UINT32 Mode) {

  if (NULL == pNvmDimmPbrProtocol) {
    return EFI_INVALID_PARAMETER;
  }
  return pNvmDimmPbrProtocol->PbrSetMode(Mode);
}

/**
  Helper for setting the current session tag id
**/
STATIC EFI_STATUS ResetPbrSession(EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol, UINT32 Id) {

  if (NULL == pNvmDimmPbrProtocol) {
    return EFI_INVALID_PARAMETER;
  }
  return pNvmDimmPbrProtocol->PbrResetSession(Id);
}

/**
  Helper for executing all cmds within a pbr buffer
**/
STATIC EFI_STATUS ExecuteCommands(PRINT_CONTEXT *pPrinterCtx, EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol, UINT32 TagId) {
  EFI_STATUS ReturnCode;
  UINT32 TagCount = 0;
  CHAR16 *pName = NULL;
  CHAR16 *pDescription = NULL;
  UINT32 Index = 0;
  UINT32 Signature;

#ifdef OS_BUILD
  EFI_HANDLE FakeBindHandle = (EFI_HANDLE)0x1;
#endif

  ReturnCode = pNvmDimmPbrProtocol->PbrGetTagCount(&TagCount);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_GET_SESSION_TAG_COUNT);
    goto Finish;
  }


  for (Index = TagId; Index < TagCount; ++Index) {

    ReturnCode = pNvmDimmPbrProtocol->PbrGetTag(Index, &Signature, &pName, &pDescription, NULL, NULL);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_GET_SESSION_TAG);
      goto Finish;
    }

#ifdef OS_BUILD
    PbrDcpmmSerializeTagId(Index+1);
#endif

    if (0 == StrICmp(PBR_DRIVER_INIT_TAG_DESCRIPTION, pName)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, MANUAL_ACTION_REQUIRED);
      goto Finish;
    }

    ResetPbrSession(pNvmDimmPbrProtocol, Index);

    //if running under the OS, the driver binding start routine needs to run before each command
    //handler.  This will simulate how individual cmds are executed from the CLI.
#ifdef OS_BUILD
    ReturnCode = NvmDimmDriverDriverBindingStart(&gNvmDimmDriverDriverBinding, FakeBindHandle, NULL);
    /*if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_DURING_DRIVER_INIT);
      goto Finish;
    }*/
#endif
    ExecuteCommand(pName);
#ifdef OS_BUILD
    ReturnCode = NvmDimmDriverDriverBindingStop(&gNvmDimmDriverDriverBinding, FakeBindHandle, 0, NULL);
    /*if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_DURING_DRIVER_UNINIT);
      goto Finish;
    }*/
#endif

    FREE_POOL_SAFE(pName);
    FREE_POOL_SAFE(pDescription);
  }

Finish:
  FREE_POOL_SAFE(pName);
  FREE_POOL_SAFE(pDescription);
  return ReturnCode;
}

/**
  Helper for executing a single cmd handler based on CLI arg string, i.e. "show -dimm"
**/
STATIC EFI_STATUS ExecuteCommand(CHAR16 *pCmdInput) {
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  struct CommandInput Input;
  struct Command Command;
  if (NULL == pCmdInput) {
    NVDIMM_DBG("pCmdInput value is NULL");
    return ReturnCode;
  }
  FillCommandInput(pCmdInput, &Input);
  ReturnCode = Parse(&Input, &Command);

  if (!EFI_ERROR(ReturnCode)) {
    /* parse success, now run the command */
    ReturnCode = ExecuteCmd(&Command);
  }

  FreeCommandInput(&Input);
  return ReturnCode;
}
