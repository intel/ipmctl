/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SmbiosUtility.h"
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Debug.h>

/**
  Retrieve Capacity for the given SMBIOS version.

  @param[in]  Size          Size field
  @param[in]  ExtendedSize  Extended size field
  @param[in]  SmbiosVersion The SMBIOS version
  @param[out] pCapacity     Pointer to the Capacity

  @retval EFI_SUCCESS Retrieval was successful
  @retval EFI_INVALID_PARAMETER Null parameter passed
**/
EFI_STATUS
GetSmbiosCapacity (
  IN     UINT16 Size,
  IN     UINT32 ExtendedSize,
  IN     SMBIOS_VERSION SmbiosVersion,
     OUT UINT64 *pCapacity
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 Capacity = 0;

  NVDIMM_ENTRY();

  if (pCapacity == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (SmbiosVersion.Major >= 2) {
    if (Size == MAX_UINT16) {
      *pCapacity = 0;
      goto Finish;
    }

    if (SmbiosVersion.Minor >= 7 || SmbiosVersion.Major == 3) {
      if (Size == MAX_INT16) {
        *pCapacity = MIB_TO_BYTES(ExtendedSize);
        goto Finish;
      }
    }

    Capacity = Size & MAX_UINT16;
    if (Size & BIT15) {
      *pCapacity = KIB_TO_BYTES(Capacity);
    } else {
      *pCapacity = MIB_TO_BYTES(Capacity);
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve SMBIOS string for the given string number.

  It is the caller's responsibility to use FreePool() to free the allocated buffer (ppSmbiosString).

  @param[in]  pSmbios         Pointer to SMBIOS structure.
  @param[in]  StringNumber    String number to return.
  @param[out] pSmbiosString   Pointer to a char buffer to where SMBIOS string will be copied.
  @param[in]  BufferLen       pSmbiosString buffer length

  @retval EFI_SUCCESS String retrieved successfully
  @retval EFI_INVALID_PARAMETER
  @retval EFI_NOT_FOUND
**/
EFI_STATUS
GetSmbiosString (
  IN     SMBIOS_STRUCTURE_POINTER *pSmbios,
  IN     UINT16                    StringNumber,
     OUT CHAR16                    *pSmbiosString,
  IN     UINT16                    BufferLen
  )
{
  UINT16  Index;
  CHAR8   *pString = NULL;
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;

  NVDIMM_ENTRY();

  if (!pSmbios || !pSmbiosString) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  if (BufferLen == 0 && StringNumber != -1) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Skip over formatted section **/
  pString = (CHAR8 *) (pSmbios->Raw + pSmbios->Hdr->Length);

  /** Look through unformatted section **/
  for (Index = 1; Index <= StringNumber; Index++) {
    if (StringNumber == Index) {
      if (AsciiStrLen(pString) > (BufferLen * sizeof(CHAR16))) {
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      AsciiStrToUnicodeStrS(pString, pSmbiosString, BufferLen);
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }

    /** Skip string **/
    for (; *pString != 0; pString++);

    pString++;

    if (*pString == 0) {
      // If double NULL then we are done. String with given number not found.
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Fill SmBios structures for first and last entry

  @param[out] pSmBiosStruct - pointer for first SmBios entry
  @param[out] pLastSmBiosStruct - pointer for last SmBios entry
**/
#ifndef OS_BUILD
VOID
GetFirstAndBoundSmBiosStructPointer(
     OUT SMBIOS_STRUCTURE_POINTER *pSmBiosStruct,
     OUT SMBIOS_STRUCTURE_POINTER *pLastSmBiosStruct,
     OUT SMBIOS_VERSION *pSmbiosVersion
  )
{
  SMBIOS_TABLE_ENTRY_POINT   *pTableEntry = NULL;
  SMBIOS_TABLE_ENTRY_POINT   *pTempTableEntry = NULL;
  SMBIOS_TABLE_ENTRY_POINT_3 *pTableEntry3 = NULL;
  UINT32 Index = 0;
  UINT8 MinorVersion = 0;

  if (pSmBiosStruct == NULL || pLastSmBiosStruct == NULL || pSmbiosVersion == NULL) {
    return;
  }

  /**
    get the smbios tables from the system configuration table
  **/
  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (CompareGuid(&gEfiSmbios3TableGuid, &(gST->ConfigurationTable[Index].VendorGuid))) {
      // Priority given to SMBIOS 3 - break out
      pTableEntry3 = (SMBIOS_TABLE_ENTRY_POINT_3 *) gST->ConfigurationTable[Index].VendorTable;
      break;
    }
    if (CompareGuid(&gEfiSmbiosTableGuid, &(gST->ConfigurationTable[Index].VendorGuid))) {
      // Even if we find this GUID, let's keep searching in case we find SMBIOS 3
      pTempTableEntry = (SMBIOS_TABLE_ENTRY_POINT *) gST->ConfigurationTable[Index].VendorTable;
      if (MinorVersion < pTempTableEntry->MinorVersion) {
        // Preference given to a higher 2.x version
        pTableEntry = pTempTableEntry;
        MinorVersion = pTableEntry->MinorVersion;
      }
    }
  }

  if (pTableEntry3 != NULL) {
    pSmBiosStruct->Raw  = (UINT8 *) (UINTN) (pTableEntry3->TableAddress);
    pLastSmBiosStruct->Raw = pSmBiosStruct->Raw + pTableEntry3->TableMaxSize;
    pSmbiosVersion->Major = pTableEntry3->MajorVersion;
    pSmbiosVersion->Minor = pTableEntry3->MinorVersion;
  } else if (pTableEntry != NULL) {
    pSmBiosStruct->Raw  = (UINT8 *) (UINTN) (pTableEntry->TableAddress);
    pLastSmBiosStruct->Raw = pSmBiosStruct->Raw + pTableEntry->TableLength;
    pSmbiosVersion->Major = pTableEntry->MajorVersion;
    pSmbiosVersion->Minor = pTableEntry->MinorVersion;
  }
}
#endif
/**
  Move current pTable pointer to the next SMBIOS structure.

  @param[in,out] pSmbios Pointer to SMBIOS structure.

  @retval EFI_SUCCESS Structure successfully updated
  @retval EFI_INVALID_PARAMETER
  @retval EFI_NOT_FOUND
**/
EFI_STATUS
GetNextSmbiosStruct(
  IN OUT SMBIOS_STRUCTURE_POINTER *pTable
  )
{
  UINT8 Index = 0;
  CHAR8 *pSmbiosStr = NULL;
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;

  if (pTable == NULL || pTable->Hdr == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Skip over formatted section **/
  pSmbiosStr = (CHAR8 *) (pTable->Raw + pTable->Hdr->Length);

  for (Index = 1; Index <= MAX_UINT8; Index++) {
    /** Skip string **/
    for (; *pSmbiosStr != '\0'; pSmbiosStr++);
    pSmbiosStr++;

    if (*pSmbiosStr == 0) {
      pTable->Raw = (UINT8 *) ++pSmbiosStr;
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }
  }

Finish:
  return ReturnCode;
}
