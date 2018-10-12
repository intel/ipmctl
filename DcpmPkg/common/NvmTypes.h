/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file NvmTypes.h
 * @brief Types for EFI_NVMDIMMS_CONFIG_PROTOCOL to configure and manage DCPMMs.
 */

#ifndef _NVM_TYPES_H_
#define _NVM_TYPES_H_

#include "NvmLimits.h"
#include "NvmWorkarounds.h"
#ifdef OS_BUILD
#include <AutoGen.h>
#include "os_types.h"
#include <NvmSharedDefs.h>
#endif // !OS_BUILD
#include <FormatStrings.h>

#define MAX_UINT8_VALUE  0xFF
#define MAX_UINT16_VALUE 0xFFFF
#define MAX_UINT32_VALUE 0xFFFFFFFF
#define MAX_UINT64_VALUE 0xFFFFFFFFFFFFFFFFULL

/**
  Needed for hii form.
  There is an error that the HII form is always displaying the value as signed, so we must skip the highest bit,
  so that we will always stay at the positive numbers.
**/
#define MAX_UINT31_VALUE 0x7FFFFFFF
#define MAX_UINT63_VALUE 0x7FFFFFFFFFFFFFFF

/**
  Those defines describe the amount of bits and digits
  for the internal loops in UINT128 to string functions.
**/
#define UINT32_DEC_MB_STRING_SIZE  12 //!< Length of string with decimal UINT32 value with "MB" suffix
#define UINT8_HEX_STRING_SIZE      6
#define UINT16_HEX_STRING_SIZE     14
#define UINT32_HEX_STRING_SIZE     26
#define UINT128_DIGITS             39
#define HII_CAPACITY_STRING_SIZE   19
#define HII_SENSOR_VALUE_STRING_SIZE   19
#define HII_HEALTH_REASON_STRNG_SIZE   110
#define HII_ISET_ID_STRNG_SIZE   64
#define HII_DIMM_ID_STRNG_SIZE   48
#define HII_APPDIRECT_PREFERENCE_OPTION_SIZE 30

/** UINT128_DIGITS + 1; it has to be a number because of Vfr compilator requirements **/
#define UINT128_STRING_SIZE        40
#define UINT64_BITS                64

#define REGION_TYPE_STR_SIZE               50
#define REGION_HEALTH_STR_SIZE             9
#define REGION_GOAL_SETTINGS_STR_SIZE      32
#define REGION_GOAL_INDEX_STR_SIZE         10

#define NSLABEL_NAME_LEN                 63 //!< Namespace name length
#define NLABEL_NAME_LEN_WITH_TERMINATOR  64 //!< Namespace name length + null terminator
#define NSGUID_LEN                       16 //!< Length of GUID in bytes
#define GUID_STR_LEN                     36 //!< Length of GUID string representation (in characters)
#define LIST_ENTRY_SIZE                  16 //!< Size of list entry field (2 pointers)
#define SENSOR_HEALTH_STR_SIZE           19
#define LABEL_VERSION_STR_SIZE           4
#define HEALTH_STR_SIZE            19

#define FEATURE_NOT_SUPPORTED 0
#define FEATURE_SUPPORTED     1

typedef struct {
  UINT64  Uint64;
  UINT64  Uint64_1;
} UINT128;

#define FW_BCD_VERSION_LEN              5
#define FW_VERSION_LEN                 19
#ifdef OS_BUILD
#define FW_API_VERSION_LEN              20
#else
#define FW_API_VERSION_LEN              6
#endif
#define MANUFACTURER_LEN               19
#define SERIAL_NUMBER_LEN              19
#define PART_NUMBER_LEN                21
#define PART_NUMBER_STR_LEN            22
#define DEVICE_LOCATOR_LEN            128
#define BANKLABEL_LEN                  32 // @todo confirm label length
#define SHUTDOWN_STATUS_LEN           136 // @todo confirm label length
#define SECURITYCAPABILITES_LEN        18 // @todo confirm label length
#define IFC_STRING_LEN                255
#define MEMORY_MODES_LEN               40 // @todo confirm label length
#define MEMORY_TYPE                    18 // @todo confirm label length
#define BOOT_STATUS_LEN               100
#define FW_UPDATE_STATUS_LEN           64
#define FW_BUILD_LEN                   16
#define FW_BUILD_STR_LEN               17
#define FW_COMMIT_ID_LEN               40
#define FW_COMMIT_ID_STR_LEN           41
#define DATE_STR_LEN                   29
#define SW_TRIG_ENABLED_DETAILS_LEN   120
#define CONTROLLER_RID_LEN             12

/** DIMM UID length, including null terminator **/
#define MAX_DIMM_UID_LENGTH      22   //!< DIMM UID hexadecimal-format representation length, including manufacturing fields
#define MIN_DIMM_UID_LENGTH      14   //!< DIMM UID hexadecimal-format representation length, excluding manufacturing fields
#define NVM_EVENT_MSG_LEN        1024 // Length of event message string

#ifdef OS_BUILD
#define	NVM_SYSLOG_SOURCE	"NVM_MGMT"
#define	NVM_SYSLOG_SRC_W	L"NVM_MGMT"
#define NVM_DEBUG_LOGGER_SOURCE "NVM_DBG_LOGGER"
#define NVM_DIMM_NAME       "Intel Persistent Memory Module"
#define MAX_SOURCE_STR_LENGTH    32

typedef char NVM_EVENT_MSG[NVM_EVENT_MSG_LEN]; // Event message string
typedef wchar_t NVM_EVENT_MSG_W[NVM_EVENT_MSG_LEN]; // Event message string
#endif // OS_BUILD



/** Memory modes of Memory Interleave Capability Information table **/
#define PCAT_MEMORY_MODE_1LM        0
#define PCAT_MEMORY_MODE_2LM        1
#define PCAT_MEMORY_MODE_PM_DIRECT  3
#define PCAT_MEMORY_MODE_PM_CACHED  4

#define MODE_DISABLED  0
#define MODE_ENABLED 1

#define CHANNEL_INTERLEAVE_SIZE_VARIABLE_NAME L"CHANNEL_INTERLEAVE_SIZE"

/** Channel interleave size **/
#define CHANNEL_INTERLEAVE_SIZE_INVALID MAX_UINT8_VALUE
#define CHANNEL_INTERLEAVE_SIZE_64B     BIT0
#define CHANNEL_INTERLEAVE_SIZE_128B    BIT1
#define CHANNEL_INTERLEAVE_SIZE_256B    BIT2
#define CHANNEL_INTERLEAVE_SIZE_4KB     BIT6
#define CHANNEL_INTERLEAVE_SIZE_1GB     BIT7

#define IMC_INTERLEAVE_SIZE_VARIABLE_NAME L"IMC_INTERLEAVE_SIZE"

#define APPDIRECT_GRANULARITY_1GIB          1
#define APPDIRECT_GRANULARITY_32GIB         0
#define APPDIRECT_GRANULARITY_MAX           APPDIRECT_GRANULARITY_1GIB
#define APPDIRECT_GRANULARITY_DEFAULT       APPDIRECT_GRANULARITY_32GIB

#define APPDIRECT_GRANULARITY_VARIABLE_NAME L"APPDIRECT_GRANULARITY"

/** IMC interleave size **/
#define IMC_INTERLEAVE_SIZE_INVALID MAX_UINT8_VALUE
#define IMC_INTERLEAVE_SIZE_64B     BIT0
#define IMC_INTERLEAVE_SIZE_128B    BIT1
#define IMC_INTERLEAVE_SIZE_256B    BIT2
#define IMC_INTERLEAVE_SIZE_4KB     BIT6
#define IMC_INTERLEAVE_SIZE_1GB     BIT7

