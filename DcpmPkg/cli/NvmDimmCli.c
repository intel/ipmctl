/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NvmDimmCli.h"
#include <Uefi.h>
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/HiiLib.h>
#include <Protocol/DriverHealth.h>
#include <Protocol/ShellParameters.h>
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <Version.h>
#include <NvmInterface.h>
#include "CommandParser.h"
#include "ShowDimmsCommand.h"
#include "ShowSocketsCommand.h"
#include "SetDimmCommand.h"
#include "DeleteDimmCommand.h"
#include "ShowRegionsCommand.h"
#include "ShowAcpiCommand.h"
#include "ShowSmbiosCommand.h"
#include "LoadCommand.h"
#include "SetSensorCommand.h"
#include "ShowSensorCommand.h"
#include "CreateGoalCommand.h"
#include "ShowGoalCommand.h"
#include "DeleteGoalCommand.h"
#include "DumpGoalCommand.h"
#include "LoadGoalCommand.h"
#include "StartDiagnosticCommand.h"
#include "ShowNamespaceCommand.h"
#include "CreateNamespaceCommand.h"
#include "DeleteNamespaceCommand.h"
#include "ShowErrorCommand.h"
#include "ShowCelCommand.h"
#include "ShowTopologyCommand.h"
#include "DumpDebugCommand.h"
#include "ShowMemoryResourcesCommand.h"
#include "ShowSystemCapabilitiesCommand.h"
#include "ShowRegisterCommand.h"
#include "Common.h"
#include "ShowFirmwareCommand.h"
#include "ShowPcdCommand.h"
#include "StartFormatCommand.h"
#include "ShowPreferencesCommand.h"
#include "SetPreferencesCommand.h"
#include "ShowPerformanceCommand.h"
#include "ShowCmdAccessPolicyCommand.h"
#include "DeletePcdCommand.h"
#include <PbrDcpmm.h>
#include "StartSessionCommand.h"
#include "StopSessionCommand.h"
#include "DumpSessionCommand.h"
#include "ShowSessionCommand.h"
#include "LoadSessionCommand.h"
#ifdef OS_BUILD
#include <Protocol/Driver/DriverBinding.h>
#else
#include <Protocol/DriverBinding.h>
#endif

#if _BullseyeCoverage
#ifndef OS_BUILD
extern int cov_dumpData(void);
#endif // !OS_BUILD
#endif // _BullseyeCoverage

#ifdef __MFG__
#include <mfg/MfgCommands.h>
#endif
#ifdef OS_BUILD
#include "DumpSupportCommand.h"
#include <stdio.h>
extern void nvm_current_cmd(struct Command Command);
extern BOOLEAN ConfigIsDdrtProtocolDisabled();
extern BOOLEAN ConfigIsLargePayloadDisabled();
#else
#include "DeletePcdCommand.h"
EFI_GUID gNvmDimmConfigProtocolGuid = EFI_DCPMM_CONFIG2_PROTOCOL_GUID;
EFI_GUID gIntelDimmConfigVariableGuid = INTEL_DIMM_CONFIG_VARIABLE_GUID;
EFI_GUID gIntelDimmPbrTagIdVariableguid = INTEL_DIMM_PBR_TAGID_VARIABLE_GUID;
EFI_GUID gNvmDimmPbrProtocolGuid = EFI_DCPMM_PBR_PROTOCOL_GUID;
#endif
EFI_GUID gNvmDimmDriverHealthGuid = EFI_DRIVER_HEALTH_PROTOCOL_GUID;

extern EFI_SHELL_INTERFACE *mEfiShellInterface;

#ifdef OS_BUILD
EFI_HANDLE gNvmDimmCliHiiHandle = (EFI_HANDLE)0x1;
extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;
extern EFI_DRIVER_BINDING_PROTOCOL gNvmDimmDriverDriverBinding;
#else
EFI_HANDLE gNvmDimmCliHiiHandle = NULL;
#ifndef MDEPKG_NDEBUG
extern volatile   UINT32  _gPcd_BinaryPatch_PcdDebugPrintErrorLevel;
#endif //MDEPKG_NDEBUG
#endif

