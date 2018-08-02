/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "CommandParser.h"
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Utility.h>
#include "Common.h"

DispInfo gDisplayInfo;
extern int g_basic_commands;

/* local fns */
EFI_STATUS findVerb(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand);
EFI_STATUS findOptions(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand);
EFI_STATUS findTargets(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand);
EFI_STATUS findProperties(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand);
EFI_STATUS MatchCommand(struct Command *pInput, struct Command *pMatch);
EFI_STATUS MatchOptions(struct Command *pInput, struct Command *pMatch);
EFI_STATUS MatchTargets(struct Command *pInputCmd, struct Command *pCmdToMatch);
EFI_STATUS MatchProperties(struct Command *pInput, struct Command *pMatch);

/*
 * Global variables
 */
static UINTN gCommandCount = 0;
static struct Command *gCommandList = NULL;
static CHAR16 *gSyntaxError = NULL;
static UINTN gPossibleMatchCount = 0;
static CHAR16 *gDetailedSyntaxError = NULL;

/*
 * Add the specified command to the list of supported commands
 */
EFI_STATUS RegisterCommand(struct Command *pCommand)
{
  EFI_STATUS Rc = EFI_SUCCESS;

  /* make sure a verb is specified */
  if (!pCommand || !pCommand->verb || StrLen(pCommand->verb) == 0) {
    NVDIMM_WARN("Failed to register the command because it's invalid");
    Rc = EFI_ABORTED;
  } else {
    /* allocate memory */
    if (gCommandCount == 0) {
      gCommandList = AllocatePool(sizeof(struct Command));
    } else {
      gCommandList = ReallocatePool(sizeof(struct Command) * gCommandCount,
          sizeof(struct Command) * (gCommandCount + 1), gCommandList);
    }
    if (gCommandList) {
      pCommand->CommandId = (UINT8)gCommandCount; // Save its index for better tracking.
      CopyMem_S(&gCommandList[gCommandCount], sizeof(struct Command), pCommand, sizeof(struct Command));
      gCommandCount++;
    } else {
      NVDIMM_WARN("Failed to register the command due to lack of resources");
      Rc = EFI_OUT_OF_RESOURCES;
    }
  }

  NVDIMM_EXIT_CHECK_I64(Rc);
  return Rc;
}

/**
  Free the allocated memory for target values
  in  the CLI command structure.

  @param[in out] pCommand pointer to the command structure
**/
VOID
FreeCommandStructure(
  IN OUT COMMAND *pCommand
  )
{
  UINT32 Index = 0;

  if (pCommand != NULL) {
    for (Index = 0; Index < MAX_TARGETS; Index++) {
      FREE_POOL_SAFE(pCommand->targets[Index].pTargetValueStr);
    }
  }
}

/**
   The function parse the input string and split it to the tokens

   The caller function is responsible for deallocation of pCmdInput. The FreeCommandInput function should be
   used to deallocate memory.

   @param[in]  pCommand  The input string
   @param[out] pCmdInput
**/
VOID
FillCommandInput(
  IN     CHAR16 *pCommand,
     OUT struct CommandInput *pCmdInput
  )
{
  if (pCommand == NULL || pCmdInput == NULL) {
    return;
  }

  pCmdInput->ppTokens = StrSplit(pCommand, L' ', &pCmdInput->TokenCount);
}


/**
 If parsing fails, retrieve a more useful syntax error
**/
CHAR16 *getSyntaxError()
{
  return gSyntaxError;
}

/**
 If parsing fails, set syntax error, but first free old one
**/
VOID SetSyntaxError(
  IN      CHAR16 *pSyntaxError
  )
{
  FREE_POOL_SAFE(gSyntaxError);
  gSyntaxError = pSyntaxError;
}

/**
 If parsing fails, set syntax error, but first free old one
**/
VOID SetDetailedSyntaxError(
  IN      CHAR16 *pDetailedSyntaxError
  )
{
  FREE_POOL_SAFE(gDetailedSyntaxError);
  gDetailedSyntaxError = pDetailedSyntaxError;
}



/*
 * Clean up the resources associated with the command list
 */
void FreeCommands()
{
  NVDIMM_ENTRY();
  gCommandCount = 0;
  FREE_POOL_SAFE(gCommandList);
  FREE_POOL_SAFE(gSyntaxError);
  FREE_POOL_SAFE(gDetailedSyntaxError);

  NVDIMM_EXIT();
}

/*
 * Clean up the resources associated with the input
 */
void FreeCommandInput(struct CommandInput *pCommandInput)
{
  NVDIMM_ENTRY();

  if (pCommandInput == NULL) {
    return;
  }

  if (pCommandInput->ppTokens == NULL) {
    pCommandInput->TokenCount = 0;
    return;
  }

  FreeStringArray(pCommandInput->ppTokens, pCommandInput->TokenCount);

  pCommandInput->ppTokens = NULL;
  pCommandInput->TokenCount = 0;

  NVDIMM_EXIT();
}

/*
* Ensure cmdline args don't include '%'
*/
EFI_STATUS
InvalidTokenScreen(
  IN struct CommandInput *pInput,
  IN CHAR16 *pHelpStr
)
{
  UINTN Index = 0;
  CHAR16 *pTmpString = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  if (NULL == pInput || NULL == pHelpStr) {
    return ReturnCode;
  }

  for (Index = 0; Index < pInput->TokenCount; Index++) {
    if (NULL != StrStr(pInput->ppTokens[Index], L"%")) {
      pTmpString = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, L"%%");
      SetSyntaxError(CatSPrintClean(pTmpString, FORMAT_NL_STR FORMAT_NL_STR,
        CLI_PARSER_DID_YOU_MEAN, pHelpStr));
      return ReturnCode;
    }
  }

  return EFI_SUCCESS;
}

/*
 * Parse the given the command line arguments to
 * identify the correct command.
 *
 * Parsing is a two step process to first identify the tokens of the input
 * and then try to match it against the list of supported commands.
 *
 * It's the responsibility of the caller function to free the allocated
 * memory for target values in the Command structure.
 */
