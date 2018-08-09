/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <NvmTypes.h>

#ifndef _LBA_COMMON_H_
#define _LBA_COMMON_H_

/**
  Namespace Index structure defines
**/
#define NSINDEX_ALIGN       256
#define NSINDEX_FREE_ALIGN  8
#define NSINDEX_SIG_LEN     16
#define NSINDEX_MAJOR       1
#define NSINDEX_MINOR_1     1
#define NSINDEX_MINOR_2     2

#define INDEX_LABEL_SIZE_TO_BYTE(a)    (128 << (a))
#define BYTE_TO_INDEX_LABEL_SIZE(a)    ((a) >> 8)
#define LABELS_TO_FREE_BYTES(a)       ((a) >> 3)
#define FREE_BLOCKS_TO_LABELS(a)       ((a) << 3)

#define FREE_BLOCKS_MASK_ALL_SET 0xFF

#define FIRST_INDEX_BLOCK  0
#define SECOND_INDEX_BLOCK 1
#define ALL_INDEX_BLOCKS   2

#define SLOT_UNKNOWN  0
#define SLOT_FREE     1
#define SLOT_USED     2

#pragma pack(push)
#pragma pack(1)

typedef union {
  struct {
    UINT32 ReadOnly : 1;   //!< Read-only label
    UINT32 Local : 1;      //!< DIMM-local namespace
    UINT32 Reserved : 1;   //!< Reserved
    UINT32 Updating : 1;   //!< Label being updated
    UINT32 Reserved2 : 28;  //!< Reserved
  } Values;
  UINT32 AsUint32;
} LABEL_FLAGS;

/**
  Namespace Index structure definition
**/
typedef struct {
  CHAR8 Signature[NSINDEX_SIG_LEN]; //!< Must be "NAMESPACE_INDEX\0"
  UINT8 Flags[3];                   //!< see flag bits below
  UINT8 LabelSize;                  //!< Size of each label in bytes in 128B
  UINT32 Sequence;                  //!< Sequence number for this index
  UINT64 MyOffset;                  //!< Offset of this index in label area
  UINT64 MySize;                    //!< Size of this index struct
  UINT64 OtherOffset;               //!< Offset of other index
  UINT64 LabelOffset;               //!< Offset of first label slot
  UINT32 NumberOfLabels;            //!< Total number of label slots
  UINT16 Major;                     //!< Label area major version
  UINT16 Minor;                     //!< Label area minor version
  UINT64 Checksum;                  //!< Fletcher64 of all fields
  /**
    The size of Free[] is rounded up so the total struct size is
    a multiple of NSINDEX_ALIGN bytes. Any bits this allocates
    beyond nlabel bits must be zero.
  **/
  UINT8 *pFree;                     //!< NumbeOfLabels bits to map free slots
  UINT8 *pReserved;                 //!< Padding
} NAMESPACE_INDEX;

/**
  Namespace Label structure defines
**/
#define NSLABEL_NAME_LEN 63

/**
  Namespace Label structure definition
**/
typedef struct {
  GUID Uuid;                                   //!< UUID per RFC 4122
  CHAR8 Name[NLABEL_NAME_LEN_WITH_TERMINATOR]; //!< Optional name (NULL-terminated)
  LABEL_FLAGS Flags;
  UINT16 NumberOfLabels;                       //!< Number of labels to describe this namespace
  UINT16 Position;                             //!< Labels position in set
  UINT64 InterleaveSetCookie;                  //!< Interleave set cookie
  UINT64 LbaSize;                              //!< LBA size in bytes or 0 for PMEM
  UINT64 Dpa;                                  //!< DPA of NVM range on this DIMM
  UINT64 RawSize;                              //!< Size of namespace
  UINT32 Slot;                                 //!< Slot of this label in label area
  UINT32 Unused;                               //!< Must be zero
} NAMESPACE_LABEL_1_1;

typedef struct {
  GUID Uuid;                                   //!< UUID per RFC 4122
  CHAR8 Name[NLABEL_NAME_LEN_WITH_TERMINATOR]; //!< Optional name (NULL-terminated)
  LABEL_FLAGS Flags;
  UINT16 NumberOfLabels;                       //!< Number of labels to describe this namespace
  UINT16 Position;                             //!< Labels position in set
  UINT64 InterleaveSetCookie;                  //!< Interleave set cookie
  UINT64 LbaSize;                              //!< LBA size in bytes or 0 for PMEM
  UINT64 Dpa;                                  //!< DPA of NVM range on this DIMM
  UINT64 RawSize;                              //!< Size of namespace
  UINT32 Slot;                                 //!< Slot of this label in label area
  UINT8 Alignment;                             //!< Advertise the preferred alignment of the data
  UINT8 Reserved[3];                           //!< Zero
  GUID TypeGuid;                               //!< Describe the acccess mechanism for the DPA range
  GUID AddressAbstractionGuid;                 //!< Identifies the address abstraction mechanism for this namespace
  UINT8 Reserved1[88];                         //!< Zero
  UINT64 Checksum;                             //!< Fletcher64

} NAMESPACE_LABEL; // 256B

typedef struct {
  NAMESPACE_INDEX Index[NAMESPACE_INDEXES];
  NAMESPACE_LABEL *pLabels;
} LABEL_STORAGE_AREA;

#pragma pack(pop)

/**
  Get current Namespace Index Id.

  Only one of two Namespace Indexes is valid at time. This function checks sequence
  numbers of both Indexes and returns current Index Id.

  @param[in] pLabelStorageArea Pointer to a LSA structure
  @param[out] pCurrentIndex Current index position in LSA structure

  @retval EFI_SUCCESS Current Index position found
  @retval EFI_INVALID_PARAMETER NULL pointer parameter provided
**/
EFI_STATUS
GetLsaIndexes(
  IN     LABEL_STORAGE_AREA *pLsa,
     OUT UINT16 *pCurrentIndex,
     OUT UINT16 *pNextIndex
  );

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
  );

/**
   Print Namespace Index

   @param[in] pNamespaceIndex Namespace Index
**/
VOID
PrintNamespaceIndex(
  IN     NAMESPACE_INDEX *pNamespaceIndex
  );

/**
   Print Namespace Label

   @param[in] pNamespaceLabel Namespace Label
**/
VOID
PrintNamespaceLabel(
  IN     NAMESPACE_LABEL *pNamespaceLabel
  );

/**
   Print Label Storage Area and all subtables

   @param[in] pLba Label Storage Area
**/
VOID
PrintLabelStorageArea(
  IN     LABEL_STORAGE_AREA *pLba
  );

VOID FreeLsaSafe(
  IN    LABEL_STORAGE_AREA **ppLabelStorageArea
  );
#endif
