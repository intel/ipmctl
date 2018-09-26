/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the implementation of various safe string functions
 * that should be used instead of the built in ones (i.e. strcpy, strncpy, ...)
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

#include "s_str.h"


char *s_strrchr(char *str, char ch, int max_len)
{
	char *last_char = NULL;

	if (str)
	{
		int i = 0;
		while ((i < max_len) && (str[i] != '\0'))
		{
			if (str[i] == ch)
			{
				last_char = &str[i];
			}
			i++;
		}
	}

	return last_char;
}

/*
 * searches a string for the null terminator up to the max_len passed
 */
size_t s_strnlen(const char *str, size_t max_len)
{
	size_t i = 0;
	if (str)
	{
		while ((i < max_len) && (str[i] != '\0'))
			i++;
	}

	return i;
}

/*
 * safe concatenation of a string.
 */
char *s_strncat(char *dst, size_t dst_size, const char *src, size_t src_size)
{
	if (dst && src && (dst_size != 0) && (src_size != 0))

	{
		size_t dst_i = s_strnlen(dst, dst_size); // current length
		int free_dst_chars = ((int)dst_size - 1) - (int)dst_i; // leave room for null terminator
		if (free_dst_chars > 0)
		{
			// handle possible size difference between src and dst
			int chars_to_copy = ((int)src_size < free_dst_chars) ? (int)src_size : free_dst_chars;
			int src_i = 0;
			while ((src_i < chars_to_copy) && ((dst[dst_i] = src[src_i]) != '\0'))
			{
				dst_i++;
				src_i++;
			}
			dst[dst_i] = '\0';
		}
	}

	return dst;
}

/*
 * safe concatenation of a string.
 */
char *s_strcat(char *dst, size_t dst_size, const char *src)
{
	if (dst && src && (dst_size != 0))
	{
		size_t dst_i = s_strnlen(dst, dst_size); // current length
		int free_dst_chars = ((int)dst_size - 1) - (int)dst_i; // leave room for null terminator
		if (free_dst_chars > 0)
		{
			int src_i = 0;
			while ((src_i < free_dst_chars) && ((dst[dst_i] = src[src_i]) != '\0'))
			{
				dst_i++;
				src_i++;
			}
			dst[dst_i] = '\0';
		}
	}

	return dst;
}

/*
 * safe copy of a string.
 */
char *s_strncpy(char *dst, size_t dst_size, const char *src, size_t src_size)
{
	if (dst && src && ((int)dst_size != 0))
	{
		int chars_to_copy = (int)dst_size - 1; // leave 1 for null terminator
		chars_to_copy = ((int)src_size < chars_to_copy) ? (int)src_size : chars_to_copy;

		int i = 0;
		while ((i < chars_to_copy) && ((dst[i] = src[i]) != '\0'))
		{
			i++;
		}

		dst[i] = '\0';
	}

	return dst;
}

/*
 * safe copy of a wide string.
 */
wchar_t *ws_strcpy(wchar_t *dst, const wchar_t *src, size_t dst_size)
{
	if (dst && src && (dst_size != 0))
	{
		wchar_t *end = &dst[dst_size - 1];
		while ((dst < end) && (*src != '\0'))
		{
			*dst++ = *src++;
		}
		*dst = '\0';
	}

	return dst;
}

/*
 * safe copy of a string.
 */
char *s_strcpy(char *dst, const char *src, size_t dst_size)
{
	if (dst && src && (dst_size != 0))
	{
		char *end = &dst[dst_size - 1];
		while ((dst < end) && (*src != '\0'))
		{
			*dst++ = *src++;
		}
		*dst = '\0';
	}

	return dst;
}

/*
 * convert a char to int
 */
int todigit(char c)
{
	return c - '0';
}


/*
 * safe string of digits to unsigned char.  Will ignore all non digits
 * until it finds the first digit. Then it will consume all base-10
 * numeric characters. In the event of failure the number of consumed
 * characters returned will be less than expected.
 */
