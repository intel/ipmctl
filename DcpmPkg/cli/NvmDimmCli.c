/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NvmDimmCli.h"
#include <Uefi.h>
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
#include "SetNamespaceCommand.h"
#include "ShowErrorCommand.h"
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
#include "ShowHostServerCommand.h"
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
#include "ShowEventCommand.h"
#include "SetEventCommand.h"
#include "DumpSupportCommand.h"
#include <stdio.h>
extern void nvm_current_cmd(struct Command Command);
extern BOOLEAN ConfigIsDdrtProtocolDisabled();
extern BOOLEAN ConfigIsLargePayloadDisabled();
#else
#include "DeletePcdCommand.h"
EFI_GUID gNvmDimmConfigProtocolGuid = EFI_DCPMM_CONFIG_PROTOCOL_GUID;
EFI_GUID gIntelDimmConfigVariableGuid = INTEL_DIMM_CONFIG_VARIABLE_GUID;
EFI_GUID gIntelDimmPbrTagIdVariableguid = INTEL_DIMM_PBR_TAGID_VARIABLE_GUID;
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
#ifdef OS_BUILD
static EFI_STATUS SetDefaultProtocolAndPayloadSizeOptions();
#endif

/**
  Supported commands
  Display CLI application help
**/
struct Command HelpCommand =
{
  HELP_VERB,                                  //!< verb
  {
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }, //!< options
#endif // OS_BUILD
    {L"", L"", L"", L"", FALSE, ValueOptional}
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
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
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

/*
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
  UINTN Argc = 0;
  CHAR16 **ppArgv = NULL;
  UINT32 NextId = 0;

#ifndef OS_BUILD
  BOOLEAN Ascii = FALSE;
  SHELL_FILE_HANDLE StdIn = NULL;
  UINTN HandleCount = 0;
  EFI_HANDLE *pHandleBuffer = NULL;
  CHAR16 *pCurrentDriverName;
  EFI_COMPONENT_NAME_PROTOCOL *pComponentName = NULL;
#else
  EFI_HANDLE FakeBindHandle = (EFI_HANDLE)0x1;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS pAttribs;
#endif
  UINT32 Mode;
  CHAR16 *pTagDescription = NULL;
  
  //get the current pbr mode (playback/record/normal)
  Rc = GetPbrMode(&Mode);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }

#ifndef OS_BUILD
#ifndef MDEPKG_NDEBUG
  /** For UEFI pre-parse CLI arguments for verbose logging **/
  if (gEfiShellParametersProtocol != NULL) {
    for (Index = 1; Index < gEfiShellParametersProtocol->Argc; Index++) {
      if (0 == StrICmp(gEfiShellParametersProtocol->Argv[Index], VERBOSE_OPTION)
        || 0 == StrICmp(gEfiShellParametersProtocol->Argv[Index], VERBOSE_OPTION_SHORT)) {
        PatchPcdSet32(PcdDebugPrintErrorLevel, DEBUG_VERBOSE);
      }
    }
  }
#endif
#endif

#ifdef OS_BUILD
  Rc = SetDefaultProtocolAndPayloadSizeOptions();
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }
#endif

  NVDIMM_ENTRY();
  Index = 0;
  ZeroMem(&Input, sizeof(Input));
  ZeroMem(&Command, sizeof(Command));

  /** Print runtime function address to ease calculation of GDB symbol loading offset. **/
  NVDIMM_DBG_CLEAN("NvmDimmCliEntryPoint=0x%016lx\n", &UefiMain);

  if (Mode == PBR_RECORD_MODE) {
    Print(L"Warning - Executing in recording mode!\n\n");
  }
  else if (Mode == PBR_PLAYBACK_MODE) {
    Print(L"Warning - Executing in playback mode!\n\n");
  }
  

#if !defined(MDEPKG_NDEBUG) && !defined(_MSC_VER)
  /**
  Enable recording AllocatePool and FreePool occurences only with GCC, under Debug build
  **/
  EnableTracing();