EFI_STATUS
Parse(
  IN     struct CommandInput *pInput,
  IN OUT struct Command *pCommand
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINTN Start = 0;
  UINTN Index = 0;
  CHAR16 *pHelpStr = NULL;

  NVDIMM_ENTRY();

  FREE_POOL_SAFE(gSyntaxError);
  FREE_POOL_SAFE(gDetailedSyntaxError);

  gPossibleMatchCount = 0;

  /* check input parameters */
  if (pCommand == NULL || pInput == NULL || pInput->ppTokens == NULL) {
    goto Finish;
  }

  if (pInput->TokenCount < 1) {
    NVDIMM_DBG("No input specified for Parse");
    goto Finish;
  }

  /* parse the input */
  Start = 0;
  ZeroMem(pCommand, sizeof(struct Command));
  for (Index = 0; Index < MAX_TARGETS; Index++) {
    pCommand->targets[Index].pTargetValueStr = AllocateZeroPool(TARGET_VALUE_LEN * sizeof(CHAR16));
    if (!pCommand->targets[Index].pTargetValueStr) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      break;
    }
  }

  ReturnCode = findVerb(&Start, pInput, pCommand);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pHelpStr = getCommandHelp(pCommand, FALSE);

  ReturnCode = InvalidTokenScreen(pInput, pHelpStr);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = findOptions(&Start, pInput, pCommand);

  /** Catch errors and send appropriate message **/
  if (EFI_ERROR(ReturnCode)) {
    switch (ReturnCode) {
    case EFI_BUFFER_TOO_SMALL: // Too long option value
      SetSyntaxError(CatSPrint(NULL, CLI_PARSER_ERR_INVALID_OPTION_VALUES, pHelpStr));
      break;
    }

    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = findTargets(&Start, pInput, pCommand);
  if (EFI_ERROR(ReturnCode)) {
    switch (ReturnCode) {
    case EFI_BUFFER_TOO_SMALL:
      SetSyntaxError(CatSPrint(NULL, CLI_PARSER_ERR_INVALID_TARGET_VALUES, pHelpStr));
    break;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = findProperties(&Start, pInput, pCommand);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /* try to match the parsed input against a registered command */
  for (Index = 0; Index < gCommandCount; Index++) {
    ReturnCode = MatchCommand(pCommand, &gCommandList[Index]);
    if (!EFI_ERROR(ReturnCode)) {
      pCommand->run = gCommandList[Index].run;
      pCommand->RunCleanup = gCommandList[Index].RunCleanup;
      pCommand->UpdateCmdCtx = gCommandList[Index].UpdateCmdCtx;
      break;
    }
  }

  /* try to give the user more useful help */
  if (EFI_ERROR(ReturnCode)) {
    if (pCommand->ShowHelp == TRUE) {
      /**
        If user used -help option, but provided command does not match any command syntax - display
        syntax of any command containing verb of entered command and return EFI_SUCCESS
      **/
      SetSyntaxError(CatSPrint(NULL, FORMAT_STR, getCommandHelp(pCommand, FALSE)));
      LongPrint(getSyntaxError());
      ReturnCode = EFI_SUCCESS;
    } else if (gPossibleMatchCount == 1 && gDetailedSyntaxError) {
      SetSyntaxError(CatSPrint(NULL, L"Syntax Error: " FORMAT_STR_NL L"Correct syntax: " FORMAT_STR, gDetailedSyntaxError, pHelpStr));
    } else {
      SetSyntaxError(CatSPrint(NULL, FORMAT_STR_NL FORMAT_STR_NL FORMAT_STR, CLI_PARSER_ERR_INVALID_COMMAND, CLI_PARSER_DID_YOU_MEAN, pHelpStr));
    }
  }

Finish:
  FREE_POOL_SAFE(pHelpStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/*
 * Identify the verb in the input
 */
EFI_STATUS findVerb(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand)
{
  EFI_STATUS rc = EFI_INVALID_PARAMETER;
  UINTN i = 0;

  NVDIMM_ENTRY();
  /* there has to be at least one verb */

  if (*pStart >= pInput->TokenCount) {
    return rc;
  }

  for (i = 0; i < gCommandCount; i++)
  {
    if (StrICmp(gCommandList[i].verb, pInput->ppTokens[*pStart]) == 0)
    {
      /* verb matches, so store it and move on */
      StrnCpyS(pCommand->verb, VERB_LEN, pInput->ppTokens[*pStart], VERB_LEN - 1);
      (*pStart)++;
      rc = EFI_SUCCESS;
      break;
    }
  }
  /* more detailed error */
  if (EFI_ERROR(rc))
  {
#ifdef OS_BUILD
    if (g_basic_commands) {
      // This should be updated when there are other comamnds a non-root user can run
      Print(L"A non-root user is restricted to run only version command\n");
    }
#endif
    SetSyntaxError(CatSPrint(NULL, CLI_PARSER_ERR_VERB_EXPECTED, pInput->ppTokens[*pStart]));
  }

  NVDIMM_EXIT_I64(rc);
  return rc;
}

/*
 * Identify the options in the input
 */
EFI_STATUS findOptions(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINTN Index = 0;
  UINTN Index2 = 0;
  UINTN Index3 = 0;
  UINTN matchedOptions = 0;
  BOOLEAN Found = FALSE;
  CHAR16 *pHelpStr = NULL;
  CHAR16 *pTmpString = NULL;

  NVDIMM_ENTRY();

  pHelpStr = getCommandHelp(pCommand, FALSE);

    /** loop through the input tokens **/
  while ((pInput->TokenCount - *pStart) > 0) {
    Found = FALSE;

    /** loop through the supported commands to find valid options **/
    for (Index = 0; Index < gCommandCount && !Found; Index++) {
      if (gCommandList[Index].options != NULL) {
        for (Index2 = 0; Index2 < MAX_OPTIONS && !Found; Index2++) {
          /** check both the long and short version of each option **/
          if ((StrICmp(pInput->ppTokens[*pStart], HELP_OPTION) == 0)
              || (StrICmp(pInput->ppTokens[*pStart], HELP_OPTION_SHORT) == 0)) {
            pCommand->ShowHelp = TRUE;
            Found = TRUE;
          } else if (StrICmp(gCommandList[Index].options[Index2].OptionNameShort,
              pInput->ppTokens[*pStart]) == 0) {
            // Check if option is copied already - to prevent duplicated option
            for (Index3 = 0; Index3 < matchedOptions; Index3++) {
              if (StrICmp(pCommand->options[Index3].OptionNameShort, pInput->ppTokens[*pStart]) == 0) {
                pTmpString = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, pInput->ppTokens[*pStart]);
                SetSyntaxError(CatSPrintClean(pTmpString, FORMAT_NL_STR FORMAT_NL_STR,
                    CLI_PARSER_DID_YOU_MEAN, pHelpStr));
                Rc = EFI_INVALID_PARAMETER;
                goto Finish;
              }
            }
            StrnCpyS(pCommand->options[matchedOptions].OptionNameShort, OPTION_LEN, pInput->ppTokens[*pStart], OPTION_LEN - 1);

            Found = TRUE;
          } else if (StrICmp(gCommandList[Index].options[Index2].OptionName,
              pInput->ppTokens[*pStart]) == 0) {
            // Check if option is copied already - to prevent duplicated option
            for (Index3 = 0; Index3 < matchedOptions; Index3++) {
              if (StrICmp(pCommand->options[Index3].OptionName, pInput->ppTokens[*pStart]) == 0) {
                pTmpString = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, pInput->ppTokens[*pStart]);
                SetSyntaxError(CatSPrintClean(pTmpString, FORMAT_NL_STR FORMAT_NL_STR,
                    CLI_PARSER_DID_YOU_MEAN, pHelpStr));
                Rc = EFI_INVALID_PARAMETER;
                goto Finish;
              }
            }
            StrnCpyS(pCommand->options[matchedOptions].OptionName, OPTION_LEN, pInput->ppTokens[*pStart], OPTION_LEN - 1);
            Found = TRUE;
          }
          /** if option is found, move to the next token **/
          if (Found) {
            (*pStart)++;
            /** check for an option value **/
            if ( ((pInput->TokenCount - *pStart) >= 1) && (pInput->ppTokens[*pStart][0] != '-') ) {
              if (StrLen(pInput->ppTokens[*pStart]) > OPTION_VALUE_LEN) {
                Rc = EFI_BUFFER_TOO_SMALL;
                break;
              } else {
                StrnCpyS(pCommand->options[matchedOptions].OptionValue, OPTION_VALUE_LEN, pInput->ppTokens[*pStart], OPTION_VALUE_LEN - 1);
                (*pStart)++;
              }
            }
            matchedOptions++;
          }
        }
      }
    }
    /** then this is not an option so move on **/
    if (!Found) {
      break;
    }
  }

Finish:
  FREE_POOL_SAFE(pHelpStr)
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/*
 * Identify the targets in the input
 */
EFI_STATUS findTargets(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINTN Index = 0;
  UINTN Index2 = 0;
  UINTN Index3 = 0;
  BOOLEAN Found = FALSE;
  UINTN matchedTargets = 0;
  CHAR16 *pHelpStr = NULL;
  CHAR16 *pTmpStr = NULL;

  NVDIMM_ENTRY();

  pHelpStr = getCommandHelp(pCommand, FALSE);

  /* loop through the input tokens */
  while ((pInput->TokenCount - *pStart) > 0)
  {
    Found = FALSE;
    /* check input against supported targets */
    for (Index = 0; Index < gCommandCount && !Found; Index++) {
      if (gCommandList[Index].targets != NULL) {
        for (Index2 = 0; Index2 < MAX_TARGETS && !Found; Index2++) {
          if (StrICmp(gCommandList[Index].targets[Index2].TargetName,
            pInput->ppTokens[*pStart]) == 0) {
            // Check if option is copied already - to prevent duplicated option
            for (Index3 = 0; Index3 < matchedTargets; Index3++) {
              if (StrICmp(pCommand->targets[Index3].TargetName, pInput->ppTokens[*pStart]) == 0) {
                pTmpStr = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, pInput->ppTokens[*pStart]);
                SetSyntaxError(CatSPrintClean(pTmpStr, FORMAT_NL_STR FORMAT_NL_STR,
                    CLI_PARSER_DID_YOU_MEAN, pHelpStr));
                Rc = EFI_INVALID_PARAMETER;
              }
            }
            StrnCpyS(pCommand->targets[matchedTargets].TargetName, TARGET_LEN, pInput->ppTokens[*pStart], TARGET_LEN - 1);
            (*pStart)++;
            Found = TRUE;

            /* check for a target value */
            if ( ((pInput->TokenCount - *pStart) >= 1) &&
                (pInput->ppTokens[*pStart][0] != '-') &&
                !ContainsCharacter('=', pInput->ppTokens[*pStart])) {
              if (StrLen(pInput->ppTokens[*pStart]) > TARGET_VALUE_LEN) {
                Rc = EFI_BUFFER_TOO_SMALL;
                break;
              } else {
                StrnCpyS(pCommand->targets[matchedTargets].pTargetValueStr, TARGET_VALUE_LEN, pInput->ppTokens[*pStart], TARGET_VALUE_LEN - 1);
                (*pStart)++;
              }
            }
            matchedTargets++;
          }
        }
      }
    }
    /* then this is not an target so move on */
    if (!Found) {
      break;
    }
  }

  FREE_POOL_SAFE(pHelpStr);
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/*
 * Identify the properties in the input
 */
EFI_STATUS findProperties(UINTN *pStart, struct CommandInput *pInput, struct Command *pCommand)
{
  EFI_STATUS Rc;
  CHAR16 *propertyName;
  CHAR16 *propertyValue;
  UINT16 propertyLength;
  UINTN matchedProperties;
  BOOLEAN Found;
  UINTN Index;
  UINTN Index2;
  CHAR16 *pHelpStr = NULL;
  CHAR16 *pTmpStr = NULL;

  NVDIMM_ENTRY();
  Rc = EFI_SUCCESS; /* no properties are required so default to success */
  matchedProperties = 0;
  pHelpStr = getCommandHelp(pCommand, FALSE);
  /* loop through the input tokens */
  while ((pInput->TokenCount - *pStart) > 0)
  {
    Rc = EFI_INVALID_PARAMETER;
    Found = FALSE;
    propertyName = NULL;
    propertyValue = NULL;

    /* properties follow the format key=value, so check for = */
    if (StrStr(pInput->ppTokens[*pStart], L"=") != NULL)
    {
      /* split the property into name and value */
      propertyLength = (UINT16)StrLen(pInput->ppTokens[*pStart]) + 1;
      propertyValue = AllocatePool(propertyLength * sizeof(CHAR16));
      if (!propertyValue)
      {
        Rc = EFI_OUT_OF_RESOURCES;
        break;
      }
      StrnCpyS(propertyValue, propertyLength, pInput->ppTokens[*pStart], propertyLength - 1);
      propertyName = StrTok(&propertyValue, L'=');
      /* name is valid */
      if (propertyName)
      {
        /*
         * loop through each command to see if the
         * property name matches any supported properties
         */
        for (Index = 0; Index < gCommandCount && !Found; Index++)
        {
          if (gCommandList[Index].properties != NULL)
          {
            for (Index2 = 0; Index2 < MAX_PROPERTIES && !Found; Index2++)
            {
              /* found a matching property */
              if (StrICmp(gCommandList[Index].properties[Index2].PropertyName,
                propertyName) == 0)
              {
                StrnCpyS(pCommand->properties[matchedProperties].PropertyName, PROPERTY_KEY_LEN, propertyName, PROPERTY_KEY_LEN - 1);
                /* value is valid */
                if (StrLen(propertyValue) > 0) {
                  StrnCpyS(pCommand->properties[matchedProperties].PropertyValue, PROPERTY_VALUE_LEN, propertyValue, PROPERTY_VALUE_LEN - 1);
                }
                Found = 1;
                (*pStart)++;
                matchedProperties++;
                Rc = EFI_SUCCESS;
                break; /* move to the next property */
              }
            }
          }
        }
        /* clean up */
        if (propertyName)
        {
          FreePool(propertyName);
        }
      } /* no property value */
      /* clean up */
      if (propertyValue)
      {
        FreePool(propertyValue);
      }
    } /* no property name */

    /* bad property or unexpected token */
    if (!Found) {
      pTmpStr = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, pInput->ppTokens[*pStart]);
      SetSyntaxError(CatSPrintClean(pTmpStr, FORMAT_NL_STR FORMAT_NL_STR, CLI_PARSER_DID_YOU_MEAN,
          pHelpStr));
      Rc = EFI_INVALID_PARAMETER;
      break;
    }
  }

  FREE_POOL_SAFE(pHelpStr);
  NVDIMM_EXIT_I(Rc);
  return Rc;
}

/*
 * Attempt to match the input to a command
 */
EFI_STATUS MatchCommand(struct Command *pInput, struct Command *pMatch)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS OptionsMatched = EFI_SUCCESS;
  EFI_STATUS TargetsMatched = EFI_SUCCESS;
  EFI_STATUS PropertiesMatched = EFI_SUCCESS;

  if (pInput == NULL || pMatch == NULL) {
    goto Finish;
  }

  /* match the verb */
  if (StrICmp(pMatch->verb, pInput->verb) == 0) {
    gPossibleMatchCount++;
    OptionsMatched = MatchOptions(pInput, pMatch);
    TargetsMatched = MatchTargets(pInput, pMatch);
    PropertiesMatched = MatchProperties(pInput, pMatch);

    /* try match the options, targets and properties */
    if ((OptionsMatched == EFI_SUCCESS) && (TargetsMatched == EFI_SUCCESS) && (PropertiesMatched == EFI_SUCCESS)) {
      /* found match! */
      ReturnCode = EFI_SUCCESS;
    }
  }

Finish:
  return ReturnCode;
}

/*
 * Attempt to match the input based on the options
 */
EFI_STATUS
MatchOptions(
  IN     struct Command *pInput,
  IN     struct Command *pMatch
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BOOLEAN MissingValue = FALSE;
  BOOLEAN RedundantValue = FALSE;
  UINTN MatchInputOptions = 0;
  UINTN MatchInputRequired = 0;
  UINTN MatchCommandOptions = 0;
  UINTN MatchCommandRequired = 0;
  UINTN MatchCount = 0;
  UINTN Index = 0;
  UINTN Index2 = 0;

  // Count options that need match in input command and matching command
  for (Index = 0; Index < MAX_OPTIONS; Index++) {
    if (StrLen(pInput->options[Index].OptionName) > 0 || StrLen(pInput->options[Index].OptionNameShort) > 0) {
      MatchInputOptions++;
    } else {
      break;
    }
  }
  for (Index = 0; Index < MAX_OPTIONS; Index++) {
    if (StrLen(pMatch->options[Index].OptionName) > 0 || StrLen(pMatch->options[Index].OptionNameShort) > 0) {
      MatchCommandOptions++;
      if (pMatch->options[Index].Required) {
        MatchCommandRequired++;
      }
    } else {
      break;
    }
  }

  if (MatchInputOptions >= MAX_OPTIONS || MatchCommandOptions >= MAX_OPTIONS) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Too many options have been provided.");
    goto Finish;
  }

  for (Index = 0; Index < MatchInputOptions; Index++) {
    MissingValue = FALSE;
    RedundantValue = FALSE;

    for (Index2 = 0; Index2 < MatchCommandOptions; Index2++) {
      // check if option name matches
      if (StrICmp(pMatch->options[Index2].OptionName, pInput->options[Index].OptionName) == 0 ||
          StrICmp(pMatch->options[Index2].OptionNameShort, pInput->options[Index].OptionNameShort) == 0) {
        // check if option is required
        if (pMatch->options[Index2].Required) {
          MatchInputRequired++;
        }
        // check if value is optional or required
        if (pMatch->options[Index2].ValueRequirement != ValueOptional) {
          if (pInput->options[Index].OptionValue && StrLen(pInput->options[Index].OptionValue) > 0) {
            if (pMatch->options[Index2].ValueRequirement == ValueRequired) {
              MatchCount++;
            } else {
              RedundantValue = TRUE;
            }
            break;
          } else {
            if (pMatch->options[Index2].ValueRequirement == ValueEmpty) {
              MatchCount++;
            } else {
              MissingValue = TRUE;
            }
            break;
          }
        } else {
          MatchCount++;
          break;
        }
      }
    }
    if (MatchCount <= Index) {
      // option specified with missing value
      if (MissingValue) {
        SetDetailedSyntaxError(
          CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_OPTION_VALUE_REQUIRED, pMatch->options[Index2].OptionName));
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      if (RedundantValue) {
        SetDetailedSyntaxError(
          CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_OPTION_VALUE_UNEXPECTED, pMatch->options[Index2].OptionName));
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      // missing required option
      if (pMatch->options[Index2].Required) {
        SetDetailedSyntaxError(
          CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_OPTION_REQUIRED, pMatch->options[Index2].OptionName));
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish; // stop looping
      }
      // if the user passed in an invalid option
      if (pInput->options[Index].OptionName &&
          StrLen(pInput->options[Index].OptionName) > 0 &&
          !containsOption(pMatch, pInput->options[Index].OptionName)) {
        SetSyntaxError(CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, pInput->options[Index].OptionName));
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish; // stop looping
      }
      // if the user passed in an invalid option abbreviation
      if (pInput->options[Index].OptionNameShort &&
          StrLen(pInput->options[Index].OptionNameShort) > 0 &&
          !containsOption(pMatch, pInput->options[Index].OptionNameShort)) {
        SetSyntaxError(CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, pInput->options[Index].OptionNameShort));
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish; // stop looping
      }
    }
  }

  if (MatchCount < MatchInputOptions || MatchInputRequired < MatchCommandRequired) {
    ReturnCode = EFI_NOT_FOUND;
  }

