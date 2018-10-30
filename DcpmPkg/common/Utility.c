/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Uefi.h>
#include <Base.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HiiLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FirmwareManagement.h>
#include <Protocol/BlockIo.h>
#include <Guid/MdeModuleHii.h>
#include <Guid/FileInfo.h>
#include "Utility.h"
#include <Guid/FileInfo.h>
#include <Protocol/DriverHealth.h>
#include <Library/DevicePathLib.h>
#include <Debug.h>
#include <NvmTypes.h>
#include <NvmInterface.h>
#include <Convert.h>
#ifdef OS_BUILD
#include <os.h>
#include <string.h>
#endif

#if defined(__LINUX__)
#include <safe_mem_lib.h>
#endif

extern EFI_GUID gNvmDimmConfigProtocolGuid;
extern EFI_GUID gIntelDimmConfigVariableGuid;
CHAR16 gFnName[1024];

#ifdef _MSC_VER
int _fltused() {
  return 0;
}
#endif


#define NOT_RFC4646_ABRV_LANGUAGE_LEN 3

#if defined(DYNAMIC_WA_ENABLE)
/**
  Local define of the Shell Protocol GetEnv function. The local definition allows the driver to use
  this function without including the Shell headers.
**/
typedef
CONST CHAR16 *
(EFIAPI *EFI_SHELL_GET_ENV_LOCAL) (
  IN CONST CHAR16 *Name OPTIONAL
);

/**
  Local, partial definition of the Shell Protocol, the first Reserved value is a pointer to a different function,
  but we don't need it here so it is masked as reserved. We ignore any functions after the GetEnv one.
**/
typedef struct {
  VOID *Reserved;
  EFI_SHELL_GET_ENV_LOCAL GetEnv;
} EFI_SHELL_PROTOCOL_GET_ENV;

/**
  Returns the value of the environment variable with the given name.

  @param[in] pVarName Unicode name of the variable to retrieve

  @retval NULL if the shell protocol could not be located or if the variable is not defined in the system
  @retval pointer to the Unicode string containing the variable value
**/
CHAR16 *
GetEnvVariable(
  IN     CHAR16 *pVarName
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pReturn = NULL;
  EFI_SHELL_PROTOCOL_GET_ENV *pGetEnvShell = NULL;
  EFI_GUID GetEnvShellProtGuid = EFI_SHELL_PROTOCOL_GUID;

  ReturnCode = gBS->LocateProtocol(&GetEnvShellProtGuid, NULL, (VOID **)&pGetEnvShell);
  if (!EFI_ERROR(ReturnCode)) {
    pReturn = (CHAR16 *)pGetEnvShell->GetEnv(pVarName);
  }

  return pReturn;
}
#endif

/**
  Generates namespace type string, caller must free it

  @param[in] Type, value corresponding to namespace type.

  @retval Pointer to type string
**/
CHAR16*
NamespaceTypeToString(
  IN     UINT8 Type
  )
{
  CHAR16 *pTypeString = NULL;
  switch(Type) {
    case APPDIRECT_NAMESPACE:
      pTypeString = CatSPrint(NULL, FORMAT_STR, L"AppDirect");
      break;
    default:
      pTypeString = CatSPrint(NULL, FORMAT_STR, L"Unknown");
      break;
  }
  return pTypeString;
}

/**
  Generates pointer to string with value corresponding to health state
  Caller is responsible for FreePool on this pointer
**/
CHAR16*
NamespaceHealthToString(
  IN     UINT16 Health
  )
{
  CHAR16 *pHealthString = NULL;
  switch(Health) {
    case NAMESPACE_HEALTH_OK:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHSTATE_OK);
      break;
    case NAMESPACE_HEALTH_WARNING:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHSTATE_WARNING);
      break;
    case NAMESPACE_HEALTH_CRITICAL:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHSTATE_CRITICAL);
      break;
    case NAMESPACE_HEALTH_UNSUPPORTED:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHSTATE_UNSUPPORTED);
      break;
    case NAMESPACE_HEALTH_LOCKED:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHSTATE_LOCKED);
      break;
    default:
      pHealthString = CatSPrint(NULL, FORMAT_STR, HEALTHSTATE_UNKNOWN);
      break;
  }
  return pHealthString;
}

/**
  Check if LIST_ENTRY list is initialized

  @param[in] ListHead list head

  @retval BOOLEAN list initialization status
**/
BOOLEAN
IsListInitialized(
  IN     LIST_ENTRY ListHead
  )
{
  return !(ListHead.BackLink == NULL || ListHead.ForwardLink == NULL ||
      ListHead.BackLink == BAD_POINTER || ListHead.ForwardLink == BAD_POINTER);
}

/**
  Calculate checksum using Fletcher64 algorithm and compares it at the given offset.
  The length parameter must be aligned to 4 (32bit).

  @param[in] pAddress Starting address of area to calculate checksum on
  @param[in] Length Length of area over which checksum is calculated
  @param[in, out] pChecksum, the pointer where the checksum lives in
  @param[in] Insert, flag telling if the checksum should be inserted at the specified address or just compared to it

  @retval TRUE if the compared checksums are equal
  @retval FALSE if the checksums differ or the input parameters are invalid
    (a NULL was passed or the length is not aligned)
**/
BOOLEAN
ChecksumOperations(
  IN     VOID *pAddress,
  IN     UINT64 Length,
  IN OUT UINT64 *pChecksum,
  IN     BOOLEAN Insert
  )
{
  UINT32 *p32 = pAddress;
  UINT32 *p32End = (UINT32 *)((UINT8 *)pAddress + Length);
  UINT32 Lo32 = 0;
  UINT32 Hi32 = 0;
  UINT64 Checksum = 0;
  BOOLEAN ChecksumMatch = FALSE;

  if ((Length % sizeof(UINT32)) != 0) {
    NVDIMM_DBG("The size specified for the checksum is not properly aligned");
    return FALSE;
  }

  if (((UINT64) pAddress % sizeof(UINT32)) != ((UINT64) pChecksum % sizeof(UINT32))) {
    NVDIMM_DBG("The address and the checksum address are not aligned together");
    return FALSE;
  }

  if (pAddress == NULL || pChecksum == NULL) {
    NVDIMM_DBG("The address or checksum pointer equal NULL");
    return FALSE;
  }

  while (p32 < p32End) {
    if (p32 == (UINT32 *) pChecksum) {
     /* Lo32 += 0; treat first 32-bits as zero */
      p32++;
      Hi32 += Lo32;
      /* Lo32 += 0; treat second 32-bits as zero */
      p32++;
      Hi32 += Lo32;
    } else {
      Lo32 += *p32;
      ++p32;
      Hi32 += Lo32;
    }
  }

  Checksum = (UINT64) Hi32 << 32 | Lo32;

  if (Insert) {
    *pChecksum = Checksum;
    return TRUE;
  }

  ChecksumMatch = (*pChecksum == Checksum);

  if (!ChecksumMatch) {
    NVDIMM_DBG("Checksum = %llx", *pChecksum);
    NVDIMM_DBG("Calculated checksum = %llx", Checksum);
  }

  return ChecksumMatch;
}

