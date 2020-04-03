/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <UefiBaseType.h>
#include <BaseLib.h>
#include <ini.h>

#if defined(__LINUX__) || defined(__ESX__)
#define INI_FILENAME		"ipmctl.conf"
#else
#define INI_FILENAME		"ipmctl.ini"
#endif

dictionary *gIni = NULL;
const char *g_p_filename = NULL;

EFI_STATUS preferences_init(IN CONST char *p_filename)
{
  if (NULL == g_p_filename) {
    if (NULL == p_filename) {
      g_p_filename = INI_FILENAME;
    }
    else {
      g_p_filename = p_filename;
    }
  }
  nvm_ini_load_dictionary(&gIni, g_p_filename);

  if (NULL == gIni)
  {
    return EFI_LOAD_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS preferences_uninit(void)
{
  nvm_ini_dump_to_file(gIni, g_p_filename, FALSE);
  nvm_ini_free_dictionary(gIni);
  gIni = NULL;
  return EFI_SUCCESS;
}

EFI_STATUS preferences_flush_the_file(void)
{
  nvm_ini_dump_to_file(gIni, g_p_filename, TRUE);
  return EFI_SUCCESS;
}

EFI_STATUS preferences_get_var_ascii(IN CONST char    *name,
	IN CONST EFI_GUID  guid,
	OUT VOID           *value,
	OUT UINTN          *size OPTIONAL)
{
	int val = nvm_ini_get_int_value(gIni, name, -1);
	if (-1 == val)
	{
		return EFI_NOT_FOUND;
	}
	if (1 == *size)
	{
		*(unsigned char*)value = (unsigned char)val;
	}
	else if (2 == *size)
	{
		*(unsigned short*)value = (unsigned short)val;
	}
	else if (4 == *size)
	{
		*(unsigned int*)value = (unsigned int)val;
	}
	else
	{
		return EFI_NOT_FOUND;
	}
	return EFI_SUCCESS;
}

EFI_STATUS preferences_get_string_ascii(IN CONST char    *name,
    IN CONST EFI_GUID  guid,
    IN UINTN           size,
    OUT VOID           *value)
{
    const char *ret_string = NULL;

    ret_string = nvm_ini_get_string(gIni, name);
    if (NULL == ret_string) 
    {
        return EFI_NOT_FOUND;
    }
    else if (AsciiStrLen(ret_string) > size)
    {
        return EFI_BUFFER_TOO_SMALL;
    }

    AsciiStrCpy(value, ret_string);

    return EFI_SUCCESS;
}

EFI_STATUS preferences_get_var(IN CONST CHAR16    *name,
  IN CONST EFI_GUID  guid,
  OUT VOID           *value,
  OUT UINTN          *size OPTIONAL)
{
	char tmp[256];
	UnicodeStrToAsciiStr(name, tmp);
	return preferences_get_var_ascii(tmp, guid, value, size);
}

EFI_STATUS preferences_get_var_string_wide(IN CONST CHAR16    *name,
  IN CONST EFI_GUID  guid,
  OUT CHAR16         *value,
  OUT UINTN          *size OPTIONAL)
{
	char key[256];
	const char * ascii_str;

	UnicodeStrToAsciiStr(name, key);
  if (NULL == (ascii_str = nvm_ini_get_string(gIni, (const char *)key)))
  {
    return EFI_NOT_FOUND;
  }
	AsciiStrToUnicodeStr(ascii_str, value);
	return EFI_SUCCESS;
}

EFI_STATUS preferences_set_var(IN CONST CHAR16 *name,
	IN CONST EFI_GUID guid, OUT VOID *value, OUT UINTN size)
{
	char key[256];
	char val[256];

	UnicodeStrToAsciiStr(name, key);

	if (1 == size)
	{
		snprintf(val, sizeof(val), "%d", *(unsigned char*)value);
	}
	else if (2 == size)
	{
		snprintf(val, sizeof(val), "%d", *(unsigned short*)value);
	}
	else if (4 == size)
	{
		snprintf(val, sizeof(val), "%d", *(unsigned int*)value);
	}
	else
	{
		return EFI_NOT_FOUND;
	}

	int ret = nvm_ini_set_value(gIni, key, val);
	if(0 != ret)
	{
		return EFI_LOAD_ERROR;
	}

	return EFI_SUCCESS;
}

EFI_STATUS preferences_set_var_string_wide(IN CONST CHAR16 *name,
	IN CONST EFI_GUID guid, IN CHAR16 *value)
{
	char key[256];
	char val[256];

	UnicodeStrToAsciiStr(name, key);
	UnicodeStrToAsciiStr(value, val);

  int ret = nvm_ini_set_value(gIni, key, val);
	if (0 != ret)
	{
		return EFI_LOAD_ERROR;
	}

	return EFI_SUCCESS;
}

EFI_STATUS preferences_set_var_string_ascii(IN CONST char *name,
	IN CONST EFI_GUID guid, IN const char *value)
{
	int ret = nvm_ini_set_value(gIni, name, value);
	if (0 != ret)
	{
		return EFI_LOAD_ERROR;
	}

  return EFI_SUCCESS;
}