size_t s_digitstrtouc(const char *const str, size_t str_len, const char **pp_end,
		unsigned char *p_result)
{
	// init some vars
	size_t str_i = 0;

	if (str && (str_len != 0) && p_result)
	{
		// index through all non-digits until first digit is hit
		while ((str_i < str_len) && !isdigit(str[str_i]) && (str[str_i] != '\0'))
		{
			str_i++;
		}

		// convert a consecutive digit sequence starting at the most significant digit
		*p_result = 0;
		while ((str_i < str_len) && isdigit(str[str_i]))
		{
			// up-cast result to int for robust comparison (next)
			unsigned int tmp_result = (((int)(*p_result)) * 10) + todigit(str[str_i]);

			// end before number becomes greater than what a unsigned short can represent
			if (tmp_result <= UCHAR_MAX)
			{
				*p_result = (unsigned char)tmp_result;
				str_i++;
			}
			else
			{
				break;
			}
		}

		if (pp_end)
		{
			// set p_end to NULL if we have consumed the entire buffer, so that we
			// do not point outside of the buffer
			*pp_end = (str_i < str_len) ? &(str[str_i]) : NULL;
		}
	}

	return str_i;
}

/*
 * safe string to unsigned short int.  Will ignore all non digits
 * until it finds the first digit. Then it will consume all base-10
 * numeric characters. In the event of failure the number of consumed
 * characters returned will be less than expected.
 */
size_t s_strtous(const char *const str, size_t str_len, const char **pp_end,
		unsigned short *p_result)
{
	// init some vars
	size_t str_i = 0;

	if (str && (str_len != 0) && p_result)
	{
		// index through all non-digits until first digit is hit
		while ((str_i < str_len) && !isdigit(str[str_i]) && (str[str_i] != '\0'))
		{
			str_i++;
		}

		// convert a consecutive digit sequence starting at the most significant digit
		*p_result = 0;
		while ((str_i < str_len) && isdigit(str[str_i]))
		{
			// up-cast result to unsigned int for robust comparison (next)
			unsigned int tmp_result = (((int)(*p_result)) * 10) + todigit(str[str_i]);

			// end before number becomes greater than what a unsigned short can represent
			if (tmp_result <= USHRT_MAX)
			{
				*p_result = (unsigned short)tmp_result;
				str_i++;
			}
			else
			{
				break;
			}
		}

		if (pp_end)
		{
			// set p_end to NULL if we have consumed the entire buffer, so that we
			// do not point outside of the buffer
			*pp_end = (str_i < str_len) ? &(str[str_i]) : NULL;
		}
	}

	return str_i;
}

/*
 * safe string to unsigned int.  Will ignore all non digits
 * until it finds the first digit. Then it will consume all base-10
 * numeric characters. In the event of failure the number of consumed
 * characters returned will be less than expected.
 */
size_t s_strtoui(const char *const str, size_t str_len, const char **pp_end,
		unsigned int *p_result)
{
	// init some vars
	size_t str_i = 0;

	if (str && (str_len != 0) && p_result)
	{
		// index through all non-digits until first digit is hit
		while ((str_i < str_len) && !isdigit(str[str_i]) && (str[str_i] != '\0'))
		{
			str_i++;
		}

		// convert a consecutive digit sequence starting at the most significant digit
		*p_result = 0;
		while ((str_i < str_len) && isdigit(str[str_i]))
		{
			// up-cast result to unsigned long long for robust comparison (next)
			unsigned long long tmp_result =
					(((unsigned long long)(*p_result)) * 10) + todigit(str[str_i]);
			// end before number becomes greater than what a unsigned short can represent
			if (tmp_result <= UINT_MAX)
			{
				*p_result = (unsigned int)tmp_result;
				str_i++;
			}
			else
			{
				break;
			}
		}

		if (pp_end)
		{
			// set p_end to NULL if we have consumed the entire buffer, so that we
			// do not point outside of the buffer
			*pp_end = (str_i < str_len) ? &(str[str_i]) : NULL;
		}
	}

	return str_i;
}

/*
 * safe string to unsigned long long.  Will ignore all non digits
 * until it finds the first digit. Then it will consume all base-10
 * numeric characters. In the event of failure the number of consumed
 * characters returned will be less than expected.
 */
