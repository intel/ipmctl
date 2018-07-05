/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVMTABLES_H_
#define _NVMTABLES_H_

#include <Types.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif

/**
  NVDIMM Firmware Interface Table (NFIT) types
**/

#define NVDIMM_SPA_RANGE_TYPE                 0
#define NVDIMM_NVDIMM_REGION_TYPE             1
#define NVDIMM_INTERLEAVE_TYPE                2
#define NVDIMM_SMBIOS_MGMT_INFO_TYPE          3
#define NVDIMM_CONTROL_REGION_TYPE            4
#define NVDIMM_BW_DATA_WINDOW_REGION_TYPE     5
#define NVDIMM_FLUSH_HINT_TYPE                6
#define NVDIMM_PLATFORM_CAPABILITIES_TYPE     7

/** Fields offsets in structures **/
#define NFIT_TABLE_HEADER_LENGTH_OFFSET              2
#define NFIT_TABLE_HEADER_LENGTH_FIELD_SIZE          2
#define PCAT_TABLE_HEADER_CHECKSUM_OFFSET            9
#define PCAT_TABLE_HEADER_LENGTH_OFFSET              2
#define PCAT_TABLE_HEADER_LENGTH_FIELD_SIZE          2

/** PCAT table types **/
#define PCAT_TYPE_PLATFORM_CAPABILITY_INFO_TABLE      0
#define PCAT_TYPE_INTERLEAVE_CAPABILITY_INFO_TABLE    1
#define PCAT_TYPE_RUNTIME_INTERFACE_TABLE             2
#define PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE  3
#define PCAT_TYPE_SOCKET_SKU_INFO_TABLE               6

#define NUMBER_OF_CHANNEL_WAYS_BITS_NUM  9

#define PCAT_HEADER_REVISION_1 1
#define PCAT_HEADER_REVISION_2 2

/** PMTT table types**/
#define	PMTT_MAX_LEN	4096
#define PMTT_TYPE_SOCKET 0
#define PMTT_TYPE_iMC 1
#define PMTT_TYPE_MODULE 2
#define PMTT_COMMON_HDR_LEN 8
#define PMTT_DDR_DCPMEM_FLAG BIT2

/** NFIT Tables structures **/
#pragma pack(push)
#pragma pack(1)
typedef struct {
  UINT16 Type;
  UINT16 Length;
} SubTableHeader;

/** Tables below are stored in ACPI tables by BIOS **/

typedef struct {
  UINT32 Signature;
  UINT32 Length;    //!< Length in bytes for entire table. It implies the number of Entry fields at the end of the table
  UINT8 Revision;
  UINT8 Checksum;         //!< Entire table must sum to zero
  UINT8 OemId[6];
  UINT64 OemTableId;      //!< the table ID is the manufacturer model ID
  UINT32 OemRevision;     //!< OEM revision of table for supplied OEM table ID
  UINT32 CreatorId;       //!< Vendor ID of utility that created the table
  UINT32 CreatorRevision; //!< Revision of utility that created the table
} TABLE_HEADER;

typedef struct {
  TABLE_HEADER Header;
  UINT8 Reserved[4];
} NFitHeader;

typedef struct {
  SubTableHeader Header;
  UINT16 SpaRangeDescriptionTableIndex;
  UINT16 Flags;
  UINT8 Reserved[4];
  UINT32 ProximityDomain;
  UINT8 AddressRangeTypeGuid[16];
  UINT64 SystemPhysicalAddressRangeBase;
  UINT64 SystemPhysicalAddressRangeLength;
  UINT64 AddressRangeMemoryMappingAttribute;
} SpaRangeTbl;

typedef union {
  struct {
    UINT32 DimmNumber:4;
    UINT32 MemChannel:4;
    UINT32 MemControllerId:4;
    UINT32 SocketId:4;
    UINT32 NodeControllerId:12;
    UINT32 Reserved:4;
  } NfitDeviceHandle;
  UINT32 AsUint32;
} NfitDeviceHandle;


