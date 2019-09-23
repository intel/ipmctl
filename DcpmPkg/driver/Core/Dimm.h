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
#include <DcpmmTypes.h>

#ifdef OS_BUILD
#define FW_CMD_ERROR_TO_EFI_STATUS(pFwCmd, ReturnCode) \
  if (FW_ERROR(pFwCmd->Status)) { \
    NVDIMM_ERR("Firmware cmd 0x%x:0x%x failed! FIS Error code: 0x%x", pFwCmd->Opcode, pFwCmd->SubOpcode, pFwCmd->Status); \
    ReturnCode = MatchFwReturnCode(pFwCmd->Status); \
  } \
  else if (DSM_ERROR(pFwCmd->DsmStatus)) { \
    NVDIMM_ERR("DSM for fw cmd 0x%x:0x%x failed! DSM Error code: 0x%x", pFwCmd->Opcode, pFwCmd->SubOpcode, pFwCmd->DsmStatus); \
    ReturnCode = MatchDsmReturnCode(pFwCmd->DsmStatus); \
  }
#else
#define FW_CMD_ERROR_TO_EFI_STATUS(pFwCmd, ReturnCode) \
  if (FW_ERROR(pFwCmd->Status)) { \
    NVDIMM_ERR("Firmware cmd 0x%x:0x%x failed! FIS Error code: 0x%x", pFwCmd->Opcode, pFwCmd->SubOpcode, pFwCmd->Status); \
    ReturnCode = MatchFwReturnCode(pFwCmd->Status); \
  }
#endif

//---> Turn on/off large payload support
#define USE_LARGE_PAYLOAD
//<---
#define DB_SHIFT 48
#define DB_SHIFT_32 48-32                      //!< DB_SHIFT in UINT32 half of command register
#define SUB_OP_SHIFT 40
#define OP_SHIFT 32
#define SQ_SHIFT 63
#define EXT_SUB_OP_SHIFT 8

#define EMULATOR_DIMM_HEALTH_STATUS       0    //!< Normal
#define EMULATOR_DIMM_TEMPERATURE         300  //!< 300K is about 26C
#define EMULATOR_DIMM_TEMPERATURE_THR     310  //!< 310K is about 35C
#define EMULATOR_DIMM_PERCENTAGE_REMAINING      75   //!< 75% of percentage remaining
#define EMULATOR_DIMM_PERCENTAGE_REMAINING_THR  5    //!< 5% of percentage remaining
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

#define DEBUG_LOG_PAYLOAD_TYPE_LARGE 0
#define DEBUG_LOG_PAYLOAD_TYPE_SMALL 1

