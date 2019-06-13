/** @file
  Main file for NULL named library for level 2 shell command functions.

  these functions are:
  attrib,   cd,   cp,   date*,  time*,  rm,   reset,
  load,     ls,   map,  mkdir,  mv,     parse,  set,  timezone*


  * functions are non-interactive only


  Copyright (c) 2009 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _UEFI_SHELL_LEVEL2_COMMANDS_LIB_H_
#define _UEFI_SHELL_LEVEL2_COMMANDS_LIB_H_

#include <Uefi.h>

#include <Guid/GlobalVariable.h>
#include <Guid/ShellLibHiiGuid.h>

#include <Protocol/Shell.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/UnicodeCollation.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/ShellCommandLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/HiiLib.h>
#include <Library/SortLib.h>
#include <Library/FileHandleLib.h>

extern CONST  CHAR16                            mFileName[];
extern        EFI_HANDLE                        gShellLevel2HiiHandle;

/**
  Function for 'attrib' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunAttrib (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'date' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunDate (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'time' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunTime (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'load' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunLoad (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'ls' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunLs (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'map' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunMap (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'reset' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunReset (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'timezone' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunTimeZone (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'set' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunSet (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'mkdir' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunMkDir (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'cd' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunCd (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'cp' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunCp (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'parse' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunParse (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'rm' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunRm (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function for 'mv' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunMv (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  returns a fully qualified directory (contains a map drive at the beginning)
  path from a unknown directory path.

  If Path is already fully qualified this will return a duplicat otherwise this
  will use get the current directory and use that to build the fully qualified
  version.

  if the return value is not NULL it must be caller freed.

  @param[in] Path         The unknown Path Value

  @retval NULL            A memory allocation failed
  @retval NULL            a fully qualified path could not be discovered.
  @retval other           pointer to a fuly qualified path.
**/
CHAR16*
GetFullyQualifiedPath(
  IN CONST CHAR16* Path
  );

/**
  Function to verify all intermediate directories in the path.

  @param[in] Path       The pointer to the path to fix.

  @retval EFI_SUCCESS   The operation was successful.
**/
EFI_STATUS
VerifyIntermediateDirectories (
  IN CONST CHAR16 *Path
  );

/**
  CaseInsensitive length limited string comparison.

  @param[in] Source   Pointer to first string.
  @param[in] Target   Pointer to second string.
  @param[in] Count    Number of characters to compare.

  @retval 0   The strings are the same.
  @return     non-zero if the strings are different.
**/
CONST CHAR16*
StrniCmp(
  IN CONST CHAR16 *Source,
  IN CONST CHAR16 *Target,
  IN CONST UINTN  Count
  );

/**
  Cleans off all the quotes in the string.

  @param[in]     OriginalString   pointer to the string to be cleaned.
  @param[out]   CleanString      The new string with all quotes removed. 
                                                  Memory allocated in the function and free 
                                                  by caller.

  @retval EFI_SUCCESS   The operation was successful.
**/
EFI_STATUS
ShellLevel2StripQuotes (
  IN  CONST CHAR16     *OriginalString,
  OUT CHAR16           **CleanString
  );

/**
  Function for 'Vol' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunVol (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Function to Copy one file to another location

  If the destination exists the user will be prompted and the result put into *resp

  @param[in] Source     pointer to source file name
  @param[in] Dest       pointer to destination file name
  @param[out] Resp      pointer to response from question.  Pass back on looped calling
  @param[in] SilentMode whether to run in quiet mode or not
  @param[in] CmdName    Source command name requesting single file copy

  @retval SHELL_SUCCESS   The source file was copied to the destination
**/
SHELL_STATUS
CopySingleFile(
  IN CONST CHAR16 *Source,
  IN CONST CHAR16 *Dest,
  OUT VOID        **Resp,
  IN BOOLEAN      SilentMode,
  IN CONST CHAR16 *CmdName
  );

/**
  Delete a node and all nodes under it (including sub directories).

  @param[in] Node   The node to start deleting with.
  @param[in] Quiet  TRUE to print no messages.

  @retval SHELL_SUCCESS       The operation was successful.
  @retval SHELL_ACCESS_DENIED A file was read only.
  @retval SHELL_ABORTED       The abort message was received.
  @retval SHELL_DEVICE_ERROR  A device error occurred reading this Node.
**/
SHELL_STATUS
CascadeDelete(
  IN EFI_SHELL_FILE_INFO  *Node,
  IN CONST BOOLEAN        Quiet
  );

#endif