/**
  Compares the two provided 128bit unsigned ints.

  @param[in] LeftValue is the first 128bit uint.
  @param[in] RightValue is the second 128bit uint.

  @retval -1 when the LeftValue is smaller than
    the RightValue
  @retval 0 when the provided values are the same
  @retval 1 when the LeftValue is bigger than
    the RithValue
**/
INT8
CompareUint128(
  IN     UINT128 LeftValue,
  IN     UINT128 RightValue
  )
{
  if (LeftValue.Uint64_1 > RightValue.Uint64_1) {
    return 1;
  } else if (LeftValue.Uint64_1 == RightValue.Uint64_1) {
    if (LeftValue.Uint64 > RightValue.Uint64) {
      return 1;
    } else if (LeftValue.Uint64 == RightValue.Uint64) {
      return 0;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

/**
  The Print function is not able to print long strings.
  This function is dividing the input string to safe lengths
  and prints all of the parts.

  @param[in] pString - the Unicode string to be printed
**/
VOID
LongPrint(
  IN     CHAR16 *pString
  )
{
  UINT32 StrOffset = 0;
  UINT32 MaxToPrint = PCD_UEFI_LIB_MAX_PRINT_BUFFER_SIZE;
  CHAR16 TempChar = L'\0';

  if (pString == NULL) {
    return;
  }

  while (pString[0] != L'\0') {
    while (pString[StrOffset] != L'\0' && StrOffset < MaxToPrint) {
      StrOffset++;
    }
    if (StrOffset == MaxToPrint) {
      TempChar = pString[StrOffset]; // Remember it and put a NULL there
      pString[StrOffset] = L'\0';
      Print(pString); // Print the string up to the newline
      pString[StrOffset] = TempChar;// Put back the stored value.
      pString += StrOffset; // Move the pointer over the printed part and the '\n'
      StrOffset = 0;
    } else { // There is a NULL after the newline or there is just a NULL
      Print(pString);
      break;
    }
  }
}

/**
  Tokenize a string by the specified delimiter and update
  the input to the remainder.
  NOTE:  Returned token needs to be freed by the caller
**/
CHAR16 *StrTok(CHAR16 **input, CONST CHAR16 delim)
{
  CHAR16 *token;
  UINT16 tokenLength;
  UINTN i;
  UINTN j;
  BOOLEAN found;

  found = FALSE;
  token = NULL;

  /** check input **/
  if ((input != NULL) && (*input != NULL) && ((*input)[0] != 0)) {
    i = 0;
    while ((*input)[i] != 0) {
      /** found the delimiter **/
      if ((*input)[i] == delim) {
        found = TRUE;
        /** create the token **/
        token = AllocatePool((i + 1) * sizeof(CHAR16));
        if (!token) {
          NVDIMM_DBG("StrTok failed due to lack of resources");
        } else {
          /** copy the token **/
          for (j = 0; j < i; j++) {
            token[j] = (*input)[j];
          }
          token[j] = 0; /** null terminate **/

          /** reset the input to the remainder **/
          for (j = i; j < StrLen(*input); j++) {
            (*input)[j - i] = (*input)[j + 1];
          }
        }
        break;
      }
      i++;
    }

    /**
      set the token to the end of the string
      and set the remainder to null
    **/
    if (!found) {
      tokenLength = (UINT16)StrLen(*input) + 1;
      token = AllocatePool(tokenLength * sizeof(CHAR16));
      if (!token) {
        NVDIMM_DBG("StrTok failed due to lack of resources");
      } else {
        StrnCpyS(token, tokenLength, *input, tokenLength - 1);
      }
      /** set input to null **/
      (*input)[0] = 0;
    }
  }

  return token;
}

/**
  Tokenize provided ASCII string

  @param[in] ppInput     Input string
  @param[in] pDelimiter Delimiter character

  @retval Pointer to token string
**/
CHAR8 *AsciiStrTok(CHAR8 **ppInput, CONST CHAR8 delim)
{
  CHAR8 *pToken = NULL;
  UINT16 TokenLength = 0;
  UINTN Index = 0;
  UINTN Index2 = 0;
  BOOLEAN Found = FALSE;

  /** check input **/
  if ((ppInput != NULL) && (*ppInput != NULL) && ((*ppInput)[0] != 0)) {
    Index = 0;
    while ((*ppInput)[Index] != 0) {
      /** found the delimiter **/
      if ((*ppInput)[Index] == delim) {
        Found = TRUE;
        /** create the token **/
        pToken = AllocatePool((Index + 1) * sizeof(CHAR8));
        if (!pToken) {
          NVDIMM_DBG("StrTok failed due to lack of resources");
        } else {
          /** copy the token **/
          for (Index2 = 0; Index2 < Index; Index2++) {
            pToken[Index2] = (*ppInput)[Index2];
          }
          pToken[Index2] = 0; /** null terminate **/

          /** reset the input to the remainder **/
          for (Index2 = Index; Index2 < AsciiStrLen(*ppInput); Index2++) {
            (*ppInput)[Index2 - Index] = (*ppInput)[Index2 + 1];
          }
        }
        break;
      }
      Index++;
    }

    /**
      set the token to the end of the string
      and set the remainder to null
    **/
    if (!Found) {
      TokenLength = (UINT16)AsciiStrLen(*ppInput) + 1;
      pToken = AllocatePool(TokenLength * sizeof(CHAR8));
      if (!pToken) {
        NVDIMM_DBG("StrTok failed due to lack of resources");
      } else {
        AsciiStrnCpyS(pToken, TokenLength, *ppInput, TokenLength - 1);
      }
      /** set input to null **/
      (*ppInput)[0] = 0;
    }
  }

  return pToken;
}

/**
  Split a string by the specified delimiter and return the split string as a string array.

  The caller is responsible for a memory deallocation of the returned array and its elements.

  @param[in] pInput the input string to split
  @param[in] Delimiter delimiter to split the string
  @param[out] pArraySize array size will be put here

  @retval NULL at least one of parameters is NULL or memory allocation failure
  @retval the split input string as an array
**/
CHAR16 **
StrSplit(
  IN     CHAR16 *pInput,
  IN     CHAR16 Delimiter,
     OUT UINT32 *pArraySize
  )
{
  CHAR16 **ppArray = NULL;
  CHAR16 *pInputTmp = NULL;
  UINT32 Index = 0;
  UINT32 DelimiterCounter = 0;

  if (pInput == NULL || pArraySize == NULL) {
    NVDIMM_DBG("At least one of parameters is NULL.");
    goto Finish;
  }

  if (pInput[0] == L'\0') {
    goto Finish;
  }

  /**
    Count the number of delimiter in the string
  **/
  for (Index = 0; pInput[Index] != L'\0'; Index++) {
    if (pInput[Index] == Delimiter) {
      DelimiterCounter += 1;
    }
  }

  /**
    1. "A,B,C": 2 delimiter, 3 array elements
    2. "A,B,":  2 delimiter, 2 array elements - StrTok returns NULL if there is '\0' after the last delimiter
                                                instead of empty string
  **/
  if (pInput[Index - 1] != Delimiter) {
    DelimiterCounter += 1;
  }

  *pArraySize = DelimiterCounter;

  /**
    Allocate an array memory and fill it with split input string
  **/

  ppArray = AllocateZeroPool(*pArraySize * sizeof(CHAR16 *));
  if (ppArray == NULL) {
    NVDIMM_ERR("Memory allocation failed.");
    goto Finish;
  }

  /** Copy the input to a tmp var to avoid changing it **/
  pInputTmp = CatSPrint(NULL, FORMAT_STR, pInput);
  if (pInputTmp == NULL) {
    NVDIMM_ERR("Memory allocation failed.");
    goto FinishCleanMemory;
  }

  for (Index = 0; Index < *pArraySize; Index++) {
    ppArray[Index] = StrTok(&pInputTmp, Delimiter);
    if (ppArray[Index] == NULL) {
      goto FinishCleanMemory;
    }
  }
  /** Success path **/
  goto Finish;

  /** Error path **/
FinishCleanMemory:
  FreeStringArray(ppArray, *pArraySize);
  ppArray = NULL;
  *pArraySize = 0;

Finish:
  FREE_POOL_SAFE(pInputTmp);
  return ppArray;
}

/**
  Split an ASCII string by the specified delimiter and return the split string as a string array.

  The caller is responsible for a memory deallocation of the returned array and its elements.

  @param[in] pInput the input string to split
  @param[in] Delimiter delimiter to split the string
  @param[out] pArraySize array size will be put here

  @retval NULL at least one of parameters is NULL or memory allocation failure
  @retval the split input string as an array
**/
CHAR8 **
AsciiStrSplit(
  IN     CHAR8 *pInput,
  IN     CHAR8 Delimiter,
     OUT UINT32 *pArraySize
  )
{
  CHAR8 **ppArray = NULL;
  CHAR8 *pInputTmp = NULL;
  UINT32 Index = 0;
  UINT32 DelimiterCounter = 0;

  if (pInput == NULL || pArraySize == NULL) {
    NVDIMM_DBG("At least one of parameters is NULL.");
    goto Finish;
  }

  if (pInput[0] == '\0') {
    goto Finish;
  }

  /**
    Count the number of delimiter in the string
  **/
  for (Index = 0; pInput[Index] != '\0'; Index++) {
    if (pInput[Index] == Delimiter) {
      DelimiterCounter += 1;
    }
  }

  /**
    1. "A,B,C": 2 delimiter, 3 array elements
    2. "A,B,":  2 delimiter, 2 array elements - StrTok returns NULL if there is '\0' after the last delimiter
                                                instead of empty string
  **/
  if (pInput[Index - 1] != Delimiter) {
    DelimiterCounter += 1;
  }

  *pArraySize = DelimiterCounter;

  /**
    Allocate an array memory and fill it with split input string
  **/

  ppArray = AllocateZeroPool(*pArraySize * sizeof(CHAR8 *));
  if (ppArray == NULL) {
    NVDIMM_ERR("Memory allocation failed.");
    goto FinishCleanMemory;
  }

  /** Copy the input to a tmp var to avoid changing it **/
  pInputTmp = AllocateZeroPool(AsciiStrSize(pInput));
  if (pInputTmp == NULL) {
    NVDIMM_ERR("Memory allocation failed.");
    goto FinishCleanMemory;
  }

  AsciiStrnCpyS(pInputTmp, AsciiStrSize(pInput) / sizeof(CHAR8), pInput, (AsciiStrSize(pInput) / sizeof(CHAR8)) - 1);

  for (Index = 0; Index < *pArraySize; Index++) {
    ppArray[Index] = AsciiStrTok(&pInputTmp, Delimiter);
    if (ppArray[Index] == NULL) {
      goto FinishCleanMemory;
    }
  }
  /** Success path **/
  goto Finish;

  /** Error path **/
FinishCleanMemory:
  FreeStringArray((CHAR16**)ppArray, *pArraySize);
  ppArray = NULL;
  *pArraySize = 0;

Finish:
  FREE_POOL_SAFE(pInputTmp);
  return ppArray;
}

/**
  First free elements of array and then free the array
  This does NOT set pointer to array to NULL

  @param[in,out] ppStringArray array of strings
  @param[in] ArraySize number of strings
**/
VOID
FreeStringArray(
  IN OUT CHAR16 **ppStringArray,
  IN     UINT32 ArraySize
  )
{
  UINT32 Index = 0;

  if (ppStringArray == NULL) {
    return;
  }

  for (Index = 0; Index < ArraySize; Index++) {
    FREE_POOL_SAFE(ppStringArray[Index]);
  }

  FreePool(ppStringArray);
}

/**
  Function that allows for nicely formatted HEX & ASCII debug output.
  It can be used to inspect memory objects without a need for debugger

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/

#define COLUMN_IN_HEX_DUMP   16

VOID
HexDebugPrint(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
  )
{
  UINT8 Byte, AsciiBuffer[COLUMN_IN_HEX_DUMP];
  UINT16 Column, NextColumn, Index, Index2;
  UINT8 *pData;

  if (pBuffer == NULL) {
    NVDIMM_DBG("pBuffer is NULL");
    return;
  }
  DebugPrint(EFI_D_INFO, "Hexdump starting at: 0x%p\n", pBuffer);
  pData = (UINT8 *) pBuffer;
  for (Index = 0; Index < Bytes; Index++) {
    Column = Index % COLUMN_IN_HEX_DUMP;
    NextColumn = (Index + 1) % COLUMN_IN_HEX_DUMP;
    Byte = *(pData + Index);
    if (Column == 0) {
      DebugPrint(EFI_D_INFO, "%.3d:", Index);
    }
    if (Index % 8 == 0) {
      DebugPrint(EFI_D_INFO, " ");
    }
    DebugPrint(EFI_D_INFO, "%.2x", *(pData + Index));
    AsciiBuffer[Column] = IsAsciiAlnumCharacter(Byte) ? Byte : '.';
    if (NextColumn == 0 && Index != 0) {
      DebugPrint(EFI_D_INFO, " ");
      for (Index2 = 0; Index2 < COLUMN_IN_HEX_DUMP; Index2++) {
        DebugPrint(EFI_D_INFO, "%c", AsciiBuffer[Index2]);
        if (Index2 == COLUMN_IN_HEX_DUMP / 2 - 1) {
          DebugPrint(EFI_D_INFO, " ");
        }
      }
      DebugPrint(EFI_D_INFO, "\n");
    }
  }
}

/**
  Function that allows for nicely formatted HEX & ASCII console output.
  It can be used to inspect memory objects without a need for debugger or dumping raw DIMM data.

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/
VOID
HexPrint(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
  )
{
  UINT8 Byte, AsciiBuffer[COLUMN_IN_HEX_DUMP];
  UINT16 Column, NextColumn, Index, Index2;
  UINT8 *pData;

  if (pBuffer == NULL) {
    NVDIMM_DBG("pBuffer is NULL");
    return;
  }
  Print(L"Hexdump for %d bytes:\n", Bytes);
  pData = (UINT8 *)pBuffer;
  for (Index = 0; Index < Bytes; Index++) {
    Column = Index % COLUMN_IN_HEX_DUMP;
    NextColumn = (Index + 1) % COLUMN_IN_HEX_DUMP;
    Byte = *(pData + Index);
    if (Column == 0) {
      Print(L"%.3d:", Index);
    }
    if (Index % 8 == 0) {
      Print(L" ");
    }
    Print(L"%.2x", *(pData + Index));
    AsciiBuffer[Column] = IsAsciiAlnumCharacter(Byte) ? Byte : '.';
    if (NextColumn == 0 && Index != 0) {
      Print(L" ");
      for (Index2 = 0; Index2 < COLUMN_IN_HEX_DUMP; Index2++) {
        Print(L"%c", AsciiBuffer[Index2]);
        if (Index2 == COLUMN_IN_HEX_DUMP / 2 - 1) {
          Print(L" ");
        }
      }
      Print(L"\n");
    }
  }
}

/**
  Return a first found handle for specified protocol.

  @param[in] pProtocolGuid protocol that EFI handle will be found for.
  @param[out] pDriverHandle is the pointer to the result handle.

  @retval EFI_INVALID_PARAMETER if one or more input parameters are NULL.
  @retval all of the LocateHandleBuffer return values.
**/
EFI_STATUS
GetDriverHandle(
  IN     EFI_GUID *pProtocolGuid,
     OUT EFI_HANDLE *pDriverHandle
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN HandleCount = 0;
  EFI_HANDLE *pHandleBuffer = NULL;

  NVDIMM_ENTRY();

  if (pProtocolGuid == NULL || pDriverHandle == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pDriverHandle = NULL;

  /**
    Find the driver handle by searching for our custom NvmDimmConfig protocol
  **/
  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, pProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(ReturnCode) || HandleCount != 1) {
    ReturnCode = EFI_NOT_FOUND;
  } else {
    *pDriverHandle = pHandleBuffer[0];
  }

Finish:
  FREE_POOL_SAFE(pHandleBuffer);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Return the NvmDimmController handle.

  @param[out] pControllerHandle is the pointer to the result handle.

  @retval EFI_INVALID_PARAMETER if the pControllerHandle is NULL.
  @retval all of the LocateHandleBuffer return values.
**/
EFI_STATUS
GetControllerHandle(
     OUT EFI_HANDLE *pControllerHandle
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  EFI_HANDLE DriverHandle = NULL;
  UINT32 Index = 0;
  EFI_HANDLE *pHandleBuffer = NULL;
  UINT64 HandleCount = 0;

  NVDIMM_ENTRY();

  if (pControllerHandle == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pControllerHandle = NULL;

  ReturnCode = GetDriverHandle(&gNvmDimmConfigProtocolGuid, &DriverHandle);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /**
    Retrieve array of all handles in the handle database
  **/
  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, &gEfiDevicePathProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not find the handles set.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  /**
    Search all of the existing device path protocol instances for a device controlled by our driver.
  **/
  for (Index = 0; Index < HandleCount; Index++) {
    ReturnCode =
		EfiTestManagedDevice(pHandleBuffer[Index],
        DriverHandle, // Our driver handle equals the driver binding handle so this call is valid
        &gEfiDevicePathProtocolGuid);

    // If the handle is managed - this is our controller.
    if (!EFI_ERROR(ReturnCode)) {
      *pControllerHandle = pHandleBuffer[Index];
      break;
    }
  }

Finish:
  if (pHandleBuffer != NULL) {
    FreePool(pHandleBuffer);
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if the Config Protocol version is right.

  @param[in] *pConfigProtocol, instance of the protocol to check

  @retval EFI_SUCCESS if the version matches.
  @retval EFI_INVALID_PARAMETER if the passed parameter equals to NULL.
  @retval EFI_INCOMPATIBLE_VERSION when the version is wrong.
**/
EFI_STATUS
CheckConfigProtocolVersion(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pConfigProtocol
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CONFIG_PROTOCOL_VERSION CurrentVersion;
  CONFIG_PROTOCOL_VERSION OpenedProtocolVersion;

  if (pConfigProtocol == NULL) {
    goto Finish;
  }

  ZeroMem(&OpenedProtocolVersion, sizeof(OpenedProtocolVersion));

  CurrentVersion.AsUint32 = NVMD_CONFIG_PROTOCOL_VERSION;

  NVDIMM_ENTRY();

  OpenedProtocolVersion.AsUint32 = pConfigProtocol->Version;
  if ((OpenedProtocolVersion.Separated.Major != CurrentVersion.Separated.Major)
      || (OpenedProtocolVersion.Separated.Minor != CurrentVersion.Separated.Minor)) {
    NVDIMM_ERR("The Config Protocol version is mismatching");
    ReturnCode = EFI_INCOMPATIBLE_VERSION;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Open the specified protocol.
  If the user does not provide a handle, the function will try
  to match the driver or the controller handle basing on the
  provided protocol GUID.
  No need to call close protocol because of the way it's opened.

  @param[in] Guid is the EFI GUID of the protocol we want to open.
  @param[out] ppProtocol is the pointer to a pointer where the opened
    protocol instance address will be returned.
  @param[in] pHandle a handle that we want to open the protocol on. OPTIONAL

  @retval EFI_SUCCESS if everything went successful.
  @retval EFI_INVALID_ARGUMENT if ppProtocol is NULL.

  Other return values from functions:
    getControllerHandle
    getDriverHandle
    gBS->OpenProtocol
**/
EFI_STATUS
OpenNvmDimmProtocol(
  IN     EFI_GUID Guid,
     OUT VOID **ppProtocol,
  IN     EFI_HANDLE pHandle OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_HANDLE DeviceHandle = NULL;

  NVDIMM_ENTRY();

  if (pHandle == NULL) {
    if (CompareGuid(&Guid, &gEfiDevicePathProtocolGuid)) {
      ReturnCode = GetControllerHandle(&DeviceHandle);
    } else {
      ReturnCode = GetDriverHandle(&gNvmDimmConfigProtocolGuid, &DeviceHandle);
    }
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not determine the target device type, error = " FORMAT_EFI_STATUS "", ReturnCode);
      goto Finish;
    }
  } else {
    DeviceHandle = pHandle;
  }

  ReturnCode = gBS->OpenProtocol(
    DeviceHandle,
    &Guid,
    ppProtocol,
    NULL,
    NULL,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_ALREADY_STARTED) {
      ReturnCode = EFI_SUCCESS;
    } else {
      NVDIMM_WARN("Failed to open NvmDimmProtocol, error = " FORMAT_EFI_STATUS "", ReturnCode);
      goto Finish;
    }
  }

  if (CompareGuid(&Guid, &gNvmDimmConfigProtocolGuid)) {
    ReturnCode = CheckConfigProtocolVersion((EFI_DCPMM_CONFIG_PROTOCOL *) *ppProtocol);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to get the proper config protocol.");
      ppProtocol = NULL;
    }
  }


Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Open file or create new file.

  @param[in] pArgFilePath path to a file that will be opened
  @param[out] pFileHandle output handler
  @param[in, optional] pCurrentDirectory is the current directory path to where
    we should start to search for the file.
  @param[in] CreateFileFlag - TRUE to create new file or FALSE to open
    existing file

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER pFilePath is NULL or empty or pFileHandle is NULL
  @retval EFI_PROTOCOL_ERROR if there is no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
**/
EFI_STATUS
OpenFile(
  IN     CHAR16 *pArgFilePath,
     OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pVolume = NULL;
  CHAR16 *pFullFilePath = NULL;
  CHAR16 *pFilePath = NULL;
  EFI_FILE_HANDLE RootDirHandle = NULL;
  EFI_HANDLE *pHandles = NULL;
  UINTN HandlesSize = 0;
  INT32 Index = 0;
  UINTN CurrentWorkingDirLength = 0;
  NVDIMM_ENTRY();

  /**
     @todo: This function have problem in two scenarios:
     1) There are two or more file systems on platform. At least two of them contains file
        with provided name. There is no way to tell which one would be opened.
        It depends on enumeration order returned by LocateHandleBuffer.
     2) Provided file name contains more than one dot (example: file.1.0.img),
        there is more than one file system on the platform and said file is not on the first one
        enumerated in pHandles returned by LocateHandleBuffer.
        In this case open and read succeed on each file system but data returned by read on those that
        do not contain said file is garbage. This is UDK issue.
  **/

  if (pArgFilePath == NULL || pArgFilePath[0] == '\0' || pFileHandle == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pCurrentDirectory != NULL && pArgFilePath[0] != '\\') {
    CurrentWorkingDirLength = StrLen(pCurrentDirectory);
    if (CurrentWorkingDirLength != 0 && pCurrentDirectory[CurrentWorkingDirLength - 1] != '\\') {
      pFullFilePath = CatSPrint(NULL, FORMAT_STR L"\\" FORMAT_STR, pCurrentDirectory, pArgFilePath);
    } else {
      pFullFilePath = CatSPrint(NULL, FORMAT_STR FORMAT_STR, pCurrentDirectory, pArgFilePath);
    }

    if (pFullFilePath != NULL) {
      pFilePath = pFullFilePath;
      while (pFilePath[0] != '\\' && pFilePath[0] != '\0') {
        pFilePath++;
      }
    }
  }

  Rc = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandlesSize, &pHandles);
  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Couldn't find EfiSimpleFileSystemProtocol: " FORMAT_EFI_STATUS "", Rc);
    goto Finish;
  }

  if (pFilePath == NULL) {
    pFilePath = pArgFilePath;
  }

  for (Index = 0; Index < HandlesSize; Index++) {
    /**
      Get the file system protocol
    **/
    Rc = gBS->OpenProtocol(
        pHandles[Index],
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID *) &pVolume,
        NULL,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
        );
    if (EFI_ERROR(Rc)) {
      goto AfterHandles;
    }

    /**
      Open the root file
    **/
    Rc = pVolume->OpenVolume(pVolume, &RootDirHandle);
    if (EFI_ERROR(Rc)) {
      goto AfterHandles;
    }

    if (CreateFileFlag) {
      // if EFI_FILE_MODE_CREATE then also EFI_FILE_MODE_READ and EFI_FILE_MODE_WRITE are needed.
      Rc = RootDirHandle->Open(RootDirHandle, pFileHandle, pFilePath,
        EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,
        0);
    } else {
       Rc = RootDirHandle->Open(RootDirHandle, pFileHandle, pFilePath, EFI_FILE_MODE_READ, 0);
    }

    RootDirHandle->Close(RootDirHandle);
    if (!EFI_ERROR(Rc)) {
      break;
    }
  }

AfterHandles:
  FreePool(pHandles);
Finish:

  if (pFullFilePath != NULL) {
    FreePool(pFullFilePath);
  }

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Open file handle of root directory from given path

  @param[in] pDevicePath - path to file
  @param[out] pFileHandle - root directory file handle

**/
EFI_STATUS
OpenRootFileVolume(
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT EFI_FILE_HANDLE *pRootDirHandle
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_HANDLE DeviceHandle = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathTmp = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pVolume = NULL;

  if (pDevicePath == NULL || pRootDirHandle == NULL) {
    goto Finish;
  }
  // Copy device path pointer, LocateDevicePath modifies it
  pDevicePathTmp = pDevicePath;
  // Locate Handle for Simple File System Protocol on device
  ReturnCode = gBS->LocateDevicePath(&gEfiSimpleFileSystemProtocolGuid, &pDevicePathTmp, &DeviceHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = gBS->OpenProtocol(DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID *) &pVolume, NULL,
        NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // Open the root file
  ReturnCode = pVolume->OpenVolume(pVolume, pRootDirHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Open file or create new file based on device path protocol.

  @param[in] pArgFilePath Pointer to path to a file that will be opened
  @param[in] pDevicePath Pointer to instance of device path protocol
  @param[in] CreateFileFlag - TRUE to create new file or FALSE to open
    existing file
  @param[out] pFileHandle Output file handler

  @retval EFI_SUCCESS File opened or created
  @retval EFI_INVALID_PARAMETER Input parameter is invalid
  @retval Others From LocateDevicePath, OpenProtocol, OpenVolume and Open
**/
EFI_STATUS
OpenFileByDevice(
  IN     CHAR16 *pArgFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  IN     BOOLEAN CreateFileFlag,
     OUT EFI_FILE_HANDLE *pFileHandle
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE RootDirHandle = NULL;
  NVDIMM_ENTRY();

  if (pArgFilePath == NULL || pDevicePath == NULL || pFileHandle == NULL) {
    goto Finish;
  }

  ReturnCode = OpenRootFileVolume(pDevicePath, &RootDirHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (CreateFileFlag) {
    // if EFI_FILE_MODE_CREATE then also EFI_FILE_MODE_READ and EFI_FILE_MODE_WRITE are needed.
    ReturnCode = RootDirHandle->Open(RootDirHandle, pFileHandle, pArgFilePath,
        EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,
        0);
  } else {
    ReturnCode = RootDirHandle->Open(RootDirHandle, pFileHandle, pArgFilePath, EFI_FILE_MODE_READ, 0);
  }

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Returns the size of the specified file.

  @param[in] FileHandle - handle to the opened file that we want to get the size for.
  @param[out] pFileSize - the result file size on bytes.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one of the input parameters is a NULL.

  Other return values associated with the GetInfo callback.
**/
EFI_STATUS
GetFileSize(
  IN     EFI_FILE_HANDLE FileHandle,
     OUT UINT64 *pFileSize
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 BuffSize = 0;
  EFI_FILE_INFO *pFileInfo = NULL;

  if (FileHandle == NULL || pFileSize == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pFileSize = 0;

  ReturnCode = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &BuffSize, pFileInfo);

  if (ReturnCode != EFI_BUFFER_TOO_SMALL) {
    NVDIMM_DBG("pFileHandle->GetInfo returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    goto Finish;
  }

  pFileInfo = AllocatePool(BuffSize);

  if (pFileInfo == NULL) {
    NVDIMM_DBG("Could not allocate resources.\n");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &BuffSize, pFileInfo);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("pFileHandle->GetInfo returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
  }

  *pFileSize = pFileInfo->FileSize;

  FreePool(pFileInfo);

Finish:
  return ReturnCode;
}

/**
  We need the GUID to find our HII handle. Instead of including the whole HII library, it is better
  just to declare a local copy of the GUID define and variable.
**/
#define EFI_HII_CONFIG_ACCESS_PROTOCOL_GUID_TEMP  \
{ 0x330d4706, 0xf2a0, 0x4e4f, { 0xa3, 0x69, 0xb6, 0x6f, 0xa8, 0xd5, 0x43, 0x85 } }

/**
  Convert all Interleave settings to string
  WARNING! *ppIoString can be reallocated. Calling function is responsible for its freeing.
  Additionally *ppIoString must be dynamically allocated.

  @param[in] PersistentSize - Persistent size of interleave set in DIMM
  @param[in] NumberOfInterleavedDimms - Number of interleaved DIMMs
  @param[in] ImcInterleaving - iMC interleaving bit map
  @param[in] ChannelInterleaving - Channel interleaving bit map

  @param[out] ppString - output string.
**/
VOID
InterleaveSettingsToString(
  IN     UINT64 PersistentSize,
  IN     UINT8 NumberOfInterleavedDimms,
  IN     UINT8 ImcInterleaving,
  IN     UINT8 ChannelInterleaving,
     OUT CHAR16 **ppString
  )
{
  CONST CHAR16 *pImcInterleaving = NULL;
  CONST CHAR16 *pChannelInterleaving = NULL;

  if (ppString == NULL) {
    NVDIMM_DBG("NULL parameter provided");
    return;
  }

  if (PersistentSize == 0) {
    *ppString =  CatSPrintClean(*ppString, L"N/A");
    return;
  }

  *ppString = CatSPrintClean(*ppString, L"x%d", NumberOfInterleavedDimms);

  pImcInterleaving = ParseImcInterleavingValue(ImcInterleaving);
  pChannelInterleaving = ParseChannelInterleavingValue(ChannelInterleaving);

  if (pImcInterleaving == NULL || pChannelInterleaving == NULL) {
    *ppString = CatSPrintClean(NULL, L"Error");
    return;
  }

  *ppString = CatSPrintClean(*ppString, L" - " FORMAT_STR L" IMC x " FORMAT_STR L" Channel", pImcInterleaving, pChannelInterleaving);

}

/**
  Convert Channel Interleaving value to output settings string

  @param[in] Interleaving - Channel Interleave BitMask

  @retval appropriate string
  @retval NULL - if Interleaving value is incorrect
**/
CONST CHAR16 *
ParseChannelInterleavingValue(
  IN     UINT8 Interleaving
  )
{
  if (IS_BIT_SET_VAR(Interleaving, CHANNEL_INTERLEAVE_SIZE_64B)) {
    return L"64B";
  }
  if (IS_BIT_SET_VAR(Interleaving, CHANNEL_INTERLEAVE_SIZE_128B)) {
    return L"128B";
  }
  if (IS_BIT_SET_VAR(Interleaving, CHANNEL_INTERLEAVE_SIZE_256B)) {
    return L"256B";
  }
  if (IS_BIT_SET_VAR(Interleaving, CHANNEL_INTERLEAVE_SIZE_4KB)) {
    return L"4KB";
  }
  if (IS_BIT_SET_VAR(Interleaving, CHANNEL_INTERLEAVE_SIZE_1GB)) {
    return L"1GB";
  }
  return NULL;
}

/**
  Convert iMC Interleaving value to output settings string

  @param[in] Interleaving - iMC Interleave BitMask

  @retval appropriate string
  @retval NULL - if Interleaving value is incorrect
**/
CONST CHAR16 *
ParseImcInterleavingValue(
  IN     UINT8 Interleaving
  )
{
  if (IS_BIT_SET_VAR(Interleaving, IMC_INTERLEAVE_SIZE_64B)) {
    return L"64B";
  }
  if (IS_BIT_SET_VAR(Interleaving, IMC_INTERLEAVE_SIZE_128B)) {
    return L"128B";
  }
  if (IS_BIT_SET_VAR(Interleaving, IMC_INTERLEAVE_SIZE_256B)) {
    return L"256B";
  }
  if (IS_BIT_SET_VAR(Interleaving, IMC_INTERLEAVE_SIZE_4KB)) {
    return L"4KB";
  }
  if (IS_BIT_SET_VAR(Interleaving, IMC_INTERLEAVE_SIZE_1GB)) {
    return L"1GB";
  }
  return NULL;
}

/**
  Appends a formatted Unicode string to a Null-terminated Unicode string

  This function appends a formatted Unicode string to the Null-terminated
  Unicode string specified by pString. pString is optional and may be NULL.
  Storage for the formatted Unicode string returned is allocated using
  AllocatePool().  The pointer to the appended string is returned.  The caller
  is responsible for freeing the returned string.

  This function also calls FreePool on the old pString buffer if it is not NULL.
  So the caller does not need to free the previous buffer.

  If pString is not NULL and not aligned on a 16-bit boundary, then ASSERT().
  If pFormatString is NULL, then ASSERT().
  If pFormatString is not aligned on a 16-bit boundary, then ASSERT().

  @param[in] pString        A Null-terminated Unicode string.
  @param[in] pFormatString  A Null-terminated Unicode format string.
  @param[in] ...            The variable argument list whose contents are
                            accessed based on the format string specified by
                            pFormatString.

  @retval NULL    There was not enough available memory.
  @return         Null-terminated Unicode string is that is the formatted
                  string appended to pString.
**/
CHAR16*
EFIAPI
CatSPrintClean(
  IN  CHAR16  *pString, OPTIONAL
  IN  CONST CHAR16  *pFormatString,
  ...
  )
{
  CHAR16 *pResult = NULL;
  VA_LIST ArgList;

  VA_START(ArgList, pFormatString);
  pResult = CatVSPrint(pString, pFormatString, ArgList);
  VA_END(ArgList);

  FREE_POOL_SAFE(pString);
  return pResult;
}

/**
  Checks if all of the DIMMS are healthy.

  @param[out] pDimmsHeathy is the pointer to a BOOLEAN value,
    where the result status will be stored.
    If at least one DIMM status differs from healthy this
    will equal FALSE.

  @retval EFI_SUCCESS if there were no problems
  @retval EFI_INVALID_PARAMETER if pDimmStatus is NULL

  Other return values from functions:
    HealthProtocol->GetHealthStatus
    OpenNvmDimmProtocol
    getControllerHandle
**/
EFI_STATUS
CheckDimmsHealth(
     OUT BOOLEAN *pDimmsStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_HANDLE Controller = NULL;
  EFI_DRIVER_HEALTH_PROTOCOL *pHealthProtocol = NULL;
  EFI_DRIVER_HEALTH_STATUS DimmsHealthStatus = {0};

  NVDIMM_ENTRY();

  if (pDimmsStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pDimmsStatus = FALSE;

  ReturnCode = GetControllerHandle(&Controller);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not get the controller handle: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gEfiDriverHealthProtocolGuid, (VOID **)&pHealthProtocol, NULL);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Could not open the driver health protocol: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

  ReturnCode = pHealthProtocol->GetHealthStatus(
      pHealthProtocol,
      Controller,
      NULL,
      &DimmsHealthStatus,
      NULL,
      NULL
      );

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not get the health status: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

  if (DimmsHealthStatus == EfiDriverHealthStatusHealthy ||
      DimmsHealthStatus == EfiDriverHealthStatusRebootRequired) {
    *pDimmsStatus = TRUE;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if the user-inputed desired ARS status matches with the
  current system-wide ARS status.

  @param[in] DesiredARSStatus Desired value of the ARS status to match against
  @param[out] pARSStatusMatched Pointer to a boolean value which shows if the
              current system ARS status matches the desired one.

  @retval EFI_SUCCESS if there were no problems
  @retval EFI_INVALID_PARAMETER if one of the input parameters is a NULL, or an invalid value.
**/
EFI_STATUS
MatchCurrentARSStatus(
  IN     UINT8 DesiredARSStatus,
     OUT BOOLEAN *pARSStatusMatched
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT8 CurrentARSStatus = ARS_STATUS_UNKNOWN;

  NVDIMM_ENTRY();

  if ((pARSStatusMatched == NULL)||
      ((DesiredARSStatus != ARS_STATUS_UNKNOWN) &&
       (DesiredARSStatus != ARS_STATUS_NOT_STARTED) &&
       (DesiredARSStatus != ARS_STATUS_IN_PROGRESS) &&
       (DesiredARSStatus != ARS_STATUS_COMPLETED) &&
       (DesiredARSStatus != ARS_STATUS_ABORTED))) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pARSStatusMatched = FALSE;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **) &pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetARSStatus(
      pNvmDimmConfigProtocol,
      &CurrentARSStatus
      );
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to get the current ARS status of the system");
    goto Finish;
  }

  if (CurrentARSStatus == DesiredARSStatus) {
    *pARSStatusMatched = TRUE;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Function to write a line of unicode text to a file.

  if Handle is NULL, return error.
  if Buffer is NULL, return error.

  @param[in]     Handle         FileHandle to write to
  @param[in]     Buffer         Buffer to write

  @retval  EFI_SUCCESS          The data was written.
  @retval  other                Error codes from Write function.
**/
EFI_STATUS
EFIAPI
WriteAsciiLine(
  IN     EFI_FILE_HANDLE Handle,
  IN     VOID *pBuffer
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN Size = 0;

  if (pBuffer == NULL || StrSize(pBuffer) < sizeof(CHAR16)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    return ReturnCode;
  }

  if (Handle == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    return ReturnCode;
  }

  // Removing the '\0' char from buffer
  Size = AsciiStrSize(pBuffer) - sizeof(CHAR8);
  ReturnCode = Handle->Write(Handle, &Size, (CHAR8 *)pBuffer);

  return ReturnCode;
}

/**
  Try to find a sought pointer in an array

  @param[in] pPointersArray Array of pointers
  @param[in] PointersNum Number of pointers in array
  @param[in] pSoughtPointer Sought pointer

  @retval TRUE if pSoughtPointer has been found in the array
  @retval FALSE otherwise
**/
BOOLEAN
IsPointerInArray(
  IN     VOID *pPointersArray[],
  IN     UINT32 PointersNum,
  IN     VOID *pSoughtPointer
  )
{
  UINT32 Index = 0;

  if (pPointersArray == NULL || pSoughtPointer == NULL) {
    return FALSE;
  }

  for (Index = 0; Index < PointersNum; Index++) {
    if (pSoughtPointer == pPointersArray[Index]) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Check if given language is supported (is on supported language list)

  @param[in] pSupportedLanguages - list of supported languages
  @param[in] pLanguage - language to verify if is supported
  @param[in] Rfc4646Language - language abbreviation is compatible with Rfc4646 standard

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_UNSUPPORTED - language is not supported
  @retval EFI_SUCCESS Is supported
**/
EFI_STATUS
CheckIfLanguageIsSupported(
  IN    CONST CHAR8 *pSupportedLanguages,
  IN    CONST CHAR8 *pLanguage,
  IN    BOOLEAN Rfc4646Language
  )
{
  CHAR8 CONST *pSupportedLanguageTmp = pSupportedLanguages;
  BOOLEAN Found = FALSE;
  UINT16 Index = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (pSupportedLanguages == NULL || pLanguage == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Invalid language parameter given");
    goto Finish;
  }

  while (pSupportedLanguageTmp[0] != '\0') {
    if (Rfc4646Language) {
      /** Languages are splited by ';' **/
      for (Index = 0; pSupportedLanguageTmp[Index] != 0 && pSupportedLanguageTmp[Index] != ';'; Index++);

      if ((AsciiStrnCmp(pSupportedLanguageTmp, pLanguage, Index) == 0) && (pLanguage[Index] == '\0')) {
        Found = TRUE;
        break;
      }
      pSupportedLanguageTmp += Index;
      for (; pSupportedLanguageTmp[0] != 0 && pSupportedLanguageTmp[0] == ';'; pSupportedLanguageTmp++);
    } else {
      /** Languages are 2 digits length separated by space **/
      if (CompareMem(pLanguage, pSupportedLanguageTmp, NOT_RFC4646_ABRV_LANGUAGE_LEN) == 0) {
        Found = TRUE;
        break;
      }
      pSupportedLanguageTmp += NOT_RFC4646_ABRV_LANGUAGE_LEN;
    }
  }

  if (!Found) {
    NVDIMM_DBG("Language (%s) was not found in supported language list (%s)", pLanguage, pSupportedLanguages);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert a character to upper case

  @param[in] InChar - character to up

  @retval - upper character
**/
CHAR16 ToUpper(
  IN      CHAR16 InChar
  ) {
  CHAR16 upper = InChar;
  if (InChar >= L'a' && InChar <= L'z') {
    upper = (CHAR16) (InChar - (L'a' - L'A'));
  }
  return upper;
}

/**
  Case Insensitive StrCmp

  @param[in] pFirstString - first string for comparison
  @param[in] pSecondString - second string for comparison

  @retval Negative number if strings don't match and pFirstString < pSecondString
  @retval 0 if strings match
  @retval Positive number if strings don't match and pFirstString > pSecondString
**/
INTN StrICmp(
  IN      CONST CHAR16 *pFirstString,
  IN      CONST CHAR16 *pSecondString
  )
{
  INTN Result = -1;
  if (pFirstString != NULL && pSecondString != NULL &&
      StrLen(pFirstString) != 0 &&
      StrLen(pSecondString) != 0 &&
      StrSize(pFirstString) == StrSize(pSecondString)) {

    while (*pFirstString != L'\0' && ToUpper(*pFirstString) == ToUpper(*pSecondString)) {
      pFirstString++;
      pSecondString++;
    }
    Result = *pFirstString - *pSecondString;
  }
  return Result;
}

/**
  Calculate a power of base.

  @param[in] Base base
  @param[in] Exponent exponent

  @retval Base ^ Exponent
**/
UINT64
Pow(
  IN     UINT64 Base,
  IN     UINT32 Exponent
  )
{
  UINT64 Result = Base;
  UINT32 Index = 0;

  if (Exponent == 0) {
    return 1;
  }

  for (Index = 1; Index < Exponent; Index++) {
    Result *= Base;
  }

  return Result;
}

/**
  Read file to given buffer

  * WARNING * caller is responsible for freeing ppFileBuffer

  @param[in] pFilePath - file path
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the physical device
                          or logical device. The device path describes the location of the device the handle is for.
  @param[out] pFileSize - number of bytes written to buffer
  @param[out] ppFileBuffer - output buffer  * WARNING * caller is responsible for freeing

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NOT_STARTED Test was not executed
  @retval EFI_SUCCESS All Ok
  @retval EFI_OUT_OF_RESOURCES if memory allocation fails.
**/
EFI_STATUS
FileRead(
  IN      CHAR16 *pFilePath,
  IN      EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  IN      CONST UINT64  MaxFileSize,
     OUT  UINT64 *pFileSize,
     OUT  VOID **ppFileBuffer
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE pFileHandle = NULL;

  if (pFileSize == NULL || ppFileBuffer == NULL || pFilePath == NULL) {
    goto Finish;
  }
#ifdef OS_BUILD
  ReturnCode = OpenFile(pFilePath, &pFileHandle, NULL, 0);
  if (EFI_ERROR(ReturnCode) || pFileHandle == NULL) {
     NVDIMM_DBG("Failed opening File (" FORMAT_EFI_STATUS ")", ReturnCode);
     goto Finish;
  }
#else
  if (pDevicePath == NULL) {
     goto Finish;
  }
  ReturnCode = OpenFileByDevice(pFilePath, pDevicePath, FALSE, &pFileHandle);
  if (EFI_ERROR(ReturnCode) || pFileHandle == NULL) {
    NVDIMM_DBG("Failed opening File (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }
#endif
  ReturnCode = GetFileSize(pFileHandle, pFileSize);
  if (EFI_ERROR(ReturnCode) || *pFileSize == 0) {
    goto Finish;
  }

  if (MaxFileSize != 0 && *pFileSize > MaxFileSize) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  *ppFileBuffer = AllocateZeroPool(*pFileSize);
  if (*ppFileBuffer == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = pFileHandle->Read(pFileHandle, pFileSize, *ppFileBuffer);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishFreeBuffer;
  }

  // Evrything went fine, do not free buffer
  goto Finish;

FinishFreeBuffer:
  FREE_POOL_SAFE(*ppFileBuffer);

Finish:
  if (pFileHandle != NULL) {
    pFileHandle->Close(pFileHandle);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Read ASCII line from a file.

  The function ignores carriage return chars.

  @param FileHandle handle to a file
  @param pLine output buffer that will be filled with read line
  @param LineSize size of pLine buffer
  @param pEndOfFile output variable to report about end of file

  @retval EFI_SUCCESS
  @retval EFI_BUFFER_TOO_SMALL when pLine buffer is too small
  @retval EFI_INVALID_PARAMETER pLine or pEndOfFile is NULL
**/
EFI_STATUS
ReadAsciiLineFromFile(
  IN     EFI_FILE_HANDLE FileHandle,
     OUT CHAR8 *pLine,
  IN     INT32 LineSize,
     OUT BOOLEAN *pEndOfFile
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  CHAR8 Buffer = 0;
  UINTN BufferSizeInBytes = sizeof(Buffer);
  INT32 Index = 0;

  NVDIMM_ENTRY();
  if (pLine == NULL || pEndOfFile == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (LineSize == 0) {
    Rc = EFI_BUFFER_TOO_SMALL;
    goto Finish;
  }

  while (!EFI_ERROR(Rc = FileHandle->Read(FileHandle, &BufferSizeInBytes, &Buffer))) {
    // End of file
    if (BufferSizeInBytes == 0) {
      *pEndOfFile = TRUE;
      break;
    }
    // Ignore Carriage Return
    if (Buffer == '\r') {
      continue;
    }
    // End of line
    if (Buffer == '\n') {
      break;
    }
    // We need to have one CHAR8 reserved for the end of string '\0'
    if (Index + 1 >= LineSize) {
      Rc = EFI_BUFFER_TOO_SMALL;
      goto Finish;
    }
    pLine[Index] = Buffer;
    Index++;
  }

  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Error reading the file: " FORMAT_EFI_STATUS "", Rc);
  }

  pLine[Index] = '\0';

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Clear memory containing string

  @param[in] pString - pointer to string to be cleared
**/
VOID
CleanStringMemory(
  IN    CHAR8 *pString
  )
{
  if (pString == NULL) {
    return;
  }

  while (*pString != L'\0') {
    *pString = L'\0';
    ++pString;
  }
}

/**
  Clear memory containing unicode string

  @param[in] pString - pointer to string to be cleared
**/
VOID
CleanUnicodeStringMemory(
  IN    CHAR16 *pString
  )
{
  if (pString == NULL) {
    return;
  }

  while (*pString != L'\0') {
    *pString = L'\0';
    ++pString;
  }
}

/**
  Get linked list size

  @param[in] pListHead   List head
  @parma[out] pListSize  Counted number of items in the list

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER At least one of the input parameters equals NULL
**/
EFI_STATUS
GetListSize(
  IN     LIST_ENTRY *pListHead,
     OUT UINT32 *pListSize
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pNode = NULL;

  if (pListHead == NULL || pListSize == NULL) {
    goto Finish;
  }

  *pListSize = 0;
  LIST_FOR_EACH(pNode, pListHead) {
    (*pListSize)++;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}

/**
  Implementation of public algorithm to calculate least common multiple of two numbers

  @param[in] A  First number
  @param[in] B  Second number

  @retval Least common multiple
**/
UINT64
FindLeastCommonMultiple(
  IN     UINT64 A,
  IN     UINT64 B
  )
{
  UINT64 LeastCommonMultiple = 0;
  UINT64 WarrantedCommonMultiple = A * B;
  UINT64 Tmp = 0;

  while (B != 0) {
    Tmp = B;
    B = A % B;
    A = Tmp;
  }

  LeastCommonMultiple = WarrantedCommonMultiple / A;

  return LeastCommonMultiple;
}

/**
  Trim white spaces from the begin and end of string

  @param[in, out] pString Null terminated string that will be trimmed

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameters is NULL
  @retval EFI_BAD_BUFFER_SIZE Size of input string is bigger than MAX_INT32
**/
EFI_STATUS
TrimString(
  IN OUT CHAR16 *pString
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINTN LengthTmp = 0;
  INT32 Length = 0;
  INT32 Index = 0;
  INT32 Offset = 0;
  BOOLEAN WhiteSpace = FALSE;

  NVDIMM_ENTRY();

  if (pString == NULL) {
    goto Finish;
  }

  LengthTmp = StrLen(pString);
  if (LengthTmp > MAX_INT32) {
    ReturnCode = EFI_BAD_BUFFER_SIZE;
    goto Finish;
  }
  Length = (INT32) LengthTmp;

  if (Length > 0) {
    /** Trim white spaces at the end of string **/
    for (Index = Length - 1; Index >= 0; Index--) {
      if (IS_WHITE_UNICODE(pString[Index])) {
        pString[Index] = L'\0';
        Length--;
      } else {
        break;
      }
    }

    /** Trim white spaces at the begin of string **/
    WhiteSpace = TRUE;
    for (Index = 0; Index < Length; Index++) {
      if (WhiteSpace && !IS_WHITE_UNICODE(pString[Index])) {
        if (Index == 0) {
          /** There is no white spaces at the begin of string, so skip this stage **/
          break;
        }

        Offset = Index;
        WhiteSpace = FALSE;
      }

      if (!WhiteSpace) {
        pString[Index - Offset] = pString[Index];
      }
    }
    pString[Length - Offset] = '\0';
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Removes all white spaces from string

  @param[in] pInputBuffer Pointer to string to remove white spaces
  @param[out] pOutputBuffer Pointer to string with no white spaces
  @param[in, out] OutputBufferLength On input, length of buffer (in CHAR16),
                  on output, length of string with no white spaces, without null-terminator

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL or string length is 0
  @retval EFI_BUFFER_TOO_SMALL Output buffer is too small
**/
EFI_STATUS
RemoveWhiteSpaces(
  IN     CHAR8 *pInputBuffer,
     OUT CHAR8 *pOutputBuffer,
  IN OUT UINT64 *pOutputBufferLength
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 InputBuffLength = 0;
  UINT64 Index = 0;
  UINT64 Index2 = 0;
  NVDIMM_ENTRY();

  if (pInputBuffer == NULL || pOutputBuffer == NULL) {
    NVDIMM_DBG("Invalid pointer");
    goto Finish;
  }
  InputBuffLength = AsciiStrLen(pInputBuffer);
  if (InputBuffLength == 0) {
    NVDIMM_DBG("Line empty, nothing to remove.");
    goto Finish;
  }
  // Output buffer needs to have place for null terminator
  if (*pOutputBufferLength - 1 < InputBuffLength) {
    NVDIMM_DBG("Invalid buffer length");
    return EFI_BUFFER_TOO_SMALL;
  }
  for (Index = 0; Index < InputBuffLength; Index++) {
    if (!IS_WHITE_ASCII(pInputBuffer[Index])) {
      pOutputBuffer[Index2] = pInputBuffer[Index];
      Index2++;
    }
  }
  *pOutputBufferLength = Index2;
  ReturnCode = EFI_SUCCESS;
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert Latched Last Shutdown Status to string

  @param[in] LatchedLastShutdownStatus structure

  @retval CLI string representation of latched last shudown status
**/
CHAR16*
LastShutdownStatusToStr(
  IN     LAST_SHUTDOWN_STATUS_DETAILS_COMBINED LastShutdownStatus
  )
{
  CHAR16 *pStatusStr = NULL;

  NVDIMM_ENTRY();

  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.PmAdr) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR, LAST_SHUTDOWN_STATUS_PM_ADR_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.PmS3) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_PM_S3_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.PmS5) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_PM_S5_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.DdrtPowerFailure) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_DDRT_POWER_FAIL_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.PmicPowerLoss) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_PMIC_POWER_LOSS_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.PmWarmReset) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_PM_WARM_RESET_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.ThermalShutdown) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_THERMAL_SHUTDOWN_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatus.Separated.FwFlushComplete) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_FW_FLUSH_COMPLETE_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatusExtended.Separated.ViralInterrupt) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_VIRAL_INTERRUPT_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatusExtended.Separated.SurpriseClockStopInterrupt) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_SURPRISE_CLOCK_STOP_INTERRUPT_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatusExtended.Separated.WriteDataFlushComplete) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_WRITE_DATA_FLUSH_COMPLETE_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatusExtended.Separated.S4PowerState) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_S4_POWER_STATE_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatusExtended.Separated.PMIdle) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_PM_IDLE_STR);
  }
  if (LastShutdownStatus.Combined.LastShutdownStatusExtended.Separated.DdrtSurpriseReset) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR FORMAT_STR, pStatusStr == NULL ? L"" : L", ", LAST_SHUTDOWN_STATUS_SURPRISE_RESET_STR);
  }
  if (pStatusStr == NULL) {
    pStatusStr = CatSPrintClean(pStatusStr,
        FORMAT_STR, LAST_SHUTDOWN_STATUS_UNKNOWN_STR);
  }
  NVDIMM_EXIT();
  return pStatusStr;
}

/**
  Converts the dimm health state reason to its  HII string equivalent
  @param[in] HiiHandle - handle for hii
  @param[in] HealthStateReason The health state reason to be converted into its HII string
  @param[out] ppHealthStateStr A pointer to the HII health state string. Dynamically allocated memory and must be released by calling function.

  @retval EFI_OUT_OF_RESOURCES if there is no space available to allocate memory for string
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
ConvertHealthStateReasonToHiiStr(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT16 HealthStatusReason,
  OUT CHAR16 **ppHealthStatusReasonStr
)
{

  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 mask = BIT0;
  NVDIMM_ENTRY();

  if (ppHealthStatusReasonStr == NULL) {
    goto Finish;
  }
  *ppHealthStatusReasonStr = NULL;
  while (mask <= BIT7) {
    switch (HealthStatusReason & mask) {
    case HEALTH_REASON_PERCENTAGE_REMAINING_LOW:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_PERCENTAGE_REMAINING), NULL));
      break;
    case HEALTH_REASON_PACKAGE_SPARING_HAS_HAPPENED:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_PACKAGE_SPARING_HAPPENED), NULL));
      break;
    case HEALTH_REASON_CAP_SELF_TEST_WARNING:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_CAP_SELF_TEST_WARNING), NULL));
      break;
    case HEALTH_REASON_PERC_REMAINING_EQUALS_ZERO:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_PERCENTAGE_REMAINING_ZERO), NULL));
      break;
    case HEALTH_REASON_DIE_FAILURE:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_DIE_FAILURE), NULL));
      break;
    case HEALTH_REASON_AIT_DRAM_DISABLED:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_AIT_DRAM_DISABLED), NULL));
      break;
    case HEALTH_REASON_CAP_SELF_TEST_FAILURE:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_CAP_SELF_TEST_FAIL), NULL));
      break;
    case HEALTH_REASON_CRITICAL_INTERNAL_STATE_FAILURE:
      *ppHealthStatusReasonStr = CatSPrintClean(*ppHealthStatusReasonStr,
        ((*ppHealthStatusReasonStr == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_CRITICAL_INTERNAL_FAILURE), NULL));
    }
    mask = mask << 1;
  }

  if (*ppHealthStatusReasonStr == NULL) {
    *ppHealthStatusReasonStr = HiiGetString(HiiHandle,
      STRING_TOKEN(STR_DCPMM_VIEW_DCPMM_FORM_NONE), NULL);
  }

  if (*ppHealthStatusReasonStr == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Converts the dimm Id to its  HII string equivalent
  @param[in] HiiHandle - handle for hii
  @param[in] pRegionInfo The Region info with DimmID and Dimmcount its HII string
  @param[out] ppDimmIdStr A pointer to the HII DimmId string. Dynamically allocated memory and must be released by calling function.

  @retval EFI_OUT_OF_RESOURCES if there is no space available to allocate memory for string
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
ConvertDimmIdToDimmListStr(
  IN     REGION_INFO *pRegionInfo,
  OUT CHAR16 **ppDimmIdStr
)
{

  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  INT32 Index = 0;
  NVDIMM_ENTRY();

  if (ppDimmIdStr == NULL) {
    goto Finish;
  }
  *ppDimmIdStr = NULL;

  for (Index = 0; Index < pRegionInfo->DimmIdCount; Index++) {
    *ppDimmIdStr = CatSPrintClean(*ppDimmIdStr,
      ((*ppDimmIdStr == NULL) ? FORMAT_HEX : FORMAT_HEX_WITH_COMMA),
         pRegionInfo->DimmId[Index]);
  }

  if (*ppDimmIdStr == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert modes supported by to string

  @param[in] ModesSupported, bits define modes supported

  @retval CLI string representation of memory modes supported
**/
CHAR16*
ModesSupportedToStr(
  IN     UINT8 ModesSupported
  )
{
  CHAR16 *pModesStr = NULL;

  NVDIMM_ENTRY();

  if (ModesSupported & BIT0) {
    pModesStr = CatSPrintClean(pModesStr, FORMAT_STR, MODES_SUPPORTED_MEMORY_MODE_STR);
  }
  if (ModesSupported & BIT2) {
    pModesStr = CatSPrintClean(pModesStr, FORMAT_STR FORMAT_STR, pModesStr == NULL ? L"" : L", ", MODES_SUPPORTED_APP_DIRECT_MODE_STR);
  }
  NVDIMM_EXIT();
  return pModesStr;
}

/**
  Convert software triggers enabled to string

  @param[in] SoftwareTriggersEnabled, bits define triggers that are enabled

  @retval CLI string representation of enabled triggers
**/
CHAR16*
SoftwareTriggersEnabledToStr(
  IN     UINT64 SoftwareTriggersEnabled
  )
{
  CHAR16 *pModesStr = NULL;

  if (!SoftwareTriggersEnabled) {
     pModesStr = CatSPrintClean(pModesStr, FORMAT_STR, SW_TRIGGERS_ENABLED_NONE_STR);
  } else {
     if (SoftwareTriggersEnabled & BIT0) {
       pModesStr = CatSPrintClean(pModesStr, FORMAT_STR, SW_TRIGGERS_ENABLED_BIT0_STR);
     }
     if (SoftwareTriggersEnabled & BIT1) {
       pModesStr = CatSPrintClean(pModesStr, FORMAT_STR FORMAT_STR, pModesStr == NULL ? L"" : L", ", SW_TRIGGERS_ENABLED_BIT1_STR);
     }
     if (SoftwareTriggersEnabled & BIT2) {
       pModesStr = CatSPrintClean(pModesStr, FORMAT_STR FORMAT_STR, pModesStr == NULL ? L"" : L", ", SW_TRIGGERS_ENABLED_BIT2_STR);
     }
     if (SoftwareTriggersEnabled & BIT3) {
       pModesStr = CatSPrintClean(pModesStr, FORMAT_STR FORMAT_STR, pModesStr == NULL ? L"" : L", ", SW_TRIGGERS_ENABLED_BIT3_STR);
     }
     if (SoftwareTriggersEnabled & BIT4) {
       pModesStr = CatSPrintClean(pModesStr, FORMAT_STR FORMAT_STR, pModesStr == NULL ? L"" : L", ", SW_TRIGGERS_ENABLED_BIT4_STR);
     }
  }
  return pModesStr;
}

/**
  Convert Security Capabilities to string

  @param[in] SecurityCapabilities, bits define capabilities

  @retval CLI string representation of security capabilities
**/
CHAR16*
SecurityCapabilitiesToStr(
  IN     UINT8 SecurityCapabilities
  )
{
  CHAR16 *pCapabilitiesStr = NULL;

  NVDIMM_ENTRY();

  if (SecurityCapabilities & BIT0) {
    pCapabilitiesStr = CatSPrintClean(pCapabilitiesStr, FORMAT_STR, SECURITY_CAPABILITIES_ENCRYPTION);
  }
  if (SecurityCapabilities & BIT1) {
    if (pCapabilitiesStr != NULL) {
      pCapabilitiesStr = CatSPrintClean(pCapabilitiesStr, FORMAT_STR, L", ");
    }
    pCapabilitiesStr = CatSPrintClean(pCapabilitiesStr, FORMAT_STR, SECURITY_CAPABILITIES_ERASE);
  } else if (SecurityCapabilities & BIT0) {
    pCapabilitiesStr = CatSPrintClean(pCapabilitiesStr, FORMAT_STR, SECURITY_CAPABILITIES_NONE);
  }
  NVDIMM_EXIT();
  return pCapabilitiesStr;
}

/**
  Convert Dimm security state to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Dimm security state

  @retval String representation of Dimm's security state
**/
CHAR16*
SecurityToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 SecurityState
  )
{
  CHAR16 *pSecurityString = NULL;
  CHAR16 *pTempStr = NULL;

  switch (SecurityState) {
  case SECURITY_DISABLED:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_DISABLED), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case SECURITY_DISABLED_FROZEN:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_DISABLED_FROZEN), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case SECURITY_LOCKED:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_LOCKED), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case SECURITY_UNLOCKED:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_UNLOCKED), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case SECURITY_UNLOCKED_FROZEN:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_UNLOCKED_FROZEN), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case SECURITY_PW_MAX:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_PW_MAX), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case SECURITY_NOT_SUPPORTED:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_NOT_SUPPORTED), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  default:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_SECSTATE_UNKNOWN), NULL);
    pSecurityString = CatSPrintClean(pSecurityString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  }

  return pSecurityString;
}

