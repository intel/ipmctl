/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DIMM_H_
#define _DIMM_H_

#include <Debug.h>
#include <Utility.h>
#include <Types.h>
#include <AcpiParsing.h>
#include <IndustryStandard/SmBios.h>

#include <NvmDimmPassThru.h>
#include <PlatformConfigData.h>

//---> Turn on/off large payload support
#define USE_LARGE_PAYLOAD
//<---

#define DB_SHIFT 48
#define DB_SHIFT_32 48-32                      //!< DB_SHIFT in UINT32 half of command register
#define SUB_OP_SHIFT 40
#define OP_SHIFT 32
#define SQ_SHIFT 63

#define EMULATOR_DIMM_HEALTH_STATUS       0    //!< Normal
#define EMULATOR_DIMM_TEMPERATURE         300  //!< 300K is about 26C
#define EMULATOR_DIMM_TEMPERATURE_THR     310  //!< 310K is about 35C
#define EMULATOR_DIMM_SPARE_CAPACITY      75   //!< 75% of spare capacity
#define EMULATOR_DIMM_SPARE_CAPACITY_THR  5    //!< 5% of spare capacity
#define EMULATOR_DIMM_PERCENTAGE_USED_THR 90   //!< 90% of space is used
#define DIMM_OUTPUT_PAYLOAD_SIZE          128  //!< The max size of the DIMM small output payload

#define MAX_SMALL_OUTPUT_REG_COUNT      32

#define BOOT_STATUS_REGISTER_OFFSET 0x20000
#define BOOT_STATUS_REGISTER_LENGTH 0x8

#define BW_STATUS_REGISTER_LENGTH       4
#define BW_APERTURE_LENGTH              8192

#define BW_DPA_MASK           0x1FFFFFFFFF
#define BW_DPA_RIGHT_SHIFT    6

#define BW_LENGHT_MASK        0xff
#define BW_LENGTH_POSITION    37

#define BW_OPERATION_MASK     0x1
#define BW_OPERATION_POSITION 45

#define BW_PENDING_MASK        0x80000000
#define BW_INVALID_ADRESS_MASK 1          // 0b1
#define BW_ACCESS_ERROR        (1 << 1)   // 0b10
#define BW_PM_ACCESS_ERROR     (1 << 4)   // 0b10000
#define BW_REGION_ACCESS_ERROR (1 << 5)   // 0b100000

#define PT_LONG_TIMEOUT_INTERVAL EFI_TIMER_PERIOD_MILLISECONDS(150)
#define PT_UPDATEFW_TIMEOUT_INTERVAL EFI_TIMER_PERIOD_SECONDS(4)

#define SPD_INTEL_VENDOR_ID 0x8980
#define SPD_DEVICE_ID 0x0000
#define SPD_DEVICE_ID_05 0x0979
#define SPD_DEVICE_ID_10 0x097A
#define SPD_DEVICE_ID_15 0x097B

typedef enum _BW_COMMAND_CODE {
  BwRead = 0,
  BwWrite = 1
} BW_COMMAND_CODE;
/*
  DIMM STRUCTS
*/

struct _PMEM_DEV;
struct _DIMM_REGION;

/**
  Making a guess that all DIMMs must use a volume label space
**/
typedef struct _LABEL_INFO {
  LIST_ENTRY LabelNode;
  UINT64 Signature;
  struct VOLUME_LABEL *pLabel;
  /**
    Offset from start of volume label space to the first copy of the volume label
  **/
  UINT64 PrimaryLabelOffset;
  /**
    Offset from the start of the volume label space to the second copy of the volume label
  **/
  UINT64 SecondaryLabelOffset;
} LABEL_INFO;

#define LABEL_INFO_SIGNATURE     SIGNATURE_64('L', 'A', 'B', 'L', 'I', 'N', 'F', 'O')
#define LABEL_INFO_FROM_NODE(a)  CR(a, LABEL_INFO, LabelNode, LABEL_INFO_SIGNATURE)

/**
  Mailbox Status Codes
**/
enum {
  MB_SUCCESS = 0x00,         //!< Command Complete
  MB_INVALID_PARAM = 0x01,   //!< Input parameter invalid
  MB_DATA_TRANS_ERR = 0x02,  //!< Error in the data transfer
  MB_INTERNAL_ERR = 0x03,    //!< Internal device error
  MB_UNSUPPORTED_CMD = 0x04, //!< Opcode or Sub opcode not supported
  MB_BUSY = 0x05,            //!< Device is busy processing a long operation
  MB_PASSPHRASE_ERR = 0x06,  //!< Incorrect Passphrase
  MB_SECURITY_ERR = 0x07,    //!< Security check on the image has failed
  MB_INVALID_STATE = 0x08,   //!< Op not permitted in current security state
  MB_SYS_TIME_ERR = 0x09,    //!< System time has not been set yet
  MB_DATA_NOT_SET = 0x0A     //!< Get data called without ever calling set data
};

typedef enum {
  LSA_OK,
  LSA_CORRUPTED,
  LSA_COULD_NOT_INIT,
  LSA_CORRUPTED_AFTER_INIT,
  LSA_COULD_NOT_READ_NAMESPACES,
  LSA_NOT_INIT
} LsaStatus;

#define OS_BW_CONTROL_REG_BASE    (0x00000000)
#define OS_BW_BLOCK_APERTURE_BASE (0x07800000)
#define OS_BW_CTRL_REG_OFFSET     (0x00000000)
#define OS_BW_STATUS_REG_OFFSET   (0x00000008)
#define OS_BW_REG_SKIP            (0x1000)
#define OS_BW_APT_SKIP            (0x2000)

typedef struct _BLOCK_WINDOW {
  volatile UINT64 *pBwCmd;     //!< Variable Amount of the command register
  volatile UINT32 *pBwStatus;  //!< Variable Amount of the status register
  volatile VOID **ppBwApt;     //!< Variable Amount of the aperture segment
  UINT32 LineSizeOfApt;        //!< Line size of the interleaved aperture
  UINT32 NumSegmentsOfApt;     //!< Number of segments of the interleaved aperture
} BLOCK_WINDOW;