typedef struct {
  SubTableHeader Header;
  NfitDeviceHandle DeviceHandle;
  UINT16 NvDimmPhysicalId;
  UINT16 NvDimmRegionalId;
  UINT16 SpaRangeDescriptionTableIndex;
  UINT16 NvdimmControlRegionDescriptorTableIndex;
  UINT64 NvDimmRegionSize;
  UINT64 RegionOffset;
  UINT64 NvDimmPhysicalAddressRegionBase;
  UINT16 InterleaveStructureIndex;
  UINT16 InterleaveWays;
  UINT16 NvDimmStateFlags;
  UINT8 Reserved[2];
} NvDimmRegionTbl;

typedef struct {
  SubTableHeader Header;
  UINT16 InterleaveStructureIndex;
  UINT8 Reserved[2];
  UINT32 NumberOfLinesDescribed;
  UINT32 LineSize;
  UINT32 LinesOffsets[0];
} InterleaveStruct;

typedef struct {
  SubTableHeader Header;
  UINT8 Reserved[4];
  UINT8 Data[0];
} SmbiosTbl;

typedef struct {
  SubTableHeader Header;
  UINT16 ControlRegionDescriptorTableIndex;
  UINT16 VendorId;
  UINT16 DeviceId;
  UINT16 Rid;
  UINT16 SubsystemVendorId;
  UINT16 SubsystemDeviceId;
  UINT16 SubsystemRid;
  UINT8 ValidFields;
  UINT8 ManufacturingLocation;
  UINT16 ManufacturingDate;
  UINT8 Reserved[2];
  UINT32 SerialNumber;
  UINT16 RegionFormatInterfaceCode;
  UINT16 NumberOfBlockControlWindows;
  UINT64 SizeOfBlockControlWindow;
  UINT64 CommandRegisterOffsetInBlockControlWindow;
  UINT64 SizeOfCommandRegisterInBlockControlWindows;
  UINT64 StatusRegisterOffsetInBlockControlWindow;
  UINT64 SizeOfStatusRegisterInBlockControlWindows;
  UINT16 ControlRegionFlag;
  UINT8 Reserved1[6];
} ControlRegionTbl;

typedef struct {
  SubTableHeader Header;
  UINT16 ControlRegionStructureIndex;
  UINT16 NumberOfBlockDataWindows;
  UINT64 BlockDataWindowStartLogicalOffset;
  UINT64 SizeOfBlockDataWindow;
  UINT64 AccessibleBlockCapacity;
  UINT64 AccessibleBlockCapacityStartAddress;
} BWRegionTbl;

typedef struct {
  SubTableHeader Header;
  NfitDeviceHandle DeviceHandle;
  UINT16 NumberOfFlushHintAddresses;
  UINT8 Reserved[6];
  UINT64 FlushHintAddress[0];
} FlushHintTbl;

#define CAPABILITY_CACHE_FLUSH    BIT0
#define CAPABILITY_MEMORY_FLUSH   BIT1
#define CAPABILITY_MEMORY_MIRROR  BIT2

typedef struct {
  SubTableHeader Header;
  UINT8 HighestValidCapability;
  UINT8 Reserved[3];
  UINT32 Capabilities;
  UINT32 Reserved_1;
} PlatformCapabilitiesTbl;

typedef struct {
  NFitHeader *pFit;
  UINT32 SpaRangeTblesNum;
  SpaRangeTbl **ppSpaRangeTbles;
  UINT32 NvDimmRegionTblesNum;
  NvDimmRegionTbl **ppNvDimmRegionTbles;
  UINT32 InterleaveTblesNum;
  InterleaveStruct **ppInterleaveTbles;
  UINT32 SmbiosTblesNum;
  SmbiosTbl **ppSmbiosTbles;
  UINT32 ControlRegionTblesNum;
  ControlRegionTbl **ppControlRegionTbles;
  UINT32 BWRegionTblesNum;
  BWRegionTbl **ppBWRegionTbles;
  UINT32 FlushHintTblesNum;
  FlushHintTbl **ppFlushHintTbles;
  UINT32 PlatformCapabilitiesTblesNum;
  PlatformCapabilitiesTbl **ppPlatformCapabilitiesTbles;
} ParsedFitHeader;

