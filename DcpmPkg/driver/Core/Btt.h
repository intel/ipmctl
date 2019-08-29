/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BTT_H__
#define __BTT_H__

#include "BttLayout.h"

extern GUID gBttAbstractionGuid;

/* Flog pair indices */
#define FLOG_0 0  //!< 0th Flog entry
#define FLOG_1 1  //!< 1st Flog entry

#define FLOG_PAIR_0 0  //!< 0th Flog pair

#define EFI_BTT_ABSTRACTION_GUID \
  { 0x18633BFC, 0x1735, 0x4217, {0x8A, 0xC9, 0x17, 0x23, 0x92, 0x82, 0xD3, 0xF8} }

#define HOW_MANY(x, y) ((((x) % (y)) == 0) ? ((x) / (y)) : (((x) / (y)) + 1))
//!< Macro defining how many objects of the size x is required to store object of the size y

#define SET_BIT(a,i) ((a)[(i)/8] |= 1<<((i)%8))                 //!< Sets i'th bit in 'a' byte array
#define CLR_BIT(a,i) ((a)[(i)/8] &= ~(1<<((i)%8)))              //!< Clears i'th bit in 'a' byte array
#define IS_BIT_SET(a,i) ((a)[(i)/8] & (1<<((i)%8)))             //!< Checks if i'th bit in 'a' byte array is set
#define IS_BIT_CLEARED(a,i) (((a)[(i)/8] & (1<<((i)%8))) == 0)  //!< Checks if i'th bit in 'a' byte array is cleared

/**
    Converts Lba into BTT_MAP_ENTRIES number

    @retval Number of BTT_MAP_ENTRIES structure within MAP region

    @param [in] Lba Block for which output is calculated
**/
#define BTT_GET_MAP_FROM_LBA(Lba) (Lba / (BTT_MAP_LOCK_ALIGN / BTT_MAP_ENTRY_SIZE))

/**
    Converts Lba into position within BTT_MAP_ENTRIES structure

    @retval Position of BTT map within BTT_MAP_ENTRIES structure

    @param [in] Lba Block for which output is calculated
**/
#define BTT_GET_POSITION_IN_MAP_FROM_LBA(Lba) (Lba % (BTT_MAP_LOCK_ALIGN / BTT_MAP_ENTRY_SIZE))

/**
    Structure for keeping Flog Entries in runtime
**/
typedef struct _FLOG_RUNTIME {
    BTT_FLOG_PAIR FlogPair;     //!< current Info
    UINT64 Entry;               //!< offset for Flog pair
    UINT8 Next;                 //!< Next write (0 or 1)
} FLOG_RUNTIME; //!< @see _FLOG_RUNTIME

/**
    Structure for keeping Arena Informations
**/
typedef struct _ARENAS {
    UINT32 Flags;           //!< arena Flags (btt_Info)
    UINT32 ExternalNLbas;   //!< Advertised number of LBAs in this arena.
    UINT32 InternalLbaSize; //!< Internal LBA size. Each block in the arena data area is this size in bytes.
                            //!< This may be larger than the ExternalLbaSize due to alignment padding between LBAs.
    UINT32 InternalNLbas;   //!< Number of blocks in the arena data area

    /**
        The following offsets are relative to the beginning of
        the encapsulating namespace.  This is different from
        how these offsets are stored on-media, where they are
        relative to the start of the arena.  The offset are
        converted by BttReadLayout() to make them more convenient
        for run-time use.
    **/
    UINT64 StartOffset;    //!< offset to start of arena
    UINT64 DataOffset;     //!< offset to arena data area
    UINT64 MapOffset;      //!< offset to area map
    UINT64 FlogOffset;     //!< offset to area Flog
    UINT64 NextOffset;     //!< offset to Next arena

    /**
        Run-time Flog state.
        The write path uses the Flog to find the free block
        it writes to before atomically making it the new
        active block for an external LBA.
        The read path doesn't use the Flog at all.
    **/
    FLOG_RUNTIME *pFlogs;

    /**
        Read tracking table.
        Before using a free block found in the Flog, the write path
        scans the Rtt to see if there are any outstanding reads on
        that block (reads that started before the block was freed by
        a concurrent write).  Unused slots in the Rtt are indicated
        by setting the error bit, BTT_MAP_ENTRY_ERROR, so that the
        entry won't match any post-map LBA when checked.
    **/
    UINT32 volatile *pRtt;
} ARENAS;     //!< @see _ARENAS

/**
    The opaque btt handle containing state tracked by this module
    for the btt namespace.  This is created by BttInit(), handed to
    all the other btt_* entry points, and deleted by BttRelease().
**/
typedef struct _BTT {
    /**
      The Laidout flag indicates whether the namespace contains valid BTT
      metadata.  It is initialized by BttReadLayout() and if no valid layout
      is found, all reads return zeros and the first write will write the
      BTT layout.
    **/
    BOOLEAN Laidout;

    /**
      Number of concurrent threads allowed per btt
    **/
    UINT32 NLanes;

    /**
      UUID of the BTT
    **/
    GUID Uuid;

    /**
      UUID of the containing namespace, used to validate BTT metadata.
    **/
    GUID ParentUuid;

    /**
      Parameters controlling/describing the BTT layout.
    **/
    UINT64 RawSize;              //!< Size of containing namespace
    UINT32 LbaSize;              //!< External LBA size
    UINT32 InternalLbaSize;      //!< Internal LBA size, physical block size for the Windows
    UINT32 NFree;               //!< Available Flog Entries
    UINT64 NLbas;               //!< Total number of external LBAs
    UINT32 NArenas;             //!< Number of Arenas
    UINT64 PrimaryInfoOffset;    //!< BTT Info Block offset on first arena

    /**
      Run-time state kept for each arena
    **/
    ARENAS *Arenas;
    // This pointer is VOID instead of NAMESPACE to avoid includes looping
    VOID *pNamespace; // The pointer to the containing namespace for the IO operations
} BTT;       //!< @see _BTT