typedef struct _DIMM {
  LIST_ENTRY DimmNode;
  UINT64 Signature;
  UINT16 DimmID; //!< SMBIOS Type 17 handle corresponding to this memory device

  /** Topology related fields **/
  BOOLEAN InNfit;                           //!< True if Dimm is in NFIT
  SMBUS_DIMM_ADDR SmbusAddress;             //!< SMBUS address
  NfitDeviceHandle DeviceHandle;
  UINT16 SocketId;
  UINT16 ImcId;
  UINT16 NodeControllerID;
  UINT16 ChannelId;
  UINT16 ChannelPos;

  UINT16 VendorId;                         //!< To allow loading of vendor specific driver
  UINT16 DeviceId;                         //!< To allow vendor to comprehend multiple devices
  UINT16 SubsystemVendorId;                //!< Vendor identifier of memory subsystem controller
  UINT16 SubsystemDeviceId;                //!< Device identifier of memory subsystem controller
  UINT16 Manufacturer;

  BOOLEAN ManufacturingInfoValid;          //!< Validity of manufacturing location and date
  UINT8 ManufacturingLocation;
  UINT16 ManufacturingDate;

  UINT32 SerialNumber;
  CHAR8 PartNumber[PART_NUMBER_LEN];
  UINT16 Rid;                              //!< Revision ID
  UINT16 SubsystemRid;                     //!< Revision ID of the subsystem memory controller
  BOOLEAN PackageSparingCapable;
  SKU_INFORMATION SkuInformation;
  /**
    Format interface code: Allows vendor hardware to be handled by a generic
    driver (behaves similar to class code in PCI)
  **/
  UINT16 FmtInterfaceCode[MAX_IFC_NUM];
  UINT32 FmtInterfaceCodeNum;

  FIRMWARE_VERSION FwVer;                  //!< Struct containing firmware version details
  /** Minimum supported version of FW API: 1.2 **/
  #define DEV_FW_API_VERSION_MAJOR_MIN   1
  #define DEV_FW_API_VERSION_MINOR_MIN   2

  UINT16 NumBlockWindows;                  //!< Number of Block Windows Supported

  UINT64 RawCapacity;                      //!< PM + volatile
  UINT64 VolatileStart;                    //!< DPA start of the Volatile region
  UINT64 VolatileCapacity;                 //!< Capacity in bytes mapped as volatile memory
  UINT64 PmStart;                          //!< DPA start of the PM region
  UINT64 PmCapacity;                       //!< DIMM Capacity (Bytes) to reserve for PM
  UINT64 InaccessibleVolatileCapacity;     //!< Capacity in bytes for use as volatile memory that has not been exposed
  UINT64 InaccessiblePersistentCapacity;   //!< Capacity in bytes for use as persistent memory that has not been exposed
  struct _DIMM_REGION *pIsRegions[MAX_IS_PER_DIMM];
  UINT32 IsRegionsNum;

  UINT8 IsNew;                             //!< if is incorporated with the rest of the AEPs in the system
  UINT8 RebootNeeded;                      //!< Whether or not reboot is required to reconfigure dimm

  UINT8 LsaStatus;                         //!< The status of the LSA partition parsing for this DIMM

  BLOCK_WINDOW *pBw;
  MAILBOX *pHostMailbox;
  NvDimmRegionTbl *pCtrlTbl;      //!< ptr to the table used to configure the mailbox
  SpaRangeTbl *pCtrlSpaTbl;       //!> ptr to the spa range table associated with the mailbox table
  NvDimmRegionTbl *pDataTbl;      //!< ptr to the table used to configure the block windows
  SpaRangeTbl *pDataSpaTbl;       //!< ptr to the spa range table associated with the block windows table
  UINT64 *pFlushAddress;          //!< address to which data needs to be written to perform a WPQ flush
  BOOLEAN FlushRequired;          //!< The boolean value indicating when the Aperature needs to be flushed before IO
  BOOLEAN ControlWindowLatch;
  BOOLEAN EncryptionEnabled;      //!< True if the DIMMs security is enabled
  UINT16 NvDimmStateFlags;

  /** Current regions config **/
  BOOLEAN Configured;                           //!< true if the DIMM is configured
  UINT64 MappedVolatileCapacity;
  UINT64 MappedPersistentCapacity;
  struct _NVM_IS *pISs[MAX_IS_PER_DIMM];
  UINT8 ConfigStatus;                           //!< Configuration Status code
  UINT32 ISsNum;

  UINT32 PcdOemPartitionSize;
  UINT32 PcdLsaPartitionSize;

  /** Goal regions config **/
  BOOLEAN RegionsGoalConfig;
  UINT64 VolatileSizeGoal;                        //!< Active only if RegionsGoalConfig is TRUE
  struct _REGION_GOAL *pRegionsGoal[MAX_IS_PER_DIMM]; //!< Active only if RegionsGoalConfig is TRUE
  UINT32 RegionsGoalNum;                            //!< Active only if RegionsGoalConfig is TRUE
  BOOLEAN PcdSynced;                              //!< Active only if RegionsGoalConfig is TRUE
  UINT8 GoalConfigStatus;                         //!< Active only if RegionsGoalConfig is TRUE

  /** List of Storage Namespaces **/
  LIST_ENTRY StorageNamespaceList;
  VOID *pPcdLsa;
  // Always allocated to be size of PCD_OEM_PARTITION_INTEL_CFG_REGION_SIZE
  VOID *pPcdOem;
  UINT32 PcdOemSize;
} DIMM;

#define DIMM_SIGNATURE     SIGNATURE_64('\0', '\0', '\0', '\0', 'D', 'I', 'M', 'M')
#define DIMM_FROM_NODE(a)  CR(a, DIMM, DimmNode, DIMM_SIGNATURE)

#define MEMMAP_RANGE_UNDEFINED               1
#define MEMMAP_RANGE_RESERVED                2
#define MEMMAP_RANGE_VOLATILE                3
#define MEMMAP_RANGE_PERSISTENT              4
#define MEMMAP_RANGE_IS                      5
#define MEMMAP_RANGE_IS_NOT_INTERLEAVED      6
#define MEMMAP_RANGE_IS_MIRROR               7
#define MEMMAP_RANGE_STORAGE_ONLY            8
#define MEMMAP_RANGE_BLOCK_NAMESPACE         9
#define MEMMAP_RANGE_APPDIRECT_NAMESPACE    10
#define MEMMAP_RANGE_FREE                   11
#define MEMMAP_RANGE_LAST_USABLE_DPA        12

typedef enum {
  FreeCapacityForPersistentRegion         = 0,
  FreeCapacityForMirrorRegion             = 1,
  FreeCapacityForStMode                 = 2,
  FreeCapacityForStModeOnInterleaved    = 3,
  FreeCapacityForStModeOnNotInterleaved = 4,
  FreeCapacityForStModeOnStOnly         = 5,
  FreeCapacityForADMode                 = 6
} FreeCapacityType;

typedef struct _MEMMAP_RANGE {
  UINT64 Signature;
  LIST_ENTRY MemmapNode;
  DIMM *pDimm;
  UINT16 RangeType;
  UINT64 RangeStartDpa;
  UINT64 RangeLength;
} MEMMAP_RANGE;

#define MEMMAP_RANGE_SIGNATURE     SIGNATURE_64('M', 'M', 'A', 'P', 'R', 'N', 'G', 'E')
#define MEMMAP_RANGE_FROM_NODE(a)  CR(a, MEMMAP_RANGE, MemmapNode, MEMMAP_RANGE_SIGNATURE)

