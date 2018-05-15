/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Convert.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

/**
  Return a string of digits after decimal point.

  Number: 12.3456, NumberOfDigits: 2, Result: 34

  @param[in] Number Number to retrieve digits from
  @param[in] NumberOfDigits Number of digits after point to retrieve

  @result Digits after point
**/
CHAR16*
GetDigitsStrAfterPointFromNumber(
  IN     double Number,
  IN     UINT32 NumberOfDigits
  )
{
  CHAR16 *pDigitsStr = NULL;
  double PostDecimal = 0.0;
  UINT32 Index = 0;
  UINT32 PostDecimalInt = 0;
  UINT32 Digit = 0;
  UINT32 PreviousDigits = 0;

  PostDecimal = Number - (UINT32) Number;

  for (Index = 1; Index <= NumberOfDigits; Index++) {
    PostDecimalInt = (UINT32) (Pow(10, Index) * PostDecimal);
    Digit = PostDecimalInt - (10 * PreviousDigits);

    pDigitsStr = CatSPrintClean(pDigitsStr, L"%d", Digit);

    PreviousDigits = PostDecimalInt;
  }

  return pDigitsStr;
}

/**
  Convert GUID structure to string
  Caller is responsible for FreePool on this pointer

  @param[in] pGuid

  @retval Pointer to string (NULL if pGuid parameter is NULL)
  @retval is pGuid is NULL or memory allocation failed
**/
CHAR16*
GuidToStr (
  IN     EFI_GUID *pGuid
  )
{
  CHAR16 *pString = NULL;

  if (pGuid == NULL) {
    return NULL;
  }

  pString = CatSPrint(NULL, L"%g", *pGuid);
  return pString;
}

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
  )
{
  CHAR16 String[UINT128_STRING_SIZE];
  CHAR16 *pToReturn = NULL;

  SetMem(String, sizeof(String), 0x0);

  Uint128ToString(Value, String);

  pToReturn = CatSPrintClean(NULL, FORMAT_STR, String);

  return pToReturn;
}

/**
  Converts 128 bit unsigned integer to an Unicode string as decimal value.

  @param[in] Value - 128 bit unsigned integer to convert.
  @param[in,out] String - Unicode string containing the provided uint128 text representation
**/
VOID
Uint128ToString(
  IN     UINT128 Value,
  IN OUT CHAR16 String[UINT128_STRING_SIZE]
  )
{
  CHAR16 Digits[UINT128_DIGITS];
  INT32 Index = 0;
  INT32 Index2 = 0;
  UINT64 PartValue = 0;
  UINT8 Counter = 0;

  SetMem(Digits, sizeof(Digits), 0x0);

  if (String == NULL) {
    return;
  }

  ZeroMem(String, UINT128_STRING_SIZE);

  PartValue = Value.Uint64_1;
  // This loop needs to be executed twice. Once for Value.Uint64_1 and once for Value.Uint64
  for (Counter = 0; Counter < 2; Counter++) {
    for (Index = (UINT64_BITS - 1); Index >= 0; Index--) {
      if ((PartValue >> Index) & 1) {
        Digits[0]++;
      }
      if (Index > 0 || Counter == 0) {
        for (Index2 = 0; Index2 < UINT128_DIGITS; Index2++) {
          Digits[Index2] *= 2;
        }
      }
      for (Index2 = 0; Index2 < (UINT128_DIGITS - 1); Index2++) {
        Digits[Index2 + 1] += Digits[Index2] / 10;
        Digits[Index2] %= 10;
      }
    }
    PartValue = Value.Uint64;
  }

  for (Index = (UINT128_DIGITS - 1); Index > 0; Index--) {
    if (Digits[Index] > 0) {
      break;
    }
  }

  for (Index2 = Index; Index2 > -1; Index2--) {
    Digits[Index2] += '0';
  }

  for (Index2 = 0; Index > -1; Index--, Index2++) {
    String[Index2] = Digits[Index];
  }

  /** set the end of string char overtly, although it is not necessary because of ZeroMem **/
  String[Index2] = '\0';
}

