/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "SetDimmCommand.h"
#include <Uefi.h>
#include <Library/ShellLib.h>
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include "Common.h"

CONST CHAR16 *pPoisonMemoryTypeStr[POISON_MEMORY_TYPE_COUNT] = {
  L"MemoryMode",
  L"AppDirect",
  L"NotDefined",
  L"PatrolScrub"
};


/** Command syntax definition **/
struct Command SetDimmCommand =
{
  SET_VERB,                                                         //!< verb
  {                                                                 //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"",HELP_FORCE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, L"Source of the Passphrase file",FALSE, ValueRequired},
    {L"", MASTER_OPTION, L"", L"",L"Set Master Passphrase", FALSE, ValueEmpty},
    {L"", DEFAULT_OPTION, L"", L"",L"Set Default Settings", FALSE, ValueEmpty}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {{DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}},      //!< targets
  {
    {CLEAR_ERROR_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired},
    {TEMPERATURE_INJ_PROPERTY, L"", HELP_TEXT_VALUE, FALSE, ValueRequired },
    {POISON_INJ_PROPERTY, L"", HELP_TEXT_VALUE, FALSE, ValueRequired },
    {POISON_TYPE_INJ_PROPERTY, L"", HELP_TEXT_VALUE, FALSE, ValueRequired},
    {PACKAGE_SPARING_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired },
    {PERCENTAGE_REAMAINING_INJ_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {FATAL_MEDIA_ERROR_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired},
    {DIRTY_SHUTDOWN_ERROR_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired},
    {LOCKSTATE_PROPERTY, L"", LOCKSTATE_VALUE_DISABLED L"|" LOCKSTATE_VALUE_UNLOCKED L"|" LOCKSTATE_VALUE_FROZEN, FALSE, ValueRequired},
    {PASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional},
    {NEWPASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional},
    {CONFIRMPASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional},
    {AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY, L"", HELP_TEXT_AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY, FALSE, ValueRequired},
    {AVG_PWR_REPORTING_TIME_CONSTANT, L"", HELP_TEXT_AVG_PWR_REPORTING_TIME_CONSTANT_PROPERTY, FALSE, ValueRequired}
    }, //!< properties
    L"Set properties of one or more " PMEM_MODULES_STR L", such as device security and modify device.",                          //!< help
  SetDimm,
  TRUE
};
CHAR16* GetCorrectClearMessageBasedOnProperty(UINT16 ErrorInjectType) {
  CHAR16 *ClearOutputPropertyString = NULL;
  switch(ErrorInjectType) {
  case ERROR_INJ_POISON:
    ClearOutputPropertyString = CLI_INFO_CLEAR_POISON_INJECT_ERROR;
    break;
  case ERROR_INJ_TEMPERATURE:
    ClearOutputPropertyString = CLI_INFO_CLEAR_TEMPERATURE_INJECT_ERROR;
    break;
  case ERROR_INJ_PACKAGE_SPARING:
    ClearOutputPropertyString = CLI_INFO_CLEAR_PACKAGE_SPARING_INJECT_ERROR;
    break;
  case ERROR_INJ_PERCENTAGE_REMAINING:
    ClearOutputPropertyString = CLI_INFO_CLEAR_PERCENTAGE_REMAINING_INJECT_ERROR;
    break;
  case ERROR_INJ_FATAL_MEDIA_ERR:
    ClearOutputPropertyString = CLI_INFO_CLEAR_FATAL_MEDIA_ERROR_INJECT_ERROR;
    break;
  case ERROR_INJ_DIRTY_SHUTDOWN:
    ClearOutputPropertyString = CLI_INFO_CLEAR_DIRTY_SHUT_DOWN_INJECT_ERROR;
    break;
  }
  return ClearOutputPropertyString;
}
/**
  Execute the set dimm command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
SetDimm(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pLockStatePropertyValue = NULL;
  CHAR16 *pPassphrase = NULL;
  CHAR16 *pNewPassphrase = NULL;
  CHAR16 *pConfirmPassphrase = NULL;
  CHAR16 *pPassphraseStatic = NULL;
  CHAR16 *pNewPassphraseStatic = NULL;
  CHAR16 *pConfirmPassphraseStatic = NULL;
  CHAR16 *pAvgPowerReportingTimeConstantMultValue = NULL;
  CHAR16 *pAvgPowerReportingTimeConstantValue = NULL;
  BOOLEAN IsNumber = FALSE;
  UINT64 ParsedNumber = 0;
  UINT8 *pAvgPowerReportingTimeConstantMult = NULL;
  UINT32 *pAvgPowerReportingTimeConstant = NULL;
  CHAR16 *pTargetValue = NULL;
  CHAR16 *pLoadUserPath = NULL;
  CHAR16 *pLoadFilePath = NULL;
  CHAR16 *pErrorMessage = NULL;
  UINT16 SecurityOperation = SECURITY_OPERATION_UNDEFINED;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  UINT32 DimmIdsCount = 0;
  UINT32 Index = 0;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pCommandStatusMessage = NULL;
  CHAR16 *pCommandStatusPreposition = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN ActionSpecified = FALSE;
  BOOLEAN OneOfPassphrasesIsEmpty = FALSE;
  BOOLEAN OneOfPassphrasesIsNotEmpty = FALSE;
  BOOLEAN LockStateFrozen = FALSE;
  BOOLEAN Force = FALSE;
  BOOLEAN Confirmation = FALSE;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  PRINT_CONTEXT *pPrinterCtx = NULL;
  BOOLEAN MasterOptionSpecified = FALSE;
  BOOLEAN DefaultOptionSpecified = FALSE;

  /*Inject error*/
  CHAR16 *pTemperature = NULL;
  CHAR16 *pPoisonAddress = NULL;
  CHAR16 *pPoisonType = NULL;
  CHAR16 *pPackageSparing = NULL;
  CHAR16 *pPercentageRemaining = NULL;
  CHAR16 *pFatalMediaError = NULL;
  CHAR16 *pDirtyShutDown = NULL;
  CHAR16 *pClearErrorInj = NULL;
  UINT16 ErrInjectType = ERROR_INJ_TYPE_INVALID;
  UINT64 TemperatureValue = 0;
  UINT64 PoisonAddressValue = 0;
  UINT64 PercentageRemainingValue = 0;
  UINT64 PoisonTypeValue = POISON_MEMORY_TYPE_PATROLSCRUB;

  UINT64 ErrorInjectionTypeSet = 0;
  UINT64 PoisonTypeValid = 0;
  UINT64 ClearStatus = 0;
  UINT64 FatalMediaError = 0;
  UINT64 PackageSparing = 0;
  UINT64 DirtyShutDown = 0;
  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
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
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
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
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
    if (!AllDimmsInListInSupportedConfig(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_POPULATION_VIOLATION);
      goto Finish;
    }
  }