EFI_STATUS
DimmInit(
  IN     struct _PMEM_DEV *pDev
);

EFI_STATUS
DimmExit(
  IN     struct _PMEM_DEV *pDev
);

EFI_STATUS
InitializeDimmInventory(
  IN OUT struct _PMEM_DEV *pDev
  );

EFI_STATUS
RemoveDimmInventory(
  IN OUT struct _PMEM_DEV *pDev
  );

EFI_STATUS
InsertDimm(
  IN     DIMM *pDimm,
     OUT struct _PMEM_DEV *pDev
  );

/**
  Get dimm by Dimm ID
  Scan the dimm list for a dimm identified by Dimm ID

  @param[in] DimmID: The SMBIOS Type 17 handle of the dimm
  @param[in] pDimms: The head of the dimm list

  @retval DIMM struct pointer if matching dimm has been found
  @retval NULL pointer if not found
**/
DIMM *
GetDimmByPid(
  IN     UINT32 DimmID,
  IN     LIST_ENTRY *pDimms
  );

/**
  Get dimm by Dimm Device Handle as UINT32
  Scan the dimm list for a dimm identified by Dimm device handle

  @param[in] DimmID: UINT32 device handle of the dimm
  @param[in] pDimms: The head of the dimm list

  @retval DIMM struct pointer if matching dimm has been found
  @retval NULL pointer if not found
**/
DIMM *
GetDimmByHandle(
  IN     UINT32 DeviceHandle,
  IN     LIST_ENTRY *pDimms
  );

/**
  Get dimm by serial number
  Scan the dimm list for a dimm identified by serial number

  @param[in] pDimms The head of the dimm list
  @param[in] DimmID The serial number of the dimm

  @retval DIMM struct pointer if matching dimm has been found
  @retval NULL pointer if not found
**/
DIMM *
GetDimmByHandle(
  IN     UINT32 DeviceHandle,
  IN     LIST_ENTRY *pDimms
  );
DIMM *
GetDimmBySerialNumber(
  IN     LIST_ENTRY *pDimms,
  IN     UINT32 SerialNumber
  );

/**
  Get dimm by its unique identifier structure
  Scan the dimm list for a dimm identified by its
  unique identifier structure

  @param[in] pDimms The head of the dimm list
  @param[in] DimmUniqueId The unique identifier structure of the dimm

  @retval DIMM struct pointer if matching dimm has been found
  @retval NULL pointer if not found
**/
DIMM *
GetDimmByUniqueIdentifier(
  IN     LIST_ENTRY *pDimms,
  IN     DIMM_UNIQUE_IDENTIFIER DimmUniqueId
  );

// TODO: Remove, only added this for debug
VOID
PrintDimmMemmap(
  IN     LIST_ENTRY *pMemmap
  );

/**
  Display memory map list. Use for debug purposes only

  @param[in] pDimm  DIMM for which list the memory map
**/
VOID
ShowDimmMemmap(
  IN     DIMM *pDimm
  );

DIMM *
GetDimmByIndex(
  IN     INT32 Index,
  IN     struct _PMEM_DEV *pDev
  );


/**
  Get DIMM by Smbus address in global structure

  @param[in] Address - Smbus address of Dimm
  @param[in] pDimms - The head of the dimm list

  @retval Found Dimm or NULL
**/
DIMM *
GetDimmBySmbusAddress(
  IN     SMBUS_DIMM_ADDR Address,
  IN     LIST_ENTRY *pDimms
  );

/**
  Get max Dimm ID
  Scan the dimm list for a max Dimm ID

  @param[in] pDimms: The head of the dimm list

  @retval Max Dimm ID or 0 if not found
**/
UINT16
GetMaxPid(
  IN     LIST_ENTRY *pDimms
  );

/**
  Retrieve list of memory regions of a DIMM

  Regions will be delivered in a form of sorted linked list with
  items containing start DPA and length of free ranges and they may overlap.
  Last item on the list will be a last DPA marker in order to point address boundary.

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pMemmap Initialized list head to which region items will be added

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_OUT_OF_RESOURCES Not enough free space on target
  @retval EFI_SUCCESS List correctly retrieved
**/
EFI_STATUS
GetDimmMemmap(
  IN     DIMM *pDimm,
     OUT LIST_ENTRY *pMemmap
  );

/**
  Retrieve list of free regions of a DIMM based on capacity type

  Free regions will be delivered in a form of sorted linked list with
  items containing start DPA and length of free ranges and they don't overlap each other

  @param[in] pDimm Target DIMM structure pointer
  @param[in] FreeCapacityTypeArg Determine a type of free capacity
  @param[out] pFreemap Initialized list head to which region items will be added

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_OUT_OF_RESOURCES Not enough free space on target
  @retval EFI_SUCCESS List correctly retrieved
**/
EFI_STATUS
GetDimmFreemap(
  IN     DIMM *pDimm,
  IN     FreeCapacityType FreeCapacityTypeArg,
     OUT LIST_ENTRY *pFreemap
  );

/**
  Free resources of memmap list items

  @param[in, out] pMemmapList Memmap list that items will be freed for
**/
VOID
FreeMemmapItems(
  IN OUT LIST_ENTRY *pMemmapList
  );

/**
  Merge overlapped ranges

  Memmap ranges may overlap each other. This function merges overlapped ranges to continuous ranges.
  Input list has to be sorted by DPA start address. Returned list will be sorted as well.

  The caller is responsible for a memory deallocation of the returned list.

  @param[in] pMemmapList  Initialized list of ranges to merge.
  @param[out] pMergedList Initialized, output list to fill with continuous ranges.

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure
**/
EFI_STATUS
MergeMemmapItems(
  IN     LIST_ENTRY *pMemmapList,
     OUT LIST_ENTRY *pMergedList
  );

/**
  Find free ranges

  Take list of usable ranges and subtract occupied ranges. The result will be list of free ranges.
  Input lists have to be sorted by DPA start address. Returned list will be sorted as well.

  The caller is responsible for a memory deallocation of the returned list.

  @param[in] pUsableRangesList    Initialized list of usable ranges.
  @param[in] pOccupiedRangesList  Initialized list of occupied ranges to subtract.
  @param[out] pFreeRangesList     Initialized, output list to fill with free ranges.

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure
**/
EFI_STATUS
FindFreeRanges(
  IN     LIST_ENTRY *pUsableRangesList,
  IN     LIST_ENTRY *pOccupiedRangesList,
     OUT LIST_ENTRY *pFreeRangesList
  );

EFI_STATUS
InitializeDimm(
     OUT DIMM **ppDimm,
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 Pid
  );

/**
  Check if the dimm interface code of this DIMM is supported

  @param[in] pDimm Dimm to check

  @retval true if supported, false otherwise
**/

BOOLEAN
IsDimmInterfaceCodeSupported(
  IN     DIMM *pDimm
  );


