/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the definition of various safe string functions
 * that should be used instead of the built in ones (i.e. strcpy, strncpy, ...)
 */

#ifndef	_S_STR_H_
#define	_S_STR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__LINUX__) || defined(__ESX__)

#define s_strtok(dest, dmax, delim, ptr) strtok_s(dest, dmax, delim, ptr)
#define s_wcstok(dest, dmax, delim, ptr) wcstok_s(dest, dmax, delim, ptr)

#else // defined(__LINUX__) || defined(__ESX__)

#define s_strtok(dest, dmax, delim, ptr) strtok_s(dest, delim, ptr)
#define s_wcstok(dest, dmax, delim, ptr) wcstok_s(dest, delim, ptr)

#endif // defined(__LINUX__) || defined(__ESX__)

#include <stdlib.h>
#include <stdarg.h>
#include <export_api.h>

#define	HEX_STR_LEN	20

/*!
 * A macro to remove trailing spaces
 * @todo
 * 		Replace all instances of this macro with the functionally identical #s_strtrim_right
 * @param[in,out] s
 * 		The string to trim.
 * @param[in] len
 * 		The size of the buffer allocated to @c s
 */
#define	remove_spaces(s, len) s_strtrim_right(s, len)

/*!
 * Safe concatenation of strings of different sizes.
 * This function will attempt to concatenate as much of @c src to @c dst as possible,
 * while still null-terminating the resulting @c dst string.
 * @remarks
 * 		Concatenation begins at, and replaces, the first null-terminator in @c dst. This
 * 		means that @c dst must be null-terminated, otherwise no concatenation can occur.
 * @param[out] dst
 * 		Pointer to the destination buffer to copy characters into
 * @param[in] dst_size
 * 		Size of the destination buffer
 * @param[in] src
 * 		Pointer to the source array to copy characters from
 * @param[in] src_size
 * 		Size of the source array
 * @return
 * 		Pointer to the @c dst.
 */
char *s_strncat(char *dst, size_t dst_size, const char *src, size_t src_size);

/*!
 * Safe concatenation of strings of identical sizes.
 * This function will attempt to concatenate as much of @c src to @c dst as possible,
 * while still null-terminating the resulting @c dst string.
 * @remarks
 * 		Concatenation begins at, and replaces, the first null-terminator in @c dst. This
 * 		means that @c dst must be null-terminated, otherwise no concatenation can occur.
 * @warning
 * 		Do not use #s_strcat if the size of the allocated char array for @c src is less
 * 		than @c dst_size.  This may cause a buffer overrun.  Use #s_strncat instead.
 * @param[out] dst
 * 		Pointer to the destination buffer to copy characters into
 * @param[in] dst_size
 * 		Size of the destination buffer
 * @param[in] src
 * 		Pointer to the source array to copy characters from
 * @return
 * 		Pointer to the @c dst.
 */
char *s_strcat(char *dst, size_t dst_size, const char *src);

/*!
 * Safely determines the length of a string up to @c max_len
 * @remarks
 * 		Stops counting characters when a null terminator or @c max_len is reached
 * @param[in] str
 * 		The string whose length to determine
 * @param[in] max_len
 * 		The maximum number of characters to consider
 * @return
 * 		A value between 0 and @c max_len, denoting the size of the string,
 * 		not including the null terminator if found.
 */
size_t s_strnlen(const char *str, size_t max_len);

/*!
 * Safe copy of a @b char array
 * @param[out] dst
 * 		Pointer to the destination buffer to copy characters into
 * @param[in] dst_size
 * 		Size of the destination buffer
 * @param[in] src
 * 		Pointer to the source array to copy characters from
 * @param[in] src_size
 * 		Size of the source array
 * @return
 * 		The number of characters that were copied including null terminator.
 */
char *s_strncpy(char *dst, size_t dst_size, const char *src, size_t src_size);

/*!
 * Safe copy of a @b char array.
 * @remarks
 * 		If @c src is not null terminated, then @c dst_size can be no larger
 * 		than @a (min(size(dst),size(src))+1) to copy over the full string,
 * 		and ensure null-termination.
 * @warning
 * 		Do not use #s_strcpy if the size of the allocated char array for @c src is less
 * 		than @c dst_size.  This may cause a buffer overrun.  Use #s_strncpy instead.
 * @param[out] dst
 * 		Pointer to the destination buffer to copy characters into
 * @param[in] src
 * 		Pointer to the source array to copy characters from
 * @param[in] dst_size
 * 		Size of the destination buffer
 * @return
 * 		The number of characters that were copied including null terminator.
 */
char *s_strcpy(char *dst, const char *src, size_t dst_size);

/*!
 * Safe string conversion to @b unsigned char.
 * @remarks
 * 		This function will ignore all non-digits until it finds the first digit.
 * 		Then it will consume all characters that are digits, up to a maximum value
 * 		of @c UCHAR_MAX (=255)
 * @param[in] str
 * 		String to search for number to convert
 * @param[in] str_len
 * 		Length of @c str
 * @param[out] pp_end
 * 		Optional double pointer to character occurring just after last numerical character consumed.
 * 		If the last numerical character converted is the final element in the @b char array with
 * 		length @c str_len, @c *pp_end will equal NULL.
 * @param[out] p_result
 * 		Result of the conversion.
 * @return
 * 		The number of characters consumed from @c str.  This value is zero if the conversion
 * 		fails due to invalid input arguments.
 */
