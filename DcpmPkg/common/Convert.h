/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CONVERT_H_
#define _CONVERT_H_

#include <Types.h>
#include <NvmTypes.h>
#include <Uefi.h>
#include <Debug.h>

//!< Macro that rounds up x value to the Next value with y multiplier
#define ROUNDUP(N, S) ((((N) + (S) - 1) / (S)) * (S))
//!< Macro that rounds down x value to the Next value with y multiplier
#define ROUNDDOWN(N, S) ((N) - ((N)%(S)))

#define DIGITS_AFTER_DECIMAL_ONE 1

/**
  Convert GUID structure to string
  Caller is responsible for FreePool on this pointer

  @param[in] pGuid

  @retval Pointer to string (NULL if pGuid parameter is NULL)
  @retval is pGuid is NULL or memory allocation failed
**/
CHAR16*
GuidToStr (
  IN     GUID *pGuid
  );


/**
  Converts 128 bit unsigned integer to an Unicode string as decimal value.
  The caller is responsible for memory deallocation of returned pointer.

  @param[in] Value - 128 bit unsigned integer to convert.

  @retval NULL, there was an error with memory allocation.
  @retval address of the allocated Unicode string containing the
    provided uint128 text representation.
**/
CHAR16 *
Uint128ToNewString(
  IN     UINT128 Value
  );

/**
  Converts 128 bit unsigned integer to an Unicode string as decimal value.

  @param[in] Value - 128 bit unsigned integer to convert.
  @param[in,out] String - Unicode string containing the provided uint128 text representation
**/
VOID
Uint128ToString(
  IN     UINT128 Value,
  IN OUT CHAR16 String[UINT128_STRING_SIZE]
  );

/**
Converts an 8 bit ASCII Source buffer to a Null-terminated Unicode Destination string.
The ASCII Source buffer may or may not be Null-terminated, and hence the function safely
appends it with a Null-terminator character.
The caller is responsible to make sure that Destination points to a buffer with size
equal to ((sizeof(Source) + 1) * sizeof(CHAR16)) in bytes.

@param[in] Source        Pointer to the ASCII Source buffer
@param[in] Length        Size of the Source buffer in bytes
@param[out] Destination  Pointer to the Null-terminated Unicode Destination string

@retval EFI_SUCCESS             The conversion was successful.
@retval EFI_INVALID_PARAMETER   A parameter was NULL or invalid.
@retval EFI_OUT_OF_RESOURCES    Memory allocation failure
**/
EFI_STATUS
EFIAPI
SafeAsciiStrToUnicodeStr (
IN     CONST CHAR8 *Source,
IN     UINT32 Length,
OUT CHAR16 *Destination
);

/**
  Check if a Unicode character is a decimal character.

  This internal function checks if a Unicode character is a
  decimal character.  The valid characters are
  L'0' to L'9'.

  @param[in] Char The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.
**/
BOOLEAN
EFIAPI
IsDecimalDigitCharacter (
  IN     CHAR16 Char
  );

/**
  Convert a Unicode character to upper case only if
  it maps to a valid small-case ASCII character.

  This internal function only deal with Unicode character
  which maps to a valid small-case ASCII character, i.e.
  L'a' to L'z'. For other Unicode character, the input character
  is returned directly.

  @param[in] Char The character to convert.

  @retval LowerCharacter   If the Char is with range L'a' to L'z'.
  @retval Unchanged        Otherwise.
**/
CHAR16
EFIAPI
CrCharToUpper (
  IN     CHAR16 Char
  );

/**
  Check if a Unicode character is a hexadecimal character.

  This internal function checks if a Unicode character is a
  numeric character.  The valid hexadecimal characters are
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.

  @param[in] Char The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.
**/
BOOLEAN
EFIAPI
IsHexaDecimalDigitCharacter (
  IN     CHAR16 Char
  );

/**
  Function to determin if an entire string is a valid number.

  If Hex it must be preceeded with a 0x or has ForceHex, set TRUE.

  @param[in] pString      The string to evaluate.
  @param[in] ForceHex     TRUE - always assume hex.
  @param[in] StopAtSpace  TRUE to halt upon finding a space, FALSE to keep going.

  @retval TRUE        It is all numeric (dec/hex) characters.
  @retval FALSE       There is a non-numeric character.
**/
BOOLEAN
EFIAPI
IsHexOrDecimalNumber (
  IN     CONST CHAR16 *pString,
  IN     CONST BOOLEAN ForceHex,
  IN     CONST BOOLEAN StopAtSpace
  );

/**
  Check ASCII character for being a alphanumeric character

  @param[in] Char  Character to be tested
  @retval    TRUE  Provided char is an alphanumeric character
  @retval    FALSE Provided char is not an alphanumeric character
**/
BOOLEAN
EFIAPI
IsAsciiAlnumCharacter (
  IN     CHAR8 Char
  );