typedef struct {
  UINT16 Type;     //!< Type of PCAT table
  UINT16 Length;   //!< Length of the table including the header and body
} PCAT_TABLE_HEADER;

typedef struct {
  /**
    HEADER
  **/
  TABLE_HEADER Header; //!< Signature for this table: 'PCAT'
  UINT8 Reserved[4];
  /**
    BODY
  **/
  /**
    A list of PCAT table structures
  **/
  VOID *pPcatTables[0];
} PLATFORM_CONFIG_ATTRIBUTES_TABLE;

typedef
union {
  UINT8 MemoryModes;
  struct {
    UINT8 OneLm           :1;
    UINT8 Memory          :1;
    UINT8 AppDirect       :1;
    UINT8 AppDirectCached :1;
    UINT8 Storage         :1;
    UINT8 SubNUMAClster   :1;
    UINT8 Reserved        :2;
  } MemoryModesFlags;
} SUPPORTED_MEMORY_MODE;

typedef struct {
    UINT8 CurrentVolatileMode : 2;
    UINT8 PersistentMode      : 2;
    UINT8 AllowedVolatileMode : 2;
    UINT8 Reserved            : 1;
    UINT8 SubNumaCluster      : 1;
  } _MEMORY_MODE_SPLIT;

typedef
union {
  UINT8 MemoryMode;
  _MEMORY_MODE_SPLIT MemoryModeSplit;
} CURRENT_MEMORY_MODE;

typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 0
  /**
    BODY
  **/
  /**
    Bit0
      If set BIOS supports changing configuration through management software.
      If clear BIOS does not allow configure change through management software
    Bit1
      If set BIOS supports runtime interface to validate management configuration change request.
      Refer to BIOS runtime interface data structure.
      Note: this bit is valid only if Bit0 is set.
  **/
  UINT8 MgmtSwConfigInputSupport;
  /**
    Bit0: Set if 1LM Mode supported
    Bit1: Set if 2LM Mode supported
    Bit2: Set if PM-Direct Mode supported
    Bit3: Set if PM-Cached Mode supported
    Bit4: Set if Block Mode supported
  **/
  SUPPORTED_MEMORY_MODE MemoryModeCapabilities;
  /**
    Memory Mode selected in the BIOS setup
    1 - 1LM mode
    2 - 2LM + PM-Direct Mode
    3 - 2LM + PM-Cached Mode
    4 - Auto (2LM if DDR4+PMM with volatile mode present, 1LM otherwise)
    Note: no direct control is given to the management software to switch the mode
  **/
  CURRENT_MEMORY_MODE CurrentMemoryMode;
  /**
    Bit0: If set Persistent Memory region mirroring is supported
    If mirror is supported, management software can select interleave sets for mirror.
  **/
  UINT8 PersistentMemoryRasCapability;
  UINT8 Reserved[8];
} PLATFORM_CAPABILITY_INFO;

typedef
union {
  UINT32 AsUint32;
  struct {
    UINT32 ChannelInterleaveSize:8;
    UINT32 iMCInterleaveSize    :8;
    UINT32 NumberOfChannelWays  :NUMBER_OF_CHANNEL_WAYS_BITS_NUM;
    UINT32 Reserved             :6;
    UINT32 Recommended          :1;
  } InterleaveFormatSplit;
} INTERLEAVE_FORMAT;

typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 1
  /**
    BODY
  **/
  /**
    Value defines memory mode
    0 - 1LM
    1 - 2LM
    3 - App Direct PM
    4 - App Direct Cached PM
  **/
  UINT8 MemoryMode;
  UINT8 Reserved[3];
  /**
    Interleave alignment size in 2^n bytes.
    n=26 for 64MB
    n=27 for 128MB
  **/
  UINT16 InterleaveAlignmentSize;
  /**
    Number of interleave formats supported by BIOS for the above memory mode. The variable body of this structure
    contains m number of interleave formats.
  **/
  UINT16 NumOfFormatsSupported;
  /**
    This field will have a list of 4byte values that provide information about BIOS supported interleave formats and
    the recommended interleave informations.

    Byte0 - Channel interleave size
    Bit0 - 64B
    Bit1 - 128B
    Bit2 - 256B
    Bit3 - Reserved
    Bit4 - Reserved
    Bit5 - Reserved
    Bit6 - 4KB
    Bit7 - 1GB

    Byte1 - iMC interleave size
    Bit0 - 64B
    Bit1 - 128B
    Bit2 - 256B
    Bit3 - Reserved
    Bit4 - Reserved
    Bit5 - Reserved
    Bit6 - 4KB
    Bit7 - 1GB

    Byte2-3 - Number of channel ways
    Bit0 - 1way
    Bit1 - 2way
    Bit2 - 3way
    Bit3 - 4way
    Bit4 - 6way
    Bit5 - 8way
    Bit6 - 12way
    Bit7 - 16way
    Bit8 - 24way
    Bit9-14 - Reserved

    Byte2-3 - Recommended Interleave format
    Bit15 - If clear, the interleave format is supported but not recommended.
            If set, the interleave format is recommended.
  **/
  INTERLEAVE_FORMAT InterleaveFormatList[0];
} MEMORY_INTERLEAVE_CAPABILITY_INFO;

typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 2
  /**
    BODY
  **/
  /**
    Verify Trigger GAS Structure
  **/
  /**
    Address space type of command register.
    1 - System I/O
  **/
  UINT8 AddressSpaceId;
  UINT8 BitWidth;       //!< The size in bits of the command register
  UINT8 BitOffset;      //!< The bit offset command register at the given address
  /**
    Command register access size
    0 - Undefined
    1 - Byte Access
    2 - Word Access
    3 - Dword Access
    4 - Qword Access
  **/
  UINT8 AccessSize;
  UINT64 Address;   //!< Register in the given address space
  /**
    Verify Trigger Operation
  **/
  /**
    Type of register operation to submit the command
    0 - Read register
    1 - Write register
  **/
  UINT8 TriggerOperationType;
  UINT8 Reserved2[7];
  UINT64 TriggerValue;        //!< If operation type is write, this field provides the data to be written
  /**
    Mask value to be used to preserve the bits on the write. If the bits are not 1, read the value from
    the address space, mask the value part and then do the write
  **/
  UINT64 TriggerMask;
  /**
    Verify Status Operation
  **/
  /**
    ACPI GAS structure with Address Space ID.
    0 - System memory
  **/
  UINT8 GasStructure[12];
  /**
    Type of register operation to submit the command
    3 - Read Memory
  **/
  UINT8 StatusOperationType;
  UINT8 Reserved3[3];
  /**
    Read the value from given address and mask using this value. Result status:
    0 - None
    1 - Busy
    2 - Done
    Results are updated in the DIMMs' config output structures
  **/
  UINT64 StatusMask;
} RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE;

typedef struct {
  /**
    HEADER
  **/
  PCAT_TABLE_HEADER Header; //!< Type: 3
  /**
    BODY
  **/
  UINT8 Reserved[2];
  UINT16 VendorId;   //!< Vendor ID of generator of the GUID who maintains the format for the GUID data
  EFI_GUID Guid;
  VOID *pGuidData[0];   //!< GUID Data Size must be 8-byte aligned
} CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE;

