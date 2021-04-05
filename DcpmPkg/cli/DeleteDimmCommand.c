/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef OS_BUILD
#include <Uefi.h>
#include <Library/ShellLib.h>
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include "DeleteDimmCommand.h"
#include "SetDimmCommand.h"
#include "Common.h"

/**
  Command syntax definition
**/
struct Command DeleteDimmCommand =
{
  DELETE_VERB,                                                           //!< verb
  {                                                                      //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT,FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"",HELP_FORCE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, L"Source of Passphrase file",FALSE, ValueRequired},
    {L"", MASTER_OPTION, L"", L"", L"Master Passphrase",FALSE, ValueEmpty},
    {L"", DEFAULT_OPTION, L"", L"", L"Default settings",FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {{DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}},         //!< targets
  {{PASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional}},  //!< properties
  L"Erase persistent data on one or more " PMEM_MODULES_STR L".",                        //!< help
  DeleteDimm, TRUE
};

/**
  Execute the delete dimm command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
DeleteDimm(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pLoadUserPath = NULL;
  CHAR16 *pLoadFilePath = NULL;
  CHAR16 *pPassphrase = NULL;
  CHAR16 *pPassphraseStatic = NULL;
  CHAR16 *pTargetValue = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  UINT16 Index = 0;
  UINT32 DimmIdsCount = 0;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN Force = FALSE;
  BOOLEAN Confirmation = FALSE;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  PRINT_CONTEXT *pPrinterCtx = NULL;
  BOOLEAN MasterOptionSpecified = FALSE;
  BOOLEAN DefaultOptionSpecified = FALSE;
  UINT16 SecurityOperation = SECURITY_OPERATION_UNDEFINED;
  DIMM_INFO DimmInfo;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

 // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_SECURITY, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  /** If no dimm IDs are specified get IDs from all dimms **/
  if (DimmIdsCount == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(pNvmDimmConfigProtocol, FALSE, &DimmIdsCount, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    if (DimmIdsCount == 0) {
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_MANAGEABLE_DIMMS);
      goto Finish;
    }
  }

  /** Check master option **/
  MasterOptionSpecified = containsOption(pCmd, MASTER_OPTION);

  /** Check default option **/
  DefaultOptionSpecified = containsOption(pCmd, DEFAULT_OPTION);

  /** Check if default option is supported on selected modules **/
  if (DefaultOptionSpecified) {
    for (Index = 0; Index < DimmIdsCount; Index++) {
      CHECK_RESULT((pNvmDimmConfigProtocol->GetDimm(pNvmDimmConfigProtocol, pDimmIds[Index], DIMM_INFO_CATEGORY_NONE, &DimmInfo)), Finish);
      if (IsDefaultMasterPassphraseRestricted(DimmInfo)) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DEFAULT_NOT_SUPPORTED_FIRMWARE_REV);
        goto Finish;
      }
    }
  }

  /** Check force option **/
  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  if (MasterOptionSpecified) {
    // FALSE = Master passphrase must be enabled for FIS >= 3.2 PMem modules as well
    if (!AllDimmsInListHaveMasterPassphraseEnabled(pDimms, DimmCount, pDimmIds, DimmIdsCount, FALSE)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_MASTER_PASSPHRASE_NOT_ENABLED);
      goto Finish;
    }
  }

  ReturnCode = GetPropertyValue(pCmd, PASSPHRASE_PROPERTY, &pPassphraseStatic);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode != EFI_NOT_FOUND) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
  }

  if ((pPassphraseStatic != NULL) &&
      (containsOption(pCmd, SOURCE_OPTION)) &&
      (StrCmp(pPassphraseStatic, L"") != 0)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
    goto Finish;
  }

  // Check -source option
  if ((pPassphraseStatic != NULL) &&
      (containsOption(pCmd, SOURCE_OPTION)) &&
      (StrCmp(pPassphraseStatic, L"") == 0)) {
    pLoadUserPath = getOptionValue(pCmd, SOURCE_OPTION);
    if (pLoadUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -source value. Out of memory");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    ReturnCode = CheckMasterAndDefaultOptions(pCmd, TRUE, MasterOptionSpecified, DefaultOptionSpecified);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (MasterOptionSpecified) {
      SecurityOperation = SECURITY_OPERATION_MASTER_ERASE_DEVICE;
    }
    else {
      SecurityOperation = SECURITY_OPERATION_ERASE_DEVICE;
    }

    pLoadFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pLoadFilePath));
    if (pLoadFilePath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    ReturnCode = GetDeviceAndFilePath(pLoadUserPath, pLoadFilePath, &pDevicePathProtocol);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
      goto Finish;
    }
    ReturnCode = ParseSourcePassFile(pCmd, pLoadFilePath, pDevicePathProtocol, &pPassphrase, NULL);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

  // Check if prompt
  } else if ((pPassphraseStatic != NULL) && (StrCmp(pPassphraseStatic, L"") == 0)) {
    ReturnCode = CheckMasterAndDefaultOptions(pCmd, TRUE, MasterOptionSpecified, DefaultOptionSpecified);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (MasterOptionSpecified) {
      SecurityOperation = SECURITY_OPERATION_MASTER_ERASE_DEVICE;
    }
    else {
      SecurityOperation = SECURITY_OPERATION_ERASE_DEVICE;
    }

    ReturnCode = PromptedInput(L"Enter passphrase:\n", FALSE, FALSE, &pPassphrase);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
      NVDIMM_DBG("Failed on PromptedInput");
      goto Finish;
    }
  } else if (pPassphraseStatic != NULL) { // Passphrase provided
    ReturnCode = CheckMasterAndDefaultOptions(pCmd, TRUE, MasterOptionSpecified, DefaultOptionSpecified);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (MasterOptionSpecified) {
      SecurityOperation = SECURITY_OPERATION_MASTER_ERASE_DEVICE;
    }
    else {
      SecurityOperation = SECURITY_OPERATION_ERASE_DEVICE;
    }

    pPassphrase = CatSPrint(NULL, FORMAT_STR, pPassphraseStatic);
  } else { // Passphrase not provided
    ReturnCode = CheckMasterAndDefaultOptions(pCmd, FALSE, MasterOptionSpecified, DefaultOptionSpecified);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (MasterOptionSpecified) {
      SecurityOperation = SECURITY_OPERATION_MASTER_ERASE_DEVICE;
     }
    else {
      SecurityOperation = SECURITY_OPERATION_ERASE_DEVICE;
    }

    // At this point an empty string has already been handled and we are now assuming passphrase
    // was not given because security is disabled and passphrase doesn't matter OR the secure erase
    // request is for the default master passphrase and an empty string is passed in for the default
    // master passphrase
    pPassphrase = CatSPrint(NULL, L"");
  }

  /** Ask for prompt when Force option is not given **/
  if (!Force) {
    for (Index = 0; Index < DimmIdsCount; Index++) {
      ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, L"Erasing " PMEM_MODULE_STR L" " FORMAT_STR L".", DimmStr);
      ReturnCode = PromptYesNo(&Confirmation);
      if (!EFI_ERROR(ReturnCode) && Confirmation) {
        ReturnCode = pNvmDimmConfigProtocol->SetSecurityState(pNvmDimmConfigProtocol,&pDimmIds[Index], 1,
              SecurityOperation, pPassphrase, NULL, pCommandStatus);
        if (EFI_ERROR(ReturnCode)) {
          goto FinishCommandStatusSet;
        }
      } else {
        PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, L"Skipped erasing data from " PMEM_MODULE_STR L" " FORMAT_STR L"\n", DimmStr);
        continue;
      }
    }
  } else {
    ReturnCode = pNvmDimmConfigProtocol->SetSecurityState(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount,
          SecurityOperation, pPassphrase, NULL, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishCommandStatusSet;
    }
  }

FinishCommandStatusSet:
  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, ERASE_STR, L"", pCommandStatus);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  CleanUnicodeStringMemory(pPassphrase);
  CleanUnicodeStringMemory(pPassphraseStatic);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pPassphrase);
  FREE_POOL_SAFE(pLoadFilePath);
  FREE_POOL_SAFE(pLoadUserPath);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the delete dimm command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterDeleteDimmCommand(
  )
{
  EFI_STATUS rc;
  NVDIMM_ENTRY();

  rc = RegisterCommand(&DeleteDimmCommand);

  NVDIMM_EXIT_I64(rc);
  return rc;
}

#endif /** OS_BUILD **/