/**
  Check if the subsystem device ID of this DIMM is supported

  @param[in] pDimm Dimm to check

  @retval true if supported, false otherwise
**/
BOOLEAN
IsSubsystemDeviceIdSupported(
  IN     DIMM *pDimm
  );

/**
  Check if the DIMM containing the specified DIMM ID is
  manageable by the driver

  @param[in] UINT16 Dimm ID to check

  @retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmIdManageable(
  IN     UINT16 DimmID
  );

/**
  Get manageability state for Dimm

  @param[in] pDimm dimm that will be returned manageability state for

  @retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmManageable(
  IN     DIMM *pDimm
  );

/**
  This function performs a DIMM information refresh through the
  DIMM Information FV command.

  @param[in,out] pDimm the DIMM that we want to refresh.

  @retval EFI_SUCCESS - the DIMM was refreshed successfully.
  @retval EFI_INVALID_PARAMETER - pDimm is NULL.
  @retval EFI_OUT_OF_RESOURCES - the memory allocation failed.
**/
EFI_STATUS
RefreshDimm(
  IN OUT DIMM *pDimm
  );

EFI_STATUS
RemoveDimm(
     OUT DIMM *pDimm,
  IN     INT32 Force
  );

/**
  Retrieve DIMM Partition Info
  Send a FW command to retrieve the partition info of the DIMM
  Update the Intel NVM Dimm with pm start, pm capacity, and pm locality

  @param[in] pDimm: The Intel NVM Dimm to gather information from

  @retval Error Code?
**/

EFI_STATUS
GetDimmPartitionInfo(
  IN     DIMM *pDimm
  );

EFI_STATUS
ReadLabels(
  IN     DIMM *pDimm,
     OUT LIST_ENTRY *pList
  );

/**
  Flushes date from the iMC buffers to the DIMM.

  The flushing is made by writing to the Flush Hint addresses.
  If there is no Flush Hint Table for the provided DIMM,
  The assumption is made that WPQ flush is not supported and not required.

  @param[in] pDimm: DIMM to flush the data into.
**/
VOID
DimmWPQFlush(
  IN     DIMM *pDimm
  );

/**
  Create and Configure the OS Mailbox
  Using the NVDIMM region table, determine the location of the OS mailbox
  in the system physical address space. For each piece of the mailbox in SPA
  map them into the virtual address space and record the location.

  @param[in] pDimm: The DIMM to create the OS mailbox for
  @parma[in] pITbl: the interleave table referenced by the mdsarmt_tbl

  @retval Success - The pointer to the completed mailbox structure
  @retval Error - NULL on error
**/
MAILBOX *
CreateMailbox(
  IN     DIMM *pDimm,
  IN     InterleaveStruct *pITbl
  );

VOID
FreeMailbox(
     OUT MAILBOX *pMb
  );

VOID
FreeDimm(
     OUT DIMM *pDimm
  );

/**
  Parse Firmware Version
  Parse the FW version returned by the FW into a CPU format
  FW Payload has the FW version encoded in a binary coded decimal format

  @param[in] Fwr - Firmware revision in BCD format

  @retval Parsed firmware version as friendly FIRMWARE_VERSION structure
**/
FIRMWARE_VERSION
ParseFwVersion(
  IN     UINT8 Fwr[FW_BCD_VERSION_LEN]
  );

/**
  Parse the BCD formatted FW API version into major and minor

  @param[out] pDimm
  @param[in] pPayload
**/
VOID
ParseFwApiVersion(
     OUT DIMM *pDimm,
  IN     PT_ID_DIMM_PAYLOAD *pPayload
  );

/**
  Check if current firmware API version is supported

  @param[in] pDimm Dimm to check

  @retval true if supported, false otherwise
**/
BOOLEAN
IsFwApiVersionSupported(
  IN     DIMM *pDimm
);

/**
  Firmware command get security info
  Execute a FW command to check the security status of a DIMM

  @param[in] pDimm: The DIMM to retrieve security info on
  @param[out] pSecurityPayload: Area to place the security info returned from FW
  @param[in] DimmId: The SMBIOS table type 17 handle of the Intel NVM Dimm

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
  @retval Various errors from FW are still TBD
**/
EFI_STATUS
FwCmdGetSecurityInfo(
  IN     DIMM *pDimm,
     OUT PT_GET_SECURITY_PAYLOAD *pSecurityPayload
  );

/**
  Firmware command to retrieve the ARS status of a particular DIMM.

  @param[in] pDimm Pointer to the DIMM to retrieve ARSStatus on
  @param[out] pDimmARSStatus Pointer to the individual DIMM ARS status

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure
  @retval Various errors from FW
**/
EFI_STATUS
FwCmdGetARS(
  IN     DIMM *pDimm,
     OUT UINT8 *pDimmARSStatus
  );

/**
  This helper function is used to determine the ARS status for the
  particular DIMM by inspecting the firmware ARS return payload.

  @param[in] pARSPayload Pointer to the ARS return payload
  @param[out] pDimmARSStatus Pointer to the individual DIMM ARS status

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
GetDimmARSStatusFromARSPayload(
  IN     PT_PAYLOAD_ADDRESS_RANGE_SCRUB *pARSPayload,
     OUT UINT8 *pDimmARSStatus
);

 /**
  Firmware command to set FW debug level

  @param[in] pDimm Target DIMM structure pointer
  @param[in] FwLogLevel - FwLogLevel to set

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdSetFWDebugLevel(
  IN     DIMM *pDimm,
  IN     UINT8 FwLogLevel
  );

 /**
  Firmware command to get FW debug level

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pFwLogLevel - output variable to save FwLogLevel

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetFWDebugLevel(
  IN     DIMM *pDimm,
     OUT UINT8 *pFwLogLevel
  );

/**
  Firmware command to get Error logs

  Small and large payloads are optional, but at least one has to be provided.

  @param[in] pDimm Target DIMM structure pointer
  @param[in] pInputPayload - filled input payload
  @param[out] pOutputPayload - small payload result data of get error log operation
  @param[in] OutputPayloadSize - size of small payload
  @param[out] pLargeOutputPayload - large payload result data of get error log operation
  @param[in] LargeOutputPayloadSize - size of large payload

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetErrorLog (
  IN     DIMM *pDimm,
  IN     PT_INPUT_PAYLOAD_GET_ERROR_LOG *pInputPayload,
     OUT VOID *pOutputPayload OPTIONAL,
  IN     UINT32 OutputPayloadSize OPTIONAL,
     OUT VOID *pLargeOutputPayload OPTIONAL,
  IN     UINT32 LargeOutputPayloadSize OPTIONAL
  );

/**
  Firmware command to get debug logs

  @param[in] pDimm Target DIMM structure pointer
  @param[in] LogSizeInMbs - number of MB to be fetched
  @param[out] pBytesWritten - number of MB fetched
  @param[out] ppOutPayload - pointer to buffer start

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetFWDebugLog(
  IN     DIMM *pDimm,
  IN     UINT64 LogSizeInMbs,
     OUT UINT64 *pBytesWritten,
     OUT VOID *ppOutPayload
  );

 /**
  Firmware command to get debug logs size in MB

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pLogSizeInMb - number of MB of Logs to be fetched

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetFWDebugLogSize(
  IN     DIMM *pDimm,
     OUT UINT64 *pLogSizeInMb
  );
/**
  Firmware command Identify DIMM.
  Execute a FW command to get information about DIMM.

  @param[in] pDimm The Intel NVM Dimm to retrieve identify info on
  @param[in] Execute on Smbus mailbox instead of DDRT
  @param[out] pPayload Area to place the identity info returned from FW

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
**/
EFI_STATUS
FwCmdIdDimm(
  IN     DIMM *pDimm,
  IN     BOOLEAN Smbus,
     OUT PT_ID_DIMM_PAYLOAD *pPayload
  );