Finish:
  return ReturnCode;
}

/*
 * Attempt to match the input based on the targets
 */
EFI_STATUS MatchTargets(struct Command *pInputCmd, struct Command *pCmdToMatch)
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  BOOLEAN MissingTargetValue = FALSE;
  BOOLEAN RedundantTargetValue = FALSE;
  BOOLEAN MissingRequiredTarget = FALSE;
  BOOLEAN ExcessiveTarget = FALSE;
  BOOLEAN Matched = FALSE;
  UINT8 IndexInput = 0;
  UINT8 IndexMatch = 0;
  UINTN InputTargetNameLen = 0;
  UINTN InputTargetValueLen = 0;
  UINTN MatchTargetNameLen = 0;
  CHAR16 *InputTargetName = NULL;
  CHAR16 *MatchTargetName = NULL;
  UINT8 RequiredTargetsAsFlags = 0;
  UINT8 FoundTargetsAsFlags = 0;

  // First find all required targets to match
  for (IndexMatch = 0; IndexMatch < MAX_TARGETS; ++IndexMatch) {
    if (pCmdToMatch->targets[IndexMatch].Required) {
      RequiredTargetsAsFlags |= (1 << IndexMatch);
    }
  }

  // Iterate all targets from input cmd
  for (IndexInput = 0; IndexInput < MAX_TARGETS; ++IndexInput) {
    InputTargetName = pInputCmd->targets[IndexInput].TargetName;
    InputTargetNameLen = StrLen(InputTargetName);

    if (InputTargetNameLen == 0) {
      // All targets from Input was processed, quit from loop.
      break;
    }
    Matched = FALSE;
    // Iterate all targets from command to match
    for (IndexMatch = 0; IndexMatch < MAX_TARGETS; ++IndexMatch) {
      MatchTargetName = pCmdToMatch->targets[IndexMatch].TargetName;
      MatchTargetNameLen = StrLen(MatchTargetName);

      if (MatchTargetNameLen == 0) {
        // All targets from Match processed, quit from inner loop.
        break;
      }

      if (StrICmp(MatchTargetName, InputTargetName) == 0) {
        // Matching target found, turn on flag
        FoundTargetsAsFlags |= (1 << IndexMatch);
        Matched = TRUE;
      } else {
        continue;
      }
      if (pInputCmd->ShowHelp == TRUE) {
         continue;
      }
      // Get target value from user given cmd
      InputTargetValueLen = StrLen(pInputCmd->targets[IndexInput].pTargetValueStr);

      // Check target value if is missing or redundant:
         if ((pCmdToMatch->targets[IndexMatch].ValueRequirement == ValueRequired) && InputTargetValueLen == 0) {
            //If target value is required but empty print help for target
            MissingTargetValue = TRUE;
            SetDetailedSyntaxError(
              CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_TARGET_VALUE_REQUIRED, pInputCmd->targets[IndexInput].TargetName));
            goto Finish;
        } else if ((pCmdToMatch->targets[IndexMatch].ValueRequirement == ValueEmpty) && InputTargetValueLen != 0) {
            RedundantTargetValue = TRUE;
            SetDetailedSyntaxError(
              CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_TARGET_VALUE_UNEXPECTED, pInputCmd->targets[IndexInput].TargetName));
            goto Finish;
        }
    }
    if (Matched == FALSE){
      // User target not found, command not matched.
      ExcessiveTarget = TRUE;
      goto Finish;
    }
  }

  // Check if all required target has been found.
  MissingRequiredTarget = (RequiredTargetsAsFlags & FoundTargetsAsFlags) != RequiredTargetsAsFlags;