/** Number of channel ways **/
#define INTERLEAVE_SET_1_WAY  BIT0
#define INTERLEAVE_SET_2_WAY  BIT1
#define INTERLEAVE_SET_3_WAY  BIT2
#define INTERLEAVE_SET_4_WAY  BIT3
#define INTERLEAVE_SET_6_WAY  BIT4
#define INTERLEAVE_SET_8_WAY  BIT5
#define INTERLEAVE_SET_12_WAY BIT6
#define INTERLEAVE_SET_16_WAY BIT7
#define INTERLEAVE_SET_24_WAY BIT8

/**
  Persistent memory types

  VFR compiler supports only basic #defines, that's why bare values are used
**/
#define PM_TYPE_AD           0x1 // BIT0
#define PM_TYPE_AD_NI        0x2 // BIT1
#define PM_TYPE_STORAGE      0x4 // BIT2
#define PM_TYPE_ALL          0x7 // (PM_TYPE_AD | PM_TYPE_AD_NI | PM_TYPE_STORAGE)

#define RECOMMENDED_SETTINGS         BIT0

/** Namespace Label Version - int representation of version **/
#define NS_LABEL_VERSION_LATEST  0
#define NS_LABEL_VERSION_1_1     1
#define NS_LABEL_VERSION_1_2     2

/**
 * @defgroup GOAL_CONFIG_STATUS Goal configuration status
 * @{
 */
#define GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS     0   ///< No Goal or goal applied successfully
#define GOAL_CONFIG_STATUS_UNKNOWN                1   ///< Unknown status
#define GOAL_CONFIG_STATUS_NEW                    2   ///< Goal is new, not yet applied. Reboot required to apply.
#define GOAL_CONFIG_STATUS_BAD_REQUEST            3   ///< Goal request is invalid
#define GOAL_CONFIG_STATUS_NOT_ENOUGH_RESOURCES   4   ///< Unable to apply goal because not enough resources
#define GOAL_CONFIG_STATUS_FIRMWARE_ERROR         5   ///< Unable to apply goal because of a FW error
#define GOAL_CONFIG_STATUS_FAILED_UNKNOWN         6   ///< Unable to apply goal. Internal error

/**
 * @}
 * Firmware types
 */
#define FW_TYPE_PRODUCTION  29
#define FW_TYPE_DFX         30
#define FW_TYPE_DEBUG       31

/**
  In UDK2010.SR1.UP1 Hii parser does not understand '*' symbol and so pointers cannot be used
  in structures visible to VFR form. One solution is to use 64-bit integer as a pointer and
  cast whenever necessary.
**/
#define HII_POINTER UINT64
#define SUPPORTED_BLOCK_SIZES_COUNT 8

/**
  Intel NVM Dimm format interface code
  The current code should be 0x201 for Storage mode and 0x301 for AppDirect mode
  We should use the values that are reported by BIOS
**/
#define DCPMM_FMT_CODE_STORAGE     0x201
#define DCPMM_FMT_CODE_APP_DIRECT  0x301

typedef struct _FIRMWARE_VERSION {
  UINT8 FwProduct;                         //!< FW Version Product Number
  UINT8 FwRevision;                        //!< FW Version Revision Number
  UINT8 FwSecurityVersion;                 //!< FW Version Security Version Number
  UINT8 FwApiMajor;                        //!< FW API Version Major
  UINT8 FwApiMinor;                        //!< FW API Version Minor
  UINT16 FwBuild;                          //!< FW Version Build Number
} FIRMWARE_VERSION;

/**
  SMBus DIMM address
**/
typedef struct _SMBUS_DIMM_ADDR {
  UINT8 Cpu;
  UINT8 Imc;
  UINT8 Slot;
} SMBUS_DIMM_ADDR;

/*namespace mode fsdax or sector*/
#define NONE_MODE 0
#define FSDAX_MODE 1
#define SECTOR_MODE 2


/* VFR compiler doesn't support typedef, that's why we use defines **/
#define DIMM_INFO_CATEGORIES        UINT16                          ///< @ref DIMM_INFO_CATEGORY_TYPES

/**
 * @defgroup DIMM_INFO_CATEGORY_TYPES DIMM Info Category Types
 * @{
 */
#define DIMM_INFO_CATEGORY_NONE                         (0)         ///< No DIMM_INFO fields will be populated
#define DIMM_INFO_CATEGORY_RESERVED                     (1 << 0)    ///< Reserved
#define DIMM_INFO_CATEGORY_SECURITY                     (1 << 1)    ///< Security fields will be populated: SecurityState.
#define DIMM_INFO_CATEGORY_PACKAGE_SPARING              (1 << 2)    ///< Package sparing fields will be populated: PackageSparingEnabled, PackageSparesAvailable.
#define DIMM_INFO_CATEGORY_ARS_STATUS                   (1 << 3)    ///< ARS status field will be populated: ARSStatus.
#define DIMM_INFO_CATEGORY_SMART_AND_HEALTH             (1 << 4)    ///< Health related fields will be populated: HealthStatusReason, LatchedLastShutdownStatus, LastShutdownTime, AitDramEnabled.
#define DIMM_INFO_CATEGORY_POWER_MGMT_POLICY            (1 << 5)    ///< Power management fields will be populated: PeakPowerBudget, AvgPowerBudget.
#define DIMM_INFO_CATEGORY_OPTIONAL_CONFIG_DATA_POLICY  (1 << 6)    ///< Optional config data policy fields will be populated: FirstFastRefresh.
#define DIMM_INFO_CATEGORY_OVERWRITE_DIMM_STATUS        (1 << 7)    ///< Overwrite DIMM status field will be populated: OverwriteDimmStatus.
#define DIMM_INFO_CATEGORY_FW_IMAGE_INFO                (1 << 8)    ///< Firmware Image info fields will be populated: LastFwUpdateStatus, StagedFwVersion, FWImageMaxSize.
#define DIMM_INFO_CATEGORY_MEM_INFO_PAGE_3              (1 << 9)    ///< Memory info page 3 fields will be populated: ErrorInjectionEnabled, MediaTemperatureInjectionEnabled, SoftwareTriggersEnabled, PoisonErrorInjectionsCounter, PoisonErrorClearCounter, MediaTemperatureInjectionsCouner, SoftwareTriggersCounter, SoftwareTriggersEnabledDetails.
#define DIMM_INFO_CATEGORY_VIRAL_POLICY                 (1 << 10)   ///< Viral policy fields will be populated: ViralPolicyEnable, ViralStatus.
#define DIMM_INFO_CATEGORY_DEVICE_CHARACTERISTICS       (1 << 11)
#define DIMM_INFO_CATEGORY_ALL                          (0xFFFF)    ///< All DIMM_INFO fields will be populated.

/**
 * @}
 * DIMM_INFO_ERROR types
 */
#define DIMM_INFO_ERROR_NONE                            0
#define DIMM_INFO_ERROR_UID                             (1 << 0)
#define DIMM_INFO_ERROR_SECURITY_INFO                   (1 << 2)
#define DIMM_INFO_ERROR_PACKAGE_SPARING                 (1 << 3)
#define DIMM_INFO_ERROR_SMART_AND_HEALTH                (1 << 4)
#define DIMM_INFO_ERROR_POWER_MGMT                      (1 << 5)
#define DIMM_INFO_ERROR_OPTIONAL_CONFIG_DATA            (1 << 6)
#define DIMM_INFO_ERROR_OVERWRITE_STATUS                (1 << 7)
#define DIMM_INFO_ERROR_CAPACITY                        (1 << 8)
#define DIMM_INFO_ERROR_FW_IMAGE_INFO                   (1 << 9)
#define DIMM_INFO_ERROR_VIRAL_POLICY                    (1 << 10)
#define DIMM_INFO_ERROR_MEM_INFO_PAGE                   (1 << 11)
#define DIMM_INFO_ERROR_MAX                             (1 << 12)
#define DIMM_INFO_ERROR_DEVICE_CHARACTERISTICS                (1 << 13)

