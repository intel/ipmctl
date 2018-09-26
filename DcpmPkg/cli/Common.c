/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "CommandParser.h"
#include "Common.h"
#include "NvmDimmCli.h"
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Library/UefiShellDebug1CommandsLib/UefiShellDebug1CommandsLib.h>
#include <Utility.h>
#include <Convert.h>
#include <Version.h>
#include <NvmInterface.h>
#include <NvmTypes.h>
#include <Show.h>
#ifdef OS_BUILD
#include <stdio.h>
#include <errno.h>
#endif

CONST CHAR16 *mpImcSize[] = {
  L"Unknown",
  L"64B",
  L"128B",
  L"256B",
  L"4KB",
  L"1GB"
};

CONST CHAR16 *mpChannelSize[] = {
  L"Unknown",
  L"64B",
  L"128B",
  L"256B",
  L"4KB",
  L"1GB"
};

CONST CHAR16 *mpDefaultSizeStrs[DISPLAY_SIZE_MAX_SIZE] = {
  PROPERTY_VALUE_AUTO,
  PROPERTY_VALUE_AUTO10,
  UNITS_OPTION_B,
  UNITS_OPTION_MB,
  UNITS_OPTION_MIB,
  UNITS_OPTION_GB,
  UNITS_OPTION_GIB,
  UNITS_OPTION_TB,
  UNITS_OPTION_TIB
};

CONST CHAR16 *mpDefaultDimmIds[DISPLAY_DIMM_ID_MAX_SIZE] = {
  PROPERTY_VALUE_HANDLE,
  PROPERTY_VALUE_UID,
};

#define ERROR_CHECKING_MIXED_SKU    L"Error: Could not check if SKU is mixed."
#define WARNING_DIMMS_SKU_MIXED     L"Warning: Mixed SKU detected. Driver functionalities limited.\n"

/**
  Retrieve a populated array and count of DIMMs in the system. The caller is
  responsible for freeing the returned array

  @param[in] pNvmDimmConfigProtocol A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] dimmInfoCategories Categories that will be populated in
             the DIMM_INFO struct.
  @param[out] ppDimms A pointer to the dimm list found in NFIT.
  @param[out] pDimmCount A pointer to the number of DIMMs found in NFIT.

  @retval EFI_SUCCESS  the dimm list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetDimmList(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
  OUT DIMM_INFO **ppDimms,
  OUT UINT32 *pDimmCount
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  if (pNvmDimmConfigProtocol == NULL || ppDimms == NULL || pDimmCount == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, pDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on GetDimmCount.");
    goto Finish;
  }

  if (*pDimmCount == 0) {
    Print(FORMAT_STR_NL, CLI_INFO_NO_DIMMS);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  *ppDimms = AllocateZeroPool(sizeof(**ppDimms) * (*pDimmCount));

  if (*ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  /** retrieve the DIMM list **/
  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, *pDimmCount, dimmInfoCategories, *ppDimms);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed to retrieve the DIMM inventory");
    goto FinishError;
  }
  goto Finish;