size_t s_digitstrtouc(const char *const str, size_t str_len, const char **pp_end,
		unsigned char *p_result);

/*!
 * Safe string conversion to @b unsigned @b short integer.
 * @remarks
 * 		This function will ignore all non-digits until it finds the first digit.
 * 		Then it will consume all characters that are digits, up to a maximum value
 * 		of @c USHRT_MAX (=65535)
 * @param[in] str
 * 		String to search for number to convert
 * @param[in] str_len
 * 		Length of @c str
 * @param[out] pp_end
 * 		Optional double pointer to character occurring just after last numerical character consumed.
 * 		If the last numerical character converted is the final element in the @b char array with
 * 		length @c str_len, @c *pp_end will equal NULL.
 * @param[out] p_result
 * 		Result of the conversion.
 * @return
 * 		The number of characters consumed from @c str.  This value is zero if the conversion
 * 		fails due to invalid input arguments.
 */
size_t s_strtous(const char *const str, size_t str_len, const char **pp_end,
		unsigned short *p_result);

/*!
 * Safe string conversion to @b unsigned integer.
 * @remarks
 * 		This function will ignore all non-digits until it finds the first digit.
 * 		Then it will consume all characters that are digits, up to a maximum value
 * 		of @c UINT_MAX (=4294967295)
 * @param[in] str
 * 		String to search for number to convert
 * @param[in] str_len
 * 		Length of @c str
 * @param[out] pp_end
 * 		Optional double pointer to character occurring just after last numerical character consumed.
 * 		If the last numerical character converted is the final element in the @b char array with
 * 		length @c str_len, @c *pp_end will equal NULL.
 * @param[out] p_result
 * 		Result of the conversion.
 * @return
 * 		The number of characters consumed from @c str.  This value is zero if the conversion
 * 		fails due to invalid input arguments.
 */
size_t s_strtoui(const char *const str, size_t str_len, const char **pp_end,
		unsigned int *p_result);

/*!
 * Safe string conversion to @b unsigned @b long @b long integer.
 * @remarks
 * 		This function will ignore all non-digits until it finds the first digit.
 * 		Then it will consume all characters that are digits, up to a maximum value
 * 		of @c ULLONG_MAX (=18446744073709551615)
 * @param[in] str
 * 		String to search for number to convert
 * @param[in] str_len
 * 		Length of @c str
 * @param[out] pp_end
 * 		Optional double pointer to character occurring just after last numerical character consumed.
 * 		If the last numerical character converted is the final element in the @b char array with
 * 		length @c str_len, @c *pp_end will equal NULL.
 * @param[out] p_result
 * 		Result of the conversion.
 * @return
 * 		The number of characters consumed from @c str.  This value is zero if the conversion
 * 		fails due to invalid input arguments.
 */
size_t s_strtoull(const char *const str, size_t str_len, const char **pp_end,
		unsigned long long *p_result);
/*!
 * Safe copy of a @b char array, omitting the null terminator
 * @param dst
 * 		Pointer to the destination buffer to copy characters into
 * @param dst_size
 * 		Size of the destination buffer
 * @param src
 * 		Pointer to the source array to copy characters from
 * @param src_size
 * 		Size of the source array
 * @return
 * 		A pointer to the destination string
 */
char *s_strncpy_unterm(char *dst, size_t dst_size, const char *src, size_t src_size);

/*!
 * Safe case-insensitive string comparison
 * @remarks
 * 		This function compares the number of characters defined by @c size
 * 		and is not ended when a null terminator is encountered.
 * @param str1
 * 		Pointer to the first string to be compared
 * @param str2
 * 		Pointer to the second string to be compared
 * @param size
 * 		The number of characters to be compared
 * @return
 * 		A value indicating the character number (index+1) where the two string differ @n
 * 		0 if equivalent @n
 * 		-1 if either string pointer is @b NULL or @c size is 0
 */
int s_strncmpi(const char *const str1, const char *const str2, size_t size);

/*!
 * Safe case-sensitive string comparison
 * @remarks
 * 		This function compares the number of characters defined by @c size
 * 		and is not ended when a null terminator is encountered.
 * @param str1
 * 		Pointer to the first string to be compared
 * @param str2
 * 		Pointer to the second string to be compared
 * @param size
 * 		The number of characters to be compared
 * @return
 * 		A value indicating the character number (index+1) where the two string differ @n
 * 		0 if equivalent @n
 * 		-1 if either string pointer is @b NULL or @c size is 0
 */
int s_strncmp(const char *const str1, const char *const str2, size_t size);