/**
  Convert ARS status value to its respective string

  @param[in] ARS status value

  @retval CLI string representation of ARS status
**/
CHAR16*
ARSStatusToStr(
  IN     UINT8 ARSStatus
  )
{
  CHAR16 *pARSStatusStr = NULL;

  NVDIMM_ENTRY();

  switch (ARSStatus) {
    case ARS_STATUS_NOT_STARTED:
      pARSStatusStr = CatSPrintClean(pARSStatusStr, FORMAT_STR, ARS_STATUS_NOT_STARTED_STR);
      break;
    case ARS_STATUS_IN_PROGRESS:
      pARSStatusStr = CatSPrintClean(pARSStatusStr, FORMAT_STR, ARS_STATUS_IN_PROGRESS_STR);
      break;
    case ARS_STATUS_COMPLETED:
      pARSStatusStr = CatSPrintClean(pARSStatusStr, FORMAT_STR, ARS_STATUS_COMPLETED_STR);
      break;
    case ARS_STATUS_ABORTED:
      pARSStatusStr = CatSPrintClean(pARSStatusStr, FORMAT_STR, ARS_STATUS_ABORTED_STR);
      break;
    case ARS_STATUS_UNKNOWN:
    default:
      pARSStatusStr = CatSPrintClean(pARSStatusStr, FORMAT_STR, ARS_STATUS_UNKNOWN_STR);
      break;
  }

  NVDIMM_EXIT();
  return pARSStatusStr;
}