/**
  Converts an 8 bit ASCII Source buffer to a Null-terminated Unicode Destination string.
  The ASCII Source buffer may or may not be Null-terminated, and hence the function
  safely appends it with a Null-terminator character.
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
  )
{
  CHAR8 *TempSource = NULL;

  if (Source == NULL || Destination == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TempSource = AllocateZeroPool(Length + 1);
  if (TempSource == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem(TempSource, Source, Length);
  TempSource[Length] = '\0';
  AsciiStrToUnicodeStr(TempSource, Destination);

  FREE_POOL_SAFE(TempSource);
  return EFI_SUCCESS;
}

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
  )
{
  return (BOOLEAN) (Char >= L'0' && Char <= L'9');
}

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
  )
{
  if (Char >= L'a' && Char <= L'z') {
    return (CHAR16) (Char - (L'a' - L'A'));
  }

  return Char;
}

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
  )
{
  return (BOOLEAN) ((Char >= L'0' && Char <= L'9') || (Char >= L'A' && Char <= L'F') || (Char >= L'a' && Char <= L'f'));
}

/**
  Function to determine if an entire string is a valid number.

  If Hex it must be preceded with a 0x or has ForceHex, set TRUE.

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
  )
{
  BOOLEAN Hex;

  //
  // chop off a single negative sign
  //
  if (pString != NULL && *pString == L'-') {
    pString++;
  }

  if (pString == NULL) {
    return (FALSE);
  }

  //
  // chop leading zeroes
  //
  while(pString != NULL && *pString == L'0'){
    pString++;
  }
  //
  // allow '0x' or '0X', but not 'x' or 'X'
  //
  if (pString != NULL && (*pString == L'x' || *pString == L'X')) {
    if (*(pString-1) != L'0') {
      //
      // we got an x without a preceding 0
      //
      return (FALSE);
    }
    pString++;
    Hex = TRUE;
  } else if (ForceHex) {
    Hex = TRUE;
  } else {
    Hex = FALSE;
  }

  //
  // loop through the remaining characters and use the lib function
  //
  for ( ; pString != NULL && *pString != CHAR_NULL && !(StopAtSpace && *pString == L' ') ; pString++){
    if (Hex) {
      if (!IsHexaDecimalDigitCharacter(*pString)) {
        return (FALSE);
      }
    } else {
      if (!IsDecimalDigitCharacter(*pString)) {
        return (FALSE);
      }
    }
  }

  return (TRUE);
}

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
  )
{
  //
  // chop off a single negative sign
  //
  if (pString != NULL && *pString == L'-') {
    pString++;
  }

  if (pString == NULL) {
    return (FALSE);
  }

  //
  // chop leading zeroes
  //
  while(pString != NULL && *pString == L'0') {
    pString++;
  }
  //
  // allow '0x' or '0X', but not 'x' or 'X'
  //
  if (pString != NULL && (*pString == L'x' || *pString == L'X')) {
    if (*(pString-1) != L'0') {
      //
      // we got an x without a preceding 0
      //
      return FALSE;
    }
    pString++;
  } else {
    return FALSE;
  }

  //
  // loop through the remaining characters and use the lib function
  //
  for ( ; pString != NULL && *pString != CHAR_NULL && !(StopAtSpace && *pString == L' ') ; pString++) {
      if (!IsHexaDecimalDigitCharacter(*pString)) {
        return FALSE;
      }
  }

  return TRUE;
}


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
  )
{
  return (BOOLEAN) ((Char >= '0' && Char <= '9') || (Char >= 'A' && Char <= 'Z') || (Char >= 'a' && Char <= 'z'));
}

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
  )
{
  return (BOOLEAN) ((Char >= L'0' && Char <= L'9') || (Char >= L'A' && Char <= L'Z') || (Char >= L'a' && Char <= L'z'));
}

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
  )
{
  UINT64 Result = 0;

  if (pString == NULL || StrSize (pString) == 0 || pValue == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*pString == L' ') || (*pString == L'\t')) {
    pString++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*pString == L'0') {
    pString++;
  }

  Result = 0;

  //
  // Skip spaces if requested
  //
  while (StopAtSpace && *pString == L' ') {
    pString++;
  }
  while (IsDecimalDigitCharacter (*pString)) {
    //
    // If the number represented by String overflows according
    // to the range defined by UINT64, then ASSERT().
    //

    if (!(Result <= (DivU64x32((((UINT64) ~0) - (*pString - L'0')),10)))) {
      return (EFI_DEVICE_ERROR);
    }

    Result = MultU64x32(Result, 10) + (*pString - L'0');
    pString++;

    //
    // Stop at spaces if requested
    //
    if (StopAtSpace && *pString == L' ') {
      break;
    }
  }

  *pValue = Result;

  return (EFI_SUCCESS);
}

/**
  Convert a Unicode character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASCII character, i.e.
  L'0' to L'9', L'a' to L'f' or L'A' to L'F'. For other
  Unicode character, the value returned does not make sense.

  @param[in] Char  The character to convert.

  @retval The numerical value converted.
**/
UINT32
EFIAPI
CrHexCharToUint (
  IN     CHAR16 Char
  )
{
  if (IsDecimalDigitCharacter (Char)) {
    return Char - L'0';
  }

  return 10 + CrCharToUpper (Char) - L'A';
}

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
  )
{
  UINT64 Result = 0;

  if (pString == NULL || StrSize(pString) == 0 || pValue == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*pString == L' ') || (*pString == L'\t')) {
    pString++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*pString == L'0') {
    pString++;
  }

  if (CrCharToUpper (*pString) == L'X') {
    if (*(pString - 1) != L'0') {
      return 0;
    }
    //
    // Skip the 'X'
    //
    pString++;
  }

  Result = 0;

  //
  // Skip spaces if requested
  //
  while (StopAtSpace && *pString == L' ') {
    pString++;
  }

  while (IsHexaDecimalDigitCharacter (*pString)) {
    //
    // If the Hex Number represented by String overflows according
    // to the range defined by UINTN, then ASSERT().
    //
    if (!(Result <= (RShiftU64((((UINT64) ~0) - CrHexCharToUint (*pString)), 4)))) {
//    if (!(Result <= ((((UINT64) ~0) - CrHexCharToUintn (*String)) >> 4))) {
      return (EFI_DEVICE_ERROR);
    }

    Result = (LShiftU64(Result, 4));
    Result += CrHexCharToUint (*pString);
    pString++;

    //
    // stop at spaces if requested
    //
    if (StopAtSpace && *pString == L' ') {
      break;
    }
  }

  *pValue = Result;
  return (EFI_SUCCESS);
}

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
  )
{
  UINT64 RetVal = 0;
  CONST CHAR16 *pWalker = NULL;
  EFI_STATUS Status = EFI_SUCCESS;
  BOOLEAN Hex = FALSE;

  Hex = ForceHex;

  if (!IsHexOrDecimalNumber(pString, Hex, StopAtSpace)) {
    if (!Hex) {
      Hex = TRUE;
      if (!IsHexOrDecimalNumber(pString, Hex, StopAtSpace)) {
        return (EFI_INVALID_PARAMETER);
      }
    } else {
      return (EFI_INVALID_PARAMETER);
    }
  }

  //
  // Chop off leading spaces
  //
  for (pWalker = pString; pWalker != NULL && *pWalker != CHAR_NULL && *pWalker == L' '; pWalker++);

  //
  // make sure we have something left that is numeric.
  //
  if (pWalker == NULL || *pWalker == CHAR_NULL || !IsHexOrDecimalNumber(pWalker, Hex, StopAtSpace)) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // do the conversion.
  //
  if (Hex || StrnCmp(pWalker, L"0x", 2) == 0 || StrnCmp(pWalker, L"0X", 2) == 0){
    Status = CrStrHexToUint64(pWalker, &RetVal, StopAtSpace);
  } else {
    Status = CrStrDecimalToUint64(pWalker, &RetVal, StopAtSpace);
  }

  if (pValue == NULL && !EFI_ERROR(Status)) {
    return (EFI_NOT_FOUND);
  }

  if (pValue != NULL) {
    *pValue = RetVal;
  }

  return (Status);
}