/**
    Signature for arena Info blocks.  Total size is 16 bytes, including
    the '\0' added to the string by the declaration (the last two bytes
    of the string are '\0').
**/
static const char Sig [] = "BTT_ARENA_INFO\0";

/**
    Lookup table of Sequence numbers.
    These are the 2-bit numbers that cycle between 01, 10, and 11.
**/
static const unsigned NSeq [] = {0, 2, 3, 1};

#define NSEQ(Seq) (NSeq[(Seq) & 3])
//!< Macro for looking up Sequence numbers.\n
//!< To advance a Sequence number to the Next number, use something like:\n
//!<     Seq = NSEQ (Seq);

/**
    Validates btt info block

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pInfo Info block to be validated
    @param [in] pBtt BTT to be compared with existing metadata
**/
EFI_STATUS
BttReadInfo(
  IN     BTT_INFO *pInfo,
  IN     BTT      *pBtt
  );

/**
    Prepare a btt namespace for use, returning an opaque handle

    When submitted a pristine namespace it will be formatted implicitly when
    touched for the first time.

    If arenas have different NFree values, we will be using the lowest one
    found as limiting to the overall "bandwidth".

    @retval PBtt namespace handle, NULL on error

    @param [in] RawSize Size of btt namespace being created
    @param [in] LbaSize Size of a block in a created namespace
    @param [in] ParentUuid[] UUID label of the namespace
    @param [in] pNamespace pointer to the BTTs parent namespace
    @param [in] pIsBttInitialized pointer to the flag if the BTT was initialized before.
**/
BTT *
BttInit(
  IN     UINT64 RawSize,
  IN     UINT32 LbaSize,
  IN     GUID *pParentUuid,
  IN     VOID *pNamespace
  );

/**
    Writes out the initial btt metadata layout

    Called with Write == TRUE only once in the life time of a btt namespace, when
    the first write happens.  The caller of this routine is responsible for
    locking out multiple threads.  This routine doesn't read anything -- by the
    time it is called, it is known there's no layout in the namespace and a new
    layout should be written.

    Calling with Write == FALSE tells this routine to do the calculations for
    Bttp->NArenas and Bttp->NLbas, but don't write out any metadata.

    If successful, sets Bttp->Laidout to 1.
    Otherwise Bttp->Laidout remains 0 so that later attempts to write will try again to create the layout.

    @retval EFI_SUCCESS if the routine succeeds

    @param [in] pBtt namespace handle
    @param [in] Write Switch informing whether calculated metadata should be written
**/
EFI_STATUS
BttWriteLayout(
  IN     BTT *pBtt,
  IN     BOOLEAN Write
  );

/**
  Read a block from a btt namespace

  @retval EFI_SUCCESS if the routine succeeds

  @param [in] pBtt namespace handle
  @param [in] Lba Logical block address to be read
  @param [out] pBuffer Read result Buffer pointer
**/
EFI_STATUS BttRead (
  IN     BTT* pBtt,
  IN     UINT64 Lba,
     OUT VOID* pBuffer
  );

/**
  Writes a block to a btt namespace

  @retval EFI_SUCCESS if the routine succeeds

  @param [in] pBtt namespace handle
  @param [in] Lba Logical block address to be written
  @param [in] pBuffer Buffer pointer to the block to be written
**/
EFI_STATUS BttWrite (
  IN     BTT* pBtt,
  IN     UINT64 Lba,
  IN     VOID* pBuffer
  );

/**
  Deletes opaque Btt_Info, done using btt namespace

  @param [in,out] pBtt namespace handle
**/
VOID BttRelease (
  IN OUT BTT* pBtt
  );

/**
  Marks a block as in an error state in a btt namespace

  @retval EFI_SUCCESS if the routine succeeds

  @param [in] pBtt namespace handle
  @param [in] Lba Logical block address
**/
EFI_STATUS BttSetError (
  IN     BTT* pBtt,
  IN     UINT32 Lba
  );

/**
  Updates the given flag for the arena info block

  @retval EFI_SUCCESS if the routine succeeds

  @param [in] pBtt Namespace handle
  @param [in] pArena Pointer to the Arena
  @param [in] SetFlag Flag to be set
**/
EFI_STATUS
BttArenaSetFlag(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena,
  IN     UINT32 SetFlag
  );

/**
  Set the error flag for the given arena

  @retval EFI_SUCCESS if the routine succeeds

  @param [in] pBtt Namespace handle
  @param [in] pArena Pointer to the Arena
**/
EFI_STATUS
BttSetArenaError(
  IN     BTT *pBtt,
  IN OUT ARENAS *pArena
  );

UINT32
BttGetMapFromLba(
  IN     UINT32 Lba
  );

UINT8
BttGetPositionInMapFromLba(
  IN     UINT32 Lba
  );

#endif //__BTT_H__