/**
  Convert dimm's boot status bitmask to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] BootStatusBitmask, bits define the boot status

  @retval CLI/HII string representation of dimm's boot status
**/
CHAR16*
BootStatusBitmaskToStr(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT16 BootStatusBitmask
  )
{

  CHAR16 *pBootStatusStr = NULL;
  CHAR16 *pTempStr = NULL;

  NVDIMM_ENTRY();

  if (BootStatusBitmask == 0) {
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_SUCCESS), NULL);
    pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
  } else if (BootStatusBitmask & DIMM_BOOT_STATUS_UNKNOWN) {
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_UNKNOWN), NULL);
    pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
  } else {
    if (BootStatusBitmask & DIMM_BOOT_STATUS_MEDIA_NOT_READY) {
      pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_MEDIA_NOT_READY), NULL);
      pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
    }
    if (BootStatusBitmask & DIMM_BOOT_STATUS_MEDIA_ERROR) {
      pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_MEDIA_ERROR), NULL);
      pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR FORMAT_STR,
        pBootStatusStr == NULL ? L"" : L", ", pTempStr);
      FREE_POOL_SAFE(pTempStr);
    }
    if (BootStatusBitmask & DIMM_BOOT_STATUS_MEDIA_DISABLED) {
      pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_MEDIA_DISABLED), NULL);
      pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR FORMAT_STR,
        pBootStatusStr == NULL ? L"" : L", ", pTempStr);
      FREE_POOL_SAFE(pTempStr);
    }
    if (BootStatusBitmask & DIMM_BOOT_STATUS_DDRT_NOT_READY) {
      pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_DDRT_NOT_READY), NULL);
      pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR FORMAT_STR,
        pBootStatusStr == NULL ? L"" : L", ", pTempStr);
      FREE_POOL_SAFE(pTempStr);
    }
    if (BootStatusBitmask & DIMM_BOOT_STATUS_MAILBOX_NOT_READY) {
      pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_MAILBOX_NOT_READY), NULL);
      pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR FORMAT_STR,
        pBootStatusStr == NULL ? L"" : L", ", pTempStr);
      FREE_POOL_SAFE(pTempStr);
    }
    if (BootStatusBitmask & DIMM_BOOT_STATUS_REBOOT_REQUIRED) {
      pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_BOOT_STATUS_RR), NULL);
      pBootStatusStr = CatSPrintClean(pBootStatusStr, FORMAT_STR FORMAT_STR,
        pBootStatusStr == NULL ? L"" : L", ", pTempStr);
      FREE_POOL_SAFE(pTempStr);
    }
  }

  NVDIMM_EXIT();
  return pBootStatusStr;
}