#endif
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
  /* with shell support level 3 */
  else if (PcdGet8(PcdShellSupportLevel) < 3) {
    Rc = EFI_UNSUPPORTED;
    NVDIMM_WARN("shellsupport level %d too low", PcdGet8(PcdShellSupportLevel));
    Print(L"Error: EFI Shell support level 3 is required to run this application\n");
    goto Finish;
  }
  // We have the shell, we need to initialize the argv, argc and stdin variables
  if (gEfiShellParametersProtocol != NULL) {
    StdIn = gEfiShellParametersProtocol->StdIn;
    Argc = gEfiShellParametersProtocol->Argc;
    ppArgv = gEfiShellParametersProtocol->Argv;
  } else if (mEfiShellInterface != NULL) {
    StdIn = mEfiShellInterface->StdIn;
    Argc = mEfiShellInterface->Argc;
    ppArgv = mEfiShellInterface->Argv;
  } else {
    NVDIMM_WARN("ShellInitialize succeeded but the shell interface and parameters protocols do not exist");
    Print(L"Error: EFI Shell 2.0 is required to run this application.\n");
    goto Finish;
  }

  gNvmDimmCliHiiHandle = HiiAddPackages(&gNvmDimmCliHiiGuid, ImageHandle, ipmctlStrings, NULL);
  if (gNvmDimmCliHiiHandle == NULL) {
    NVDIMM_WARN("Unable to add string package to Hii");
    goto Finish;
  }

  // Check for NVM Protocol
  Rc = gBS->LocateHandleBuffer(ByProtocol, &gNvmDimmConfigProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(Rc) || HandleCount != 1) {
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
  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Failed to open the Component Name protocol, error = " FORMAT_EFI_STATUS "", Rc);
    goto Finish;
  }

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

