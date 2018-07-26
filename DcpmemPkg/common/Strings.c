/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "Strings.h"

CHAR8* string_concat(CHAR8* str1, CHAR8* str2, BOOLEAN free_when_done)
{

  CHAR8* retval = NULL;
  if (!str1 || ! str2)
  {
    Print(L"NULL string passed as input to string_concat\n");
    return NULL;
  }

  UINT64 len1 = string_length(str1);
  UINT64 len2 = string_length(str2);
  UINT64 size = (len1 + len2);

  retval = get_empty_string(size);

  MyMemCopy(retval, len1, str1);
  retval += len1;
  MyMemCopy(retval, len2, str2);
  retval -= len1;

  if (free_when_done)
  {
    FREE_POOL_SAFE(str1);
    FREE_POOL_SAFE(str2);
  }

  return retval;
}

CHAR8* string_array_concat(CHAR8** str_array, UINT64 string_count, BOOLEAN free_when_done, UINT64* final_length)
{
  CHAR8* retval = NULL;
  CHAR8* head = NULL;
  UINT64 x = 0;
  UINT64 strlen = 0;
  *final_length = 0;
  for (x = 0; x < string_count; x++)
  {
    *final_length += string_length(str_array[x]);
  }

  retval = get_empty_string(*final_length);
  head = retval;

  for (x = 0; x < string_count; x++)
  {
    strlen = string_length(str_array[x]);
    MyMemCopy(head, strlen, str_array[x]);
    head += strlen;
  }

  if (free_when_done && NULL != str_array)
  {
    for (x = 0; x < string_count; x++)
    {
      if (NULL == str_array[x]) continue;
      FREE_POOL_SAFE(str_array[x]);
    }
    FREE_POOL_SAFE(str_array);
  }

  return retval;
}

UINT64 string_length(CHAR8* str)
{
  UINT64 len = 0;
  if (!str)
  {
    return len;
  }

  while (str[len])
  {
    len++;
  }

  return len;
}

CHAR8** string_split(CHAR8* str, CHAR8 splitchar, UINT64 maxelements, UINT64* elements)
{
  CHAR8** retval = NULL;

  if (!str)
  {
    *elements = 0;
    return retval;
  }

  CHAR8** str_heads = NULL;
  UINT64* string_lengths = NULL;
  CHAR8* str_head = str;
  UINT64 len = 0;
  UINT32 x = 0;
  *elements = 1;

  //count the number of substrings, up to maxelements (unless it's <= 0)
  while (*str_head)
  {
    if (*str_head == splitchar)
    {
      *elements = *elements + 1;
      if (maxelements > 1 && *elements >= maxelements)
      {
        break;
      }
    }

    str_head++;
  }

  //allocate room for the return strings and metadata
  retval = AllocateZeroPool(*elements * sizeof(CHAR8*));
  if (NULL == retval)
  {
    Print(L"Unable to allocate memory for new string\n");
    goto Finish;
  }

  str_heads = AllocateZeroPool(*elements * sizeof(CHAR8*));
  if (NULL == str_heads)
  {
    Print(L"Unable to allocate memory for new string\n");
    goto Finish;
  }
  string_lengths = AllocateZeroPool(*elements * sizeof(UINT32*));
  if (NULL == string_lengths)
  {
    Print(L"Unable to allocate memory for new string\n");
    goto Finish;
  }
  for (; x < *elements; x++)
  {
    retval[x] = NULL;
    str_heads[x] = NULL;
    string_lengths[x] = 0;
  }

  x = 0;
  str_head = str;
  str_heads[x] = str_head;

  while (*str_head)
  {
    if (*str_head == splitchar)
    {
      x++;
      str_head++; //skip the split char
      str_heads[x] = str_head;
      if (x == ((*elements) - 1)) //Get the length of the rest of the string
      {
        while(*str_head)
        {
          string_lengths[x]++;
          str_head++;
        }

        break;
      }

      continue;
    }

    string_lengths[x]++;
    str_head++;
  }

  x = 0;
  for (; x < *elements; x++)
  {
    str_head = str_heads[x];
    len = string_lengths[x];
    if (len == 0)
    {
      retval[x] = NULL;
      continue;
    }

    retval[x] = get_empty_string(len);
    MyMemCopy(retval[x], len, str_head);
  }

Finish:

  if (NULL != str_heads)
  {
    FREE_POOL_SAFE(str_heads);
  }

  if (NULL != string_lengths)
  {
    FREE_POOL_SAFE(string_lengths);
  }

  return retval;
}