/**
  Firmware command Device Characteristics

  @param[in] pDimm The Intel NVM Dimm to retrieve device characteristics info for
  @param[out] ppPayload Area to place returned info from FW
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS            Success
  @retval EFI_INVALID_PARAMETER  One or more input parameters are NULL
  @retval EFI_OUT_OF_RESOURCES   Memory allocation failure
**/
EFI_STATUS
FwCmdDeviceCharacteristics (
  IN     DIMM *pDimm,
     OUT PT_DEVICE_CHARACTERISTICS_PAYLOAD **ppPayload
  );

/**
  Firmware command get Platform Config Data.
  Execute a FW command to get information about DIMM regions and REGIONs configuration.

  The caller is responsible for a memory deallocation of the ppPlatformConfigData

  @param[in] pDimm The Intel NVM Dimm to retrieve identity info on
  @param[in] PartitionId Partition number to get data from
  @param[out] ppRawData Pointer to a new buffer pointer for storing retrieved data
  @param[out] pDataSize Pointer to the retrieved data buffer size

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
**/
EFI_STATUS
FwCmdGetPlatformConfigData(
  IN     DIMM *pDimm,
  IN     UINT8 PartitionId,
  OUT    UINT8 **ppRawData
  );

/**
Firmware command get Platform Config Data.
Execute a FW command to get information about DIMM regions and REGIONs configuration.

The caller is responsible for a memory deallocation of the ppPlatformConfigData

@param[in] pDimm The Intel NVM Dimm to retrieve identity info on
@param[in] PartitionId Partition number to get data from
@param[in] DataOffset Data read starting point
@param[in] DataSize Number of bytes to read
@param[out] ppRawData Pointer to a new buffer pointer for storing retrieved data

@retval EFI_SUCCESS: Success
@retval EFI_OUT_OF_RESOURCES: memory allocation failure
**/
EFI_STATUS
FwCmdGetPcdDataFromOffset(
	IN     DIMM *pDimm,
	IN     UINT8 PartitionId,
	IN     UINT32 *pDataOffset,
	IN     UINT32 *pDataSize,
	OUT    UINT8 *ppRawData
);

/**
  Firmware command to get the PCD size

  @param[in] pDimm The target DIMM
  @param[in] PartitionId The partition ID of the PCD
  @param[out] pPcdSize Pointer to the PCD size

  @retval EFI_INVALID_PARAMETER Invalid parameter passed
  @retval EFI_OUT_OF_RESOURCES Could not allocate memory
  @retval EFI_SUCCESS Command successfully run
**/
EFI_STATUS
FwCmdGetPlatformConfigDataSize (
  IN     DIMM *pDimm,
  IN     UINT8 PartitionId,
     OUT UINT32 *pPcdSize
  );

/**
  Firmware command set Platform Config Data.
  Execute a FW command to send REGIONs configuration to the Platform Config Data.

  @param[in] pDimm The Intel NVM Dimm to send Platform Config Data to
  @param[in] PartitionId Partition number for data to be send to
  @param[in] pRawData Pointer to a data buffer that will be sent to the DIMM
  @param[in] RawDataSize Size of pRawData in bytes

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
**/
EFI_STATUS
FwCmdSetPlatformConfigData(
  IN     DIMM *pDimm,
  IN     UINT8 PartitionId,
  IN     UINT8 *pRawData,
  IN     UINT32 RawDataSize
);

/**
  Firmware command to get Alarm Thresholds

  @param[in] pDimm The Intel NVM Dimm to retrieve Alarm Thresholds
  @param[out] ppPayloadAlarmThresholds Area to place the Alarm Thresholds data returned from FW.
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadAlarmThresholds is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
 **/
EFI_STATUS
FwCmdGetAlarmThresholds(
  IN     DIMM *pDimm,
     OUT PT_PAYLOAD_ALARM_THRESHOLDS **ppPayloadAlarmThresholds
  );

/**
  Firmware command to set Alarm Thresholds

  @param[in] pDimm The Intel NVM Dimm to set Alarm Thresholds
  @param[in] ppPayloadAlarmThresholds Alarm Thresholds data to set

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadAlarmThresholds is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
 **/
EFI_STATUS
FwCmdSetAlarmThresholds(
  IN     DIMM *pDimm,
  IN     PT_PAYLOAD_ALARM_THRESHOLDS *pPayloadAlarmThresholds
  );

/**
  Firmware command to get SMART and Health Info

  @param[in] pDimm The Intel NVM Dimm to retrieve SMART and Health Info
  @param[out] ppPayloadSmartAndHealth Area to place SMART and Health Info data
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadSmartAndHealth is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
 **/
EFI_STATUS
FwCmdGetSmartAndHealth(
  IN     DIMM *pDimm,
     OUT PT_PAYLOAD_SMART_AND_HEALTH **ppPayloadSmartAndHealth
  );