/**
  Check Unicode character for being a alphanumeric character

  @param[in] Char  Character to be tested
  @retval    TRUE  Provided char is an alphanumeric character
  @retval    FALSE Provided char is not an alphanumeric character
**/
BOOLEAN
EFIAPI
IsUnicodeAlnumCharacter (
  IN     CHAR16 Char
  );

/**
  Convert a Null-terminated Unicode decimal string to a value of
  type UINT64.

  This function returns a value of type UINT64 by interpreting the contents
  of the Unicode string specified by String as a decimal number. The format
  of the input Unicode string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The
  function will ignore the pad space, which includes spaces or
  tab characters, before [decimal digits]. The running zero in the
  beginning of [decimal digits] will be ignored. Then, the function
  stops at the first character that is a not a valid decimal character
  or a Null-terminator, whichever one comes first.

  If String has only pad spaces, then 0 is returned.
  If String has no pad spaces or valid decimal digits,
  then 0 is returned.

  @param[in] pString      A pointer to a Null-terminated Unicode string.
  @param[out] pValue      Upon a successful return the value of the conversion.
  @param[in] StopAtSpace  FALSE to skip spaces.

  @retval EFI_SUCCESS             The conversion was successful.
  @retval EFI_INVALID_PARAMETER   A parameter was NULL or invalid.
  @retval EFI_DEVICE_ERROR        An overflow occurred.
**/
EFI_STATUS
EFIAPI
CrStrDecimalToUint64 (
  IN     CONST CHAR16 *pString,
     OUT UINT64 *pValue,
  IN     CONST BOOLEAN StopAtSpace
  );

/**
  Convert a Unicode character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASII character, i.e.
  L'0' to L'9', L'a' to L'f' or L'A' to L'F'. For other
  Unicode character, the value returned does not make sense.

  @param[in] Char  The character to convert.

  @retval The numerical value converted.
**/
UINT32
EFIAPI
CrHexCharToUint (
  IN     CHAR16 Char
  );

/**
  Convert a Null-terminated Unicode hexadecimal string to a value of type UINT64.

  This function returns a value of type UINTN by interpreting the contents
  of the Unicode string specified by String as a hexadecimal number.
  The format of the input Unicode string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix.
  If "x" appears in the input string, it must be prefixed with at least one 0.
  The function will ignore the pad space, which includes spaces or tab characters,
  before [zeros], [x] or [hexadecimal digit]. The running zero before [x] or
  [hexadecimal digit] will be ignored. Then, the decoding starts after [x] or the
  first valid hexadecimal digit. Then, the function stops at the first character that is
  a not a valid hexadecimal character or NULL, whichever one comes first.

  If String has only pad spaces, then zero is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits,
  then zero is returned.

  @param[in] pString      A pointer to a Null-terminated Unicode string.
  @param[out] pValue      Upon a successful return the value of the conversion.
  @param[in] StopAtSpace  FALSE to skip spaces.

  @retval EFI_SUCCESS             The conversion was successful.
  @retval EFI_INVALID_PARAMETER   A parameter was NULL or invalid.
  @retval EFI_DEVICE_ERROR        An overflow occurred.
**/
EFI_STATUS
EFIAPI
CrStrHexToUint64 (
  IN     CONST CHAR16 *pString,
     OUT UINT64 *pValue,
  IN     CONST BOOLEAN StopAtSpace
  );

/**
  Function to verify and convert a string to its numerical value.

  If Hex it must be preceded with a 0x, 0X, or has ForceHex set TRUE.

  @param[in] pString      The string to evaluate.
  @param[out] pValue      Upon a successful return the value of the conversion.
  @param[in] ForceHex     TRUE - always assume hex.
  @param[in] StopAtSpace  FALSE to skip spaces.

  @retval EFI_SUCCESS             The conversion was successful.
  @retval EFI_INVALID_PARAMETER   String contained an invalid character.
  @retval EFI_NOT_FOUND           String was a number, but Value was NULL.
**/
EFI_STATUS
EFIAPI
ConvertStringToUint64(
  IN     CONST CHAR16 *pString,
     OUT UINT64 *pValue,
  IN     CONST BOOLEAN ForceHex,
  IN     CONST BOOLEAN StopAtSpace
  );

/**
  Checks if the provided Unicode string is a proper hex or dec value and
  decodes the value. The result is stored at the pOutValue pointer.

  @param[in] pString is the pointer to an Unicode string that the caller
    wants to check (and parse) if it contains hex or dec value.
  @param[out] pOutValue is the pointer to a UINT64 value where the
    decoded result will be stored.

  @retval TRUE if the provided string is a valid hex or dec value and
    it was successfully decoded.
  @retval FALSE if the provided string is not a valid hex or dec value
    or there was an error while decoding its value.
**/
BOOLEAN
GetU64FromString (
  IN     CHAR16 *pString,
     OUT UINT64 *pOutValue
  );

