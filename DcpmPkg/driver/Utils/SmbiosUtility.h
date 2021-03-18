/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SMBIOSUTILITY_H_
#define _SMBIOSUTILITY_H_

#include <IndustryStandard/SmBios.h>
#include <Guid/SmBios.h>

#define SMBIOS_STRING_INVALID 0

typedef struct {
  UINT8   AnchorString[5];
  UINT8   EntryPointStructureChecksum;
  UINT8   EntryPointLength;
  UINT8   MajorVersion;
  UINT8   MinorVersion;
  UINT8   Docrev;
  UINT8   EntryPointRevision;
  UINT8   Reserved;
  UINT32  TableMaxSize;
  UINT64  TableAddress;
} SMBIOS_TABLE_ENTRY_POINT_3;

typedef struct {
  UINT8 Major;
  UINT8 Minor;
} SMBIOS_VERSION;

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
  );

/**
  Retrieve SMBIOS string for the given string number.

  It is the caller's responsibility to use FreePool() to free the allocated buffer (ppSmbiosString).

  @param[in]  pSmbios         Pointer to SMBIOS structure.
  @param[in]  StringNumber    String number to return.
  @param[out] pSmbiosString   Pointer to a char buffer to where SMBIOS string will be copied.
                              Buffer must be allocated.
  @param[in]  BufferLen       pSmbiosString buffer length

  @retval EFI_SUCCESS String retrieved successfully
  @retval EFI_INVALID_PARAMETER
  @retval EFI_NOT_FOUND
**/
EFI_STATUS
GetSmbiosString (
  IN     SMBIOS_STRUCTURE_POINTER *pSmbios,
  IN     UINT16                    StringNumber,
     OUT CHAR16                    *ppSmbiosString,
  IN     UINT16                    BufferLen
  );

/**
  Move current pTable pointer to the next SMBIOS structure.

  @param[in,out] pSmbios Pointer to SMBIOS structure.

  @retval EFI_SUCCESS Structure successfully updated
  @retval EFI_INVALID_PARAMETER
  @retval EFI_NOT_FOUND
**/
EFI_STATUS
GetNextSmbiosStruct (
  IN OUT SMBIOS_STRUCTURE_POINTER *pTable
  );

/**
  Fill SmBios structures for first and bound entry

  @param[out] pSmBiosStruct - pointer for first SmBios entry
  @param[out] pBoundSmBiosStruct - pointer for nonexistent (one after last) SmBios entry
**/
EFI_STATUS
GetFirstAndBoundSmBiosStructPointer(
  OUT SMBIOS_STRUCTURE_POINTER *pSmBiosStruct,
  OUT SMBIOS_STRUCTURE_POINTER *pLastSmBiosStruct,
  OUT SMBIOS_VERSION *pSmbiosVersion
);


#endif /* _SMBIOSUTILITY_H_ */
