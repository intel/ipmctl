/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PFN_H_
#define _PFN_H_

#include "PfnLayout.h"

extern GUID gPfnAbstractionGuid;

#define EFI_PFN_ABSTRACTION_GUID \
  { 0x266400BA, 0xFB9F, 0x4677, {0xBC, 0xB0, 0x96, 0x8F, 0x11, 0xD0, 0xD2, 0x25} }

typedef struct _PFN {
  /**
    UUID of the PFN
  **/
  GUID Uuid;
  /**
    UUID of the containing namespace, used to validate PFN metadata.
  **/
  GUID ParentUuid;

  UINT64 RawSize;         //!< Size of containing namespace
  UINT32 LbaSize;         //!< External LBA size

  UINT32 StartPad;        //!< Padding to align the capacity to a Linux "section" boundary (128MB)
  UINT64 DataOff;         //!< Data offset relative to namespace_base + start pad

  VOID *pNamespace; // The pointer to the containing namespace for the IO operations
} PFN;

/**
    Signature for PFN info block.  Total size is 16 bytes, including
    the '\0' added to the string by the declaration (the last two bytes
    of the string are '\0').
**/
static const char PfnSig[] = "NVDIMM_PFN_INFO\0";

/**
  Prepare a pfn namespace for use

  @param [in] RawSize Size of pfn namespace being created
  @param [in] LbaSize Size of a block in a created namespace
  @param [in] ParentUuid[] UUID label of the namespace
  @param [in] pNamespace pointer to the PFNs parent namespace
  @param [out] pPfn pointer to pfn struct to initialize

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Null input
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
PfnInit(
  IN     UINT64 RawSize,
  IN     UINT32 LbaSize,
  IN     GUID *pParentUuid,
  IN     VOID *pNamespace
  );

/**
  Read a block from a pfn namespace

  @param [in] pPfn namespace handle
  @param [in] Lba Logical block address to be read
  @param [out] pBuffer Read result Buffer pointer

  @retval EFI_SUCCESS if the routine succeeds
**/
EFI_STATUS
PfnRead(
  IN     PFN *pPfn,
  IN     UINT64 LBA,
     OUT VOID *pBuffer
  );

/**
  Write a block to a pfn namespace

  @param [in] pPfn namespace handle
  @param [in] Lba Logical block address to be written
  @param [out] pBuffer Buffer pointer to the block to be written

  @retval EFI_SUCCESS if the routine succeeds
**/
EFI_STATUS
PfnWrite(
  IN     PFN *pPfn,
  IN     UINT64 LBA,
  IN     VOID *pBuffer
  );

/**
  Validates pfn info block

  @retval EFI_SUCCESS if the routine succeeds
  @retval EFI_ABORTED invalid pfn info block

  @param [in] pInfo Info block to be validated
  @param [in] pPfn PFN to be compared with existing metadata
**/
EFI_STATUS
PfnValidateInfo(
  IN     PFN_INFO *pInfo,
  IN     PFN      *pPFN
  );

#endif //_PFN_H_
