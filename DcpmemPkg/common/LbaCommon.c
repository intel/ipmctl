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

/**
   Print Namespace Index

   @param[in] pNamespaceIndex Namespace Index
**/
VOID
PrintNamespaceIndex(
  IN     NAMESPACE_INDEX *pNamespaceIndex
  )
{
  CHAR16 Buffer[NSINDEX_SIG_LEN + 1];

  if (pNamespaceIndex == NULL) {
    return;
  }

  ZeroMem(Buffer, sizeof(Buffer));

  Print(L"Signature   : " FORMAT_STR_NL, AsciiStrToUnicodeStrS((CHAR8 *) &pNamespaceIndex->Signature, Buffer, NSINDEX_SIG_LEN + 1));
  Print(L"Flags       : 0x%x\n", *pNamespaceIndex->Flags);
  Print(L"LabelSize   : 0x%x\n", pNamespaceIndex->LabelSize);
  Print(L"Sequence    : 0x%x\n", pNamespaceIndex->Sequence);
  Print(L"MyOffset    : 0x%llx\n", pNamespaceIndex->MyOffset);
  Print(L"MySize      : 0x%llx\n", pNamespaceIndex->MySize);
  Print(L"OtherOffset : 0x%llx\n", pNamespaceIndex->OtherOffset);
  Print(L"LabelOffset : 0x%llx\n", pNamespaceIndex->LabelOffset);
  Print(L"NumOfLabels : 0x%x\n", pNamespaceIndex->NumberOfLabels);
  Print(L"Major       : 0x%x\n", pNamespaceIndex->Major);
  Print(L"Minor       : 0x%x\n", pNamespaceIndex->Minor);
  Print(L"Checksum    : 0x%llx\n", pNamespaceIndex->Checksum);

  Print(L"Free:\n");
  HexPrint(pNamespaceIndex->pFree,
             LABELS_TO_FREE_BYTES(ROUNDUP(pNamespaceIndex->NumberOfLabels, 8)));

  Print(L"\n");
}

/**
   Print Namespace Label

   @param[in] pNamespaceLabel Namespace Label
**/
VOID
PrintNamespaceLabel(
  IN     NAMESPACE_LABEL *pNamespaceLabel
  )
{
  CHAR16 Buffer[NLABEL_NAME_LEN_WITH_TERMINATOR];

  if (pNamespaceLabel == NULL) {
    return;
  }

  ZeroMem(Buffer, sizeof(Buffer));

  Print(L"Uuid          : %g\n", pNamespaceLabel->Uuid);
  Print(L"Name          : " FORMAT_STR_NL, AsciiStrToUnicodeStrS((CHAR8 *) &pNamespaceLabel->Name, Buffer, NLABEL_NAME_LEN_WITH_TERMINATOR));
  Print(L"Flags         : 0x%x\n", pNamespaceLabel->Flags);
  Print(L"NumOfLabels   : 0x%x\n", pNamespaceLabel->NumberOfLabels);
  Print(L"Position      : 0x%x\n", pNamespaceLabel->Position);
  Print(L"ISetCookie    : 0x%llx\n", pNamespaceLabel->InterleaveSetCookie);
  Print(L"LbaSize       : 0x%llx\n", pNamespaceLabel->LbaSize);
  Print(L"Dpa           : 0x%llx\n", pNamespaceLabel->Dpa);
  Print(L"RawSize       : 0x%llx\n", pNamespaceLabel->RawSize);
  Print(L"Slot          : 0x%x\n", pNamespaceLabel->Slot);
  Print(L"Alignment     : 0x%x\n", pNamespaceLabel->Alignment);
  Print(L"TypeGuid      : %g\n", pNamespaceLabel->TypeGuid);
  Print(L"AddrAbstrGuid : %g\n", pNamespaceLabel->AddressAbstractionGuid);
  Print(L"Checksum      : 0x%llx\n", pNamespaceLabel->Checksum);
  Print(L"\n");
}

/**
   Print Label Storage Area and all subtables

   @param[in] pLba Label Storage Area
**/
VOID
PrintLabelStorageArea(
  IN     LABEL_STORAGE_AREA *pLba
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 Index = 0;
  UINT16 CurrentIndex = 0;
  UINT16 SlotStatus = SLOT_UNKNOWN;
  BOOLEAN First = FALSE;

  if (pLba == NULL) {
    return;
  }

  ReturnCode = GetLsaIndexes(pLba, &CurrentIndex, NULL);
  if (EFI_ERROR(ReturnCode)) {
    return;
  }

  Print(L"Label Storage Area - Current Index\n");

  PrintNamespaceIndex(&pLba->Index[CurrentIndex]);

  for (Index = 0, First = TRUE; Index < pLba->Index[CurrentIndex].NumberOfLabels; Index++) {
    CheckSlotStatus(&pLba->Index[CurrentIndex], Index, &SlotStatus);
    if (SlotStatus == SLOT_FREE) {
      continue;
    }

    if (First) {
      Print(L"Label Storage Area - Labels\n");
      First = FALSE;
    }

    PrintNamespaceLabel(&pLba->pLabels[Index]);
  }

  if (!First) {
    Print(L"\n");
  }
}

VOID FreeLsaSafe(
  IN    LABEL_STORAGE_AREA **ppLabelStorageArea
  )
{

  UINT32 Index = 0;
  if (*ppLabelStorageArea != NULL) {
    for (Index = 0; Index < NAMESPACE_INDEXES; Index++) {
      FREE_POOL_SAFE(((*ppLabelStorageArea)->Index)->pFree);
      FREE_POOL_SAFE(((*ppLabelStorageArea)->Index)->pReserved);
    }
    FREE_POOL_SAFE((*ppLabelStorageArea)->pLabels);
    FREE_POOL_SAFE(*ppLabelStorageArea);
  }
}