// The "global dimm struct" is at &gNvmDimmData->PMEMDev.Dimms and is populated
// at HII driver loading, so they are included by default on any call to GetDimmInfo()
// in NvmDimmConfig.c
typedef struct _DIMM_INFO {
  // From global dimm struct
  UINT16 ManufacturerId;                    //!< Manufacturer number
  BOOLEAN ManufacturingInfoValid;           //!< Validity of manufacturing location and date
  UINT8 ManufacturingLocation;              //!< Manufacturing location
  UINT16 ManufacturingDate;                 //!< Manufacturing data
  UINT32 SerialNumber;                      //!< Serial number string
  CHAR16 PartNumber[PART_NUMBER_STR_LEN];   //!< Part number string
  CHAR16 DeviceLocator[DEVICE_LOCATOR_LEN]; //!< describing the physically-labeled socket or board position
  CHAR16 BankLabel[BANKLABEL_LEN];          //!< identifies the physically labeled bank
  FIRMWARE_VERSION FwVer;                   //!< FNV FW revision
  UINT16 DimmID;                            //!< SMBIOS Type 17 handle
  UINT16 SocketId;                          //!< socket id
  UINT16 InterfaceFormatCode[MAX_IFC_NUM];  //!< format interface codes
  UINT32 InterfaceFormatCodeNum;            //!< number of format interface codes
  UINT8 FormFactor;                         //!< The DIMM form factor
  UINT16 DataWidth;                         //!< The width in bits used to store user data
  UINT16 TotalWidth;                        //!< The width in bits for data and error correction and/or data redundancy
  UINT16 Speed;                             //!< The speed in nanoseconds
  UINT64 CapacityFromSmbios;                //!< The DIMM capacity from SMBIOS in bytes
  UINT64 Capacity;                          //!< DIMM capacity in bytes
  UINT64 VolatileCapacity;                  //!< Capacity in bytes mapped as volatile memory
  UINT64 PmCapacity;                        //!< Capacity in bytes reserved for persistent memory

  //DIMM_INFO_CATEGORY_SECURITY
  UINT8 SecurityState;                      //!< Identifies the security status of the DIMM collected from FW

  //DIMM_INFO_CATEGORY_PACKAGE_SPARING
  BOOLEAN PackageSparingCapable;            //!< Whether or not the DIMM is capable of package sparing
  UINT8 PackageSparingEnabled;              //!< Whether or not the package sparing policy is enabled on the DIMM
  UINT8 PackageSparesAvailable;             //!< Whether or not the DIMM still has package spares available,
                                            //!< and the package spare has not yet been used by the package sparing policy;
                                            //!< this value will be 0 if the DIMM is not package sparing capable as per SKU

  //DIMM_INFO_CATEGORY_ARS_STATUS
  UINT8 ARSStatus;                          //!< Address Range Scrub (ARS) operation status for the DIMM

  //DIMM_INFO_CATEGORY_SMART_AND_HEALTH
  UINT8 HealthState;                        //!< overall health state
  UINT16 HealthStatusReason;                //!< Health state reason(s)
  UINT32 LatchedLastShutdownStatusDetails;  //!< The detailed status of the last shutdown of the DIMM.
  UINT32 UnlatchedLastShutdownStatusDetails; //!< The detailed status of the last shutdown of the DIMM.
  UINT64 LastShutdownTime;                  //!< The time the system was last shutdown.
  UINT8 AitDramEnabled;                     //!< Whether or not the DIMM AIT DRAM is enabled

  //DIMM_INFO_CATEGORY_POWER_MGMT_POLICY
  UINT16 PeakPowerBudget;                   //!< The power budget in mW used for instantaneous power (100-20000 mW). The default is 100 mW.
  UINT16 AvgPowerBudget;                    //!< The power budget in mW used for average power (100-18000 mW). The default is 100 mW.

  //DIMM_INFO_CATEGORY_OPTIONAL_CONFIG_DATA_POLICY
  BOOLEAN FirstFastRefresh;                 //!< true if acceleration of the first refresh cycle is enabled

  //DIMM_INFO_CATEGORY_VIRAL_POLICY
  BOOLEAN ViralPolicyEnable;                //!< true if viral policy is enabled
  BOOLEAN ViralStatus;                      //!< true if the status is viral

  // From global dimm struct
  UINT64 AppDirectCapacity;                 //!< Capacity in bytes mapped as persistent memory
  UINT64 UnconfiguredCapacity;              //!< Total DIMM capacity in bytes that needs further configuration.
  UINT64 ReservedCapacity;                  //!< Total DIMM capacity in bytes that is reserved for metadata.
  UINT64 InaccessibleCapacity;              //!< Capacity in bytes for use that has not been exposed

  //DIMM_INFO_CATEGORY_FW_IMAGE_INFO
  FIRMWARE_VERSION StagedFwVersion;         //!< The current staged firmare version
  UINT32 FWImageMaxSize;                    //!< The maximum size of the Firmware
  UINT8 LastFwUpdateStatus;                 //!< Status of the last FW update

  //DIMM_INFO_CATEGORY_MEM_INFO_PAGE_3
  BOOLEAN ErrorInjectionEnabled;            //!< BIT 0 - Error injection is enabled
  BOOLEAN MediaTemperatureInjectionEnabled; //!< BIT 1 - Media temperature Injection is enabled
  BOOLEAN SoftwareTriggersEnabled;          //!< BIT 2 - One of SW trigger is enabled
  UINT32 PoisonErrorInjectionsCounter;      //!< This counter will be incremented each time the set poison error is successfully executed
  UINT32 PoisonErrorClearCounter;           //!< This counter will be incremented each time the clear poison error is successfully executed
  UINT32 MediaTemperatureInjectionsCounter; //!< This counter will be incremented each time the media temperature is injected
  UINT32 SoftwareTriggersCounter;           //!< This counter is incremented each time a software trigger is enabled
  UINT64 SoftwareTriggersEnabledDetails;    //!< For each bit set, the corresponding trigger is currently enabled.

  // From global dimm struct
  UINT8 ManageabilityState;                 //!< if the DIMM is manageable by this SW
  UINT8 IsNew;                              //!< if is incorporated with the rest of the DCPMMs in the system
  UINT8 RebootNeeded;                       //!< Whether or not reboot is required to reconfigure dimm
  UINT32 SkuInformation;                    //!< Information about SKU modes
  UINT16 VendorId;                          //!< vendor id
  UINT16 DeviceId;                          //!< device id
  UINT16 SubsystemVendorId;                 //!< Vendor id of the subsytem memory controller
  UINT16 SubsystemDeviceId;                 //!< Device id of the subsystem memory controler
  UINT16 Rid;                               //!< revision id
  UINT16 SubsystemRid;                      //!< Revision id of the subsystem memory controller from NFIT
  UINT16 ImcId;                             //!< memory controller id
  UINT16 ChannelId;                         //!< memory channel within an imc
  UINT16 ChannelPos;                        //!< position in the channel within an imc
  UINT16 NodeControllerID;                  //!< The node controller identifier
  UINT8 MemoryType;                         //!< memory type
  UINT8 ConfigStatus;                       //!< ConfigurationStatus code
  UINT8 ModesSupported;                     //!< A list of the modes supported by the DIMM
  BOOLEAN SecurityCapabilities;             //!< The security features supported by the DIMM
  BOOLEAN SKUViolation;                     //!< The configuration of the DIMM is unsupported due to a license issue
  UINT8 OverwriteDimmStatus;                //!< Overwrite DIMM operation status
  BOOLEAN Configured;                       //!< true if the DIMM is configured
  CHAR16 ManufacturerStr[MANUFACTURER_LEN]; //!< Manufacturer string matched from manufacturer string number.

  UINT32 DimmHandle;                        //!< The DIMM handle
  SMBUS_DIMM_ADDR SmbusAddress;             //!< SMBUS address
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];      //!< Globally unique NVDIMM id (in hexadecimal format representation)
  UINT16 ErrorMask;                         //!< Bit mask representing which FW functions failed, see DIMM_INFO_ERROR types