size_t s_strtoull(const char *const str, size_t str_len, const char **pp_end,
		unsigned long long *p_result)
{
	// init some vars
	size_t str_i = 0;

	if (str && (str_len != 0) && p_result)
	{
		// index through all non-digits until first digit is hit
		while ((str_i < str_len) && !isdigit(str[str_i]) && (str[str_i] != '\0'))
		{
			str_i++;
		}

		// convert a consecutive digit sequence starting at the most significant digit
		*p_result = 0;
		while ((str_i < str_len) && isdigit(str[str_i]))
		{
			unsigned long long tmp_result;

			// We have to do this little dance around ULLONG_MAX because
			// we have no way to store a number larger than ULLONG_MAX
			// So we check to see if multiplying our current result by ten
			// will overflow and, if not, multiply by ten and see if adding
			// the next digit will overflow
			if (*p_result * 10 >= *p_result)
			{
				tmp_result = *p_result * 10;
			}
			else
			{
				break;
			}

			if (tmp_result + todigit(str[str_i]) >= tmp_result)
			{
				*p_result = tmp_result + todigit(str[str_i]);
				str_i++;
			}
			else
			{
				break;
			}
		}

		if (pp_end)
		{
			// set p_end to NULL if we have consumed the entire buffer, so that we
			// do not point outside of the buffer
			*pp_end = (str_i < str_len) ? &(str[str_i]) : NULL;
		}
	}

	return str_i;
}

/*
 * safe copy of a string, omitting null terminator
 */
char *s_strncpy_unterm(char *dst, size_t dst_size, const char *src, size_t src_size)
{
	int i = 0;
	if (src && dst)
	{
		int chars_to_copy = (int)dst_size < (int)src_size ? (int)dst_size : (int)src_size;
		while ((i < chars_to_copy) && (src[i] != '\0'))
		{
			dst[i] = src[i];
			i++;
		}
	}

	return dst;
}

/*
 * safe case-insensitive string comparison
 */
int s_strncmpi(const char *const str1, const char *const str2, size_t size)
{
	int ret = -1;

	if (str1 && str2 && (size != 0))
	{
		ret = 1;
		unsigned int idx = 0;
		while ((idx < size) && (toupper(str1[idx]) == toupper(str2[idx])))
		{
			idx++;
		}

		if (idx == size)
		{
			// return zero if the entire string matches up to the input size
			ret = 0;
		}
		else
		{
			// return the (+1) index of the difference
			ret = idx + 1;
		}
	}

	return ret;
}

/*
 * safe case-sensitive string comparison
 */
int s_strncmp(const char *const str1, const char *const str2, size_t size)
{
	int ret = -1;

	if (str1 && str2 && (size != 0))
	{
		ret = 1;
		unsigned int idx = 0;
		while ((idx < size) && (str1[idx] == str2[idx]))
		{
			idx++;
		}

		if (idx == size)
		{
			// return zero if the entire string matches up to the input size
			ret = 0;
		}
		else
		{
			// return the (+1) index of the difference
			ret = idx + 1;
		}
	}

	return ret;
}
/*
 * Function used to trim trailing white spaces from the given string.
 * The parameter passed to this function is assumed to be a null-terminated
 * string. If a non null-terminated string is passed then
 * the last character will be replaced with '\0'
 */
void s_strtrim_right(char *s, unsigned int len)
{
	if (s && (len != 0))
	{
		// make *ptr point to the first '\0' in the parameter s of length len
		// if memchr does not return NULL, then the parameter 's' is a null-terminated string
		char *ptr = (char *)memchr(s, 0, len);

		if (ptr == NULL)
		{
			// if the parameter 's' is a non null-terminated string then the string
			// is null-terminated and the last character in the string 's' is removed
			ptr = s + len - 1;
			*ptr = '\0';
		}

		ptr--;

		// scan the string 's' backwards to find all the whitespaces
		while ((ptr >= s) && isspace(*ptr))
		{
			ptr--;
		}

		// Null-terminate the string to remove the trailing whitespaces
		*(ptr + 1) = '\0';
	}
}

/*
 * Function used to trim leading white spaces from the given string.
 * The string passed in will have all leading whitespace removed.  If the
 * string is not null terminated, a '\0' will be placed at the end of the
 * new string, with respect to the given string length
 */
void s_strtrim_left(char *s, unsigned int len)
{
	if (s && (len != 0))
	{
		// start at beginning of string to find out how many leading spaces there are
		unsigned int src_idx = 0;
		while ((src_idx < len) && (s[src_idx] != '\0'))
		{
			if (isspace(s[src_idx]))
			{
				src_idx++;
			}
			else
			{
				break;
			}
		}

		// begin replacement
		unsigned int dst_idx = 0;
		while ((src_idx < len) && (s[src_idx] != '\0'))
		{
			s[dst_idx] = s[src_idx];
			dst_idx++;
			src_idx++;
		}

		// terminate appropriately
		if ((src_idx == len) && (dst_idx != 0))
		{
			// this case means that the src was not null terminated,
			// *and* there was something remaining after the trim
			s[dst_idx - 1] = '\0';
		}
		else
		{
			// this case means that there was null termination,
			// *or* there was nothing left after the trim
			s[dst_idx] = '\0';
		}
	}
}

