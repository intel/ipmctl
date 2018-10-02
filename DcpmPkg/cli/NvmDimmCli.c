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
#else
#include "DeletePcdCommand.h"
EFI_GUID gNvmDimmConfigProtocolGuid = EFI_DCPMM_CONFIG_PROTOCOL_GUID;
EFI_GUID gIntelDimmConfigVariableGuid = INTEL_DIMM_CONFIG_VARIABLE_GUID;
#endif
EFI_GUID gNvmDimmDriverHealthGuid = EFI_DRIVER_HEALTH_PROTOCOL_GUID;

extern EFI_SHELL_INTERFACE *mEfiShellInterface;

#ifdef OS_BUILD
EFI_HANDLE gNvmDimmCliHiiHandle = (EFI_HANDLE)0x1;
extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;
#else
EFI_HANDLE gNvmDimmCliHiiHandle = NULL;
#endif

#define NVMDIMM_CLI_HII_GUID \
  { 0x26e4ac23, 0xd32f, 0x4788, {0x83, 0x95, 0xb0, 0x2a, 0x33, 0x0c, 0x28, 0x26}}

EFI_GUID gNvmDimmCliHiiGuid = NVMDIMM_CLI_HII_GUID;

/* Local fns */
static EFI_STATUS showVersion(struct Command *pCmd);

/**
  Supported commands
  Display CLI application help
**/
struct Command HelpCommand =
{
  HELP_VERB,                                  //!< verb
  {
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
#ifndef OS_BUILD
  BOOLEAN Ascii = FALSE;
  SHELL_FILE_HANDLE StdIn = NULL;
  UINTN HandleCount = 0;
  EFI_HANDLE *pHandleBuffer = NULL;
#endif

  NVDIMM_ENTRY();

  ZeroMem(&Input, sizeof(Input));
  ZeroMem(&Command, sizeof(Command));

  /** Print runtime function address to ease calculation of GDB symbol loading offset. **/
  NVDIMM_DBG_CLEAN("NvmDimmCliEntryPoint=0x%016lx\n", &UefiMain);


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
        FREE_POOL_SAFE(pLine);

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
      FreePool(pLine);

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
        Rc = ExecuteCmd(&Command);
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

  Rc = RegisterDeletePcdCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }

#endif
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

  /* Debug Utility commands */
#ifndef MDEPKG_NDEBUG
  Rc = RegisterShowCmdAccessPolicyCommand();
  if (EFI_ERROR(Rc)) {
    goto done;
  }
#endif

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
 * Print the CLI app version
 */
EFI_STATUS showVersion(struct Command *pCmd)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pPath = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
#if !defined(__LINUX__)
  CHAR16 ApiVersion[FW_API_VERSION_LEN] = {0};
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
