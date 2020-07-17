/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PCD_COMMON_H_
#define _PCD_COMMON_H_

#include <NvmTables.h>
#include <LbaCommon.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif

extern EFI_GUID gIntelDimmConfigVariableGuid;

#define PCD_TARGET_ALL         0
#define PCD_TARGET_CONFIG      1
#define PCD_TARGET_NAMESPACES  2

#define NVDIMM_CONFIGURATION_HEADER_SIG     SIGNATURE_32('D', 'M', 'H', 'D')
#define NVDIMM_CURRENT_CONFIG_SIG           SIGNATURE_32('C', 'C', 'U', 'R')
#define NVDIMM_CONFIGURATION_INPUT_SIG      SIGNATURE_32('C', 'I', 'N', '_')
#define NVDIMM_CONFIGURATION_OUTPUT_SIG     SIGNATURE_32('C', 'O', 'U', 'T')
#define NVDIMM_CONFIGURATION_HEADER_LOWEST_COMPATIBLE_REVISION 1
#define NVDIMM_CONFIGURATION_HEADER_OEM_ID "INTEL "
#define NVDIMM_CONFIGURATION_HEADER_OEM_ID_LEN 6
#define NVDIMM_CONFIGURATION_HEADER_OEM_TABLE_ID SIGNATURE_64('P', 'U', 'R', 'L', 'E', 'Y', ' ', ' ')
#define NVDIMM_CONFIGURATION_HEADER_OEM_REVISION 2
#define NVDIMM_CONFIGURATION_HEADER_CREATOR_ID SIGNATURE_32('I', 'N', 'T', 'L')
#define NVDIMM_CONFIGURATION_HEADER_CREATOR_REVISION 0
#define NVDIMM_CONFIGURATION_TABLES_REVISION_1       1
#define NVDIMM_CONFIGURATION_TABLES_REVISION_2       2
#define NVDIMM_CONFIGURATION_TABLES_MAJOR_REVISION_1 1
#define NVDIMM_CONFIGURATION_TABLES_MINOR_REVISION_1 1
#define NVDIMM_CONFIGURATION_TABLES_MINOR_REVISION_2 2
#define NVDIMM_CONFIGURATION_TABLES_MINOR_REVISION_3 3
#define NVDIMM_CONFIGURATION_TABLES_REVISION_DEFAULT NVDIMM_CONFIGURATION_TABLES_REVISION_2

#define IS_NVDIMM_CONFIGURATION_HEADER_REV_INVALID(table) ((table->Header.Revision.AsUint8 != NVDIMM_CONFIGURATION_TABLES_REVISION_1) && (table->Header.Revision.AsUint8 != NVDIMM_CONFIGURATION_TABLES_REVISION_2) && \
                                                          ((table->Header.Revision.Split.Major != NVDIMM_CONFIGURATION_TABLES_MAJOR_REVISION_1) || (table->Header.Revision.Split.Minor != NVDIMM_CONFIGURATION_TABLES_MINOR_REVISION_1)) && \
                                                          ((table->Header.Revision.Split.Major != NVDIMM_CONFIGURATION_TABLES_MAJOR_REVISION_1) || (table->Header.Revision.Split.Minor != NVDIMM_CONFIGURATION_TABLES_MINOR_REVISION_2)) && \
                                                          ((table->Header.Revision.Split.Major != NVDIMM_CONFIGURATION_TABLES_MAJOR_REVISION_1) || (table->Header.Revision.Split.Minor != NVDIMM_CONFIGURATION_TABLES_MINOR_REVISION_3)))

#define LSA_NAMESPACE_INDEX_SIG_L        SIGNATURE_64('N', 'A', 'M', 'E', 'S', 'P', 'A', 'C')
#define LSA_NAMESPACE_INDEX_SIG_H        SIGNATURE_64('E', '_', 'I', 'N', 'D', 'E', 'X', '\0')
#define LSA_NAMESPACE_INDEX_SIG          (LSA_NAMESPACE_INDEX_SIG_L | LSA_NAMESPACE_INDEX_SIG_H << 64)

/** PCAT table types **/
#define PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE         4
#define PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE        5

#define NVDIMM_MEMORY_PERSISTENT_TYPE         2
#define NVDIMM_MEMORY_PERSISTENT_CACHED_TYPE  3