/**
  Convert string value to double

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] pString String value to convert
  @param[out] pOutValue Target double value

  @retval EFI_INVALID_PARAMETER No valid value inside
  @retval EFI_SUCCESS Conversion successful
**/
EFI_STATUS
StringToDouble(
  IN     EFI_HANDLE HiiHandle,
  IN     CHAR16 *pString,
     OUT double *pOutValue
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pLastChar = NULL;
  CHAR16 **ppStringElements = NULL;
  UINT64 DecimalElements[2] = {0};
  UINT32 ElementsCount = 0;
  UINT32 Index = 0;
  BOOLEAN Valid = 0;
  double Decimal = 0.0;
  double Fractional = 0.0;
  CHAR16 *pDecimalMarkStr = NULL;

  NVDIMM_ENTRY();

  if (pString == NULL || pOutValue == NULL) {
    goto Finish;
  }

  pDecimalMarkStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_DECIMAL_MARK), NULL);

  if (pDecimalMarkStr == NULL) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Ignore leading white chars
  while ((*pString == L' ') || (*pString == L'\t')) {
    pString++;
  }

  // Delete trailing zeros
  if (StrStr(pString, pDecimalMarkStr) != NULL) {
    pLastChar = pString + StrLen(pString) * sizeof(pString[0]) - 1;
    while (*pLastChar == L'0') {
      *pLastChar = L'\0';
      pLastChar--;
    }
  }

  ppStringElements = StrSplit(pString, pDecimalMarkStr[0], &ElementsCount);
  if (ppStringElements == NULL || ElementsCount == 0 || ElementsCount > 2) {
    goto Finish;
  }

  for (Index = 0; Index < ElementsCount; Index++) {
    Valid = GetU64FromString(ppStringElements[Index], &DecimalElements[Index]);
    if (!Valid) {
      goto Finish;
    }
  }
  Decimal = (double) DecimalElements[0];
  Fractional = (double) DecimalElements[1];

  if (ElementsCount == 2) {
    for (Index = 0; Index < StrLen(ppStringElements[1]); Index++) {
      Fractional = Fractional * 0.1;
    }
  }

  *pOutValue = Decimal + Fractional;
  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDecimalMarkStr);
  FreeStringArray(ppStringElements, ElementsCount);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Compare a PackageSparing capability, encryption, soft SKU capabilities and SKU mode types.

  @param[in] SkuInformation1 - first SkuInformation to compare
  @param[in] SkuInformation2 - second SkuInformation to compare

  @retval NVM_SUCCESS - if everything went fine
  @retval NVM_ERR_DIMM_SKU_MODE_MISMATCH - if mode conflict occurred
  @retval NVM_ERR_DIMM_SKU_SECURITY_MISMATCH - if security mode conflict occurred
