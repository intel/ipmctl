/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Base.h>
#include "os_str.h"

#ifdef __cplusplus
extern "C"
{
#endif


int os_sscanf(
    const char *buffer,
    const char *format,
    ... )
{
  int result = 0;
  va_list arg;

  va_start(arg, format);
#ifdef _MSC_VER
  result = vsscanf_s(buffer, format, arg);
#else
  result = vsscanf(buffer, format, arg);
#endif
  va_end(arg);

  return result;
}

int os_snprintf(
    char *s,
    size_t len,
    const char *format,
    ...)
{
  int result = 0;
  va_list arg;

  va_start(arg, format);
#ifdef _MSC_VER
  result = vsprintf_s(s, len, format, arg);
#else
  result = vsnprintf(s, len, format, arg);
#endif
  va_end(arg);

  return result;
}

int os_swprintf(
    wchar_t *ws,
    size_t len,
    const wchar_t *format,
    ...)
{
  int result = 0;
  va_list arg;

  va_start(arg, format);
#ifdef _MSC_VER
  result = vswprintf_s(ws, len, format, arg);
#else
  result = vswprintf(ws, len, format, arg);
#endif
  va_end(arg);

  return result;
}

int os_vsnprintf(
    char *s,
    size_t len,
    const char *format,
    va_list arg)
{
#ifdef _MSC_VER
  return vsprintf_s(s, len, format, arg);
#else
  return vsnprintf(s, len, format, arg);
#endif
}

int os_vswprintf(
    wchar_t *ws,
    size_t len,
    const wchar_t *format,
    va_list arg)
{
#ifdef _MSC_VER
  return vswprintf_s(ws, len, format, arg);
#else
  return vswprintf(ws, len, format, arg);
#endif
}

size_t os_strnlen(
    const char *s,
    size_t maxlen)
{
#ifdef _MSC_VER
  return strnlen_s(s, maxlen);
#else
  return strnlen(s, maxlen);
#endif
}

size_t os_wcsnlen(
    const wchar_t *s,
    size_t maxlen)
{
#ifdef _MSC_VER
  return wcsnlen_s(s, maxlen);
#else
  return wcsnlen(s, maxlen);
#endif
}

int os_strcpy(
    char *dest,
    size_t destSize,
    const char *source)
{
#ifdef _MSC_VER
  return strcpy_s(dest, destSize, source);
#else
  strncpy(dest, source, destSize);
  dest[destSize - 1] = '\0'; // in case when strlen(source) >= destSize
  return 0;
#endif
}

int os_wcscpy(
    wchar_t *dest,
    size_t destSize,
    const wchar_t *source)
{
#ifdef _MSC_VER
  return wcscpy_s(dest, destSize, source);
#else
  wcsncpy(dest, source, destSize);
  dest[destSize - 1] = L'\0'; // in case when wcslen(source) >= destSize
  return 0;
#endif
}

int os_wcsncpy(
    wchar_t *dest,
    size_t destSize,
    const wchar_t *source,
    size_t count)
{
#ifdef _MSC_VER
  return wcsncpy_s(dest, destSize, source, count);
#else
  if (count < destSize) {
    wcsncpy(dest, source, count);
    dest[count] = L'\0'; // in case when wcslen(source) >= count
    return 0;
  } else {
    return -1;
  }
#endif
}

int os_strcat(
    char *dest,
    size_t destSize,
    const char *source)
{
#ifdef _MSC_VER
  return strcat_s(dest, destSize, source);
#else
  int remainingSize = destSize - strnlen(dest, destSize);
  strncat(dest, source, remainingSize - 1);
  return 0;
#endif
}

int os_strncat(
    char *dest,
    size_t destSize,
    const char *source,
    size_t count)
{
#ifdef _MSC_VER
  return strncat_s(dest, destSize, source, count);
#else
  if (strnlen(dest, destSize) + count < destSize) {
    strncat(dest, source, count);
    return 0;
  } else {
    return -1;
  }
#endif
}

int os_wcsncat(
    wchar_t *dest,
    size_t destSize,
    const wchar_t *source,
    size_t count)
{
#ifdef _MSC_VER
  return wcsncat_s(dest, destSize, source, count);
#else
  if (wcsnlen(dest, destSize) + count < destSize) {
    wcsncat(dest, source, count);
    return 0;
  } else {
    return -1;
  }
#endif
}

int os_memcpy(
    void *dest,
    size_t destSize,
    const void *src,
    size_t count)
{
#ifdef _MSC_VER
  return memcpy_s(dest, destSize, src, count);
#else
  int status = 0;
  if (destSize < count) {
    count = destSize;
    status = -1;
  }
  memcpy(dest, src, count);
  return status;
#endif
}

char *os_strtok(
    char *str,
    const char *delim,
    char **ptr)
{
#ifdef _MSC_VER
  return strtok_s(str, delim, ptr);
#else
  return strtok_r(str, delim, ptr);
#endif
}

wchar_t *os_wcstok(
    wchar_t *str,
    const wchar_t *delim,
    wchar_t **ptr)
{
#ifdef _MSC_VER
  return wcstok_s(str, delim, ptr);
#else
  return wcstok(str, delim, ptr);
#endif
}

int os_fopen(
    FILE **streamptr,
    const char *filename,
    const char *mode)
{
#ifdef _MSC_VER
  return fopen_s(streamptr, filename, mode);
#else
  if (streamptr) {
    *streamptr = fopen(filename, mode);
    return (*streamptr) ? 0 : -1;
  } else {
    return -1;
  }
#endif
}

#ifdef __cplusplus
}
#endif