#define PART_NUMBER_SIZE    20

#define NVDIMM_CONFIG_HEADER_CURRENT_SIZE_OFFSET  36
#define NVDIMM_CONFIG_HEADER_CURRENT_START_OFFSET 40
#define NVDIMM_CONFIG_HEADER_INPUT_SIZE_OFFSET    44
#define NVDIMM_CONFIG_HEADER_INPUT_START_OFFSET   48
#define NVDIMM_CONFIG_HEADER_OUTPUT_SIZE_OFFSET   52
#define NVDIMM_CONFIG_HEADER_OUTPUT_START_OFFSET  56

#define NVDIMM_TABLE_HEADER_REVISION               1
#define NVDIMM_TABLE_HEADER_OEM_TABLE_ID           0
#define NVDIMM_TABLE_HEADER_OEM_REVISION           2
#define NVDIMM_TABLE_HEADER_CREATOR_REVISION       1

#define NVDIMM_CONF_INPUT_PART_SIZE_CHANGE_STATUS  0

#define PCD_PARTITION_SIZE                           0x20000
#define PCD_OEM_PARTITION_INTEL_CFG_REGION_SIZE      0x10000
#define PCD_OEM_PARTITION_ID                         1
#define PCD_LSA_PARTITION_ID                         2

#define CONFIG_OUTPUT_STATUS_UNKNOWN                         0
#define CONFIG_OUTPUT_STATUS_SUCCESS                         1
#define CONFIG_OUTPUT_STATUS_ERROR                           2
#define CONFIG_OUTPUT_STATUS_NM_FM_RATIO_UNSUPPORTED         6
#define CONFIG_OUTPUT_STATUS_CPU_MAX_MEMORY_LIMIT_VIOLATION  7
#define CONFIG_OUTPUT_STATUS_POPULATION_ISSUE                8

#define PARTITION_SIZE_CHANGE_STATUS_UNDEFINED              0
#define PARTITION_SIZE_CHANGE_STATUS_SUCCESS                1
#define PARTITION_SIZE_CHANGE_STATUS_RESERVED               2
#define PARTITION_SIZE_CHANGE_STATUS_DIMM_MISSING           3
#define PARTITION_SIZE_CHANGE_STATUS_ISET_MISSING           4
#define PARTITION_SIZE_CHANGE_STATUS_EXCEED_SIZE            5
#define PARTITION_SIZE_CHANGE_STATUS_FW_ERROR               6
#define PARTITION_SIZE_CHANGE_STATUS_EXCEED_DRAM_DECODERS   7
#define PARTITION_SIZE_CHANGE_STATUS_UNSUPPORTED_ALIGNMENT  8

#define INTERLEAVE_INFO_STATUS_UNDEFINED              0
#define INTERLEAVE_INFO_STATUS_SUCCESS                1
#define INTERLEAVE_INFO_STATUS_NOT_PROCESSED          2
#define INTERLEAVE_INFO_STATUS_DIMM_MISSING           3
#define INTERLEAVE_INFO_STATUS_ISET_MISSING           4
#define INTERLEAVE_INFO_STATUS_EXCEED_DRAM_DECODERS   5
#define INTERLEAVE_INFO_STATUS_EXCEED_MAX_SPA_SPACE   6
#define INTERLEAVE_INFO_STATUS_MIRROR_FAILED          7
#define INTERLEAVE_INFO_STATUS_PARTITIONING_FAILED    8
#define INTERLEAVE_INFO_STATUS_CIN_MISSING            9
#define INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH      10
#define INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT  11
#define INTERLEAVE_INFO_STATUS_REQUEST_UNSUPPORTED    12

#define MAX_PCD_TABLE_STATUS_LENGTH 12

/** Tables below are stored on DIMMs **/
#pragma pack(push)
#pragma pack(1)
typedef struct {
  /**
    HEADER
  **/
  TABLE_HEADER Header; //!< Signature for this table: 'DMHD'
  /**
    BODY
  **/
  UINT32 CurrentConfDataSize;    //!< Size of data area allocated to the current configuration information in bytes
  UINT32 CurrentConfStartOffset; //!< Starting location of current configuration. Valid if the size is non-zero
  UINT32 ConfInputDataSize;      //!< Size of data area allocated to the configuration request area in bytes
  UINT32 ConfInputStartOffset;   //!< Starting location of configuration request data. Valid if the size is non-zero
  UINT32 ConfOutputDataSize;     //!< Size of data area allocated to the configuration response area in bytes
  UINT32 ConfOutputStartOffset;  //!< Starting location of configuration response data. Valid if the size is non-zero
} NVDIMM_CONFIGURATION_HEADER;