#ifdef OS_BUILD
  CHAR8 ActionRequired;                     //!< Action Required bit, the value stored in the <uid>.ar file (see event.c file)
#endif // OS_BUILD

  UINT16 ControllerRid;                     //!< Revision id of the subsystem memory controller from FIS
  UINT16 MaxAveragePowerBudget;             //!< Maximum average power budget supported by the Module in mW
  BOOLEAN MasterPassphraseEnabled;          //!< If 1, master passphrase is enabled
} DIMM_INFO;

typedef struct _TOPOLOGY_DIMM_INFO {
  UINT16 DimmID;                            //!< SMBIOS Type 17 handle corresponding to this memory device
  UINT16 SocketID;                          //!< Socket ID for the memory device
  UINT8 MemoryType;                         //!< memory type
  UINT64 VolatileCapacity;                  //!< Capacity in bytes mapped as volatile memory
  CHAR16 DeviceLocator[DEVICE_LOCATOR_LEN]; //!< describing the physically-labeled socket or board position
  CHAR16 BankLabel[BANKLABEL_LEN];          //!< identifies the physically labeled bank
} TOPOLOGY_DIMM_INFO;

typedef struct _SOCKET_INFO {
  UINT16 SocketId;                   //!< Zero indexed processor identifer
  UINT64 MappedMemoryLimit;          //!< Maximum amount of physical memory in bytes allowed to be mapped into SPA based on the SKU of the processor
  UINT64 TotalMappedMemory;          //!< Total amount of physical memory in bytes currently mapped into the SPA for the processor
} SOCKET_INFO;

typedef struct _SYSTEM_CAPABILITIES_INFO {
  /**
    Bit0
      If set BIOS supports changing configuration through management software.
      If clear BIOS does not allow configure change through management software
    Bit1
      If set BIOS supports runtime interface to validate management configuration change request.
      Refer to BIOS runtime interface data structure.
      Note: this bit is valid only if Bit0 is set.
  **/
  UINT8 PlatformConfigSupported;
  UINT16 InterleaveAlignmentSize;                   //!< Memory alignment size in bytes required when configuring pools
  UINT8 OperatingModeSupport;                       //!< Memory modes supported by BIOS
  UINT8 CurrentOperatingMode;                       //!< Memory modes (volatile and persistent) currently selected by BIOS
  UINT16 InterleaveFormatsSupportedNum;             //!< Number of elements in list
  HII_POINTER PtrInterleaveFormatsSupported;        //!< List of supported interleave set formats
  UINT64 MinNsSize;                                 //!< Minimum namespace size in bytes
  UINT64 NsBlockSizes[SUPPORTED_BLOCK_SIZES_COUNT]; //!< Supported namespace block sizes in bytes
  UINT8 AppDirectMirrorSupported;
  UINT8 DimmSpareSupported;
  UINT8 AppDirectMigrationSupported;
  UINT8 RenameNsSupported;
  UINT8 GrowPmNsSupported;
  UINT8 ShrinkPmNsSupported;
  UINT8 GrowBlkNsSupported;
  UINT8 ShrinkBlkNsSupported;
  UINT8 InitiateScrubSupported;
  UINT64 PartitioningAlignment;                     //!< The alignment for the create goal partition sizes in bytes
  UINT64 InterleaveSetsAlignment;
  BOOLEAN AdrSupported;
  BOOLEAN EraseDeviceDataSupported;
  BOOLEAN EnableDeviceSecuritySupported;
  BOOLEAN DisableDeviceSecuritySupported;
  BOOLEAN UnlockDeviceSecuritySupported;
  BOOLEAN FreezeDeviceSecuritySupported;
  BOOLEAN ChangeDevicePassphraseSupported;
  BOOLEAN ChangeMasterPassphraseSupported;
  BOOLEAN MasterEraseDeviceDataSupported;
} SYSTEM_CAPABILITIES_INFO;

typedef struct _MEMORY_RESOURCES_INFO {
  UINT64 RawCapacity;               //!< Sum of the raw capacity on all dimms
  UINT64 VolatileCapacity;          //!< Sum of the usable volatile capacity on all dimms
  UINT64 AppDirectCapacity;         //!< Sum of the usable appdirect capacity on all dimms
  UINT64 UnconfiguredCapacity;      //!< Sum of the capacity that is only accessible as Storage capacity
  UINT64 InaccessibleCapacity;      //!< Sum of the capacity that is inaccessible due to a licensing issue
  UINT64 ReservedCapacity;          //!< Sum of the capacity reserved for metadata on all dimms
} MEMORY_RESOURCES_INFO;

typedef struct _DIMM_PERFORMANCE_DATA {
  UINT16  DimmId;             //!< SMBIOS Type 17 handle corresponding to this memory device
  UINT128 MediaReads;         //!< Number of 64 byte reads from media on the DCPMM since last AC cycle
  UINT128 MediaWrites;        //!< Number of 64 byte writes to media on the DCPMM since last AC cycle
  UINT128 ReadRequests;       //!< Number of DDRT read transactions the DCPMM has serviced since last AC cycle
  UINT128 WriteRequests;      //!< Number of DDRT write transactions the DCPMM has serviced since last AC cycle
  UINT128 TotalMediaReads;    //!< Lifetime number of 64 byte reads from media on the DCPMM
  UINT128 TotalMediaWrites;   //!< Lifetime number of 64 byte writes to media on the DCPMM
  UINT128 TotalReadRequests;  //!< Lifetime number of DDRT read transactions the DCPMM has serviced
  UINT128 TotalWriteRequests; //!< Lifetime number of DDRT write transactions the DCPMM has serviced
  // These are deprecated in the FIS, but leaving these in to preserve functionality
  // of manufacturing command (MfgShowPerformanceCommand.c). They are set to 0s
  // in GetDimmsPerformanceData
  UINT128 TotalBlockReadRequests;   //!< Lifetime number of BW read requests the DCPMM has serviced
  UINT128 TotalBlockWriteRequests;  //!< Lifetime number of BW write requests the DCPMM has serviced
} DIMM_PERFORMANCE_DATA;

/** Namespace information */
typedef struct _NAMESPACE_INFO {
  UINT8 NamespaceInfoNode[LIST_ENTRY_SIZE];     //!< Node (instead of LIST_ENTRY because of HII compilation issues)
  UINT64 Signature;                             //!< List signature
  UINT8 NamespaceGuid[NSGUID_LEN];              //!< UUID per RFC 4122
  UINT16 NamespaceId;                           //!< Namespace ID
  UINT8 Name[NLABEL_NAME_LEN_WITH_TERMINATOR];  //!< Optional name 63 characters + (NULL-terminator)
  CHAR16 Capacity[UINT128_DIGITS];              //!< Capacity string
  CHAR16 FreeCapacity[UINT128_DIGITS];          //!< Remaining Capacity String
  UINT16 HealthState;                           //!< Health state. Ok, Warning, Critical, Broken Mirror.
  UINT16 RegionId;                              //!< ID of related region/IS
  UINT64 BlockSize;                             //!< Internal Block Size to calculate Capacity
  UINT64 LogicalBlockSize;                      //!< Logical Block Size from NS Label
  UINT64 BlockCount;                            //!< Block Count
  UINT8 NamespaceMode;                          //!< 0: None, 1: fsdax / pfn, 2: btt/sector
  UINT8 NamespaceType;                          //!< 1: Block, 2: PM
  UINT16 Major;                                 //!< Namespace label major version
  UINT16 Minor;                                 //!< Namespace label minor version
  UINT64 UsableSize;                            //!< Usable size of namespace, minus metadata
} NAMESPACE_INFO;