  /** If no dimm IDs are specified get IDs from all dimms **/
  if (DimmIdsCount == 0) {
      ReturnCode = GetManageableDimmsNumberAndId(pNvmDimmConfigProtocol, TRUE, &DimmIdsCount, &pDimmIds);
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

  /** Check force option **/
  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  /**
      This command allows for different property sets depending on what action is to be taken.
      Here we check if input contains properties from different actions because they are not
      allowed together.
  **/
  if (!EFI_ERROR(ContainsProperty(pCmd, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY))
    || !EFI_ERROR(ContainsProperty(pCmd, AVG_PWR_REPORTING_TIME_CONSTANT))) {
    ActionSpecified = TRUE;
  }

  if (containsOption(pCmd, SOURCE_OPTION) ||
    !EFI_ERROR(ContainsProperty(pCmd, PASSPHRASE_PROPERTY)) ||
    !EFI_ERROR(ContainsProperty(pCmd, NEWPASSPHRASE_PROPERTY)) ||
    !EFI_ERROR(ContainsProperty(pCmd, CONFIRMPASSPHRASE_PROPERTY)) ||
    !EFI_ERROR(ContainsProperty(pCmd, LOCKSTATE_PROPERTY))
    ) {
      if (ActionSpecified) {
        /** We already found a specified action, more are not allowed **/
        ReturnCode = EFI_INVALID_PARAMETER;
      } else {
        /** Found specified action **/
       ActionSpecified = TRUE;
      }
    }

    if (!EFI_ERROR(ContainsProperty(pCmd, TEMPERATURE_INJ_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        } else {
            /** Found specified action **/
            ActionSpecified = TRUE;
            ErrorInjectionTypeSet = 1;
        }
    }

    if (!EFI_ERROR(ContainsProperty(pCmd, POISON_INJ_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        } else {
            /** Found specified action **/
            ActionSpecified = TRUE;
            ErrorInjectionTypeSet = 1;
        }
    }
    /*If there is poison type property then there should be poison address property */
    if (!EFI_ERROR(ContainsProperty(pCmd, POISON_TYPE_INJ_PROPERTY))) {
        GetPropertyValue(pCmd, POISON_INJ_PROPERTY, &pPoisonAddress);
        if ((ActionSpecified && pPoisonAddress == NULL) || !ActionSpecified) {
          ReturnCode = EFI_INVALID_PARAMETER;
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERROR_POISON_TYPE_WITHOUT_ADDRESS);
          goto Finish;
        }
    }

    if (!EFI_ERROR(ContainsProperty(pCmd, PACKAGE_SPARING_INJ_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        } else {
            /** Found specified action **/
            ActionSpecified = TRUE;
            ErrorInjectionTypeSet = 1;
        }
    }

    if (!EFI_ERROR(ContainsProperty(pCmd, PERCENTAGE_REAMAINING_INJ_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        } else {
            /** Found specified action **/
            ActionSpecified = TRUE;
            ErrorInjectionTypeSet = 1;
        }
    }

    if (!EFI_ERROR(ContainsProperty(pCmd, FATAL_MEDIA_ERROR_INJ_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        } else {
            /** Found specified action **/
            ActionSpecified = TRUE;
            ErrorInjectionTypeSet = 1;
        }
    }

    if (!EFI_ERROR(ContainsProperty(pCmd, DIRTY_SHUTDOWN_ERROR_INJ_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        } else {
            /** Found specified action **/
            ActionSpecified = TRUE;
            ErrorInjectionTypeSet = 1;
        }
    }
    /*Clear error injection requires exacly one error injection type being set*/
    if (!EFI_ERROR(ContainsProperty(pCmd, CLEAR_ERROR_INJ_PROPERTY))) {
        if ((ActionSpecified && !ErrorInjectionTypeSet) || !ActionSpecified) {
          ReturnCode = EFI_INVALID_PARAMETER;
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERROR_CLEAR_PROPERTY_NOT_COMBINED);
          goto Finish;
        }
    }

  /** Syntax error - mixed properties from different set -dimm commands **/
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
    goto Finish;
  }
  /** Syntax error - no properties specified. **/
  if (!ActionSpecified) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  /**
    Set Security State
  **/
  GetPropertyValue(pCmd, LOCKSTATE_PROPERTY, &pLockStatePropertyValue);
  GetPropertyValue(pCmd, PASSPHRASE_PROPERTY, &pPassphraseStatic);
  GetPropertyValue(pCmd, NEWPASSPHRASE_PROPERTY, &pNewPassphraseStatic);
  GetPropertyValue(pCmd, CONFIRMPASSPHRASE_PROPERTY, &pConfirmPassphraseStatic);

  if (MasterOptionSpecified) {
    ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount, DIMM_INFO_CATEGORY_SECURITY, pDimms);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_ABORTED;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
      goto Finish;
    }

    for (Index = 0; Index < DimmCount; Index++) {
      if (pDimms[Index].MasterPassphraseEnabled != TRUE) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_MASTER_PASSPHRASE_NOT_ENABLED);
        goto Finish;
      }
    }
  }

  pLoadFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pLoadFilePath));
  if (pLoadFilePath == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  if (pLockStatePropertyValue != NULL ||
      pPassphraseStatic != NULL ||
      pNewPassphraseStatic != NULL ||
      pConfirmPassphraseStatic != NULL ||
      containsOption(pCmd, SOURCE_OPTION)) {

    NVDIMM_DBG("pPropertyValue=%p", pLockStatePropertyValue);
    NVDIMM_DBG("pPassphrase=%p", pPassphraseStatic);
    NVDIMM_DBG("pNewPassphrase=%p", pNewPassphraseStatic);
    NVDIMM_DBG("pConfirmPassphrase=%p", pConfirmPassphraseStatic);

    LockStateFrozen = (pLockStatePropertyValue != NULL &&
        (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_FROZEN) == 0));

    if (pLockStatePropertyValue == NULL && pPassphraseStatic == NULL && pNewPassphraseStatic != NULL &&
        pConfirmPassphraseStatic != NULL) { // Passphrase not provided
      ReturnCode = CheckMasterAndDefaultOptions(pCmd, FALSE, MasterOptionSpecified, DefaultOptionSpecified);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      if (MasterOptionSpecified) {
        pPassphrase = CatSPrint(NULL, L"");
        SecurityOperation = SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE;
        pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Change master passphrase");
      }
      else {
        SecurityOperation = SECURITY_OPERATION_SET_PASSPHRASE;
        pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Set passphrase");
      }
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L" on");
    } else if (pLockStatePropertyValue == NULL && pPassphraseStatic != NULL && pNewPassphraseStatic != NULL &&
        pConfirmPassphraseStatic != NULL) { // Passphrase provided
      ReturnCode = CheckMasterAndDefaultOptions(pCmd, TRUE, MasterOptionSpecified, DefaultOptionSpecified);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      if (MasterOptionSpecified) {
        SecurityOperation = SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE;
        pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Change master passphrase");
      }
      else {
        SecurityOperation = SECURITY_OPERATION_CHANGE_PASSPHRASE;
        pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Change passphrase");
      }
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L" on");
    } else if (pLockStatePropertyValue != NULL && pNewPassphraseStatic != NULL &&
          ((StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_DISABLED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_UNLOCKED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_FROZEN) == 0))) {
      pErrorMessage = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, L"NewPassphrase=");
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR, pErrorMessage);
      goto Finish;

    } else if (pLockStatePropertyValue != NULL && pConfirmPassphraseStatic != NULL &&
          ((StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_DISABLED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_UNLOCKED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_FROZEN) == 0))) {
      pErrorMessage = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, L"ConfirmPassphrase=");
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR, pErrorMessage);
      goto Finish;

    } else if (pLockStatePropertyValue != NULL && pPassphraseStatic != NULL &&
          (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_DISABLED) == 0)) {
      SecurityOperation = SECURITY_OPERATION_DISABLE_PASSPHRASE;
      pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Remove passphrase");
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L" from");

    } else if (pLockStatePropertyValue != NULL && pPassphraseStatic != NULL &&
          (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_UNLOCKED) == 0)) {
      SecurityOperation = SECURITY_OPERATION_UNLOCK_DEVICE;
      pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Unlock");
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L"");

    } else if (LockStateFrozen) {
      SecurityOperation = SECURITY_OPERATION_FREEZE_DEVICE;
      pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Freeze lock");
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L"");

    } else {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCOMPLETE_SYNTAX);
      goto Finish;
    }

    OneOfPassphrasesIsEmpty = ((pPassphraseStatic != NULL && StrCmp(pPassphraseStatic, L"") == 0) ||
        (pNewPassphraseStatic != NULL && StrCmp(pNewPassphraseStatic, L"") == 0) ||
        (pConfirmPassphraseStatic != NULL && StrCmp(pConfirmPassphraseStatic, L"") == 0));
    OneOfPassphrasesIsNotEmpty = ((pPassphraseStatic != NULL && StrCmp(pPassphraseStatic, L"") != 0) ||
        (pNewPassphraseStatic != NULL && StrCmp(pNewPassphraseStatic, L"") != 0) ||
        (pConfirmPassphraseStatic != NULL && StrCmp(pConfirmPassphraseStatic, L"") != 0));
    // Check -source option
    if (containsOption(pCmd, SOURCE_OPTION) && !LockStateFrozen) {
      if (OneOfPassphrasesIsNotEmpty) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
        goto Finish;
      }

      pLoadUserPath = getOptionValue(pCmd, SOURCE_OPTION);
      if (pLoadUserPath == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        NVDIMM_ERR("Could not get -source value. Out of memory");
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
        goto Finish;
      }

      ReturnCode = GetDeviceAndFilePath(pLoadUserPath, pLoadFilePath, &pDevicePathProtocol);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_WRONG_FILE_PATH);
        goto Finish;
      }

      ReturnCode = ParseSourcePassFile(pCmd, pLoadFilePath, pDevicePathProtocol, &pPassphrase, &pNewPassphrase);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("ParseSourcePassFile failed (" FORMAT_EFI_STATUS ")", ReturnCode);
        goto Finish;
      }
      // Check if required passwords have been found in the file
      if ((pPassphrase == NULL && pPassphraseStatic != NULL) ||
          (pNewPassphrase == NULL && pNewPassphraseStatic != NULL)) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_WRONG_FILE_DATA);
        goto Finish;
      // NewPassphrase and ConfirmPassphrase occur together
      } else if (pNewPassphrase != NULL) {
        pConfirmPassphrase = CatSPrint(NULL, FORMAT_STR, pNewPassphrase);
      }
    // Check prompts
    } else {
      if (OneOfPassphrasesIsEmpty && OneOfPassphrasesIsNotEmpty) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
        goto Finish;
      }
      // Check prompt request Passphrase
      if (pPassphraseStatic != NULL && StrCmp(pPassphraseStatic, L"") == 0) {
        ReturnCode = PromptedInput(L"Enter passphrase:\n", FALSE, FALSE, &pPassphrase);
        if (EFI_ERROR(ReturnCode)) {
          ReturnCode = EFI_INVALID_PARAMETER;
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
          NVDIMM_DBG("Failed on PromptedInput");
          goto Finish;
        }
      } else if (pPassphraseStatic != NULL) {
        pPassphrase = CatSPrint(NULL, FORMAT_STR, pPassphraseStatic);
      }

      // Check prompt request NewPassphrase
      if (pNewPassphraseStatic != NULL && StrCmp(pNewPassphraseStatic, L"") == 0) {
        ReturnCode = PromptedInput(L"Enter new passphrase:", FALSE, FALSE, &pNewPassphrase);
        if (EFI_ERROR(ReturnCode)) {
          ReturnCode = EFI_INVALID_PARAMETER;
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
          NVDIMM_DBG("Failed on PromptedInput");
          goto Finish;
        }
      } else if (pNewPassphraseStatic != NULL) {
        pNewPassphrase = CatSPrint(NULL, FORMAT_STR, pNewPassphraseStatic);
      }

      // Check prompt request ConfirmPassphrase
      if (pConfirmPassphraseStatic != NULL && StrCmp(pConfirmPassphraseStatic, L"") == 0) {
        ReturnCode = PromptedInput(L"Confirm passphrase:", FALSE, FALSE, &pConfirmPassphrase);
        if (EFI_ERROR(ReturnCode)) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
          NVDIMM_DBG("Failed on PromptedInput");
          goto Finish;
        }
      } else if (pConfirmPassphraseStatic != NULL) {
        pConfirmPassphrase = CatSPrint(NULL, FORMAT_STR, pConfirmPassphraseStatic);
      }
    }

    if (pLockStatePropertyValue == NULL && pNewPassphrase != NULL && pConfirmPassphrase != NULL) {
      if ((StrCmp(pNewPassphrase, pConfirmPassphrase) != 0)) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASES_DO_NOT_MATCH);
        goto FinishCommandStatusSet;
      }
    }

    ReturnCode = pNvmDimmConfigProtocol->SetSecurityState(pNvmDimmConfigProtocol,
        pDimmIds, DimmIdsCount,
        SecurityOperation,
        pPassphrase, pNewPassphrase, pCommandStatus);

    goto FinishCommandStatusSet;
  }

  /**
    Set AveragePowerReportingTimeConstantMultiplier, AveragePowerReportingTimeConstantMultiplier
  **/
  GetPropertyValue(pCmd, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY, &pAvgPowerReportingTimeConstantMultValue);
  GetPropertyValue(pCmd, AVG_PWR_REPORTING_TIME_CONSTANT, &pAvgPowerReportingTimeConstantValue);

  if (pAvgPowerReportingTimeConstantMultValue || pAvgPowerReportingTimeConstantValue) {
    if (pAvgPowerReportingTimeConstantMultValue) {
      // If average power reporting time constant multiplier property exists, check its validity
      IsNumber = GetU64FromString(pAvgPowerReportingTimeConstantMultValue, &ParsedNumber);
      if (!IsNumber) {
        NVDIMM_WARN("Average power reporting time constant multiplier value is not a number");
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY_AVG_PWR_REPORTING_TIME_CONSTANT_MULT);
        goto Finish;
      }
      else if (ParsedNumber > AVG_PWR_REPORTING_TIME_CONSTANT_MULT_MAX) {
        NVDIMM_WARN("Average power reporting time constant multiplier value %d is greater than maximum %d", ParsedNumber, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_MAX);
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY_AVG_PWR_REPORTING_TIME_CONSTANT_MULT);
        goto Finish;
      }
      pAvgPowerReportingTimeConstantMult = AllocateZeroPool(sizeof(UINT8));
      *pAvgPowerReportingTimeConstantMult = (UINT8)ParsedNumber;
    }

    if (pAvgPowerReportingTimeConstantValue) {
      // If average power reporting time constant property exists, check its validity
      IsNumber = GetU64FromString(pAvgPowerReportingTimeConstantValue, &ParsedNumber);
      if (!IsNumber) {
        NVDIMM_WARN("Average power reporting time constant value is not a number");
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY_AVG_PWR_REPORTING_TIME_CONSTANT);
        goto Finish;
      }
      else if (ParsedNumber > AVG_PWR_REPORTING_TIME_CONSTANT_MAX) {
        NVDIMM_WARN("Average power reporting time constant value %d is greater than maximum %d", ParsedNumber, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_MAX);
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY_AVG_PWR_REPORTING_TIME_CONSTANT);
        goto Finish;
      }
      pAvgPowerReportingTimeConstant = AllocateZeroPool(sizeof(UINT32));
      *pAvgPowerReportingTimeConstant = (UINT32)ParsedNumber;
    }

      pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Modify");
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L"");

    if (!Force) {
      for (Index = 0; Index < DimmIdsCount; Index++) {
        ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
        if (EFI_ERROR(ReturnCode)) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
          goto Finish;
        }
        ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCode)) {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
          goto Finish;
        }
        PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, L"Modifying device settings on " PMEM_MODULE_STR L" (" FORMAT_STR L").", DimmStr);
        ReturnCode = PromptYesNo(&Confirmation);
        if (!EFI_ERROR(ReturnCode) && Confirmation) {
          ReturnCode = pNvmDimmConfigProtocol->SetOptionalConfigurationDataPolicy(pNvmDimmConfigProtocol,
            &pDimmIds[Index], 1, pAvgPowerReportingTimeConstantMult, pAvgPowerReportingTimeConstant, pCommandStatus);
        } else {
          PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, L"Skipping modify device settings on " PMEM_MODULE_STR L" (" FORMAT_STR L")\n", DimmStr);
          continue;
        }
      }
    } else {
      ReturnCode = pNvmDimmConfigProtocol->SetOptionalConfigurationDataPolicy(pNvmDimmConfigProtocol,
        pDimmIds, DimmIdsCount, pAvgPowerReportingTimeConstantMult, pAvgPowerReportingTimeConstant, pCommandStatus);
      goto FinishCommandStatusSet;
    }
  } else {
    // It is ok if average power reporting time constant multiplier property doesn't exist, it is an optional param
    ReturnCode = EFI_SUCCESS;
  }

  GetPropertyValue(pCmd, TEMPERATURE_INJ_PROPERTY, &pTemperature);
  GetPropertyValue(pCmd, POISON_INJ_PROPERTY, &pPoisonAddress);
  GetPropertyValue(pCmd, POISON_TYPE_INJ_PROPERTY, &pPoisonType);
  GetPropertyValue(pCmd, PACKAGE_SPARING_INJ_PROPERTY, &pPackageSparing);
  GetPropertyValue(pCmd, PERCENTAGE_REAMAINING_INJ_PROPERTY, &pPercentageRemaining);
  GetPropertyValue(pCmd, FATAL_MEDIA_ERROR_INJ_PROPERTY, &pFatalMediaError);
  GetPropertyValue(pCmd, DIRTY_SHUTDOWN_ERROR_INJ_PROPERTY, &pDirtyShutDown);
  GetPropertyValue(pCmd, CLEAR_ERROR_INJ_PROPERTY, &pClearErrorInj);

  // Implementation notes:
  // Assumes only one property can be set.
  // pCommandStatusMessage seems to be a print wrapper around the pCommandStatus that
  //   is finally returned from InjectError() at the bottom.
  // If there is an error with converting to an integer, we don't want to print
  //   out the command status message as InjectError() is never called. Go to finish
  //   with an invalid parameter

  /**
  Inject error Temperature
  **/
  if (pTemperature != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_TEMPERATURE_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ErrInjectType = ERROR_INJ_TEMPERATURE;
    ReturnCode = GetU64FromString(pTemperature, &TemperatureValue);
    if (!ReturnCode) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  /**
  Inject Poison Error
  **/
  if (pPoisonAddress != NULL) {
      pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_POISON_INJECT_ERROR, pPoisonAddress);
      pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
      ErrInjectType = ERROR_INJ_POISON;
      ReturnCode =  IsHexValue(pPoisonAddress, FALSE);
    // ReturnCode here indicates if it is hex value
      if (!ReturnCode) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'Poison'\n", CLI_SYNTAX_ERROR,
          pPoisonAddress, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
        goto Finish;
      }
    ReturnCode = GetU64FromString(pPoisonAddress, &PoisonAddressValue);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'Poison'\n", CLI_SYNTAX_ERROR,
        pPoisonAddress, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      goto Finish;
    }
  }

  /**
  Inject Poison Type Error
  **/
  if (pPoisonType != NULL) {
      /**
        Check if poison MemoryType is valid
      **/
      for (Index = 0; Index < POISON_MEMORY_TYPE_COUNT; ++Index) {
          if (0 == StrICmp(pPoisonType, pPoisonMemoryTypeStr[Index])) {
              PoisonTypeValid = 1;
              PoisonTypeValue = (UINT8)Index + 1;
          }
      }
      if (!PoisonTypeValid) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_POISON_TYPE);
        goto Finish;
      }
  }

  /**
  PACKAGE_SPARING_INJ_PROPERTY
  **/
  if (pPackageSparing != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_PACKAGE_SPARING_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pPackageSparing, (UINT64 *)&PackageSparing);

      if (!ReturnCode || 1 != PackageSparing) {
        ReturnCode = EFI_INVALID_PARAMETER;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'PackageSparing'\n", CLI_SYNTAX_ERROR,
          pPackageSparing, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
        goto Finish;
      }
      ErrInjectType = ERROR_INJ_PACKAGE_SPARING;
  }
  /**
   Percentage remaining property
  **/
  if (pPercentageRemaining != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_PERCENTAGE_REMAINING_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pPercentageRemaining, &PercentageRemainingValue);

    if (!ReturnCode || PercentageRemainingValue > 100) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'PercentageRemaining'\n", CLI_SYNTAX_ERROR,
        pPercentageRemaining, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      goto Finish;
    }
    ErrInjectType = ERROR_INJ_PERCENTAGE_REMAINING;
  }

  /**
   Fatal Media Error Inj property
  **/
  if (pFatalMediaError != NULL) {
#if defined( _MSC_VER ) && defined( OS_BUILD ) // Windows
    // We can't recover from an injected fatal media error in Windows
    // because nvmdimm.sys requires GetCommandEffectLog to work and that
    // command requires the media to be up because it uses the large payload. Therefore
    // redirect people to use the Microsoft tools for doing this that ignores the
    // lack of a Command Effect Log for the dimm.
    Print(CLI_ERR_INJECT_FATAL_ERROR_UNSUPPORTED_ON_OS);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
#endif // _MSC_VER
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_FATAL_MEDIA_ERROR_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pFatalMediaError, &FatalMediaError);
    if (!ReturnCode || 1 != FatalMediaError) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'FatalMediaError'\n", CLI_SYNTAX_ERROR,
        pFatalMediaError, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      goto Finish;
    }
    ErrInjectType = ERROR_INJ_FATAL_MEDIA_ERR;
  }

  /**
   Dirty shutdown error injection
  **/
  if (pDirtyShutDown != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_DIRTY_SHUT_DOWN_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pDirtyShutDown, &DirtyShutDown);

    if (!ReturnCode ||  1 != DirtyShutDown) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'DirtyShutDown'\n", CLI_SYNTAX_ERROR, pDirtyShutDown,
        CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      goto Finish;
    }
    ErrInjectType = ERROR_INJ_DIRTY_SHUTDOWN;
  }
  /**
    Clear error injection
  **/
  if (pClearErrorInj != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, GetCorrectClearMessageBasedOnProperty(ErrInjectType), pPoisonAddress);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pClearErrorInj, &ClearStatus);

    if (!ReturnCode ||  1 != ClearStatus) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'Clear'\n", CLI_SYNTAX_ERROR, ClearStatus,
        CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      goto Finish;
    }
  }
  if (ErrInjectType != ERROR_INJ_TYPE_INVALID) {
    ReturnCode = pNvmDimmConfigProtocol->InjectError(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount,
    (UINT8)ErrInjectType, (UINT8)ClearStatus, &TemperatureValue,
    &PoisonAddressValue, (UINT8 *)&PoisonTypeValue, (UINT8 *)&PercentageRemainingValue, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishCommandStatusSet;
    }
  }

FinishCommandStatusSet:
  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, pCommandStatusMessage, pCommandStatusPreposition, pCommandStatus);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  CleanUnicodeStringMemory(pLockStatePropertyValue);
  CleanUnicodeStringMemory(pPassphraseStatic);
  CleanUnicodeStringMemory(pNewPassphraseStatic);
  CleanUnicodeStringMemory(pConfirmPassphraseStatic);
  CleanUnicodeStringMemory(pPassphrase);
  CleanUnicodeStringMemory(pNewPassphrase);
  CleanUnicodeStringMemory(pConfirmPassphrase);
  FREE_POOL_SAFE(pPassphrase);
  FREE_POOL_SAFE(pNewPassphrase);
  FREE_POOL_SAFE(pConfirmPassphrase);
  FREE_POOL_SAFE(pLoadFilePath);
  FREE_POOL_SAFE(pLoadUserPath);
  FREE_POOL_SAFE(pErrorMessage);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pCommandStatusMessage);
  FREE_POOL_SAFE(pCommandStatusPreposition);
  FREE_POOL_SAFE(pAvgPowerReportingTimeConstantMult);
  FREE_POOL_SAFE(pAvgPowerReportingTimeConstant);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the set dimm command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterSetDimmCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&SetDimmCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