typedef struct {
  /**
    HEADER
  **/
  TABLE_HEADER Header; //!< Signature for this table: 'CCUR'
  /**
    BODY
  **/
  /**
    Status codes:
    00 - Undefined
    01 - DIMM is configured successfully
    02 - Reserved
    03 - All the DIMMs in the interleave set not found. Volatile memory is mapped to the SPA if possible
    04 - Persistent Memory not mapped due to matching Interleave set not found. Volatile memory is mapped to the SPA if possible
    05 - DIMM added to the system or moved within the system or DIMM is not yet configured
         Volatile memory is mapped to the SPA if possible. Current configuration present in the DIMM is not modified (Reserved)
    06 - New configuration input structures have errors, old configuration used. Refer to the config output structures
         for additional errors
    07 - New configuration input structures have errors. Volatile memory is mapped to the SPA if possible
         Refer to the config output structures for addition errors
    08 - Configuration Input Checksum not valid
    09 - Configuration Input data Revision is not supported
    10 - Current Configuration Checksum not valid
    11 - DCPMM is not mapped to SPA due to a health issue or configuration change
    12 - DCPMM persistent and volatile memory is not mapped due to a population issue
    13 - DCPMM volatile memory is not mapped since NM:FM ratio is not supported
    14 - DCPMM is not mapped due to a violation of the CPU maximum memory limit
    15 - DCPMM persistent memory mapped, but volatile memory is not mapped due to a population issue
  **/
  UINT16 ConfigStatus;
  UINT8 Reserved[2];
  UINT64 VolatileMemSizeIntoSpa;   //!< Total amount of 2LM size from the DIMM mapped into the SPA
  UINT64 PersistentMemSizeIntoSpa; //!< Total amount of PM size from DIMM mapped into the SPA
  /**
    A list of PCAT table structures
  **/
  VOID *pPcatTables[0];
} NVDIMM_CURRENT_CONFIG;

typedef enum {
  Undefined = 0,
  Success = 1,
  Reserved = 2,
  NotAllDimmsInInterleaveFound = 3,
  MatchinInterleaveNotFound = 4,
  PartitionSizeExceedsSizeInput = 5,
  FwError = 6,
  InsufficientDramDecoders = 7,
  PersistentMemoryNotAligned = 8
} PartitionSizeChangeStatusCode;

typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 4
  /**
    BODY
  **/
  /**
    Valid only on the config output structure.
    Byte0:
      0 - Undefined
      1 - Success
      2 - Reserved
      3 - All the DIMMs in the interleave set not found
      4 - Matching Interleave set not found (Matching DIMMs found)
      5 - Total partition size defined in the interleave set exceeds the partition size input
      6 - DIMM FW returned error
      7 - Insufficient number of DRAM Decoders available to map all the DIMMs in the interleave set.
          Repartition not performed.
      8 - Persistent memory partition size is not aligned to the Interleave Alignment Size specified
          in the Interface Capability Information Table
    Byte1:
      DIMM FW Error Response Code. Only valid boot time records, runtime validation will not invoke DIMM FW
      to get the data
  **/
  UINT32 PartitionSizeChangeStatus;
  /**
    Size of persistent memory partition in bytes. This size must be aligned to "Interleave Alignment Size" specified
    in the "Interface Capability Information" table.
  **/
  UINT64 PmPartitionSize;
} NVDIMM_PARTITION_SIZE_CHANGE;