/** Default AppDirect Namespace block size **/
#define AD_NAMESPACE_BLOCK_SIZE 1

#define AD_NAMESPACE_LABEL_LBA_SIZE_512 512
#define AD_NAMESPACE_LABEL_LBA_SIZE_4K 4096

#define NAMESPACE_PURPOSE_LEN            64
#define NAMESPACE_INFO_SIGNATURE     SIGNATURE_64('N', 'S', 'P', 'A', 'C', 'I', 'N', 'F')
#define NAMESPACE_INFO_FROM_NODE(a)  CR(a, NAMESPACE_INFO, NamespaceInfoNode, NAMESPACE_INFO_SIGNATURE)

#define DIMM_PID_ALL      0xFFFF
#define DIMM_PID_INVALID  0xFFFE
#define DIMM_PID_NOTSET   0xFFFD
#define REGION_ID_NOTSET                    0xFFFD
#define REGION_VOLATILE_SIZE_ALIGNMENT_B    GIB_TO_BYTES(1ULL)
#define REGION_PERSISTENT_SIZE_ALIGNMENT_B  GIB_TO_BYTES(1ULL)

/**
  Minimum size and alignment of AppDirect and Block Namespace is 1GB
**/
#define PM_NAMESPACE_MIN_SIZE           BYTES_IN_GIBIBYTE
#define BLOCK_NAMESPACE_MIN_SIZE        BYTES_IN_GIBIBYTE
#define NAMESPACE_4KB_ALIGNMENT_SIZE    KIB_TO_BYTES(4)

/** Region Health States */
#define RegionHealthStateNormal     1
#define RegionHealthStateError      2
#define RegionHealthStateUnknown    3
#define RegionHealthStatePending    4
#define RegionHealthStateLocked     5

/** Region Interleave Format Types */
#define DEFAULT_INTERLEAVE_SET_TYPE 0
#define INTERLEAVED                 1
#define NON_INTERLEAVED             2
#define MIRRORED                    3

/** Reserve Types */
#define RESERVE_DIMM_NONE                0
#define RESERVE_DIMM_STORAGE             1
#define RESERVE_DIMM_AD_NOT_INTERLEAVED  2

#define DEFAULT_CHANNEL_INTERLEAVE_SIZE 0
#define DEFAULT_IMC_INTERLEAVE_SIZE     0

/* Region Information provides details about a PMEM region (interleave set).*/
typedef struct _REGION_INFO {
  UINT16 RegionId;                  ///< Region identifier
  UINT16 SocketId;                  ///< Socket identifier
  UINT8 RegionType;                 ///< Region type
  UINT64 Capacity;                  ///< Region total raw capacity
  UINT64 FreeCapacity;              ///< Region total free capacity. Raw less capacity used by namespaces
  UINT64 AppDirNamespaceMaxSize;    ///< Maximum size of an AppDirect namespace
  UINT64 AppDirNamespaceMinSize;    ///< Minimum size of an AppDirect namespace
  UINT16 Health;                    ///< Health state of region
  UINT16 DimmId[12];                ///< DIMM IDs associated with this region
  UINT16 DimmIdCount;               ///< Number of DIMMs found in DimmId
  UINT64 CookieId;                  ///< Interleave set ID
  HII_POINTER PtrInterlaveFormats;  ///< Pointer to array of Interleave Formats
  UINT32 InterleaveFormatsNum;      ///< Number of Interleave Formats
} REGION_INFO;

typedef struct _REGION_GOAL_TEMPLATE {
  UINT64 Size;                //!< Size of the region in bytes
  UINT8 InterleaveSetType;    //!< Type of interleave set: non-interleaved, interleaved, mirrored
  BOOLEAN Asymmetrical;       //!< Determine if region goal use asymmetrical config on socket
} REGION_GOAL_TEMPLATE;

/** Structure describes the usage characteristics and regions (interleave sets) of the specified DIMM */
typedef struct _REGION_GOAL_PER_DIMM_INFO {
  UINT32 DimmID;                                    //!< DIMM ID
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];              //!< DIMM UID
  UINT16 SocketId;                                  //!< Socket ID that DIMM is found
  UINT32 PersistentRegions;                         //!< Count of persistent regions
  UINT64 VolatileSize;                              //!< Volatile capacity
  UINT64 StorageCapacity;                           //!< Any capacity not allocated to Volatile or AppDirect regions
  UINT8 NumberOfInterleavedDimms[MAX_IS_PER_DIMM];  //!< Count of DIMMs that are part of related Interleaved AppDirect regions
  UINT64 AppDirectSize[MAX_IS_PER_DIMM];            //!< AppDirect capacity
  UINT8 InterleaveSetType[MAX_IS_PER_DIMM];         //!< Type of interleave set: non-interleaved, interleaved, mirrored
  UINT8 ImcInterleaving[MAX_IS_PER_DIMM];           //!< IMC interleaving as bit field
  UINT8 ChannelInterleaving[MAX_IS_PER_DIMM];       //!< Channel interleaving as bit field
  UINT8 AppDirectIndex[MAX_IS_PER_DIMM];            //!< AppDirect Index
  UINT8 Status;                                     //!< Goal config status. See @ref GOAL_CONFIG_STATUS.
} REGION_GOAL_PER_DIMM_INFO;

#define MAX_ERROR_LOG_STRUCT_SIZE 64

/** Error Log Info */
typedef struct _ERROR_LOG_INFO {
  UINT16 DimmID;                                //!< DIMM ID of associated log info
  UINT64 SystemTimestamp;                       //!< Unix epoch time of log entry
  UINT8 ErrorType;                              //!< Error Log type. See @ref ERROR_LOG_TYPES.
  UINT8 OutputData[MAX_ERROR_LOG_STRUCT_SIZE];  //!< Error log data
} ERROR_LOG_INFO;

#define MAX_PAYLOAD_DEBUG_ENTRY_SIZE 128
typedef struct _DEBUG_LOG_INFO {
  UINT8 DimmID;
  UINT8 Data[MAX_PAYLOAD_DEBUG_ENTRY_SIZE];
  //@Todo Discuss and diagnose how handle Large payload
} DEBUG_LOG_INFO;

/** Namespace related defines**/
#define UNKNOWN_TYPE_NAMESPACE            0
#define STORAGE_NAMESPACE                 1
#define APPDIRECT_NAMESPACE               2

#define NAMESPACE_BLOCK_COUNT_UNDEFINED   0
#define NAMESPACE_PM_NAMESPACE_BLOCK_SIZE 1
#define NAMESPACE_PROPERTY_TRUE           1
#define NAMESPACE_PROPERTY_FALSE          0

#define IGNORE_YES_NO_VALUE_IGNORE        0
#define IGNORE_YES_NO_VALUE_YES           1
#define IGNORE_YES_NO_VALUE_NO            2

/**
  @defgroup HEALTH_STATUS Health Status
  @{
**/
#define HEALTH_UNKNOWN               0    ///< Unknown health status
#define HEALTH_HEALTHY               1    ///< DIMM Healthy
#define HEALTH_NON_CRITICAL_FAILURE  2    ///< Non-Critical (maintenance required)
#define HEALTH_CRITICAL_FAILURE      3    ///< Critical (feature or performance degraded due to failure)
#define HEALTH_FATAL_FAILURE         4    ///< Fatal (data loss has occurred or is imminent)
#define HEALTH_UNMANAGEABLE          5    ///< DIMM is unmanagable
#define HEALTH_NON_FUNCTIONAL        6    ///< DIMM is non-functional

