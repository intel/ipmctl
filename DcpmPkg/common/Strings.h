/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _STRINGS_H_
#define _STRINGS_H_

#include <Debug.h>
#include "Common.h"

 /**
string_concat command

@param[in] str1  - string 1
@param[in] str2  - string 2
@param[in] free_when_done  - if it should release the memory of the strings when done

@retval the concatenated string
**/
CHAR8* string_concat(
  IN CHAR8* str1,
  IN CHAR8* str2,
  IN BOOLEAN free_when_done
);

/**
string concatenate command - THIS FREES THE PASSED STRINGS WHEN FINISHED

@param[in] str_array  - string array
@param[in] string_count  - the length of the string array
@param[in] free_when_done  - if it should release the memory of the strings when done
@param[out] final_length  - if it should release the memory of the strings when done

@retval the concatenated string
**/
CHAR8* string_array_concat(
  IN CHAR8** str_array,
  IN UINT64 string_count,
  IN BOOLEAN free_when_done,
  OUT UINT64* final_length
);

/*
string copy command

@param[in] source - the source string

@retval a copy of the string
*/
CHAR8* string_copy(
  IN CHAR8* source
);

/*
get_empty_string command

@param[in] length - how long of a string that you want

@retval a string [length] long + terminator, initialized with 0 in each index
*/
CHAR8* get_empty_string(
  IN UINT64 length
);

/*
string_length command

@param[in] str - the string to measure

@retval the length of the passed string
*/
UINT64 string_length(
  IN  CHAR8* str
);


/*
string_split command

@param[in] str - the string to split
@param[in] splitchar - the char to split on
@param[in] maxelements - 0 for unlimited, or > 0 to prevent too many resulting strings
@param[out] elements - the number of found elements

@retval an array of strings [elements] long
*/
CHAR8** string_split(
  IN CHAR8* str,
  IN CHAR8 splitchar,
  IN UINT64 maxelements,
  OUT UINT64* elements
);

/*
bytes_to_u32 command

@param[in] bytes - the bytes to convert (must be 4 or more)

@retval the UINT32 value of the passed string
*/
UINT32 bytes_to_u32(
  IN UINT8* bytes
);

/*
a_to_u32 command

@param[in] str - the string to convert

@retval the UINT32 value of the passed string
*/
UINT32 a_to_u32(
  IN CHAR8* str
);

/*
u32_to_a command

@param[in] val - the value to convert
@param[in] format_hex - if the output should be hex
@param[in] max_len - the max length of string to return
@param[in] uppercase - if upper case values should be used when hex

@retval the string representation of the passed value
*/
CHAR8* u32_to_a(
  IN UINT32 val,
  IN BOOLEAN format_hex,
  IN UINT64 max_len,
  IN BOOLEAN uppercase
);

/*
nlog_format command

@param[in] format - the string to base a format on (processes %X, %x and %d only)
@param[in] values - the values to add to the format str
@param[in] values_length - the length of the valeus array

@retval returns the formatted string
*/
CHAR8*
nlog_format(
  IN CHAR8  *format,
  IN UINT32** values,
  IN UINT64 values_length
);

/*
Pads a string AND FREES THE PASSED ONE
@param[in] str - the string to pad
@param[in] pad_len - the length the final string should be
@param[in] pad_char - the char to use for padding
@param[in] free_when_done  - if it should release the memory of the strings when done

@retval returns the padded string
*/
CHAR8*
pad_left(
  IN CHAR8  *str,
  IN UINT64 pad_len,
  IN CHAR8 pad_char,
  IN BOOLEAN free_when_done
);


/*
Copys one memory range's value to another
@param[in] dest - the string to pad
@param[in] len - the char to use for padding
@param[in] source  - if it should release the memory of the strings when done

@retval returns a reference to the dest
*/
CHAR8*
MyMemCopy(
  IN CHAR8* dest,
  IN UINT64 len,
  IN CHAR8* source
);
#endif