/**
  Interleave change or current interleave information
**/
typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 5
  /**
    BODY
  **/
  UINT16 InterleaveSetIndex; //!< Logical index number, it should same for all the DIMMs in the interleave set
  /**
    Total number of DIMMs participating in this interleave set. DIMM Info structure at the end of this structure
    is repeated for each DIMM in the interleave set
  **/
  UINT8 NumOfDimmsInInterleaveSet;
  /**
    1 - Reserved
    2 - App Direct PM (Persistent)
    3 - Reserved
  **/
  UINT8 InterleaveMemoryType;
  /**
    Byte0 - Channel Interleave Size
    Byte1 - iMC Interleave Size
    One of the supported values from PCAT Mmeory Interleave Capability Info Table
  **/
  UINT8 InterleaveFormatChannel;
  UINT8 InterleaveFormatImc;
  UINT8 Reserved1[3];
  /**
    Config Input:
    Reserved zero

    Config Output:
    0  - Information not processed
    1  - Successfully interleaved the request
    2  - Information not processed due to an error
    3  - Unable to find matching DIMMs in the interleave set
    4  - Matching DIMMs found, but interleave information does not match
    5  - Insufficient number of DRAM Decoders available to map all the DIMMs in the interleave set.
         This interleave set may not be mapped to system address space
    6  - Memory mapping failed due to unavailable system address space
    7  - Reserved
    8  - Partitioning request failed
    9  - Matching DIMMs found, but CIN missing in a DIMM in the interleave set
    10 - Channel interleave does not match between the MCs being interleaved
    11 - Partition Offset or Size is not a multiple of Interleave Aligment Size in Memory Interleave
         Capability Information sub-table of PCAT
    12 - Request unsuppported
  **/
  UINT8 InterleaveChangeStatus;
  UINT8 Reserved2[10];
  VOID *pIdentificationInfoList[0];  //!< DIMM interleave set information
} NVDIMM_INTERLEAVE_INFORMATION3;

/**
  Interleave change or current interleave information
**/
typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 5
  /**
    BODY
  **/
  UINT16 InterleaveSetIndex; //!< Logical index number, it should same for all the DIMMs in the interleave set
  /**
    Total number of DIMMs participating in this interleave set. DIMM Info structure at the end of this structure
    is repeated for each DIMM in the interleave set
  **/
  UINT8 NumOfDimmsInInterleaveSet;
  /**
    1 - 2LM (Volatile)
    2 - App Direct PM (Persistent)
    3 - Reserved
  **/
  UINT8 InterleaveMemoryType;
  /**
    Interleave format to be used for this interleave set. One of value from Interleave Format List table
  **/
  UINT8 InterleaveFormatChannel;
  UINT8 InterleaveFormatImc;
  UINT16 InterleaveFormatWays;
  /**
    Config Input:
    0 - Disable Mirror for this interleave set
    1 - Enable Mirror for this interleave set

    Config Output:
    0 - Mirror Disabled for this interleave set
    1 - Mirror Enabled for this interleave set
  **/
  UINT8 MirrorEnable;
  /**
    Config Input:
    Reserved zero

    Config Output:
    0 - Information not processed
    1 - Successfully interleaved the request
    2 - Unable to find matching DIMMs in the interleave set
    3 - Matching DIMMs found, but interleave information does not match
    4 - Insufficient number of DRAM Decoders available to map all the DIMMs in the interleave set.
        This interleave set may not be mapped to system address space
    5 - Memory mapping failed due to unavailable system address space
    6 - Mirror mapping failed due to unavailable resources
  **/
  UINT8 InterleaveChangeStatus;
  UINT8 MemorySpare;
  UINT8 Reserved[9];
  VOID *pIdentificationInfoList[0];  //!< DIMM interleave set information
} NVDIMM_INTERLEAVE_INFORMATION;

typedef struct _DIMM_UNIQUE_IDENTIFIER {
  UINT16 ManufacturerId;                   //!< Indicates the manufacturer of the module
  UINT8 ManufacturingLocation;             //!< Unique manufacturing location of the module
  UINT16 ManufacturingDate;                //!< Indicates manufacturing year and week
  UINT32 SerialNumber;                     //!< Unique serial number for the module
} DIMM_UNIQUE_IDENTIFIER;

typedef union {
  UINT64 AsUint64;
  struct {
    UINT32 SocketId        :8;
    UINT32 DieId           :8;
    UINT32 MemControllerId :8;
    UINT32 ChannelId       :8;
    UINT32 SlotId          :8;
    UINT32 Reserved        :24;
  }Split;
} DIMM_LOCATION;