/*
 * Function used to remove whitespace from both sides of a string.
 * If the string is not null terminated, it will force null termination
 * no later in the string than with respect to the given length of the string
 */
void s_strtrim(char *s, unsigned int len)
{
	if (s && (len != 0))
	{
		s_strtrim_left(s, len);
		s_strtrim_right(s, len);
	}
}

/*
 * Function used to replace all instances of a target_char within the src_str,
 * and put that result into a new char*.  Replacement includes all chars, up-to
 * but not including, a null terminator;
 */
int s_strrep_char(char *dst_str, size_t dst_size, const char *src_str, size_t src_size,
		const char target_char, const char *rep_str, size_t rep_size)
{
	int rc = -1;

	size_t dst_idx = 0;
	size_t src_idx = 0;
	// do until entire src_str is examined, or until max chars copied, leaving room for null-term
	for (src_idx = 0; ((src_idx < src_size) && (dst_idx < dst_size - 1)); src_idx++)
	{
		// check replacement
		if (src_str[src_idx] == target_char)
		{
			// check to see if we have room in our dst_str for replacement
			if (dst_idx + rep_size < dst_size)
			{
				// replace
				for (size_t rep_idx = 0; rep_idx < rep_size; rep_idx++)
				{
					dst_str[dst_idx] = rep_str[rep_idx];
					dst_idx++;
				}
			}
			else // wont fit when null-terminated
			{
				// return number of src_chars processed and break
				rc = (int)src_idx;
				break;
			}
		}
		else if (src_str[src_idx] == '\0')
		{
			// terminate string and break
			rc = 0;
			break;
		}
		else
		{
			// copy char
			dst_str[dst_idx] = src_str[src_idx];
			dst_idx++;

			if (src_idx == src_size - 1)
			{
				// finished w/o terminator
				rc = 0;
			}
		}
	}

	// Null-terminate the string when done
	dst_str[dst_idx] = '\0';

	// detect if we exit'd the loop without a clear stop
	if (rc == -1)
	{
		rc = (int)src_idx;
	}

	return rc;
}

/*!
 * Lifted from man(3) snprintf and modified to assure we never
 * encounter a buffer over run condition. Has all the conditional
 * logic to deal with the libc variations over the years.
 */
char *make_message(int *stringlen, const char *fmt, va_list ap)
{
	int n, size = 1024;
	char *p, *np;

	if ((NULL == stringlen) || (NULL == fmt))
	{
		errno = EINVAL;
		return (NULL);
	}
	*stringlen = 0;
	if ((p = malloc(size)) == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}
	while (1)
	{
		/* Try to print in the allocated space. */
		n = vsnprintf(p, size, fmt, ap);
		/* If that worked, return the string. */
		if (n > -1 && n < size)
		{
			*stringlen = n;
			return p;
		}
		/* Else try again with more space. */
		if (n > -1)		/* glibc 2.1 */
		{
			size = n+1; /* precisely what is needed */
		}
		else			/* glibc 2.0 */
		{
			size *= 2;	/* twice the old size */
			if (size > 16384)
			{
				// Something is wrong with the supplied
				// params.  We are not going to process
				// 16K strings.
				free(p);
				errno = EINVAL;
				return (NULL);
			}
		}
		if ((np = realloc(p, size)) == NULL)
		{
			free(p);
			errno = ENOMEM;
			return (NULL);
		}
		else
		{
			p = np;
		}
	}
}

int s_snprintf(char *str, size_t size, const char *format, ...)
{
	va_list ap;
	char *formatted = NULL;
	int formatted_len;

	errno = 0;
	va_start(ap, format);
	formatted = make_message(&formatted_len, format, ap);
	va_end(ap);
	if (NULL == formatted)
	{
		return (-1);
	}
	s_strcpy(str, formatted, size);

	free(formatted);
	formatted = NULL;

	if (formatted_len >= size)
	{
		errno = EINVAL;
	}
	return (formatted_len);
}

/*
 * Format an unsigned int (4 bytes) to a hex string with 0x prefixed
 */
int get_hex_string(unsigned long long val, char *destStr, size_t len)
{
	int bytes = -1;
	if (destStr != NULL)
	{
		bytes = s_snprintf(destStr, len, "0x%llx", val);
	}

	return bytes;
}