/**
 Command to send a pass-through firmware command to retrieve a specified memory info page

 @param[in] pDimm Dimm to retrieve the specified memory info page from
 @param[in] PageNum The specific memory info page
 @param[in] PageSize The size of memory info page, which is 128 bytes
 @param[out] ppPayloadMemoryInfoPage Area to place the retrieved memory info page contents
   The caller is responsible to free the allocated memory with the FreePool function.

 @retval EFI_SUCCESS Success
 @retval EFI_INVALID_PARAMETER pDimm or ppPayloadMediaErrorsInfo is NULL
 @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetMemoryInfoPage (
  IN     DIMM *pDimm,
  IN     CONST UINT8 PageNum,
  IN     CONST UINT32 PageSize,
     OUT VOID **ppPayloadMemoryInfoPage
  );

/**
  Firmware command to get Firmware Image Info

  @param[in] pDimm Dimm to retrieve Firmware Image Info for
  @param[out] ppPayloadFwImage Area to place FIrmware Image Info data
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadFwImage is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetFirmwareImageInfo (
  IN     DIMM *pDimm,
     OUT PT_PAYLOAD_FW_IMAGE_INFO **ppPayloadFwImage
  );

/**
  Firmware command to get Power Management Policy Info (for FIS 1.3+)

  @param[in] pDimm The Intel Apache Pass to retrieve Power Management Policy Info
  @param[out] ppPayloadPowerManagementPolicy Area to place Power Management Policy Info data
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadPowerManagementPolicy is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetPowerManagementPolicy(
  IN     DIMM *pDimm,
     OUT PT_PAYLOAD_POWER_MANAGEMENT_POLICY *pPayloadPowerManagementPolicy
  );

#ifdef OS_BUILD

/**
  Firmware command to get PMON Info

  @param[in] pDimm The Intel Apache Pass to retrieve PMON Info
  @param[out] pPayloadPMONRegisters Area to place PMON Registers data
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or pPayloadPMONRegisters is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetPMONRegisters(
  IN     DIMM *pDimm,
  IN     UINT8 SmartDataMask,
     OUT PT_PMON_REGISTERS *pPayloadPMONRegisters
  );

/**
  Firmware command to set PMON Info

  @param[in] pDimm The Intel Apache Pass to retrieve PMON Info
  @param[out] PMONGroupEnable  Specifies which PMON Group to enable.
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdSetPMONRegisters(
  IN     DIMM *pDimm,
  IN     UINT8 PMONGroupEnable
  );
#endif
/**
  Firmware command to get package sparing policy

  @param[in] pDimm The Intel NVM Dimm to retrieve Package Sparing policy
  @param[out] ppPayloadPackageSparingPolicy Area to place Package Sparing policy data
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadPackageSparingPolicy is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetPackageSparingPolicy(
  IN     DIMM *pDimm,
     OUT PT_PAYLOAD_GET_PACKAGE_SPARING_POLICY **ppPayloadPackageSparingPolicy
  );

/**
  Get long operation status FW command

  @param[in] pDimm Dimm to retrieve long operation status from
  @param[out] pFwStatus FW status returned by dimm. FW_INTERNAL_DEVICE_ERROR means there is no long operation currently
  @param[out] pLongOpStatus Filled payload with data

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more pamaters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
FwCmdGetLongOperationStatus(
  IN     DIMM *pDimm,
     OUT UINT8 *pFwStatus,
     OUT PT_OUTPUT_PAYLOAD_FW_LONG_OP_STATUS *pLongOpStatus
  );

/**
  Execute Firmware command to Get DIMM Partition Info

  @param[in]  pDimm     The DIMM to retrieve security info on
  @param[out] pPayload  Area to place the info returned from FW

  @retval EFI_INVALID_PARAMETER NULL pointer for DIMM structure provided
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure
  @retval EFI_...               Other errors from subroutines
  @retval EFI_SUCCESS           Success
**/
EFI_STATUS
FwCmdGetDimmPartitionInfo(
  IN     DIMM *pDimm,
     OUT PT_DIMM_PARTITION_INFO_PAYLOAD *pPayload
  );

/**
  Create and configure block window
  Create the block window structure. This includes locating
  each part of the block window in the system physical address space, and
  mapping each part into the virtual address space.

  @param[in, out] pDimm: DIMM to create the Bw for
  @param[in] PFitHead: Parsed Fit Head
  @parma[in] pMbITbl: the interleave table for mailbox
  @parma[in] pBwITbl: the interleave table for block window

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES in case of allocate memory error
**/
EFI_STATUS
EFIAPI
CreateBw(
  IN OUT DIMM *pDimm,
  IN     ParsedFitHeader *pFitHead,
  IN     InterleaveStruct *pMbITbl OPTIONAL,
  IN     InterleaveStruct *pBwITbl OPTIONAL
  );

/**
  Set Block Window Command to read/write operation

  @param[in] Dpa - Memory DPA
  @param[in] Length - The transfer size is the number of cache lines (Cache line = 64 bytes)
  @param[in] BwOperation - Read/Write command
  @param[out] pBwCommand - 64-bit Command Register buffer
**/
VOID
PrepareBwCommand(
  IN     UINT64 Dpa,
  IN     UINT8 Length,
  IN     UINT8 BwOperation,
     OUT UINT64 *pCommand
  );

/**
  Check Block Input Parameters

  @param[in] pDimm: DIMM to check block window pointers

  @retval EFI_INVALID_PARAMETER if pDimm or some internal Block Window pointer is NULL (pBw, pBw->pBwCmd,
  pBw->pBwApt, pBw->pBwStatus)
**/
EFI_STATUS
EFIAPI
CheckBlockInputParameters(
  IN     DIMM *pDimm
  );

/**
  Poll Firmware Command Completion
  Poll the status register of the BW waiting for the status register complete bit to be set.

  @param[in] pDimm - Dimm with block window with submitted command
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the BW command.
             A Timeout value of 0 means that this function will wait infinitely for the command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.
  @param[out] pStatus returned Status from BW status register

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR FW error received
  @retval EFI_TIMEOUT A timeout occurred while waiting for the command to execute.
**/
EFI_STATUS
EFIAPI
CheckBwCmdTimeout(
  IN     DIMM *pDimm,
  IN     UINT64 Timeout,
     OUT UINT32 *pStatus
  );

/**
  Get command status from command status register

  @param[in] pDimm - pointer to DIMM with Block Window

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER in case of memory allocate error
  @retval other - error code matching to status register
**/
EFI_STATUS
EFIAPI
GetBwCommandStatus(
  IN     DIMM *pDimm
  );

/**
  Read a number of bytes from a DIMM

  @param[in] pDimm: DIMM to read from
  @param[in] Offset: offset from the start of the region this mem type uses
  @param[in] Nbytes: Number of bytes to read
  @param[out] pBuffer: Buffer to place bytes into

  @retval EFI_ACCESS_DENIED if BW request attempts to access a locked or disabled BW or PM region
  @retval EFI_DEVICE_ERROR If DIMM DPA address is invalid or uncorrectable access error occured
  @retval EFI_INVALID_PARAMETER If pDimm, pBuffer or some internal BW pointer is NULL
  @retval EFI_TIMEOUT A timeout occurred while waiting for the command to execute.
**/
EFI_STATUS
EFIAPI
ApertureRead(
  IN     DIMM *pDimm,
  IN     UINT64 Offset,
  IN     UINT64 Nbytes,
     OUT CHAR8 *pBuffer
  );

