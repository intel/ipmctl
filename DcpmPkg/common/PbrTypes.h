/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PBR_TYPES_H_
#define _PBR_TYPES_H_
#include <Types.h>

//Generic PBR defines
#define PBR_NORMAL_MODE                       0x0
#define PBR_RECORD_MODE                       0x1
#define PBR_PLAYBACK_MODE                     0x2

#define GET_NEXT_DATA_INDEX                   -1
#define MAX_PARTITIONS                        100
#define MAX_TAG_NAME                          256
#define INVALID_TAG_ID                        0xFFFFFFFF
#define PARTITION_GROW_SZ_MULTIPLIER          10

#define PBR_SW_VERSION_MAX                    25
#define PBR_OS_NAME_MAX                       100
#define PBR_OS_VERSION_MAX                    100
#define PBR_FILE_DESCRIPTION_MAX              1024

#define PBR_INVALID_SIG                       0
#define PBR_LOGICAL_DATA_SIG                  SIGNATURE_32('P', 'B', 'L', 'D')
#define PBR_HEADER_SIG                        SIGNATURE_32('P', 'B', 'R', 'H')
#define PBR_TAG_HEADER_SIG                    SIGNATURE_32('P', 'B', 'T', 'H')
#define PBR_TAG_SIG                           SIGNATURE_32('P', 'B', 'T', 'I')


/**set playback/record/normal mode**/
#define PBR_SET_MODE(ctx, mode) \
  (ctx)->PbrMode = mode

/**get current playback/record/normal mode**/
#define PBR_GET_MODE(ctx) \
  (ctx)->PbrMode

/**obtain pointer to the playback/recording module context**/
#define PBR_CTX() \
  &gPbrContext

#pragma pack(push)
#pragma pack(1)
/**part of the main PbrContext struct, used to keep track on individual data partitions in the session**/
typedef struct _PbrPartitionContext {
  UINT32 PartitionSig;                                        //!< Unique identifier that categorizes a set of data
  UINT32 PartitionSize;                                       //!< Size in bytes of the partition
  UINT32 PartitionLogicalDataCnt;                             //!< How many logical data items exist in the partition
  UINT32 PartitionCurrentOffset;                              //!< Offset used to keep track of current position when in record or playback mode
  UINT32 PartitionEndOffset;                                  //!< Placeholder
  VOID  *PartitionData;                                       //!< Pointer to actual data item
}PbrPartitionContext;

/**the main pbr context that contains pointers to various data structures**/
typedef struct _PbrContext {
  UINT32 PbrMode;                                             //!< PBR_NORMAL_MODE, PBR_RECORD_MODE, PBR_PLAYBACK_MODE
  VOID  *PbrMainHeader;                                       //!< Main PBR buffer header, includes partition table
  PbrPartitionContext PartitionContexts[MAX_PARTITIONS];
}PbrContext;

/**entries in the partition table**/
typedef struct _PbrPartitionTableEntry {
  UINT32 Signature;                                           //!< Defines the type of partition
  UINT32 Size;                                                //!< Size of partition including the partition header
  UINT32 Offset;                                              //!< Offset of the partition within a fully stitched pbr image
  UINT32 LogicalDataCnt;                                      //!< Number of logical data items within a partition
}PbrPartitionTableEntry;

typedef struct _PbrPartitionLogicalDataItem {
  UINT32 Signature;                                           //!< PBR_LOGICAL_DATA_SIG
  UINT32 Size;                                                //!< Size of Data in bytes
  UINT32 LogicalIndex;                                        //!< Index within the partition
  UINT8 Data[];                                               //!< Start of actual data
}PbrPartitionLogicalDataItem;

/**partition table, which describes location of each partition**/
typedef struct _PbrPartitionTable {
  PbrPartitionTableEntry Partitions[MAX_PARTITIONS];
}PbrPartitionTable;

/**main pbr header that includes the partition table**/
typedef struct _PbrHeader {
  UINT32              Signature;                              //!< PBR_HEADER_SIG
  PbrPartitionTable   PartitionTable;                         //!< Partition table, describes partition locations within a stitched img
  CHAR8               SwVersion[PBR_SW_VERSION_MAX];          //!< SW/Driver version used to record data
  CHAR8               OsVersion[PBR_OS_VERSION_MAX];          //!< Execution OS, i.e. UEFI/Linux/Windows
  CHAR8               OsName[PBR_OS_NAME_MAX];                //!< Execution OS name
  CHAR8               Description[PBR_FILE_DESCRIPTION_MAX];  //!< Highlevel description of pbr file
}PbrHeader;

/**tag data struct that is used within the tag partition**/
typedef struct _Tag {
  UINT32 Signature;                                           //!< PBR_TAG_SIG
  UINT32 TagSignature;                                        //!< type of tag
  UINT32 TagId;                                               //!< Unique ID
  UINT32 TagSize;                                             //!< Size of the tag that follows this data structure
  UINT32 PartitionInfoCnt;                                    //!< The number of partition info items that follows this struct
}Tag;

typedef struct _TagPartitionInfo {
  UINT32 PartitionSignature;                                  //!< Signature of the partition
  UINT32 PartitionCurrentOffset;                              //!< Playback or Recording offset of the partition
}TagPartitionInfo;

extern PbrContext gPbrContext;                                //!< extern global context
#pragma pack(pop)
#endif //_PBR_TYPES_H_
