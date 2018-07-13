/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <CommandParser.h>

#if defined(__LINUX__) || defined(__ESX__)
#define EXE_NAME               L"ipmctl"
#elif defined(_MSC_VER)
#define EXE_NAME               L"ipmctl.exe"
#else
#define EXE_NAME               L"ipmctl.efi"
#endif
#define APP_DESCRIPTION        L"Command Line Interface"
#define DRIVER_API_DESCRIPTION L"Driver API"

extern        EFI_HANDLE                        gNvmDimmCliHiiHandle;

//
// This is the generated String package data for all .UNI files.
// This data array is ready to be used as input of HiiAddPackages() to
// create a packagelist (which contains Form packages, String packages, etc).
//
extern unsigned char ipmctlStrings[];
extern int g_basic_commands;
/**
  Register commands on the commands list

  @retval a return code from called functions
**/
EFI_STATUS
RegisterCommands(
  );

/**
Register basic commands on the commands list for non-root users

@retval a return code from called functions
**/
EFI_STATUS
RegisterNonAdminUserCommands(
);

/**
  Print the CLI application help
**/
EFI_STATUS showHelp(struct Command *pCmd);
