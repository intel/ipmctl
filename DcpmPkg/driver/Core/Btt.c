/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <NvmTypes.h>
#include "Btt.h"
#include "BttLayout.h"
#include "Namespace.h"
#include <Convert.h>
GUID gBttAbstractionGuid = EFI_BTT_ABSTRACTION_GUID;

/**
    Loads up a single flog pair

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in] pArena Pointer to the Arena from which the Flog Pair is to be read
    @param [in] FlogOffset Offset to the specific Flog entry
    @param [out] pFlogRuntime Result of Flog read
    @param [in] FlogNum Number of Flog to be read
**/
STATIC
EFI_STATUS
BttReadFlogPair(
  IN     BTT *pBtt,
  IN     ARENAS *pArena,
  IN     UINT64 FlogOffset,
     OUT FLOG_RUNTIME *pFlogRuntime,
  IN     UINT32 FlogNum
  );

/**
    Loads up all the flog Entries for an arena

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in,out] pArena Pointer to the Arena from which the Flog Pairs are to be read
**/
STATIC
EFI_STATUS
BttReadFlogs(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena
  );

/**
    Writes out an updated Flog entry

    The Flog Entries are not Checksummed.  Instead, increasing Sequence
    numbers are used to atomically switch the active Flog entry between
    the first and second struct btt_Flog in each slot.  In order for this
    to work, the Sequence number must be updated only after all the other
    fields in the Flog are updated.  So the writes to the Flog are broken
    into two writes, one for the first three fields (lba, OldMap, NewMap)
    and, only after those fields are known to be written durably, the
    second write for the Seq field is done.

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in,out] pArena Pointer to the Arena from which the Flog Pair is to be read
    @param [in] Lba Logical block address to be written
    @param [in] OldMap Previous map entry to be written
    @param [in] NewMap New map entry to be written
**/
STATIC
EFI_STATUS
BttFlogUpdate(
  IN     BTT *pBtt,
  IN     ARENAS *pArena,
  IN     UINT32 Lba,
  IN     UINT32 OldMap,
  IN     UINT32 NewMap
  );

/**
    Constructs a read tracking table for an arena

    The Rtt is big enough to hold an entry for each free block (NFree)

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in,out] pArena Pointer to the Arena with Rtt to be created
**/
STATIC
EFI_STATUS
BttBuildRtt(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena
  );

/**
    Loads up an arena and build run-time state

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in] ArenaOffset Offset to the Arena to be read
    @param [in,out] pArena Pointer to the Arena to be read
**/
STATIC
EFI_STATUS
BttReadArena(
  IN     BTT *pBtt,
  IN     UINT64 ArenaOffset,
  IN OUT ARENAS *pArena
  );

/**
    Loads up all Arenas and builds run-time state

    On entry, layout must be known to be valid, and the number of Arenas must be known.

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in] NArenas Number of arenas to be read
**/
STATIC
EFI_STATUS
BttReadArenas(
  IN     BTT *pBtt,
  IN     UINT32 NArenas
  );

/**
    Loads up layout Info from btt namespace

    Called once when the btt namespace is opened for use.
    Sets Bttp->Laidout to 0 if no valid layout is found, 1 otherwise.

    Any recovery actions required (as indicated by the Flog state) are
    performed by this routine.

    Any quick checks for layout consistency are performed by this routine
    (quick enough to be done each time a BTT area is opened for use, not
    like the slow consistency checks done by BttCheck()).

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
**/
STATIC
EFI_STATUS
BttReadLayout(
  IN     BTT *pBtt
  );

/*
    Satisfies a read with a block of zeros

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [out] Buffer Output buffer
**/
STATIC
EFI_STATUS
BttZeroBlock(
  IN     BTT *pBtt,
     OUT VOID *pBuffer
  );

/**
    Calculates the arena & pre-map LBA

    This routine takes the external LBA and matches it to the
    appropriate arena, adjusting the Lba for use within that arena.

    If successful, *pArena is a pointer to the appropriate arena struct in the run-time state,
    and *PreMapLba is the LBA adjusted to an arena-internal LBA (also known as the pre-map LBA).

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in] Lba External LBA
    @param [out] pArena Pointer to the appropriate arena struct in the run-time state
    @param [out] PreMapLba LBA adjusted to an arena-internal LBA
**/
STATIC
EFI_STATUS
BttLbaToArenaLba(
  IN     BTT *pBtt,
  IN     UINT64 Lba,
     OUT ARENAS **ppArena,
     OUT UINT32 *PreMapLba
  );

/**
  Performs a consistency check on an arena

  @retval EFI_SUCCESS if the routine succeeds

  @param [in] pBtt namespace handle
  @param [in] pArena Pointer to the Arena to be checked
**/
STATIC
EFI_STATUS
BttCheckArena(
  IN     BTT *pBtt,
  IN     ARENAS *pArena
  );

/**
    Verifies if Lba is invalid

    This function is used at the top of the entry points where an external
    LBA is provided, like this:

    if (!NT_SUCCESS (IsLbaValid(Bttp, Lba)))\n
        return STATUS_UNSUCCESSFUL;

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in] Lba Logical block address
**/
STATIC
EFI_STATUS
IsLbaValid(
  IN   BTT *pBtt,
  IN   UINT64 Lba
  );

BTT *
BttInit(
  IN     UINT64 RawSize,
  IN     UINT32 LbaSize,
  IN     GUID *pParentUuid,
  IN     VOID *pNamespace
  )
{
  BTT *pBtt = NULL;

  NVDIMM_DBG("RawSize=%x LbaSize=%d", RawSize, LbaSize);

  if(RawSize < BTT_NAMESPACE_MIN_SIZE) {
    NVDIMM_DBG("RawSize smaller than BTT_MIN_SIZE %d", BTT_NAMESPACE_MIN_SIZE);
    return NULL;
  }

  pBtt = AllocatePool(sizeof(BTT));
  if(pBtt == NULL) {
    NVDIMM_DBG("Memory allocation for %x bytes failed", sizeof(BTT));
    return NULL;
  }

  ZeroMem(pBtt, sizeof(BTT));

  CopyMem_S(&pBtt->ParentUuid, sizeof(pBtt->ParentUuid), pParentUuid, sizeof(GUID));
  pBtt->RawSize = RawSize;
  pBtt->LbaSize = LbaSize;
  pBtt->pNamespace = pNamespace;

  if ((((NAMESPACE *) pNamespace)->Major == NSINDEX_MAJOR) &&
      (((NAMESPACE *) pNamespace)->Minor == NSINDEX_MINOR_1)) {
    pBtt->PrimaryInfoOffset = BTT_PRIMARY_INFO_BLOCK_OFFSET_1_1;
  } else {
    pBtt->PrimaryInfoOffset = BTT_PRIMARY_INFO_BLOCK_OFFSET;
  }

  /**
    Load up layout, if it exists.

    Whether BttReadLayout() finds a valid layout or not, it finishes
    updating these layout-related fields:
    pBtt->NFree
    pBtt->NLbas
    pBtt->NArenas
    since these fields are used even before a valid layout it written.
  **/

  if(EFI_ERROR(BttReadLayout(pBtt))) {
    BttRelease(pBtt);  /* free up any allocations */
    return NULL;
  }
  // Set blockcount to usable size, excluding metadata
  ((NAMESPACE *) pNamespace)->UsableSize = pBtt->NLbas * pBtt->LbaSize;

  NVDIMM_DBG("Success, pBtt=%p", pBtt);
  return pBtt;
}

