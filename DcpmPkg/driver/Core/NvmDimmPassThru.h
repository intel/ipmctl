/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVMDIMMPASSTHRU_H_
#define _NVMDIMMPASSTHRU_H_

#include <Types.h>
#include <NvmDimmDriverData.h>

struct _DIMM;

/**
  Version number is BCD with format:
  MMmmRr
  M = Major version number
  m = minor version number
  R - Release type (Alpha, Beta, Test, etc)
  r = release version
**/
#define NVMDIMM_PASS_THRU_GENERAL_RELEASE 0
#define NVMDIMM_PASS_THRU_ALPHA_RELEASE   1
#define NVMDIMM_PASS_THRU_BETA_RELEASE    2
#define NVMDIMM_PASS_THRU_TEST_RELEASE    3

#define NVMDIMM_PASS_THRU_MAJOR_VERSION   1
#define NVMDIMM_PASS_THRU_MINOR_VERSION   0
#define NVMDIMM_PASS_THRU_RELEASE_TYPE    NVMDIMM_PASS_THRU_TEST_RELEASE
#define NVMDIMM_PASS_THRU_RELEASE_NUMBER  0

#define NVMD_PASS_THRU_PROTOCOL_VERSION  NVMDIMM_PASS_THRU_MAJOR_VERSION<<16 | \
  NVMDIMM_PASS_THRU_MINOR_VERSION<<8 | NVMDIMM_PASS_THRU_RELEASE_TYPE<<4 | \
  NVMDIMM_PASS_THRU_RELEASE_NUMBER

#define OS_MB_OFFSET        0x100000    //!< Offset from the start of the CTRL region to the start of the OS mailbox
#define OS_MB_IN_OFFSET     (2 << 20)   //!< Offset from the start of the CTRL region to the start of the OS mailbox large input payload
#define OS_MB_OUT_OFFSET (3 << 20)      //!< Offset from the start of the CTRL region to the start of the OS mailbox large output payload

#define REG_SIZE (8)                    //!< Size of a Intel NVM Dimm Mailbox Register Bytes
#define NONCE_SIZE (4)                  //!< Size of a Intel NVM Dimm Nonce Register Bytes
#define PAYLOAD_BETWEEN_SIZE (64)       //!< Size between payload[n] and payload[n+1]
#define IN_SINGLE_PAYLOAD_SIZE (8)      //!< Size of the input payload register
#define IN_PAYLOAD_NUM (16)             //!< Total number of the input payload registers
#define OUT_SINGLE_PAYLOAD_SIZE (8)     //!< Size of the output payload register
#define OUT_PAYLOAD_NUM (16)            //!< Total number of the output payload registers

/**
  Control region offsets
**/
#define MB_COMMAND_OFFSET       0
#define MB_NONCE0_OFFSET        0x40
#define MB_NONCE1_OFFSET        0x80
#define MB_IN_PAYLOAD0_OFFSET   0xC0
#define MB_STATUS_OFFSET        0x4C0
#define MB_OUT_PAYLOAD0_OFFSET  0x500

#define DIMM_SN_LEN     20         //!< DIMM Serial Number buffer length
#define DIMM_MFR_LEN    20         //!< Manufacturer name buffer length
#define DIMM_PN_LEN     20         //!< DIMM Part Number buffer length
#define DIMM_FW_REV_LEN 14         //!< DIMM FW Revision buffer length

#define SECURITY_NONCE_LEN      8       //!< Length of a security nonce
#define BCD_DATE_LEN            8       //!< Length of a BDC Formatted Date
#define BCD_TIME_LEN            9       //!< Length of a BDC Formatted Time
#define FW_DEBUG_LOG_LEN        128     //!< Length of a firmware debug log chunk via SMBus
#define FW_IMG_MAX_CHUNK_SIZE   126     //!< The maximum size of a firmware image chunk

#define SECONDS_TO_MICROSECONDS(Seconds) MultU64x32((UINT64)(Seconds), 1000000)
#define SECONDS_TO_MICROSECONDS_32(Seconds) Seconds*1000000
#define SECONDS_TO_MILLISECONDS(Seconds) (Seconds * 1000)

// PassThru timeout in microseconds
#define PT_TIMEOUT_INTERVAL SECONDS_TO_MICROSECONDS(1)

// PassThru timeout in milliseconds
#define PT_TIMEOUT_INTERVAL_EXT SECONDS_TO_MILLISECONDS(1)

// Dcpmm timeout in microseconds
#define DCPMM_TIMEOUT_INTERVAL SECONDS_TO_MICROSECONDS_32(1)

// Smbus delay period in microseconds
#define SMBUS_PT_DELAY_PERIOD_IN_US  100

// Format timeout in microseconds
// 120 minutes for 512 GiB dimm
#define PT_FORMAT_DIMM_MAX_TIMEOUT SECONDS_TO_MICROSECONDS(7200)

#define FW_ABORTED_RETRIES_COUNT_MAX 5

/*
* Triggers to modify left shift value - error injection
*/
#define PACKAGE_SPARING_TRIGGER (1 << 0)
#define FATAL_ERROR_TRIGGER  (1 << 2)
#define SPARE_BLOCK_PERCENTAGE_TRIGGER  (1 << 3)
#define DIRTY_SHUTDOWN_TRIGGER  (1 << 4)

// Opt-In Codes - value 0x00 is invalid
enum OPT_IN_CODE {
  NVM_SVN_DOWNGRADE = 0x01,
  NVM_SECURE_ERASE_POLICY = 0x02,
  NVM_S3_RESUME = 0x03,
  NVM_FW_ACTIVATE = 0x04
};


#pragma pack(push)
#pragma pack(1)

/**
  Makes a CacheLine durable through the iMC for an Uncacheable region of memory
**/

VOID
DurableCacheLineWrite(
  IN      VOID *pCacheLineAddress,
  IN      VOID *pRegularBuffer,
  IN      UINT32 NumOfBytes
  );

/**
  Copy data from a regular buffer to an interleaved buffer.
  Using DurableCacheLineWrites
  Both buffers have to be equal or greater than NumOfBytes.

  It is expected that the interleaved buffer is at least one cache
  line between interleaved addresses

  @param[in]  pRegularBuffer       input regular buffer
  @param[out] ppInterleavedBuffer  output interleaved buffer
  @param[in]  LineSize line size of interleaved buffer
  @param[in]  NumOfBytes           number of bytes to copy
**/
VOID
DurableCacheLineWriteToInterleavedBuffer(
  IN     VOID *pRegularBuffer,
     OUT VOID **ppInterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 NumOfBytes
  );

/**
  Clear a part or whole of interleaved buffer.
  Using DurableCacheLineWrites

  It is expected that the interleaved buffer is at least one cache
  line between interleaved addresses

  @param[out] ppInterleavedBuffer  interleaved buffer to clear
  @param[in] LineSize line size of interleaved buffer
  @param[in]  NumOfBytes           number of bytes to clear
**/
VOID
DurableCacheLineClearInterleavedBuffer(
     OUT VOID **ppInterleavedBuffer,
  IN     UINT32 LineSize,
  IN     UINT32 NumOfBytes
  );

/**
  Pass through command to FW
  Sends a command to FW and waits for response from firmware

  @param[in,out] pCmd A firmware command structure
  @param[in] pMb OPTIONAL A mailbox to call pass through command, if NULL passed then mailbox is taken from inventory base
             Inventory base may be not initialized on early driver stage.
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the protocol command.
             A Timeout value of 0 means that this function will wait indefinitely for the protocol command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter
  @retval EFI_DEVICE_ERROR FW error received
  @retval EFI_TIMEOUT A timeout occurred while waiting for the protocol command to execute.
**/
EFI_STATUS
EFIAPI
DefaultPassThru (
  IN     struct _DIMM *pDimm,
  IN OUT NVM_FW_CMD *pCmd,
  IN     UINT64 Timeout
  );