/**
  @}
  @defgroup HEALTH_STATUS_REASONS Health Status Reasons
  @{
**/
#define HEALTH_STATUS_REASON_NONE                          0    ///< None
#define HEALTH_REASON_PERCENTAGE_REMAINING_LOW            BIT0  ///< Percentage remaining less than or equal to 1%
#define HEALTH_REASON_PACKAGE_SPARING_HAS_HAPPENED        BIT1  ///< Package sparing has happened
#define HEALTH_REASON_CAP_SELF_TEST_WARNING               BIT2  ///< CAP Self-Test returned a warning
#define HEALTH_REASON_PERC_REMAINING_EQUALS_ZERO          BIT3  ///< Percentage remaining is 0
#define HEALTH_REASON_DIE_FAILURE                         BIT4  ///< Die Failure (after package sparing if available)
#define HEALTH_REASON_AIT_DRAM_DISABLED                   BIT5  ///< AIT DRAM state is disabled
#define HEALTH_REASON_CAP_SELF_TEST_FAILURE               BIT6  ///< CAP Self-Test failed
#define HEALTH_REASON_CRITICAL_INTERNAL_STATE_FAILURE     BIT7  ///< Critical internal state failure

/**
  @}
  Security operations
**/
#define SECURITY_OPERATION_UNDEFINED                0
#define SECURITY_OPERATION_SET_PASSPHRASE           1
#define SECURITY_OPERATION_CHANGE_PASSPHRASE        2
#define SECURITY_OPERATION_DISABLE_PASSPHRASE       3
#define SECURITY_OPERATION_UNLOCK_DEVICE            4
#define SECURITY_OPERATION_FREEZE_DEVICE            5
#define SECURITY_OPERATION_ERASE_DEVICE             6
#define SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE 7
#define SECURITY_OPERATION_MASTER_ERASE_DEVICE      8

/**
  Security states
**/
#define SECURITY_UNKNOWN          0
#define SECURITY_DISABLED         1
#define SECURITY_LOCKED           2
#define SECURITY_UNLOCKED         3
#define SECURITY_FROZEN           4
#define SECURITY_PW_MAX           5
#define SECURITY_NOT_SUPPORTED    6
#define SECURITY_STATES_COUNT     7
#define SECURITY_MIXED_STATE      8 // Mixed security state in all dims view

/**
  Passphrase Type
**/
#define SECURITY_USER_PASSPHRASE    0x0
#define SECURITY_MASTER_PASSPHRASE  0x1

/**
  Address Range Scrub (ARS) Status
**/
#define ARS_STATUS_UNKNOWN        0
#define ARS_STATUS_NOT_STARTED    1
#define ARS_STATUS_IN_PROGRESS    2
#define ARS_STATUS_COMPLETED      3
#define ARS_STATUS_ABORTED        4

/** Overwrite DIMM operation status **/
#define OVERWRITE_DIMM_STATUS_UNKNOWN      0
#define OVERWRITE_DIMM_STATUS_NOT_STARTED  1
#define OVERWRITE_DIMM_STATUS_IN_PROGRESS  2
#define OVERWRITE_DIMM_STATUS_COMPLETED    3

/** DDRT Training Status **/
#define DDRT_TRAINING_NOT_COMPLETE  0x00
#define DDRT_TRAINING_COMPLETE      0x01
#define DDRT_TRAINING_FAILURE       0x02
#define DDRT_S3_COMPLETE            0x03
#define DDRT_TRAINING_UNKNOWN       0xFF

/** Dimm Boot Status Bitmask **/
#define DIMM_BOOT_STATUS_UNKNOWN              BIT0
#define DIMM_BOOT_STATUS_MEDIA_NOT_READY      BIT1
#define DIMM_BOOT_STATUS_MEDIA_ERROR          BIT2
#define DIMM_BOOT_STATUS_MEDIA_DISABLED       BIT3
#define DIMM_BOOT_STATUS_FW_ASSERT            BIT4
#define DIMM_BOOT_STATUS_DDRT_NOT_READY       BIT5
#define DIMM_BOOT_STATUS_MAILBOX_NOT_READY    BIT6
#define DIMM_BOOT_STATUS_REBOOT_REQUIRED      BIT7

/**
  System-wide ARS Status Bitmask
 **/
#define ARS_STATUS_MASK_UNKNOWN        BIT0
#define ARS_STATUS_MASK_NOT_STARTED    BIT1
#define ARS_STATUS_MASK_IN_PROGRESS    BIT2
#define ARS_STATUS_MASK_COMPLETED      BIT3
#define ARS_STATUS_MASK_ABORTED        BIT4

/**
  Form Factor
**/
#define FORMFACTOR_EMPTY     0
#define FORMFACTOR_UNKNOWN   2
#define FORMFACTOR_NVMDIMM   9
#define FORMFACTOR_SODIMM    13

/**
  Supported memory modes by PCAT table 0
**/
#define SUPPORTED_ONE_LM_BIT          0
#define SUPPORTED_TWO_LM_BIT          1
#define SUPPORTED_PM_DIRECT_MODE_BIT  2
#define SUPPORTED_PM_CACHED_MODE_BIT  3
#define SUPPORTED_BLOCK_MODE_BIT      4
#define SUPPORTED_SUBNUMA_CLUSTER_BIT 5

/**
  Current memory mode by PCAT table 0
**/
#define CURRENT_1LM                 0x1
#define CURRENT_2LM_PM_DIRECT       0x2
#define CURRENT_2LM_PM_CACHED       0x3
#define CURRENT_AUTO                0x4 // 2LM if DDR4 + Intel NVMDIMM with volatile mode present, 1LM otherwise

/**
  Platform config with management software supported by PCAT table 0
**/
#define PLATFROM_CONFIG_SUPPORTED_BIT      0
#define RUNTIME_CHANGE_REQUEST_SUPPORT_BIT 1

/**
  Memory Type
**/
#define MEMORYTYPE_UNKNOWN  0
#define MEMORYTYPE_DDR4     1
#define MEMORYTYPE_DCPM     2

/**
  @todo(after official SmBIOS spec release): update with correct values from SMBIOS spec
  DIMM type
**/
#define FORM_FACTOR_DIMM     0x9
#define FORM_FACTOR_SODIMM   0xD
/**
  @todo(after official SmBIOS spec release): update with correct values from SMBIOS spec
  DIMM type
**/
#define SMBIOS_MEMORY_TYPE_DDR4   0x1A
#define SMBIOS_MEMORY_TYPE_DCPM   0x18

/**
  Package Sparing Capable
**/
#define PACKAGE_SPARING_NOT_CAPABLE       0
#define PACKAGE_SPARING_CAPABLE           1

/**
  Package Sparing Enabled
**/
#define PACKAGE_SPARING_DISABLED          0
#define PACKAGE_SPARING_ENABLED           1

/**
  Package Sparing Supported
**/
#define PACKAGE_SPARING_NOT_SUPPORTED  0
#define PACKAGE_SPARING_SUPPORTED     1

/**
  Package Spares Available
**/
#define PACKAGE_SPARES_NOT_AVAILABLE  0
#define PACKAGE_SPARES_AVAILABLE      1

/**
  Namespace Health Status
**/
#define NAMESPACE_HEALTH_UNKNOWN       0
#define NAMESPACE_HEALTH_OK            1
#define NAMESPACE_HEALTH_WARNING       2
#define NAMESPACE_HEALTH_CRITICAL      3
#define NAMESPACE_HEALTH_UNSUPPORTED   4
#define NAMESPACE_HEALTH_LOCKED        5