//
// Translate between the NFIT device handle node/socket pair and an absolute socket index
// The 4 bit Socket ID field allows a maximum of 16 sockets per node
//
#define NFIT_SOCKETS_PER_NODE                                   16
#define SOCKET_INDEX_TO_NFIT_SOCKET_ID(_skt)                    (_skt % NFIT_SOCKETS_PER_NODE)
#define SOCKET_INDEX_TO_NFIT_NODE_ID(_skt)                      (_skt / NFIT_SOCKETS_PER_NODE)
#define NFIT_NODE_SOCKET_TO_SOCKET_INDEX(_nodeId, _socketId)    ((_nodeId * NFIT_SOCKETS_PER_NODE) + (_socketId))

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
  UINT16 SubsystemRid;                     //!< Revision ID of the subsystem memory controller from NFIT
  SKU_INFORMATION SkuInformation;
  /**
    Format interface code: Allows vendor hardware to be handled by a generic
    driver (behaves similar to class code in PCI)
  **/
  UINT16 FmtInterfaceCode[MAX_IFC_NUM];
  UINT32 FmtInterfaceCodeNum;

  FIRMWARE_VERSION FwVer;                  //!< Struct containing firmware version details

  UINT64 RawCapacity;                      //!< PM + volatile
  UINT64 VolatileStart;                    //!< DPA start of the Volatile region
  UINT64 VolatileCapacity;                 //!< Capacity in bytes mapped as volatile memory
  UINT64 PmStart;                          //!< DPA start of the PM region
  UINT64 PmCapacity;                       //!< DIMM Capacity (Bytes) to reserve for PM
  UINT64 InaccessibleVolatileCapacity;     //!< Capacity in bytes for use as volatile memory that has not been exposed
  UINT64 InaccessiblePersistentCapacity;   //!< Capacity in bytes for use as persistent memory that has not been exposed
  struct _DIMM_REGION *pIsRegions[MAX_IS_PER_DIMM];
  UINT32 IsRegionsNum;
  struct _DIMM_REGION *pIsRegionsNfit[MAX_IS_PER_DIMM];
  UINT32 IsRegionsNfitNum;

  UINT8 IsNew;                             //!< if is incorporated with the rest of the DIMMs in the system
  UINT8 RebootNeeded;                      //!< Whether or not reboot is required to reconfigure dimm

  UINT8 LsaStatus;                         //!< The status of the LSA partition parsing for this DIMM

  BLOCK_WINDOW *pBw;
  NvDimmRegionMappingStructure *pRegionMappingStructure;      //!< ptr to the table used to configure the mailbox
  SpaRangeTbl *pCtrlSpaTbl;       //!> ptr to the spa range table associated with the mailbox table
  NvDimmRegionMappingStructure *pBlockDataRegionMappingStructure;      //!< ptr to the table used to configure the block windows
  SpaRangeTbl *pBlockDataSpaTbl;       //!< ptr to the spa range table associated with the block windows table
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
  struct _NVM_IS *pISsNfit[MAX_IS_PER_DIMM];
  UINT32 ISsNfitNum;

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

  UINT16 ControllerRid;             //!< Revision ID of the subsystem memory controller from FIS

  // If the dimm was declared non-functional during our driver initialization
  BOOLEAN NonFunctional;

  DIMM_BSR Bsr;
  UINT16 BootStatusBitmask;

  /*
  A pointer to a cached copy of the LABEL_STORAGE_AREA for this DIMM. This
  is only used during namespace initialzation so it doesn't need to be repeatedly
  reloaded. It should not be considered current outside of initialization.
  */
  LABEL_STORAGE_AREA *pLsa;
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

#define DISABLE_ARS_TOTAL_TIMEOUT_SEC     2
#define POLL_ARS_LONG_OP_DELAY_US         100000  //100ms delay between calls to retreive long op
#define MAX_FW_UPDATE_RETRY_ON_DEV_BUSY   3
#define DSM_RETRY_SUGGESTED               0x5


#ifdef OS_BUILD
#define INI_PREFERENCES_LARGE_PAYLOAD_DISABLED L"LARGE_PAYLOAD_DISABLED"
/*
* Function get the ini configuration only on the first call
*
* It returns TRUE in case of large payload access is disabled and FALSE otherwise
*/
BOOLEAN ConfigIsLargePayloadDisabled();

#define INI_PREFERENCES_DDRT_PROTOCOL_DISABLED L"DDRT_PROTOCOL_DISABLED"
/*
* Function get the ini configuration only on the first call
*
* It returns TRUE in case of DDRT protocol access is disabled and FALSE otherwise
*/
BOOLEAN ConfigIsDdrtProtocolDisabled();
#endif // OS_BUILD

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

