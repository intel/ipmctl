/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include "Pfn.h"
#include "PfnLayout.h"
#include "Namespace.h"

GUID gPfnAbstractionGuid = EFI_PFN_ABSTRACTION_GUID;

/**
  Prepare a pfn namespace for use

  @param [in] RawSize Size of pfn namespace being created
  @param [in] LbaSize Size of a block in a created namespace
  @param [in] ParentUuid[] UUID label of the namespace
  @param [in out] pNamespace pointer to pfn namespace

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Null input
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
PfnInit(
  IN     UINT64 RawSize,
  IN     UINT32 LbaSize,
  IN     GUID *pParentUuid,
  IN OUT VOID *pNamespace
  )
{
  PFN_INFO *pPfnInfo = NULL;
  PFN *pPfn = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (pParentUuid == NULL || pNamespace == NULL) {
    goto FinishWithError;
  }

  pPfn = AllocateZeroPool(sizeof(PFN));
  if (pPfn == NULL) {
    NVDIMM_DBG("Memory allocation for %x bytes failed", sizeof(PFN));
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto FinishWithError;
  }

  pPfnInfo = AllocateZeroPool(sizeof(PFN_INFO));
  if (pPfnInfo == NULL) {
    NVDIMM_DBG("Memory allocation for %x bytes failed", sizeof(PFN_INFO));
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto FinishWithError;
  }

  CopyMem(&pPfn->ParentUuid, pParentUuid, sizeof(GUID));
  pPfn->RawSize = RawSize;
  pPfn->LbaSize = LbaSize;
  pPfn->pNamespace = pNamespace;

  ReturnCode = ReadNamespaceBytes(pNamespace, PFN_INFO_BLOCK_OFFSET, pPfnInfo, sizeof(PFN_INFO));
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to read namespace bytes");
    goto FinishWithError;
  } else {
    pPfn->DataOff = pPfnInfo->DataOff;
    pPfn->StartPad = pPfnInfo->StartPad;
    ((NAMESPACE *) pNamespace)->pPfn = pPfn;

    // Set blockcount to usable size, excluding metadata
    ((NAMESPACE *) pNamespace)->UsableSize = ((NAMESPACE *) pNamespace)->BlockCount -
      pPfnInfo->StartPad - pPfnInfo->EndTrunc - pPfnInfo->DataOff;

    goto Finish;
  }

FinishWithError:
  FREE_POOL_SAFE(pPfn);
  pPfn = NULL;
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pPfnInfo);
  return ReturnCode;
}

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
  )
{
  UINT64 Offset = 0;
  NAMESPACE *pNamespace = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  pNamespace = pPfn->pNamespace;

  Offset = pPfn->StartPad + pPfn->DataOff + (LBA * pNamespace->Media.BlockSize);

  ReturnCode = ReadNamespaceBytes(pPfn->pNamespace, Offset, pBuffer, pPfn->LbaSize);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Pfn read failed %d", ReturnCode);
    return ReturnCode;
  }

  return EFI_SUCCESS;
}

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
  )
{
  UINT64 Offset = 0;
  NAMESPACE *pNamespace = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  pNamespace = pPfn->pNamespace;

  Offset = pPfn->StartPad + pPfn->DataOff + (LBA * pNamespace->Media.BlockSize);

  ReturnCode = WriteNamespaceBytes(pPfn->pNamespace, Offset, pBuffer, pPfn->LbaSize);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Pfn write failed %d", ReturnCode);
    return ReturnCode;
  }

  return EFI_SUCCESS;
}

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
  IN     PFN      *pPfn OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pInfo == NULL) {
    goto Finish;
  }

  if (CompareMem(pInfo->Sig, PfnSig, PFNINFO_SIG_LEN) != 0) {
    NVDIMM_DBG("Invalid PFN signature ");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pPfn != NULL) {
    if (CompareMem(&pInfo->ParentUuid, &pPfn->ParentUuid, sizeof(GUID)) != 0) {
      NVDIMM_DBG("parent UUID mismatch");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  /* to be valid, the fields must Checksum correctly */
  if (!ChecksumOperations((VOID *) pInfo, sizeof(PFN_INFO), &pInfo->Checksum, FALSE)) {
    NVDIMM_DBG("Invalid checksum");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  /* to be valid, Info block must have Major version of at least 1 */
  if (pInfo->Major == 0) {
    NVDIMM_DBG("Invalid major version(0)");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
