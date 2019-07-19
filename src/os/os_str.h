/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OS_STR_H_
#define	OS_STR_H_

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#ifdef _MSC_VER
#include <io.h>
#include <conio.h>
#include <time.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

int os_sscanf(
    const char *buffer,
    const char *format,
    ... );

int os_snprintf(
    char *s,
    size_t len,
    const char *format,
    ...);

int os_swprintf(
    wchar_t *ws,
    size_t len,
    const wchar_t *format,
    ...);

int os_vsnprintf(
    char *s,
    size_t len,
    const char *format,
    va_list arg);

int os_vswprintf(
    wchar_t *ws,
    size_t len,
    const wchar_t *format,
    va_list arg);

size_t os_strnlen(
    const char *s,
    size_t maxlen);

size_t os_wcsnlen(
    const wchar_t *s,
    size_t maxlen);

int os_strcpy(
    char *dest,
    size_t destSize,
    const char *source);

int os_wcscpy(
    wchar_t *dest,
    size_t destSize,
    const wchar_t *source);

int os_wcsncpy(
    wchar_t *dest,
    size_t destSize,
    const wchar_t *source,
    size_t count);

int os_strcat(
    char *dest,
    size_t destSize,
    const char *source);

int os_strncat(
    char *dest,
    size_t destSize,
    const char *source,
    size_t count);

int os_wcsncat(
    wchar_t *dest,
    size_t destSize,
    const wchar_t *source,
    size_t count);

int os_memcpy(
    void *dest,
    size_t destSize,
    const void *src,
    size_t count);

char *os_strtok(
    char *str,
    const char *delim,
    char **ptr);

wchar_t *os_wcstok(
    wchar_t *str,
    const wchar_t *delim,
    wchar_t **ptr);

int os_fopen(
    FILE **streamptr,
    const char *filename,
    const char *mode);

#ifdef __cplusplus
}
#endif

#endif //OS_STR_H_