**/
NvmStatusCode
SkuComparison(

  IN     UINT32 SkuInformation1,
  IN     UINT32 SkuInformation2
  )
{
  NvmStatusCode StatusCode = NVM_SUCCESS;
  NVDIMM_ENTRY();

  if ((SkuInformation1 & SKU_MODES_MASK) !=
      (SkuInformation2 & SKU_MODES_MASK)) {
    StatusCode = NVM_ERR_DIMM_SKU_MODE_MISMATCH;
    goto Finish;
  }

  if ((SkuInformation1 & SKU_ENCRYPTION_MASK) !=
      (SkuInformation2 & SKU_ENCRYPTION_MASK)) {
    StatusCode = NVM_ERR_DIMM_SKU_SECURITY_MISMATCH;
    goto Finish;
  }

  /** Everything went fine **/
Finish:
  NVDIMM_EXIT();
  return StatusCode;
}

/**
  Check if SKU conflict occurred.
  Any mixed modes between DIMMs are prohibited on a platform.

  @param[in] pDimmInfo1 - first DIMM_INFO to compare SKU mode
  @param[in] pDimmInfo2 - second DIMM_INFO to compare SKU mode
  @param[out] pSkuModeMismatch - pointer to a BOOLEAN value that will
    represent result of comparison

  @retval - Appropriate CLI return code
**/
EFI_STATUS
IsSkuModeMismatch(
  IN     DIMM_INFO *pDimmInfo1 OPTIONAL,
  IN     DIMM_INFO *pDimmInfo2 OPTIONAL,
     OUT BOOLEAN *pSkuModeMismatch
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NvmStatusCode StatusCode = NVM_ERR_INVALID_PARAMETER;
  NVDIMM_ENTRY();

  if (pDimmInfo1 == NULL || pDimmInfo2 == NULL || pSkuModeMismatch == NULL) {
    goto Finish;
  }
  *pSkuModeMismatch = FALSE;

  StatusCode = SkuComparison(pDimmInfo1->SkuInformation,
                             pDimmInfo2->SkuInformation);

  if (StatusCode != NVM_SUCCESS) {
    *pSkuModeMismatch = TRUE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I(ReturnCode);
  return ReturnCode;
}

/**
  Convert type to string

  @param[in] MemoryType, integer define type

  @retval CLI string representation of memory type
**/
CHAR16*
MemoryTypeToStr(
  IN     UINT8 MemoryType
  )
{
  CHAR16 *pTempStr = NULL;

  switch (MemoryType) {
    case MEMORYTYPE_DDR4:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, MEMORY_TYPE_DDR4_STR);
      break;
    case MEMORYTYPE_DCPM:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, MEMORY_TYPE_DCPM_STR);
      break;
    default:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, MEMORY_TYPE_UNKNOWN_STR);
      break;
  }
  return pTempStr;
}

/**
  Sort Linked List by using Bubble Sort.

  @param[in, out] LIST HEAD to sort
  @param[in] Compare Pointer to function that is needed for items comparing. It should return:
                     -1 if "first < second"
                     0  if "first == second"
                     1  if "first > second"

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
BubbleSortLinkedList(
  IN OUT LIST_ENTRY *pList,
  IN     INT32 (*Compare) (VOID *first, VOID *second)
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN Swapped = FALSE;
  LIST_ENTRY *pNodeCurrentEntry = NULL;
  LIST_ENTRY *pNodeNextEntry = NULL;

  NVDIMM_ENTRY();

  if (IsListEmpty(pList) || Compare == NULL) {
    goto Finish;
  }

  do {
    Swapped = FALSE;

    pNodeCurrentEntry = pList->ForwardLink;
    pNodeNextEntry = pNodeCurrentEntry->ForwardLink;

    while (pNodeNextEntry != pList) {

      if (Compare(pNodeCurrentEntry, pNodeNextEntry) > 0) {

		  LIST_ENTRY *
			  EFIAPI
			  SwapListEntries(
				  IN OUT  LIST_ENTRY                *FirstEntry,
				  IN OUT  LIST_ENTRY                *SecondEntry
			  ); SwapListEntries(pNodeCurrentEntry, pNodeNextEntry);
        pNodeCurrentEntry = pNodeNextEntry;
        pNodeNextEntry = pNodeNextEntry->ForwardLink;
        Swapped = TRUE;
      }

      pNodeCurrentEntry = pNodeNextEntry;
      pNodeNextEntry = pNodeNextEntry->ForwardLink;
    }
  } while (Swapped);

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Sort an array by using Bubble Sort.

  @param[in, out] pArray Array to sort
  @param[in] Count Number of items in array
  @param[in] ItemSize Size of item in bytes
  @param[in] Compare Pointer to function that is needed for items comparing. It should return:
                     -1 if "first < second"
                     0  if "first == second"
                     1  if "first > second"

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
BubbleSort(
  IN OUT VOID *pArray,
  IN     UINT32 Count,
  IN     UINT32 ItemSize,
  IN     INT32 (*Compare) (VOID *first, VOID *second)
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN Swapped = FALSE;
  UINT32 Index = 0;
  VOID *pTmpItem = NULL;
  UINT8 *pFirst = NULL;
  UINT8 *pSecond = NULL;

  NVDIMM_ENTRY();

  if (pArray == NULL || Compare == NULL) {
    goto Finish;
  }

  pTmpItem = AllocateZeroPool(ItemSize);
  if (pTmpItem == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  do {
    Swapped = FALSE;
    for (Index = 1; Index < Count; Index++) {
      pFirst = (UINT8 *) pArray + (Index - 1) * ItemSize;
      pSecond = (UINT8 *) pArray + Index * ItemSize;

      if (Compare(pFirst, pSecond) > 0) {
        CopyMem_S(pTmpItem, ItemSize, pFirst, ItemSize);
        CopyMem_S(pFirst, ItemSize, pSecond, ItemSize);
        CopyMem_S(pSecond, ItemSize, pTmpItem, ItemSize);
        Swapped = TRUE;
      }
    }
  } while (Swapped);

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pTmpItem);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert from units type to a string

  @param[in] UnitsToDisplay The type of units to be used

  @retval String representation of the units type
**/
CHAR16*
UnitsToStr(
  IN     UINT16 UnitsToDisplay
  )
{
  CHAR16 *pTempStr = NULL;

  switch (UnitsToDisplay) {
    case DISPLAY_SIZE_UNIT_B:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_B_STR);
      break;
    case DISPLAY_SIZE_UNIT_MB:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_MB_STR);
      break;
    case DISPLAY_SIZE_UNIT_MIB:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_MIB_STR);
      break;
    case DISPLAY_SIZE_UNIT_GB:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_GB_STR);
      break;
    case DISPLAY_SIZE_UNIT_GIB:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_GIB_STR);
      break;
    case DISPLAY_SIZE_UNIT_TB:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_TB_STR);
      break;
    case DISPLAY_SIZE_UNIT_TIB:
      pTempStr =  CatSPrint(NULL, FORMAT_STR, UNITS_TIB_STR);
      break;
    default:
      NVDIMM_DBG("Invalid units type!");
      break;
  }
  return pTempStr;
}

