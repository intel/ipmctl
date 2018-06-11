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

CONST CHAR16 *pFwDebugLogLevelStr[FW_LOG_LEVELS_COUNT] = {
  L"Error",
  L"Warning",
  L"Info",
  L"Debug"
};

CONST CHAR16 *pPoisonMemoryTypeStr[POISON_MEMORY_TYPE_COUNT] = {
	L"MemoryMode",
	L"AppDirect",
	L"NotDefined", // This was storage mode
	L"PatrolScrub"
};


/** Command syntax definition **/
struct Command SetDimmCommand =
{
  SET_VERB,                                                         //!< verb
  {                                                                   //!< options
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, FALSE, ValueRequired}
  },
  {{DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}},      //!< targets
  {
    {LOCKSTATE_PROPERTY, L"", LOCKSTATE_VALUE_DISABLED L"|" LOCKSTATE_VALUE_UNLOCKED L"|" LOCKSTATE_VALUE_FROZEN, FALSE, ValueRequired},
    {PASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional},
    {NEWPASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional},
    {CONFIRMPASSPHRASE_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueOptional},
    {FW_LOGLEVEL_PROPERTY, L"", HELP_TEXT_STRING, FALSE, ValueRequired},
    {FIRST_FAST_REFRESH_PROPERTY, L"", PROPERTY_VALUE_0 L"|" PROPERTY_VALUE_1, FALSE, ValueRequired},
    {VIRAL_POLICY_PROPERTY, L"", PROPERTY_VALUE_0 L"|" PROPERTY_VALUE_1, FALSE, ValueRequired},
    {CLEAR_ERROR_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired},
    {TEMPERATURE_INJ_PROPERTY, L"", HELP_TEXT_VALUE, FALSE, ValueRequired },
    {POISON_INJ_PROPERTY, L"", HELP_TEXT_VALUE, FALSE, ValueRequired },
    {POISON_TYPE_INJ_PROPERTY, L"", HELP_TEXT_VALUE, FALSE, ValueRequired},
    {PACKAGE_SPARING_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired },
    {PERCENTAGE_REAMAINING_INJ_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {FATAL_MEDIA_ERROR_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired},
    {DIRTY_SHUTDOWN_ERROR_INJ_PROPERTY, L"", PROPERTY_VALUE_1, FALSE, ValueRequired}
  },                                                                //!< properties
  L"Set properties of one or more DIMMs.",                          //!< help
  SetDimm
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pLockStatePropertyValue = NULL;
  CHAR16 *pPassphrase = NULL;
  CHAR16 *pNewPassphrase = NULL;
  CHAR16 *pConfirmPassphrase = NULL;
  CHAR16 *pPassphraseStatic = NULL;
  CHAR16 *pNewPassphraseStatic = NULL;
  CHAR16 *pConfirmPassphraseStatic = NULL;
  CHAR16 *pFirstFastRefreshValue = NULL;
  CHAR16 *pViralPolicyValue = NULL;
  CHAR16 *pFwLogLevel = NULL;
  CHAR16 *pTargetValue = NULL;
  CHAR16 *pLoadUserPath = NULL;
  CHAR16 *pLoadFilePath = NULL;
  CHAR16 *pErrorMessage = NULL;
  UINT16 SecurityOperation = SECURITY_OPERATION_UNDEFINED;
  UINT8 FirstFastRefreshState = OPTIONAL_DATA_UNDEFINED;
  UINT8 ViralPolicyState = OPTIONAL_DATA_UNDEFINED;
  UINT8 FwLogLevel = FW_LOG_LEVELS_INVALID_LEVEL; // unknown log level
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
  UINT64 TemperatureInteger;
  UINT64 PoisonAddress;
  UINT64 PercentageRemaining;
  UINT8  PoisonType = POISON_MEMORY_TYPE_PATROLSCRUB;
  UINT8 ErrorInjectionTypeSet = 0;
  UINT8 PoisonTypeValid = 0;
  UINT8 ClearStatus = 0;
  UINT8 FatalMediaError;
  UINT8 PackageSparing;
  UINT8 DirtyShutDown;
  NVDIMM_ENTRY();

  SetDisplayInfo(L"SetDimm", ResultsView);

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto FinishError;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto FinishError;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
	  goto FinishError;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto FinishError;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto FinishError;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
    }
  }

  /** If no dimm IDs are specified get IDs from all dimms **/
  if (DimmIdsCount == 0) {
      ReturnCode = GetManageableDimmsNumberAndId(&DimmIdsCount, &pDimmIds);
      if (EFI_ERROR(ReturnCode)) {
        Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
        goto FinishError;
      }
      if (DimmIdsCount == 0) {
          Print(FORMAT_STR_NL, CLI_INFO_NO_MANAGEABLE_DIMMS);
          ReturnCode = EFI_NOT_FOUND;
          goto FinishError;
      }
  }

  /** Check force option **/
  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
      Force = TRUE;
  }

  /**
      This command allows for different property sets depending on what action is to be taken.
      Here we check if input contains properties from different actions because they are not
      allowed together.
  **/
  if (!EFI_ERROR(ContainsProperty(pCmd, FIRST_FAST_REFRESH_PROPERTY)) ||
      !EFI_ERROR(ContainsProperty(pCmd, VIRAL_POLICY_PROPERTY))) {
      /** Found specified action **/
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
    if (!EFI_ERROR(ContainsProperty(pCmd, FW_LOGLEVEL_PROPERTY))) {
        if (ActionSpecified) {
            /** We already found a specified action, more are not allowed **/
            ReturnCode = EFI_INVALID_PARAMETER;
        }   else {
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
          Print(FORMAT_STR_NL, CLI_ERROR_POISON_TYPE_WITHOUT_ADDRESS);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto FinishError;
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
    /*Clear error injection requires exacly one  error injection type being set*/
    if (!EFI_ERROR(ContainsProperty(pCmd, CLEAR_ERROR_INJ_PROPERTY))) {
        if ((ActionSpecified && !ErrorInjectionTypeSet) || !ActionSpecified) {
          Print(FORMAT_STR_NL, CLI_ERROR_CLEAR_PROPERTY_NOT_COMBINED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto FinishError;
        }
    }

  /** Syntax error - mixed properties from different set -dimm commands **/
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }
  /** Syntax error - no properties specified. **/
  if (!ActionSpecified) {
    Print(FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  /**
    Set Security State
  **/
  GetPropertyValue(pCmd, LOCKSTATE_PROPERTY, &pLockStatePropertyValue);
  GetPropertyValue(pCmd, PASSPHRASE_PROPERTY, &pPassphraseStatic);
  GetPropertyValue(pCmd, NEWPASSPHRASE_PROPERTY, &pNewPassphraseStatic);
  GetPropertyValue(pCmd, CONFIRMPASSPHRASE_PROPERTY, &pConfirmPassphraseStatic);

  pLoadFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pLoadFilePath));
  if (pLoadFilePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
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
        pConfirmPassphraseStatic != NULL) {
      SecurityOperation = SECURITY_OPERATION_SET_PASSPHRASE;
      pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Set passphrase");
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L" on");

    } else if (pLockStatePropertyValue == NULL && pPassphraseStatic != NULL && pNewPassphraseStatic != NULL &&
        pConfirmPassphraseStatic != NULL) {
      SecurityOperation = SECURITY_OPERATION_CHANGE_PASSPHRASE;
      pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Change passphrase");
      pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L" on");

    } else if (pLockStatePropertyValue != NULL && pNewPassphraseStatic != NULL &&
          ((StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_DISABLED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_UNLOCKED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_FROZEN) == 0))) {
      pErrorMessage = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, L"NewPassphrase=");
      Print(FORMAT_STR_NL, pErrorMessage);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;

    } else if (pLockStatePropertyValue != NULL && pConfirmPassphraseStatic != NULL &&
          ((StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_DISABLED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_UNLOCKED) == 0) ||
           (StrICmp(pLockStatePropertyValue, LOCKSTATE_VALUE_FROZEN) == 0))) {
      pErrorMessage = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, L"ConfirmPassphrase=");
      Print(FORMAT_STR_NL, pErrorMessage);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;

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
      Print(FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
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
        Print(FORMAT_STR_NL, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto FinishError;
      }

      pLoadUserPath = getOptionValue(pCmd, SOURCE_OPTION);
      if (pLoadUserPath == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        NVDIMM_ERR("Could not get -source value. Out of memory");
        Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
        goto Finish;
      }

      ReturnCode = GetDeviceAndFilePath(pLoadUserPath, pLoadFilePath, &pDevicePathProtocol);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to get file path (%r)", ReturnCode);
        goto Finish;
      }

      ReturnCode = ParseSourcePassFile(pLoadFilePath, pDevicePathProtocol, &pPassphrase, &pNewPassphrase);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("ParseSourcePassFile failed (%r)", ReturnCode);
        goto FinishError;
      }
      // Check if required passwords have been found in the file
      if ((pPassphrase == NULL && pPassphraseStatic != NULL) ||
          (pNewPassphrase == NULL && pNewPassphraseStatic != NULL)) {
        Print(FORMAT_STR_NL, CLI_ERR_WRONG_FILE_DATA);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      // NewPassphrase and ConfirmPassphrase occur together
      } else if (pNewPassphrase != NULL) {
        pConfirmPassphrase = CatSPrint(NULL, FORMAT_STR, pNewPassphrase);
      }
    // Check prompts
    } else {
      if (OneOfPassphrasesIsEmpty && OneOfPassphrasesIsNotEmpty) {
        Print(FORMAT_STR_NL, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto FinishError;
      }
      // Check prompt request Passphrase
      if (pPassphraseStatic != NULL && StrCmp(pPassphraseStatic, L"") == 0) {
        ReturnCode = PromptedInput(L"Enter passphrase:\n", FALSE, FALSE, &pPassphrase);
        if (EFI_ERROR(ReturnCode)) {
          Print(FORMAT_STR_NL, CLI_ERR_PROMPT_INVALID);
          NVDIMM_DBG("Failed on PromptedInput");
          ReturnCode = EFI_INVALID_PARAMETER;
          goto FinishError;
        }
      } else if (pPassphraseStatic != NULL) {
        pPassphrase = CatSPrint(NULL, FORMAT_STR, pPassphraseStatic);
      }

      // Check prompt request NewPassphrase
      if (pNewPassphraseStatic != NULL && StrCmp(pNewPassphraseStatic, L"") == 0) {
        ReturnCode = PromptedInput(L"Enter new passphrase:", FALSE, FALSE, &pNewPassphrase);
        if (EFI_ERROR(ReturnCode)) {
          Print(FORMAT_STR_NL, CLI_ERR_PROMPT_INVALID);
          NVDIMM_DBG("Failed on PromptedInput");
          ReturnCode = EFI_INVALID_PARAMETER;
          goto FinishError;
        }
      } else if (pNewPassphraseStatic != NULL) {
        pNewPassphrase = CatSPrint(NULL, FORMAT_STR, pNewPassphraseStatic);
      }

      // Check prompt request ConfirmPassphrase
      if (pConfirmPassphraseStatic != NULL && StrCmp(pConfirmPassphraseStatic, L"") == 0) {
        ReturnCode = PromptedInput(L"Confirm passphrase:", FALSE, FALSE, &pConfirmPassphrase);
        if (EFI_ERROR(ReturnCode)) {
          Print(FORMAT_STR_NL, CLI_ERR_PROMPT_INVALID);
          NVDIMM_DBG("Failed on PromptedInput");
          goto FinishError;
        }
      } else if (pConfirmPassphraseStatic != NULL) {
        pConfirmPassphrase = CatSPrint(NULL, FORMAT_STR, pConfirmPassphraseStatic);
      }
    }

    if (pLockStatePropertyValue == NULL && pNewPassphrase != NULL && pConfirmPassphrase != NULL) {
      if ((StrCmp(pNewPassphrase, pConfirmPassphrase) != 0)) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASES_DO_NOT_MATCH);
        goto Finish;
      }
    }

    ReturnCode = pNvmDimmConfigProtocol->SetSecurityState(pNvmDimmConfigProtocol,
        pDimmIds, DimmIdsCount,
        SecurityOperation,
        pPassphrase, pNewPassphrase, pCommandStatus);

    goto Finish;
  }

  /**
    Set FirstFastRefresh and/or ViralPolicy
  **/
  GetPropertyValue(pCmd, FIRST_FAST_REFRESH_PROPERTY, &pFirstFastRefreshValue);
  GetPropertyValue(pCmd, VIRAL_POLICY_PROPERTY, &pViralPolicyValue);

  // Call the driver protocol function if either of the property is requested to be set
  if ((pFirstFastRefreshValue != NULL) || (pViralPolicyValue != NULL))
  {
    pCommandStatusMessage = CatSPrint(NULL, L"Modify DIMM");
    if (pFirstFastRefreshValue != NULL) {
      if (StrCmp(pFirstFastRefreshValue, PROPERTY_VALUE_0) == 0) {
        FirstFastRefreshState = FIRST_FAST_REFRESH_DISABLED;
      } else if (StrCmp(pFirstFastRefreshValue, PROPERTY_VALUE_1) == 0) {
        FirstFastRefreshState = FIRST_FAST_REFRESH_ENABLED;
      } else {
        Print(FORMAT_STR L": Error (%d) - " FORMAT_STR_NL, pCommandStatusMessage, EFI_INVALID_PARAMETER,
               CLI_ERR_INCORRECT_VALUE_PROPERTY_FIRST_FAST_REFRESH);
        goto FinishError;
      }
    }
    if (pViralPolicyValue != NULL) {
      if (StrCmp(pViralPolicyValue, PROPERTY_VALUE_0) == 0) {
        ViralPolicyState = VIRAL_POLICY_DISABLED;
      } else if (StrCmp(pViralPolicyValue, PROPERTY_VALUE_1) == 0) {
        ViralPolicyState = VIRAL_POLICY_ENABLED;
      } else {
        Print(FORMAT_STR L": Error (%d) - " FORMAT_STR_NL, pCommandStatusMessage, EFI_INVALID_PARAMETER,
               CLI_ERR_INCORRECT_VALUE_PROPERTY_VIRAL_POLICY);
        goto FinishError;
      }
    }

    pCommandStatusMessage = CatSPrint(NULL, FORMAT_STR, L"Modify");
    pCommandStatusPreposition = CatSPrint(NULL, FORMAT_STR, L"");

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
        Print(L"Modifying device settings on DIMM (" FORMAT_STR L").", DimmStr);
        ReturnCode = PromptYesNo(&Confirmation);
        if (!EFI_ERROR(ReturnCode) && Confirmation) {
          ReturnCode = pNvmDimmConfigProtocol->SetOptionalConfigurationDataPolicy(pNvmDimmConfigProtocol,
              &pDimmIds[Index], 1, FirstFastRefreshState, ViralPolicyState, pCommandStatus);
          if (EFI_ERROR(ReturnCode)) {
            goto Finish;
          }
        } else {
          Print(L"Skipping modify device settings on DIMM (" FORMAT_STR L")\n", DimmStr);
          continue;
        }
      }
    } else {
      ReturnCode = pNvmDimmConfigProtocol->SetOptionalConfigurationDataPolicy(pNvmDimmConfigProtocol,
          pDimmIds, DimmIdsCount, FirstFastRefreshState, ViralPolicyState, pCommandStatus);
      goto Finish;
    }
  }

  /**
    Set FwLevel
  **/
  GetPropertyValue(pCmd, FW_LOGLEVEL_PROPERTY, &pFwLogLevel);

  if (pFwLogLevel != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_SET_FW_LOG_LEVEL);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);

    /**
      Trying to match FWLevel, if fail INVALID_LEVEL will be passed to drv.
    **/
    for (Index = 0; Index < FW_LOG_LEVELS_COUNT; ++Index) {
      if (0 == StrCmp(pFwLogLevel, pFwDebugLogLevelStr[Index])) {
        FwLogLevel = (UINT8)Index + 1; // adding 1 to indicate the exact fw log level (not the loop index)
      }
    }
    ReturnCode = pNvmDimmConfigProtocol->SetFwLogLevel(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, FwLogLevel, pCommandStatus);
    goto Finish;
  }

  GetPropertyValue(pCmd, TEMPERATURE_INJ_PROPERTY, &pTemperature);
  GetPropertyValue(pCmd, POISON_INJ_PROPERTY, &pPoisonAddress);
  GetPropertyValue(pCmd, POISON_TYPE_INJ_PROPERTY, &pPoisonType);
  GetPropertyValue(pCmd, PACKAGE_SPARING_INJ_PROPERTY, &pPackageSparing);
  GetPropertyValue(pCmd, PERCENTAGE_REAMAINING_INJ_PROPERTY, &pPercentageRemaining);
  GetPropertyValue(pCmd, FATAL_MEDIA_ERROR_INJ_PROPERTY, &pFatalMediaError);
  GetPropertyValue(pCmd, DIRTY_SHUTDOWN_ERROR_INJ_PROPERTY, &pDirtyShutDown);
  GetPropertyValue(pCmd, CLEAR_ERROR_INJ_PROPERTY, &pClearErrorInj);

  /**
  Inject error Temperature
  **/
  if (pTemperature != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_TEMPERATURE_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ErrInjectType = ERROR_INJ_TEMPERATURE;
    ReturnCode = GetU64FromString(pTemperature, &TemperatureInteger);
    if (!ReturnCode) {
      Print(FORMAT_STR_NL, CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
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
        Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'Poison'\n", CLI_SYNTAX_ERROR,
          pPoisonAddress, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto FinishError;
      }
    ReturnCode = GetU64FromString(pPoisonAddress, &PoisonAddress);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'Poison'\n", CLI_SYNTAX_ERROR,
        pPoisonAddress, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
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
          if (0 == StrCmp(pPoisonType, pPoisonMemoryTypeStr[Index])) {
              PoisonTypeValid = 1;
              PoisonType = (UINT8)Index + 1;
          }
      }
      if (!PoisonTypeValid) {
        Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_POISON_TYPE);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto FinishError;
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
        Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'PackageSparing'\n", CLI_SYNTAX_ERROR,
          pPackageSparing, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto FinishError;
      }
      ErrInjectType = ERROR_INJ_PACKAGE_SPARING;
  }
  /**
   Percentage remaining property
  **/
  if (pPercentageRemaining != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_PERCENTAGE_REMAINING_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pPercentageRemaining, &PercentageRemaining);

    if (!ReturnCode || PercentageRemaining > 100) {
      Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'PercentageRemaining'\n", CLI_SYNTAX_ERROR,
        pPercentageRemaining, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
    }
    ErrInjectType = ERROR_INJ_PERCENTAGE_REMAINING;
  }

  /**
   Fatal Media Error Inj property
  **/
  if (pFatalMediaError != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_FATAL_MEDIA_ERROR_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pFatalMediaError, (UINT64 *)&FatalMediaError);
    if (!ReturnCode || 1 != FatalMediaError) {
      Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'FatalMediaError'\n", CLI_SYNTAX_ERROR,
        pFatalMediaError, CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
    }
    ErrInjectType = ERROR_INJ_FATAL_MEDIA_ERR;
  }

  /**
   Dirty shutdown error injection
  **/
  if (pDirtyShutDown != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, CLI_INFO_DIRTY_SHUT_DOWN_INJECT_ERROR);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pDirtyShutDown, (UINT64 *)&DirtyShutDown);

    if (!ReturnCode ||  1 != DirtyShutDown) {
      Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'DirtyShutDown'\n", CLI_SYNTAX_ERROR, pDirtyShutDown,
        CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
    }
    ErrInjectType = ERROR_INJ_DIRTY_SHUTDOWN;
  }
  /**
    Clear error injection
  **/
  if (pClearErrorInj != NULL) {
    pCommandStatusMessage = CatSPrint(NULL, GetCorrectClearMessageBasedOnProperty(ErrInjectType), pPoisonAddress);
    pCommandStatusPreposition = CatSPrint(NULL, CLI_INFO_ON);
    ReturnCode = GetU64FromString(pClearErrorInj, (UINT64 *)&ClearStatus);

    if (!ReturnCode ||  1 != ClearStatus) {
      Print(FORMAT_STR FORMAT_STR_SINGLE_QUOTE FORMAT_STR L" 'Clear'\n", CLI_SYNTAX_ERROR, ClearStatus,
        CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishError;
    }
  }
  if (ErrInjectType != ERROR_INJ_TYPE_INVALID) {
    ReturnCode = pNvmDimmConfigProtocol->InjectError(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount,
    (UINT8)ErrInjectType, ClearStatus, &TemperatureInteger,
    &PoisonAddress, &PoisonType, (UINT8 *)&PercentageRemaining, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }
Finish:
  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  DisplayCommandStatus(pCommandStatusMessage, pCommandStatusPreposition, pCommandStatus);
FinishError:
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
