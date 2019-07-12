/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the Windows implementation of the os_adapter.h
 * system call wrappers.
 */

#include <windows.h>
#include <winnt.h>
#include <stdio.h>
#include <strsafe.h>
#include "os_types.h"

/**
Sleeps for a given number of microseconds.

Note - Windows sleeps in millisecond intervals.
**/
int bs_sleep(int microseconds) {
  Sleep(microseconds / 1000);
  return 0;
}
#define REGISTRY_PATH "Software\\ipmctl"

int registry_volatile_write(const char *key, unsigned int dword_val)
{
  DWORD ReturnCode;
  HKEY hKey;

  //Check if the registry exists
  ReturnCode = RegOpenKeyExA(
    HKEY_LOCAL_MACHINE,
	  REGISTRY_PATH,
    0,
    KEY_WRITE,
    &hKey
  );

  if (ReturnCode != ERROR_SUCCESS)
  {
	  DWORD dwDisposition;
	  // Create a key if it did not exist
	  ReturnCode = RegCreateKeyExA(
		  HKEY_LOCAL_MACHINE,
		  REGISTRY_PATH,
		  0,
		  NULL,
		  REG_OPTION_VOLATILE,
		  KEY_ALL_ACCESS,
		  NULL,
		  &hKey,
		  &dwDisposition
	  );

	  if (ReturnCode != ERROR_SUCCESS)
	  {
		  return ReturnCode;
	  }
  }

  ReturnCode = RegSetValueExA(
	  hKey,
	  key,
	  0,
	  REG_DWORD,
	  (BYTE*)&dword_val,
	  sizeof(dword_val));

  RegCloseKey(hKey);

  return ReturnCode;
}

int registry_read(const char *key, unsigned int *dword_val, unsigned int default_val)
{
  HKEY hKey;
  DWORD ReturnCode;
  DWORD cbData;
  DWORD cbVal = 0;

  //Check if the registry exists
  ReturnCode = RegOpenKeyExA(
    HKEY_LOCAL_MACHINE,
	  REGISTRY_PATH,
    0,
    KEY_READ,
    &hKey
  );

  if (ReturnCode != ERROR_SUCCESS)
  {
	  *dword_val = default_val;
	  return ReturnCode;
  }

  ReturnCode = RegQueryValueExA(
	                hKey,
	                key,
	                NULL,
	                NULL,
	                (LPBYTE)&cbVal,
	                &cbData);

  if (ReturnCode == ERROR_SUCCESS)
  {
	  *dword_val = cbVal;
  }
	else
	{
		*dword_val = default_val;
	}
  RegCloseKey(hKey);
  return ReturnCode;
}