/**
  Convert last firmware update status to string.
  The caller function is obligated to free memory of the returned string.

  @param[in] Last Firmware update status value to convert

  @retval output string or NULL if memory allocation failed
**/
CHAR16 *
LastFwUpdateStatusToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 LastFwUpdateStatus
  )
{
  CHAR16 *pLastFwUpdateStatusString = NULL;
  CHAR16 *pTempStr = NULL;

  switch (LastFwUpdateStatus) {
  case FW_UPDATE_STATUS_STAGED_SUCCESS:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_FW_UPDATE_STATUS_STAGED), NULL);
    pLastFwUpdateStatusString = CatSPrintClean(pLastFwUpdateStatusString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case FW_UPDATE_STATUS_LOAD_SUCCESS:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_FW_UPDATE_STATUS_SUCCESS), NULL);
    pLastFwUpdateStatusString = CatSPrintClean(pLastFwUpdateStatusString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  case FW_UPDATE_STATUS_FAILED:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_FW_UPDATE_STATUS_FAIL), NULL);
    pLastFwUpdateStatusString = CatSPrintClean(pLastFwUpdateStatusString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  default:
    pTempStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_FW_UPDATE_STATUS_UNKNOWN), NULL);
    pLastFwUpdateStatusString = CatSPrintClean(pLastFwUpdateStatusString, FORMAT_STR, pTempStr);
    FREE_POOL_SAFE(pTempStr);
    break;
  }

  return pLastFwUpdateStatusString;
}

/**
  Determines if an array, whose size is known in bytes has all elements as zero
  @param[in] pArray    Pointer to the input array
  @param[in] ArraySize Array size in bytes
  @param[out] pAllElementsZero Pointer to a boolean that stores the
    result whether all array elements are zero

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
AllElementsInArrayZero(
  IN OUT VOID *pArray,
  IN     UINT32 ArraySize,
     OUT BOOLEAN *pAllElementsZero
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR8 *pTempArray = (CHAR8 *)pArray;
  UINT32 Index = 0;
  BOOLEAN TempAllElementsZero = TRUE;

  NVDIMM_ENTRY();

  if (pArray == NULL|| pAllElementsZero == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < ArraySize; Index++) {
    if (pTempArray[Index] != 0) {
      TempAllElementsZero = FALSE;
      break;
    }
  }

  *pAllElementsZero = TempAllElementsZero;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Endian swap a uint32 value
  @param[in] OrigVal Value to modify

  @retval Value with the endian swap
**/
UINT32
EndianSwapUint32(
  IN UINT32 OrigVal
  )
{
  UINT32 NewVal;
  NewVal = ((OrigVal & 0x000000FF) << 24) |
		  ((OrigVal & 0x0000FF00) << 8) |
		  ((OrigVal & 0x00FF0000) >> 8) |
		  ((OrigVal & 0xFF000000) >> 24);
  return NewVal;
}

/**
  Endian swap a uint16 value
  @param[in] OrigVal Value to modify

  @retval Value with the endian swap
**/
UINT16
EndianSwapUint16(
  IN UINT16 OrigVal
  )
{
  UINT16 NewVal;
  NewVal = ((OrigVal & 0x00FF) << 8) |
		  ((OrigVal & 0xFF00) >> 8);
  return NewVal;
}

/**
  Converts EPOCH time in number of seconds into a human readable time string
  @param[in] TimeInSesconds Number of seconds (EPOCH time)

  @retval Human readable time string
**/
CHAR16 *GetTimeFormatString (UINT64 TimeInSeconds)
{
  int TimeSeconds = 0,
      TimeMinutes = 0,
      TimeHours = 0,
      TimeMonth = 0,
      TimeMonthday = 0,
      TimeYear = 0,
      TimeWeekday = 0;

  UINT64 PartialDayInSeconds = 0;
  int NumberOfFullDays = 0;
  int CENTURY_MARKER = 1900; // EPOCH year century
  int EPOCH_YEAR_START = 1970; // EPOCH start = Thu Jan 1 1970 00:00:00
  int WEEKDAY_OFFSET_FROM_EPOCH_START = 4;
  int SECONDS_PER_MINUTE = 60;
  int SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
  UINT64 SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;
  int DaysPerMonth[2][12] = {
      { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }, // days per month in regular years
      { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }  // days per month in leap year
  };
  int Year = EPOCH_YEAR_START;
  CHAR16 *pTimeFormatString = NULL;

  const CHAR16 *DayOfWeek[] = {
      L"Sun",
      L"Mon",
      L"Tue",
      L"Wed",
      L"Thu",
      L"Fri",
      L"Sat"
  };
  const CHAR16 *Month[] = {
      L"Jan",
      L"Feb",
      L"Mar",
      L"Apr",
      L"May",
      L"Jun",
      L"Jul",
      L"Aug",
      L"Sep",
      L"Oct",
      L"Nov",
      L"Dec"
  };

  PartialDayInSeconds = (UINT64) TimeInSeconds % SECONDS_PER_DAY;
  NumberOfFullDays = (int)(TimeInSeconds / SECONDS_PER_DAY);

  TimeSeconds = PartialDayInSeconds % SECONDS_PER_MINUTE;
  TimeMinutes = (PartialDayInSeconds % SECONDS_PER_HOUR) / 60;
  TimeHours = (int)(PartialDayInSeconds / SECONDS_PER_HOUR);
  TimeWeekday = (NumberOfFullDays + WEEKDAY_OFFSET_FROM_EPOCH_START) % 7;

  while (NumberOfFullDays >= DAYS_IN_YEAR(Year)) {
    NumberOfFullDays -= DAYS_IN_YEAR(Year);
    Year++;
  }

  TimeYear = Year - CENTURY_MARKER;
  TimeMonth = 0;
  while (NumberOfFullDays >= DaysPerMonth[IS_LEAP_YEAR(Year)][TimeMonth]) {
    NumberOfFullDays -= DaysPerMonth[IS_LEAP_YEAR(Year)][TimeMonth];
    TimeMonth++;
  }

  TimeMonthday = NumberOfFullDays + 1;

  pTimeFormatString = CatSPrintClean(pTimeFormatString,
    FORMAT_STR_SPACE FORMAT_STR L" %02d %02d:%02d:%02d UTC %d",
    DayOfWeek[TimeWeekday],
    Month[TimeMonth],
    TimeMonthday,
    TimeHours,
    TimeMinutes,
    TimeSeconds,
    TimeYear + CENTURY_MARKER
    );

return pTimeFormatString;
}