#define NVMDIMM_CLI_HII_GUID \
  { 0x26e4ac23, 0xd32f, 0x4788, {0x83, 0x95, 0xb0, 0x2a, 0x33, 0x0c, 0x28, 0x26}}

EFI_GUID gNvmDimmCliHiiGuid = NVMDIMM_CLI_HII_GUID;

/* Local fns */
static EFI_STATUS showVersion(struct Command *pCmd);
static EFI_STATUS GetPbrMode(UINT32 *Mode);
static EFI_STATUS SetPbrTag(CHAR16 *pName, CHAR16 *pDescription);
static EFI_STATUS ResetPbrSession(UINT32 TagId);
static EFI_STATUS SetDefaultProtocolAndPayloadSizeOptions();

/**
  Supported commands
  Display CLI application help
**/
struct Command HelpCommand =
{
  HELP_VERB,                                  //!< verb
  {
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    {OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }, //!< options
#endif // OS_BUILD
    {L"", L"", L"", L"",L"", FALSE, ValueOptional}
  }, //!< options
  {{L"", L"", L"", FALSE, ValueOptional}},      //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},      //!< properties
  L"Display the CLI help.",                   //!< help
  showHelp                                    //!< run function
};

/**
  Display the CLI application version
**/
struct Command VersionCommand =
{
  VERSION_VERB,                               //!< verb
  {
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}                         //!< options
#endif
  }, //!< options
  {{L"", L"", L"", FALSE, ValueOptional}},      //!< targets
  {{L"", L"", L"", FALSE}},                     //!< properties
  L"Display the CLI version.",                //!< help
  showVersion,                                 //!< run function
  TRUE
};

BOOLEAN HelpRequested = FALSE;
BOOLEAN FullHelpRequested = FALSE;

/**
Reviews the passed tokens for help|-h|-help flags and prepares the token
order for proper display
**/
VOID FixHelp(CHAR16** ppTokens, UINT32* pCount){
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 HelpIndex = 0;
  CHAR16 *pHelpTokenTmp = NULL;
  UINT32 HelpFlags = 0;
  BOOLEAN HelpTokenIndexWrong = FALSE;

  HelpRequested = FALSE;
  FullHelpRequested = FALSE;
  if (ppTokens == NULL || pCount  == NULL || *pCount <= 0) {
    return;
  }

  //look for simple requests for global help
  if (0 == StrICmp(ppTokens[0], HELP_VERB) ||
      0 == StrICmp(ppTokens[0], HELP_OPTION) ||
      0 == StrICmp(ppTokens[0], HELP_OPTION_SHORT)) {
    HelpRequested = TRUE;
    FullHelpRequested = TRUE;
    for (Index = 1; Index < *pCount; Index++) {
      FREE_POOL_SAFE(ppTokens[Index]);
    }
    *pCount = 1;
    return;
  }

  //Examine the post-verb to determine if/where the help token(s) are
  for (Index = 1; Index < *pCount; Index++) {
    if (0 == StrICmp(ppTokens[Index], HELP_VERB) ||
        0 == StrICmp(ppTokens[Index], HELP_OPTION) ||
        0 == StrICmp(ppTokens[Index], HELP_OPTION_SHORT)) {

      HelpFlags++;
      HelpRequested = TRUE;
      if (HelpFlags == 1) {
        //retain the help token, but change to the short version
        ppTokens[Index][0] = '-';
        ppTokens[Index][1] = 'h';
        ppTokens[Index][2] = 0;
        HelpIndex = Index;
        pHelpTokenTmp = ppTokens[Index];
        HelpTokenIndexWrong = Index > 1;
      }
      else {
        //dispose of duplicate help flags
        FREE_POOL_SAFE(ppTokens[Index]);
      }

      //create a 'hole' in the array
      ppTokens[Index] = NULL;
    }
  }

  //nothing to do
  if (HelpFlags == 0) return;

  if (TRUE == HelpTokenIndexWrong) {
    //make a spot at slot 1 for the help token
    for (Index = HelpIndex; Index > 1; Index--)
    {
      ppTokens[Index] = ppTokens[Index - 1];
    }
  }

  //move the token to the right spot
  ppTokens[1] = pHelpTokenTmp;

  //collapse any 'holes' in the array (caused by multiple help flags)
  for (Index = 1; Index < *pCount; Index++) {
    if (ppTokens[Index] == NULL) {
      for (Index2 = Index + 1; Index2 < *pCount; Index2++) {
        if (ppTokens[Index2] != NULL) {
          ppTokens[Index] = ppTokens[Index2];
          ppTokens[Index2] = NULL;
          break;
        }
      }
    }
  }

  //adjust the count to account for removed parameters
  for (Index = 1; Index < *pCount; Index++) {
    if (ppTokens[Index] == NULL) {
      break;
    }
  }

  //set the new count
  *pCount = Index;
}