typedef struct {
  /**
    HEADER
   **/
  PCAT_TABLE_HEADER Header; //!< Type: 6
  /**
    BODY
   **/
  UINT16 SocketId;           //!< Zero indexed NUMA node identifier
  UINT8 Reserved[2];
  UINT64 MappedMemorySizeLimit;      //!< Total amount of physical memory in bytes allowed to be mapped into SPA based on the SKU of the CPU specified by Socket ID
  UINT64 TotalMemorySizeMappedToSpa; //!< Total amount of physical memory in bytes currently mapped into the SPA for the CPU specified by Socket ID
  UINT64 CachingMemorySize;          //!< Total amount of physical memory in bytes used for caching when the system is in 2LM mode
} SOCKET_SKU_INFO_TABLE;

/** PMTT table **/
typedef struct {
  TABLE_HEADER Header;
  UINT8 Reserved[4];
  VOID  *PMTTAggregatedDevices[0];
} PMTT_TABLE;

/* Header common to socket, iMC and Module*/
typedef struct {
  /*
  * Type of aggregated device
  * 0 - Socket
  * 1 - Memory controller
  * 2 - Module
  * 3 - 0xFF
  */
  UINT8 Type;

  UINT8 Reserved1;
  /*
  * Length in bytes for entire table.
  */
  UINT16 Length;
  UINT16 Flags;
  UINT16 Reserved2;
} PMTT_COMMON_HEADER;

/*
* Memory aggregator device structure
* Type 0 - socket
*/
typedef  struct {
  UINT16 SocketId;
  UINT16 Reserved3;

  // Memory controller comes here
} PMTT_SOCKET;

/*
* Memory aggregator device structure
* Type 1 - iMC
*/
typedef struct {
  UINT32 ReadLatency;
  UINT32 WriteLatency;
  UINT32 ReadBW;
  UINT32 WriteBW;
  UINT16 OptimalAccessUnit;
  UINT16 OptimalAccessAlignment;
  UINT16 Reserved3;
  UINT16 NoOfProximityDomains;
  UINT32 ProximityDomainArray; //!< Supposed to be an array but BIOS is filling in 0s for now
  // Module structure follows this
} PMTT_iMC;

/*
* Memory aggregator device structure
* Type 2 - Module
*/
typedef struct {
  UINT16 PhysicalComponentId;
  UINT16 Reserved3;
  UINT32 SizeOfDimm;
  UINT32 SmbiosHandle;
} PMTT_MODULE;

#pragma pack(pop)

typedef struct {
  PLATFORM_CONFIG_ATTRIBUTES_TABLE *pPlatformConfigAttr;
  PLATFORM_CAPABILITY_INFO **ppPlatformCapabilityInfo;
  UINT32 PlatformCapabilityInfoNum;
  MEMORY_INTERLEAVE_CAPABILITY_INFO **ppMemoryInterleaveCapabilityInfo;
  UINT32 MemoryInterleaveCapabilityInfoNum;
  RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE **ppRuntimeInterfaceValConfInput;
  UINT32 RuntimeInterfaceValConfInputNum;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE **ppConfigManagementAttributesInfo;
  UINT32 ConfigManagementAttributesInfoNum;
  SOCKET_SKU_INFO_TABLE **ppSocketSkuInfoTable;
  UINT32 SocketSkuInfoNum;
} ParsedPcatHeader;

/**
  Frees the memory associated in the parsed PCAT table.

  @param[in, out] pParsedPcat pointer to the PCAT header.
**/
VOID
FreeParsedPcat(
  IN OUT ParsedPcatHeader *pParsedPcat
  );

/**
  Frees the memory associated in the parsed NFit table.

  @param[in] pParsedNfit pointer to the NFit header.
**/
VOID
FreeParsedNfit(
  IN     ParsedFitHeader *pParsedNfit
  );

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif /** _NVMTABLES_H_ **/