STATIC
EFI_STATUS
IsLbaValid(
  IN   BTT *pBtt,
  IN   UINT64 Lba
  )
{
  if (pBtt == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Lba >= pBtt->NLbas) {
    NVDIMM_DBG("lba out of range(NLbas %lu)", pBtt->NLbas);
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
BttReadInfo(
  IN     BTT_INFO *pInfo,
  IN     BTT      *pBtt OPTIONAL
  )
{
  if (pInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (CompareMem(pInfo->Sig, Sig, BTTINFO_SIG_LEN) != 0) {
    NVDIMM_DBG("Invalid BTT signature ");
    return EFI_ABORTED;
  }

  if (pBtt != NULL) {
    if (CompareMem(&pInfo->ParentUuid, &pBtt->ParentUuid, sizeof(GUID)) != 0){
      NVDIMM_DBG("parent UUID mismatch");
      return EFI_ABORTED;
    }
  }

  /* to be valid, the fields must Checksum correctly */
  if (!ChecksumOperations((VOID *)pInfo, sizeof(BTT_INFO), &pInfo->Checksum, FALSE)) {
    NVDIMM_DBG("Invalid checksum");
    return EFI_ABORTED;
  }

  /* to be valid, Info block must have Major version of at least 1 */
  if (pInfo->Major == 0) {
    NVDIMM_DBG("Invalid major version(0)");
    return EFI_ABORTED;
  }

  return EFI_SUCCESS;
}

STATIC
INLINE
BOOLEAN
MapEntryIsError(
  IN     UINT32 MapEntry
  )
{
  return (MapEntry & ~BTT_MAP_ENTRY_LBA_MASK) == BTT_MAP_ENTRY_ERROR;
}

STATIC
INLINE
BOOLEAN
MapEntryIsInitial(
  IN     UINT32 MapEntry
  )
{
  return (MapEntry & ~BTT_MAP_ENTRY_LBA_MASK) == 0;
}

STATIC
INLINE
BOOLEAN
MapEntryIsZeroOrInitial(
  IN     UINT32 MapEntry
  )
{
  UINT32 EntryFlags = MapEntry & ~BTT_MAP_ENTRY_LBA_MASK;
  return ((EntryFlags == 0) || (EntryFlags == BTT_MAP_ENTRY_ZERO));
}

STATIC
EFI_STATUS
BttReadFlogPair(
  IN     BTT *pBtt,
  IN     ARENAS *pArena,
  IN     UINT64 FlogOffset,
     OUT FLOG_RUNTIME *pFlogRuntime,
  IN     UINT32 FlogNum
  )
{
  BTT_FLOG_PAIR * pFlogPair = NULL;
  UINT8 CurrentFlogIndex = 0;
  UINT64 MapEntryOffset = 0;
  BTT_MAP_ENTRIES Entry;
  UINT8 CurrentMapPos = 0;
  UINT32 CurrentMap = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  pFlogPair = &(pFlogRuntime->FlogPair);

  SetMem(pFlogPair, sizeof(BTT_FLOG_PAIR), 0x0);
  SetMem(&Entry, sizeof(Entry), 0x0);

  NVDIMM_VERB("pBttp=%p pArena=%p FlogOffset=%x pFlogRuntime=%p FlogNum=%d",
    pBtt, pArena, FlogOffset, pFlogRuntime, FlogNum);

  if(!pBtt || !pFlogRuntime) {
    return EFI_INVALID_PARAMETER;
  }
  pFlogRuntime->Entry = FlogOffset;

  if(FlogOffset == 0) {
    NVDIMM_DBG("invalid flog offset %llu", FlogOffset);
    return EFI_INVALID_PARAMETER;
  }

  ReturnCode = ReadNamespaceBytes(
    pBtt->pNamespace,
    FlogOffset,
    pFlogPair,
    sizeof(BTT_FLOG_PAIR)
  );

  if(EFI_ERROR(ReturnCode)) {
    return ReturnCode;
  }

  if(EFI_ERROR(IsLbaValid(pBtt, pFlogPair->Flog[0].Lba)) || EFI_ERROR(IsLbaValid(pBtt, pFlogPair->Flog[1].Lba))) {
    return EFI_INVALID_PARAMETER;
  }

  NVDIMM_VERB("FlogPair[0] FlogOffset=%x OldMap=%d NewMap=%d Seq=%d",
    FlogOffset, pFlogPair->Flog[0].OldMap, pFlogPair->Flog[0].NewMap, pFlogPair->Flog[0].Seq);
  NVDIMM_VERB("FlogPair[1] OldMap=%d NewMap=%d Seq=%d",
    pFlogPair->Flog[1].OldMap, pFlogPair->Flog[1].NewMap, pFlogPair->Flog[1].Seq);

  /*
    Interesting cases:
      - no valid Seq numbers:  layout consistency error
      - one valid Seq number:  that's the current Entry
      - two valid Seq numbers: higher number is current Entry
      - identical Seq numbers: layout consistency error
  */
  if (pFlogPair->Flog[0].Seq == pFlogPair->Flog[1].Seq) {
    NVDIMM_DBG("Flog layout error: bad Seq numbers %d %d\n", pFlogPair->Flog[0].Seq, pFlogPair->Flog[1].Seq);
    SET_BIT(&pArena->Flags, BTTINFO_FLAG_ERROR);
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  } else if (pFlogPair->Flog[0].Seq == 0) {
    /* singleton valid Flog at FlogPair[1] */
    CurrentFlogIndex = 1;
    pFlogRuntime->Next = 0;
  } else if(pFlogPair->Flog[1].Seq == 0) {
    /* singleton valid Flog at FlogPair[0] */
    CurrentFlogIndex = 0;
    pFlogRuntime->Next = 1;
  } else if(NSEQ(pFlogPair->Flog[0].Seq) == pFlogPair->Flog[1].Seq) {
    /* FlogPair[1] has the later Sequence number */
    CurrentFlogIndex = 1;
    pFlogRuntime->Next = 0;
  } else if (pFlogPair->Flog[0].Seq == NSEQ(pFlogPair->Flog[1].Seq)) {
    /* FlogPair[0] has the later Sequence number */
    CurrentFlogIndex = 0;
    pFlogRuntime->Next = 1;
  } else {
    NVDIMM_ERR("Flog layout error, not off by 1");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  NVDIMM_VERB("run-time Flog Next is %d", pFlogRuntime->Next);

  NVDIMM_VERB("read Flog[%d]: Lba %d old %d%s%s new %d%s%s", FlogNum,
    pFlogPair->Flog[CurrentFlogIndex].Lba,
    pFlogPair->Flog[CurrentFlogIndex].OldMap & BTT_MAP_ENTRY_LBA_MASK,
     (pFlogPair->Flog[CurrentFlogIndex].OldMap & BTT_MAP_ENTRY_ERROR) ? " ERROR" : "",
     (pFlogPair->Flog[CurrentFlogIndex].OldMap & BTT_MAP_ENTRY_ZERO) ? " ZERO" : "",
    pFlogPair->Flog[CurrentFlogIndex].NewMap & BTT_MAP_ENTRY_LBA_MASK,
     (pFlogPair->Flog[CurrentFlogIndex].NewMap & BTT_MAP_ENTRY_ERROR) ? " ERROR" : "",
     (pFlogPair->Flog[CurrentFlogIndex].NewMap & BTT_MAP_ENTRY_ZERO) ? " ZERO" : "");

  /*
    Decide if the current Flog Info represents a completed
    operation or an incomplete operation.  If completed, the
    OldMap field will contain the free block to be used for
    the Next write.  But if the operation didn't complete(indicated
    by the map Entry not being updated), then NewMap is the free
    block since it never became active according to the map.

    A special case, used by Flog Entries when first created, is
    when OldMap == NewMap.  This Counts as a complete Entry
    and doesn't require reading the map to see if recovery is
    required.
  */
  if(pFlogPair->Flog[CurrentFlogIndex].OldMap == pFlogPair->Flog[CurrentFlogIndex].NewMap) {
    NVDIMM_DBG("Flog[%d] Entry complete(initial state)", FlogNum);
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  /* convert pre-map LBA into an offset into the map */
  MapEntryOffset = pArena->MapOffset + sizeof(BTT_MAP_ENTRIES) * BttGetMapFromLba(pFlogPair->Flog[CurrentFlogIndex].Lba);

  /* read current map Entry */
  CHECK_RESULT(ReadNamespaceBytes(
    pBtt->pNamespace,
    MapEntryOffset,
    &Entry,
    sizeof(BTT_MAP_ENTRIES)
  ), Finish);

  CurrentMapPos = BttGetPositionInMapFromLba(pFlogPair->Flog[CurrentFlogIndex].Lba);
  CurrentMap = Entry.MapEntryLba [CurrentMapPos];

  if (MapEntryIsInitial(CurrentMap)) {
    Entry.MapEntryLba[CurrentMapPos] = pFlogPair->Flog[CurrentFlogIndex].Lba | BTT_MAP_ENTRY_NORMAL;
    CurrentMap = Entry.MapEntryLba[CurrentMapPos];
  }

  if(pFlogPair->Flog[CurrentFlogIndex].NewMap != CurrentMap && pFlogPair->Flog[CurrentFlogIndex].OldMap == CurrentMap) {
    /* last update didn't complete */
    NVDIMM_VERB("recover Flog[%d]: map[%d]: %d",
      FlogNum, pFlogPair->Flog[CurrentFlogIndex].Lba, pFlogPair->Flog[CurrentFlogIndex].NewMap);

    /*
      Recovery step is to complete the transaction by
      updating the map Entry.
    */
    Entry.MapEntryLba [CurrentMapPos] = pFlogPair->Flog[CurrentFlogIndex].NewMap;
    EFI_STATUS WriteResult = WriteNamespaceBytes(
      pBtt->pNamespace,
      MapEntryOffset,
      &Entry,
      sizeof(BTT_MAP_ENTRIES)
    );

    if(EFI_ERROR(WriteResult)) {
      return WriteResult;
    }
  }

  ReturnCode = EFI_SUCCESS;
Finish:
  return ReturnCode;
}
/*
 * The flog entries are not checksummed.  Instead, increasing sequence
 * numbers are used to atomically switch the active flog entry between
 * the first and second struct btt_flog in each slot.  In order for this
 * to work, the sequence number must be updated only after all the other
 * fields in the flog are updated.  So the writes to the flog are broken
 * into two writes, one for the first three fields (lba, old_map, new_map)
 * and, only after those fields are known to be written durably, the
 * second write for the seq field is done.
 *
 *
 * NOTE: Our code differs from the spec in keeping a copy of the flog
 * pair around instead of just the current flog.
 */
STATIC
EFI_STATUS
BttFlogUpdate(
  IN     BTT *pBtt,
  IN     ARENAS *pArena,
  IN     UINT32 Lba,
  IN     UINT32 OldMap,
  IN     UINT32 NewMap
  )
{
  BTT_FLOG * pCurrentFlog = NULL;
  BTT_FLOG * pNextFlog = NULL;
  BTT_FLOG_PAIR * pFlogPair = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 NextFlogOffset = 0;

  NVDIMM_DBG("pBttp=%p pArena=%p ", pBtt, pArena);
  NVDIMM_DBG("LBA=%x OldMap=%d NewMap=%d", Lba, OldMap, NewMap);

  if(!pBtt || !pArena) {
    return EFI_INVALID_PARAMETER;
  }

  pFlogPair = &(pArena->pFlogs[FLOG_PAIR_0].FlogPair);
  pNextFlog = &(pFlogPair->Flog[pArena->pFlogs[FLOG_PAIR_0].Next]);
  if (FLOG_0 == pArena->pFlogs[FLOG_PAIR_0].Next) {
    pCurrentFlog = &(pFlogPair->Flog[FLOG_1]);
  }
  else if (FLOG_1 == pArena->pFlogs[FLOG_PAIR_0].Next) {
    pCurrentFlog = &(pFlogPair->Flog[FLOG_0]);
  }
  else {
    NVDIMM_ERR("ERROR: Invalid FLOG[0].Next index value:%d\n", pArena->pFlogs[0].Next);
    return EFI_BAD_BUFFER_SIZE;
  }

  // Update the pNextFlog of our internal flog pair. We currently differ from
  // the reference implementation in keeping the flog pair instead of just
  // the current flog.
  pNextFlog->Lba = Lba;
  pNextFlog->OldMap = OldMap;
  pNextFlog->NewMap = NewMap;
  pNextFlog->Seq = NSEQ(pCurrentFlog->Seq);

  // Write out the pNextFlog entry to the dimm

  NextFlogOffset = pArena->pFlogs[0].Entry + pArena->pFlogs[0].Next*sizeof(BTT_FLOG);

  // write out first two fields first
  CHECK_RESULT(WriteNamespaceBytes(pBtt->pNamespace,
      NextFlogOffset, pNextFlog, sizeof(UINT32) * 2), Finish);

  NextFlogOffset += sizeof(UINT32) * 2;

  // write out new_map and seq field to make it active
  CHECK_RESULT(WriteNamespaceBytes(pBtt->pNamespace,
    NextFlogOffset, &(pNextFlog->NewMap), sizeof(UINT32) * 2), Finish);

  // Flog Entry written successfully, update run-time state
  pArena->pFlogs[0].Next = 1 - pArena->pFlogs[0].Next;

  NVDIMM_VERB("update Flog[0]: Lba=%d old=%d%s%s new %d%s%s", Lba,
    OldMap & BTT_MAP_ENTRY_LBA_MASK,(OldMap & BTT_MAP_ENTRY_ERROR) ? " ERROR" : "",
     (OldMap & BTT_MAP_ENTRY_ZERO) ? " ZERO" : "", NewMap & BTT_MAP_ENTRY_LBA_MASK,
     (NewMap & BTT_MAP_ENTRY_ERROR) ? " ERROR" : "",(NewMap & BTT_MAP_ENTRY_ZERO) ? " ZERO" : "");

  ReturnCode = EFI_SUCCESS;
Finish:
  return ReturnCode;
}

STATIC
EFI_STATUS
BttReadFlogs(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena
  )
{
  EFI_STATUS ReadFlogPairResult;
  UINT64 FlogOffset = 0;
  FLOG_RUNTIME *pFlogRuntime = NULL;
  UINT32 Index = 0;

  if(!pBtt || !pArena) {
    return EFI_INVALID_PARAMETER;
  }

  pArena->pFlogs = (FLOG_RUNTIME *)AllocatePool(pBtt->NFree * sizeof(FLOG_RUNTIME));
  if(!pArena->pFlogs) {
    NVDIMM_VERB("Memory allocation for %d Flog Entries", pBtt->NFree);
    return EFI_OUT_OF_RESOURCES;
  }
  ZeroMem(pArena->pFlogs, pBtt->NFree * sizeof(FLOG_RUNTIME));

  /*
    Load up the Flog state.  BttReadFlogPair() will determine if
    any recovery steps are required take them on the in-memory
    data structures it creates. Sets error flag when it
    determines an invalid state.
  */
  FlogOffset = pArena->FlogOffset;
  pFlogRuntime = pArena->pFlogs;

  for(Index = 0; Index < pBtt->NFree; Index++) {
    ReadFlogPairResult = BttReadFlogPair(pBtt, pArena, FlogOffset, pFlogRuntime, Index);
    if(EFI_ERROR(ReadFlogPairResult)) {
      BttSetArenaError(pBtt, pArena);
      return ReadFlogPairResult;
    }

    /* prepare for Next time around the loop */
    FlogOffset += ROUNDUP(sizeof(BTT_FLOG_PAIR), BTT_FLOG_PAIR_ALIGN);
    pFlogRuntime++;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BttBuildRtt(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena
  )
{
  UINT32 Lane = 0;

  if(!pBtt || !pArena) {
    return EFI_INVALID_PARAMETER;
  }

  pArena->pRtt = AllocatePool(pBtt->NFree * sizeof(UINT32));
  if(!pArena->pRtt) {
    NVDIMM_DBG("Memory allocation for %d Rtt Entries failed", pBtt->NFree);
    return EFI_OUT_OF_RESOURCES;
  }

  for(Lane = 0; Lane < pBtt->NFree; Lane++) {
    pArena->pRtt[Lane] = BTT_MAP_ENTRY_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BttReadArena(
  IN     BTT *pBtt,
  IN     UINT64 ArenaOffset,
  IN OUT ARENAS *pArena
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BTT_INFO *pBttInfo = NULL;

  NVDIMM_VERB("pBttp=%p pArena=%p ArenaOffset=%lld", pBtt, pArena, ArenaOffset);

  if (pBtt == NULL || pArena == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pBttInfo = (BTT_INFO *) AllocateZeroPool(sizeof(*pBttInfo));
  if (pBttInfo == NULL) {
    NVDIMM_DBG("Memory allocation for BTT Info failed");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  NVDIMM_DBG("ArenaOffset=%lx", ArenaOffset);

  CHECK_RESULT(ReadNamespaceBytes(pBtt->pNamespace, ArenaOffset, pBttInfo, sizeof(BTT_INFO)), Finish);

  pArena->Flags = pBttInfo->Flags;
  pArena->ExternalNLbas = pBttInfo->ExternalNLbas;
  pArena->InternalLbaSize = pBttInfo->InternalLbaSize;
  pArena->InternalNLbas = pBttInfo->InternalNLbas;

  // pBttInfo offsets are relative to beginning of this arena's info block
  // pArena offsets are relative to beginning of encapsulating namespace
  pArena->StartOffset = ArenaOffset;
  pArena->DataOffset = ArenaOffset + pBttInfo->DataOffset;
  pArena->MapOffset = ArenaOffset + pBttInfo->MapOffset;
  pArena->FlogOffset = ArenaOffset + pBttInfo->FlogOffset;
  pArena->NextOffset = ArenaOffset + pBttInfo->NextOffset;

  CHECK_RESULT(BttReadFlogs(pBtt, pArena), Finish);
  CHECK_RESULT(BttBuildRtt(pBtt, pArena), Finish);

Finish:
  FREE_POOL_SAFE(pBttInfo);
  return ReturnCode;
}

STATIC
EFI_STATUS
BttReadArenas(
  IN     BTT *pBtt,
  IN     UINT32 NArenas
  )
{
  UINT32 ArenasSize = 0;
  UINT64 ArenaOffset = 0;
  ARENAS *pArena = NULL;
  UINT32 Index = 0;

  if(!pBtt) {
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS ErrorValue = EFI_INVALID_PARAMETER;
  ArenasSize = NArenas * sizeof(ARENAS);
  pBtt->Arenas = AllocatePool(ArenasSize);
  if(!pBtt->Arenas) {
    NVDIMM_DBG("Memory allocation for %d Arenas failed", NArenas);
    ErrorValue = EFI_OUT_OF_RESOURCES;
    goto RetVal;
  }
  ZeroMem(pBtt->Arenas, ArenasSize);

  /* Set ArenaOffset to PrimaryInfoOffset */
  ArenaOffset = pBtt->PrimaryInfoOffset;

  pArena = pBtt->Arenas;
  EFI_STATUS ReadArenaResult;
  for(Index = 0; Index < NArenas; Index++) {
    ReadArenaResult = BttReadArena(pBtt, ArenaOffset, pArena);
    if(EFI_ERROR(ReadArenaResult)) {
      ErrorValue = ReadArenaResult;
      goto RetVal;
    }

    /* prepare for Next time around the loop */
    ArenaOffset = pArena->NextOffset;
    pArena++;
  }

  pBtt->Laidout = TRUE;
  return EFI_SUCCESS;

RetVal:
  NVDIMM_DBG("Error clean up");
  if(pBtt->Arenas) {
    for(Index = 0; Index < pBtt->NArenas; Index++) {
      if(pBtt->Arenas[Index].pFlogs != NULL) {
        FreePool(pBtt->Arenas[Index].pFlogs);
        pBtt->Arenas[Index].pFlogs = NULL;
      }
      if(pBtt->Arenas[Index].pRtt != NULL) {
        FreePool((void *)pBtt->Arenas[Index].pRtt);
        pBtt->Arenas[Index].pRtt = NULL;
      }
    }
    FreePool(pBtt->Arenas);
    pBtt->Arenas = NULL;
  }
  return ErrorValue;
}

EFI_STATUS
BttWriteLayout(
  IN     BTT *pBtt,
  IN     BOOLEAN Write
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 FlogSize = 0;
  UINT32 InternalLbaSize = 0;
  UINT64 TotalNLbas = 0;
  UINT64 RawSize = 0;
  UINT8 ArenaNumber = 0;
  UINT64 ArenaOffset = 0;
  UINT64 ArenaRawSize = 0;
  UINT64 ArenaDataSize = 0;
  UINT32 InternalNLbas = 0;
  UINT32 ExternalNLbas = 0;
  UINT64 MapSize = 0;
  UINT64 NextOffset = 0;
  UINT64 InfoOffset = 0;
  UINT64 FlogOffset = 0;
  UINT64 MapOffset = 0;
  UINT64 DataOffset = 0;
  UINT64 MapEntryOffset = 0;
  BTT_MAP_ENTRIES *pMap = NULL;
  UINT32 MapBlock = 0;
  UINT32 Index = 0;
  BTT_INFO *pBttInfo = NULL;
  UINT64 FlogEntryOffset = 0;
  UINT32 NextFreeLba = 0;
  BTT_FLOG_PAIR FlogPair;

  SetMem(&FlogPair, sizeof(FlogPair), 0x0);
  NVDIMM_VERB("pBtt=%p Write=%d", pBtt, Write);

  if (pBtt == NULL) {
    goto Finish;
  }

  if (pBtt->RawSize < BTT_NAMESPACE_MIN_SIZE) {
    goto Finish;
  }

  if (pBtt->NFree == 0) {
    goto Finish;
  }

  if (Write) {
    GenerateRandomGuid(&pBtt->Uuid);
  }

  /**
    The number of Arenas is the number of full arena of
    size BTT_MAX_ARENA that fit into RawSize and then, if
    the remainder is at least BTT_MIN_SIZE in size, then
    that adds one more arena.
  **/
  pBtt->NArenas = (UINT8)(pBtt->RawSize / BTT_MAX_ARENA_SIZE);
  if(pBtt->RawSize % BTT_MAX_ARENA_SIZE >= BTT_NAMESPACE_MIN_SIZE) {
    pBtt->NArenas++;
  }
  NVDIMM_DBG("NArenas=%d", pBtt->NArenas);

  FlogSize = pBtt->NFree * ROUNDUP(sizeof(BTT_FLOG_PAIR), BTT_FLOG_PAIR_ALIGN);
  FlogSize = ROUNDUP(FlogSize, BTT_ALIGNMENT);

  InternalLbaSize = pBtt->LbaSize;
  if(InternalLbaSize < BTT_MIN_LBA_SIZE) {
    InternalLbaSize = BTT_MIN_LBA_SIZE;
  }

  InternalLbaSize = ROUNDUP(InternalLbaSize, CACHE_LINE_SIZE);
  /* check for overflow */
  if(InternalLbaSize < CACHE_LINE_SIZE) {
    NVDIMM_DBG("Invalid LBA size after alignment: %d ", InternalLbaSize);
    goto Finish;
  }
  pBtt->InternalLbaSize = InternalLbaSize;

  NVDIMM_VERB("Adjusted InternalLbaSize: %d", InternalLbaSize);

  RawSize = pBtt->RawSize;

  pBttInfo = (BTT_INFO *) AllocateZeroPool(sizeof(*pBttInfo));
  if (pBttInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pMap = (BTT_MAP_ENTRIES *) AllocateZeroPool(BTT_ALIGNMENT);
  if (pMap == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  /** Set ArenaOffset to offset of 1st info block**/
  ArenaOffset = pBtt->PrimaryInfoOffset;

  // for each arena
  while (RawSize >= BTT_NAMESPACE_MIN_SIZE) {
    NVDIMM_DBG("Layout arena %d", ArenaNumber);

    ArenaRawSize = RawSize;
    if(ArenaRawSize > BTT_MAX_ARENA_SIZE) {
      ArenaRawSize = BTT_MAX_ARENA_SIZE;
    }
    RawSize -= ArenaRawSize;
    ArenaNumber++;

    ArenaDataSize = ArenaRawSize;
    ArenaDataSize -= 2 * sizeof(BTT_INFO);
    ArenaDataSize -= FlogSize;

    /* allow for map alignment padding */
    InternalNLbas = (UINT32)((ArenaDataSize - BTT_ALIGNMENT) / (InternalLbaSize + BTT_MAP_ENTRY_SIZE));
    /* ensure the number of blocks is at least 2*NFree */
    if (InternalNLbas < 2 * pBtt->NFree) {
      NVDIMM_DBG("Number of internal blocks: %x, expected at least %d", InternalNLbas, 2 * pBtt->NFree);
      goto Finish;
    }
    ExternalNLbas = InternalNLbas - pBtt->NFree;

    NVDIMM_DBG("InternalNLbas=%d ExternalNLbas=%d", InternalNLbas, ExternalNLbas);

    TotalNLbas += ExternalNLbas;

    MapSize = ROUNDUP(ExternalNLbas * BTT_MAP_ENTRY_SIZE, BTT_ALIGNMENT);
    ArenaDataSize -= MapSize;

    if (ArenaDataSize / InternalLbaSize < InternalNLbas) {
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    /**
      The rest of the loop body calculates metadata structures
      and lays it out for this arena.  So only continue if
      the write flag is set.
    **/
    if (!Write) {
      continue;
    }

    InfoOffset = ArenaRawSize - sizeof(BTT_INFO);
    FlogOffset = InfoOffset - FlogSize;
    MapOffset = FlogOffset - MapSize;
    DataOffset = MapOffset - ArenaDataSize;

    if(RawSize >= BTT_NAMESPACE_MIN_SIZE) {
      NextOffset = BTT_MAX_ARENA_SIZE;
    } else {
      NextOffset = 0;
    }

    NVDIMM_DBG("Namespace offsets:");
    NVDIMM_DBG("ArenaOffset  0x%012lx", ArenaOffset);
    NVDIMM_DBG("DataOffset  0x%012lx", ArenaOffset + DataOffset);
    NVDIMM_DBG("MapOffset   0x%012lx", ArenaOffset + MapOffset);
    NVDIMM_DBG("FlogOffset  0x%012lx", ArenaOffset + FlogOffset);
    NVDIMM_DBG("InfoOffset 0x%012lx", ArenaOffset + InfoOffset);
    NVDIMM_DBG("NextOffset  0x%012lx", ArenaOffset + NextOffset);

    /* Zero out the initial map, identity style */
    MapEntryOffset = ArenaOffset + MapOffset;
    // Write map layout in 4k blocks
    for(MapBlock = 0; MapBlock <= MapSize / BTT_ALIGNMENT; MapBlock++) {
      ReturnCode = WriteNamespaceBytes
         (pBtt->pNamespace,
        MapEntryOffset + (MapBlock * BTT_ALIGNMENT), pMap, BTT_ALIGNMENT);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
    }

    /* write out the initial Flog */
    FlogEntryOffset = ArenaOffset + FlogOffset;
    NextFreeLba = ExternalNLbas;
    ZeroMem(&FlogPair.Flog[1], sizeof(BTT_FLOG));
    for (Index = 0; Index < pBtt->NFree; Index++) {
      FlogPair.Flog[0].Lba = 0;
      FlogPair.Flog[0].OldMap = FlogPair.Flog[0].NewMap = NextFreeLba;
      FlogPair.Flog[0].Seq = 1;

      /*
        Write both btt_Flog structs in the pair, writing
        the second one as all zeros.
      */
      NVDIMM_VERB("Flog[%d] Entry off=%x initial %d + zero = %d",
        Index, FlogEntryOffset, NextFreeLba, NextFreeLba);
      ReturnCode = WriteNamespaceBytes
         (pBtt->pNamespace,
        FlogEntryOffset, &FlogPair, sizeof(BTT_FLOG_PAIR));
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      FlogEntryOffset += sizeof(BTT_FLOG_PAIR);
      FlogEntryOffset = ROUNDUP(FlogEntryOffset, BTT_FLOG_PAIR_ALIGN);

      NextFreeLba++;
    }

    // Construct the BTT Info block and write it out at both the beginning and end of the arena.
    ZeroMem(pBttInfo, sizeof(*pBttInfo));
    CopyMem_S(pBttInfo->Sig, sizeof(pBttInfo->Sig), Sig, BTTINFO_SIG_LEN);
    CopyMem_S(&pBttInfo->Uuid, sizeof(pBttInfo->Uuid), &pBtt->Uuid, sizeof(GUID));
    CopyMem_S(&pBttInfo->ParentUuid, sizeof(pBttInfo->ParentUuid), &pBtt->ParentUuid, sizeof(GUID));
    // Check BTT version. 2.0 offset is 0, 1.1 offset is 4K.
    if (pBtt->PrimaryInfoOffset == BTT_PRIMARY_INFO_BLOCK_OFFSET) {
      pBttInfo->Major = 2;
      pBttInfo->Minor = 0;
    } else {
      pBttInfo->Major = 1;
      pBttInfo->Minor = 1;
    }
    pBttInfo->ExternalLbaSize = pBtt->LbaSize;
    pBttInfo->ExternalNLbas = ExternalNLbas;
    pBttInfo->InternalLbaSize = InternalLbaSize;
    pBttInfo->InternalNLbas = InternalNLbas;
    pBttInfo->NFree = pBtt->NFree;
    pBttInfo->InfoSize = sizeof(*pBttInfo);
    // Following offsets are relative to the beginning of this arena info block
    pBttInfo->NextOffset = NextOffset;
    pBttInfo->DataOffset = DataOffset;
    pBttInfo->MapOffset = MapOffset;
    pBttInfo->FlogOffset = FlogOffset;
    pBttInfo->InfoOffset = InfoOffset;

    NVDIMM_DBG("BTT info block offsets:");
    NVDIMM_DBG("DataOffset  0x%012lx", pBttInfo->DataOffset);
    NVDIMM_DBG("MapOffset   0x%012lx", pBttInfo->MapOffset);
    NVDIMM_DBG("FlogOffset  0x%012lx", pBttInfo->FlogOffset);
    NVDIMM_DBG("Info2Offset 0x%012lx", pBttInfo->InfoOffset);
    NVDIMM_DBG("NextOffset  0x%012lx", pBttInfo->NextOffset);

    ChecksumOperations((VOID *)pBttInfo, sizeof(BTT_INFO), &pBttInfo->Checksum, TRUE);

    ReturnCode = WriteNamespaceBytes(pBtt->pNamespace, ArenaOffset, pBttInfo, sizeof(BTT_INFO));
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = WriteNamespaceBytes(pBtt->pNamespace, ArenaOffset + InfoOffset, pBttInfo, sizeof(BTT_INFO));
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ArenaOffset += NextOffset;
  }

  if (pBtt->NArenas != ArenaNumber) {
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  pBtt->NLbas = TotalNLbas;

  if (Write) {
    //The layout is written now, so load up the Arenas, and set laidout flag.
    BttReadArenas(pBtt, pBtt->NArenas);
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pMap);
  FREE_POOL_SAFE(pBttInfo);
  return ReturnCode;
}

STATIC
EFI_STATUS
BttReadLayout(
  IN     BTT *pBtt
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 NArenas = 0;
  UINT32 SmallestNFree = MAX_UINT32_VALUE;
  UINT64 RawSize = 0;
  UINT64 TotalNLbas = 0;
  UINT64 ArenaOffset = 0;
  BTT_INFO *pBttInfo = NULL;

  NVDIMM_DBG("pBttp=%p", pBtt);

  if (pBtt == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pBttInfo = (BTT_INFO *) AllocateZeroPool(sizeof(*pBttInfo));
  if (pBttInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pBtt->NFree = BTT_DEFAULT_NFREE;
  RawSize = pBtt->RawSize;

  ArenaOffset = pBtt->PrimaryInfoOffset;

  // For each arena, see if there's a valid Info block
  while(RawSize >= BTT_NAMESPACE_MIN_SIZE) {
    NArenas++;

    NVDIMM_DBG("ArenaOffset: %llx", ArenaOffset);

    ReturnCode = ReadNamespaceBytes
       (pBtt->pNamespace, ArenaOffset, pBttInfo, sizeof(BTT_INFO));
    if(EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    NVDIMM_DBG("BTT info block offsets:");
    NVDIMM_DBG("DataOffset  0x%012lx", pBttInfo->DataOffset);
    NVDIMM_DBG("MapOffset   0x%012lx", pBttInfo->MapOffset);
    NVDIMM_DBG("FlogOffset  0x%012lx", pBttInfo->FlogOffset);
    NVDIMM_DBG("Info2Offset 0x%012lx", pBttInfo->InfoOffset);
    NVDIMM_DBG("NextOffset  0x%012lx", pBttInfo->NextOffset);

    ReturnCode = BttReadInfo(pBttInfo, pBtt);
    if (EFI_ERROR(ReturnCode)) {
      /**
        Failed to find complete BTT metadata.  Just
        calculate the NArenas and NLbas values that will
        result when BttWriteLayout() gets called.  This
        allows checks against NLbas to work correctly
        even before the layout is written.

        Need to check for a backup info block.
        If valid backup info block found, copy to primary info block.
        See UEFI 2.7 6.3.5
      **/
      ReturnCode = BttWriteLayout(pBtt, FALSE);
      goto Finish;
    }

    if(pBttInfo->ExternalLbaSize != pBtt->LbaSize) {
      /* can't read it assuming the wrong block size */
      NVDIMM_DBG("inconsistent lbasize, ns: %d btt:%d", pBttInfo->ExternalLbaSize, pBtt->LbaSize);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    if(pBttInfo->NFree == 0) {
      NVDIMM_DBG("invalid NFree");
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    if(pBttInfo->ExternalNLbas == 0) {
      NVDIMM_DBG("invalid ExternalNLbas");
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    if(pBttInfo->NextOffset &&(pBttInfo->NextOffset != BTT_MAX_ARENA_SIZE)) {
      NVDIMM_DBG("invalid arena size: %llx", pBttInfo->NextOffset);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    if(pBttInfo->NFree < SmallestNFree) {
      SmallestNFree = pBttInfo->NFree;
    }

    TotalNLbas += pBttInfo->ExternalNLbas;
    ArenaOffset += pBttInfo->NextOffset;
    if(pBttInfo->NextOffset == 0) {
      break;
    }
    if(pBttInfo->NextOffset > RawSize) {
      NVDIMM_DBG("invalid next arena offset. Next: %llx RawSize: %llx", pBttInfo->NextOffset, RawSize);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    RawSize -= pBttInfo->NextOffset;
  }

  if(!NArenas) {
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  pBtt->NArenas = NArenas;
  pBtt->NLbas = TotalNLbas;

  // All Arenas were valid.  NFree should be the smallest value found among different arenas.
  if(SmallestNFree < pBtt->NFree) {
    pBtt->NFree = SmallestNFree;
  }

  // Load up Arenas.
  ReturnCode = BttReadArenas(pBtt, NArenas);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  FREE_POOL_SAFE(pBttInfo);
  return ReturnCode;
}

STATIC
EFI_STATUS
BttZeroBlock(
  IN     BTT *pBtt,
     OUT VOID *pBuffer
  )
{
  if(!pBtt) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem(pBuffer, pBtt->LbaSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BttLbaToArenaLba(
  IN     BTT *pBtt,
  IN     UINT64 Lba,
     OUT ARENAS **ppArena,
     OUT UINT32 *PreMapLba
  )
{
  UINT8 Arena = 0;

  if(!pBtt) {
    return EFI_INVALID_PARAMETER;
  }

  if(!pBtt->Laidout) {
    return EFI_ABORTED;
  }

  for(Arena = 0; Arena < pBtt->NArenas; Arena++) {
    if(Lba < pBtt->Arenas [Arena].ExternalNLbas) {
      break;
    } else {
      Lba -= pBtt->Arenas [Arena].ExternalNLbas;
    }
  }

  if(Arena >= pBtt->NArenas) {
    return EFI_ABORTED;
  }

  *ppArena = &pBtt->Arenas [Arena];
  *PreMapLba = (UINT32)Lba;

  NVDIMM_VERB("pArena=%p PreMapLBA=%x", *ppArena, *PreMapLba);
  return EFI_SUCCESS;
}

/**
  Read a block from a btt namespace

  @param [in] pBtt namespace handle
  @param [in] Lba Logical block address to be read
  @param [out] pBuffer Read result Buffer pointer

  @retval EFI_SUCCESS if the routine succeeds
**/
EFI_STATUS
BttRead(
  IN     BTT *pBtt,
  IN     UINT64 Lba,
     OUT VOID *pBuffer
  )
{
  ARENAS *pArena = NULL;
  UINT32 PreMapLba = 0;
  UINT64 MapEntryOffset = 0;
  BTT_MAP_ENTRIES Entry;
  UINT8 PosInPreMapLba = 0;
  UINT32 CurrentMap = 0;
  INT8 MapCheck = 0;
  BTT_MAP_ENTRIES LatestEntry;
  UINT32 LatestMap = 0;
  UINT64 DataBlockOffset = 0;
  EFI_STATUS RetVal = EFI_SUCCESS;

  SetMem(&Entry, sizeof(Entry), 0x0);
  SetMem(&LatestEntry, sizeof(LatestEntry), 0x0);

  NVDIMM_VERB("pBtt=%p LBA=%x reading!", pBtt, Lba);

  if(!pBtt || !pBuffer) {
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Result = IsLbaValid(pBtt, Lba);
  if(EFI_ERROR(Result)) {
    return Result;
  }

  /* if there's no layout written yet, all reads come back as zeros */
  if(!pBtt->Laidout) {
    return BttZeroBlock(pBtt, pBuffer);
  }

  /* find which arena LBA lives in, and the offset to the map Entry */
  Result = BttLbaToArenaLba(pBtt, Lba, &pArena, &PreMapLba);
  if(EFI_ERROR(Result))
    return Result;

  /* convert pre-map LBA into an offset into the map */
  MapEntryOffset = pArena->MapOffset + sizeof(BTT_MAP_ENTRIES) * BttGetMapFromLba(PreMapLba);

  /*
    Read the current map Entry to get the post-map LBA for the data
    block read.
  */

  Result = ReadNamespaceBytes
     (pBtt->pNamespace, MapEntryOffset, &Entry, sizeof(BTT_MAP_ENTRIES));
  if(EFI_ERROR(Result)) {
    return Result;
  }
  PosInPreMapLba = BttGetPositionInMapFromLba(PreMapLba);
  CurrentMap = Entry.MapEntryLba[PosInPreMapLba];
  /*
    Retries come back to the top of this loop(for a rare case where
    the map is changed by another thread doing writes to the same LBA).
  */
  while(MapCheck == 0) {
    if(MapEntryIsError(CurrentMap)) {
      NVDIMM_DBG("EIO due to map Entry Error flag");
      return EFI_ABORTED;
    }

    if(MapEntryIsZeroOrInitial(CurrentMap)) {
      return BttZeroBlock(pBtt, pBuffer);
    }

    /*
       Record the post-map LBA in the read tracking table during
       the read.  The write will check Entries in the read tracking
       table before allocating a block for a write, waiting for
       outstanding reads on that block to complete.
       No need to mask off ERROR and ZERO bits since the above
       checks make sure they are clear at this point.
    */
    pArena->pRtt[0] = CurrentMap;

    /*
       In case this thread was preempted between reading Entry and
       storing it in the Rtt, check to see if the map changed.  If
       it changed, the block about to be read is at least free now
      (in the Flog, but that's okay since the data will still be
       undisturbed) and potentially allocated and being used for
       another write(data disturbed, so not okay to continue).
    */
    Result = ReadNamespaceBytes
       (pBtt->pNamespace,
      MapEntryOffset, &LatestEntry, sizeof(BTT_MAP_ENTRIES));
    if(EFI_ERROR(Result)) {
      pArena->pRtt[0] = BTT_MAP_ENTRY_ERROR;
      return Result;
    }
    LatestMap = LatestEntry.MapEntryLba [PosInPreMapLba];
    if(CurrentMap == LatestMap) {
      MapCheck++;          /* map stayed the same */
    }
    else {
      CurrentMap = LatestMap;   /* try again */
    }
  }

  /*
     It is safe to read the block now, since the Rtt protects the
     block from getting re-allocated to something else by a write.

     Convert the offset in bytes to block offset
  */
  DataBlockOffset = pArena->DataOffset + (UINT64)(CurrentMap & BTT_MAP_ENTRY_LBA_MASK) * pArena->InternalLbaSize;
  NVDIMM_DBG("LBA=%x->LBAbtt=%x, Offset[B]=%lx",
      Lba, (UINT64) CurrentMap & BTT_MAP_ENTRY_LBA_MASK, DataBlockOffset);

  RetVal = ReadNamespaceBytes
     (pBtt->pNamespace, DataBlockOffset, pBuffer, pBtt->LbaSize);

  /* done with read, so clear out Rtt Entry */
  pArena->pRtt[0] = BTT_MAP_ENTRY_ERROR;

  return RetVal;
}

EFI_STATUS
BttCheck(
  IN     BTT *pBtt
  )
{
  EFI_STATUS retVal = EFI_SUCCESS;
  ARENAS *pArena = NULL;
  UINT8 Index = 0;

  NVDIMM_DBG("Btt %p", pBtt);

  if(!pBtt) {
    return EFI_INVALID_PARAMETER;
  }

  if(!pBtt->Laidout) {
    /* consistent by definition */
    NVDIMM_DBG("no layout yet");
    return retVal;
  }

  // for each arena
  pArena = pBtt->Arenas;
  for(Index = 0; Index < pBtt->NArenas; Index++) {
    // Perform the consistency checks for the arena.
    retVal = BttCheckArena(pBtt, pArena);
    if(EFI_ERROR(retVal)) {
      return retVal;
    }
  }

  return retVal;
}

STATIC
EFI_STATUS
BttCheckArena(
  IN     BTT *pBtt,
  IN     ARENAS *pArena
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Bitmapsize = 0;
  UINT8 *pBitmap = NULL;
  UINT32 MapEntry = 0;
  UINT32 RemainingMaps = 0;
  UINT32 MapsCount = 0;
  UINT8 MapEndPosition = 0;
  BTT_MAP_ENTRIES *pMap = NULL;
  UINT64 MapSize = 0;
  UINT32 MapBlock = 0;
  UINT32 Index = 0;
  UINT8 Position = 0;
  UINT8 CurrentFlogIndex = 0;
  UINT32 Entry = 0;

  NVDIMM_DBG("Bttp %p pArena %p", pBtt, pArena);

  if (pBtt == NULL || pArena == NULL) {
    goto Finish;
  }

  pMap = (BTT_MAP_ENTRIES *) AllocateZeroPool(BTT_ALIGNMENT);
  if (pMap == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  Bitmapsize = HOW_MANY(pArena->InternalNLbas, 8);
  pBitmap = (UINT8 *)AllocatePool(Bitmapsize);
  if(!pBitmap) {
    NVDIMM_DBG("!Memory allocation for Bitmap");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  ZeroMem(pBitmap, Bitmapsize);

  /**
    Go through every post-map LBA mentioned in the map and make sure
    there are no duplicates.  Bitmap is used to track which LBAs have
    been seen so far.
  **/
  MapsCount = BttGetMapFromLba(pArena->ExternalNLbas);
  MapEndPosition = BttGetPositionInMapFromLba(pArena->ExternalNLbas);
  MapSize = ROUNDUP(pArena->ExternalNLbas * BTT_MAP_ENTRY_SIZE, BTT_ALIGNMENT);
  // Read entire map layout in 4k blocks
  for(MapBlock = 0; MapBlock <= MapSize / BTT_ALIGNMENT; MapBlock++) {
    ReturnCode = ReadNamespaceBytes
       (pBtt->pNamespace, pArena->MapOffset +(MapBlock * BTT_ALIGNMENT), pMap, BTT_ALIGNMENT);
    if(EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if(MapsCount > MapBlock * BTT_ALIGNMENT / sizeof(BTT_MAP_ENTRIES)) { //protect overturn
      RemainingMaps = MapsCount - MapBlock * BTT_ALIGNMENT / sizeof(BTT_MAP_ENTRIES);
    }
    else {
      break;
    }
    for (Index = 0; Index < BTT_ALIGNMENT / sizeof(BTT_MAP_ENTRIES); Index++) {
      if(Index > RemainingMaps) {   //last BTT_MAP_ENTRIES within this MapBlock
        break;
      }
      for (Position = 0; Position < BTT_MAP_LOCK_ALIGN / BTT_MAP_ENTRY_SIZE; Position++) {
        //End the loop for the last Lba
        if(Index == RemainingMaps && Position == MapEndPosition) {
          break;
        }
        MapEntry = pMap [Index].MapEntryLba [Position];

        /* for debug, dump non-zero map Entries */
        if((MapEntry & BTT_MAP_ENTRY_ZERO) == 0) {
          NVDIMM_VERB("map[%d]: %d%s%s", Index, MapEntry & BTT_MAP_ENTRY_LBA_MASK,
             (MapEntry & BTT_MAP_ENTRY_ERROR) ? " ERROR" : "",(MapEntry & BTT_MAP_ENTRY_ZERO) ? " ZERO" : "");
        }

        if (MapEntryIsInitial(MapEntry)) {
          MapEntry = MapBlock * (BTT_ALIGNMENT / BTT_MAP_ENTRY_SIZE) + Index *
              (sizeof(BTT_MAP_ENTRIES) / BTT_MAP_ENTRY_SIZE) + Position;
        } else {
          MapEntry &= BTT_MAP_ENTRY_LBA_MASK;
        }

        /* check if entry is valid */
        if(MapEntry >= pArena->ExternalNLbas) {
          NVDIMM_DBG("map[%d] Entry out of bounds: %d", Index, MapEntry);
          goto Finish;
        }

        if (IS_BIT_SET(pBitmap, MapEntry)) {
          NVDIMM_DBG("map[%d] duplicate Entry: %d", Index, MapEntry);
          ReturnCode = EFI_ABORTED;
          goto Finish;
        } else {
          SET_BIT(pBitmap, MapEntry);
        }
      }
    }
  }

  /*
    Go through the free blocks in the Flog, adding them to Bitmap
    and checking for duplications.  It is sufficient to read the
    run-time Flog here, avoiding more calls to NsRead.
  */
  for (Index = 0; Index < pBtt->NFree; Index++) {
    CurrentFlogIndex = 1 - pArena->pFlogs[Index].Next;
    Entry = pArena->pFlogs[Index].FlogPair.Flog[CurrentFlogIndex].OldMap;
    Entry &= BTT_MAP_ENTRY_LBA_MASK;

    if (IS_BIT_SET(pBitmap, Entry)) {
      NVDIMM_DBG("Flog[%d] duplicate Entry: %d", Index, Entry);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    } else {
      SET_BIT(pBitmap, Entry);
    }
  }

  /*
    Make sure every possible post-map LBA was accounted for
    in the two loops above.
  */
  for(Index = 0; Index < pArena->InternalNLbas; Index++) {
    if (IS_BIT_CLEARED(pBitmap, Index)) {
      NVDIMM_DBG("Unreferenced LBA: %d", Index);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pBitmap);
  FREE_POOL_SAFE(pMap);
  return ReturnCode;
}

STATIC
EFI_STATUS
BttMapLock(
  IN     BTT *pBtt,
  IN     ARENAS *pArena,
     OUT BTT_MAP_ENTRIES *Entry,
  IN     UINT32 PreMapLba
  )
{
  UINT32 MapNumber = 0;
  UINT32 MapPosition = 0;
  UINT64 MapEntryOffset = 0;
  UINT32 BttMapLockNum = 0;

  if(!pBtt || !pArena) {
    return EFI_INVALID_PARAMETER;
  }

  MapNumber = BttGetMapFromLba(PreMapLba);
  MapPosition = BttGetPositionInMapFromLba(PreMapLba);
  MapEntryOffset = pArena->MapOffset + sizeof(BTT_MAP_ENTRIES) * MapNumber;

  /*
    BttMapLock[] contains NFree locks which are used to protect the map
    from concurrent access to the same cache line. The index into
    BttMapLock[] is calculated by looking at the byte offset into the map
     (PreMapLba * BTT_MAP_ENTRY_SIZE), figuring out how many cache lines
    that is into the map that is(dividing by BTT_MAP_LOCK_ALIGN), and
    then selecting one of nfree locks(the modulo at the end).
  */
  BttMapLockNum = MapNumber % pBtt->NFree;

  /* read the old map Entry */
  EFI_STATUS ReadResult = ReadNamespaceBytes
     (pBtt->pNamespace, MapEntryOffset, Entry, sizeof(BTT_MAP_ENTRIES));
  if(EFI_ERROR(ReadResult)) {
    return ReadResult;
  }

  /* if map entry is in its initial state return premap_lba */
  if (MapEntryIsInitial(Entry->MapEntryLba[MapPosition])) {
    Entry->MapEntryLba[MapPosition] = PreMapLba | BTT_MAP_ENTRY_NORMAL;
  }

  NVDIMM_VERB("locked maps[%u], LBAs: %u - %u", BttMapLockNum, Entry->MapEntryLba[0] & BTT_MAP_ENTRY_LBA_MASK,
    Entry->MapEntryLba[CACHE_LINE_SIZE / sizeof(UINT32) - 1] & BTT_MAP_ENTRY_LBA_MASK);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
BttMapUnlock(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena,
  IN     BTT_MAP_ENTRIES *Entry,
  IN     UINT32 PreMapLba
  )
{
  UINT32 MapNumber = 0;
  UINT64 MapEntryOffset = 0;
  UINT32 BttMapLockNum = 0;

  NVDIMM_VERB("Bttp %p pArena %p Entry %p PreMapLba %u", pBtt, pArena, Entry, PreMapLba);

  if(!pBtt || !pArena) {
    return EFI_INVALID_PARAMETER;
  }

  MapNumber = BttGetMapFromLba(PreMapLba);
  MapEntryOffset = pArena->MapOffset + sizeof(BTT_MAP_ENTRIES) * MapNumber;
  BttMapLockNum = MapNumber % pBtt->NFree;

  /* write the new map Entry */
  EFI_STATUS RetVal = WriteNamespaceBytes
     (pBtt->pNamespace,
    MapEntryOffset, Entry, sizeof(BTT_MAP_ENTRIES));

 //pArena->BttMapLock [BttMapLockNum].IndSpinlockRelease(pMapLockHandle);
 NVDIMM_DBG("unlocked maps[%u], LBAs: %u - %u", BttMapLockNum, Entry->MapEntryLba [0] & BTT_MAP_ENTRY_LBA_MASK,
    Entry->MapEntryLba [CACHE_LINE_SIZE / sizeof(UINT32) - 1] & BTT_MAP_ENTRY_LBA_MASK);
  return RetVal;
}

/**
  Writes a block to a btt namespace

  @param [in] pBtt namespace handle
  @param [in] Lba Logical block address to be written
  @param [in] pBuffer Buffer pointer to the block to be written

  @retval EFI_SUCCESS if the routine succeeds
**/
EFI_STATUS
BttWrite(
  IN     BTT *pBtt,
  IN     UINT64 Lba,
  IN     VOID *pBuffer
  )
{
  ARENAS *pArena = NULL;
  UINT32 PreMapLba = 0;
  UINT8 CurrentFlogIndex = 0;
  UINT32 FreeMap = 0;
  UINT32 Index = 0;
  UINT64 DataBlockOffset = 0;
  BTT_MAP_ENTRIES MapEntry;
  UINT8 PosInEntry = 0;
  UINT32 OldMap = 0;

  NVDIMM_VERB("pBtt=%p LBA=%x writing!", pBtt, Lba);
  SetMem(&MapEntry, sizeof(MapEntry), 0x0);

  if(!pBtt) {
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS RetVal = IsLbaValid(pBtt, Lba);
  if(EFI_ERROR(RetVal)) {
    return RetVal;
  }

  /* first write through here will initialize the metadata layout */
  if(!pBtt->Laidout) {
    RetVal = BttWriteLayout(pBtt, TRUE);

    if(EFI_ERROR(RetVal)) {
      return RetVal;
    }
  }

  /* find which arena LBA lives in, and the offset to the map Entry */
  RetVal = BttLbaToArenaLba(pBtt, Lba, &pArena, &PreMapLba);
  if(EFI_ERROR(RetVal)) {
    return RetVal;
  }

  /* if the arena is in an Error state, writing is not allowed */
  if(pArena->Flags & BTTINFO_FLAG_ERROR_MASK) {
    NVDIMM_DBG("EIO due to BttInfo Error Flags 0x%x", pArena->Flags & BTTINFO_FLAG_ERROR_MASK);
    return EFI_ABORTED;
  }

  /*
     This routine was passed a unique "Lane" which is an index
     into the Flog.  That means the free block held by Flog[Lane]
     is assigned to this thread and to no other threads(no additional
     locking required).  So start by performing the write to the
     free block.  It is only safe to write to a free block if it
     doesn't appear in the read tracking table, so scan that first
     and if found, wait for the thread reading from it to finish.
  */
  CurrentFlogIndex = 1 - pArena->pFlogs[0].Next;
  FreeMap = (pArena->pFlogs[0].FlogPair.Flog[CurrentFlogIndex].OldMap & BTT_MAP_ENTRY_LBA_MASK) | BTT_MAP_ENTRY_NORMAL;

  NVDIMM_VERB("FreeMap=%x(before mask %x)", FreeMap, pArena->pFlogs[0].FlogPair.Flog[CurrentFlogIndex].OldMap);

  /* wait for other threads to finish any reads on free block */
  for(Index = 0; Index < pBtt->NLanes; Index++) {
    while(pArena->pRtt[Index] == FreeMap) {
      ;
    }
  } // to be deleted in UEFI

  // it is now safe to perform write to the free block
  DataBlockOffset = pArena->DataOffset + (UINT64)(FreeMap & BTT_MAP_ENTRY_LBA_MASK) * pArena->InternalLbaSize;
  NVDIMM_DBG("LBA=%x->LBAbtt=%x Offset[B]=%lx",
      Lba, (UINT64) FreeMap & BTT_MAP_ENTRY_LBA_MASK, DataBlockOffset);
  RetVal = WriteNamespaceBytes
     (pBtt->pNamespace,
    DataBlockOffset, pBuffer, pBtt->LbaSize);
  if(EFI_ERROR(RetVal)) {
    return RetVal;
  }

  // Make the new block active atomically by updating the on-media Flog and then updating the map.
  RetVal = BttMapLock(pBtt, pArena, &MapEntry, PreMapLba);
  if(EFI_ERROR(RetVal)) {
    return RetVal;
  }

  /* update the Flog */
  PosInEntry = BttGetPositionInMapFromLba(PreMapLba);
  OldMap = MapEntry.MapEntryLba[PosInEntry];
  RetVal = BttFlogUpdate(pBtt, pArena, PreMapLba, OldMap, FreeMap);
  if(EFI_ERROR(RetVal)) {
    NVDIMM_DBG("Could not update the BTT Flog!\nBttp %p pArena %p PreMapLba %u", pBtt, pArena, PreMapLba);
    return RetVal;
  }

  MapEntry.MapEntryLba [PosInEntry] = FreeMap;
  RetVal = BttMapUnlock(pBtt, pArena, &MapEntry, PreMapLba);
  if(EFI_ERROR(RetVal)) {
    BttSetArenaError(pBtt, pArena);
    return RetVal;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
BttArenaSetFlag(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena,
  IN     UINT32 SetFlag
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 ArenaOff = 0;
  BTT_INFO *pBttInfo = NULL;
  /* update runtime state */
  pArena->Flags = SetFlag;

  if (!pBtt->Laidout) {
    /* no layout yet to update */
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  pBttInfo = (BTT_INFO *) AllocateZeroPool(sizeof(*pBttInfo));
  if (pBttInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  /*
    Read, modify and write out the info block
    at both the beginning and end of the arena.
  */
  ArenaOff = pArena->StartOffset;

  /* protect from simultaneous writes to the layout */
  ReturnCode = ReadNamespaceBytes
     (pBtt->pNamespace, ArenaOff, pBttInfo, sizeof(BTT_INFO));
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /* update flags */
  pBttInfo->Flags |= SetFlag;

  /* update checksum */
  ChecksumOperations((VOID *)pBttInfo, sizeof(BTT_INFO), &pBttInfo->Checksum, TRUE);

  ReturnCode = WriteNamespaceBytes
     (pBtt->pNamespace, ArenaOff, pBttInfo, sizeof(BTT_INFO));
  if(EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = WriteNamespaceBytes
     (pBtt->pNamespace, ArenaOff + pBttInfo->InfoOffset, pBttInfo, sizeof(BTT_INFO));
  if(EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  FREE_POOL_SAFE(pBttInfo);
  return ReturnCode;
}

EFI_STATUS
BttSetArenaError(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena
  )
{
  return BttArenaSetFlag(pBtt, pArena, BTTINFO_FLAG_ERROR);
}

VOID
BttRelease(
  IN OUT BTT *pBtt
  )
{
  UINT8 Index = 0;
  NVDIMM_DBG("Bttp %p", pBtt);
  ASSERT(pBtt != NULL);

  if(pBtt) {
    if(pBtt->Arenas) {
      for(Index = 0; Index < pBtt->NArenas; Index++) {
        if(pBtt->Arenas[Index].pFlogs) {
          FreePool(pBtt->Arenas[Index].pFlogs);
        }
        if(pBtt->Arenas[Index].pRtt) {
          FreePool((UINT32 *)pBtt->Arenas[Index].pRtt);
        }
        //if(pBtt->Arenas[Index].BttMapLock) {
        //  FreePool(pBtt->Arenas[Index].BttMapLock);
        //}
      }
      FreePool(pBtt->Arenas);
    }
    FreePool(pBtt);
  }
}

UINT32
BttGetMapFromLba(
  IN     UINT32 Lba
  )
{
  return Lba / (BTT_MAP_LOCK_ALIGN / BTT_MAP_ENTRY_SIZE);
}

UINT8
BttGetPositionInMapFromLba(
  IN     UINT32 Lba
  )
{
  return Lba % (BTT_MAP_LOCK_ALIGN / BTT_MAP_ENTRY_SIZE);
}