/*!
 * Safe copy of a @b wchar_t array.
 * @remarks
 * 		If @c src is not null terminated, then @c dst_size can be no larger
 * 		than @a (min(size(dst),size(src))+1) to copy over the full string,
 * 		and ensure null-termination.
 * @warning
 * 		Do not use #ws_strcpy if the size of the allocated char array for @c src is less
 * 		than @c dst_size.  This may cause a buffer overrun.
 * @param[out] dst
 * 		Pointer to the destination buffer to copy characters into.
 * @param[in] src
 * 		Pointer to the source array to copy characters from
 * @param[in] dst_size
 * 		Size of the destination buffer, which must be greater than zero.
 * @return
 * 		A copy of the @c dst pointer.
 */
wchar_t *ws_strcpy(wchar_t *dst, const wchar_t *src, size_t dst_size);

/*!
 * Safe trim of trailing white space from the given string.
 * @remarks
 * 		If the string @c s is not null terminated, then the last character will
 * 		be replaced with a null-terminator, after which trimming will resume.
 * @param[in,out] s
 * 		The string to trim.
 * @param[in] len
 * 		The size of the buffer allocated to @c s
 */
void s_strtrim_right(char *s, unsigned int len);

/*!
 * Safe trim of leading white space from the given string.
 * @remarks
 * 		If the string @c s is not null terminated, the last character will
 * 		be replaced with a null-terminator, relative to @c len
 * @param[in,out] s
 * 		The string to trim.
 * @param[in] len
 * 		The size of the buffer allocated to @c s
 */
void s_strtrim_left(char *s, unsigned int len);

/*!
 * Safe trim of white space at both ends of a string.
 * @remarks
 * 		If the string is not null terminated, it will force null termination
 * 		no later in the string that with respect to @c len
 * @param[in,out] s
 * 		The string to trim.
 * @param[in] len
 * 		The size of the buffer allocated to @c s
 */
void s_strtrim(char *s, unsigned int len);

/*!
 * Safe replacement of all found instances of a specific @b char, with a given string
 * @todo
 * 		The implementation is flawed in that trimming of trailing white space may not
 * 		occur correctly.
 * @remarks
 * 		Replacement includes all characters, excluding null terminators.
 * @param[out] dst_str
 * 		The destination string, where each instance of @c target_char has been replaced
 * 		with @c rep_cstr
 * @param[in] dst_size
 * 		The size of the buffer allocated to @c dst_str
 * @param[in] src_str
 * 		The source string
 * @param[in] src_size
 * 		The size of the buffer allocated to @c src_str
 * @param[in] target_char
 * 		The @b char to be replaced with @c rep_cstr
 * @param[in] rep_cstr
 * 		The @b char array used to replace @c target_char
 * @param[in] rep_cstr_size
 * 		The size of the buffer allocated to @c rep_cstr
 * @return
 * 		0 if complete replacement was successful, else @n
 * 		the number of characters from the src_str successfully processed.
 */
int s_strrep_char(char *dst_str, size_t dst_size, const char *src_str, size_t src_size,
		const char target_char, const char *rep_cstr, size_t rep_cstr_size);

/*!
 * Safe find of the last instance of a given @b char within a string.
 * @param[in] str
 * 		The string to be searched for @b char @c ch
 * @param[in] ch
 * 		The @b char which we are searching for the last instance of.
 * @param[in] max_len
 * 		The maximum number of characters to search for @c ch within.
 * @return
 * 		A pointer to the last instance of @c ch within @c str @n
 * 		else, @b NULL if an instance of @c ch is not found.
 */
char *s_strrchr(char *str, char ch, int max_len);

/*!
 * @brief Safe version of snprintf
 * Following the convention of C99 formatting specifiers, this
 * function writes at most "size" characters "str" using "format"
 * as a template to parse the optional va_arg list following "format".
 *
 * @param[out] str
 * 		The formatted return string.
 * @param[in] size
 * 		Number of bytes (chars) available for use in str.
 * @param[in] format
 * 		standard, null terminated printf format string to be parsed.
 * @param[in] ...
 * 		va_arg list of arguments to be formatted into str.
 * @return
 * 		This function conforms to C99 standards for snprint, but assures
 * 		a null termintated return buffer
 * 		Upon success, returns the number of characters placed in "str"
 * 		Only "size" characters plus the null terminator will be placed
 * 		in "str".  If "size" was too small, the return value will be the
 * 		number of characters that would have been put in "str", but
 * 		"str" will be truncated to at most "size"-1 + the null terminator
 * 		If an error occurs, -1 will be returned and errno set
 */
int s_snprintf(char *str, size_t size, const char *format, ...);

/*!
 * @brief Safe version of get_hex_string
 * This function writes at most "len" characters "destStr"
 * by formatting "val" to a hex string prefixed with 0x
 *
 * @param[in] val
 *		The integer value to format.
 * @param[out] destStr
 *		The destination buffer for the formatted string.
 * @param[in] len
 *		Number of bytes (chars) available for use in destStr.
 * @return
 *		This function upon success returns the number of bytes
 *		(characters) placed in the string
 */
int get_hex_string(unsigned long long val, char *destStr, size_t len);

#ifdef __cplusplus
}
#endif


#endif /* _S_STR_H_ */