/**
  Make the capacity string based on the requested units

  @param[in] HiiHandle The handle for the hii instance (used for string pack)
  @param[in] Capacity The input capacity in bytes
  @param[in] CurrentUnits The requested type of units to convert the capacity into
  @param[in] AppendUnits Flag to append units to the resulting capacity string
  @param[out] ppCapacityStr Pointer to string that displays the capacity and units

  @retval EFI_INVALID_PARAMETER if input parameter is null
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
MakeCapacityString (
  IN     EFI_HII_HANDLE HiiHandle,
  IN     UINT64 Capacity,
  IN     UINT16 CurrentUnits,
  IN     BOOLEAN AppendUnits,
     OUT CHAR16 **ppCapacityStr
  );

/**
  A helper function to convert a capacity value in bytes as per the requested units
  to a printable string.

  @param[in] Capacity The input capacity in bytes
  @param[in] Units The requested type of units to convert the capacity into
  @param[out] pFormattedSizeString Pointer to the converted size string

  Note: The caller is responsible for freeing the returned string
  Note: The returned value will always be less than the actual value
  (don't over-report size)

  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
GetFormattedSizeString (
  IN     UINT64 Capacity,
  IN     UINT16 Units,
  IN     UINT32 NumberOfDigitsAfterDecimal,
     OUT CHAR16 **ppFormattedSizeString
);

/**
  Gets the best possible units to display capacity

  @param[in] Capacity The input capacity in bytes
  @param[in] CurrentUnits The units for best possible representation of capacity

  @retval The optimal units to use for the given combination
**/
UINT16
GetBestDisplayForCapacity (
  IN     UINT64 Capacity,
  IN     UINT16 CurrentUnits
  );

/**
  Get preferred value as string

  @param[in] Number Value as number
  @param[in] pString Value as string
  @param[in] NumberPreferred True if the number is preferred
  @param[out] pResultString String representation of preferred value
  @param[in] ResultStringLen Length of pResultString

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
GetPreferredValueAsString(
  IN     UINT32 Number,
  IN     CHAR16 *pString OPTIONAL,
  IN     BOOLEAN NumberPreferred,
     OUT CHAR16 *pResultString,
  IN     UINT32 ResultStringLen
  );

/**
  Safely computes and populates capacity in bytes using the conversion factor for the particular units

  @param[in]  Capacity The input capacity
  @param[in]  ConversionFactor Value used to convert to bytes (based on the particular units)
  @param[out] pCapacityBytes Pointer to capacity in bytes

  @retval EFI_SUCCESS if the computation was successful
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_BAD_BUFFER_SIZE if overflow check fails for the input capacity because of buffer size limitations
**/
EFI_STATUS
SafeCapacityConversionToBytes (
  IN     double Capacity,
  IN     UINT64 ConversionFactor,
     OUT UINT64 *pCapacityBytes
  );

/**
  Convert the given capacity into bytes

  @param[in]  Capacity The input capacity
  @param[in]  Units The units representing the given capacity
  @param[out] pCapacityBytes Pointer to return value - Capacity in bytes

  @retval EFI_SUCCESS if the conversion was successful
  @retval EFI_INVALID_PARAMETER if the input parameter was incorrect or null
**/
EFI_STATUS
ConvertToBytes (
  IN     double Capacity,
  IN     UINT16 Units,
     OUT UINT64 *pCapacityBytes
  );

/**
  Get physical block size - aligned to cache line size

  @param[in] BlockSize Usable block size

  @retval Physical block size
**/
UINT64
GetPhysicalBlockSize(
  IN     UINT64 BlockSize
  );

/**
  Get granularity size for a stored granularity value

  @param[in] AppDirectGranularity stored as driver preference value

  @retval granularity size
**/
UINT64
ConvertAppDirectGranularityPreference(
  IN      UINT8 AppDirectGranularity
  );

/**
Check if a Unicode character is a hexadecimal value.

This internal function checks if a Unicode character is a
numeric value.

@param[in] pString The character to check against.
@param[in] StopAtSpace  TRUE to halt upon finding a space, FALSE to keep going.

@retval TRUE  If the pString is a hexadecmial number.
@retval FALSE If the pString is not a hexadecmial number.
**/
BOOLEAN
EFIAPI
IsHexValue(
  IN     CONST CHAR16 *pString,
  IN     CONST BOOLEAN StopAtSpace
);

#endif /** _CONVERT_H_ **/
