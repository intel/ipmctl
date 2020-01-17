/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _BTT_LAYOUT_H_
#define _BTT_LAYOUT_H_

#define BTT_ALIGNMENT 4096                    //!< Alignment of all BTT structures
#define BTTINFO_SIG_LEN 16                    //!< Signature length
#define BTT_PRIMARY_INFO_BLOCK_OFFSET 0           //!< BTT Info Block offset on first arena
#define BTT_PRIMARY_INFO_BLOCK_OFFSET_1_1 4096    //!< BTT Info Block offset on first arena for 1.1

#include <Dimm.h>


#pragma pack(push, 1)

/*!
    Layout of BTT Info block.
*/
typedef struct _BTT_INFO {
    UINT8 Sig [BTTINFO_SIG_LEN];            //!< must be "BTT_ARENA_INFO\0\0"
    GUID Uuid;                              //!< BTT UUID
    GUID ParentUuid;                        //!< UUID of container
    UINT32 Flags;                           //!< see flag bits below
    UINT16 Major;                           //!< major version
    UINT16 Minor;                           //!< minor version
    UINT32 ExternalLbaSize;                 //!< advertised LBA size (bytes)
    UINT32 ExternalNLbas;                   //!< advertised LBAs in this arena
    UINT32 InternalLbaSize;                 //!< size of data area blocks (bytes)
    UINT32 InternalNLbas;                   //!< number of blocks in data area
    UINT32 NFree;                           //!< number of free blocks
    UINT32 InfoSize;                        //!< size of this Info block

    /*!
        The following offsets are relative to the beginning of the BttInfo block.
    */
    UINT64 NextOffset;      //!< offset to next arena (or zero)
    UINT64 DataOffset;      //!< offset to arena data area
    UINT64 MapOffset;       //!< offset to area map
    UINT64 FlogOffset;      //!< offset to area flog
    UINT64 InfoOffset;      //!< offset to backup info block

    UINT8 Unused [3968];     //!< alignment to BTT_ALIGNMENT 4096

    UINT64 Checksum;        //!< Fletcher64 of all fields
}     BTT_INFO;     //!< @see _BTT_INFO

/*!
    Definitions for flags mask for BTT_INFO structure above.
*/
#define BTTINFO_FLAG_ERROR      0x00000001  //!< error state (read-only)
#define BTTINFO_FLAG_ERROR_MASK 0x00000001  //!< all error bits

/*!
    Layout of a BTT "Flog" entry.

    The "NoFree" field in the BTT Info block determines how many of these
    Flog Entries there are, and each entry consists of two of the following
    structs (entry updates alternate between the two structs), padded up
    to a cache line boundary to isolate adjacent updates.
*/
typedef struct _BTT_FLOG {
    UINT32 Lba;         //!< last pre-map LBA using this entry
    UINT32 OldMap;      //!< old post-map LBA (the freed block)
    UINT32 NewMap;      //!< new post-map LBA
    UINT32 Seq;         //!< Sequence number (01, 10, 11)
} BTT_FLOG; //!< @see _BTT_FLOG

/*!
    Padding to cache line boundary to isolate adjacent updates.
*/
#define BTT_FLOG_PAIR_ALIGN 64

/*!
    BTT Flog pair
*/
typedef struct _BTT_FLOG_PAIR {
    BTT_FLOG Flog [2];                                          //!< Flog Pair
    UINT8 unused [BTT_FLOG_PAIR_ALIGN - 2*sizeof (BTT_FLOG)];   //!< Padding to 64 bytes
} BTT_FLOG_PAIR; //!< @see _BTT_FLOG_PAIR

/*!
   Struct mapping between External and Internal LBA, padded to 64 bytes as that's the minimum chunk that can be saved via block apertures
*/
typedef struct _BTT_MAP_ENTRIES {
    UINT32 MapEntryLba [CACHE_LINE_SIZE / sizeof(UINT32)]; //!< Mapping between external to internal LBA
} BTT_MAP_ENTRIES;

/*!
    Layout of a BTT "map" entry. 4-byte internal LBA offset.
*/
#define BTT_MAP_ENTRY_ZERO (1u << 31)       //!< Zero'th bit
#define BTT_MAP_ENTRY_ERROR (1u << 30)      //!< Error bit
#define BTT_MAP_ENTRY_NORMAL (3u << 30)     //!< Normal map entry has both the zero and error bits set
#define BTT_MAP_ENTRY_LBA_MASK 0x3fffffff   //!< Lba mask
#define BTT_MAP_ENTRY_SIZE 4                //!< Size of Map Entry
#define BTT_MAP_LOCK_ALIGN 64               //!< Map alignment

/*!
    BTT layout properties...
*/
#define BTT_NAMESPACE_MIN_SIZE      MIB_TO_BYTES(16)     //!< Btt namespace minimal size = 16MiB
#define BTT_MAX_ARENA_SIZE          GIB_TO_BYTES(512ULL) //!< Arena maximum size = 512GiB
#define BTT_MIN_LBA_SIZE            512                  //!< Minimal Lba size = 512 bytes
#define BTT_INTERNAL_LBA_ALIGNMENT  256                  //!< Lba byte alignment
#define BTT_DEFAULT_NFREE           256                  //!< Default number of flog entries

#pragma pack(pop)
#endif //_BTT_LAYOUT_H_