Finish:
  if (!MissingRequiredTarget && !ExcessiveTarget) {
    // All required targets matched, now check Value errors.
    if (MissingTargetValue || RedundantTargetValue) {
      ReturnCode = EFI_INVALID_PARAMETER;
    } else {
      ReturnCode = EFI_SUCCESS; // All ok, matched target
    }
  } else {
    //NVDIMM_WARN("Input don't match Command with ID=%d", pCmdToMatch->CommandId);
    ReturnCode = EFI_NOT_FOUND;
  }

  return ReturnCode;
}

/*
 * Attempt to match the input based on the properties
 */
EFI_STATUS MatchProperties(struct Command *pInput, struct Command *pMatch)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BOOLEAN Matched = TRUE;
  UINTN IndexInput = 0;
  UINTN IndexMatch = 0;
  BOOLEAN PropertyFound[MAX_PROPERTIES] = {FALSE};
  UINT16 InputPropertyValueLen = 0;

  for (IndexMatch = 0; IndexMatch < MAX_PROPERTIES && StrLen(pMatch->properties[IndexMatch].PropertyName) != 0; IndexMatch++) {
    Matched = FALSE;
    for (IndexInput = 0; IndexInput < MAX_PROPERTIES && StrLen(pInput->properties[IndexInput].PropertyName) != 0; IndexInput++) {
      if (StrICmp(pMatch->properties[IndexMatch].PropertyName, pInput->properties[IndexInput].PropertyName) == 0) {
        Matched = TRUE;

        /* Get property value from user given cmd */
        InputPropertyValueLen = (UINT16)StrLen(pInput->properties[IndexInput].PropertyValue);

        /* Check property value if is missing or redundant */
        if ((pMatch->properties[IndexMatch].ValueRequirement == ValueRequired) && InputPropertyValueLen == 0) {
          SetDetailedSyntaxError(
            CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_PROPERTY_VALUE_REQUIRED, pInput->properties[IndexInput].PropertyName));
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        } else if ((pMatch->properties[IndexMatch].ValueRequirement == ValueEmpty) && InputPropertyValueLen != 0) {
          SetDetailedSyntaxError(
            CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_PROPERTY_VALUE_UNEXPECTED, pInput->properties[IndexInput].PropertyName));
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }

        break;
      }
    }
    if (!Matched && pMatch->properties[IndexMatch].Required) {
      SetDetailedSyntaxError(
          CatSPrint(NULL, CLI_PARSER_DETAILED_ERR_PROPERTY_REQUIRED,  pMatch->properties[IndexMatch].PropertyName));
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  /* Looking for foreign and doubled properties */
  for (IndexInput = 0; IndexInput < MAX_PROPERTIES && StrLen(pInput->properties[IndexInput].PropertyName) != 0 ; IndexInput++) {
    Matched = FALSE;
    for (IndexMatch = 0; IndexMatch < MAX_PROPERTIES && StrLen(pMatch->properties[IndexMatch].PropertyName) != 0; IndexMatch++) {
      if ((StrICmp(pInput->properties[IndexInput].PropertyName, pMatch->properties[IndexMatch].PropertyName) == 0)
          && !PropertyFound[IndexMatch]) {
        Matched = TRUE;
        PropertyFound[IndexMatch] = TRUE;
        break;
      }
    }
    if (!Matched) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
Finish:
  return ReturnCode;
}

/**
  Get the help for a command read from the user.

  @param[in] pCommand a pointer to the parsed struct Command.
  @param[in] SingleCommand a BOOLEAN flag indicating if we are
    trying to match to a single command help or to all commands
    with the same verb.

  @retval NULL if the command verb could not be matched to any
    of the registered commands. Or the pointer to the help message.

  NOTE: If the return pointer is not NULL, the caller is responsible
  to free the memory using FreePool.
**/
CHAR16
*getCommandHelp(
  IN     struct Command *pCommand,
  BOOLEAN SingleCommand
  )
{
  UINTN Index = 0;
  UINTN Index2 = 0;
  EFI_STATUS CommandMatchingStatus = EFI_SUCCESS;
  CHAR16 *pHelp = NULL;

  NVDIMM_ENTRY();
  for(Index=0; Index < gCommandCount; Index++) {
    /**
      if the user wants help for all commands or for a specific command
      and it matches the verb, then continue to add the pHelp
    **/
    CommandMatchingStatus = MatchCommand(pCommand, &gCommandList[Index]);
    if ( !gCommandList[Index].Hidden &&
      ((SingleCommand && !EFI_ERROR(CommandMatchingStatus))
      || (!SingleCommand && (pCommand == NULL || (pCommand != NULL && StrICmp(pCommand->verb, gCommandList[Index].verb) == 0))))) {
      /** full verb syntax with help string **/
      if (pCommand == NULL || SingleCommand || (pCommand != NULL && pCommand->ShowHelp == TRUE)) {
        pHelp = CatSPrintClean(pHelp, FORMAT_STR_NL, gCommandList[Index].pHelp);
        pHelp = CatSPrintClean(pHelp, L"    " FORMAT_STR_SPACE, gCommandList[Index].verb);
      } else { /** syntax error help so just print syntax **/
        pHelp = CatSPrintClean(pHelp, FORMAT_STR_SPACE, gCommandList[Index].verb);
      }

      pHelp = CatSPrintClean(pHelp, L"[-help|-h] ");
      /* add the options pHelp */
      for (Index2 = 0; Index2 < MAX_OPTIONS; Index2++) {
        if (gCommandList[Index].options[Index2].OptionName &&
          StrLen(gCommandList[Index].options[Index2].OptionName) > 0) {
          if (!gCommandList[Index].options[Index2].Required) {
            pHelp = CatSPrintClean(pHelp, L"[");
          }
          if (gCommandList[Index].options[Index2].OptionNameShort &&
            StrLen(gCommandList[Index].options[Index2].OptionNameShort) > 0) {
            pHelp = CatSPrintClean(pHelp, FORMAT_STR L"|" FORMAT_STR,
                gCommandList[Index].options[Index2].OptionName,
                gCommandList[Index].options[Index2].OptionNameShort);
          } else {
            pHelp = CatSPrintClean(pHelp, FORMAT_STR,
                gCommandList[Index].options[Index2].OptionName);
          }
          if (StrLen(gCommandList[Index].options[Index2].pHelp) != 0) {
            pHelp = CatSPrintClean(pHelp, L" (" FORMAT_STR L")", gCommandList[Index].options[Index2].pHelp);
          }
          if (!gCommandList[Index].options[Index2].Required) {
            pHelp = CatSPrintClean(pHelp, L"]");
          }
          pHelp = CatSPrintClean(pHelp, L" ");
        }
      }

      /* add the targets pHelp */
      for (Index2 = 0; Index2 < MAX_TARGETS; Index2++)
      {
        if (gCommandList[Index].targets[Index2].TargetName &&
          StrLen(gCommandList[Index].targets[Index2].TargetName) > 0)
        {
          if (!gCommandList[Index].targets[Index2].Required)
          {
            pHelp = CatSPrintClean(pHelp, L"[");
          }
          pHelp = CatSPrintClean(pHelp, FORMAT_STR,
              gCommandList[Index].targets[Index2].TargetName);
          if (gCommandList[Index].targets[Index2].pHelp && StrLen(gCommandList[Index].targets[Index2].pHelp) > 0)
          {
            if (gCommandList[Index].targets[Index2].ValueRequirement == ValueOptional) {
              pHelp = CatSPrintClean(pHelp, L" [(" FORMAT_STR L")]",
                  gCommandList[Index].targets[Index2].pHelp);
            } else {
              pHelp = CatSPrintClean(pHelp, L" (" FORMAT_STR L")",
                  gCommandList[Index].targets[Index2].pHelp);
            }
          }
          if (!gCommandList[Index].targets[Index2].Required)
          {
            pHelp = CatSPrintClean(pHelp, L"]");
          }
          pHelp = CatSPrintClean(pHelp, L" ");
        }
      }

      /** add the properties pHelp **/
      for (Index2 = 0; Index2 < MAX_PROPERTIES; Index2++) {
        if (gCommandList[Index].properties[Index2].PropertyName &&
          StrLen(gCommandList[Index].properties[Index2].PropertyName) > 0) {
          if (!gCommandList[Index].properties[Index2].Required) {
            pHelp = CatSPrintClean(pHelp, L"[");
          }
          pHelp = CatSPrintClean(pHelp, FORMAT_STR L"=(" FORMAT_STR L")",
              gCommandList[Index].properties[Index2].PropertyName,
              gCommandList[Index].properties[Index2].pHelp);
          if (!gCommandList[Index].properties[Index2].Required) {
            pHelp = CatSPrintClean(pHelp, L"]");
          }
          pHelp = CatSPrintClean(pHelp, L" ");
        }
      }
      pHelp = CatSPrintClean(pHelp, L"\n");
    }
  }
  NVDIMM_EXIT();
  return pHelp;
}

/**
  Check if a specific property is found
    @param[in] pCmd is a pointer to the struct Command that contains the user input.
    @param[in] pProperty is a CHAR16 string that represents the property we want to find.

    @retval EFI_SUCCESS if we've found the property.
    @retval EFI_NOT_FOUND if no such property exists for the given pCmd.
    @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
ContainsProperty(
  IN     CONST struct Command *pCmd,
  IN     CONST CHAR16 *pProperty
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  INT32 Index;
  NVDIMM_ENTRY();

  if (pCmd == NULL || pProperty == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < MAX_PROPERTIES; Index++) {
    if (StrICmp(pCmd->properties[Index].PropertyName, pProperty) == 0) {
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }

  NVDIMM_EXIT();
  return ReturnCode;
}

/**
  Get a specific property value
    @param[in] pCmd is a pointer to the struct Command that .
    @param[in] pProperty is a CHAR16 string that represents the property we want to find.
    @param[out] ppReturnValue is a pointer to a pointer to the 16-bit character string
        that will contain the return property value.

    @retval EFI_SUCCESS if we've found the property and the value is set.
    @retval EFI_NOT_FOUND if no such property exists for the given pCmd.
    @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
GetPropertyValue(
  IN     CONST struct Command *pCmd,
  IN     CONST CHAR16 *pProperty,
     OUT CHAR16 **ppReturnValue
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  INT32 Index;
  NVDIMM_ENTRY();

  if (pCmd == NULL || pProperty == NULL || ppReturnValue == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ppReturnValue = NULL;
  for (Index = 0; Index < MAX_PROPERTIES; Index++) {
    if (StrICmp(pCmd->properties[Index].PropertyName, pProperty) == 0) {
      ReturnCode = EFI_SUCCESS;
      *ppReturnValue = (CHAR16*)pCmd->properties[Index].PropertyValue;
      break;
    }
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/*
 * Check if a specific option is found
 */
BOOLEAN containsOption(CONST struct Command *pCmd, CONST CHAR16 *option)
{
  BOOLEAN found = FALSE;
  INT32 i;
  NVDIMM_ENTRY();

  for (i = 0; i < MAX_OPTIONS; i++)
  {
    if (StrICmp(pCmd->options[i].OptionName, option) == 0 ||
      StrICmp(pCmd->options[i].OptionNameShort, option) == 0)
    {
      found = TRUE;
      break;
    }
  }

  NVDIMM_EXIT();
  return found;
}

/**
  Check if a specific target is found in the command

  @param[in] pCmd
  @param[in] pTarget

  @retval TRUE if the target has been found
  @retval FALSE if the target has not been found
**/
BOOLEAN
ContainTarget(
  IN CONST struct Command *pCmd,
  IN CONST CHAR16 *pTarget
  )
{
  BOOLEAN Found = FALSE;
  INT32 Index;
  NVDIMM_ENTRY();

  if (pCmd == NULL || pTarget == NULL) {
    return Found;
  }

  for (Index = 0; Index < MAX_TARGETS; Index++) {
    if (StrICmp(pCmd->targets[Index].TargetName, pTarget) == 0) {
      Found = TRUE;
      break;
    }
  }

  NVDIMM_EXIT();
  return Found;
}

/*
 * Get the value of a specific option
 */
CHAR16* getOptionValue(CONST struct Command *pCmd,
    CONST CHAR16 *option)
{
  INT32 i;
  CHAR16 *value = NULL;
  NVDIMM_ENTRY();

  for (i = 0; i < MAX_OPTIONS; i++)
  {
    if (StrICmp(pCmd->options[i].OptionName, option) == 0 ||
      StrICmp(pCmd->options[i].OptionNameShort, option) == 0)
    {
      value = CatSPrint(NULL, FORMAT_STR, pCmd->options[i].OptionValue);
      break;
    }
  }

  NVDIMM_EXIT();
  return value;
}

/**
  Get the value of a specific target

  @param[in] pCmd
  @param[in] pTarget

  @retval the target value if the target has been found
  @retval NULL otherwise
**/
CHAR16*
GetTargetValue(
  IN struct Command *pCmd,
  IN CONST CHAR16 *pTarget
  )
{
  INT32 Index;
  CHAR16 *pValue = NULL;
  NVDIMM_ENTRY();

  if (pCmd == NULL || pTarget == NULL) {
    return pValue;
  }

  for (Index = 0; Index < MAX_TARGETS; Index++) {
    if (StrICmp(pCmd->targets[Index].TargetName, pTarget) == 0) {
      pValue = pCmd->targets[Index].pTargetValueStr;
      break;
    }
  }

  NVDIMM_EXIT();
  return pValue;
}

/*
 * Determine if the specified value is in the specified comma
 * separated display list.
 */
BOOLEAN ContainsValue(CONST CHAR16 *displayList,
    CONST CHAR16 *value)
{
  CHAR16 *tmpList;
  CHAR16 *token;
  BOOLEAN found = FALSE;
  NVDIMM_ENTRY();

  /* copy the input to a tmp var to avoid changing it */
  tmpList = CatSPrint(NULL, FORMAT_STR, displayList);
  if (tmpList)
  {
    token = StrTok(&tmpList, L',');
    while (token && !found)
    {
      if (StrICmp(value, token) == 0)
      {
        found = TRUE;
      }
      FreePool(token);
      token = StrTok(&tmpList, L',');
    }
    if (token)
    {
      FreePool(token);
    }
    if (tmpList)
    {
      FreePool(tmpList);
    }
  }

  NVDIMM_EXIT();
  return found;
}

BOOLEAN
ContainsCharacter(
  IN     CHAR16 Character,
  IN     CONST CHAR16* pInputString
  )
{
  UINT32 Length = 0;
  UINT32 Index = 0;

  if (pInputString == NULL) {
    return FALSE;
  }

  Length = (UINT32)StrLen(pInputString);

  for (Index=0; Index<Length; Index++) {
    if (pInputString[Index] == Character) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Get the value of the units option

  @param[in] pCmd The input command structure
  @param[out] pUnitsToDisplay Units to display based on input units option

  @retval EFI_INVALID_PARAMETER if input parameter is NULL, else EFI_SUCCESS
**/
EFI_STATUS
GetUnitsOption(
  IN     CONST struct Command *pCmd,
     OUT UINT16 *pUnitsToDisplay
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pOptionsValue = NULL;

  NVDIMM_ENTRY();
  if (pCmd == NULL || pUnitsToDisplay == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pUnitsToDisplay = DISPLAY_SIZE_UNIT_UNKNOWN;

  /** If either of the units options are requested **/
  if (containsOption(pCmd, UNITS_OPTION) || containsOption(pCmd, UNITS_OPTION_SHORT)) {
    pOptionsValue = getOptionValue(pCmd, UNITS_OPTION);
    if (pOptionsValue == NULL) {
      pOptionsValue = getOptionValue(pCmd, UNITS_OPTION_SHORT);
    }

    if (pOptionsValue != NULL) {
      if (StrICmp(pOptionsValue, UNITS_OPTION_B) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_B;
      } else if (StrICmp(pOptionsValue, UNITS_OPTION_MB) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_MB;
      } else if (StrICmp(pOptionsValue, UNITS_OPTION_MIB) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_MIB;
      } else if (StrICmp(pOptionsValue, UNITS_OPTION_GB) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_GB;
      } else if (StrICmp(pOptionsValue, UNITS_OPTION_GIB) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_GIB;
      } else if (StrICmp(pOptionsValue, UNITS_OPTION_TB) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_TB;
      } else if (StrICmp(pOptionsValue, UNITS_OPTION_TIB) == 0) {
        *pUnitsToDisplay = DISPLAY_SIZE_UNIT_TIB;
      } else {
        ReturnCode = EFI_INVALID_PARAMETER;
        Print(FORMAT_STR, CLI_ERR_INCORRECT_VALUE_OPTION_UNITS);
        goto Finish;
      }
    } else {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  Finish:
  FREE_POOL_SAFE(pOptionsValue);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Sets a display information needed when outputting alternative formats like XML.
@param[in] pName is a CHAR16 string that represents the output message.
@param[in] Type represents the type of output being displayed.
@param[in] pDelims is a CHAR16 string that represents deliminters to use when parsing text output
@retval EFI_SUCCESS if the name was copied correctly.
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
SetDisplayInfo(
    IN     CONST CHAR16 *pName,
    IN     CONST UINT8 Type,
    IN     CONST CHAR16 *pDelims
)
{
    if (NULL == pName){
        return EFI_INVALID_PARAMETER;
    }
    UnicodeSPrint(gDisplayInfo.Name, sizeof(gDisplayInfo.Name), FORMAT_STR, pName);
    gDisplayInfo.Type = Type;
    if(pDelims) {
      UnicodeSPrint(gDisplayInfo.Delims, sizeof(gDisplayInfo.Delims), FORMAT_STR, pDelims);
    }
    else {
      UnicodeSPrint(gDisplayInfo.Delims, sizeof(gDisplayInfo.Delims), L"");
    }
    return EFI_SUCCESS;
}

/**
Get display information needed when outputting alternative formats like XML.
@param[out] pName is a CHAR16 string that represents the output message.
@param[int] NameSize is the size of pName in bytes
@param[out] pType represents the type of output being displayed.
@param[out] pDelims represents the deliminters to use when parsing text output.
@param[int] DelimsSize is the size of pDelims in bytes
@retval EFI_SUCCESS if the name was copied correctly.
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
GetDisplayInfo(
   OUT    CHAR16 *pName,
   IN     CONST UINT32 NameSize,
   OUT    UINT8 *pType,
   OUT    CHAR16 *pDelims,
   IN     CONST UINT32 DelimnsSize
)
{
   if (NULL == pName || NULL == pType || NULL == pDelims){
      return EFI_INVALID_PARAMETER;
   }
   UnicodeSPrint(pName, NameSize, FORMAT_STR, gDisplayInfo.Name);
   UnicodeSPrint(pDelims, DelimnsSize, FORMAT_STR, gDisplayInfo.Delims);
   *pType = gDisplayInfo.Type;
   return EFI_SUCCESS;
}

/**
Execute UpdateCmdCtx (if defined), run, and RunCleanup (if defined).
@param[in] pCommand pointer to the command structure
@retval EFI_SUCCESS if the name was copied correctly.
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
ExecuteCmd(COMMAND *pCommand) {

  EFI_STATUS Rc = EFI_SUCCESS;

  if (NULL == pCommand)
    return EFI_INVALID_PARAMETER;

  if (NULL == (pCommand->pShowCtx = (SHOW_CMD_CONTEXT*)AllocateZeroPool(sizeof(SHOW_CMD_CONTEXT)))) {
    return EFI_OUT_OF_RESOURCES;
  }

  if (EFI_SUCCESS != (Rc = ReadCmdLineShowOptions(&pCommand->pShowCtx->FormatType, pCommand))) {
    goto Finish;
  }

  if (pCommand->UpdateCmdCtx)
    Rc = pCommand->UpdateCmdCtx(pCommand);

  if (EFI_ERROR(Rc))
    goto Finish;

  if (NULL == pCommand->run) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Rc = pCommand->run(pCommand);

  if (EFI_ERROR(Rc))
    goto Finish;

  if (pCommand->RunCleanup)
    Rc = pCommand->RunCleanup(pCommand);

Finish:
  FREE_POOL_SAFE(pCommand->pShowCtx);
  return Rc;
}