/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef __PREFERENCES_H__
#define __PREFERENCES_H__

#include <UefiBaseType.h>

EFI_STATUS preferences_init(void);
EFI_STATUS preferences_uninit(void);

EFI_STATUS preferences_get_var_ascii(
	IN CONST char *name,
	IN CONST EFI_GUID  guid,
	OUT VOID  *value,
	OUT UINTN  *size OPTIONAL);

EFI_STATUS preferences_get_string_ascii(
    IN CONST char      *name,
    IN CONST EFI_GUID   guid,
    IN UINTN            size,
    OUT VOID           *value);

EFI_STATUS preferences_get_var(
	IN CONST CHAR16 *name,
	IN CONST EFI_GUID guid,
	OUT VOID *value,
	OUT UINTN *size OPTIONAL);

EFI_STATUS preferences_set_var(
	IN CONST CHAR16 *name,
	IN CONST EFI_GUID guid,
	OUT VOID *value,
	OUT UINTN size);

EFI_STATUS preferences_set_var_string_wide(IN CONST CHAR16 *name,
	IN CONST EFI_GUID guid, IN CHAR16 *value);

EFI_STATUS preferences_get_var_string_wide(IN CONST CHAR16    *name,
	IN CONST EFI_GUID  guid,
	OUT CHAR16         *value,
	OUT UINTN          *size OPTIONAL);

EFI_STATUS preferences_set_var_string_ascii(IN CONST char *name,
	IN CONST EFI_GUID guid, IN const char *value);
#endif // __PREFERENCES_H__