/**
  Write a number of bytes to a DIMM

  @param[out] pDimm: DIMM to write to
  @param[in] Offset: offset from the start of the region this mem type uses
  @param[in] Nbytes: Number of bytes to write
  @param[in] pBuffer: Buffer containing data to write

  @retval EFI_ACCESS_DENIED if BW request attempts to access a locked or disabled BW or PM region
  @retval EFI_DEVICE_ERROR If DIMM DPA address is invalid or uncorrectable access error occured
  @retval EFI_INVALID_PARAMETER If pDimm, pBuffer or some internal BW pointer is NULL
  @retval EFI_TIMEOUT A timeout occurred while waiting for the command to execute.
**/
EFI_STATUS
EFIAPI
ApertureWrite(
     OUT DIMM *pDimm,
  IN     UINT64 Offset,
  IN     UINT64 Nbytes,
  IN     CHAR8 *pBuffer
  );

/**
  Copy data from an interleaved buffer to a regular buffer.

  Both buffers have to be equal or greater than NumOfBytes.

  @param[out] pRegularBuffer output regular buffer
  @param[in] ppInterleavedBuffer input interleaved buffer
  @param[in] LineSize line size of interleaved buffer
  @param[in] NumOfBytes number of bytes to copy
**/
VOID
ReadFromInterleavedBuffer(
     OUT VOID *pRegularBuffer,
  IN     VOID **ppInterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 NumOfBytes
  );

/**
  Flush data from an interleaved buffer.

  The InterleavedBuffer needs to be at least NumOfBytes.

  @param[in] ppInterleavedBuffer input interleaved buffer
  @param[in] LineSize line size of interleaved buffer
  @param[in] NumOfBytes number of bytes to copy
**/
VOID
FlushInterleavedBuffer(
  IN     VOID **ppInterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 NumOfBytes
  );

/**
  Copies 'Length' no of bytes from source buffer into destination buffer
  The function attempts to perform an 8 byte copy and falls back to 1 byte copies if required

  @param[in] SourceBuffer Source address
  @param[in] Length The length in no of bytes
  @param[out] DestinationBuffer Destination address
**/
VOID *
CopyMem_8 (
  IN OUT VOID      *DestinationBuffer,
  IN     CONST VOID *SourceBuffer,
  IN     UINTN      Length
  );

/**
  Copy data from a regular buffer to an interleaved buffer.

  Both buffers have to be equal or greater than NumOfBytes.

  @param[in]  pRegularBuffer       input regular buffer
  @param[out] ppInterleavedBuffer  output interleaved buffer
  @param[in]  LineSize             line size of interleaved buffer
  @param[in]  NumOfBytes           number of bytes to copy
**/
VOID
WriteToInterleavedBuffer(
  IN     VOID *pRegularBuffer,
     OUT VOID **ppInterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 NumOfBytes
  );

/**
  Clear a part or whole of interleaved buffer.

  @param[out] ppInterleavedBuffer  interleaved buffer to clear
  @param[in]  LineSize             line size of interleaved buffer
  @param[in]  NumOfBytes           number of bytes to clear
**/
VOID
ClearInterleavedBuffer(
     OUT VOID **ppInterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 NumOfBytes
  );

/**
  Get Platform Config Data oem partition and check a correctness of header.

  The caller is responsible for a memory deallocation of the ppPlatformConfigData

  @param[in] pDimm The Intel NVM Dimm to retrieve PCD from
  @param[out] ppPlatformConfigData Pointer to a new buffer pointer for storing retrieved data

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR Incorrect PCD header
  @retval Other return codes from FwCmdGetPlatformConfigData
**/
EFI_STATUS
GetPlatformConfigDataOemPartition(
  IN     DIMM *pDimm,
     OUT NVDIMM_CONFIGURATION_HEADER **ppPlatformConfigData
  );

/**
  Set Platform Config Data OEM Partition Intel config region.
  We only write to the first 64KiB of Intel FW/SW config metadata. The latter
  64KiB is reserved for OEM use.

  @param[in] pDimm The Intel NVM Dimm to set PCD
  @param[in] pNewConf Pointer to new config data to write
  @param[in] NewConfSize Size of pNewConf

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER NULL inputs or bad size
  @retval Other return codes from FwCmdSetPlatformConfigData
**/
EFI_STATUS
SetPlatformConfigDataOemPartition(
  IN     DIMM *pDimm,
  IN     NVDIMM_CONFIGURATION_HEADER *pNewConf,
  IN     UINT32 NewConfSize
  );

/**
  Payload is the same for set and get operation
**/
EFI_STATUS
FwCmdGetOptionalConfigurationDataPolicy(
  IN     DIMM *pDimm,
     OUT PT_OPTIONAL_DATA_POLICY_PAYLOAD *pOptionalDataPolicyPayload
  );

/**
  Payload is the same for set and get operation
**/
EFI_STATUS
FwCmdSetOptionalConfigurationDataPolicy(
  IN     DIMM *pDimm,
  IN     PT_OPTIONAL_DATA_POLICY_PAYLOAD *pOptionalDataPolicyPayload
  );

/**
  Get error logs for given dimm parse it and save in common error log structure

  @param[in] pDimm - pointer to DIMM to get errors
  @param[in] ThermalError - is thermal error (if not it is media error)
  @param[in] HighLevel - high level if true, low level otherwise
  @param[in] SequenceNumber - sequence number of error to fetch in queue
  @param[in] MaxErrorsToSave - max number of new error entries that can be saved in output array
  @param[out] pErrorsFetched - number of new error entries saved in output array
  @param[out] pErrorLogs - output array of errors

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if some pointer is NULL
  @retval other - error code matching to status register
**/
EFI_STATUS
GetAndParseFwErrorLogForDimm(
  IN     DIMM *pDimm,
  IN     CONST BOOLEAN ThermalError,
  IN     CONST BOOLEAN HighLevel,
  IN     CONST UINT16 SequenceNumber,
  IN     UINT32 MaxErrorsToSave,
     OUT UINT32 *pErrorsFetched,
     OUT ERROR_LOG_INFO *pErrorLogs
  );

/**
  Get requested number of specific DIMM registers for given DIMM id

  @param[in] pDimm - pointer to DIMM to get registers for.
  @param[out] pBsr - Pointer to buffer for Boot Status register, contains
              high and low 4B register.
  @param[out] pFwMailboxStatus - Pointer to buffer for Host Fw Mailbox Status Register
  @param[in] SmallOutputRegisterCount - Number of small output registers to get, max 32.
  @param[out] pFwMailboxOutput - Pointer to buffer for Host Fw Mailbox small output Register.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
GetKeyDimmRegisters(
  IN     DIMM *pDimm,
     OUT UINT64 *pBsr,
     OUT UINT64 *pFwMailboxStatus,
  IN     UINT32 SmallOutputRegisterCount,
     OUT UINT64 *pFwMailboxOutput
  );

/**
  Get count of media and/or thermal errors on given DIMM

  @param[in] pDimm - pointer to DIMM to get registers for.
  @param[out] pMediaLogCount - number of media errors on DIMM
  @param[out] pThermalLogCount - number of thermal errors on DIMM

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
FwCmdGetErrorCount(
  IN     DIMM *pDimm,
     OUT UINT32 *pMediaLogCount OPTIONAL,
     OUT UINT32 *pThermalLogCount OPTIONAL
  );

/**
  Matches FW return code to one of available EFI_STATUS EFI base types

  @param[in] Status - status byte returned from FW command

  @retval - Appropriate EFI_STATUS
**/
EFI_STATUS
MatchFwReturnCode (
  IN     UINT8 FwStatus
  );