UINT32 a_to_u32(CHAR8* str)
{
  UINT32 retval = 0;

  if (!str)
  {
    return retval;
  }

  UINT32 x = 0;
  while (str[x])
  {
    retval = retval * 10;
    UINT8 c = 0x7f & str[x];
    retval += (c - 48);
    x++;
  }

  return retval;
}

UINT32 bytes_to_u32(UINT8* bytes)
{
  UINT32 retval = 0;
  if (!bytes)
  {
    return retval;
  }

  UINT32 x0 = ((UINT32)bytes[0]);
  UINT32 x1 = ((UINT32)bytes[1]) << 8;
  UINT32 x2 = ((UINT32)bytes[2]) << 16;
  UINT32 x3 = ((UINT32)bytes[3]) << 24;
  retval = (UINT32)x0 + (UINT32)x1 + (UINT32)x2 + (UINT32)x3;

  return retval;
}

CHAR8* string_copy(CHAR8* source)
{
  CHAR8* retval = NULL;
  if (!source)
  {
    return retval;
  }

  UINT64 len = 0;

  if (source)
  {
    len = string_length(source);
  }

  retval = get_empty_string(len);
  if (source)
  {
    MyMemCopy(retval, len, source);
  }

  return retval;
}

CHAR8* get_empty_string(UINT64 length)
{
  UINT64 bytes = (length * sizeof(CHAR8)) + sizeof(CHAR8);
  CHAR8* retval = AllocateZeroPool(bytes);
  if (NULL == retval)
  {
    Print(L"Unable to allocate memory for new string\n");
    return NULL;
  }

  return retval;
}

CHAR8*
nlog_format(
  CHAR8  *format,
  UINT32** values,
  UINT64 values_length
)
{
  UINT64 max_format_code_len = 10;
  UINT64 str_len = string_length(format);
  UINT64 format_len = 0;
  UINT64 current_value_index = 0;
  UINT64 chars_per_int = 15;
  UINT32 current_value = 0;


  if (NULL == values || !values_length)
  {
    return string_copy(format);
  }

  str_len = str_len + (values_length * chars_per_int);


  CHAR8* retval = get_empty_string(str_len);
  CHAR8* buffer = get_empty_string(max_format_code_len); //to hold format modifiers
  CHAR8* zero_buffer = get_empty_string(max_format_code_len);
  CHAR8* value_buffer = NULL;
  CHAR8* format_head = format;
  CHAR8* retval_head = retval;
  CHAR8* format_start;

  format_start = format_head;
  str_len = 0;
  while (*format_head)
  {
    if (*format_head != '%')
    {
      str_len++;
      format_head++;
      continue;
    }

    if (str_len)
    {
      MyMemCopy(retval_head, str_len, format_start);
      retval_head += str_len;
      format_head++; //consume the %
      format_start = format_head;
    }

    //clean out the format buffer
    MyMemCopy(buffer, max_format_code_len, zero_buffer);

    str_len = 0;
    while (*format_head &&
      *format_head != 'X' &&
      *format_head != 'x'&&
      *format_head != 'd' &&
      *format_head != 'u')
    {
      format_head++;
      str_len++;
    }

    //figure out any length modifiers between the % and the X/x/d/u
    format_len = 0;
    if (str_len)
    {
      if (str_len < max_format_code_len)
      {
        MyMemCopy(buffer, str_len, format_start);
        if (string_length(buffer) > 0)
        {
          format_len = a_to_u32(buffer);
        }
      }
      else
      {
        //something bad... malformed request. Ignore.
      }
    }

    current_value = *values[current_value_index];
    current_value_index++;
    if (*format_head == 'X')
    {
      value_buffer = u32_to_a(current_value, TRUE, format_len, TRUE);
    }
    else if (*format_head == 'x')
    {
      value_buffer = u32_to_a(current_value, TRUE, format_len, FALSE);
    }
    else if (*format_head == 'd')
    {
      value_buffer = u32_to_a(current_value, FALSE, format_len, FALSE);
    }
    else if (*format_head == 'u')
    {
      value_buffer = u32_to_a(current_value, FALSE, format_len, FALSE);
    }

    if (format_len > 0)
    {
      str_len = string_length(value_buffer);
      if (format_len > 0 && str_len < format_len)
      {
        value_buffer = pad_left(value_buffer, format_len, '0', TRUE);
      }
    }

    str_len = string_length(value_buffer);
    MyMemCopy(retval_head, str_len, value_buffer);
    retval_head += str_len;
    FREE_POOL_SAFE(value_buffer);

    format_head++; //consume the format char
    format_start = format_head;
    str_len = 0;
  }

  if (str_len && format_start)
  {
    MyMemCopy(retval_head, str_len, format_start);
  }

  FREE_POOL_SAFE(buffer);
  FREE_POOL_SAFE(zero_buffer);

  return retval;
}

