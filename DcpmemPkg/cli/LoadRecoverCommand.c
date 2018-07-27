/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "LoadRecoverCommand.h"
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Protocol/EfiShell.h>
#include "Common.h"

#define PROGRESS_EVENT_TIMEOUT    EFI_TIMER_PERIOD_SECONDS(1)
#define PRINT_PRIORITY            8

/**
  Command syntax definition
**/
struct Command LoadRecoverCommand =
{
  LOAD_VERB,                                                              //!< verb
  {                                                                       //!< options
    {EXAMINE_OPTION_SHORT, EXAMINE_OPTION, L"", L"", FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, TRUE, ValueRequired},
    {L"", RECOVER_OPTION, L"", HELP_TEXT_FLASH_SPI, TRUE, ValueOptional}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {{DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}},          //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                                //!< properties
  L"Flash firmware on one or more non-functional DIMMs.",                 //!< help
  LoadRecover                                                             //!< run function
};

/**
  Register the load recover command
**/
EFI_STATUS
RegisterLoadRecoverCommand (
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&LoadRecoverCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the load recover command
  @param[in] pCmd the command structure that contains the user input data.

  @retval EFI_SUCCESS if everything went OK - including the firmware load process.
  @retval EFI_INVALID_PARAMETER if the user input is invalid or the file validation fails
  @retval EFI_UNSUPPORTED if the driver is not loaded or there are no DCPMEM modules in the system.
  @retval EFI_NOT_FOUND if there is no DIMM with the user specified PID
**/
EFI_STATUS
LoadRecover(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pFileName = NULL;
  CHAR16 *pRelativeFileName = NULL;
  CHAR16 *pTargetValue = NULL;
  CONST CHAR16 *pWorkingDirectory = NULL;
  EFI_SHELL_PROTOCOL *pEfiShell = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  BOOLEAN Examine = FALSE;
  FW_IMAGE_INFO *pFwImageInfo = NULL;
  EFI_HANDLE *pHandles = NULL;
  UINTN HandlesCount = 0;
  EFI_EVENT ProgressEvent = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  BOOLEAN FlashSpi = FALSE;
  CHAR16 *pOptionsValue = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  BOOLEAN Confirmation = FALSE;
  UINT16 Index1 = 0;
  UINT16 Index2 = 0;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **) &pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (DimmCount == 0) {
    Print(FORMAT_STR_NL, CLI_INFO_NO_NON_FUNCTIONAL_DIMMS);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
  if (pDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, DimmCount, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_INFO_LOAD_RECOVER_INVALID_DIMM);
      goto Finish;
    }
  }
  Print(CLI_RECOVER_DIMM_PROMPT_STR);
  // Print out the dimm identifiers to be recovered
  if (DimmIdsCount == 0) {
    // Iterate through all uninitialized dimms
    for (Index1 = 0; Index1 < DimmCount; Index1++) {
      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index1].DimmHandle, pDimms[Index1].DimmUid,
                  DimmStr, MAX_DIMM_UID_LENGTH);
      Print(L"%s ", DimmStr);
    }
  } else {
    // Print the handle/uid for each passed in dimm. It's already filtered by
    // GetDimmIdsFromString, but it doesn't return handle/uid
    for (Index1 = 0; Index1 < DimmIdsCount; Index1++) {
      for (Index2 = 0; Index2 < DimmCount; Index2++) {
        if (pDimms[Index2].DimmID == pDimmIds[Index1]) {
          ReturnCode = GetPreferredDimmIdAsString(pDimms[Index2].DimmHandle, pDimms[Index2].DimmUid,
                      DimmStr, MAX_DIMM_UID_LENGTH);
          Print(L"%s ", DimmStr);
          break;
        }
      }
    }
  }
  Print(L"\n");

  // Warn about disabling TSOD for SMBUS operations
  Print(CLI_RECOVER_DIMM_TSOD_REMINDER_STR);

  ReturnCode = PromptYesNo(&Confirmation);
  if (EFI_ERROR(ReturnCode) || !Confirmation) {
    ReturnCode = EFI_NOT_STARTED;
    goto Finish;
  }

  // check options
  pFileName = getOptionValue(pCmd, SOURCE_OPTION);
  if (pFileName == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_WRONG_FILE_PATH);
    goto Finish;
  }

  if (containsOption(pCmd, EXAMINE_OPTION) && containsOption(pCmd, EXAMINE_OPTION_SHORT)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_EXAMINE_USED_TOGETHER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Examine = containsOption(pCmd, EXAMINE_OPTION) || containsOption(pCmd, EXAMINE_OPTION_SHORT);

  pOptionsValue = getOptionValue(pCmd, RECOVER_OPTION);

  if (pOptionsValue == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  } else {
    if (StrICmp(pOptionsValue, RECOVER_OPTION_FLASH_SPI) == 0) {
      FlashSpi = TRUE;
    } else if (StrLen(pOptionsValue) == 0) {
      FlashSpi = FALSE;
    } else {
      ReturnCode = EFI_INVALID_PARAMETER;
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_RECOVER);
      goto Finish;
    }
  }

  /**
    In this case the user could have typed "FS0:\..."
    We are searching for the file on all FS so we need to remove the first chars until we have a "\"
  **/
  ReturnCode = GetRelativePath(pFileName, &pRelativeFileName);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, &gEfiShellProtocolGuid, NULL, &HandlesCount, &pHandles);

  if (!EFI_ERROR(ReturnCode) && HandlesCount < MAX_SHELL_PROTOCOL_HANDLES) {
    ReturnCode = gBS->OpenProtocol(pHandles[0], &gEfiShellProtocolGuid, (VOID *) &pEfiShell, NULL, NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (!EFI_ERROR(ReturnCode)) {
      pWorkingDirectory = pEfiShell->GetCurDir(NULL);
      if (pWorkingDirectory == NULL) {
        NVDIMM_WARN("Error while getting the Working Directory.");
      }
    } else {
      NVDIMM_WARN("Error while opening the shell protocol. Code: " FORMAT_EFI_STATUS "", ReturnCode);
    }
  } else {
    NVDIMM_WARN("Error while opening the shell protocol. Code: " FORMAT_EFI_STATUS "", ReturnCode);
    /**
      We can still try to open the file. If it is in the root directory, we will be able to open it.
    **/
  }

  if (Examine) {
    pFwImageInfo = AllocateZeroPool(sizeof(*pFwImageInfo));
    if (pFwImageInfo == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
  }

  // Create callback that will print progress
  if (!Examine) {
    gBS->CreateEvent((EVT_TIMER|EVT_NOTIFY_SIGNAL), PRINT_PRIORITY, PrintProgress, pCommandStatus, &ProgressEvent);
    gBS->SetTimer(ProgressEvent, TimerPeriodic, PROGRESS_EVENT_TIMEOUT);
  }

  ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pRelativeFileName,
      (CHAR16 *) pWorkingDirectory, Examine, FALSE, TRUE, FlashSpi, pFwImageInfo, pCommandStatus);

  if (Examine && pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU) {
    if (pFwImageInfo != NULL && pCommandStatus->GeneralStatus == NVM_SUCCESS) {

      Print(FORMAT_STR L": %02d.%02d.%02d.%04d\n",
        pFileName,
        pFwImageInfo->ImageVersion.ProductNumber.Version,
        pFwImageInfo->ImageVersion.RevisionNumber.Version,
        pFwImageInfo->ImageVersion.SecurityVersionNumber.Version,
        pFwImageInfo->ImageVersion.BuildNumber.Build);
    } else {
      Print(FORMAT_STR FORMAT_STR_NL, pFileName, CLI_ERR_VERSION_RETRIEVE);
    }
    DisplayCommandStatus(L"", L"", pCommandStatus);
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    goto Finish;
  } else {
    if (!EFI_ERROR(ReturnCode)) {
      Print(L"\n");
    }
    gBS->CloseEvent(ProgressEvent);
  }

  DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pOptionsValue);
  FREE_POOL_SAFE(pFileName);
  FREE_POOL_SAFE(pFwImageInfo);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