FinishError:
  FREE_POOL_SAFE(*ppDimms);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Parse the string and return the array of unsigned integers

  Example
    String: "1,3,7"
    Array[0]: 1
    Array[1]: 3
    Array[2]: 7

  @param[in] pString string to parse
  @param[out] ppUints allocated, filled array with the uints
  @param[out] pUintsNum size of uints array

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER the format of string is not proper
**/
EFI_STATUS
GetUintsFromString(
  IN     CHAR16 *pString,
  OUT UINT16 **ppUints,
  OUT UINT32 *pUintsNum
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  CHAR16 **ppUintsStr = NULL;
  UINTN ParsedNumber = 0;
  BOOLEAN IsNumber = FALSE;

  NVDIMM_ENTRY();

  if (pString == NULL || ppUints == NULL || pUintsNum == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /**
    No targets specified - select all targets (If value is required - command won't pass parsing process.)
  **/
  if (StrLen(pString) == 0) {
    *ppUints = NULL;
    *pUintsNum = 0;
    Rc = EFI_SUCCESS;
    goto Finish;
  }

  ppUintsStr = StrSplit(pString, L',', pUintsNum);

  if (ppUintsStr == NULL) {
    Rc = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  *ppUints = AllocateZeroPool(*pUintsNum * sizeof(**ppUints));
  if (*ppUints == NULL) {
    Rc = EFI_OUT_OF_RESOURCES;
    goto FinishError;
  }

  for (Index = 0; Index < *pUintsNum; Index++) {
    IsNumber = GetU64FromString(ppUintsStr[Index], &ParsedNumber);

    if (!IsNumber) {
      Rc = EFI_INVALID_PARAMETER;
      goto FinishError;
    }

    (*ppUints)[Index] = (UINT16)ParsedNumber;
  }
  goto Finish;

FinishError:
  FREE_POOL_SAFE(*ppUints);
Finish:
  FreeStringArray(ppUintsStr, pUintsNum == NULL ? 0 : *pUintsNum);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Parses the dimm target string (which can contain DimmIDs as NFIT handles and/or DimmUIDs),
  and returns an array of DimmIDs in the SMBIOS physical-id forms.
  Also checks for invalid DimmIDs and duplicate entries.

  Example
    String: "8089-00-0000-76543210,30,0x0022"
    Array[0]: 28
    Array[1]: 30
    Array[2]: 34

  @param[in] pDimmString The dimm target string to parse.
  @param[in] pDimmInfo The dimm list found in NFIT.
  @param[in] DimmCount Size of the pDimmInfo array.
  @param[out] ppDimmIds Pointer to the array allocated and filled with the SMBIOS DimmIDs.
  @param[out] pDimmIdsCount Size of the pDimmIds array.

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER inputs are null, the format of string is not proper, duplicated Dimm IDs
  @retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetDimmIdsFromString(
  IN     CHAR16 *pDimmString,
  IN     DIMM_INFO *pDimmInfo,
  IN     UINT32 DimmCount,
  OUT UINT16 **ppDimmIds,
  OUT UINT32 *pDimmIdsCount
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  CHAR16 **ppDimmIdTokensStr = NULL;
  UINT32 *pParsedDimmIdNumber = NULL;
  UINT64 DimmIdNumberTmp = 0;
  BOOLEAN *pIsDimmIdNumber = NULL;
  BOOLEAN DimmIdFound = FALSE;
  UINT32 Index = 0;
  UINT32 Index2 = 0;

  NVDIMM_ENTRY();

  if ((pDimmString == NULL) || (pDimmInfo == NULL) || (ppDimmIds == NULL) || (pDimmIdsCount == NULL)) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /**
    No DIMM targets specified - select all targets (If value is required - command won't pass parsing process.)
  **/
  if (StrLen(pDimmString) == 0) {
    *ppDimmIds = NULL;
    *pDimmIdsCount = 0;
    Rc = EFI_SUCCESS;
    goto Finish;
  }

  ppDimmIdTokensStr = StrSplit(pDimmString, L',', pDimmIdsCount);
  if (ppDimmIdTokensStr == NULL) {
    Rc = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  *ppDimmIds = AllocateZeroPool(*pDimmIdsCount * sizeof(**ppDimmIds));
  pParsedDimmIdNumber = AllocateZeroPool(*pDimmIdsCount * sizeof(*pParsedDimmIdNumber));
  pIsDimmIdNumber = AllocateZeroPool(*pDimmIdsCount * sizeof(*pIsDimmIdNumber));
  if ((*ppDimmIds == NULL) || (pParsedDimmIdNumber == NULL) || (pIsDimmIdNumber == NULL)) {
    Rc = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto FinishError;
  }

  for (Index = 0; Index < *pDimmIdsCount; Index++) {
    pIsDimmIdNumber[Index] = GetU64FromString(ppDimmIdTokensStr[Index], &DimmIdNumberTmp);
    if ((pIsDimmIdNumber[Index]) && (DimmIdNumberTmp > MAX_UINT32)) {
      Print(L"\nDimmID size cannot exceed 32 bits. Invalid DimmID: " FORMAT_STR_NL, ppDimmIdTokensStr[Index]);
      Rc = EFI_INVALID_PARAMETER;
      goto FinishError;
    }

    pParsedDimmIdNumber[Index] = (UINT32)DimmIdNumberTmp;
    DimmIdNumberTmp = 0;
  }

  for (Index = 0; Index < *pDimmIdsCount; Index++) {
    DimmIdFound = FALSE;

    /**
      Checking if the specified DIMMs exist
    **/
    for (Index2 = 0; Index2 < DimmCount; Index2++) {
      if ((!pIsDimmIdNumber[Index] && StrICmp(ppDimmIdTokensStr[Index], pDimmInfo[Index2].DimmUid) == 0) ||
        (pIsDimmIdNumber[Index] && pDimmInfo[Index2].DimmHandle == pParsedDimmIdNumber[Index]))
      {
        // Note: For uninitialized dimms, the DimmID is always 0x0
        // It can cause havok later on if you care about those dimms
        // We should switch to using DimmHandle at some point, as we
        // don't accept DimmID as a command line dimm specifier!
        // (we only accept positional handle and uid)
        (*ppDimmIds)[Index] = pDimmInfo[Index2].DimmID;
        DimmIdFound = TRUE;
        break;
      }
    }

    if (!DimmIdFound) {
      Rc = EFI_NOT_FOUND;
      Print(L"\nDIMM not found. Invalid DimmID: " FORMAT_STR_NL, ppDimmIdTokensStr[Index]);
      goto FinishError;
    }
  }

  /**
    Checking for duplicate entries
  **/
  for (Index = 0; Index < *pDimmIdsCount; Index++) {
    for (Index2 = (Index + 1); Index2 < *pDimmIdsCount; Index2++) {
      if ((*ppDimmIds)[Index] == (*ppDimmIds)[Index2]) {
        Rc = EFI_INVALID_PARAMETER;
        Print(L"\nDuplicated DimmID: " FORMAT_STR_NL, ppDimmIdTokensStr[Index2]);
        goto FinishError;
      }
    }
  }
  goto Finish;

FinishError:
  FREE_POOL_SAFE(*ppDimmIds);
Finish:
  FreeStringArray(ppDimmIdTokensStr, pDimmIdsCount == NULL ? 0 : *pDimmIdsCount);
  FREE_POOL_SAFE(pParsedDimmIdNumber);
  FREE_POOL_SAFE(pIsDimmIdNumber);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
Parses the dimm target string (which can contain DimmIDs as SMBIOS type-17 handles and/or DimmUIDs),
and returns a DimmUid.

Example
String: "8089-00-0000-13325476" or "30" or "0x0022"

@param[in] pDimmString The dimm target string to parse.
@param[in] pDimmInfo The dimm list found in NFIT.
@param[in] DimmCount Size of the pDimmInfo array.
@param[out] pDimmUid Pointer to the NVM_UID buffer.

@retval EFI_SUCCESS
@retval EFI_OUT_OF_RESOURCES memory allocation failure
@retval EFI_INVALID_PARAMETER the format of string is not proper
@retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetDimmUidFromString(
  IN     CHAR16 *pDimmString,
  IN     DIMM_INFO *pDimmInfo,
  IN     UINT32 DimmCount,
  OUT    CHAR8 *pDimmUid
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 ParsedDimmIdNumber;
  UINT64 DimmIdNumberTmp = 0;
  BOOLEAN IsDimmIdNumber;
  BOOLEAN DimmIdFound = FALSE;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if ((pDimmString == NULL) || (pDimmInfo == NULL) || (pDimmUid == NULL)) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /**
  No DIMM targets specified - select all targets (If value is required - command won't pass parsing process.)
  **/
  if (StrLen(pDimmString) == 0) {
    Rc = EFI_SUCCESS;
    goto Finish;
  }

  IsDimmIdNumber = GetU64FromString(pDimmString, &DimmIdNumberTmp);
  if ((IsDimmIdNumber) && (DimmIdNumberTmp > MAX_UINT32)) {
    Print(L"\nDimmID size cannot exceed 32 bits. Invalid DimmID: " FORMAT_STR_NL, pDimmString);
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ParsedDimmIdNumber = (UINT32)DimmIdNumberTmp;
  DimmIdNumberTmp = 0;
  DimmIdFound = FALSE;

  /**
  Checking if the specified DIMMs exist
  **/
  for (Index = 0; Index < DimmCount; Index++) {
    if ((!IsDimmIdNumber && StrICmp(pDimmString, pDimmInfo[Index].DimmUid) == 0) ||
      (IsDimmIdNumber && pDimmInfo[Index].DimmHandle == ParsedDimmIdNumber))
    {
      UnicodeStrToAsciiStrS(pDimmInfo[Index].DimmUid, pDimmUid, MAX_DIMM_UID_LENGTH);
      DimmIdFound = TRUE;
      break;
    }
  }

  if (!DimmIdFound) {
    Rc = EFI_NOT_FOUND;
    Print(L"\nDIMM not found. Invalid DimmID: " FORMAT_STR_NL, pDimmString);
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Check if the uint is in the uints array

  @param[in] pUints array of the uints
  @param[in] UintsNum number of uints in the array
  @param[in] UintToFind searched uint

  @retval TRUE if the uint has been found
  @retval FALSE if the uint has not been found
**/
BOOLEAN
ContainUint(
  IN UINT16 *pUints,
  IN UINT32 UintsNum,
  IN UINT16 UintToFind
)
{
  UINT32 Index;
  BOOLEAN ReturnCode = FALSE;

  NVDIMM_ENTRY();

  if (pUints == NULL) {
    ReturnCode = FALSE;
    goto Finish;
  }

  for (Index = 0; Index < UintsNum; Index++) {
    if (pUints[Index] == UintToFind) {
      ReturnCode = TRUE;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode ? EFI_SUCCESS : EFI_ABORTED);
  return ReturnCode;
}

/**
  Check if the Guid is in the Guids array

  @param[in] ppGuids array of the Guid pointers
  @param[in] GuidsNum number of Guids in the array
  @param[in] pGuidToFind pointer to GUID with information to find

  @retval TRUE if table contains guid with same data as *pGuidToFind
  @retval FALSE
**/
BOOLEAN
ContainGuid(
  IN GUID **ppGuids,
  IN UINT32 GuidsNum,
  IN GUID *pGuidToFind
)
{
  UINT32 Index;
  BOOLEAN ReturnCode = FALSE;

  NVDIMM_ENTRY();

  if (ppGuids == NULL || pGuidToFind == NULL) {
    ReturnCode = FALSE;
    goto Finish;
  }

  for (Index = 0; Index < GuidsNum; Index++) {
    if (CompareMem(ppGuids[Index], pGuidToFind, sizeof(GUID)) == 0) {
      ReturnCode = TRUE;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode ? EFI_SUCCESS : EFI_ABORTED);
  return ReturnCode;
}

/**
  Gets number of Manageable Dimms and their IDs

  @param[out] DimmIdsCount  is the pointer to variable, where number of dimms will be stored.
  @param[out] ppDimmIds is the pointer to variable, where IDs of dimms will be stored.

  @retval EFI_NOT_FOUND if the connection with NvmDimmProtocol can't be estabilished
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
  @retval EFI_INVALID_PARAMETER if number of dimms or dimm IDs have not been assigned properly.
  @retval EFI_SUCCESS if succefully assigned number of dimms and IDs to variables.
**/
EFI_STATUS
GetManageableDimmsNumberAndId(
  OUT UINT32 *pDimmIdsCount,
  OUT UINT16 **ppDimmIds
)
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM_INFO *pDimms = NULL;
  UINT16 Index = 0;
  UINT16 NewListIndex = 0;

  NVDIMM_ENTRY();

  if (pDimmIdsCount == NULL || ppDimmIds == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, pDimmIdsCount);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * (*pDimmIdsCount));
  *ppDimmIds = AllocateZeroPool(sizeof(**ppDimmIds) * (*pDimmIdsCount));
  if (pDimms == NULL || *ppDimmIds == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, *pDimmIdsCount, DIMM_INFO_CATEGORY_NONE, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  for (Index = 0; Index < *pDimmIdsCount; Index++) {
    if (pDimms[Index].ManageabilityState == MANAGEMENT_VALID_CONFIG) {
      (*ppDimmIds)[NewListIndex] = pDimms[Index].DimmID;
      NewListIndex++;
    }
  }
  *pDimmIdsCount = NewListIndex;

  if (NewListIndex == 0) {
    ReturnCode = NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if the provided display list string contains only the valid values.

  @param[in] pDisplayValues pointer to the Unicode string containing the user
    input display list.
  @param[in] ppAllowedDisplayValues pointer to an array of Unicode strings
    that define the valid display values.
  @param[in] Count is the number of valid display values in ppAllowedDisplayValues.

  @retval EFI_SUCCESS if all of the provided display values are valid.
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
  @retval EFI_INVALID_PARAMETER if one or more of the provided display values
    is not a valid one. Or if pDisplayValues or ppAllowedDisplayValues is NULL.
**/
EFI_STATUS
CheckDisplayList(
  IN     CHAR16 *pDisplayValues,
  IN     CHAR16 **ppAllowedDisplayValues,
  IN     UINT16 Count
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 **ppSplitDisplayValues = NULL;
  UINT32 SplitDisplayValuesSize = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  BOOLEAN CorrectDisplayValue = FALSE;

  NVDIMM_ENTRY();

  if (pDisplayValues == NULL || ppAllowedDisplayValues == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ppSplitDisplayValues = StrSplit(pDisplayValues, L',', &SplitDisplayValuesSize);
  if (ppSplitDisplayValues == NULL) {
    Print(L"Error: Out of memory.\n");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  for (Index = 0; Index < SplitDisplayValuesSize; Index++) {
    CorrectDisplayValue = FALSE;

    for (Index2 = 0; Index2 < Count; Index2++) { // Check through all of the valid values
      if (StrICmp(ppSplitDisplayValues[Index], ppAllowedDisplayValues[Index2]) == 0) {
        CorrectDisplayValue = TRUE; // This value is allowed
        break; // If we find a match, leave the loop
      }
    }

    if (!CorrectDisplayValue) { // If this value is not allowed, set the return code.
      ReturnCode = EFI_INVALID_PARAMETER;
    }
  }

  FreeStringArray(ppSplitDisplayValues, SplitDisplayValuesSize);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if user has specified the options -a|-all and -d|-display.
  Those two flags exclude each other so the function also checks
  if the user didn't provide them both.
  If the -d|-display option has been found, the its values are checked
  against the allowed values for this parameter.

  @param[in] pCommand is the pointer to a Command structure that is tested
    for the options presence.
  @param[in] ppAllowedDisplayValues is a pointer to an array of Unicode
    strings considered as the valid values for the -d|-display option.
  @param[in] AllowedDisplayValuesCount is a UINT32 value that represents
    the number of elements in the array pointed by ppAllowedDisplayValues.
  @param[out] pAllOptionSet is a pointer to a BOOLEAN value that will
    represent the presence of the -a|-all option in the Command pointed
    by pCommand.
  @param[out] pDisplayOptionSet is a pointer to a BOOLEAN value that will
    represent the presence of the -d|-display option in the Command pointed
    by pCommand.
  @param[out] ppDisplayOptionValue is a pointer to an Unicode string
    pointer. If the -d|-display option is present, this pointer will
    be set to the option value Unicode string.

  @retval EFI_SUCCESS the check went fine, there were no errors
  @retval EFI_INVALID_PARAMETER if the user provided both options,
    the display option has been provided and has some invalid values or
    if at least one of the input pointer parameters is NULL.
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
**/
EFI_STATUS
CheckAllAndDisplayOptions(
  IN     struct Command *pCommand,
  IN     CHAR16 **ppAllowedDisplayValues,
  IN     UINT32 AllowedDisplayValuesCount,
  OUT BOOLEAN *pAllOptionSet,
  OUT BOOLEAN *pDisplayOptionSet,
  OUT CHAR16 **ppDisplayOptionValue
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pDisplayValues = NULL;

  NVDIMM_ENTRY();

  if (pAllOptionSet == NULL || ppAllowedDisplayValues == NULL || pCommand == NULL
    || pDisplayOptionSet == NULL || ppDisplayOptionValue == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** if the all option was specified **/
  if (containsOption(pCommand, ALL_OPTION) || containsOption(pCommand, ALL_OPTION_SHORT)) {
    *pAllOptionSet = TRUE;
  }
  /** if the display option was specified **/
  pDisplayValues = getOptionValue(pCommand, DISPLAY_OPTION);
  if (pDisplayValues) {
    *pDisplayOptionSet = TRUE;
  }
  else {
    pDisplayValues = getOptionValue(pCommand, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      *pDisplayOptionSet = TRUE;
    }
  }

  *ppDisplayOptionValue = pDisplayValues;

  /** make sure they didn't specify both the all and display options **/
  if (*pAllOptionSet && *pDisplayOptionSet) {
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Check that the display parameters are correct (if display option is set) **/
  if (*pDisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, ppAllowedDisplayValues,
      (UINT16)AllowedDisplayValuesCount);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if the DIMMs Sku is mixed.
  If it is - displays a message to notify the user.
  Also if there was an error while getting the
  DIMMs status a proper message is printed.
**/
VOID
WarnUserIfSkuIsMixed(
)
{
  BOOLEAN SkuMixed = FALSE;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  ReturnCode = IsSkuMixed(&SkuMixed);

  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, ERROR_CHECKING_MIXED_SKU);
  }
  else {
    if (SkuMixed) {
      Print(FORMAT_STR_NL, WARNING_DIMMS_SKU_MIXED);
    }
  }

  NVDIMM_EXIT();
}

/**
  Display command status with specified command message.
  Function displays per DIMM status if such exists and
  summarizing status for whole command. Memory allocated
  for status message and command status is freed after
  status is displayed.

  @param[in] pStatusMessage String with command information
  @param[in] pStatusPreposition String with preposition
  @param[in] pCommandStatus Command status data

  @retval EFI_INVALID_PARAMETER pCommandStatus is NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
DisplayCommandStatus(
  IN     CONST CHAR16 *pStatusMessage,
  IN     CONST CHAR16 *pStatusPreposition,
  IN     COMMAND_STATUS *pCommandStatus
)

{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pOutputBuffer = NULL;
  UINT8 DimmIdentifier = 0;
  BOOLEAN ObjectIdNumberPreferred = FALSE;

  if (pStatusMessage == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  ReturnCode = GetDimmIdentifierPreference(&DimmIdentifier);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ObjectIdNumberPreferred = DimmIdentifier == DISPLAY_DIMM_ID_HANDLE;

  ReturnCode = CreateCommandStatusString(gNvmDimmCliHiiHandle, pStatusMessage, pStatusPreposition, pCommandStatus,
    ObjectIdNumberPreferred, &pOutputBuffer);

  Print(FORMAT_STR, pOutputBuffer);

Finish:
  FREE_POOL_SAFE(pOutputBuffer);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Print namespace operation status

  @param[in] pCommandStatus - Command status
  @param[in] pStatusOperation - Type of operation
  @param[in] pStatusFailure - Failure text
  @param[in] NamespaceGuid - Namespace ID

  @retval EFI_INVALID_PARAMETER if pCommandStatus is NULL
  @retval EFI_SUCCESS
**/
EFI_STATUS
DisplayNamespaceOperationStatus(
  IN     COMMAND_STATUS *pCommandStatus,
  IN     CONST CHAR16 *pStatusOperation,
  IN     CONST CHAR16 *pStatusFailure,
  IN     UINT16 NamespaceId
)
{
  CHAR16 *pSingleStatusCodeMessage = NULL;

  NVDIMM_ENTRY();

  if (pCommandStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (!NVM_ERROR(pCommandStatus->GeneralStatus)) {
    Print(FORMAT_STR_SPACE L"namespace (0x%04x): Success\n", pStatusOperation, NamespaceId);
  }
  else {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, pCommandStatus->GeneralStatus);
    Print(FORMAT_STR_SPACE L"namespace (0x%04x): " FORMAT_STR L" (%d) (" FORMAT_STR L")\n", pStatusOperation, NamespaceId, pStatusFailure,
      pCommandStatus->GeneralStatus, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  NVDIMM_EXIT();
  return EFI_SUCCESS;
}

/**
  Retrieve property by name and assign its value to UINT64.

  @param[in] pCmd Command containing the property
  @param[in] pPropertyName String with property name

  @param[out] pOutValue target UINT64 value

  @retval FALSE if there was no such property or it doesn't contain
    a valid value
**/
BOOLEAN
PropertyToUint64(
  IN     struct Command *pCmd,
  IN     CHAR16 *pPropertyName,
  OUT UINT64 *pOutValue
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN IsValid = FALSE;
  CHAR16 *pStringValue = NULL;

  ReturnCode = GetPropertyValue(pCmd, pPropertyName, &pStringValue);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  IsValid = GetU64FromString(pStringValue, pOutValue);

Finish:
  return IsValid;
}

/**
  Retrieve property by name and assign its value to double

  @param[in] pCmd Command containing the property
  @param[in] pPropertyName String with property name
  @param[out] pOutValue Target double value

  @retval EFI_INVALID_PARAMETER Property not found or no valid value inside
  @retval EFI_SUCCESS Conversion successful
**/
EFI_STATUS
PropertyToDouble(
  IN     struct Command *pCmd,
  IN     CHAR16 *pPropertyName,
  OUT double *pOutValue
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pPropertyValue = NULL;

  if (pCmd == NULL || pPropertyName == NULL || pOutValue == NULL) {
    goto Finish;
  }

  ReturnCode = GetPropertyValue(pCmd, pPropertyName, &pPropertyValue);
  if (EFI_ERROR(ReturnCode) || pPropertyValue == NULL) {
    goto Finish;
  }

  ReturnCode = StringToDouble(gNvmDimmCliHiiHandle, pPropertyValue, pOutValue);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

  NVDIMM_DBG("Converted %s string to %f double", pPropertyValue, *pOutValue);

Finish:
  return ReturnCode;
}

/**
  Extracts working directory path from file path

  @param[in] pUserFilePath Pointer to string with user specified file path
  @param[out] pOutFilePath Pointer to actual file path
  @param[out] ppDevicePath Pointer to where to store device path

  @retval EFI_SUCCESS Extraction success
  @retval EFI_INVALID_PARAMETER Invalid parameter
  @retval EFI_OUT_OF_RESOURCES Out of resources
**/
EFI_STATUS
GetDeviceAndFilePath(
  IN     CHAR16 *pUserFilePath,
  OUT CHAR16 *pOutFilePath,
  OUT  EFI_DEVICE_PATH_PROTOCOL **ppDevicePath
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_SHELL_PROTOCOL *pEfiShell = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pDevPathInternal = NULL;
  EFI_HANDLE *pHandles = NULL;
  UINTN HandlesCount = 0;
  CHAR16 *pTmpFilePath = NULL;
  CHAR16 *pTmpWorkingDir = NULL;
  CONST CHAR16* pCurDir = NULL;
  CHAR16 *pCurDirPath = NULL;
  NVDIMM_ENTRY();

  if (pUserFilePath == NULL || pOutFilePath == NULL || ppDevicePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }
#ifdef OS_BUILD
  StrnCpyS(pOutFilePath, OPTION_VALUE_LEN, pUserFilePath, OPTION_VALUE_LEN - 1);
  return EFI_SUCCESS;
#endif
  pTmpWorkingDir = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pTmpWorkingDir));
  if (pTmpWorkingDir == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Add " .\ "(current dir) to the file path if no path is specified
  if (!ContainsCharacter(L'\\', pUserFilePath)) {
    pCurDirPath = CatSPrint(NULL, L".\\" FORMAT_STR, pUserFilePath);
  }
  else {
    pCurDirPath = CatSPrint(NULL, pUserFilePath);
  }
  if (pCurDirPath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Get Efi Shell Protocol
  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, &gEfiShellProtocolGuid, NULL, &HandlesCount, &pHandles);
  if (EFI_ERROR(ReturnCode) || HandlesCount >= MAX_SHELL_PROTOCOL_HANDLES) {
    NVDIMM_WARN("Error while opening the shell protocol. Code: " FORMAT_EFI_STATUS "", ReturnCode);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
  ReturnCode = gBS->OpenProtocol(
    pHandles[0],
    &gEfiShellProtocolGuid,
    (VOID *)&pEfiShell,
    NULL,
    NULL,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL
  );
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Error while opening the shell protocol. Code: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

  // If User has not typed "Fsx:\", get current working directory
  if (!ContainsCharacter(L':', pCurDirPath)) {
    // Otherwise, path is relative to current directory
    pCurDir = pEfiShell->GetCurDir(NULL);
    if (pCurDir == NULL) {
      NVDIMM_DBG("Error while getting the Working Directory.");
      goto Finish;
    }
    if (StrLen(pCurDir) + 1 > OPTION_VALUE_LEN) {
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    StrnCpyS(pTmpWorkingDir, OPTION_VALUE_LEN, pCurDir, OPTION_VALUE_LEN - 1);
    // Take null terminator into account
    StrnCatS(pTmpWorkingDir, OPTION_VALUE_LEN, pCurDirPath, OPTION_VALUE_LEN - StrLen(pTmpWorkingDir) - 1);
  }
  else {
    StrnCpyS(pTmpWorkingDir, OPTION_VALUE_LEN, pCurDirPath, OPTION_VALUE_LEN - 1);
  }

  // Extract working directory
  pTmpFilePath = pTmpWorkingDir;
  while (pTmpFilePath[0] != L'\\' && pTmpFilePath[0] != L'\0') {
    pTmpFilePath++;
  }
  StrnCpyS(pOutFilePath, OPTION_VALUE_LEN, pTmpFilePath, OPTION_VALUE_LEN - 1);

  // Get Path to Device
  pDevPathInternal = pEfiShell->GetDevicePathFromFilePath(pTmpWorkingDir);
  if (pDevPathInternal == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_WRONG_FILE_PATH);
    goto Finish;
  }

  *ppDevicePath = pDevPathInternal;
  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pCurDirPath);
  if (pTmpWorkingDir != NULL) {
    FreePool(pTmpWorkingDir);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Match driver command status to CLI return code

  @param[in] Status - NVM_STATUS returned from driver

  @retval - Appropriate EFI return code
**/
EFI_STATUS
MatchCliReturnCode(
  IN     NVM_STATUS Status
)
{
  EFI_STATUS ReturnCode = EFI_ABORTED;
  switch (Status) {
  case NVM_SUCCESS:
  case NVM_SUCCESS_IMAGE_EXAMINE_OK:
  case NVM_SUCCESS_FW_RESET_REQUIRED:
  case NVM_WARN_BLOCK_MODE_DISABLED:
  case NVM_WARN_2LM_MODE_OFF:
  case NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU:
    ReturnCode = EFI_SUCCESS;
    break;

  case NVM_ERR_PASSPHRASE_TOO_LONG:
  case NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED:
  case NVM_ERR_PASSPHRASE_NOT_PROVIDED:
  case NVM_ERR_PASSPHRASES_DO_NOT_MATCH:
  case NVM_ERR_IMAGE_FILE_NOT_VALID:
  case NVM_ERR_SENSOR_NOT_VALID:
  case NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE:
  case NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE:
  case NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE:
  case NVM_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE:
  case NVM_ERR_UNSUPPORTED_BLOCK_SIZE:
  case NVM_ERR_NONE_DIMM_FULFILLS_CRITERIA:
  case NVM_ERR_INVALID_NAMESPACE_CAPACITY:
  case NVM_ERR_NAMESPACE_TOO_SMALL_FOR_BTT:
  case NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_BLOCK_NAMESPACE:
  case NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_PM_NAMESPACE:
  case NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS:
  case NVM_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS:
  case NVM_ERR_INVALID_PARAMETER:
    ReturnCode = EFI_INVALID_PARAMETER;
    break;

  case NVM_ERR_NOT_ENOUGH_FREE_SPACE:
  case NVM_ERR_NOT_ENOUGH_FREE_SPACE_BTT:
    ReturnCode = EFI_OUT_OF_RESOURCES;
    break;

  case NVM_ERR_DIMM_NOT_FOUND:
  case NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND:
  case NVM_ERR_SOCKET_ID_NOT_VALID:
  case NVM_ERR_REGION_NOT_FOUND:
  case NVM_ERR_NAMESPACE_DOES_NOT_EXIST:
  case NVM_ERR_REGION_NO_GOAL_EXISTS_ON_DIMM:
    ReturnCode = EFI_NOT_FOUND;
    break;

  case NVM_ERR_ENABLE_SECURITY_NOT_ALLOWED:
  case NVM_ERR_CREATE_GOAL_NOT_ALLOWED:
  case NVM_ERR_INVALID_SECURITY_STATE:
  case NVM_ERR_INVALID_PASSPHRASE:
  case NVM_ERR_RECOVERY_ACCESS_NOT_ENABLED:
    ReturnCode = EFI_ACCESS_DENIED;
    break;

  case NVM_ERR_OPERATION_NOT_STARTED:
  case NVM_ERR_FORCE_REQUIRED:
  case NVM_ERR_OPERATION_FAILED:
  case NVM_ERR_DIMM_ID_DUPLICATED:
  case NVM_ERR_SOCKET_ID_DUPLICATED:
  case NVM_ERR_UNABLE_TO_GET_SECURITY_STATE:
  case NVM_ERR_INCONSISTENT_SECURITY_STATE:
  case NVM_ERR_SECURITY_COUNT_EXPIRED:
  case NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM:
  case NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM:
  case NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM:
  case NVM_ERR_REGION_CONF_APPLYING_FAILED:
  case NVM_ERR_REGION_CONF_UNSUPPORTED_CONFIG:
  case NVM_ERR_DUMP_FILE_OPERATION_FAILED:
  case NVM_ERR_LOAD_VERSION:
  case NVM_ERR_LOAD_INVALID_DATA_IN_FILE:
  case NVM_ERR_LOAD_IMPROPER_CONFIG_IN_FILE:
  case NVM_ERR_LOAD_DIMM_COUNT_MISMATCH:
  case NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN:
  case NVM_ERR_INVALID_SECURITY_OPERATION:
  case NVM_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED:
  case NVM_ERR_DUMP_NO_CONFIGURED_DIMMS:
  case NVM_ERR_REGION_NOT_HEALTHY:
  case NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS:
  case NVM_ERR_FAILED_TO_UPDATE_BTT:
  case NVM_ERR_SMBIOS_DIMM_ENTRY_NOT_FOUND_IN_NFIT:
  case NVM_ERR_IMAGE_FILE_NOT_COMPATIBLE_TO_CTLR_STEPPING:
  case NVM_ERR_IMAGE_EXAMINE_INVALID:
  case NVM_ERR_FIRMWARE_API_NOT_VALID:
  case NVM_ERR_FIRMWARE_VERSION_NOT_VALID:
  case NVM_ERR_REGION_GOAL_NAMESPACE_EXISTS:
  case NVM_ERR_REGION_REMAINING_SIZE_NOT_IN_LAST_PROPERTY:
  case NVM_ERR_ARS_IN_PROGRESS:
  case NVM_ERR_APPDIRECT_IN_SYSTEM:
  case NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU:
  case NVM_ERR_SECURE_ERASE_NAMESPACE_EXISTS:
    ReturnCode = EFI_ABORTED;
    break;

  case NVM_ERR_OPERATION_NOT_SUPPORTED:
    ReturnCode = EFI_UNSUPPORTED;
    break;

  default:
    ReturnCode = EFI_ABORTED;
    break;
  }
  return ReturnCode;
}

/**
  Get free space of volume from given path

  @param[in] pFileHandle - file handle protocol
  @param[out] pFreeSpace - free space

  @retval - Appropriate EFI return code
**/
EFI_STATUS
GetVolumeFreeSpace(
  IN      EFI_FILE_HANDLE pFileHandle,
  OUT  UINT64  *pFreeSpace
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_SYSTEM_INFO *pFileSystemInfo = NULL;
  EFI_GUID FileSystemInfoGuid = EFI_FILE_SYSTEM_INFO_ID;
  UINT64 BufferSize = MAX_FILE_SYSTEM_STRUCT_SIZE;
  NVDIMM_ENTRY();

  if (pFreeSpace == NULL || pFileHandle == NULL) {
    goto Finish;
  }

  pFileSystemInfo = AllocateZeroPool(BufferSize);
  if (pFileSystemInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  ReturnCode = pFileHandle->GetInfo(pFileHandle, &FileSystemInfoGuid, &BufferSize, pFileSystemInfo);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  *pFreeSpace = pFileSystemInfo->FreeSpace;

Finish:
  FREE_POOL_SAFE(pFileSystemInfo);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;

}

/**
  Check if file exists

  @param[in] pDumpUserPath - destination file path
  @param[out] pExists - pointer to whether or not destination file already exists

  @retval - Appropriate EFI return code
**/
EFI_STATUS
FileExists(
  IN     CHAR16* pDumpUserPath,
  OUT BOOLEAN* pExists
)
{

  EFI_FILE_HANDLE pFileHandle = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  *pExists = FALSE;

  NVDIMM_ENTRY();

#ifdef OS_BUILD
  pFileHandle = NULL;
  ReturnCode = OpenFile(pDumpUserPath, &pFileHandle, NULL, FALSE);
  *pExists = ReturnCode != EFI_NOT_FOUND;
  ReturnCode = EFI_SUCCESS;
#else
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  CHAR16 *pDumpFilePath = NULL;

  pDumpFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDumpFilePath));
  if (pDumpFilePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = GetDeviceAndFilePath(pDumpUserPath, pDumpFilePath, &pDevicePathProtocol);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }

  ReturnCode = OpenFileByDevice(pDumpFilePath, pDevicePathProtocol, FALSE, &pFileHandle);
  if (!EFI_ERROR(ReturnCode)) {
    *pExists = TRUE;
    pFileHandle->Close(pFileHandle);
  }
#endif
Finish:
#ifndef OS_BUILD
  FREE_POOL_SAFE(pDumpFilePath);
#endif
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Delete file

  @param[in] pDumpUserPath - file path to delete

  @retval - Appropriate EFI return code
**/
EFI_STATUS
DeleteFile(
  IN     CHAR16* pFilePath
)
{
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  EFI_FILE_HANDLE pFileHandle = NULL;
  CHAR16 *pDumpFilePath = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE RootDirHandle = NULL;

  NVDIMM_ENTRY();

  pDumpFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDumpFilePath));
  if (pDumpFilePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = GetDeviceAndFilePath(pFilePath, pDumpFilePath, &pDevicePathProtocol);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }
  ReturnCode = OpenRootFileVolume(pDevicePathProtocol, &RootDirHandle);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to open file volume (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }

  ReturnCode = RootDirHandle->Open(RootDirHandle, &pFileHandle, pFilePath, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (!EFI_ERROR(ReturnCode)) {
    ReturnCode = pFileHandle->Delete(pFileHandle);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to delete file path (" FORMAT_EFI_STATUS ")", ReturnCode);
      goto Finish;
    }
  }

Finish:
  FREE_POOL_SAFE(pDumpFilePath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Dump data to file

  @param[in] pDumpUserPath - destination file path
  @param[in] BufferSize - data size to write
  @param[in] pBuffer - pointer to buffer
  @param[in] Overwrite - enforce overwriting file

  @retval - Appropriate EFI return code
**/
EFI_STATUS
DumpToFile(
  IN     CHAR16* pDumpUserPath,
  IN     UINT64 BufferSize,
  OUT VOID* pBuffer,
  IN     BOOLEAN Overwrite
)
{
#ifdef OS_BUILD
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR8 *path = (CHAR8 *)AllocatePool(StrLen(pDumpUserPath) + 1);
  if (NULL == path) {
    NVDIMM_WARN("Failed to allocate enough memory.");
    return EFI_OUT_OF_RESOURCES;
  }
  UnicodeStrToAsciiStrS(pDumpUserPath, path, StrLen(pDumpUserPath) + 1);
  FILE *destFile = fopen(path, "wb+");
  if (NULL == destFile) {
    NVDIMM_WARN("Failed to open file (%s) errno: (%d)", path, errno);
    FreePool(path);
    return EFI_INVALID_PARAMETER;
  }
  size_t bytes_written = fwrite(pBuffer, 1, (size_t)BufferSize, destFile);
  if (bytes_written != BufferSize) {
    NVDIMM_WARN("Failed to write file (%s) errno: (%d)", path, errno);
    ReturnCode = EFI_INVALID_PARAMETER;
  }

  FreePool(path);
  fclose(destFile);
  return ReturnCode;
#else
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  EFI_FILE_HANDLE pFileHandle = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE RootDirHandle = NULL;
  CHAR16 *pDumpFilePath = NULL;
  UINT64 FileSize = 0;
  UINT64 FreeVolumeSpace = 0;
  UINT64 SizeToWrite = 0;
  NVDIMM_ENTRY();

  if (pDumpUserPath == NULL || pBuffer == NULL) {
    goto Finish;
  }

  pDumpFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDumpFilePath));
  if (pDumpFilePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = GetDeviceAndFilePath(pDumpUserPath, pDumpFilePath, &pDevicePathProtocol);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }

  ReturnCode = OpenRootFileVolume(pDevicePathProtocol, &RootDirHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = GetVolumeFreeSpace(RootDirHandle, &FreeVolumeSpace);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (FreeVolumeSpace < BufferSize) {
    ReturnCode = EFI_VOLUME_FULL;
    goto Finish;
  }

  // Create new file for dump
  ReturnCode = OpenFileByDevice(pDumpFilePath, pDevicePathProtocol, TRUE, &pFileHandle);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to open file (" FORMAT_EFI_STATUS ") (%s)", ReturnCode, pDumpFilePath);
    goto Finish;
  }

  // Get File Size
  ReturnCode = GetFileSize(pFileHandle, &FileSize);
  // Check if file already exists and has some size
  if (FileSize != 0) {
    if (Overwrite) {
      ReturnCode = pFileHandle->Delete(pFileHandle);

      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed deleting old dump file (" FORMAT_EFI_STATUS ")", ReturnCode);
        goto Finish;
      }

      // Create new file for dump
      ReturnCode = OpenFileByDevice(pDumpFilePath, pDevicePathProtocol, TRUE, &pFileHandle);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to create dump file (" FORMAT_EFI_STATUS ")", ReturnCode);
        goto Finish;
      }
    }
    else {
      NVDIMM_WARN("File exists and we're not allowed to overwrite (%s)", pDumpFilePath);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
  SizeToWrite = BufferSize;
  ReturnCode = pFileHandle->Write(pFileHandle, &SizeToWrite, pBuffer);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Error occurred during write (%s)", pDumpFilePath);
    goto FinishCloseFile;
  }

FinishCloseFile:
  ReturnCode = pFileHandle->Close(pFileHandle);

Finish:
  FREE_POOL_SAFE(pDumpFilePath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
#endif
}

/**
  Prints supported or recommended appdirect settings

  @param[in] pFormatList pointer to variable length interleave formats array
  @param[in] FormatNum number of the appdirect settings formats
  @param[in] PrintRecommended if TRUE Recommended settings will be printed
             if FALSE Supported settings will be printed
  @param[in] Mode Set mode to print different format
**/
VOID
PrintAppDirectSettings(
  IN    INTERLEAVE_FORMAT *pFormatList,
  IN    UINT16 FormatNum,
  IN    BOOLEAN PrintRecommended,
  IN    UINT8 Mode
)
{
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 InterleaveWay = 0;
  UINT32 WayNumber = 0;
  UINT32 ImcStringIndex = 0;
  UINT32 ChannelStringIndex = 0;
  BOOLEAN First = TRUE;

  if (pFormatList == NULL) {
    return;
  }

  if (Mode == PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD) {
    if (PrintRecommended) {
      Print(L"RecommendedAppDirectSettings=");
    }
    else {
      Print(L"SupportedAppDirectSettings=");
    }
  }

  for (Index = 0; Index < FormatNum; Index++) {
    if (PrintRecommended && !pFormatList[Index].InterleaveFormatSplit.Recommended) {
      continue;
    }

    for (Index2 = 0; Index2 < NUMBER_OF_CHANNEL_WAYS_BITS_NUM; Index2++) {
      /** Check each bit **/
      InterleaveWay = pFormatList[Index].InterleaveFormatSplit.NumberOfChannelWays & (1 << Index2);

      switch (InterleaveWay) {
      case INTERLEAVE_SET_1_WAY:
        WayNumber = 1;
        break;
      case INTERLEAVE_SET_2_WAY:
        WayNumber = 2;
        break;
      case INTERLEAVE_SET_3_WAY:
        WayNumber = 3;
        break;
      case INTERLEAVE_SET_4_WAY:
        WayNumber = 4;
        break;
      case INTERLEAVE_SET_6_WAY:
        WayNumber = 6;
        break;
      case INTERLEAVE_SET_8_WAY:
        WayNumber = 8;
        break;
      case INTERLEAVE_SET_12_WAY:
        WayNumber = 12;
        break;
      case INTERLEAVE_SET_16_WAY:
        WayNumber = 16;
        break;
      case INTERLEAVE_SET_24_WAY:
        WayNumber = 24;
        break;
      default:
        WayNumber = 0;
        break;
      }

      if (WayNumber == 0) {
        continue;
      }

      switch (pFormatList[Index].InterleaveFormatSplit.iMCInterleaveSize) {
      case IMC_INTERLEAVE_SIZE_64B:
        ImcStringIndex = 1;
        break;
      case IMC_INTERLEAVE_SIZE_128B:
        ImcStringIndex = 2;
        break;
      case IMC_INTERLEAVE_SIZE_256B:
        ImcStringIndex = 3;
        break;
      case IMC_INTERLEAVE_SIZE_4KB:
        ImcStringIndex = 4;
        break;
      case IMC_INTERLEAVE_SIZE_1GB:
        ImcStringIndex = 5;
        break;
      default:
        ImcStringIndex = 0;
        break;
      }

      switch (pFormatList[Index].InterleaveFormatSplit.ChannelInterleaveSize) {
      case CHANNEL_INTERLEAVE_SIZE_64B:
        ChannelStringIndex = 1;
        break;
      case CHANNEL_INTERLEAVE_SIZE_128B:
        ChannelStringIndex = 2;
        break;
      case CHANNEL_INTERLEAVE_SIZE_256B:
        ChannelStringIndex = 3;
        break;
      case CHANNEL_INTERLEAVE_SIZE_4KB:
        ChannelStringIndex = 4;
        break;
      case CHANNEL_INTERLEAVE_SIZE_1GB:
        ChannelStringIndex = 5;
        break;
      default:
        ChannelStringIndex = 0;
        break;
      }

      if (ImcStringIndex >= sizeof(mpImcSize)) {
        ImcStringIndex = 0;
      }

      if (ChannelStringIndex >= sizeof(mpChannelSize)) {
        ChannelStringIndex = 0;
      }

      if (!First) {
        Print(L", ");
      }
      else {
        First = FALSE;
      }

      if (Mode == PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD) {
        if (InterleaveWay == INTERLEAVE_SET_1_WAY) {
          Print(L"x1 (ByOne)");
        }
        else {
          Print(L"x%d - " FORMAT_STR L" iMC x " FORMAT_STR L" Channel (", WayNumber, mpImcSize[ImcStringIndex], mpChannelSize[ChannelStringIndex]);
          Print(FORMAT_STR L"_" FORMAT_STR L")", mpImcSize[ImcStringIndex], mpChannelSize[ChannelStringIndex]);
        }
      }
      else if (Mode == PRINT_SETTINGS_FORMAT_FOR_SHOW_REGION_CMD) {
        if (InterleaveWay == INTERLEAVE_SET_1_WAY) {
          Print(L"x1 (ByOne)");
        }
        else {
          Print(L"x%d - " FORMAT_STR L" iMC x " FORMAT_STR L" Channel (" FORMAT_STR L"_" FORMAT_STR L")", WayNumber, mpImcSize[ImcStringIndex],
            mpChannelSize[ChannelStringIndex], mpImcSize[ImcStringIndex], mpChannelSize[ChannelStringIndex]);
        }
      }
    }
  }
  Print(L"\n");
}

/**
  Read source file and return current passphrase to unlock device.

  @param[in] pFileHandle File handler to read Passphrase from
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the
                          physical device or logical device. The device path describes the location of the device
                          the handle is for.
  @param[out] ppCurrentPassphrase
  @param[out] ppNewPassphrase

  @retval EFI_SUCCESS File load and parse success
  @retval EFI_INVALID_PARAMETER Invalid Parameter during load
  @retval other Return Codes from TrimLineBuffer,
                GetLoadPoolData, GetLoadDimmData, GetLoadValue functions
**/
EFI_STATUS
ParseSourcePassFile(
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  OUT CHAR16 **ppCurrentPassphrase OPTIONAL,
  OUT CHAR16 **ppNewPassphrase OPTIONAL
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pReadBuffer = NULL;
  UINT32 Index = 0;
  UINT32 NumberOfLines = 0;
  UINT64 FileBufferSize = 0;
  UINT64 StringLength = 0;
  CHAR16 **ppLinesBuffer = NULL;
  VOID *pFileBuffer = NULL;
  CHAR16 *pPassFromFile = NULL;
  CHAR16 *pFileString = NULL;
  UINT32 NumberOfChars = 0;
  BOOLEAN PassphraseProvided = FALSE;
  BOOLEAN NewPassphraseProvided = FALSE;

  NVDIMM_ENTRY();
#ifndef OS_BUILD
  if (pDevicePath == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Invalid Pointer");
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }
#endif
  if (pFilePath == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Invalid Pointer");
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  ReturnCode = FileRead(pFilePath, pDevicePath, MAX_CONFIG_DUMP_FILE_SIZE, &FileBufferSize, (VOID **)&pFileBuffer);
  if (EFI_ERROR(ReturnCode) || pFileBuffer == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_WRONG_FILE_PATH);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Verify if it is Unicode file:
  //If it is not a Unicode File Convert the File String
  if (*((CHAR16 *)pFileBuffer) != UTF_16_BOM) {
    pFileString = AllocateZeroPool((FileBufferSize * sizeof(CHAR16)) + sizeof(L'\0'));
    if (pFileString == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    ReturnCode = SafeAsciiStrToUnicodeStr((const CHAR8 *)pFileBuffer, (UINT32)FileBufferSize, pFileString);
    Index = 0;
    FREE_POOL_SAFE(pFileBuffer);
  }
  else {
    // Add size of L'\0' (UTF16) char
    // ReallocatePool frees pFileBuffer after completion. Do not need to call FREE_POOL_SAFE for pFileBuffer
    pFileString = ReallocatePool(FileBufferSize, FileBufferSize + sizeof(L'\0'), pFileBuffer);
    if (pFileString == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    Index = 1;
    NumberOfChars = (UINT32)(FileBufferSize / sizeof(CHAR16));
    pFileString[NumberOfChars] = L'\0';
  }

  // Split input file to lines
  ppLinesBuffer = StrSplit(&pFileString[Index], L'\n', &NumberOfLines);
  if (ppLinesBuffer == NULL || NumberOfLines == 0) {
    Print(L"Error: The file is empty.\n");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < NumberOfLines; ++Index) {
    // Ignore comment line that starts with '#'
    if (StrStr(ppLinesBuffer[Index], L"#") != NULL) {
      continue;
    }
    pPassFromFile = (CHAR16*)StrStr(ppLinesBuffer[Index], L"=");
    if (pPassFromFile == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_INVALID_PASSPHRASE_FROM_FILE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    // Move offset to skip '=' char
    pPassFromFile++;
    StringLength = StrLen(pPassFromFile);
    if (StringLength == 0) {
      Print(FORMAT_STR_NL, CLI_ERR_INVALID_PASSPHRASE_FROM_FILE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    // Cut off carriage return
    if (pPassFromFile[StringLength - 1] == L'\r') {
      StringLength--;
      if (StringLength == 0) {
        Print(FORMAT_STR_NL, CLI_ERR_INVALID_PASSPHRASE_FROM_FILE);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      pPassFromFile[StringLength] = L'\0';
    }

    NewPassphraseProvided =
      StrnCmp(ppLinesBuffer[Index], NEWPASSPHRASE_PROPERTY, StrLen(NEWPASSPHRASE_PROPERTY)) == 0;
    PassphraseProvided =
      StrnCmp(ppLinesBuffer[Index], PASSPHRASE_PROPERTY, StrLen(PASSPHRASE_PROPERTY)) == 0;

    if (ppNewPassphrase != NULL && *ppNewPassphrase == NULL && NewPassphraseProvided) {
      *ppNewPassphrase = CatSPrint(NULL, FORMAT_STR, pPassFromFile);
    }
    else if (ppCurrentPassphrase != NULL && *ppCurrentPassphrase == NULL && PassphraseProvided) {
      *ppCurrentPassphrase = CatSPrint(NULL, FORMAT_STR, pPassFromFile);
    }
    else {
      Print(FORMAT_STR_NL, CLI_ERR_WRONG_FILE_DATA);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

Finish:
  for (Index = 0; ppLinesBuffer != NULL && Index < NumberOfLines; ++Index) {
    FREE_POOL_SAFE(ppLinesBuffer[Index]);
  }
  FREE_POOL_SAFE(pFileString);
  FREE_POOL_SAFE(ppLinesBuffer);
  FREE_POOL_SAFE(pReadBuffer);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#ifndef OS_BUILD
/**
  Prompted input request

  @param[in] pPrompt - information about expected input
  @param[in] ShowInput - Show characters written by user
  @param[in] OnlyAlphanumeric - Allow only for alphanumeric characters
  @param[out] ppReturnValue - is a pointer to a pointer to the 16-bit character string
        that will contain the return value

  @retval - Appropriate CLI return code
**/
EFI_STATUS
PromptedInput(
  IN     CHAR16 *pPrompt,
  IN     BOOLEAN ShowInput,
  IN     BOOLEAN OnlyAlphanumeric,
  OUT CHAR16 **ppReturnValue
)
{
  CHAR16 *pBuffer = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (pPrompt == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Print(FORMAT_STR, pPrompt);
  ReturnCode = ConsoleInput(ShowInput, OnlyAlphanumeric, &pBuffer, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *ppReturnValue = pBuffer;

Finish:
  Print(L"\n");
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Display "yes/no" question and retrieve reply using prompt mechanism

  @param[out] pConfirmation Confirmation from prompt

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
PromptYesNo(
  OUT BOOLEAN *pConfirmation
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pPromptReply = NULL;
  BOOLEAN ValidInput = FALSE;

  NVDIMM_ENTRY();

  if (pConfirmation == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = PromptedInput(PROMPT_CONTINUE_QUESTION, TRUE, TRUE, &pPromptReply);
  if ((NULL == pPromptReply) || (EFI_ERROR(ReturnCode))) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ValidInput = StrLen(pPromptReply) == 1 &&
    (StrICmp(pPromptReply, L"y") == 0 || StrICmp(pPromptReply, L"n") == 0);
  if (EFI_ERROR(ReturnCode) || !ValidInput) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (StrICmp(pPromptReply, L"y") == 0) {
    *pConfirmation = TRUE;
  }
  else {
    *pConfirmation = FALSE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPromptReply);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif
/**
  Read input from console
  @param[in] ShowInput - Show characters written by user
  @param[in] OnlyAlphanumeric - Allow only for alphanumeric characters
  @param[in, out] ppReturnValue - is a pointer to a pointer to the 16-bit character
        string without null-terminator that will contain the return value
  @param[in, out] pBufferSize - is a pointer to the Size in bytes of the return buffer

  @retval - Appropriate CLI return code
**/
EFI_STATUS
ConsoleInput(
  IN     BOOLEAN ShowInput,
  IN     BOOLEAN OnlyAlphanumeric,
  IN OUT CHAR16 **ppReturnValue,
  IN OUT UINTN *pBufferSize OPTIONAL
)
{
  EFI_INPUT_KEY Key = { 0 };
  UINTN SizeInBytes = 0;
  CHAR16 *pBuffer = NULL;
  UINTN EventIndex = 0;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (ppReturnValue == NULL) {
    goto Finish;
  }

  while (1) {
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    ReturnCode = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(ReturnCode)) {
      Print(L"Error reading key strokes.\n");
      goto Finish;
    }

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      if (pBuffer == NULL || StrLen(pBuffer) == 0 || SizeInBytes <= 0) {
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      else {
        *ppReturnValue = pBuffer;
        if (pBufferSize != NULL) {
          *pBufferSize = SizeInBytes;
        }
        break;
      }
    }

    if ((SizeInBytes != 0 && pBuffer == NULL) ||
      (SizeInBytes == 0 && pBuffer != NULL)) {
      ReturnCode = EFI_BAD_BUFFER_SIZE;
      goto Finish;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (pBuffer != NULL && StrLen(pBuffer) > 0) {
        pBuffer[StrLen(pBuffer) - 1] = L'\0';
        if (ShowInput) {
          Print(L"%c", Key.UnicodeChar);
        }
      }
    }
    else {
      if (!OnlyAlphanumeric || IsUnicodeAlnumCharacter(Key.UnicodeChar)) {
        StrnCatGrow(&pBuffer, &SizeInBytes, &Key.UnicodeChar, 1);
        if (NULL == pBuffer) {
           Print(L"Failure inputing characters.\n");
           break;
        }
        if (ShowInput) {
          Print(L"%c", Key.UnicodeChar);
        }
      }
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check all DIMMs if SKU conflict occurred.

  @param[out] pSkuMixedMode is a pointer to a BOOLEAN value that will
    represent the presence of SKU mixed mode

  @retval EFI_INVALID_PARAMETER Input parameter was NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
IsSkuMixed(
  OUT BOOLEAN *pSkuMixedMode
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 DimmCount = 0;
  UINT32 Index = 0;
  DIMM_INFO *pDimmsInformation = NULL;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DIMM_INFO *pFirstManageableDimmInfo = NULL;

  NVDIMM_ENTRY();

  if (pSkuMixedMode == NULL) {
    goto Finish;
  }
  *pSkuMixedMode = FALSE;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID**)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pDimmsInformation = AllocateZeroPool(DimmCount * sizeof(*pDimmsInformation));
  if (pDimmsInformation == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount, DIMM_INFO_CATEGORY_NONE, pDimmsInformation);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; Index++) {
    if (pDimmsInformation[Index].ManageabilityState == MANAGEMENT_VALID_CONFIG) {
      pFirstManageableDimmInfo = &(pDimmsInformation[Index]);
      break;
    }
  }

  while (++Index < DimmCount) {
    if (pDimmsInformation[Index].ManageabilityState == MANAGEMENT_VALID_CONFIG) {
      ReturnCode = IsSkuModeMismatch(pFirstManageableDimmInfo, &(pDimmsInformation[Index]), pSkuMixedMode);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      if (*pSkuMixedMode == TRUE) {
        break;
      }
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimmsInformation);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Print Load Firmware progress for all DIMMs

  @param[in] ProgressEvent EFI Event
  @param[in] pContext context pointer
**/
VOID
EFIAPI
PrintProgress(
  IN     EFI_EVENT ProgressEvent,
  IN     VOID *pContext
)
{
  OBJECT_STATUS *pObjectStatus = NULL;
  LIST_ENTRY *pObjectStatusNode = NULL;
  STATIC UINT32 LastObjectId = 0;
  COMMAND_STATUS *pCommandStatus = NULL;

  /**
     For reuse of this function one should do one of two things:
     1) Add string pointer to COMMAND_STATUS and pass it to Print instead of current define
     2) Use some other structure instead of COMMAND_STATUS
  **/

  if (pContext == NULL) {
    goto Finish;
  }

  pCommandStatus = (COMMAND_STATUS*)pContext;
  BOOLEAN
    EFIAPI
    IsListEmpty(
      IN      CONST LIST_ENTRY          *ListHead
    );

  if (!IsListInitialized(pCommandStatus->ObjectStatusList) && !IsListEmpty(&pCommandStatus->ObjectStatusList)) {
    goto Finish;
  }

  LIST_FOR_EACH(pObjectStatusNode, &pCommandStatus->ObjectStatusList) {
    pObjectStatus = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);
    if (IsSetNvmStatus(pObjectStatus, NVM_OPERATION_IN_PROGRESS)) {
      if (LastObjectId == 0) {
        LastObjectId = pObjectStatus->ObjectId;
      }
      else if (LastObjectId != pObjectStatus->ObjectId) {
        Print(L"\n");
        LastObjectId = pObjectStatus->ObjectId;
      }

      Print(CLI_PROGRESS_STR, pObjectStatus->ObjectId, pObjectStatus->Progress);
      break;
    }
  }

Finish:
  return;
}

/**
  Get relative path from absolute path.
  Output pointer points to the same string as input but with necessary offset. Caller shall not free it.

  @param[in] pAbsolutePath Absolute path
  @param[out] ppRelativePath Relative path

  @retval EFI_INVALID_PARAMETER Input parameter was NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
GetRelativePath(
  IN     CHAR16 *pAbsolutePath,
  OUT CHAR16 **ppRelativePath
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (pAbsolutePath == NULL) {
    goto Finish;
  }

  *ppRelativePath = pAbsolutePath;

  if (ContainsCharacter(':', *ppRelativePath)) {
    while (*ppRelativePath[0] != '\\' && *ppRelativePath[0] != '\0') {
      (*ppRelativePath)++;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}

/**
  Check if all dimms in the specified pDimmIds list are manageable.
  This helper method assumes all the dimms in the list exist.
  This helper method also assumes the parameters are non-null.

  @param[in] pDimmInfo The dimm list found in NFIT.
  @param[in] DimmCount Size of the pDimmInfo array.
  @param[in] pDimmIds Pointer to the array of DimmIDs to check.
  @param[in] pDimmIdsCount Size of the pDimmIds array.

  @retval TRUE if all Dimms in pDimmIds list are manageable
  @retval FALSE if at least one DIMM is not manageable
**/
BOOLEAN
AllDimmsInListAreManageable(
  IN     DIMM_INFO *pAllDimms,
  IN     UINT32 AllDimmCount,
  IN     UINT16 *pDimmsListToCheck,
  IN     UINT32 DimmsToCheckCount
)
{
  BOOLEAN Manageable = TRUE;
  UINT32 AllDimmListIndex = 0;
  UINT32 DimmsToCheckIndex = 0;

  for (DimmsToCheckIndex = 0; DimmsToCheckIndex < DimmsToCheckCount; DimmsToCheckIndex++) {
    for (AllDimmListIndex = 0; AllDimmListIndex < AllDimmCount; AllDimmListIndex++) {
      if (pAllDimms[AllDimmListIndex].DimmID == pDimmsListToCheck[DimmsToCheckIndex]) {
        if (pAllDimms[AllDimmListIndex].ManageabilityState != MANAGEMENT_VALID_CONFIG) {
          Manageable = FALSE;
          break;
        }
      }
    }
  }

  NVDIMM_EXIT();
  return Manageable;
}

/**
  Retrieve the User Cli Display Preferences from RunTime Services.

  @param[out] pDisplayPreferences pointer to the current driver preferences.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
ReadRunTimeCliDisplayPreferences(
  IN  OUT DISPLAY_PREFERENCES *pDisplayPreferences
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN VariableSize = 0;
  NVDIMM_ENTRY();

  if (pDisplayPreferences == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ZeroMem(pDisplayPreferences, sizeof(*pDisplayPreferences));

  VariableSize = sizeof(pDisplayPreferences->DimmIdentifier);
  ReturnCode = GET_VARIABLE(
    DISPLAY_DIMM_ID_VARIABLE_NAME,
    gNvmDimmCliVariableGuid,
    &VariableSize,
    &pDisplayPreferences->DimmIdentifier);

  if (ReturnCode == EFI_NOT_FOUND) {
    pDisplayPreferences->DimmIdentifier = DISPLAY_DIMM_ID_DEFAULT;
    ReturnCode = EFI_SUCCESS;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve DimmID Display Variable");
    goto Finish;
  }

  VariableSize = sizeof(pDisplayPreferences->SizeUnit);
  ReturnCode = GET_VARIABLE(
    DISPLAY_SIZE_VARIABLE_NAME,
    gNvmDimmCliVariableGuid,
    &VariableSize,
    &pDisplayPreferences->SizeUnit);

  if (ReturnCode == EFI_NOT_FOUND) {
    pDisplayPreferences->SizeUnit = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
    ReturnCode = EFI_SUCCESS;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve Size Display Variable");
    goto Finish;
  }

  if (pDisplayPreferences->SizeUnit >= DISPLAY_SIZE_MAX_SIZE ||
    pDisplayPreferences->DimmIdentifier >= DISPLAY_DIMM_ID_MAX_SIZE) {
    NVDIMM_DBG("Parameters retrieved from RT services are invalid, setting defaults");
    pDisplayPreferences->SizeUnit = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
    pDisplayPreferences->DimmIdentifier = DISPLAY_DIMM_ID_DEFAULT;
    goto Finish;
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Retrieve the User Cli Display Preferences CMD line arguements.

@param[out] pDisplayPreferences pointer to the current driver preferences.

@retval EFI_INVALID_PARAMETER One or more parameters are invalid
@retval EFI_SUCCESS All ok
**/
EFI_STATUS
ReadCmdLineShowOptions(
  IN OUT SHOW_FORMAT_TYPE *pFormatType,
  IN struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *OutputOptions = NULL;
  CHAR16 **Toks = NULL;
  UINT32 NumToks = 0;
  UINT32 Index = 0;

  if (NULL == pFormatType) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == (OutputOptions = getOptionValue(pCmd, OUTPUT_OPTION_SHORT))) {
    if (NULL == (OutputOptions = getOptionValue(pCmd, OUTPUT_OPTION))) {
      *pFormatType = TEXT;
      SET_FORMAT_LIST_FLAG(pCmd->pShowCtx);
      return ReturnCode;
    }
  }

  *pFormatType = TEXT; //default

  if (NULL != (Toks = StrSplit(OutputOptions, L',', &NumToks))) {
    for (Index = 0; Index < NumToks; ++Index) {
      if (0 == StrICmp(Toks[Index], OUTPUT_OPTION_VERBOSE)) {
        SET_FORMAT_VERBOSE_FLAG(pCmd->pShowCtx);
      }
      else if (0 == StrICmp(Toks[Index], OUTPUT_OPTION_TEXT)) {
        *pFormatType = TEXT;
      }
      else if (0 == StrICmp(Toks[Index], OUTPUT_OPTION_NVMXML)) {
        *pFormatType = XML;
      }
      else if (0 == StrICmp(Toks[Index], OUTPUT_OPTION_ESX_XML)) {
        *pFormatType = XML;
        SET_FORMAT_ESX_KV_FLAG(pCmd->pShowCtx);
      }
      else if (0 == StrICmp(Toks[Index], OUTPUT_OPTION_ESX_TABLE_XML)) {
        *pFormatType = XML;
        SET_FORMAT_ESX_CUSTOM_FLAG(pCmd->pShowCtx);
      }
      else {
        // Print out syntax specific help message for invalid -output option
        CHAR16 * pHelpStr = getCommandHelp(pCmd, TRUE);
        CHAR16 *pSyntaxTokStr = CatSPrint(NULL, CLI_PARSER_ERR_UNEXPECTED_TOKEN, Toks[Index]);
        if (NULL != pHelpStr) {
          CHAR16 *pSyntaxHelp = CatSPrintClean(pSyntaxTokStr, FORMAT_NL_STR FORMAT_NL_STR, CLI_PARSER_DID_YOU_MEAN, pHelpStr);
          LongPrint(pSyntaxHelp);
          FREE_POOL_SAFE(pSyntaxHelp);
        }
        else
        {
          // in case the command is bad, try to print something helpful.
          LongPrint(pSyntaxTokStr);
          FREE_POOL_SAFE(pSyntaxTokStr);
        }
        FREE_POOL_SAFE(pHelpStr);
        ReturnCode = EFI_INVALID_PARAMETER;
      }
    }
  }

  FreeStringArray(Toks, NumToks);
  return ReturnCode;
}


/**
   Get Dimm identifier preference

   @param[out] pDimmIdentifier Variable to store Dimm identerfier preference

   @retval EFI_SUCCESS Success
   @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
GetDimmIdentifierPreference(
  OUT UINT8 *pDimmIdentifier
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DISPLAY_PREFERENCES DisplayPreferences;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pDimmIdentifier == NULL) {
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }

  *pDimmIdentifier = DisplayPreferences.DimmIdentifier;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get Dimm identifier as string based on user preference

  @param[in] DimmId Dimm ID as number
  @param[in] pDimmUid Dimmm UID as string
  @param[out] pResultString String representation of preferred value
  @param[in] ResultStringLen Length of pResultString

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
GetPreferredDimmIdAsString(
  IN     UINT32 DimmId,
  IN     CHAR16 *pDimmUid OPTIONAL,
  OUT CHAR16 *pResultString,
  IN     UINT32 ResultStringLen
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT8 DimmIdentifier = 0;

  NVDIMM_ENTRY();

  if (pResultString == NULL) {
    goto Finish;
  }
  ReturnCode = GetDimmIdentifierPreference(&DimmIdentifier);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  ReturnCode = GetPreferredValueAsString(DimmId, pDimmUid, DimmIdentifier == DISPLAY_DIMM_ID_HANDLE,
    pResultString, ResultStringLen);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve Display DimmID Runtime Index from Property String

  @param[in] String to try to discover index for

  @retval DimmID Index of DimmID property string
  @retval Size of Array if not found
**/
UINT8 GetDimmIDIndex(
  IN  CHAR16 *pDimmIDStr
)
{
  UINT8 Index = 0;
  for (Index = 0; Index < DISPLAY_DIMM_ID_MAX_SIZE; Index++) {
    if (StrICmp(pDimmIDStr, mpDefaultDimmIds[Index]) == 0) {
      break;
    }
  }

  return Index;
}

/**
  Retrieve Display Size Runtime Index from Property String

  @param[in] String to try to discover index for

  @retval Display Size Index of Size property string
  @retval Size of Array if not found
**/
UINT8 GetDisplaySizeIndex(
  IN  CHAR16 *pSizeStr
)
{
  UINT8 Index = 0;
  for (Index = 0; Index < DISPLAY_SIZE_MAX_SIZE; Index++) {
    if (StrICmp(pSizeStr, mpDefaultSizeStrs[Index]) == 0) {
      break;
    }
  }

  return Index;
}

/**
  Retrieve Display DimmID String from RunTime variable index

  @param[in] Index to retrieve

  @retval NULL Index was invalid
  @retval DimmID String of user display preference
**/
CONST CHAR16 *GetDimmIDStr(
  IN  UINT8 DimmIDIndex
)
{
  if (DimmIDIndex >= DISPLAY_DIMM_ID_MAX_SIZE) {
    return NULL;
  }
  return mpDefaultDimmIds[DimmIDIndex];
}

/**
  Retrieve Display Size String from RunTime variable index

  @param[in] Index to retrieve

  @retval NULL Index was invalid
  @retval Size String of user display preference
**/
CONST CHAR16 *GetDisplaySizeStr(
  IN  UINT8 DisplaySizeIndex
)
{
  if (DisplaySizeIndex >= DISPLAY_SIZE_MAX_SIZE) {
    return NULL;
  }
  return mpDefaultSizeStrs[DisplaySizeIndex];
}

/**
Allocate and return string which is related with the binary RegionType value.
The caller function is obligated to free memory of the returned string.

@param[in] RegionType - region type

@retval - output string
**/
CHAR16 *
RegionTypeToString(
  IN     UINT8 RegionType
)
{
  CHAR16 *pRegionTypeString = NULL;

  if ((RegionType & PM_TYPE_AD) != 0) {
    pRegionTypeString = CatSPrintClean(pRegionTypeString, FORMAT_STR, PERSISTENT_MEM_TYPE_AD_STR);
  }

  if ((RegionType & PM_TYPE_AD_NI) != 0) {
    pRegionTypeString = CatSPrintClean(pRegionTypeString, FORMAT_STR  FORMAT_STR,
      pRegionTypeString == NULL ? L"" : L", ", PERSISTENT_MEM_TYPE_AD_NI_STR);
  }

  return pRegionTypeString;
}

/**
  Gets the DIMM handle corresponding to Dimm PID and also the index

  @param[in] DimmId - DIMM ID
  @param[in] pDimms - List of DIMMs
  @param[in] DimmsNum - Number of DIMMs
  @param[out] pDimmHandle - The Dimm Handle corresponding to the DIMM ID
  @param[out] pDimmIndex - The Index of the found DIMM

  @retval - EFI_STATUS Success
  @retval - EFI_INVALID_PARAMETER Invalid parameter
  @retval - EFI_NOT_FOUND Dimm not found
**/
EFI_STATUS
GetDimmHandleByPid(
  IN     UINT16 DimmId,
  IN     DIMM_INFO *pDimms,
  IN     UINT32 DimmsNum,
  OUT UINT32 *pDimmHandle,
  OUT UINT32 *pDimmIndex
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM_INFO *pFoundDimm = NULL;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmHandle == NULL || pDimmIndex == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (pDimms[Index].DimmID == DimmId) {
      pFoundDimm = &pDimms[Index];
      *pDimmIndex = Index;
      break;
    }
  }

  if (pFoundDimm == NULL) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  *pDimmHandle = pFoundDimm->DimmHandle;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