/**
  Create DIMM
  Perform all functions needed for DIMM initialization this includes:
  setting up mailbox structure
  retrieving and recording security status
  retrieving and recording the FW version
  retrieving and recording partition information
  setting up block windows

  @param[in] pNewDimm: input dimm structure to populate
  @param[in] pFitHead: fully populated NVM Firmware Interface Table
  @param[in] pPmttHead: fully populated Platform Memory Topology Table
  @param[in] Pid: SMBIOS Dimm ID of the DIMM to create

  @retval EFI_SUCCESS          - Success
  @retval EFI_OUT_OF_RESOURCES - AllocateZeroPool failure
  @retval EFI_DEVICE_ERROR     - Other errors
**/
EFI_STATUS
InitializeDimm (
  IN     DIMM *pNewDimm,
  IN     ParsedFitHeader *pFitHead,
  IN     ParsedPmttHeader *pPmttHead,
  IN     UINT16 Pid
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
  Firmware command get security Opt-In
  Execute a FW command to check the security Opt-In code of a DIMM

  @param[in] pDimm: The DIMM to retrieve security info on
  @param[in] OptInCode: Opt-In Code that is requested status for
  @param[out] pSecurityOptIn: Area to place the returned from FW

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
  @retval Various errors from FW are still TBD
**/
EFI_STATUS
FwCmdGetSecurityOptIn(
  IN     DIMM *pDimm,
  IN     UINT16 OptInCode,
  OUT PT_OUTPUT_PAYLOAD_GET_SECURITY_OPT_IN *pSecurityOptIn
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
  Firmware command to disable ARS

  @param[in] pDimm Pointer to the DIMM to retrieve ARSStatus on

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES  Memory allocation failure
  @retval Various errors from FW
**/
EFI_STATUS
FwCmdDisableARS(
  IN     DIMM *pDimm
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
  Firmware command to get Command Effect Log Entries

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetCommandEffectLog(
  IN      DIMM  *pDimm,
  IN      PT_INPUT_PAYLOAD_GET_COMMAND_EFFECT_LOG *pInputPayload,
  OUT VOID *pOutputPayload OPTIONAL,
  IN      UINT32 OutputPayloadSize OPTIONAL,
  OUT VOID *pLargeOutputPayload OPTIONAL,
  IN      UINT32 LargeOutputPayloadSize OPTIONAL
);

/**
  Firmware command to get a specified debug log

  @param[in]  pDimm Target DIMM structure pointer
  @param[in]  LogSource Debug log source buffer to retrieve
  @param[out] ppDebugLogBuffer - an allocated buffer containing the raw debug logs
  @param[out] pDebugLogBufferSize - the size of the raw debug log buffer
  @param[out] pCommandStatus structure containing detailed NVM error codes

  Note: The caller is responsible for freeing the returned buffers

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetFwDebugLog (
  IN     DIMM *pDimm,
  IN     UINT8 LogSource,
     OUT VOID **ppDebugLogBuffer,
     OUT UINTN *pDebugLogBufferSize,
     OUT COMMAND_STATUS *pCommandStatus
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
FwCmdGetFwDebugLogSize(
  IN     DIMM *pDimm,
     OUT UINT64 *pLogSizeInMb
  );
/**
  Firmware command Identify DIMM.
  Execute a FW command to get information about DIMM.

  @param[in] pDimm The Intel NVM Dimm to retrieve identify info on
  @param[out] pPayload Area to place the identity info returned from FW

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
**/
EFI_STATUS
FwCmdIdDimm(
  IN     DIMM *pDimm,
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
     OUT PT_DEVICE_CHARACTERISTICS_OUT **ppPayload
  );

/**
  Firmware command access/read Platform Config Data using small payload only.

  The function allows to specify the requested data offset and the size.
  The function is going to allocate the ppRawData buffer if it is not allocated.
  The buffer's minimal size is the size of the Partition!

  @param[in] pDimm The Intel NVM Dimm to retrieve identity info on
  @param[in] PartitionId Partition number to get data from
  @param[in] ReqOffset Data read starting point
  @param[in] ReqDataSize Number of bytes to read
  @param[out] Pointer to the buffer pointer for storing retrieved data

  @retval EFI_SUCCESS Success
  @retval Error code
**/
EFI_STATUS
FwGetPCDFromOffsetSmallPayload(
  IN  DIMM *pDimm,
  IN  UINT8 PartitionId,
  IN  UINT32 ReqOffset,
  IN  UINT32 ReqDataSize,
  OUT UINT8 **ppRawData);

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
  Firmware command access/write Platform Config Data using small payload only.

  The function allows to specify the requested data offset and the size.
  The buffer's minimal size is the size of the Partition!

  @param[in] pDimm The Intel NVM Dimm to send Platform Config Data to
  @param[in] PartitionId Partition number for data to be send to
  @param[in] pRawData Pointer to a data buffer that will be sent to the DIMM
  @param[in] ReqOffset Data write starting point
  @param[in] ReqDataSize Number of bytes to write

  @retval EFI_SUCCESS Success
  @retval Error code
**/
EFI_STATUS
FwSetPCDFromOffsetSmallPayload(
  IN  DIMM *pDimm,
  IN  UINT8 PartitionId,
  IN  UINT8 *pRawData,
  IN  UINT32 ReqOffset,
  IN  UINT32 ReqDataSize);

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

  @param[in] pDimm The Intel DCPMM to retrieve Power Management Policy Info
  @param[out] ppPayloadPowerManagementPolicy Area to place Power Management Policy Info data
    The caller is responsible to free the allocated memory with the FreePool function.

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pDimm or ppPayloadPowerManagementPolicy is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
FwCmdGetPowerManagementPolicy(
  IN     DIMM *pDimm,
     OUT PT_POWER_MANAGEMENT_POLICY_OUT **ppPayloadPowerManagementPolicy
  );

#ifdef OS_BUILD

/**
  Firmware command to get PMON Info

  @param[in] pDimm The DC PMEM Module to retrieve PMON Info
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
  OUT    PMON_REGISTERS *pPayloadPMONRegisters
  );

/**
  Firmware command to set PMON Info

  @param[in] pDimm The DC PMEM Module to retrieve PMON Info
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
  @param[in] RegularBufferSz size of the RegualrBuffer
  @param[in] ppInterleavedBuffer input interleaved buffer
  @param[in] LineSize line size of interleaved buffer
  @param[in] NumOfBytes number of bytes to copy
**/
VOID
ReadFromInterleavedBuffer(
  OUT VOID *pRegularBuffer,
  IN     UINTN RegularBufferSz,
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
  Get Platform Config Data OEM partition Intel config region and check a correctness of header.
  We only return the actua PCD config data, from the first 64KiB of Intel FW/SW config metadata.
  The latter 64KiB is reserved for OEM use.

  The caller is responsible for a memory deallocation of the ppPlatformConfigData

  @param[in] pDimm The Intel NVM Dimm to retrieve PCD from
  @param[in] RetoreCorrupt If true will generate a default PCD when a corrupt header is found
  @param[out] ppPlatformConfigData Pointer to a new buffer pointer for storing retrieved data

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR Incorrect PCD header
  @retval Other return codes from GetPcdOemConfigDataUsingSmallPayload
**/
EFI_STATUS
GetPlatformConfigDataOemPartition(
  IN     DIMM *pDimm,
  IN     BOOLEAN RestoreCorrupt,
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
  Firmware command Get Viral Policy
  Execute a FW command to check the security status of a DIMM

  @param[in] pDimm The DIMM to retrieve viral policy
  @param[out] pViralPolicyPayload buffer to retrieve DIMM FW response

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Paramter supplied is invalid
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval Various errors from FW
**/
EFI_STATUS
FwCmdGetViralPolicy(
  IN     DIMM *pDimm,
  OUT    PT_VIRAL_POLICY_PAYLOAD *pViralPolicyPayload
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

#ifdef OS_BUILD
/**
  Matches DSM return code to one of available EFI_STATUS EFI base types

  @param[in] DsmStatus - status byte returned from FW command

  @retval - Appropriate EFI_STATUS
**/
EFI_STATUS
MatchDsmReturnCode(
  IN     UINT8 DsmStatus
);
#endif

/**
  Check if SKU conflict occurred.
  Any mixed modes between DIMMs are prohibited on a platform.

  @param[in] pDimm1 - first DIMM to compare SKU mode
  @param[in] pDimm2 - second DIMM to compare SKU mode

  @retval NVM_SUCCESS - if everything went fine
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

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter
**/
EFI_STATUS
FwCmdFormatDimm(
  IN    DIMM *pDimm
  );

/**
  Firmware command to get DDRT IO init info

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pDdrtIoInitInfo pointer to filled payload with DDRT IO init info

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdGetDdrtIoInitInfo(
  IN     DIMM *pDimm,
     OUT PT_OUTPUT_PAYLOAD_GET_DDRT_IO_INIT_INFO *pDdrtIoInitInfo
  );

/**
  Get Command Access Policy for a specific command
  @param[IN] pDimm Target DIMM structure pointer
  @param[IN] Opcode for the command
  @param[IN] SubOpcode for the command
  @param[OUT] pRestricted TRUE if restricted, else FALSE

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdGetCommandAccessPolicy(
  IN  DIMM *pDimm,
  IN  UINT8 Opcode,
  IN  UINT8 Subopcode,
  OUT BOOLEAN *pRestricted
);

/**
  Inject Temperature error payload
  @param[IN] pDimm Target DIMM structure pointer
  @param[IN] subopcode for error injection command
  @param[OUT] pinjectInputPayload - input payload to be sent
  @param[OUT] pFwStatus FW status returned by dimm

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
FwCmdInjectError(
  IN     DIMM *pDimm,
  IN     UINT8 SubOpcode,
  OUT void *pinjectInputPayload,
  OUT UINT8 *pFwStatus
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

/**
  Firmware command to get extended ADR status info

  @param[in] pDimm Target DIMM structure pointer
  @param[out] pExtendedAdrInfo pointer to filled payload with extended ADR info

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR if failed to open PassThru protocol
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
  @retval EFI_UNSUPPORTED if FIS doesn't support Get Admin Features/Extended ADR
**/
EFI_STATUS
FwCmdGetExtendedAdrInfo(
  IN     DIMM *pDimm,
  OUT PT_OUTPUT_PAYLOAD_GET_EADR *pExtendedAdrInfo
);

/**
Get manageability state for Dimm

@param[in] pDimm the DIMM struct

@retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmManageable(
  IN  DIMM *pDimm
);

/**
Get supported configuration state for Dimm

@param[in] pDimm the DIMM struct

@retval BOOLEAN whether or not dimm is in supported config
**/
BOOLEAN
IsDimmInSupportedConfig(
  IN  DIMM *pDimm
);

/**
Check if the dimm interface code of this DIMM is supported

@param[in] pDimm the DIMM struct

@retval true if supported, false otherwise
**/
BOOLEAN
IsDimmInterfaceCodeSupported(
  IN  DIMM *pDimm
);

/**
Check if the subsystem device ID of this DIMM is supported

@param[in] pDimm the DIMM struct

@retval true if supported, false otherwise
**/
BOOLEAN
IsSubsystemDeviceIdSupported(
  IN  DIMM *pDimm
);

/**
Check if current firmware API version is supported

@param[in] pDimm the DIMM struct

@retval true if supported, false otherwise
**/
BOOLEAN
IsFwApiVersionSupported(
  IN  DIMM *pDimm
);

/**
Clears the PCD Cache on each DIMM in the global DIMM list

@retval EFI_SUCCESS Success
**/
EFI_STATUS ClearPcdCacheOnDimmList(VOID);

/**
  Set Obj Status when DIMM is not found using Id expected by end user

  @param[in] DimmId the Pid for the DIMM that was not found
  @param[in] pDimms Pointer to head of list where DimmId should be found
  @param[out] pCommandStatus Pointer to command status structure

**/
VOID
SetObjStatusForDimmNotFound(
  IN     UINT16 DimmId,
  IN     LIST_ENTRY *pDimms,
  OUT COMMAND_STATUS *pCommandStatus
);

/**
Set object status for DIMM

@param[out] pCommandStatus Pointer to command status structure
@param[in] pDimm DIMM for which the object status is being set
@param[in] Status Object status to set
@param[in] If TRUE - clear all other status before setting this one
**/
VOID
SetObjStatusForDimmWithErase(
  OUT COMMAND_STATUS *pCommandStatus,
  IN     DIMM *pDimm,
  IN     NVM_STATUS Status,
  IN     BOOLEAN EraseFirst
);

/**
Determine the total size of PCD Config Data area by finding the largest
offset any of the 3 data sets.

@param[in]  pOemHeader    Pointer to NVDIMM Configuration Header
@param[out] pOemDataSize  Size of the PCD Config Data

@retval EFI_INVALID_PARAMETER NULL pointer for DIMM structure provided
@retval EFI_SUCCESS           Success
**/
EFI_STATUS GetPcdOemDataSize(
  NVDIMM_CONFIGURATION_HEADER *pOemHeader,
  UINT32 *pOemDataSize
);

/**
  Check if sending a large payload command over the DDRT large payload
  mailbox is possible. Used by callers often to determine chunking behavior.

  @param[in] pDimm The DCPMM to retrieve information on

  @retval TRUE: DDRT large payload mailbox is available
  @retval FALSE: DDRT large payload mailbox is not available
**/
BOOLEAN
IsLargePayloadAvailable(
  IN DIMM *pDimm
);

EFI_STATUS
PassThru(
  IN     struct _DIMM *pDimm,
  IN OUT FW_CMD *pCmd,
  IN     UINT64 Timeout
);

/**
  Makes Bios emulated pass thru call and acquires the DCPMM Boot
  Status Register

  @param[in] pDimm The DCPMM to retrieve identify info on
  @param[out] pBsrValue Pointer to memory to copy BSR value to

  @retval EFI_SUCCESS: Success
  @retval EFI_OUT_OF_RESOURCES: memory allocation failure
**/

EFI_STATUS
EFIAPI
FwCmdGetBsr(
  IN     DIMM *pDimm,
     OUT UINT64 *pBsrValue
);

/**
  Gather boot status register value and populate the boot status bitmask

  @param[in] pDimm to retrieve DDRT Training status from
  @param[out] pBsr BSR Boot Status Register to retrieve and convert to bitmask
  @param[out] pBootStatusBitmask Pointer to the boot status bitmask

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/

EFI_STATUS
PopulateDimmBsrAndBootStatusBitmask(
  IN     DIMM *pDimm,
  OUT DIMM_BSR *pBsr,
  OUT UINT16 *pBootStatusBitmask
);

/**
  Passthrough FIS command by Dcpmm BIOS protocol.

  @param[in] pDimm Target DIMM structure pointer
  @param[in, out] pCmd Firmware command structure pointer
  @param[in] Timeout Optional command timeout in microseconds
  @param[in] DcpmmInterface Interface for FIS request

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
DcpmmCmd(
  IN     struct _DIMM *pDimm,
  IN OUT FW_CMD *pCmd,
  IN     UINT32 Timeout OPTIONAL,
  IN     DCPMM_FIS_INTERFACE DcpmmInterface
);

/**
  Get large payload info by Dcpmm BIOS protocol.

  @param[in] pDimm Target DIMM structure pointer
  @param[in] Timeout Optional command timeout in microseconds
  @param[in] DcpmmInterface Interface for FIS request
  @param[out] pOutput Large payload info output data buffer
  @param[out] pStatus FIS request status

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
DcpmmLargePayloadInfo(
  IN     struct _DIMM *pDimm,
  IN     UINT32 Timeout OPTIONAL,
  IN     DCPMM_FIS_INTERFACE DcpmmInterface,
     OUT DCPMM_FIS_OUTPUT *pOutput,
     OUT UINT8 *pStatus
);

/**
  Write large payload by Dcpmm BIOS protocol.

  @param[in] pDimm Target DIMM structure pointer
  @param[in] pInput Input data buffer
  @param[in] InputSize Total input data size
  @param[in] MaxChunkSize Maximum chunk of data to write
  @param[in] Timeout Optional command timeout in microseconds
  @param[in] DcpmmInterface Interface for FIS request
  @param[out] pStatus FIS request status

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
DcpmmLargePayloadWrite(
  IN     struct _DIMM *pDimm,
  IN     UINT8 *pInput,
  IN     UINT32 InputSize,
  IN     UINT32 MaxChunkSize,
  IN     UINT32 Timeout OPTIONAL,
  IN     DCPMM_FIS_INTERFACE DcpmmInterface,
     OUT UINT8 *pStatus
);

/**
  Read large payload by Dcpmm BIOS protocol.

  @param[in] pDimm Target DIMM structure pointer
  @param[in] OutputSize Total output data size
  @param[in] MaxChunkSize Maximum chunk of data to read
  @param[in] Timeout Optional command timeout in microseconds
  @param[in] DcpmmInterface Interface for FIS request
  @param[out] pOutput Output data buffer
  @param[out] pStatus FIS request status pointer

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
DcpmmLargePayloadRead(
  IN     struct _DIMM *pDimm,
  IN     UINT32 OutputSize,
  IN     UINT32 MaxChunkSize,
  IN     UINT32 Timeout OPTIONAL,
  IN     DCPMM_FIS_INTERFACE DcpmmInterface,
  IN OUT UINT8 *pOutput,
     OUT UINT8 *pStatus
);
#endif