/**
  Pass through command to FW, but retry FW_ABORTED_RETRIES_COUNT_MAX times if we receive a FW_ABORTED
  response code back.

  @param[in,out] pCmd A firmware command structure
  @param[in] pMb OPTIONAL A mailbox to call pass through command, if NULL passed then mailbox is taken from inventory base
             Inventory base may be not initialized on early driver stage.
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the protocol command.
             A Timeout value of 0 means that this function will wait indefinitely for the protocol command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter
  @retval EFI_DEVICE_ERROR FW error received
  @retval EFI_TIMEOUT A timeout occurred while waiting for the protocol command to execute.
**/
EFI_STATUS
EFIAPI
PassThruWithRetryOnFwAborted(
  IN     struct _DIMM *pDimm,
  IN OUT NVM_FW_CMD *pCmd,
  IN     UINT64 Timeout
  );


#ifdef __cplusplus
extern "C"
{
#endif

/**
  Defines the Firmware Command Table opcodes accessed via the EFI_DCPMM_PASS_THRU_PROTOCOL
**/
enum PassthroughOpcode {
  PtIdentifyDimm = 0x01,       //!< Retrieve physical inventory data for DIMM
  PtGetSecInfo = 0x02,         //!< Retrieve security information from DIMM
  PtSetSecInfo = 0x03,         //!< Send a security related command to DIMM
  PtGetFeatures = 0x04,        //!< Retrieve modifiable settings for DIMM
  PtSetFeatures = 0x05,        //!< Modify settings for DIMM
  PtGetAdminFeatures = 0x06,   //!< Gets the advanced DIMM settings
  PtSetAdminFeatures = 0x07,   //!< Sets the advanced DIMM settings
  PtGetLog = 0x08,             //!< Retrieve administrative data, error info, other FW data
  PtUpdateFw = 0x09,           //!< Move an image to the DIMM
  PtInjectError = 0x0A,        //!< Validation only CMD to trigger error conditions
  PtCustomerFormat = 0xF1,     //!< DFX command for factory reset
  PtEmulatedBiosCommands = 0xFD, //!< Perform BIOS emulated command
  PtMax = 0xFE
};

/**
  Defines the Sub-Opcodes for PtIdentifyDimm
**/
enum IdentifyDimmSubop {
  SubopIdentify = 0x0,
  SubopDeviceCharacteristics = 0x1
};

/**
  Defines the Sub-Opcodes for PtGetSecInfo
**/
enum GetSecInfoSubop {
  SubopGetSecState = 0x00,          //!< Returns the DIMM security state
  SubOpGetSecOptIn = 0x02           //!< Returns the DIMM security Opt-In
};

/**
  Defines the Sub-Opcodes for PtSetSecInfo
**/
enum SetSecInfoSubop {
  SubopOverwriteDimm = 0x01,
  SubopSetMasterPass  = 0xF0,           //!< Changes the security master passphrase
  SubopSetPass  = 0xF1,                 //!< Changes the security administrator passphrase
  SubopDisablePass = 0xF2,              //!< Disables the current password on a drive
  SubopUnlockUnit = 0xF3,               //!< Unlocks the persistent region
  SubopReserved = 0xF4,                 //!< First cmd in erase sequence
  SubopSecEraseUnit = 0xF5,             //!< Second cmd in erase sequence
  SubopSecFreezeLock = 0xF6             //!< Prevents changes to all security states until reset
};

/**
  Defines the Sub-Opcodes for PtGetFeatures & PtSetFeatures
**/
enum GetSetFeatSubop {
  SubopAlarmThresholds = 0x01,      //!< Get/Set alarm threshold data (temperature, spare)
  SubopPolicyPowMgmt = 0x02,        //!< Get various power settings
  SubopPolicyPackageSparing = 0x03, //!< Get/Set the DIMM Package sparing policy parameters
  SubopAddressRangeScrub= 0x04,     //!< Get/Set Address Range Scrub information and state
  SubopDDRTAlerts = 0x05,           //!< Get what alerts are set to notify the user
  SubopConfigDataPolicy = 0x06,     //!< Get/Set Optional Configuration Data Policy
  SubopPMONRegisters = 0x07         //!< Get/Set PMON Registers
};

/**
  Defines the Sub-Opcodes for PtGetAdminFeatures & PtSetAdminFeatures
**/
enum GetSetAdminFeatSubop {
  SubopSystemTime = 0x00,               //!< Get/Set the internal System Time
  SubopPlatformDataInfo = 0x01,         //!< Get/Set the PCD data
  SubopDimmPartitionInfo = 0x02,        //!< Get the current DIMM partitions configuration
  SubopFwDbgLogLevel = 0x03,            //!< Get/Set the logging level of the internal logger
  SubopConfigLockdown = 0x05,           //!< Get whether or not the lock down has occurred also can disable it
  SubopDdrtIoInitInfo = 0x06,           //!< Get the DDRT initialization info
  SubopGetSupportedSkuFeatures = 0x07,  //!< Get/Set the data regarding supported DIMM SKU capabilities and features
  SubopLatchSystemShutdownState = 0x09, //!< Get/Set the last system shutdown state data
  SubopViralPolicy = 0x0A,              //!< Get/Set the Viral Policy.
  SubopCommandAccessPolicy = 0xCA,      //!< Get/Set the command access policy
  SubopExtendedAdr = 0xEA,              //!< Get the current extended ADR status of the FW
};

/**
  Defines the Sub-Opcodes for PtGetLog
**/
enum GetLogSubop
{
  SubopSmartHealth = 0x00,        //!< Retrieves various SMART and Health information
  SubopFwImageInfo = 0x01,        //!< Retrieves information about the FW image
  SubopFwDbg = 0x02,              //!< Retrieves a binary log
  SubopMemInfo = 0x03,            //!< Retrieves memory information regarding the state of the DIMM
  SubopLongOperationStat = 0x04,  //!< Retrieves current status of any long operation in effect
  SubopErrorLog = 0x05,           //!< Retrieves an error log
  SubopFailureAnalysis = 0xFA,    //!< Retrieves the data that can be used for failure analysis.
  SubopCommandEffectLog = 0xFF    //!< Retrieves the Command Effect Log
};

/**
  Defines the Sub-Opcodes for PtUpdateFw
**/
enum UpdateFwSubop
{
  SubopUpdateFw = 0x00,       //!< Updates the FW Images
  SubopExecuteFw = 0x01,      //!< Executes a new updated FW Image. (without a restart)
  SubopFwActivate = 0x03      //!< Used to update a successfully downloaded and staged FW without reboot.
};

/**
  Defines the Sub-Opcodes for PtInjectError
**/
enum InjectErrorSubop
{
  SubopEnableInjection = 0x00,        //!< Allows for errors to be injected
  SubopErrorPoison = 0x01,            //!< Sets poison bit on a DPA
  SubopMediaErrorTemperature = 0x02,  //!< Injects a particular temperature to cause a temperature error
  SubopSoftwareErrorTriggers = 0x03   //!< SW override triggers to trip various SW alarms
};

/**
  Defines the Sub-Opcodes for PtEmulatedBiosCommands
**/
enum PtEmulatedBiosCommandsSubop {
  SubopGetLPInfo = 0x00,
  SubopWriteLPInput = 0x01,           //!< Returns large payload mailbox information
  SubopReadLPOutput = 0x02,           //!< Copies a buffer to the large payload input mailbox
  SubopGetBSR = 0x03,                 //!< Copies the large payload output mailbox to a buffer
  SubopReserved2 = 0x04,              //!< Reserved
  SubopExtVendorSpecific = 0x05,      //!< Performs specified command with user-defined timeout and transport interface
};

/**
  Defines the Transport Interface type for PtExtVendorSpecific
**/
enum GetTransportInterface {
  DdrtTransportInterface = 0x00,
  SmbusTransportInterface = 0x01,
  Reserved1 = 0x02,
  Reserved2 = 0x03
};


/**
  Payload -> command options -> payload type.
**/
#define PCD_CMD_OPT_LARGE_PAYLOAD 0x0         //!< Default
#define PCD_CMD_OPT_SMALL_PAYLOAD 0x1


/**
  Input Payload -> command options -> retrieve type.
**/
#define PCD_CMD_OPT_PARTITION_DATA  0x0         //!< Default
#define PCD_CMD_OPT_PARTITION_SIZE  0x1

/**
  Payloads for passthrough fw commands
**/

#define FWR_PRODUCT_VERSION_OFFSET    4
#define FWR_REVISION_VERSION_OFFSET   3
#define FWR_SECURITY_VERSION_OFFSET   2
#define FWR_BUILD_VERSION_HI_OFFSET   1
#define FWR_BUILD_VERSION_LOW_OFFSET  0

#define PCD_SET_SMALL_PAYLOAD_DATA_SIZE   64
#define PCD_GET_SMALL_PAYLOAD_DATA_SIZE   128
#define PCD_LARGE_PAYLOAD_DATA_SIZE 0x1000

#define SMALL_PAYLOAD_SIZE 128

/**
 Memory Page options for Memory Info Input Payload
**/
#define MEMORY_INFO_PAGE_0 0X0
#define MEMORY_INFO_PAGE_1 0X1
#define MEMORY_INFO_PAGE_3 0X3
#define MEMORY_INFO_PAGE_4 0X4


/**
  MemInfo page 3 Errror Inject status bits
 **/
#define ERR_INJECTION_ENABLED_BIT 0x0
#define ERR_INJECTION_ENABLED_BIT_MASK 0x1
#define ERR_INJECTION_MEDIA_TEMP_ENABLED_BIT 0x1
#define ERR_INJECTION_MEDIA_TEMP_ENABLED_BIT_MASK 0x1
#define ERR_INJECTION_SW_TRIGGER_ENABLED_BIT 0x2
#define ERR_INJECTION_SW_TRIGGER_ENABLED_BIT_MASK 0x1
/**
  Passthrough Payload:
    Opcode: 0x01h (Identify DIMM)
**/
typedef struct {
  UINT16 Vid;                     //!< 1-0   : DIMM vendor id
  UINT16 Did;                     //!< 3-2   : Device ID
  UINT16 Rid;                     //!< 5-4   : Revision ID
  UINT16 Ifc;                     //!< 7-6   : Interface format code (0x301)
  UINT8 Fwr[FW_BCD_VERSION_LEN];  //!< 12-8  : BCD formatted firmware revision
  UINT8 Reserved0;                //!< 13    : Reserved
  UINT8 Fswr;                     //!< 14    : Feature SW Required Mask
  UINT8 Reserved1;                //!< 15    : Reserved
  UINT8 Reserved2[16];            //!< 31-16 : Reserved
  UINT32 Rc;                      //!< 35-32 : Raw capacity
  UINT16 Mf;                      //!< 37-36 : Manufacturer ID (Deprecated)
  UINT32 Sn;                      //!< 41-38 : Serial Number ID (Deprecated)
  CHAR8 Pn[DIMM_PN_LEN];          //!< 61-42 : ASCII Part Number
  UINT32 DimmSku;                 //!< 65-62 : DIMM SKU
  UINT8 Reserved3[2];             //!< 66-67 : Reserved
  UINT16 ApiVer;                  //!< 69-68 : API Version
  UINT8 DimmUid[9];               //!< 78-70 : DIMM Unique ID (UID)
  UINT16 ActiveApiVer;            //!< 80-79 : Active API
  UINT8 Reserved4[47];            //!< 127-81: Reserved
} PT_ID_DIMM_PAYLOAD;

typedef struct {
    TEMPERATURE ControllerShutdownThreshold;
    TEMPERATURE MediaShutdownThreshold;
    TEMPERATURE MediaThrottlingStartThreshold;
    TEMPERATURE MediaThrottlingStopThreshold;
    TEMPERATURE ControllerThrottlingStartThreshold;
    TEMPERATURE ControllerThrottlingStopThreshold;
    UINT16 MaxAveragePowerLimit;
    UINT8 Reserved[114];
} PT_DEVICE_CHARACTERISTICS_PAYLOAD;

typedef struct {
  TEMPERATURE ControllerShutdownThreshold;
  TEMPERATURE MediaShutdownThreshold;
  TEMPERATURE MediaThrottlingStartThreshold;
  TEMPERATURE MediaThrottlingStopThreshold;
  TEMPERATURE ControllerThrottlingStartThreshold;
  TEMPERATURE ControllerThrottlingStopThreshold;
  UINT16 MaxAveragePowerLimit;
  UINT16 MaxMemoryBandwidthBoostMaxPowerLimit;
  UINT32 MaxMemoryBandwidthBoostAveragePowerTimeConstant;
  UINT32 MemoryBandwidthBoostAveragePowerTimeConstantStep;
  UINT32 MaxAveragePowerReportingTimeConstant;
  UINT32 AveragePowerReportingTimeConstantStep;
  UINT8 Reserved[96];
} PT_DEVICE_CHARACTERISTICS_PAYLOAD_2_1;

typedef struct {
  UINT8 FisMajor;
  UINT8 FisMinor;
  union {
    PT_DEVICE_CHARACTERISTICS_PAYLOAD       Fis_1_15;
    PT_DEVICE_CHARACTERISTICS_PAYLOAD_2_1   Fis_2_01;
    UINT8 Data[0];
  }Payload;
}PT_DEVICE_CHARACTERISTICS_OUT;

/**
  Passthrough Payload:
    Opcode:     0x06h (Get Admin Features)
    Sub-Opcode: 0x02h (DIMM Partition Info)
**/
typedef struct {
  UINT32 VolatileCapacity;
  UINT32 Resrvd;
  UINT64 VolatileStart;
  UINT32 PersistentCapacity;
  UINT32 Resrvd2;
  UINT64 PersistentStart;
  UINT32 RawCapacity;
  UINT8 Resrvd3[92];
} PT_DIMM_PARTITION_INFO_PAYLOAD;

/**
  Passthrough Payload:
    Opcode:     0x06h (Get Admin Features)
    Sub-Opcode: 0x04h (Persistent Partition)
**/
typedef struct {
  // Bit 0: Partition Enabled, Bit 1: Viral Policy Enabled
  UINT8 State;
  UINT8 Resrvd[127];
} PT_DIMM_PARTITION_STATE_PAYLOAD;

/**
  Passthrough Payload:
    Opcode:     0x02h (Get Security Info)
    Sub-Opcode: 0x00h (Get Security State)
**/
typedef struct {
  union {
    struct {
      UINT32 Reserved1                  : 1;
      UINT32 SecurityEnabled            : 1;
      UINT32 SecurityLocked             : 1;
      UINT32 SecurityFrozen             : 1;
      UINT32 UserSecurityCountExpired   : 1;
      UINT32 SecurityNotSupported       : 1; //!< This SKU does not support Security Feature Set
      UINT32 BIOSSecurityNonceSet       : 1;
      UINT32 Reserved2                  : 1;
      UINT32 MasterPassphraseEnabled    : 1;
      UINT32 MasterSecurityCountExpired : 1;
      UINT32 Reserved3                  : 22;
    } Separated;
    UINT32 AsUint32;
  } SecurityStatus;

  union {
    struct {
      UINT32 SecurityErasePolicy  : 1; //!< 0 - Never been set, 1 - Secure Erase Policy opted in
      UINT32 Reserved             :31;
    } Separated;
    UINT32 AsUint32;
  } OptInStatus;

  UINT8 Reserved[120];
} PT_GET_SECURITY_PAYLOAD;

/**
  Passthrough Input Payload:
    Opcode:     0x02h (Get Security Info)
    Sub-Opcode: 0x02h (Get Security Opt-In)
**/
typedef struct {
  UINT16 OptInCode;
  UINT8 Reserved[126];
} PT_INPUT_PAYLOAD_GET_SECURITY_OPT_IN;

/**
  Passthrough Output Payload:
    Opcode:     0x02h (Get Security Info)
    Sub-Opcode: 0x02h (Get Security Opt-In)
**/
typedef struct {
  UINT16 OptInCode;
  UINT8 Reserved[2];
  UINT32 OptInValue;
  UINT8 OptInModify;
  UINT8 Reserved2[3];
  UINT8 OptInWindow;
  UINT8 Reserved3[51];
  UINT8 OptInSpecificData[64];
} PT_OUTPUT_PAYLOAD_GET_SECURITY_OPT_IN;

/**
  Passthrough Payload:
    Opcode:     0x02h (Set Security Info)
    Sub-Opcode: 0xF1h (Set Passphrase)
**/
typedef struct {
  UINT8 PassphraseCurrent[PASSPHRASE_BUFFER_SIZE]; //!< 31:0 The current security passphrase
  UINT8 PassphraseType;                            //!< 32 Passphrase Type for secure erase
  UINT8 Reserved1[31];                             //!< 63:33 Reserved
  UINT8 PassphraseNew[PASSPHRASE_BUFFER_SIZE];     //!< 64:95 The new passphrase to be set/changed to
  UINT8 Reserved2[32];                             //!< 127:96 Reserved
} PT_SET_SECURITY_PAYLOAD;

typedef struct {
  UINT8 Reserved1[4];                                  //!< 3:0 Reserved
  UINT32 AveragePowerReportingTimeConstant;            //!< 7:4 Average Power Reporting Time Constant
  UINT8 Reserved2[120];                                //!< 127:4 Reserved
} PT_OPTIONAL_DATA_POLICY_PAYLOAD_2_1;

typedef struct {
  UINT8 FisMajor;
  UINT8 FisMinor;
  union {
    PT_OPTIONAL_DATA_POLICY_PAYLOAD_2_1   Fis_2_01;
    UINT8 Data[0];
  }Payload;
} PT_OPTIONAL_DATA_POLICY_PAYLOAD;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x01h (Alarm Thresholds)
**/
typedef struct
{
  /**
    Enable/Disable alarms.
  **/
  union {
    UINT16 AllBits;
    struct {
      UINT16 PercentageRemaining     : 1;
      UINT16 MediaTemperature        : 1;
      UINT16 ControllerTemperature   : 1;
      UINT16                         : 13; //!< Reserved
    } Separated;
  } Enable;

  /**
    When spare levels fall below this percentage based value, asynchronous
    events may be triggered and may cause a transition in the overall health
    state
  **/
  UINT8 PercentageRemainingThreshold;
  /**
    Media temperature threshold (in Celsius) above this threshold trigger asynchronous
    events and may cause a transition in the overall health state
  **/
  TEMPERATURE MediaTemperatureThreshold;

  /**
    Controller temperature threshold (in Celsius) above this threshold trigger asynchronous
    events and may cause a transition in the overall health state
  **/
  TEMPERATURE ControllerTemperatureThreshold;

  UINT8 Reserved[121];
} PT_PAYLOAD_ALARM_THRESHOLDS;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x02h (Power Management Policy)
**/
typedef struct {
  UINT8 Reserved1;
  /**
    Power budget in mW used for instantaneous power.
    Valid range for power budget 10000 - 20000 mW.
  **/
  UINT16 PeakPowerBudget;
  /**
    Power budget in mW used for averaged power.
    Valid range for power budget 10000 - 18000 mW.
  **/
  UINT16 AveragePowerLimit;

  UINT8 Reserved2[123];
} PT_PAYLOAD_POWER_MANAGEMENT_POLICY;

typedef struct {
  UINT8 Reserved1[3];
  /**
    Power limit in mW used for averaged power.
    Valid range for power limit 10000 - 18000 mW.
  **/
  UINT16 AveragePowerLimit;

  UINT8 Reserved2;
  /**
    Returns if the Memory Bandwidth Boost Mode is currently enabled or not.
  **/
  UINT8 MemoryBandwidthBoostFeature;
  /**
    Power limit [mW] used for limiting the Memory Bandwidth Boost Mode power consumption.
    Valid range for Memory Bandwidth Boost Power Limit starts from 15000 - X mW, where X represents
    the value returned from Get Device Characteristics command's Max Memory Bandwidth Boost Max Power Limit field.
  **/
  UINT16 MemoryBandwidthBoostMaxPowerLimit;
  /**
    The value used as a base time window for power usage measurements [ms].
  **/
  UINT32 MemoryBandwidthBoostAveragePowerTimeConstant;

  UINT8 Reserved3[115];
} PT_PAYLOAD_POWER_MANAGEMENT_POLICY_2_1;

typedef struct {
  UINT8 FisMajor;
  UINT8 FisMinor;
  union {
    PT_PAYLOAD_POWER_MANAGEMENT_POLICY       Fis_1_15;
    PT_PAYLOAD_POWER_MANAGEMENT_POLICY_2_1   Fis_2_01;
    UINT8 Data[0];
  }Payload;
} PT_POWER_MANAGEMENT_POLICY_OUT;

typedef struct {
  UINT8 PayloadType : 1;
  UINT8 RetrieveOption : 1;
  UINT8 Reserved : 6;
} PT_INPUT_PAYLOAD_COMMAND_OPTIONS;

/**
  Passthrough Payload:
    Opcode:      0x06h (Get Admin Features)
    Sub-Opcode:  0x01h (Platform Config Data)
**/
typedef struct {
  /**
    PartitionId possible values:
    0x00 - 1st Partition - Interleave configurations - for BIOS usage only
    0x01 - 2nd Partition - Interleave configurations - for OEM and OS usage
    0x02 - 3rd Partition - Namespace Label Storage Area
  **/
  UINT8 PartitionId;                            //!< 0     :
  PT_INPUT_PAYLOAD_COMMAND_OPTIONS CmdOptions;  //!< 1     : Additional options passed
  UINT32 Offset;                                //!< 5-2   : (SmallPayload only) Offset in bytes of partition to start reading from
  UINT8 Reserved[122];                          //!< 127-6 :
} PT_INPUT_PAYLOAD_GET_PLATFORM_CONFIG_DATA;

/**
  Passthrough Payload:
    Opcode:      0x06h (Get Admin Features)
    Sub-Opcode:  0x01h (Platform Config Data)
**/
typedef struct {
  UINT32 Size;
  UINT8 Reserved[124];
} PT_OUTPUT_PAYLOAD_GET_PLATFORM_CONFIG_DATA_SIZE;

/**
  Passthrough Payload:
    Opcode:    0x06h (Get Admin Features)
    Sub-Opcode:  0x05h (FW Debug Log Level)
**/
typedef struct  {
  /**
    The current logging level of the FW (0-255).

    0 = Disabled
    1 = Error
    2 = Warning
    3 = Info
    4 = Debug
  **/
  UINT8 LogLevel;                                  //!< 0 Log Level
  UINT8 LogsCount;                                 //!< 1 Number of logs to retrieve
} PT_OUTPUT_PAYLOAD_FW_DEBUG_LOG_LEVEL;

/**
  This struct holds information about DIMM capabilities and features
**/
typedef struct _SKU_INFORMATION {
  UINT32 MemoryModeEnabled              : 1;
  UINT32                                : 1;   //!< Reserved
  UINT32 AppDirectModeEnabled           : 1;
  UINT32 PackageSparingCapable          : 1;
  UINT32                                : 12;  //!< Reserved
  UINT32 SoftProgramableSku             : 1;
  UINT32 EncryptionEnabled              : 1;
  UINT32                                : 14;  //!< Reserved
} SKU_INFORMATION;

/**
  Passthrough Payload:
    Opcode:    0x06h (Get Admin Features)
    Sub-Opcode:  0x0Ah (Viral Policy)
**/
typedef struct {
  UINT8 ViralPolicyEnable;     //!< Viral Policy Enable: 0 - Disabled, 1 - Enabled
  UINT8 ViralStatus;           //!< Viral Status: 0 - Not Viral, 1 - Viral
  UINT8 Reserved[126];         //!< Reserved
} PT_VIRAL_POLICY_PAYLOAD;

/**
  Passthrough Payload:
    Opcode:      0x07h (Set Admin Features)
    Sub-Opcode:  0x01h (Platform Config Data)
**/
typedef struct {
  /**
    0x00 - 1st Partition - Interleave configurations - for BIOS usage only
    0x01 - 2nd Partition - Interleave configurations - for OEM and OS usage
    0x02 - 3rd Partition - Namespace Label Storage Area
  **/
  UINT8 PartitionId;                               //!< 0      : Which partition to access
  UINT8 PayloadType;                               //!< 1      : Large or small payload
  UINT32 Offset;                                   //!< 5-2    : Offset in bytes of partition to start reading from
  UINT8 Reserved[58];                              //!< 63-6   : Reserved
  UINT8 Data[64];                                  //!< 127-64 :
} PT_INPUT_PAYLOAD_SET_DATA_PLATFORM_CONFIG_DATA;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x03h (Package Sparing Policy)
**/
typedef struct
{
  UINT8 Enable;           //!< Reflects whether the package sparing policy is enabled or disabled (0x00 = Disabled).
  UINT8 Reserved1;        //!< Reserved
  UINT8 Supported;        //!< Designates whether or not the DIMM still supports package sparing.
  UINT8 Reserved[125];    //!< 127-3 : Reserved
} PT_PAYLOAD_GET_PACKAGE_SPARING_POLICY;

/**
  Passthrough Payload:
    Opcode:    0x05h (Set Features)
    Sub-Opcode:  0x03h (Package Sparing Policy)
**/
typedef struct {
  UINT8 Enable;           //!< Reflects whether the package sparing policy is enabled or disabled (0x00 = Disabled).
  UINT8 Reserved[127];    //!< 127-1 : Reserved
} PT_PAYLOAD_SET_PACKAGE_SPARING_POLICY;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x04h (Address Range Scrub)
**/
typedef struct {
  UINT8 Enable;              //!< Indicates whether an Address Range Scrub is in progress.
  UINT8 Reserved1[3];
  UINT64 DPAStartAddress;    //!< Address from which to start the range scrub.
  UINT64 DPAEndAddress;      //!< Address to end the range scrub.
  UINT64 DPACurrentAddress;  //!< Address that is being currently scrubbed.
  UINT8 Reserved2[100];
} PT_PAYLOAD_ADDRESS_RANGE_SCRUB;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x04h (Address Range Scrub)
**/
typedef struct {
  UINT8 Enable;              //!< Indicates whether an Address Range Scrub is in progress.
  UINT8 Reserved1[3];
  UINT64 DPAStartAddress;    //!< Address from which to start the range scrub.
  UINT64 DPAEndAddress;      //!< Address to end the range scrub.
  UINT8 Reserved2[108];
} PT_PAYLOAD_SET_ADDRESS_RANGE_SCRUB;

typedef union _SMART_VALIDATION_FLAGS {
  UINT32 AllFlags;
  struct {
    UINT32 HealthStatus                     : 1;
    UINT32 PercentageRemaining              : 1;
    UINT32                                  : 1; //!< Reserved
    UINT32 MediaTemperature                 : 1;
    UINT32 ControllerTemperature            : 1;
    UINT32 LatchedDirtyShutdownCount        : 1;
    UINT32 AITDRAMStatus                    : 1;
    UINT32 HealthStatusReason               : 1;
    UINT32                                  : 1;  //!< Reserved
    UINT32 AlarmTrips                       : 1;
    UINT32 LatchedLastShutdownStatus        : 1;
    UINT32 SizeOfVendorSpecificDataValid    : 1;
    UINT32                                  : 20; //!< Reserved
  } Separated;
} SMART_VALIDATION_FLAGS;

typedef struct {
  UINT64 PowerCycles;       //!< Number of DIMM power cycles
  UINT64 PowerOnTime;       //!< Lifetime hours the DIMM has been powered on (represented in seconds)
  UINT64 UpTime;            //!< Current uptime of the DIMM for the current power cycle
  UINT32 UnlatchedDirtyShutdownCount;   //!< This is the # of times that the FW received an unexpected power loss

  /**
    Display the status of the last shutdown that occurred
    Bit 0: PM ADR Command (0 - Not Received, 1 - Received)
    Bit 1: PM S3 (0 - Not Received, 1 - Received)
    Bit 2: PM S5 (0 - Not Received, 1 - Received)
    Bit 3: DDRT Power Fail Command Received (0 - Not Received, 1 - Received)
    Bit 4: PMIC Power Loss (0 - Not Received, 1 - PMIC Power Loss)
    Bit 5: PM Warm Reset (0 - Not Received, 1 - Received)
    Bit 6: Thermal Shutdown Received (0 - Did not occur, 1 Thermal Shutdown Triggered)
    Bit 7: Controller Flush Complete (0 - Did not occur, 1 - Completed)
  **/
  LAST_SHUTDOWN_STATUS_DETAILS LatchedLastShutdownDetails;

  UINT64 LastShutdownTime;

  /**
    Display extended details of the last shutdown that occurred
    Bit 0: Viral Interrupt Command (0 - Not Received, 1 - Received)
    Bit 1: Surprise Clock Stop Interrupt (0 - Not Received, 1 - Received)
    Bit 2: Write Data Flush Complete (0 - Not Completed, 1 - Completed)
    Bit 3: S4 Power State (0 - Not Received, 1 - Received)
    Bit 4: PM Idle (0 - Not Received, 1 - Received)
    Bit 5: Surprise Reset (0 - Not Received, 1 - Received)
    Bit 6-23: Reserved
  **/
  LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED LatchedLastShutdownExtendedDetails;

  UINT8 Reserved[2];

   /**
    Display the status of the last shutdown that occurred
    Bit 0: PM ADR Command (0 - Not Received, 1 - Received)
    Bit 1: PM S3 (0 - Not Received, 1 - Received)
    Bit 2: PM S5 (0 - Not Received, 1 - Received)
    Bit 3: DDRT Power Fail Command Received (0 - Not Received, 1 - Received)
    Bit 4: PMIC Power Loss (0 - Not Received, 1 - PMIC Power Loss)
    Bit 5: PM Warm Reset (0 - Not Received, 1 - Received)
    Bit 6: Thermal Shutdown Received (0 - Did not occur, 1 Thermal Shutdown Triggered)
    Bit 7: Controller Flush Complete (0 - Did not occur, 1 - Completed)
  **/
  LAST_SHUTDOWN_STATUS_DETAILS UnlatchedLastShutdownDetails;

  /**
    Display extended details of the last shutdown that occurred
    Bit 0: Viral Interrupt Command (0 - Not Received, 1 - Received)
    Bit 1: Surprise Clock Stop Interrupt (0 - Not Received, 1 - Received)
    Bit 2: Write Data Flush Complete (0 - Not Completed, 1 - Completed)
    Bit 3: S4 Power State (0 - Not Received, 1 - Received)
    Bit 4: PM Idle (0 - Not Received, 1 - Received)
    Bit 5: Surprise Reset (0 - Not Received, 1 - Received)
    Bit 6-23: Reserved
  **/
  LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED UnlatchedLastShutdownExtendedDetails;

  TEMPERATURE MaxMediaTemperature;      //!< The highest die temperature reported in degrees Celsius.
  TEMPERATURE MaxControllerTemperature; //!< The highest controller temperature repored in degrees Celsius.

  UINT8 ThermalThrottlePerformanceLossPercent; //!< The average loss % due to thermal throttling since last read in current boot
  UINT8 Reserved1[41];
} SMART_INTEL_SPECIFIC_DATA;

/**
  Passthrough Payload:
    Opcode:    0x08h (Get Log Page)
    Sub-Opcode:  0x00h (SMART & Health Info)
    FIS 1.9
**/
typedef struct {
  /** If validation flag is not set it indicates that corresponding field is not valid**/
  SMART_VALIDATION_FLAGS ValidationFlags;

  UINT32 Reserved;
  /**
    Overall health summary
    Bit 0: Normal (no issues detected)
    Bit 1: Noncritical (maintenance required)
    Bit 2: Critical (features or performance degraded due to failure)
    Bit 3: Fatal (data loss has occurred or is imminent)
    Bits 7-4 Reserved
  **/
  UINT8 HealthStatus;

  UINT8 PercentageRemaining;          //!< remaining percentage remaining as a percentage of factory configured spare

  UINT8 Reserved2;
  /**
    Bits to signify whether or not values has tripped their respective thresholds.
    Bit 0: Spare Blocks trips (0 - not tripped, 1 - tripped)
    Bit 1: Media Temperature trip (0 - not tripped, 1 - tripped)
    Bit 2: Controller Temperature trip (0 - not tripped, 1 - tripped)
  **/
  union {
    UINT8 AllFlags;
    struct {
      UINT8 PercentageRemaining : 1;
      UINT8 MediaTemperature : 1;
      UINT8 ControllerTemperature : 1;
    } Separated;
  } AlarmTrips;

  TEMPERATURE MediaTemperature;      //!< Current temperature in Celcius. This is the highest die temperature reported.
  TEMPERATURE ControllerTemperature; //!< Current temperature in Celcius. This is the temperature of the controller.

  UINT32 LatchedDirtyShutdownCount;     //!< Number of times the DIMM Last Shutdown State (LSS) was non-zero.
  UINT8 AITDRAMStatus;            //!< The current state of the AIT DRAM (0 - failure occurred, 1 - loaded)
  UINT16 HealthStatusReason;      //!<  Indicates why the module is in the current Health State
  UINT8 Reserved3[8];

  /**
    00h:       Clean Shutdown
    01h - FFh: Not Clean Shutdown
  **/
  UINT8 LatchedLastShutdownStatus;
  UINT32 VendorSpecificDataSize; //!< Size of Vendor specific structure
  SMART_INTEL_SPECIFIC_DATA VendorSpecificData;
} PT_PAYLOAD_SMART_AND_HEALTH;

/**
  Passthrough Payload - Get Log Page - Firmware Image Info
**/
typedef struct {
  UINT8 FwRevision[FW_BCD_VERSION_LEN];
  UINT8 Reserved1;
  UINT16 FWImageMaxSize;
  UINT8 Reserved2[8];
  UINT8 StagedFwRevision[FW_BCD_VERSION_LEN];
  UINT8 StagedFwActivatable;
  UINT8 LastFwUpdateStatus;
  UINT8 QuiesceRequired;
  UINT16 ActivationTime;
  UINT8 Reserved4[102];
} PT_PAYLOAD_FW_IMAGE_INFO;


#define SRAM_LOG_PAGE_SIZE_BYTES KIB_TO_BYTES(2)
#define SPI_LOG_PAGE_SIZE_BYTES  KIB_TO_BYTES(2)

enum GetFWDebugLogLogAction {
  ActionRetrieveDbgLogSize = 0x00,
  ActionGetDbgLogPage = 0x01,
  ActionGetSramLogPage = 0x02,
  ActionGetSpiLogPage = 0x03,
  ActionInvalid = 0x04,
};

/**
  Passthrough Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x02h (Firmware Debug Log)
**/
typedef struct {
  UINT8   LogAction;
  UINT32  LogPageOffset;
  UINT8   PayloadType;
  UINT8   Reserved[122];
} PT_INPUT_PAYLOAD_FW_DEBUG_LOG;

typedef struct {
  UINT8   LogSize;          //!< Log size in MB
  UINT8   Reserved[127];
} PT_OUTPUT_PAYLOAD_FW_DEBUG_LOG;

/**
  Passthrough Input Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x03h (Memory Info)
**/
typedef struct {
  UINT8 MemoryPage;         //!< Page of the memory information to retrieve
  UINT8 Reserved[127];
} PT_INPUT_PAYLOAD_MEMORY_INFO;

/**
  Passthrough Output Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x03h (Memory Info)
    Page: 0 (Current Boot Info)
**/
typedef struct {
  UINT128 MediaReads;         //!< Number of 64 byte reads from media on the DCPMM since last AC cycle
  UINT128 MediaWrites;        //!< Number of 64 byte writes to media on the DCPMM since last AC cycle
  UINT128 ReadRequests;       //!< Number of DDRT read transactions the DCPMM has serviced since last AC cycle
  UINT128 WriteRequests;      //!< Number of DDRT write transactions the DCPMM has serviced since last AC cycle
  UINT8 Reserved[64];   //!< Reserved
} PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE0;

/**
  Passthrough Output Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x03h (Memory Info)
    Page: 1 (Lifetime Info)
 **/
typedef struct {
  UINT128 TotalMediaReads;          //!< Lifetime number of 64 byte reads from media on the DCPMM
  UINT128 TotalMediaWrites;         //!< Lifetime number of 64 byte writes to media on the DCPMM
  UINT128 TotalReadRequests;        //!< Lifetime number of DDRT read transactions the DCPMM has serviced
  UINT128 TotalWriteRequests;       //!< Lifetime number of DDRT write transactions the DCPMM has serviced
  UINT8 Reserved[64];   //!< Reserved
} PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE1;

/**
  Passthrough Output Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x03h (Memory Info)
    Page: 3 (Error Injection Statistics)
 **/
typedef struct {
  /** This bit field specifies the error inject state:
  * 0 - Error injection enabled
  * 1 - Media temperature injection is enabled
  * 2 - At least one software trigger is enabled
  * 31:2 - reserved
  **/
  UINT32 ErrorInjectStatus;
  UINT32 PoisonErrorInjectionsCounter;      //!< This counter will be incremented each time the set poison error is successfully executed
  UINT32 PoisonErrorClearCounter;           //!< This counter will be incremented each time the clear poison error is successfully executed
  UINT32 MediaTemperatureInjectionsCounter; //!< This counter will be incremented each time the media temperature is injected
  UINT32 SoftwareTriggersCounter;           //!< This counter is incremented each time a software trigger is enabled
  UINT64 SoftwareTriggersEnabledDetails;    //!< For each bit set, the corresponding trigger is currently enabled.
  UINT8 Reserved[100];                      //!< Reserved
} PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE3;

/**
   Passthrough Output Payload:
     Opcode:      0x08h (Get Log Page)
     Sub-Opcode:  0x03h (Memory Info)
     Page: 4 (Average Power Consumption Statistics)
  **/
typedef struct {
  UINT16 DcpmmAveragePower;                 //!< Average power consumption by the module
  UINT16 AveragePower12V;                   //!< 12V average power consumption by the module
  UINT16 AveragePower1_2V;                  //!< 1.2V average power consumption by the module
  UINT8 Reserved[122];                      //!< Reserved
} PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE4;

/**
  Passthrough Output Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x04h (Long Operation Status)
**/
typedef struct {
  UINT8  CmdOpcode;
  UINT8  CmdSubcode;
  UINT16 Percent;
  UINT32 EstimatedTimeLeft;
  UINT8  Status;
  UINT8  CmdSpecificData[119];
} PT_OUTPUT_PAYLOAD_FW_LONG_OP_STATUS;

enum GetErrorLogLevel {
  ErrorLogLowPriority = 0x00,
  ErrorLogHighPriority = 0x01,
  ErrorLogInvalidPriority = 0x02,
};

enum GetErrorLogType {
  ErrorLogTypeMedia = 0x00,
  ErrorLogTypeThermal = 0x01,
  ErrorLogTypeInvalid = 0x02,
};

enum GetErrorLogInfo {
  ErrorLogInfoEntries = 0x00,
  ErrorLogInfoData = 0x01,
  ErrorLogInfoInvalid = 0x02,
};

enum GetErrorLogPayloadReturn {
  ErrorLogSmallPayload = 0x00,
  ErrorLogLargePayload = 0x01,
  ErrorLogInvalidPayload = 0x02,
};
/**
  Transaction type that caused error. Limited to 64 transaction types
**/
enum GetErrorTransacitonType {
  ErrorTransaction2LMRead        = 0x00,
  ErrorTransaction2LMWrite       = 0x01,
  ErrorTransactionPMRead         = 0x02,
  ErrorTransactionPMWrite        = 0x03,
  ErrorTransactionBWRead         = 0x04,
  ErrorTransactionBWWrite        = 0x05,
  ErrorTransactionAITRead        = 0x06,
  ErrorTransactionAITWrite       = 0x07,
  ErrorTransactionWearLevelMove  = 0x08,
  ErrorTransactionPatrolScrub    = 0x09,
  ErrorTransactionCSRRead        = 0x0A,
  ErrorTransactionCSRWrite       = 0x0B,
  /** 0x0C - 0x40  Reserved for other transaction **/
  ErrorTransactionNotValid       = 0x41,    /** 0x41 - 0xFF  Not Valid **/
};

/**
  Temperature group being reported.
**/
enum GetErrorThermalReportedType {
  ErrorThermalReportedLow        = 0x1,
  ErrorThermalReportedHigh       = 0x2,
  ErrorThermalReportedCritical   = 0x4
};

/**
  Mailbox status return codes.
**/
enum MailboxStatusReturnCode {
  MailboxSuccess        = FW_SUCCESS,
  MailboxDeviceBusy     = FW_DEVICE_BUSY,
  MailboxDataNotSet     = FW_DATA_NOT_SET
};

/**
  Passthrough Input Payload:
     Opcode:      0x08h (Get Log Page)
     Sub-Opcode:  0x05h (Error Log)
**/
typedef struct {
  union {
    UINT8 AsUint8;
    struct {
      UINT8 LogLevel                 : 1;   //!< Specifies which error log to retrieve
      UINT8 LogType                  : 1;   //!< Specifies which log type to access
      UINT8 LogInfo                  : 1;   //!< Specifies which log type data to return (entries / log info)
      UINT8 LogEntriesPayloadReturn  : 1;   //!< Specifies which payload return log entries (small / large)
      UINT8                          : 4;   //!< Reserved
    } Separated;
  } LogParameters;
  UINT16 SequenceNumber;     //!< Log entries with sequence number equal or higher than the provided will be returned
  UINT16 RequestCount;       //!< Max number of log entries requested for this access
  UINT8 Reserved[123];
} PT_INPUT_PAYLOAD_GET_ERROR_LOG;

typedef struct _LOG_INFO_DATA_RETURN {
  UINT16 MaxLogEntries;
  UINT16 CurrentSequenceNum;
  UINT16 OldestSequenceNum;
  UINT64 OldestLogEntryTimestamp;
  UINT64 NewestLogEntryTimestamp;
  UINT8  AdditionalLogStatus;
  UINT8  Reserved[105];
} LOG_INFO_DATA_RETURN;

/**
  Passthrough Output Payload (Media):
     Opcode:      0x08h (Get Log Page)
     Sub-Opcode:  0x05h (Error Log)
**/
typedef struct {
    UINT16 ReturnCount;           //!< Number of log entries returned
    UINT8 LogEntries[126];        //!< Media log entry table
} PT_OUTPUT_PAYLOAD_GET_ERROR_LOG;

/**
  Passthrough Output Media Log Entry Format
**/
typedef struct {
  UINT64  SystemTimestamp;    //!< Unix epoch time of log entry
  UINT64  Dpa;                //!< Specifies DPA address of error
  UINT64  Pda;                //!< Specifies PDA address of the failure
  UINT8   Range;              //!< Specifies the length in address space of this error. Ranges will be encoded as power of 2.
  UINT8   ErrorType;          //!< Indicates what kind of error was logged.
  union {
    UINT8 AsUint8;            //!< Indicates error flags for this entry.
    struct {
      UINT8 PdaValid  : 1;    //!< Indicates the PDA address is valid.
      UINT8 DpaValid  : 1;    //!< Indicates the DPA address is valid.
      UINT8 Interrupt : 1;    //!< Indicates this error generated an interrupt packet
      UINT8           : 1;    //!< Reserved
      UINT8 Viral     : 1;    //!< Indicates Viral was signaled for this error
      UINT8           : 3;    //!< Reserved
    } Spearated;
  } ErrorFlags;
  UINT8  TransactionType;     //!< Indicates what transaction caused the error
  UINT16 SequenceNum;
  UINT8  Reserved[2];
} PT_OUTPUT_PAYLOAD_GET_ERROR_LOG_MEDIA_ENTRY;

/**
  Passthrough Output Thermal Log Entry Format
**/
typedef struct {
  UINT64  SystemTimestamp;    //!< Unix epoch time of log entry
  union {
    UINT32 AsUint32;
    struct {
      UINT32 Temperature : 15; //!< In celsius
      UINT32 Sign        : 1;  //!< Positive or negative
      UINT32 Reported    : 3;  //!< Temperature being reported
      UINT32 Type        : 2;  //!< Controller or media temperature
      UINT32             : 11; //!< Reserved
    } Separated;
  } HostReportedTempData;
  UINT16 SequenceNum;
  UINT8  Reserved[2];
} PT_OUTPUT_PAYLOAD_GET_ERROR_LOG_THERMAL_ENTRY;

/**
  Passthrough Input Payload:
     Opcode:      0x08h (Get Log Page)
     Sub-Opcode:  0xFFh (Command Effect Log)
**/
typedef struct {
  UINT8 PayloadType;
  UINT8 LogAction;
  UINT8 EntryOffset;
  UINT8 Reserved[125];
} PT_INPUT_PAYLOAD_GET_COMMAND_EFFECT_LOG;

/**
  Get Command Effect Log Input Payload Enum types
**/

enum GetCelPayloadType {
  LargePayload = 0x00,
  SmallPayload = 0x01
};

enum GetCelLogAction {
  EntriesCount = 0x00,
  CelEntries = 0x01
};

/**
  Passthrough Output Payload:
     Opcode:      0x08h (Get Log Page)
     Sub-Opcode:  0xFFh (Command Effect Log)
**/
typedef struct {
  UINT32  LogEntryCount;
  UINT8   Reserved[124];
} PT_OUTPUT_PAYLOAD_GET_CEL_COUNT;

typedef struct {
  COMMAND_EFFECT_LOG_ENTRY CelEntry[16];
} PT_OUTPUT_PAYLOAD_GET_CEL_ENTRIES;

typedef struct {
  union {
    PT_OUTPUT_PAYLOAD_GET_CEL_COUNT CelCount;
    PT_OUTPUT_PAYLOAD_GET_CEL_ENTRIES CelEntries;
    UINT8 Data[0];
  } LogTypeData;
} PT_OUTPUT_PAYLOAD_GET_COMMAND_EFFECT_LOG;

/**
Passthrough Input Payload:
Opcode:    0x07h (Get Admin Feature)
Sub-Opcode:  0xCAh (Command Access Policy)
**/
typedef struct {
  UINT8  Opcode;
  UINT8  Subopcode;
  UINT8  Reserved2[126];
} PT_INPUT_PAYLOAD_GET_COMMAND_ACCESS_POLICY;

typedef struct {
  UINT8 Restriction;
  UINT8  Reserved[127];
} PT_OUTPUT_PAYLOAD_GET_COMMAND_ACCESS_POLICY;

/**
  Passthrough Output DDRT IO Init Info
**/

typedef struct {
  union {
    UINT8 AsUint8;
    struct {
      /**
        Bit 0-3: Valid Values:
          0b0000 = 1600 MT/s (default)
          0b0001 = 1866 MT/s
          0b0010 = 2133 MT/s
          0b0011 = 2400 MT/s
          0b0100 = 2666 MT/s
          0b0101 = 2933 MT/s
          0b0110 = 3200 MT/s
          0b0111 = reserved
          0b1xxx = reserved
        Bit 4: VDDQ (0 - 1.2V (default), 1 - reserved for low voltage)
        Bit 5: Write Preamble (0 - 1 nCk (default), 1 - 2 nCk)
        Bit 6: Read Preamble (0 - 1 nCk (default), 1 - 2 nCk)
        Bit 7: Gate PLL (0 - PLL's Un-Gated, 1 - PLL's Gated)
      **/
      UINT8 OperatingFrequency : 4; //!< Valid values above.
      UINT8 Vddq               : 1; //!< Encoding for DDRT voltage
      UINT8 WritePreamble      : 1; //!< DDRT Mode Register for Write Preamble.
      UINT8 ReadPreamble       : 1; //!< DDRT Mode Register for Read Preamble.
      UINT8 GatePll            : 1; //!< This denotes wheter the FW is gating ppl's for programming.
    } Separated;
  } DdrtIoInfo;
  /**
    DDRT Training State possible values:
    0x00 - Training Not Complete
    0x01 - Training Complete
    0x02 - Training Failure
    0x03 - S3 Complete
    0x04 - Normal Mode Complete
  **/
  UINT8 DdrtTrainingStatus; //!<Designates training has been completed by BIOS.
  union {
    UINT8 AsUint8;
    struct {
      /**
        Bit 0: CKE Synchronization (0 - disabled (default), 1 - enabled)
        Bit 1-7: reserved
      **/
      UINT8 CKESynchronization : 1; //!< Allows enabling or disabling of the CKE synchronization
      UINT8                    : 7; //!< reserved
    } Separated;
  } AdditionalDDRTOptions;
  UINT8 Reserved[125];
} PT_OUTPUT_PAYLOAD_GET_DDRT_IO_INIT_INFO;

/**
Passthrough Payload:
  Opcode: 0x0Ah (Inject Error)
  Sub-Opcode: 0x02h (Media Temperature Error)
**/
typedef struct {
  /*
  * Allows the enabling or disabling of the temperature error
  * 0x00h - Off (default)
  * 0x01h - On
  */
  UINT8 Enable;
        union {
          /*
          * A number representing the temperature (Celsius) to inject
          * Bit 3-0: Fractional value of temperature (0.0625 C resolution)
          * Bit 4-14: Integer value of temperature in Celsius
          * Bit 15: Sign Bit ( 1 = negative, 0 = positive)
          */
          struct {
            UINT16 TemperatureFractional:4;
            UINT16 TemperatureInteger:11;
            UINT16 TemperatureSign:1;
          } Separated;
          /*
          * A number representing the temperature (Celsius) to inject
          * Bit 14-0: Temperature in Celsius with 0.0625 C resolution
          * Bit 15: Sign Bit ( 1 = negative, 0 = positive)
          */
          struct {
            UINT16 Temperature:15;
            UINT16 Sign:1;
          } SignSeparated;
          UINT16 AsUint16;
        } Temperature;
  UINT8 Reserved[125];
} PT_INPUT_PAYLOAD_INJECT_TEMPERATURE;

/**
Passthrough Payload:
Opcode: 0x0Ah (Inject Error)
Sub-Opcode: 0x01h (Poison Error)
**/
typedef struct {
  /*
  * Allows the enabling or disabling of poison for this address
  * 0x00h - Clear
  * 0x01h - Set
  */
  UINT8 Enable;
  UINT8 Reserved1;

  /*
  * 0x00 - Intel Reserved
  * 0x01 - 2LM
  * 0x02 - App Direct
  * 0x03 - Intel Reserved
  * 0x04 - Patrol scrub (Memory Transaction type)
  * 0xFF - 0x05 - Intel Reserved
  */
  UINT8 Memory;
  UINT8 Reserved2;
  UINT64 DpaAddress; /* Address to set the poison bit for */
  UINT8 Reserved3[116];
} PT_INPUT_PAYLOAD_INJECT_POISON;

typedef union {
  UINT8 AllBits;
  struct {
    UINT8 Enable : 1;
    UINT8 Value  : 7;
  }Separated;
} PERCENTAGE_REMAINING;
/*
* Passthrough Payload:
*    Opcode:    0x0Ah (Inject Error)
*    Sub-Opcode:  0x03h (Software Triggers)
*/
typedef struct {
  /*
  * Contains a bit field of the triggers
  * Bit 0: Package Spare Trigger
  * Bit 1: Reserved
  * Bit 2: Fatal Error Trigger
  * Bit 3: Spare Block Percentage Trigger
  * Bit 4: Dirty Shutdown Trigger
  * Bit 63-5: Reserved
  */
  UINT64 TriggersToModify;

  /*
  * Spoofs FW to initiate a Package Sparing.
  * 0x0h - Do Not/Disable Trigger
  * 0x1h - Enable Trigger
  */
  UINT8 PackageSparingTrigger;

  UINT16 Reserved1;

  /*
  * Spoofs FW to trigger a fatal media error.
  * 0x0h - Do Not/Disable Trigger
  * 0x1h - Enable Trigger
  */
  UINT8 FatalErrorTrigger;

  /*
  * Spoofs spare block percentage within the DIMM.
  * Bit 0 - Enable/Disable Trigger
  * 0x0h - Do Not/Disable Trigger
  * 0x1h - Enable Trigger
  * Bits 7:1 - Spare Block Percentage (valid values are between 0 and 100)
  */
  PERCENTAGE_REMAINING SpareBlockPercentageTrigger;

  /*
  * Spoofs a dirty shutdown on the next power cycle.
  * 0x0h - Do Not/Disable Trigger
  * 0x1h - Enable Trigger
  */
  UINT8 DirtyShutdownTrigger;

  UINT8 Reserved2[115];
} PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS;

/*
* Passthrough Payload:
*    Opcode:    0x0Ah (Inject Error)
*    Sub-Opcode:  0x00h (Enable Injection)
*  Small Input Payload
*/
typedef struct {
  /*
  * Used to turn off/on injection functionality
  * 0x00h - Off ( default)
  * 0x01h - On
  */
  UINT8 Enable;
  UINT8 Reserved[127];
} PT_INPUT_PAYLOAD_ENABLE_INJECTION;

/**
Passthrough Payload:
Opcode:    0x06h (Get Admin Features)
Sub-Opcode:  0x00h (System Time)
**/
typedef struct {
  UINT64 UnixTime;     //!< The number of seconds since 1 January 1970
  UINT8 Reserved[120];
} PT_SYTEM_TIME_PAYLOAD;

/**
  Passthrough Output Payload:
     Opcode:      0x06h (Get Admin Features)
     Sub-Opcode:  0xEAh (Extended ADR)
**/
typedef struct {
  UINT8 ExtendedAdrStatus;            //!< Specifies whether extended ADR flow is enabled
  UINT8 PreviousExtendedAdrStatus;    //!< Was extended ADR flow enabled during last power cycle
  UINT8 Reserved[126];                //!< Padding to meet 128byte payload size
} PT_OUTPUT_PAYLOAD_GET_EADR;

/**
  Passthrough Payload:
    Opcode:    0x06h (Get Admin Features)
    Sub-Opcode:  0x09h (Latch System Shutdown State)
**/
typedef struct {
  UINT8 LatchSystemShutdownState;                       //!< Specifies whether latch is enabled
  UINT8 PreviousPowerCycleLatchSystemShutdownState;     //!< Specifies whether latch was enabled during the last power cycle
  UINT8 Reserved[126];
} PT_OUTPUT_PAYLOAD_GET_LATCH_SYSTEM_SHUTDOWN_STATE;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* _NVMDIMMPASSTHRU_H_ */