/*                                          ./
 * The entry point for the application.
 *
 * @param[in] ImageHandle    The firmware allocated handle for the EFI image.
 * @param[in] pSystemTable    A pointer to the EFI System Table.
 *
 * @retval EFI_SUCCESS       The entry point is executed successfully.
 * @retval other             Some error occurs when executing this entry point.
 */
EFI_STATUS
EFIAPI
UefiMain(
  IN     EFI_HANDLE ImageHandle,
  IN     EFI_SYSTEM_TABLE *pSystemTable
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  struct CommandInput Input;
  struct Command Command;
  INT32 Index = 0;
  CHAR16 *pLine = NULL;
  BOOLEAN MoreInput = TRUE;
  BOOLEAN HelpShown = FALSE;
  UINTN Argc = 0;
  CHAR16 **ppArgv = NULL;
  UINT32 NextId = 0;
#ifndef OS_BUILD
  SHELL_FILE_HANDLE StdIn = NULL;
#endif

  NVDIMM_ENTRY();

  ZeroMem(&Input, sizeof(Input));
  ZeroMem(&Command, sizeof(Command));

#ifndef OS_BUILD
  InitErrorAndWarningNvmStatusCodes();

  /* only support EFI shell 2.0 */
  Rc = ShellInitialize();
  if (EFI_ERROR(Rc)) {
    Rc = EFI_UNSUPPORTED;
    NVDIMM_WARN("ShellInitialize failed, rc = 0x%llx", Rc);
    Print(L"Error: EFI Shell 2.0 is required to run this application\n");
    goto Finish;
  }
#endif


  if (gEfiShellParametersProtocol == NULL) {
    Rc = EFI_UNSUPPORTED;
#ifndef OS_BUILD
    NVDIMM_WARN("ShellInitialize succeeded but the shell interface and parameters protocols do not exist");
    Print(L"Error: EFI Shell 2.0 is required to run this application.\n");
#else
    NVDIMM_WARN("Shell interface and parameters protocols do not exist");
#endif
    goto Finish;
  }

#ifndef OS_BUILD
  StdIn = gEfiShellParametersProtocol->StdIn;
#endif
  Argc = gEfiShellParametersProtocol->Argc;
  ppArgv = gEfiShellParametersProtocol->Argv;

  for (Index = 1; Index < Argc; Index++) {
#ifndef OS_BUILD
#ifndef MDEPKG_NDEBUG
    /** For UEFI pre-parse CLI arguments for verbose logging **/
    if (0 == StrICmp(ppArgv[Index], VERBOSE_OPTION)
      || 0 == StrICmp(ppArgv[Index], VERBOSE_OPTION_SHORT)) {
      PatchPcdSet32(PcdDebugPrintErrorLevel, DEBUG_VERBOSE);
    }
#endif
#endif

    /** Need to set some flags in the case that the user wants help, but there are no DIMMs in the system **/
    if (0 == StrICmp(ppArgv[Index], HELP_VERB)
      || 0 == StrICmp(ppArgv[Index], HELP_OPTION)
      || 0 == StrICmp(ppArgv[Index], HELP_OPTION_SHORT)) {
      HelpRequested = TRUE;
      if (Argc == 2) {
        FullHelpRequested = TRUE;
      }
    }
  }

  if (Argc == 1) {
#ifndef OS_BUILD
    /* Verify input was not redirected from a file */
    if (ShellGetFileInfo(StdIn) == NULL) {
#endif
      HelpRequested = TRUE;
      FullHelpRequested = TRUE;
#ifndef OS_BUILD
    }
#endif
  }

#ifndef OS_BUILD
  BOOLEAN Ascii = FALSE;
  UINTN HandleCount = 0;
  EFI_HANDLE *pHandleBuffer = NULL;
  CHAR16 *pCurrentDriverName;
  EFI_COMPONENT_NAME_PROTOCOL *pComponentName = NULL;
  SetSerialAttributes();
#else
  EFI_HANDLE FakeBindHandle = (EFI_HANDLE)0x1;
#endif
  UINT32 Mode = PBR_NORMAL_MODE;
  CHAR16 *pTagDescription = NULL;

  if (!HelpRequested) {
    //get the current pbr mode (playback/record/normal)
    Rc = GetPbrMode(&Mode);
    if (EFI_ERROR(Rc) && (EFI_NOT_FOUND != Rc)) {
      goto Finish;
    }

    if (Mode == PBR_RECORD_MODE) {
      Print(L"Warning - Executing in recording mode!\n\n");
    }
    else if (Mode == PBR_PLAYBACK_MODE) {
      Print(L"Warning - Executing in playback mode!\n\n");
    }

    Rc = SetDefaultProtocolAndPayloadSizeOptions();
    if (EFI_ERROR(Rc) && (EFI_NOT_FOUND != Rc)) {
      goto Finish;
    }
  }

  Index = 0;

  /** Print runtime function address to ease calculation of GDB symbol loading offset. **/
  NVDIMM_DBG_CLEAN("NvmDimmCliEntryPoint=0x%016lx\n", &UefiMain);

#ifndef OS_BUILD
    /* with shell support level 3 */
    if (PcdGet8(PcdShellSupportLevel) < 3) {
      Rc = EFI_UNSUPPORTED;
      NVDIMM_WARN("shellsupport level %d too low", PcdGet8(PcdShellSupportLevel));
      Print(L"Error: EFI Shell support level 3 is required to run this application\n");
      goto Finish;
    }

    if (FALSE == HelpRequested) {
      gNvmDimmCliHiiHandle = HiiAddPackages(&gNvmDimmCliHiiGuid, ImageHandle, ipmctlStrings, NULL);

      if (gNvmDimmCliHiiHandle == NULL) {
        NVDIMM_WARN("Unable to add string package to Hii");
        goto Finish;
      }

      // Check for NVM Protocol
      Rc = gBS->LocateHandleBuffer(ByProtocol, &gNvmDimmConfigProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
      if (EFI_NOT_FOUND != Rc && (EFI_ERROR(Rc) || HandleCount != 1)) {
        if (Rc == EFI_NOT_FOUND) {
          Print(FORMAT_STR_NL, CLI_ERR_FAILED_TO_FIND_PROTOCOL);
          goto Finish;
        }
        Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
        Rc = EFI_NOT_FOUND;
        goto Finish;
      }

      Rc = OpenNvmDimmProtocol(
        gEfiComponentNameProtocolGuid,
        (VOID**)&pComponentName, NULL);
      if (EFI_ERROR(Rc) && EFI_NOT_FOUND != Rc) {
        NVDIMM_DBG("Failed to open the Component Name protocol, error = " FORMAT_EFI_STATUS "", Rc);
        goto Finish;
      }

      if (pComponentName != NULL) {
        //Get current driver name
        Rc = pComponentName->GetDriverName(
          pComponentName, "eng", &pCurrentDriverName);
        if (EFI_ERROR(Rc)) {
          NVDIMM_DBG("Could not get the driver name, error = " FORMAT_EFI_STATUS "", Rc);
          goto Finish;
        }

        //Compare to the CLI version and print warning if there is a version mismatch
        if (StrCmp(PMEM_MODULE_NAME NVMDIMM_VERSION_STRING L" Driver", pCurrentDriverName) != 0) {
          Print(FORMAT_STR_NL, CLI_WARNING_CLI_DRIVER_VERSION_MISMATCH);
        }
      }
    }

#else
  if (g_basic_commands)
  {
    Rc = RegisterNonAdminUserCommands();
    if (EFI_ERROR(Rc)) {
      goto Finish;
    }
  }
  else
#endif //OS_BUILD
  {
    Rc = RegisterCommands();
    if (EFI_ERROR(Rc)) {
      goto Finish;
    }
  }
  while (MoreInput) {
    Input.TokenCount = 0;
#ifndef OS_BUILD
    /* user entered a command on the command pLine */
    if (ShellGetFileInfo(StdIn) == NULL) {
#endif
      /* 1st arg is the name of the app, so skip it */
      if (Argc > 1 && FALSE == FullHelpRequested) {
        MoreInput = FALSE; /* only one command is supported on the command pLine */

        for (Index = 1; Index < Argc; Index++) {
          pLine = CatSPrintClean(pLine, FORMAT_STR_SPACE, ppArgv[Index]);

          if (pLine == NULL) {
            Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
            Rc = EFI_OUT_OF_RESOURCES;
            goto FinishAfterRegCmds;
          }
        }
        FillCommandInput(pLine, &Input);


        if (Input.ppTokens == NULL) {
          Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
          Rc = EFI_OUT_OF_RESOURCES;
          goto FinishAfterRegCmds;
        }
      } else {
        /* user did not enter a command */
        showHelp(NULL);
        HelpShown = TRUE;
        break;
      }
#ifndef OS_BUILD
    } else {
      /* input was redirected from a file */
      pLine = NULL;
      pLine = ShellFileHandleReturnLine(StdIn, &Ascii);
      if (pLine == NULL || StrLen(pLine) == 0) {
        /* line was empty, go to next pLine */
        if (ShellFileHandleEof(StdIn)) {
          MoreInput = FALSE;
        }
        if (pLine != NULL) {
          FreePool(pLine);
          pLine = NULL;
        }
        continue;
      }

      /* parse the line into individual tokens */
      FillCommandInput(pLine, &Input);

      if (Input.ppTokens == NULL) {
        Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
        Rc = EFI_OUT_OF_RESOURCES;
        goto FinishAfterRegCmds;

      }

      if (ShellFileHandleEof(StdIn)) {
        MoreInput = FALSE;
      }
    }
#endif
    /* Fix the passed tokens as needed */
    FixHelp(Input.ppTokens, &Input.TokenCount);
    if (TRUE == FullHelpRequested) {
      showHelp(NULL);
      HelpShown = TRUE;
      break;
    }

    /* run the command */
    Rc = Parse(&Input, &Command);

    if (!HelpRequested) {
      if (PBR_NORMAL_MODE != Mode && !Command.ExcludeDriverBinding) {
        if (PBR_RECORD_MODE == Mode) {
          pTagDescription = CatSPrint(NULL, L"%d", Rc);
          SetPbrTag(pLine, pTagDescription);
          FREE_POOL_SAFE(pTagDescription);
        }
        else {
          //CLI is responsible for tracking the tagid.
          //The id is saved to a non-persistent volatile store, and is incremented
          //after each CLI cmd invocation.  Given we have the tagid that should
          //be executed next, explicitely reset the pbr session to that id before
          //running the cmd.
          PbrDcpmmDeserializeTagId(&NextId, 0);
          ResetPbrSession(NextId);
          PbrDcpmmSerializeTagId(NextId + 1);
        }
      }
    }

    if (!EFI_ERROR(Rc)) {
      /* parse success, now run the command */
      if (Command.ShowHelp) {
        showHelp(&Command);
        HelpShown = TRUE;
      } else {
#ifdef OS_BUILD //WA, remove after all CMDs convert to "unified printing" mechanism
        if (Command.PrinterCtrlSupported) {
          gOsShellParametersProtocol.StdOut = stdout;
        }
#endif

#ifdef OS_BUILD
        if (!Command.ExcludeDriverBinding) {
          Rc = NvmDimmDriverDriverBindingStart(&gNvmDimmDriverDriverBinding, FakeBindHandle, NULL);
        }
#endif

        Rc = ExecuteCmd(&Command);

#ifdef OS_BUILD
        if (!Command.ExcludeDriverBinding) {
          NvmDimmDriverDriverBindingStop(&gNvmDimmDriverDriverBinding, FakeBindHandle, 0, NULL);
        }
#endif
      }
      if (EFI_ERROR(Rc)) {
        MoreInput = FALSE; /* stop on failures */
      }
    } else { /* syntax error */

      /* print the error */
      LongPrint(getSyntaxError());
      Print(L"\n");
      MoreInput = FALSE; /* stop on failures */
    }
#ifdef OS_BUILD
    nvm_current_cmd(Command);
#endif
    FreeCommandInput(&Input);
    FreeCommandStructure(&Command);
  } /* end while more input */
FinishAfterRegCmds:
  /* clean up */
  FreeCommands();

Finish:
  FREE_POOL_SAFE(pLine);
  FreeCommandInput(&Input);
  if (gNvmDimmCliHiiHandle != NULL) {
    HiiRemovePackages(gNvmDimmCliHiiHandle);
  }
#if _BullseyeCoverage
#ifndef OS_BUILD
  cov_dumpData();
#endif // !OS_BUILD
#endif // _BullseyeCoverage

  //if help was displayed and not explicitly requested, ensure an error is returned
  if (TRUE == HelpShown && FALSE == HelpRequested && FALSE == FullHelpRequested && EFI_SUCCESS == Rc) {
    Rc = EFI_INVALID_PARAMETER;
  }

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
Register basic commands on the commands list for non-root users

@retval a return code from called functions
**/
EFI_STATUS
RegisterNonAdminUserCommands(
)
{
  EFI_STATUS Rc;

  NVDIMM_ENTRY();
  Rc = RegisterCommand(&VersionCommand);
  if (EFI_ERROR(Rc)) {
    goto done;
  }
done:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}
/**
  Register commands on the commands list

  @retval a return code from called functions
**/
EFI_STATUS
RegisterCommands(
)
{
  EFI_STATUS Rc;

  NVDIMM_ENTRY();

  /* Always register */
  Rc = RegisterCommand(&HelpCommand);
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterNonAdminUserCommands();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  // Dimm Discovery Commands
  Rc = RegisterShowTopologyCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowSocketsCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowDimmsCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowMemoryResourcesCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowSystemCapabilitiesCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  // Provisioning Commands
  Rc = RegisterCreateGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDumpGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterLoadGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDeleteGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  // Security Commands
  Rc = RegisterSetDimmCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDeleteDimmCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  // Persistent Memory Provisioning Commands
  Rc = RegisterShowRegionsCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#ifndef OS_BUILD
  Rc = RegisterShowNamespaceCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterCreateNamespaceCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDeleteNamespaceCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif

  // Instrumentation Commands
  Rc = RegisterShowSensorCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
  Rc = RegisterSetSensorCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#ifdef OS_BUILD
#ifdef __MFG__
  Rc = RegisterMfgCommands();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#else
  Rc = RegisterShowPerformanceCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif // __MFG__
#endif // OS_BUILD

  // Support and Maintenance Commands
  Rc = RegisterShowFirmwareCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterLoadCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowPreferencesCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterSetPreferencesCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#ifdef OS_BUILD
  Rc = RegisterDumpSupportCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif // OS_BUILD

  // Debug Commands
  Rc = RegisterStartDiagnosticCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowErrorCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDumpDebugCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowAcpiCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowPcdCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDeletePcdCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowCmdAccessPolicyCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowCelCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#ifndef OS_BUILD
  /* Debug Utility commands */
  Rc = registerShowSmbiosCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#ifndef MDEPKG_NDEBUG
  Rc = RegisterShowRegisterCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif

#ifdef FORMAT_SUPPORTED
  Rc = RegisterStartFormatCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif
#endif

  // PBR Commands
  Rc = RegisterStartSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDumpSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
  Rc = RegisterLoadSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterStopSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

done:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/*
 * Print the CLI application help
 */
EFI_STATUS showHelp(struct Command *pCmd)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pHelp = NULL;

  NVDIMM_ENTRY();

  if ((pCmd == NULL) || (StrCmp(pCmd->verb, HELP_VERB) == 0 && pCmd->ShowHelp == FALSE)) {
#ifndef OS_BUILD
    //Page break option only for UEFI
    ShellSetPageBreakMode(TRUE);
#endif
    Print(FORMAT_STR_SPACE FORMAT_STR_NL_NL L"    Usage: " FORMAT_STR L" <verb>[<options>][<targets>][<properties>]\n\nCommands:\n", PRODUCT_NAME, APP_DESCRIPTION, EXE_NAME);
    pHelp = getOverallCommandHelp();
  } else {
    pHelp = getCommandHelp(pCmd, TRUE);
  }

  if (pHelp != NULL) {
    LongPrint(pHelp);
    FreePool(pHelp);
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#define DS_ROOT_PATH                          L"/SoftwareList"
#define DS_SOFTWARE_PATH                      L"/SoftwareList/Software"
#define DS_SOFTWARE_INDEX_PATH                L"/SoftwareList/Software[%d]"


PRINTER_LIST_ATTRIB ShowVersionListAttributes =
{
 {
    {
      L"Software",                            //GROUP LEVEL TYPE
      L"$(Component) Version $(Version)",     //NULL or GROUP LEVEL HEADER
      NULL,                                   //NULL or KEY VAL FORMAT STR
      L"Component;Version"                    //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowVersionDataSetAttribs =
{
  &ShowVersionListAttributes,
  NULL
};

/*
 * Set a PBR session tag
 */
EFI_STATUS SetPbrTag(CHAR16 *pName, CHAR16 *pDescription) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol = NULL;


  ReturnCode = OpenNvmDimmProtocol(gNvmDimmPbrProtocolGuid, (VOID **)&pNvmDimmPbrProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    Print(CLI_ERR_OPENING_PBR_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmPbrProtocol->PbrSetTag(PBR_DCPMM_CLI_SIG, pName, pDescription, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(CLI_ERR_FAILED_TO_SET_SESSION_TAG);
    goto Finish;
  }
Finish:
  return ReturnCode;
}

/*
 * Get the current PBR session mode (playback/record)
 */
EFI_STATUS GetPbrMode(UINT32 *Mode) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol = NULL;

  *Mode = PBR_NORMAL_MODE;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmPbrProtocolGuid, (VOID **)&pNvmDimmPbrProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    NVDIMM_DBG("Failed to open the PBR protocol, error = " FORMAT_EFI_STATUS, ReturnCode);
    goto Finish;
  }

  ReturnCode = pNvmDimmPbrProtocol->PbrGetMode(Mode);
  if (EFI_ERROR(ReturnCode)) {
    Print(CLI_ERR_FAILED_TO_GET_PBR_MODE);
    goto Finish;
  }
Finish:
  return ReturnCode;
}

/*
 * Reset the session to a specified tag
 */
EFI_STATUS ResetPbrSession(UINT32 TagId) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol = NULL;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmPbrProtocolGuid, (VOID **)&pNvmDimmPbrProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    Print(CLI_ERR_OPENING_PBR_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmPbrProtocol->PbrResetSession(TagId);
  if (EFI_ERROR(ReturnCode)) {
    Print(CLI_ERR_FAILED_TO_SET_SESSION_TAG);
    goto Finish;
  }
Finish:
  return ReturnCode;
}

/*
 * Print the CLI app version
 */
EFI_STATUS showVersion(struct Command *pCmd)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pPath = NULL;
  UINT32 DimmIndex = 0;
  UINT32 DimmCount = 0;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmFromTheFutureCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];

#if !defined(__LINUX__)
  CHAR16 ApiVersion[FW_API_VERSION_LEN] = { 0 };
#endif

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (ReturnCode != EFI_NOT_FOUND && EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  if (ReturnCode != EFI_NOT_FOUND) {
#if !defined(__LINUX__)
    ReturnCode = pNvmDimmConfigProtocol->GetDriverApiVersion(pNvmDimmConfigProtocol, ApiVersion);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
      goto Finish;
    }
#endif
  }

    PRINTER_BUILD_KEY_PATH(pPath, DS_SOFTWARE_INDEX_PATH, 0);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Component", PRODUCT_NAME L" " APP_DESCRIPTION);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Version", NVMDIMM_VERSION_STRING);

#if !defined(__LINUX__)
    PRINTER_BUILD_KEY_PATH(pPath, DS_SOFTWARE_INDEX_PATH, 1);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Component", PRODUCT_NAME L" " DRIVER_API_DESCRIPTION);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Version", ApiVersion);
#endif

    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
      goto Finish;
    }

  /*Check FIS against compiled version in this SW... warn if the FIS version from FW is > version from this SW*/
  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
  if (NULL == pDimms) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
    DIMM_INFO_CATEGORY_ALL, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DCPMM inventory found in NFIT");
    goto Finish;
  }

  for (DimmIndex = 0; DimmIndex < DimmCount; DimmIndex++) {
    ReturnCode = GetPreferredDimmIdAsString(pDimms[DimmIndex].DimmHandle, pDimms[DimmIndex].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_ABORTED;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to determine DCPMM id for DCPMM ID %d", pDimms[DimmIndex].DimmHandle);
      goto Finish;
    }

    if (pDimms[DimmIndex].FwVer.FwApiMajor > MAX_FIS_SUPPORTED_BY_THIS_SW_MAJOR ||
      (pDimms[DimmIndex].FwVer.FwApiMajor == MAX_FIS_SUPPORTED_BY_THIS_SW_MAJOR &&
        pDimms[DimmIndex].FwVer.FwApiMinor > MAX_FIS_SUPPORTED_BY_THIS_SW_MINOR)) {
      DimmFromTheFutureCount++;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"DCPMM " FORMAT_STR L" supports FIS %d.%d\r\n",
        DimmStr,
        pDimms[DimmIndex].FwVer.FwApiMajor,
        pDimms[DimmIndex].FwVer.FwApiMinor);
    }

  }

  if (DimmFromTheFutureCount > 0) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"This ipmctl software version predates the firmware interface specification version (FIS | FWAPIVersion: %d.%d) for %d DCPMM(s). It is recommended to update ipmctl.\r\n",
      MAX_FIS_SUPPORTED_BY_THIS_SW_MAJOR,
      MAX_FIS_SUPPORTED_BY_THIS_SW_MINOR,
      DimmFromTheFutureCount);
  }

  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowVersionDataSetAttribs);
  //Force as list
  PRINTER_ENABLE_LIST_TABLE_FORMAT(pPrinterCtx);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STATUS SetDefaultProtocolAndPayloadSizeOptions()
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS Attribs;
#ifdef OS_BUILD
  // Default value for ini file (OS only) is set in ipmctl_default.h
  BOOLEAN IsDdrtProtocolDisabled = ConfigIsDdrtProtocolDisabled();
  BOOLEAN IsLargePayloadDisabled = ConfigIsLargePayloadDisabled();
#endif // OS_BUILD
  NVDIMM_ENTRY();

  // Clearly set defaults. Auto = no restrictions
  Attribs.Protocol = FisTransportAuto;
  Attribs.PayloadSize = FisTransportSizeAuto;

#ifdef OS_BUILD
  // Equivalent to passing "-smbus"
  if (IsDdrtProtocolDisabled) {
    Attribs.Protocol = FisTransportSmbus;
    Attribs.PayloadSize = FisTransportSizeSmallMb;
  }

  if (IsLargePayloadDisabled) {
    Attribs.PayloadSize = FisTransportSizeSmallMb;
  }
#endif // OS_BUILD

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  CHECK_RESULT(pNvmDimmConfigProtocol->SetFisTransportAttributes(pNvmDimmConfigProtocol, Attribs), Finish);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
