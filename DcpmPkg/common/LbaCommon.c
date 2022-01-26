/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Convert.h>
#include "LbaCommon.h"

/**
  Get current Namespace Index Id.

  Only one of two Namespace Indexes is valid at time. This function checks sequence
  numbers of both Indexes and returns current Index Id.

  @param[in] pLabelStorageArea Pointer to a LSA structure
  @param[out] pCurrentIndex Current index position in LSA structure
  @param[out] pNextIndex Next index position to be updated in LSA structure

  @retval EFI_SUCCESS Current Index position found
  @retval EFI_INVALID_PARAMETER NULL pointer parameter provided
**/
EFI_STATUS
GetLsaIndexes(
  IN     LABEL_STORAGE_AREA *pLsa,
     OUT UINT16 *pCurrentIndex,
     OUT UINT16 *pNextIndex
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 FirstSeq = 0;
  UINT32 SecondSeq = 0;
  UINT32 SeqSum = 0;
  UINT32 CurrentSeq = 0;
  UINT32 OtherSeq = 0;
  NVDIMM_ENTRY();

  if (pLsa == NULL || (pCurrentIndex == NULL && pNextIndex == NULL)) {
    goto Finish;
  }

  FirstSeq = pLsa->Index[FIRST_INDEX_BLOCK].Sequence;
  SecondSeq = pLsa->Index[SECOND_INDEX_BLOCK].Sequence;
  SeqSum = FirstSeq + SecondSeq;
  if (SeqSum == 0 || SeqSum > 6) {
    //invalid sequence numbers
    NVDIMM_DBG("Invalid sequence numbers FirstSeq: %d, SecondSeq: %d", FirstSeq, SecondSeq);
    goto Finish;
  }

  if (SeqSum == 4) {
    //seq num case [3,1] [1,3] lower one is newer
    CurrentSeq = MIN(FirstSeq, SecondSeq);
    OtherSeq = MAX(FirstSeq, SecondSeq);
  } else {
    //seq num all other cases, higher one is newer/valid
    CurrentSeq = MAX(FirstSeq, SecondSeq);
    OtherSeq = MIN(FirstSeq, SecondSeq);
  }

  //if both sequences are the same, use the one with higher offset: SecondSeq
  if (CurrentSeq == SecondSeq) {
    if (pCurrentIndex != NULL) {
      *pCurrentIndex = SECOND_INDEX_BLOCK;
    }
    if (pNextIndex != NULL) {
      *pNextIndex = FIRST_INDEX_BLOCK;
    }
  } else {
    if (pCurrentIndex != NULL) {
      *pCurrentIndex = FIRST_INDEX_BLOCK;
    }
    if (pNextIndex != NULL) {
      *pNextIndex = SECOND_INDEX_BLOCK;
    }
  }

  NVDIMM_DBG("[current: pos=%d seq=%d] other: pos=%d, seq=%d",
      pCurrentIndex != NULL ? *pCurrentIndex : -1, CurrentSeq,
      pNextIndex != NULL ? *pNextIndex : -1, OtherSeq);
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Function checks Namespace slot status.

  @param[in] pIndex Index Block in which to update free status
  @param[in] SlotNumber Number of a slot on which to update status
  @param[out pSlotStatus Return value representing current status. This
    can be SLOT_FREE or SLOT_USED.

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
CheckSlotStatus(
  IN     NAMESPACE_INDEX *pIndex,
  IN     UINT16 SlotNumber,
     OUT UINT16 *pSlotStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  CONST UINT16 BitsInBlock = sizeof(UINT8) * 8;         // how many bits in a block
  CONST UINT16 BlockNumber = SlotNumber / BitsInBlock;  // subsequent block number in the bitmap
  CONST UINT8 BitNumber = (CONST UINT8)(SlotNumber % BitsInBlock);     // subsequent bit number in a block
  UINT8 BitValue = 0;

  if (pIndex == NULL || pSlotStatus == NULL) {
    goto Finish;
  }

  BitValue = pIndex->pFree[BlockNumber] & (1 << BitNumber);
  if (BitValue == 0) {
    *pSlotStatus = SLOT_USED;
  } else {
    *pSlotStatus = SLOT_FREE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}


VOID FreeLsaSafe(
  IN    LABEL_STORAGE_AREA **ppLabelStorageArea
  )
{
  UINT32 IndexIdx = 0;
  NAMESPACE_INDEX *pNamespaceIndex = NULL;

  if (ppLabelStorageArea != NULL && *ppLabelStorageArea != NULL) {
    for (IndexIdx = 0; IndexIdx < NAMESPACE_INDEXES; IndexIdx++) {
      pNamespaceIndex = &(((*ppLabelStorageArea)->Index)[IndexIdx]);
      FREE_POOL_SAFE(pNamespaceIndex->pFree);
      FREE_POOL_SAFE(pNamespaceIndex->pReserved);
    }
    FREE_POOL_SAFE((*ppLabelStorageArea)->pLabels);
    FREE_POOL_SAFE(*ppLabelStorageArea);
  }
}
