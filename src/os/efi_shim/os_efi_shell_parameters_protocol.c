/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/ShellCommandLib.h>
#include <Guid/FileInfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <s_str.h>
#ifdef _MSC_VER
#include <io.h>
#include <conio.h>
#else
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#define _read read
#define _getch getchar
#include <safe_str_lib.h>
#endif

#include <sys/stat.h> 
#include <fcntl.h>
#include "os_efi_shell_parameters_protocol.h"

EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;
EFI_SHELL_PARAMETERS_PROTOCOL *gEfiShellParametersProtocol = &gOsShellParametersProtocol;

int g_fast_path = 0;
int g_file_io = 0;
#define STR_DASH_OUTPUT_SHORT   "-o"
#define STR_DASH_OUTPUT_LONG    "-output"
#define STR_NVMXML              "nvmxml"
#define STR_ESXXML              "esx"
#define STR_ESXTABLE            "esxtable"
#define STR_TEXT                "text"
#define STR_VERBOSE             "verbose"
#define STR_DASH_FAST_LONG      "-fast"
#define MAX_INPUT_PARAMS        256

EFI_STATUS init_protocol_shell_parameters_protocol(int argc, char *argv[])
{
	int i = 1;
	int stripped_args = 0;
  if (argc > MAX_INPUT_PARAMS) {
    return EFI_INVALID_PARAMETER;
  }
  
  gOsShellParametersProtocol.Argv = AllocateZeroPool(MAX_INPUT_PARAMS * sizeof(CHAR16*));
	if (NULL == gOsShellParametersProtocol.Argv) {
		return EFI_OUT_OF_RESOURCES;
	}
	gOsShellParametersProtocol.StdErr = stderr;
	gOsShellParametersProtocol.StdOut = stdout;
	gOsShellParametersProtocol.StdIn = stdin;
	gOsShellParametersProtocol.Argc = argc;

	for (int Index = 1; Index < argc; Index++) {
    stripped_args = 0;
		if ( (0 == s_strncmpi(argv[Index], STR_DASH_OUTPUT_SHORT, strlen(STR_DASH_OUTPUT_SHORT)+1) ||
			  0 == s_strncmpi(argv[Index], STR_DASH_OUTPUT_LONG, strlen(STR_DASH_OUTPUT_LONG+1))) &&
        Index+1 != argc)
		{
        char *tok;
        tok = strtok(argv[Index + 1], ",");

        while(tok)
        {
          if (0 == s_strncmpi(tok, STR_NVMXML, strlen(STR_NVMXML) + 1) ||
              0 == s_strncmpi(tok, STR_ESXXML, strlen(STR_ESXXML) + 1) ||
              0 == s_strncmpi(tok, STR_ESXTABLE, strlen(STR_ESXTABLE) + 1))
          {
            g_file_io = 1;
            stripped_args = 1;
            gOsShellParametersProtocol.StdOut = fopen("output.tmp", "w+");
          }
          else if(0 == s_strncmpi(tok, STR_TEXT, strlen(STR_TEXT) + 1))
          {
            stripped_args = 1;
          }
          //todo setup logging to enable verbose and output to stdout (not persistently)
          else if(0 == s_strncmpi(tok, STR_VERBOSE, strlen(STR_VERBOSE) + 1))
          {
            stripped_args = 1;
          }
          else
          {
            stripped_args = 0;
            g_file_io = 0;
            gOsShellParametersProtocol.StdOut = stdout;
            break;
          }
          tok = strtok(NULL, ",");
        }

        if(stripped_args)
        {
          gOsShellParametersProtocol.Argc -= 2;
          ++Index;
        }
    }
    else if (0 == s_strncmpi(argv[Index], STR_DASH_FAST_LONG, strlen(STR_DASH_FAST_LONG) + 1))
    {
      --gOsShellParametersProtocol.Argc;
      g_fast_path = 1;
      stripped_args = 1;
    }

    if(!stripped_args)
    {
      int argvSize = strlen(argv[Index]);
      VOID * ptr = AllocateZeroPool((argvSize + 1) * sizeof(wchar_t));
	    if (NULL == ptr) {
		    FreePool(gOsShellParametersProtocol.Argv);
		    return EFI_OUT_OF_RESOURCES;
	    }
	    gOsShellParametersProtocol.Argv[i] = AsciiStrToUnicodeStr(argv[Index], ptr);
	    ++i;
    }
	}
	return 0;
}

int uninit_protocol_shell_parameters_protocol()
{
  int Index = 0;
	if (g_file_io)
		fclose(gOsShellParametersProtocol.StdOut);

  for(Index = 0; Index < gOsShellParametersProtocol.Argc; ++Index)
  {
    if(NULL != gOsShellParametersProtocol.Argv[Index])
    {
      FreePool(gOsShellParametersProtocol.Argv[Index]);
    }
  }

  if(NULL != gOsShellParametersProtocol.Argv)
  {
    FreePool(gOsShellParametersProtocol.Argv);
  }
	return EFI_SUCCESS;
}