CHAR8* u32_to_a(
  UINT32 val,
  BOOLEAN format_hex,
  UINT64 max_len,
  BOOLEAN uppercase
)
{

  if (val == 0)
  {
    return string_copy("0");
  }

  CHAR8* volatile retval = NULL;
  CHAR8* volatile int_str = NULL;
  CHAR8* volatile int_str_head = NULL;
  CHAR8* volatile int_str_ptr = NULL;
  CHAR8* new_retval = NULL;
  UINT64 trim = 0;
  UINT32 base = 10;
  UINT64 len = 0;
  UINT32 cntr = 0;
  UINT32 current_val;

  if (format_hex)
  {
    base = 16;
  }

  if (16 == base)
  {
    len = 8;
  }
  else if (10 == base)
  {
    len = 10;
  }

  int_str = get_empty_string(len);
  int_str_ptr = int_str + (len - 1);

  cntr = 0;
  int_str_head = int_str;
  while (cntr < len)
  {
    cntr++;
    *int_str_head = '0';
    int_str_head++;
  }

  while (val > 0)
  {
    current_val = (val % base);
    len++;
    if (current_val >= 0 && current_val <= 9)
    {
      *int_str_ptr = (CHAR8)(current_val + (UINT32)'0');
    }
    else
    {
      if (uppercase)
      {
        *int_str_ptr = (CHAR8)((current_val - 10) + (UINT32)'A');
      }
      else
      {
        *int_str_ptr = (CHAR8)((current_val - 10) + (UINT32)'a');
      }
    }

    int_str_ptr--;
    val = val / base;
  }

  int_str_ptr++;
  len--;
  retval = get_empty_string(len);
  MyMemCopy(retval, len, int_str_ptr);
  FREE_POOL_SAFE(int_str);

  if (0 < max_len)
  {
    len = string_length(retval);

    if (len > max_len)
    {
      trim = len - max_len;
      new_retval = get_empty_string(max_len);
      MyMemCopy(new_retval, max_len, (retval + trim));
      FREE_POOL_SAFE(retval);
      retval = new_retval;
    }
    else if (len < max_len)
    {
      retval = pad_left(retval, max_len, '0', TRUE);
    }
  }

  return retval;
}

CHAR8*
pad_left(
  CHAR8  *str,
  UINT64 pad_len,
  CHAR8 pad_char,
  BOOLEAN free_when_done
)
{
  CHAR8* volatile retval = NULL;
  UINT64 x = 0;
  if (NULL == str)
  {
    return retval;
  }

  UINT64 current_length = string_length(str);
  if (current_length >= pad_len)
  {
    retval = string_copy(str);
  }
  else
  {
    pad_len = pad_len - current_length;
    retval = get_empty_string(pad_len + current_length);
    x = 0;
    for (; x < pad_len; x++)
    {
      retval[x] = pad_char;
    }

    MyMemCopy((retval + pad_len), current_length, str);
  }

  if (free_when_done)
  {
    FREE_POOL_SAFE(str);
  }

  return retval;
}

CHAR8*
MyMemCopy(
  CHAR8* dest,
  UINT64 len,
  CHAR8* source
)
{

  CHAR8* volatile s = source;
  CHAR8* volatile d = dest;

  if (d && s)
  {
    UINT64 x = 0;
    for (; x < len; x++)
    {
      d[x] = s[x];
    }
  }

  return d;
}