/**
  Check if SKU conflict occurred.
  Any mixed modes between DIMMs are prohibited on a platform.

  @param[in] pDimm1 - first DIMM to compare SKU mode
  @param[in] pDimm2 - second DIMM to compare SKU mode

  @retval NVM_SUCCESS - if everything went fine
  @retval NVM_ERR_DIMM_SKU_PACKAGE_SPARING_MISMATCH - if Package Sparing conflict occurred
  @retval NVM_ERR_DIMM_SKU_MODE_MISMATCH - if mode conflict occurred
  @retval NVM_ERR_DIMM_SKU_SECURITY_MISMATCH - if security mode conflict occurred
**/
NvmStatusCode
IsDimmSkuModeMismatch(
  IN     DIMM *pDimm1,
  IN     DIMM *pDimm2
  );

/**
  Calculate a size of capacity lost to volatile alignment and space that is not partitioned

  @param[in] Dimm to retrieve reserved size for

  @retval Amount of capacity that will be reserved
**/
UINT64
GetReservedCapacity(
  IN     DIMM *pDimm
  );

/**
  Transform temperature in FW format to usual integer in Celsius

  @param[in] Temperature Temperature from FW

  @retval Value in Celsius
**/
INT16
TransformFwTempToRealValue(
  IN     TEMPERATURE Temperature
  );

/**
  Transform temperature from usual integer in Celsius to FW format

  @param[in] Value Temperature in Celsius

  @retval Temperature in FW format
**/
TEMPERATURE
TransformRealValueToFwTemp(
  IN     INT16 Value
  );

/**
  Get the Dimm UID (a globally unique NVDIMM identifier) for DIMM,
  as per the following representation defined in ACPI 6.1 specification:
    "%02x%02x-%02x-%02x%2x-%02x%02x%02x%02x" (if the Manufacturing Location and Manufacturing Date fields are valid)
    "%02x%02x-%02x%02x%02x%02x" (if the Manufacturing Location and Manufacturing Date fields are invalid)

  @param[in] pDimm DIMM for which the UID is being initialized
  @param[out] pDimmUid Array to store Dimm UID
  @param[in] DimmUidLen Size of pDimmUid

  @retval EFI_SUCCESS  Dimm UID field was initialized successfully.
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL.
**/
EFI_STATUS
GetDimmUid(
  IN     DIMM *pDimm,
     OUT CHAR16 *pDimmUid,
  IN     UINT32 DimmUidLen
  );

/**
  Set object status for DIMM

  @param[out] pCommandStatus Pointer to command status structure
  @param[in] pDimm DIMM for which the object status is being set
  @param[in] Status Object status to set
**/
VOID
SetObjStatusForDimm(
     OUT COMMAND_STATUS *pCommandStatus,
  IN     DIMM *pDimm,
  IN     NVM_STATUS Status
  );

/**
  Get overwrite DIMM operation status for DIMM

  @param[in] pDimm DIMM to retrieve overwrite DIMM operation status from
  @parma[out] pOverwriteDimmStatus Retrieved status

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
GetOverwriteDimmStatus(
  IN     DIMM *pDimm,
     OUT UINT8 *pOverwriteDimmStatus
  );

/**
  Customer Format Dimm
  Send a customer format command through the smbus

  @param[in] pDimm The dimm to attempt to format
  @param[in] Smbus Execute on SMBUS mailbox or DDRT

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter
**/
EFI_STATUS
FwCmdFormatDimm(
  IN    DIMM *pDimm,
  IN    BOOLEAN Smbus
  );

/**
  Firmware command to get failure analysis data

  @param[in] pDimm Target DIMM structure pointer
  @param[in] ID TokenID for blob to retrieve
  @param[out] ppOutputBuffer Pointer to buffer start
  @param[out] pBytesWritten Number of bytes fetched

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR If failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_INVALID_PARAMETER Input parameter null
**/
EFI_STATUS
FwCmdGetFailureAnalysisData(
  IN     DIMM *pDimm,
  IN     UINT32 ID,
     OUT VOID **ppOutputBuffer,
     OUT UINT64 *pBytesWritten
  );

/**
  Firmware command to get failure analysis inventory

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pMaxFATokenID Last TokenID of all valid FA data blobs

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR If failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_INVALID_PARAMETER Input parameter null
**/
EFI_STATUS
FwCmdGetFAInventory(
  IN     DIMM *pDimm,
     OUT UINT32 *pMaxFATokenID
  );

/**
  Firmware command to gAet failure analysis blob header

  @param[in] pDimm Target DIMM structure pointer
  @param[in] ID TokenID of the FA blob header to retrieve
  @param[out] pFABlobHeader Pointer to filled payload with FA blob header

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdGetFABlobHeader(
  IN    DIMM *pDimm,
  IN     UINT32 ID,
     OUT PT_OUTPUT_PAYLOAD_GET_FA_BLOB_HEADER *pFABlobHeader
  );

/**
  Firmware command to get DDRT IO init info

  @param[in] pDimm Target DIMM structure pointer
  @param[in] Execute on Smbus mailbox instead of DDRT
  @param[out] pDdrtIoInitInfo pointer to filled payload with DDRT IO init info

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdGetDdrtIoInitInfo(
  IN     DIMM *pDimm,
  IN     BOOLEAN Smbus,
     OUT PT_OUTPUT_PAYLOAD_GET_DDRT_IO_INIT_INFO *pDdrtIoInitInfo
  );

/**
  Inject Temperature error payload
  @param[IN] pDimm Target DIMM structure pointer
  @param[IN] subopcode for error injection command
  @param[IN] pinjectInputPayload - input payload to be sent

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdInjectError(
	IN     DIMM *pDimm,
	IN	   UINT8 SubOpcode,
	OUT void *pinjectInputPayload
);

/**
  Firmware command to get DIMMs system time

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pSystemTimePayload pointer to filled payload

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdGetSystemTime(
  IN     DIMM *pDimm,
  OUT PT_SYTEM_TIME_PAYLOAD *pSystemTimePayload
);

#ifdef OS_BUILD
extern UINT64 EFIAPI GetBsr(DIMM *pDimm);
#define BSR(pDimm) GetBsr(pDimm)
#else //OS_BUILD
#define BSR(pDimm) *(pDimm->pHostMailbox->pBsr)
#endif //OS_BUILD
#endif