/**
  Checks if the provided Unicode string is a proper hex or dec value and
  decodes the value. The result is stored at the pOutVlue pointer.

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
  )
{
  BOOLEAN IsValid = FALSE;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (pString == NULL || pOutValue == NULL) {
    NVDIMM_DBG("pString or pOutValue is NULL.");
    return FALSE;
  }

  // negative values shouldn't be converted
  if (StrnCmp(pString, L"-", 1) == 0) {
    return FALSE;
  }

  IsValid = IsHexOrDecimalNumber(pString, FALSE, TRUE);

  if (!IsValid) {
    return FALSE;
  }

  ReturnCode = ConvertStringToUint64(pString, pOutValue, FALSE, TRUE);

  if (EFI_ERROR(ReturnCode)) {
    return FALSE;
  }

  return TRUE;
}
/**
  A helper function to convert a capacity value in bytes as per the requested units

  @param[in] Capacity The input capacity in bytes
  @param[in] Units The requested type of units to convert the capacity into
  @param[out] pConvertedCapacity Pointer to the converted capacity value

  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
ConvertCapacityPerUnits (
  IN     UINT64 Capacity,
  IN     UINT16 Units,
     OUT double *pConvertedCapacity
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (pConvertedCapacity == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  switch(Units) {
    case DISPLAY_SIZE_UNIT_B:
      *pConvertedCapacity = (double) Capacity;
      break;
    case DISPLAY_SIZE_UNIT_MIB:
      *pConvertedCapacity = BYTES_TO_MIB_DOUBLE(Capacity);
      break;
    case DISPLAY_SIZE_UNIT_MB:
      *pConvertedCapacity = BYTES_TO_MB_DOUBLE(Capacity);
      break;
    case DISPLAY_SIZE_UNIT_GIB:
      *pConvertedCapacity = BYTES_TO_GIB_DOUBLE(Capacity);
      break;
    case DISPLAY_SIZE_UNIT_GB:
      *pConvertedCapacity = BYTES_TO_GB_DOUBLE(Capacity);
      break;
    case DISPLAY_SIZE_UNIT_TIB:
      *pConvertedCapacity = BYTES_TO_TIB_DOUBLE(Capacity);
      break;
    case DISPLAY_SIZE_UNIT_TB:
      *pConvertedCapacity = BYTES_TO_TB_DOUBLE(Capacity);
      break;
    default:
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Make the capacity string based on the requested units

  @param[in] Capacity The input capacity in bytes
  @param[in] CurrentUnits The requested type of units to convert the capacity into
  @param[in] AppendUnits Flag to append units to the resulting capacity string
  @param[out] ppCapacityStr Pointer to string that displays the capacity and units

  @retval EFI_INVALID_PARAMETER if input parameter is null
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
MakeCapacityString (
  IN     UINT64 Capacity,
  IN     UINT16 CurrentUnits,
  IN     BOOLEAN AppendUnits,
     OUT CHAR16 **ppCapacityStr
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 UnitsSelector = 0;
  double ConvertedCapacity = 0.0;
  CHAR16 *pUnitsStr = NULL;
  CHAR16 *pDigitsStr = NULL;

  NVDIMM_ENTRY();

  UnitsSelector = CurrentUnits;

  if (ppCapacityStr == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Get the best way to display the capacity if the request is AUTO/AUTO_10 **/
  if ((CurrentUnits == DISPLAY_SIZE_UNIT_AUTO) ||
      (CurrentUnits == DISPLAY_SIZE_UNIT_AUTO_10)) {
    UnitsSelector = GetBestDisplayForCapacity(Capacity, CurrentUnits);
  }

  if (UnitsSelector != DISPLAY_SIZE_UNIT_B) {
    ReturnCode = ConvertCapacityPerUnits(Capacity, UnitsSelector, &ConvertedCapacity);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  if (!AppendUnits) {
    pUnitsStr = CatSPrintClean(pUnitsStr, L"");
  } else {
    pUnitsStr = CatSPrintClean(pUnitsStr, L" " FORMAT_STR, UnitsToStr(UnitsSelector));
  }

  pDigitsStr = GetDigitsStrAfterPointFromNumber(ConvertedCapacity, 1);
  if (pDigitsStr == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (UnitsSelector == DISPLAY_SIZE_UNIT_B) {
    *ppCapacityStr = CatSPrintClean(*ppCapacityStr, FORMAT_UINT64 FORMAT_STR, Capacity, pUnitsStr);
  } else {
    *ppCapacityStr = CatSPrintClean(*ppCapacityStr, FORMAT_UINT64 L"." FORMAT_STR FORMAT_STR, (UINT64)ConvertedCapacity, pDigitsStr, pUnitsStr);
  }

Finish:
  FREE_POOL_SAFE(pUnitsStr);
  FREE_POOL_SAFE(pDigitsStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Gets the best possible units to display capacity

  @param[in] Capacity The input capacity in bytes
  @param[in] CurrentUnits The units for best possible representation of capacity

  @retval The optimal units to use for the given combination
**/
UINT16
GetBestDisplayForCapacity (
  IN     UINT64 Capacity,
  IN     UINT16 CurrentUnits)
{
  UINT16 OptimalUnits = DISPLAY_SIZE_UNIT_GIB;

  if (CurrentUnits == DISPLAY_SIZE_UNIT_AUTO) {
    if (((Capacity/BYTES_IN_MEBIBYTE)/BYTES_IN_MEBIBYTE > 0) != 0) {
      OptimalUnits = DISPLAY_SIZE_UNIT_TIB;
    } else if ((Capacity/BYTES_IN_GIBIBYTE > 0) != 0) {
      OptimalUnits = DISPLAY_SIZE_UNIT_GIB;
    } else if ((Capacity/BYTES_IN_MEBIBYTE > 0) != 0) {
      OptimalUnits = DISPLAY_SIZE_UNIT_MIB;
    } else {
      OptimalUnits = DISPLAY_SIZE_UNIT_B;
    }
  } else if (CurrentUnits == DISPLAY_SIZE_UNIT_AUTO_10) {
    if (((Capacity/BYTES_IN_MEGABYTE)/BYTES_IN_MEGABYTE > 0) != 0) {
      OptimalUnits = DISPLAY_SIZE_UNIT_TB;
    } else if ((Capacity/BYTES_IN_GIGABYTE > 0) != 0) {
      OptimalUnits = DISPLAY_SIZE_UNIT_GB;
    } else if ((Capacity/BYTES_IN_MEGABYTE > 0) != 0) {
      OptimalUnits = DISPLAY_SIZE_UNIT_MB;
    } else {
      OptimalUnits = DISPLAY_SIZE_UNIT_B;
    }
  }
  return OptimalUnits;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pResultString == NULL) {
    goto Finish;
  }

  if (pString == NULL || NumberPreferred) {
    UnicodeSPrint(pResultString, (ResultStringLen - 1) * sizeof(*pResultString), L"0x%04x", Number);
  } else {
    StrnCpy(pResultString, pString, ResultStringLen - 1);
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if ((ConversionFactor == 0) || (pCapacityBytes == NULL))  {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  //Overflow check
  if (Capacity > (double)(MAX_UINT64_VALUE / ConversionFactor)) {
    ReturnCode = EFI_BAD_BUFFER_SIZE;
    goto Finish;
  }

  *pCapacityBytes = (UINT64)(Capacity * ConversionFactor);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
     OUT UINT64 *pCapacityBytes)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (pCapacityBytes == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  switch(Units) {
    case DISPLAY_SIZE_UNIT_B:
      *pCapacityBytes = (UINT64) Capacity;
      break;
    case DISPLAY_SIZE_UNIT_MIB:
      ReturnCode = SafeCapacityConversionToBytes(Capacity, BYTES_IN_MEBIBYTE, pCapacityBytes);
      break;
    case DISPLAY_SIZE_UNIT_MB:
      ReturnCode = SafeCapacityConversionToBytes(Capacity, BYTES_IN_MEGABYTE, pCapacityBytes);
      break;
    case DISPLAY_SIZE_UNIT_GIB:
      ReturnCode = SafeCapacityConversionToBytes(Capacity, BYTES_IN_GIBIBYTE, pCapacityBytes);
      break;
    case DISPLAY_SIZE_UNIT_GB:
      ReturnCode = SafeCapacityConversionToBytes(Capacity, BYTES_IN_GIGABYTE, pCapacityBytes);
      break;
    case DISPLAY_SIZE_UNIT_TIB:
      ReturnCode = SafeCapacityConversionToBytes(Capacity, BYTES_IN_TEBIBYTE, pCapacityBytes);
      break;
    case DISPLAY_SIZE_UNIT_TB:
      ReturnCode = SafeCapacityConversionToBytes(Capacity, BYTES_IN_TERABYTE, pCapacityBytes);
      break;
    default:
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get physical block size - aligned to cache line size

  @param[in] BlockSize Usable block size

  @retval Physical block size
**/
UINT64
GetPhysicalBlockSize(
  IN     UINT64 BlockSize
  )
{
  return (BlockSize == AD_NAMESPACE_BLOCK_SIZE) ? AD_NAMESPACE_BLOCK_SIZE : ROUNDUP(BlockSize, CACHE_LINE_SIZE);
}

/**
  Get granularity size for a stored granularity value

  @param[in] AppDirectGranularity stored as driver preference value

  @retval granularity size
**/
UINT64
ConvertAppDirectGranularityPreference(
  IN      UINT8 AppDirectGranularity
  )
{
  switch (AppDirectGranularity) {
  case APPDIRECT_GRANULARITY_1GIB:
    return SIZE_1GB;
    break;
  case APPDIRECT_GRANULARITY_32GIB:
  default:
    return SIZE_32GB;
  }
}