/**
  Manageability states
**/
#define MANAGEMENT_INVALID_CONFIG 0
#define MANAGEMENT_VALID_CONFIG   1

/**
  Fw Logging Level
**/
#define FW_LOG_LEVEL_DISABLED 0
#define FW_LOG_LEVEL_ERROR    1
#define FW_LOG_LEVEL_WARNING  2
#define FW_LOG_LEVEL_INFO     3
#define FW_LOG_LEVEL_DEBUG    4

#define OPTIONAL_DATA_UNDEFINED 0xFFUL
/**
  First Fast Refresh states
**/
#define FIRST_FAST_REFRESH_DISABLED  0
#define FIRST_FAST_REFRESH_ENABLED   1
#define FIRST_FAST_REFRESH_MIXED     2

/**
  Namespace security capabilities.
**/
#define SECURITY_CAPABILITIES_ENCRYPTION_SUPPORTED  BIT0
#define SECURITY_CAPABILITIES_ERASE_CAPABLE         BIT1

/* VFR compiler doesn't support enums, that's why we use defines */
/**
 * @defgroup SENSOR_TYPES Sensor Types
 * Sensor IDs for the various sensor types
 * @{
 */
#define SENSOR_TYPE_DIMM_HEALTH                0                ///< DIMM Health Sensor ID
#define SENSOR_TYPE_MEDIA_TEMPERATURE          1                ///< Media Temperature Sensor ID
#define SENSOR_TYPE_CONTROLLER_TEMPERATURE     2                ///< Controller Temperature Sensor ID
#define SENSOR_TYPE_PERCENTAGE_REMAINING       3                ///< Percentage Remaining Sensor ID
#define SENSOR_TYPE_LATCHED_DIRTY_SHUTDOWNS    4                ///< Latched Dirty Shutdowns Count Sensor ID
#define SENSOR_TYPE_POWER_ON_TIME              5                ///< Power On Time Sensor ID
#define SENSOR_TYPE_UP_TIME                    6                ///< Up-Time Sensor ID
#define SENSOR_TYPE_POWER_CYCLES               7                ///< Power Cycles Sensor ID
#define SENSOR_TYPE_FW_ERROR_COUNT             8                ///< Firmware Error Count Sensor ID
#define SENSOR_TYPE_UNLATCHED_DIRTY_SHUTDOWNS  9                ///< Unlatched Dirty Shutdowns Count Sensor ID
#define SENSOR_TYPE_ALL                        10               ///< All Sensor IDs
#define SENSOR_TYPE_COUNT                      SENSOR_TYPE_ALL  ///< Total count of all supported sensor types

/** @} */

/** Sensor states **/
#define SENSOR_STATE_NORMAL        0    ///< Sensor state normal
#define SENSOR_STATE_NON_CRITICAL  1    ///< Sensor state non-critical
#define SENSOR_STATE_CRITICAL      2    ///< Sensor state critical
#define SENSOR_STATE_FATAL         3    ///< Sensor state fatal
#define SENSOR_STATE_UNKNOWN       4    ///< Sensor state unknown

/** Sensor enabled/disabled **/
#define SENSOR_DISABLED     0           ///< Sensor type disabled
#define SENSOR_ENABLED      1           ///< Sensor type enabled
#define SENSOR_NA_ENABLED   2           ///< Sensor type can't be disabled/enabled

/** show error **/
#define ERROR_LOG_DEFAULT_SEQUENCE_NUMBER  0
#define ERROR_LOG_MAX_SEQUENCE_NUMBER      65535
#define ERROR_LOG_MAX_COUNT                255
#define ERROR_LOG_DEFAULT_COUNT            1

/** dump FW debuglog **/
#define MAX_LOG_PAGE_OFFSET       0xFFFFFFFF

/**
* @defgroup AIT_DRAM_STATUS AIT DRAM Status types
* @{
*/
#define AIT_DRAM_DISABLED  0    ///< ATI DRAM Disabled
#define AIT_DRAM_ENABLED   1    ///< ATI DRAM Enabled

/**
* @}
* Error injection Enabled states
**/
#define ERR_INJ_DISABLED  0
#define ERR_INJ_ENABLED   1

/** Media temperature Enabled states **/
#define MEDIA_TEMP_DISABLED  0
#define MEDIA_TEMP_ENABLED   1

/** Software trigger Enabled states **/
#define SW_TRIGGER_DISABLED  0
#define SW_TRIGGER_ENABLED   1

/**
  00 - Undefined
  01 - DIMM is configured successfully
  02 - Reserved
  03 - All the DIMMs in the interleave set not found. Volatile memory is mapped to the SPA if possible
  04 - Matching Interleave set not found. Volatile memory is mapped to the SPA if possible
  05 - DIMM added to the system or moved within the system or DIMM is not yet configured.
  Volatile memory is mapped to the SPA if possible. Current configuration present in the DIMM is not modified.
  06 - New configuration input structures have errors, old configuration used. Refer to the config output structures
  for additional errors.
  07 - New configuration input structures have errors. Volatile memory is mapped to the SPA if possible.
  Refer to the config output structures for addition errors
  08 - Configuration Input Checksum not valid
  09 - Configuration Input data Revision is not supported
  10 - Current Configuration Checksum not valid
**/
#define DIMM_CONFIG_UNDEFINED               0
#define DIMM_CONFIG_SUCCESS                 1
#define DIMM_CONFIG_RESERVED                2
#define DIMM_CONFIG_IS_INCOMPLETE           3
#define DIMM_CONFIG_NO_MATCHING_IS          4
#define DIMM_CONFIG_NEW_DIMM                5
#define DIMM_CONFIG_OLD_CONFIG_USED         6
#define DIMM_CONFIG_BAD_CONFIG              7
#define DIMM_CONFIG_IN_CHECKSUM_NOT_VALID   8
#define DIMM_CONFIG_REVISION_NOT_SUPPORTED  9
#define DIMM_CONFIG_CURR_CHECKSUM_NOT_VALID 10

/**
  00 - Valid
  01 - Not Configured
  02 - Failed - Bad configuration
  03 - Failed - Broken interleave
  04 - Failed - Reverted
  05 - Failed - Unsupported
**/
#define DIMM_INFO_CONFIG_VALID                   0
#define DIMM_INFO_CONFIG_NOT_CONFIG              1
#define DIMM_INFO_CONFIG_BAD_CONFIG              2
#define DIMM_INFO_CONFIG_BROKEN_INTERLEAVE       3
#define DIMM_INFO_CONFIG_REVERTED                4
#define DIMM_INFO_CONFIG_UNSUPPORTED             5

typedef struct {
  UINT8  Type;
  UINT8  State;
  UINT8  Enabled;
  UINT8  SettableThresholds;
  UINT8  SupportedThresholds;
  CHAR16 ValueString[HII_SENSOR_VALUE_STRING_SIZE];
  UINT64 Value;
  UINT64 NonCriticalThreshold;
  UINT64 NewNonCriticalThreshold;
  UINT64 CriticalLowerThreshold;
  UINT64 CriticalUpperThreshold;
  UINT64 FatalThreshold;
} DIMM_SENSOR_HII;

#define DIMM_SENSOR_ALARM_DISABLED 0
#define DIMM_SENSOR_ALARM_ENABLED  1

#define DISPLAY_DIMM_ID_HANDLE    0 //!< Use SMBIOS Type 17 handle
#define DISPLAY_DIMM_ID_UID       1 //!< Use DIMM UID
#define DISPLAY_DIMM_ID_MAX_SIZE  2 //!< Number of possible Dimm Identifiers