/**
  Convert goal status bitmask to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Status bits that define the goal status

  @retval CLI/HII string representation of goal status
**/
CHAR16*
GoalStatusToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 Status
  )
{
  CHAR16 *pGoalStatusString = NULL;
  CHAR16 *pTempStr = NULL;

  if (HiiHandle == NULL) {
    return NULL;
  }

  switch (Status) {
    case GOAL_CONFIG_STATUS_UNKNOWN:
      pTempStr = HiiGetString(HiiHandle,
        STRING_TOKEN(STR_DCPMM_REGIONS_FORM_GOAL_STATUS_UNKNOWN), NULL);
      pGoalStatusString = CatSPrintClean(pGoalStatusString, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
      break;

    case GOAL_CONFIG_STATUS_NEW:
      pTempStr = HiiGetString(HiiHandle,
        STRING_TOKEN(STR_DCPMM_REGIONS_FORM_GOAL_STATUS_REBOOT_REQUIRED), NULL);
      pGoalStatusString = CatSPrintClean(pGoalStatusString, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
      break;

    case GOAL_CONFIG_STATUS_BAD_REQUEST:
      pTempStr = HiiGetString(HiiHandle,
        STRING_TOKEN(STR_DCPMM_REGIONS_FORM_GOAL_STATUS_INVALID_GOAL), NULL);
      pGoalStatusString = CatSPrintClean(pGoalStatusString, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
      break;

    case GOAL_CONFIG_STATUS_NOT_ENOUGH_RESOURCES:
      pTempStr = HiiGetString(HiiHandle,
        STRING_TOKEN(STR_DCPMM_REGIONS_FORM_GOAL_STATUS_NOT_ENOUGH_RESOURCES), NULL);
      pGoalStatusString = CatSPrintClean(pGoalStatusString, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
      break;

    case GOAL_CONFIG_STATUS_FIRMWARE_ERROR:
      pTempStr = HiiGetString(HiiHandle,
        STRING_TOKEN(STR_DCPMM_REGIONS_FORM_GOAL_STATUS_FIRMWARE_ERROR), NULL);
      pGoalStatusString = CatSPrintClean(pGoalStatusString, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
      break;

    default:
      pTempStr = HiiGetString(HiiHandle,
        STRING_TOKEN(STR_DCPMM_REGIONS_FORM_GOAL_STATUS_UNKNOWN_ERROR), NULL);
      pGoalStatusString = CatSPrintClean(pGoalStatusString, FORMAT_STR, pTempStr);
      FREE_POOL_SAFE(pTempStr);
      break;
   }

   return pGoalStatusString;
}

/**
  Poll long operation status

  Polls the status of the background operation on the dimm.

  @param [in] pNvmDimmConfigProtocol Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param [in] DimmId Dimm ID of the dimm to poll status
  @param [in] OpcodeToPoll Specify an opcode to poll, 0 to poll regardless of opcode
  @param [in] SubOpcodeToPoll Specify an opcode to poll
  @param [in] Timeout for the background operation
**/
EFI_STATUS
PollLongOpStatus(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 DimmId,
  IN     UINT8 OpcodeToPoll OPTIONAL,
  IN     UINT8 SubOpcodeToPoll OPTIONAL,
  IN     UINT64 Timeout
  )
{
  UINT8 EventCount = 0;
  EFI_STATUS ReturnCode = EFI_DEVICE_ERROR;
  UINT64 WaitIndex = 0;
  EFI_EVENT WaitList[2];
  EFI_STATUS LongOpEfiStatus = EFI_SUCCESS;
  UINT8 CmdOpcode = 0;
  UINT8 CmdSubOpcode = 0;

  ZeroMem(WaitList, sizeof(WaitList));

  gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &WaitList[LONG_OP_POLL_EVENT_TIMER]);
  gBS->SetTimer(WaitList[LONG_OP_POLL_EVENT_TIMER], TimerPeriodic, LONG_OP_POLL_TIMER_INTERVAL);
  EventCount++;

  if (Timeout > 0) {
    gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &WaitList[LONG_OP_POLL_EVENT_TIMEOUT]);
    gBS->SetTimer(WaitList[LONG_OP_POLL_EVENT_TIMEOUT], TimerRelative, Timeout);
    EventCount++;
  }

  do {
    ReturnCode = gBS->WaitForEvent(EventCount, WaitList, &WaitIndex);
    if (EFI_ERROR(ReturnCode)) {
      break;
    }

    ReturnCode = pNvmDimmConfigProtocol->GetLongOpStatus(pNvmDimmConfigProtocol, DimmId,
      &CmdOpcode, &CmdSubOpcode, NULL, NULL, &LongOpEfiStatus);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not get long operation status");
      goto Finish;
    }

    // If user passed in an opcode, validate that it matches the long operation opcode
    // If it doesn't match, assume it is < FIS 1.6 and not supported
    if (OpcodeToPoll != 0) {
      if (OpcodeToPoll != CmdOpcode || SubOpcodeToPoll != CmdSubOpcode) {
        ReturnCode = EFI_INCOMPATIBLE_VERSION;
        goto Finish;
      }
    }
    // Report back failure with the long op command
    if (EFI_ERROR(LongOpEfiStatus)) {
      if (LongOpEfiStatus != EFI_NO_RESPONSE) {
        ReturnCode = LongOpEfiStatus;
        goto Finish;
      }
    }

    if (WaitIndex == LONG_OP_POLL_EVENT_TIMEOUT) {
      NVDIMM_DBG("Timed out polling long operation status");
      ReturnCode = EFI_TIMEOUT;
      goto Finish;
    }
  } while (LongOpEfiStatus != EFI_SUCCESS);

  ReturnCode = EFI_SUCCESS;

Finish:
  gBS->CloseEvent(WaitList[LONG_OP_POLL_EVENT_TIMER]);
  if (Timeout > 0) {
    gBS->CloseEvent(WaitList[LONG_OP_POLL_EVENT_TIMEOUT]);
  }

  return ReturnCode;
}

EFI_STATUS
GetNSLabelMajorMinorVersion(
  IN     UINT32 NamespaceLabelVersion,
     OUT UINT16 *pMajor,
     OUT UINT16 *pMinor
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pMajor == NULL || pMinor == NULL) {
    goto Finish;
  }

  if ((NamespaceLabelVersion == NS_LABEL_VERSION_LATEST) ||
      (NamespaceLabelVersion == NS_LABEL_VERSION_1_2)) {
    *pMajor = NSINDEX_MAJOR;
    *pMinor = NSINDEX_MINOR_2;
    ReturnCode = EFI_SUCCESS;
  } else if (NamespaceLabelVersion == NS_LABEL_VERSION_1_1) {
    *pMajor = NSINDEX_MAJOR;
    *pMinor = NSINDEX_MINOR_1;
    ReturnCode = EFI_SUCCESS;
  } else {
    NVDIMM_DBG("Invalid NamespaceLabelVersion: %d", NamespaceLabelVersion);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Get basic information about the host server

@param[out] pHostServerInfo pointer to a HOST_SERVER_INFO struct

@retval EFI_SUCCESS Success
@retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
GetHostServerInfo(
   OUT HOST_SERVER_INFO *pHostServerInfo
)
{
   EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
   NVDIMM_ENTRY();
#ifdef OS_BUILD
   char name[HOST_SERVER_NAME_LEN];
   char osName[HOST_SERVER_OS_NAME_LEN];
   char osVersion[HOST_SERVER_OS_VERSION_LEN];
   if (NULL == pHostServerInfo)
   {
      goto Finish;
   }
   if (0 != os_get_host_name(name, HOST_SERVER_NAME_LEN))
   {
      goto Finish;
   }
   else
   {
      AsciiStrToUnicodeStrS(name, pHostServerInfo->Name, HOST_SERVER_NAME_LEN);
   }

   if (0 != os_get_os_name(osName, HOST_SERVER_OS_NAME_LEN))
   {
      goto Finish;
   }
   else
   {
      AsciiStrToUnicodeStrS(osName, pHostServerInfo->OsName, HOST_SERVER_OS_NAME_LEN);
   }

   if (0 != os_get_os_version(osVersion, HOST_SERVER_OS_VERSION_LEN))
   {
      goto Finish;
   }
   else
   {
      AsciiStrToUnicodeStrS(osVersion, pHostServerInfo->OsVersion, HOST_SERVER_OS_VERSION_LEN);
   }
   ReturnCode = EFI_SUCCESS;
#else
   //2nd arg is size of the destination buffer in bytes so sizeof is appropriate
   UnicodeSPrint(pHostServerInfo->OsName, sizeof(pHostServerInfo->OsName), L"UEFI");
   UnicodeSPrint(pHostServerInfo->Name, sizeof(pHostServerInfo->Name), L"N/A");
   UnicodeSPrint(pHostServerInfo->OsVersion, sizeof(pHostServerInfo->OsVersion), L"N/A");
   ReturnCode = EFI_SUCCESS;
   goto Finish;
#endif

Finish:
   NVDIMM_EXIT_I64(ReturnCode);
   return ReturnCode;
}

/**
Copies a source buffer to a destination buffer, and returns the destination buffer.


@param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
@param  DestLength          The length in bytes of DestinationBuffer.
@param  SourceBuffer        The pointer to the source buffer of the memory copy.
@param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

@return DestinationBuffer.

**/
VOID *
CopyMem_S(
  OUT VOID       *DestinationBuffer,
  IN UINTN       DestLength,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
)
{
#ifdef OS_BUILD
  int status = memcpy_s(DestinationBuffer, DestLength, SourceBuffer, Length);
  if(status != 0)
    NVDIMM_CRIT("Memcpy_s failed with ErrorCode: %x", status);
  return DestinationBuffer;
#else
  return CopyMem(DestinationBuffer, SourceBuffer, Length);
#endif
}

/**
  Retrives Intel Dimm Config EFI vars

  User is responsible for freeing ppIntelDIMMConfig

  @param[out] pIntelDIMMConfig Pointer to struct to fill with EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
RetrieveIntelDIMMConfig(
     OUT INTEL_DIMM_CONFIG **ppIntelDIMMConfig
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINTN VariableSize = 0;

  NVDIMM_ENTRY();

  *ppIntelDIMMConfig = AllocateZeroPool(sizeof(INTEL_DIMM_CONFIG));
  if (*ppIntelDIMMConfig == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  VariableSize = sizeof(INTEL_DIMM_CONFIG);
  ReturnCode = GET_VARIABLE(
    INTEL_DIMM_CONFIG_VARIABLE_NAME,
    gIntelDimmConfigVariableGuid,
    &VariableSize,
    *ppIntelDIMMConfig);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not find IntelDIMMConfigs");
    FREE_POOL_SAFE(*ppIntelDIMMConfig);
    goto Finish;
  }

  NVDIMM_DBG("Revision: %d", (*ppIntelDIMMConfig)->Revision);
  NVDIMM_DBG("ProvisionCapacityMode: %d", (*ppIntelDIMMConfig)->ProvisionCapacityMode);
  NVDIMM_DBG("MemorySize: %d", (*ppIntelDIMMConfig)->MemorySize);
  NVDIMM_DBG("PMType: %d", (*ppIntelDIMMConfig)->PMType);
  NVDIMM_DBG("ProvisionNamespaceMode: %d", (*ppIntelDIMMConfig)->ProvisionNamespaceMode);
  NVDIMM_DBG("NamespaceFlags: %d", (*ppIntelDIMMConfig)->NamespaceFlags);
  NVDIMM_DBG("ProvisionCapacityStatus: %d", (*ppIntelDIMMConfig)->ProvisionCapacityStatus);
  NVDIMM_DBG("ProvisionNamespaceStatus: %d", (*ppIntelDIMMConfig)->ProvisionNamespaceStatus);
  NVDIMM_DBG("NamespaceLabelVersion: %d", (*ppIntelDIMMConfig)->NamespaceLabelVersion);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
Get manageability state for Dimm

@param[in] SubsystemVendorId the SubsystemVendorId
@param[in] interfaceCodeNum the number of interface codes
@param[in] interfaceCodes the interface codes
@param[in] SubsystemDeviceId the subsystem device ID
@param[in] fwMajor the fw major version
@param[in] fwMinor the fw minor version


@retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmManageableByValues(
  IN  UINT16 SubsystemVendorId,
  IN  UINT32 interfaceCodeNum,
  IN  UINT16* interfaceCodes,
  IN  UINT16 SubsystemDeviceId,
  IN  UINT8 fwMajor,
  IN  UINT8 fwMinor
)
{
  BOOLEAN Manageable = FALSE;

  if (IsDimmInterfaceCodeSupportedByValues(interfaceCodeNum, interfaceCodes) &&
    (SPD_INTEL_VENDOR_ID == SubsystemVendorId) &&
    IsSubsystemDeviceIdSupportedByValues(SubsystemDeviceId) &&
    IsFwApiVersionSupportedByValues(fwMajor, fwMinor))
  {
    Manageable = TRUE;
  }

  return Manageable;
}

/**
Check if the dimm interface code of this DIMM is supported

@param[in] interfaceCodeNum the number of interface codes
@param[in] interfaceCodes the interface codes

@retval true if supported, false otherwise
**/
BOOLEAN
IsDimmInterfaceCodeSupportedByValues(
  IN  UINT32 interfaceCodeNum,
  IN  UINT16* interfaceCodes
)
{
  BOOLEAN Supported = FALSE;
  UINT32 Index = 0;

  if (interfaceCodes != NULL)
  {
    for (Index = 0; Index < interfaceCodeNum; Index++) {
      if (DCPMM_FMT_CODE_APP_DIRECT == interfaceCodes[Index] ||
        DCPMM_FMT_CODE_STORAGE == interfaceCodes[Index]) {
        Supported = TRUE;
        break;
      }
    }
  }

  return Supported;
}

/**
Check if the subsystem device ID of this DIMM is supported

@param[in] SubsystemDeviceId the subsystem device ID

@retval true if supported, false otherwise
**/
BOOLEAN
IsSubsystemDeviceIdSupportedByValues(
  IN UINT16 SubsystemDeviceId
)
{
  BOOLEAN Supported = FALSE;

  if ((SubsystemDeviceId >= SPD_DEVICE_ID_05) &&
    (SubsystemDeviceId <= SPD_DEVICE_ID_15)) {
    Supported = TRUE;
  }

  return Supported;
}

/**
Check if current firmware API version is supported

@param[in] major the major version
@param[in] minor the minor version

@retval true if supported, false otherwise
**/
BOOLEAN
IsFwApiVersionSupportedByValues(
  IN   UINT8 major,
  IN   UINT8 minor
)
{
  BOOLEAN VerSupported = TRUE;

  if ((major < DEV_FW_API_VERSION_MAJOR_MIN) ||
    (major == DEV_FW_API_VERSION_MAJOR_MIN &&
      minor < DEV_FW_API_VERSION_MINOR_MIN)) {
    VerSupported = FALSE;
  }
  return VerSupported;
}


/**
  Convert controller revision id to string

  @param[in] Controller revision id

  @retval CLI string representation of the controller revision id
**/
CHAR16*
ControllerRidToStr(
  IN     UINT16 ControllerRid
  )
{
  CHAR16 *pSteppingStr = NULL;
  UINT8 BaseStep = 0;
  UINT8 MetalStep = 0;

  NVDIMM_ENTRY();

  BaseStep = ControllerRid & CONTROLLER_REVISION_BASE_STEP_MASK;
  MetalStep = ControllerRid & CONTROLLER_REVISION_METAL_STEP_MASK;

  switch (BaseStep) {
    case CONTROLLER_REVISON_A_STEP:
      pSteppingStr = CatSPrintClean(NULL, FORMAT_STEPPING, CONTROLLER_REVISON_A_STEP_STR, MetalStep,
        ControllerRid);
      break;
    case CONTROLLER_REVISON_S_STEP:
      pSteppingStr = CatSPrintClean(NULL, FORMAT_STEPPING, CONTROLLER_REVISON_S_STEP_STR, MetalStep,
        ControllerRid);
      break;
    case CONTROLLER_REVISON_B_STEP:
      pSteppingStr = CatSPrintClean(NULL, FORMAT_STEPPING, CONTROLLER_REVISON_B_STEP_STR, MetalStep,
        ControllerRid);
      break;
    case CONTROLLER_REVISON_C_STEP:
      pSteppingStr = CatSPrintClean(NULL, FORMAT_STEPPING, CONTROLLER_REVISON_C_STEP_STR, MetalStep,
        ControllerRid);
      break;
    default:
      pSteppingStr = CatSPrintClean(NULL, FORMAT_STR, CONTROLLER_STEPPING_UNKNOWN_STR);
      break;
  }

  NVDIMM_EXIT();
  return pSteppingStr;
}

/**
Set object status for DIMM_INFO

@param[out] pCommandStatus Pointer to command status structure
@param[in] pDimm DIMM_INFO for which the object status is being set
@param[in] Status Object status to set
**/
VOID
SetObjStatusForDimmInfo(
  OUT COMMAND_STATUS *pCommandStatus,
  IN     DIMM_INFO *pDimm,
  IN     NVM_STATUS Status
)
{
  SetObjStatusForDimmInfoWithErase(pCommandStatus, pDimm, Status, FALSE);
}

/**
Set object status for DIMM_INFO

@param[out] pCommandStatus Pointer to command status structure
@param[in] pDimm DIMM_INFO for which the object status is being set
@param[in] Status Object status to set
@param[in] If TRUE - clear all other status before setting this one
**/
VOID
SetObjStatusForDimmInfoWithErase(
  OUT COMMAND_STATUS *pCommandStatus,
  IN     DIMM_INFO *pDimm,
  IN     NVM_STATUS Status,
  IN     BOOLEAN EraseFirst
)
{
  UINT32 idx = 0;
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  CHAR16 *TmpDimmUid = NULL;

  if (pDimm == NULL || pCommandStatus == NULL) {
    return;
  }

  for (idx = 0; idx < MAX_DIMM_UID_LENGTH; idx++) {
    DimmUid[idx] = 0;
  }


  if (pDimm->VendorId != 0 && pDimm->ManufacturingInfoValid != FALSE && pDimm->SerialNumber != 0) {
    TmpDimmUid = CatSPrint(NULL, L"%04x", EndianSwapUint16(pDimm->VendorId));
    if (pDimm->ManufacturingInfoValid == TRUE) {
      TmpDimmUid = CatSPrintClean(TmpDimmUid, L"-%02x-%04x", pDimm->ManufacturingLocation, EndianSwapUint16(pDimm->ManufacturingDate));
    }
    TmpDimmUid = CatSPrintClean(TmpDimmUid, L"-%08x", EndianSwapUint32(pDimm->SerialNumber));
  }
  else {
    TmpDimmUid = CatSPrint(NULL, L"");
  }

  if (TmpDimmUid != NULL) {
    StrnCpyS(DimmUid, MAX_DIMM_UID_LENGTH, TmpDimmUid, MAX_DIMM_UID_LENGTH - 1);
    FREE_POOL_SAFE(TmpDimmUid);
  } 

  if (EraseFirst) {
    EraseObjStatus(pCommandStatus, pDimm->DimmHandle, DimmUid, MAX_DIMM_UID_LENGTH);
  }

  SetObjStatus(pCommandStatus, pDimm->DimmHandle, DimmUid, MAX_DIMM_UID_LENGTH, Status);
}