#else
  Argc = gEfiShellParametersProtocol->Argc;
  ppArgv = gEfiShellParametersProtocol->Argv;
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
      if (Argc > 1) {
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
    /* run the command */
    Rc = Parse(&Input, &Command);

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

    if (!EFI_ERROR(Rc)) {
      /* parse success, now run the command */
      if (Command.ShowHelp) {
        showHelp(&Command);
      } else {
#ifdef OS_BUILD //WA, remove after all CMDs convert to "unified printing" mechanism
        if (Command.PrinterCtrlSupported) {
          gOsShellParametersProtocol.StdOut = stdout;
        }
#endif

#ifdef OS_BUILD
        if (!Command.ExcludeDriverBinding) {
          Rc = NvmDimmDriverDriverBindingStart(&gNvmDimmDriverDriverBinding, FakeBindHandle, NULL);
          if (EFI_UNSUPPORTED == Rc) {
            Rc = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
            if (EFI_ERROR(Rc)) {
              goto Finish;
            }

            Rc = pNvmDimmConfigProtocol->GetFisTransportAttributes(pNvmDimmConfigProtocol, &pAttribs);
            if (EFI_ERROR(Rc)) {
              goto Finish;
            }

            if (IS_SMBUS_ENABLED(pAttribs)) {
              Print(CLI_ERR_TRANSPORT_PROTOCOL_UNSUPPORTED_ON_OS, PROTOCOL_OPTION_SMBUS, PROTOCOL_OPTION_DDRT);
            }
          }
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
  if (gNvmDimmCliHiiHandle != NULL) {
    HiiRemovePackages(gNvmDimmCliHiiHandle);
  }
#if _BullseyeCoverage
#ifndef OS_BUILD
  cov_dumpData();
#endif // !OS_BUILD
#endif // _BullseyeCoverage
#if !defined(MDEPKG_NDEBUG) && !defined(_MSC_VER)
  /**
  Disable recording AllocatePool and FreePool occurrences, print list and clear it
  **/
  FlushPointerTrace((CHAR16 *)__WFUNCTION__);
#endif
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

  /* Base Utility commands */

  Rc = RegisterLoadCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterSetDimmCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDeleteDimmCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowRegionsCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterCreateGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }


  Rc = RegisterDeleteGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterLoadGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDumpGoalCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterSetSensorCommand();
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

  Rc = RegisterSetNamespaceCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif
  Rc = RegisterStartSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterStopSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }


  Rc = RegisterDumpSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterLoadSessionCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }


  Rc = RegisterDeletePcdCommand();
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

  Rc = RegisterShowDimmsCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowSocketsCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowSensorCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterStartDiagnosticCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowTopologyCommand();
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

  Rc = RegisterShowFirmwareCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = registerShowAcpiCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterShowPcdCommand();
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

  Rc = RegisterShowCmdAccessPolicyCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#ifdef OS_BUILD
  Rc = RegisterShowHostServerCommand();
  if (EFI_ERROR(Rc)) {
     goto done;
  }

  Rc = RegisterShowEventCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterSetEventCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

  Rc = RegisterDumpSupportCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

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

#ifndef OS_BUILD
  /* Debug Utility commands */
#ifndef MDEPKG_NDEBUG
  Rc = registerShowSmbiosCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

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
    Print(FORMAT_STR_SPACE FORMAT_STR_NL_NL L"    Usage: " FORMAT_STR L" <verb>[<options>][<targets>][<properties>]\n\nCommands:\n", PRODUCT_NAME, APP_DESCRIPTION, EXE_NAME);
    pHelp = getCommandHelp(NULL, FALSE);
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;


  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    Print(CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->PbrSetTag(PBR_DCPMM_CLI_SIG, pName, pDescription, NULL);
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    Print(CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->PbrGetMode(Mode);
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    Print(CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->PbrResetSession(TagId);
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

#if !defined(__LINUX__)
  ReturnCode = pNvmDimmConfigProtocol->GetDriverApiVersion(pNvmDimmConfigProtocol, ApiVersion);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }
#endif

  PRINTER_BUILD_KEY_PATH(pPath, DS_SOFTWARE_INDEX_PATH, 0);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Component", PRODUCT_NAME L" " APP_DESCRIPTION);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Version", NVMDIMM_VERSION_STRING);

#if !defined(__LINUX__)
  PRINTER_BUILD_KEY_PATH(pPath, DS_SOFTWARE_INDEX_PATH, 1);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Component", PRODUCT_NAME L" " DRIVER_API_DESCRIPTION);
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Version", ApiVersion);
#endif

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
        pDimms[DimmIndex].FwVer.FwApiMinor,
        MAX_FIS_SUPPORTED_BY_THIS_SW_MAJOR,
        MAX_FIS_SUPPORTED_BY_THIS_SW_MINOR);
    }

  }

  if (DimmFromTheFutureCount > 0) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"This ipmctl software version predates the firmware interface specification version (FIS | FWAPIVersion: %d.%d.) for %d DCPMM(s). It is recommended to update ipmctl.\r\n",
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

#ifdef OS_BUILD
EFI_STATUS SetDefaultProtocolAndPayloadSizeOptions()
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS pAttribs;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);

  if (ConfigIsDdrtProtocolDisabled()) {
    pAttribs.Protocol = FisTransportSmbus;
    pAttribs.PayloadSize = FisTransportSmallMb;
  }
  else {
    pAttribs.Protocol = FisTransportDdrt;
  }

  if (ConfigIsLargePayloadDisabled()) {
    pAttribs.PayloadSize = FisTransportSmallMb;
  }
  else {
    pAttribs.PayloadSize = FisTransportLargeMb;
  }

  ReturnCode = pNvmDimmConfigProtocol->SetFisTransportAttributes(pNvmDimmConfigProtocol, pAttribs);

  return ReturnCode;
}
#endif