#define DISPLAY_DIMM_ID_DEFAULT DISPLAY_DIMM_ID_HANDLE
#define APP_DIRECT_SETTINGS_INDEX_DEFAULT 0

#define BMI_DEFAULT MODE_DISABLED     //!< default setting for boot minimal init

#define DISPLAY_SIZE_UNIT_AUTO     0  //!< Automatically use the best format for each capacity in binary format
#define DISPLAY_SIZE_UNIT_AUTO_10  1  //!< Automatically use the best format for each capacity in decimal multiples of bytes
#define DISPLAY_SIZE_UNIT_B        2  //!< Display all capacities in bytes
#define DISPLAY_SIZE_UNIT_MB       3  //!< Display all capacities in megabytes
#define DISPLAY_SIZE_UNIT_MIB      4  //!< Display all capacities in mebibytes
#define DISPLAY_SIZE_UNIT_GB       5  //!< Display all capacities in gigabytes
#define DISPLAY_SIZE_UNIT_GIB      6  //!< Display all capacities in gibibytes
#define DISPLAY_SIZE_UNIT_TB       7  //!< Display all capacities in terabytes
#define DISPLAY_SIZE_UNIT_TIB      8  //!< Display all capacities in tebibytes
#define DISPLAY_SIZE_MAX_SIZE      9  //!< Number of possible size display options
#define DISPLAY_SIZE_UNIT_UNKNOWN 10  //!< Unknown units

#define DISPLAY_DIMM_ID_VARIABLE_NAME L"CLI_DEFAULT_DIMM_ID"
#define DISPLAY_SIZE_VARIABLE_NAME L"CLI_DEFAULT_SIZE"

/** Intel DIMM Config automatic provisioning **/
#define INTEL_DIMM_CONFIG_VARIABLE_NAME L"IntelDIMMConfig"

#define INTEL_DIMM_CONFIG_REVISION 2

#define PROVISION_CAPACITY_MODE_MANUAL 0
#define PROVISION_CAPACITY_MODE_AUTO   1

#define MEMORY_SIZE_MAX_PERCENT 100

#define PM_TYPE_APPDIRECT                 0
#define PM_TYPE_APPDIRECT_NOT_INTERLEAVED 1

#define PROVISION_NAMESPACE_MODE_MANUAL 0
#define PROVISION_NAMESPACE_MODE_AUTO   1

#define NAMESPACE_FLAG_NONE 0
#define NAMESPACE_FLAG_BTT  1

#define PROVISION_CAPACITY_STATUS_NEW_UNKNOWN                0
#define PROVISION_CAPACITY_STATUS_SUCCESS                    1
#define PROVISION_CAPACITY_STATUS_ERROR                      2
#define PROVISION_CAPACITY_STATUS_PENDING                    3
#define PROVISION_CAPACITY_STATUS_PENDING_SECURITY_DISABLED  4

#define PROVISION_NAMESPACE_STATUS_NEW_UNKNOWN   0
#define PROVISION_NAMESPACE_STATUS_SUCCESS       1
#define PROVISION_NAMESPACE_STATUS_ERROR  2

/**
  GUID for IntelDimmConfig variables for automatic provisioning
 **/
#define INTEL_DIMM_CONFIG_VARIABLE_GUID \
  { 0x76fcbfb6, 0x38fe, 0x41fd, {0x90, 0x1d, 0x16, 0x45, 0x31, 0x22, 0xf0, 0x35}}

typedef struct _INTEL_DIMM_CONFIG {
  UINT8 Revision;
  UINT8 ProvisionCapacityMode;
  UINT8 MemorySize;
  UINT8 PMType;
  UINT8 ProvisionNamespaceMode;
  UINT8 NamespaceFlags;
  UINT8 ProvisionCapacityStatus;
  UINT8 ProvisionNamespaceStatus;
  UINT8 NamespaceLabelVersion;
  UINT8 Reserved[7];
} INTEL_DIMM_CONFIG;

#define OUTPUT_NVM_XML        1 << 0
#define OUTPUT_ESX_XML        1 << 1
#define OUTPUT_ESX_XML_TABLE  1 << 2
#define OUTPUT_TEXT           1 << 3
#define OUTPUT_VERBOSE        1 << 4
#define OUTPUT_ALL            0xFFFFFFFF

typedef struct _DISPLAY_PREFERENCES {
  UINT8 DimmIdentifier; //!< Default display of DIMM identifiers
  UINT8 SizeUnit;       //!< Default display capacity unit
  UINT8 OutputTypeMask;
} DISPLAY_PREFERENCES;

#define OUTPUT_NVM_XML_OPTION_SET(DispPref) ((DispPref.OutputTypeMask & OUTPUT_NVM_XML) != 0)
#define OUTPUT_ESX_XML_OPTION_SET(DispPref) ((DispPref.OutputTypeMask & OUTPUT_ESX_XML) != 0)
#define OUTPUT_ESX_XML_TABLE_OPTION_SET(DispPref) ((DispPref.OutputTypeMask & OUTPUT_ESX_XML_TABLE) != 0)
#define OUTPUT_TEXT_OPTION_SET(DispPref) ((DispPref.OutputTypeMask & OUTPUT_TEXT) != 0)
#define OUTPUT_VERBOSE_OPTION_SET(DispPref) ((DispPref.OutputTypeMask & OUTPUT_VERBOSE) != 0)

typedef struct _DRIVER_PREFERENCES {
  UINT8 ImcInterleaving;      //!< IMC interleaving as bit field
  UINT8 ChannelInterleaving;  //!< Channel interleaving as bit field
  UINT8 AppDirectGranularity; //!< Granularity of AppDirect memory when goals are created
} DRIVER_PREFERENCES;

#define HOST_SERVER_NAME_LEN        100
#define HOST_SERVER_OS_NAME_LEN     100
#define HOST_SERVER_OS_VERSION_LEN  100

typedef struct _HOST_SERVER_INFO
{
   CHAR16 Name[HOST_SERVER_NAME_LEN];
   CHAR16 OsName[HOST_SERVER_OS_NAME_LEN];
   CHAR16 OsVersion[HOST_SERVER_OS_VERSION_LEN];
}HOST_SERVER_INFO;

/** Command Access Policy reflects if a FW Op/Sub Opcode is restricted */
typedef struct _COMMAND_ACCESS_POLICY_ENTRY
{
  UINT8   Opcode;         //!< Opcode of the FW command
  UINT8   SubOpcode;      //!< SubOpcode of the FW command
  BOOLEAN Restricted;     //!< True if the command is restricted.
} COMMAND_ACCESS_POLICY_ENTRY;

/** Total number of diagnostic test types */
#define DIAGNOSTIC_TEST_COUNT 4

/**
Start Diagnostic Command tests codes
**/

#define DIAGNOSTIC_TEST_UNKNOWN     (0)
#define DIAGNOSTIC_TEST_QUICK       (BIT0)
#define DIAGNOSTIC_TEST_CONFIG      (BIT1)
#define DIAGNOSTIC_TEST_SECURITY    (BIT2)
#define DIAGNOSTIC_TEST_FW          (BIT3)
#define DIAGNOSTIC_TEST_ALL         (DIAGNOSTIC_TEST_QUICK | DIAGNOSTIC_TEST_CONFIG | DIAGNOSTIC_TEST_SECURITY | DIAGNOSTIC_TEST_FW)

/** Get FW Debug Log Source Definitions */
#define FW_DEBUG_LOG_SOURCE_MEDIA   0
#define FW_DEBUG_LOG_SOURCE_SRAM    1
#define FW_DEBUG_LOG_SOURCE_SPI     2
#define FW_DEBUG_LOG_SOURCE_MAX     2
#define NUM_FW_DEBUG_LOG_SOURCES    3

#endif /** _NVM_TYPES_H_ **/