typedef struct {
  DIMM_UNIQUE_IDENTIFIER DimmIdentification;
  /**
   Byte0 - Socket ID
   Byte1 - Die ID
   Byte2 - iMC ID
   Byte3 - Channel ID
   Byte4 - Slot ID
   Byte[5-7] - Reserved
 **/
  DIMM_LOCATION DimmLocation;
  UINT8 Reserved[15];
  /**
    Logical offset from the base of the partition type in bytes.
    DPA = FW returned DPA for partition + offset
  **/
  UINT64 PartitionOffset;
  UINT64 PmPartitionSize;         //!< Size in bytes contributed by this DIMM for this interleave set
} NVDIMM_IDENTIFICATION_INFORMATION3;

typedef struct {
  union {
    struct {
      UINT16 DimmManufacturerId;                //!< Manufacturer ID string from DIMM
      UINT32 DimmSerialNumber;                  //!< Serial number string from DIMM
      CHAR8 DimmPartNumber[PART_NUMBER_SIZE];   //!< Dimm part number
      UINT8 Reserved[6];
    } Version1;
    struct {
      DIMM_UNIQUE_IDENTIFIER Uid;
      UINT8 Reserved[23];
    } Version2;
  }DimmIdentification;
  /**
    Logical offset from the base of the partition type in bytes.
    DPA = FW returned DPA for partition + offset
  **/
  UINT64 PartitionOffset;
  UINT64 PmPartitionSize;         //!< Size in bytes contributed by this DIMM for this interleave set
} NVDIMM_IDENTIFICATION_INFORMATION;

typedef struct {
  /**
    HEADER
  **/
  TABLE_HEADER Header; //!< Signature for this table: 'CIN_'
  /**
    BODY
  **/
  /**
    Request sequence number. The BIOS copies this sequence number to the response structure once the BIOS
    completes processing the request input
  **/
  UINT32 SequenceNumber;
  UINT8 Reserved[8];
  /**
    A list of PCAT table structures
  **/
  VOID *pPcatTables[0];
} NVDIMM_PLATFORM_CONFIG_INPUT;

typedef struct {
  /**
    HEADER
  **/
  TABLE_HEADER Header; //!< Signature for this table: 'COUT'
  /**
    BODY
  **/
  /**
    Copy of the configure input structure sequence number to indicate that the BIOS has completed processing
    input structure
  **/
  UINT32 SequenceNumber;
  /**
    BIOS response after config input processing
    0 - Undefined
    1 - Config Change applied successfully
    2 - Boot time processing complete, errors found. Refer to individual records for error details
    6 - Goal not applied because the NM:FM ratio is not supported
    7 - Goal not applied due to a violation of the CPU maximum memory limit
    8 - Goal not applied due to a population issue
    **/
  UINT8 ValidationStatus;
  UINT8 Reserved[7];
  /**
    A list of PCAT table structures
  **/
  VOID *pPcatTables[0];
} NVDIMM_PLATFORM_CONFIG_OUTPUT;

#pragma pack(pop)

typedef struct _DIMM_PCD_INFO {
  UINT32 DimmId;
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  NVDIMM_CONFIGURATION_HEADER *pConfHeader;
  LABEL_STORAGE_AREA *pLabelStorageArea;
} DIMM_PCD_INFO;

/**
  Free dimm PCD info array

  @param[in, out] pDimmPcdInfo Pointer to output array of PCDs
  @param[in, out] DimmPcdInfoCount Number of items in Dimm PCD Info
**/
VOID
FreeDimmPcdInfoArray(
  IN OUT DIMM_PCD_INFO *pDimmPcdInfo,
  IN OUT UINT32 DimmPcdInfoCount
  );

#define GET_NVDIMM_CURRENT_CONFIG(NvdimmConfHeader) ((NVDIMM_CURRENT_CONFIG *) \
    GET_VOID_PTR_OFFSET(NvdimmConfHeader, NvdimmConfHeader->CurrentConfStartOffset))
#define GET_NVDIMM_PLATFORM_CONFIG_INPUT(NvdimmConfHeader) ((NVDIMM_PLATFORM_CONFIG_INPUT *) \
    GET_VOID_PTR_OFFSET(NvdimmConfHeader, NvdimmConfHeader->ConfInputStartOffset))
#define GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(NvdimmConfHeader) ((NVDIMM_PLATFORM_CONFIG_OUTPUT *) \
    GET_VOID_PTR_OFFSET(NvdimmConfHeader, NvdimmConfHeader->ConfOutputStartOffset))

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
