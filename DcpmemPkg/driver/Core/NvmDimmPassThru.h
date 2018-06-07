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

// PassThru timeout in microseconds
#define PT_TIMEOUT_INTERVAL SECONDS_TO_MICROSECONDS(1)

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


typedef struct {
  volatile UINT64 *pCommand;                      //!< va of the command register
  volatile UINT64 *pNonce0;                       //!< va of the nonce0 register
  volatile UINT64 *pNonce1;                       //!< va of the nonce1 register
  volatile UINT64 *pInPayload[IN_PAYLOAD_NUM];    //!< va of the payload registers write only
  volatile UINT64 *pStatus;                       //!< va of the status register
  volatile UINT64 *pOutPayload[OUT_PAYLOAD_NUM];  //!< va of the payload registers read only
  UINT32 MbInLineSize;
  UINT32 MbOutLineSize;
  UINT32 NumMbInSegments;                         //!< number of segments of the IN mailbox
  UINT32 NumMbOutSegments;                        //!< number of segments of the OUT mailbox
  UINT8 SequenceBit;                              //!< current sequence bit state for mailbox
  volatile VOID **ppMbIn;                         //!< va of the IN mailbox segments
  volatile VOID **ppMbOut;                        //!< va of the OUT mailbox segments
  volatile UINT64 *pBsr;                          //!< va of the DIMMs BSR register
} MAILBOX;

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
  Poll Firmware Command Completion
  Poll the status register of the mailbox waiting for the
  mailbox complete bit to be set

  @param[in] pMb - The mailbox the fw cmd was submitted on
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the protocol command.
             A Timeout value of 0 means that this function will wait indefinitely for the protocol command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.
  @param[out] pStatus The Fw status to be returned when command completes
  @param[out] pBsr The boot status register when the command completes OPTIONAL

  @retval EFI_SUCCESS Success
  @retval EFI_DEVICE_ERROR FW error received
  @retval EFI_TIMEOUT A timeout occurred while waiting for the protocol command to execute.

**/
EFI_STATUS
PollCmdCompletion(
  IN      MAILBOX *pMb,
  IN      UINT64 Timeout,
      OUT UINT64 *pStatus,
      OUT UINT64 *pBsr OPTIONAL
  );

/**
  Pass thru command to FW
  Sends a command to FW and waits for response from firmware

  @param[in,out] pCmd A firmware command structure
  @param[in] pMb OPTIONAL A mailbox to call pass thru command, if NULL passed then mailbox is taken from inventory base
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
PassThru (
  IN     struct _DIMM *pDimm,
  IN OUT FW_CMD *pCmd,
  IN     UINT64 Timeout
  );

/**
  Pass thru command to FW, but retry FW_ABORTED_RETRIES_COUNT_MAX times if we receive a FW_ABORTED
  response code back.

  @param[in,out] pCmd A firmware command structure
  @param[in] pMb OPTIONAL A mailbox to call pass thru command, if NULL passed then mailbox is taken from inventory base
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
  IN OUT FW_CMD *pCmd,
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
  PtMax = 0xF2
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
};

/**
  Defines the Sub-Opcodes for PtSetSecInfo
**/
enum SetSecInfoSubop {
  SubopOverwriteDimm = 0x01,
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
  SubopExecuteFw = 0x01       //!< Executes a new updated FW Image. (without a restart)
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


/**
  MemInfo page 3 Errror Inject status bits
 **/
#define ERR_INJECTION_ENABLED_BIT 0x01
#define ERR_INJECTION_MEDIA_TEMP_ENABLED_BIT 0x02
#define ERR_INJECTION_SW_TRIGGER_ENABLED_BIT 0x04

/**
  Passthrough Payload:
    Opcode: 0x01h (Identify DIMM)
**/
typedef struct {
  UINT16 Vid;                     //!< 1-0   : DIMM vendor id
  UINT16 Did;                     //!< 3-2   : Device ID
  UINT16 Rid;                     //!< 5-4   : Revision ID
  UINT16 Ifc;                     //!< 7-6   : Interface format code (0x301)
  UINT8 Fwr[FW_BCD_VERSION_LEN];  //!< 12-8  : BCD formated firmware revision
  UINT8 Reservd0;                 //!< 13    : Reserved
  UINT8 Fswr;                     //!< 14    : Feature SW Required Mask
  UINT8 Reservd1;                 //!< 15    : Reserved
  UINT16 Nbw;                     //!< 17-16 : Number of block windows
  UINT8 Reserved[10];             //!< 27-18 : Reserved
  UINT32 Obmcr;                   //!< 31-28 : Offset of block mode control region
  UINT32 Rc;                      //!< 35-32 : Raw capacity
  UINT16 Mf;                      //!< 37-36 : Manufacturer ID
  UINT32 Sn;                      //!< 41-38 : Serial Number ID
  CHAR8 Pn[DIMM_PN_LEN];          //!< 61-42 : ASCII Part Number
  UINT32 DimmSku;                 //!< 65-62 : DIMM SKU
  UINT16 Ifce;                    //!< 67-66 : Interface format code extra (0x201)
  UINT16 ApiVer;                  //!< 69-68 : API Version
  UINT8 Resrvd2[58];              //!< 127-70: Reserved
} PT_ID_DIMM_PAYLOAD;

typedef struct {
  TEMPERATURE ControllerShutdownThreshold;
  TEMPERATURE MediaShutdownThreshold;
  TEMPERATURE MediaThrottlingStartThreshold;
  TEMPERATURE MediaThrottlingStopThreshold;
  UINT8 Reserved[120];
} PT_DEVICE_CHARACTERISTICS_PAYLOAD;

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
  UINT8 SecurityStatus;
  UINT8 Resrvd[127];
} PT_GET_SECURITY_PAYLOAD;

/**
  Passthrough Payload:
    Opcode:     0x02h (Set Security Info)
    Sub-Opcode: 0xF1h (Set Passphrase)
**/
typedef struct {
  INT8 PassphraseCurrent[PASSPHRASE_BUFFER_SIZE]; //!< 31:0 The current security passphrase
  UINT8 Reserved[32];                             //!< 63:32 Reserved
  INT8 PassphraseNew[PASSPHRASE_BUFFER_SIZE];     //!< The new passphrase to be set/changed to
} PT_SET_SECURITY_PAYLOAD;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x06h (Optional Configuration Data Policy)
**/
typedef struct {
  UINT8 FirstFastRefresh;     //!< Enable / disable of acceleration of first refresh cycle
  UINT8 ViralPolicyEnable;    //!< This shows curent state of Viral Policies
  UINT8 ViralStatus;          //!< This determines the viral status
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
    Sub-Opcode:  0x02h (Power Managment Policy)
**/
typedef struct {
  /**
    Enable/Disable reflects whether the power managment policy is enabled or disabled.
    Disabling power managment will automatically change the Stop/Gate timers to 0x00
    thus allowing for no power savings.
  **/
  UINT8 Enable;
  /**
    Power budget in mW used for instantaneous power.
    Valid range for power budget 10000 - 20000 mW.
  **/
  UINT16 PeakPowerBudget;
    /**
    Power budget in mW used for averaged power.
    Valid range for power budget 10000 - 18000 mW.
    **/
  UINT16 AveragePowerBudget;

  UINT8 Reserved[123];
} PT_PAYLOAD_POWER_MANAGEMENT_POLICY;

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
  UINT32 Size;      //!< Size in bytes of the selected partition
  UINT32 TotalSize; //!< Total size in bytes of the Platform Config Area
} PT_OUTPUT_PAYLOAD_GET_PLATFORM_CONFIG_DATA;

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
  UINT32 StorageModeEnabled             : 1;
  UINT32 AppDirectModeEnabled           : 1;
  UINT32 PackageSparingCapable          : 1;
  UINT32                                : 12;  //!< Reserved
  UINT32 SoftProgramableSku             : 1;
  UINT32 EncryptionEnabled              : 1;
  UINT32                                : 14;  //!< Reserved
} SKU_INFORMATION;

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
  UINT8 Aggressiveness;   //!< How aggressive to be on package sparing (0...255)
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
  UINT8 Aggressiveness;   //!< How aggressive to be on package sparing (0...255)
  UINT8 Reserved[126];    //!< 127-2 : Reserved
} PT_PAYLOAD_SET_PACKAGE_SPARING_POLICY;

/**
  Passthrough Payload:
    Opcode:    0x04h (Get Features)
    Sub-Opcode:  0x04h (Address Range Scrub)
**/
typedef struct {
  UINT8 Enable;              //!< Indicates whether an Address Range Scrub is in progress.
  UINT8 Reserved1[2];
  UINT64 DPAStartAddress;    //!< Address from which to start the range scrub.
  UINT64 DPAEndAddress;      //!< Address to end the range scrub.
  UINT64 DPACurrentAddress;  //!< Address that is being currently scrubbed.
  UINT8 Reserved2[101];
} PT_PAYLOAD_ADDRESS_RANGE_SCRUB;

typedef union _SMART_VALIDATION_FLAGS {
  UINT32 AllFlags;
  struct {
    UINT32 HealthStatus                     : 1;
    UINT32 PercentageRemaining              : 1;
    UINT32 PercentageUsed                   : 1;
    UINT32 MediaTemperature                 : 1;
    UINT32 ControllerTemperature            : 1;
    UINT32 UnsafeShutdownCount               : 1;
    UINT32 AITDRAMStatus                    : 1;
    UINT32 HealthStausReason                : 1;
    UINT32                                  : 1;  //!< Reserved
    UINT32 AlarmTrips                       : 1;
    UINT32 LastShutdownStatus               : 1;
    UINT32 SizeOfVendorSpecificDataValid    : 1;
    UINT32                                  : 20; //!< Reserved
  } Separated;
} SMART_VALIDATION_FLAGS;

typedef struct {
  UINT64 PowerCycles;       //!< Number of DIMM power cycles
  UINT64 PowerOnTime;       //!< Lifetime hours the DIMM has been powered on (represented in seconds)
  UINT64 UpTime;            //!< Current uptime of the DIMM for the current power cycle
  UINT32 DirtyShutdowns;   //!< This is the # of times that the FW received an unexpected power loss

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
  LAST_SHUTDOWN_STATUS LastShutdownDetails;

  UINT64 LastShutdownTime;

  /**
    Display extended details of the last shutdown that occured
    Bit 0: Viral Interrupt Command (0 - Not Received, 1 - Received)
    Bit 1: Surprise Clock Stop Interrupt (0 - Not Received, 1 - Received)
    Bit 0: Write Data Flush Complete (0 - Not Completed, 1 - Completed)
    Bit 1: S4 Power State (0 - Not Received, 1 - Received)
    Bit 4-23: Reserved
  **/
  LAST_SHUTDOWN_STATUS_EXTENDED LastShutdownExtendedDetails;

  UINT8 Reserved[52];
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
  /**
    Device life span as a percentage.
    100 = warranted life span of device has been reached however values up to 255 can be used.
  **/
  UINT8 PercentageUsed;

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

  UINT32 DirtyShutdownCount;     //!< Number of times the DIMM Last Shutdown State (LSS) was non-zero.
  UINT8 AITDRAMStatus;            //!< The current state of the AIT DRAM (0 - disabled, 1 - enabled)
  UINT16 HealthStatusReason;      //!<  Indicates why the module is in the current Health State
  UINT8 Reserved2[8];

  /**
    00h:       Clean Shutdown
    01h - FFh: Not Clean Shutdown
  **/
  UINT8 LastShutdownStatus;
  UINT32 VendorSpecificDataSize; //!< Size of Vendor specific structure
  SMART_INTEL_SPECIFIC_DATA VendorSpecificData;
} PT_PAYLOAD_SMART_AND_HEALTH;

/**
  Passthrough Payload - Get Log Page - Firmware Image Info
**/
typedef struct {
  UINT8 FwRevision[FW_BCD_VERSION_LEN];
  UINT8 FwType;
  UINT8 Reserved1[10];
  UINT8 StagedFwRevision[FW_BCD_VERSION_LEN];
  UINT8 StagedFwType;
  UINT8 LastFwUpdateStatus;
  UINT8 Reserved2[9];
  CHAR8 CommitId[FW_COMMIT_ID_LENGTH];
  CHAR8 BuildConfiguration[FW_BUILD_CONFIGURATION_LENGTH];
  UINT8 Reserved3[40];
} PT_PAYLOAD_FW_IMAGE_INFO;

enum GetFWDebugLogLogAction {
  ActionRetrieveDbgLogSize = 0x00,
  ActionGetDbgLogPage = 0x01,
  ActionInvalid = 0x02,
};

/**
  Passthrough Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x02h (Firmware Debug Log)
**/
typedef struct {
  UINT8   LogAction;
  UINT32  LogPageOffset;
  UINT8   Reserved[123];
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

typedef struct {
  UINT8  CmdOpcode;
  UINT8  CmdSubcode;
  UINT16 Percent;
  UINT32 EstimatedTimeLeft;
  UINT8  Status;
  UINT8  CmdSpecificData[119];
} PT_OUTPUT_PAYLOAD_FW_LONG_OP_STATUS;

/**
  Passthrough Output Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x03h (Memory Info)
    Page: 0 (Current Boot Info)
**/
typedef struct {
  UINT128 BytesRead;          //!< Number of 64 byte reads from the DIMM
  UINT128 BytesWritten;       //!< Number of 64 byte writes to the DIMM
  UINT128 ReadRequests;       //!< Number of DDRT read transactions the DIMM has serviced
  UINT128 WriteRequests;      //!< Number of DDRT write transactions the DIMM has serviced
  UINT128 BlockReadRequests;  //!< Number of BW read requests the DIMM has serviced
  UINT128 BlockWriteRequests; //!< Number of BW write requests the DIMM has serviced
  UINT8 Reserved[32];   //!< Reserved
} PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE0;

/**
  Passthrough Output Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x03h (Memory Info)
    Page: 1 (Lifetime Info)
 **/
typedef struct {
  UINT128 TotalBytesRead;           //!< Lifetime number of 64 byte reads from the DIMM
  UINT128 TotalBytesWritten;        //!< Lifetime number of 64 byte writes to the DIMM
  UINT128 TotalReadRequests;        //!< Lifetime number of DDRT read transactions the DIMM has serviced
  UINT128 TotalWriteRequests;       //!< Lifetime number of DDRT write transactions the DIMM has serviced
  UINT128 TotalBlockReadRequests;   //!< Lifetime number of BW read requests the DIMM has serviced
  UINT128 TotalBlockWriteRequests;  //!< Lifetime number of BW write requests the DIMM has serviced
  UINT8 Reserved[32];   //!< Reserved
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
  UINT8 Reserved[108];                      //!< Reserved
} PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE3;

/**
  Passthrough Payload:
    Opcode:      0x08h (Get Log Page)
    Sub-Opcode:  0x04h (Long Operations Status)
**/
typedef struct
{
  /**
    This will coincide with the opcode & sub-opcode
    Bits 7:0 - Opcode
    Bits 15:8 - Sub-Opcode
  **/
  UINT16 Command;
  UINT16 PercentComplete; //!< The % complete of the current command (BCD encoded)

  /**
    Estimated Time to Completion.
    Time in seconds till the Long Operation in Progress is expected to be completed
  **/
  UINT32 EstimatedTimeToCompletion;
  UINT8 StatusCode; //!< The completed mailbox status code of the long operation
  UINT8 reserved[119]; //!< Reserved
} PT_PAYLOAD_LONG_OPERATION_STATUS;

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
  union {
    UINT8 RequestCountFis1_2;
    UINT16 RequestCountFis1_3;
  } RequestCount;            //!< Max number of log entries requested for this access
  UINT8 Reserved[123];
} PT_INPUT_PAYLOAD_GET_ERROR_LOG;

typedef struct _LOG_INFO_DATA_RETURN {
  UINT16 MaxLogEntries;
  union {
    struct {
      UINT16 NewLogEntriesFis;
      UINT64 OldestLogEntryTimestamp;
      UINT64 NewestLogEnrtyTimestamp;
      UINT8 Reserved[108];
    } FIS_1_2;
    struct {
      UINT16 CurrentSequenceNum;
      UINT16 OldestSequenceNum;
      UINT64 OldestLogEntryTimestamp;
      UINT64 NewestLogEnrtyTimestamp;
      UINT8 Reserved[106];
    } FIS_1_3;
  } Params;
} LOG_INFO_DATA_RETURN;

/**
  Passthrough Output Payload (Media):
     Opcode:      0x08h (Get Log Page)
     Sub-Opcode:  0x05h (Error Log)
**/
typedef struct {
  union {
    struct {
      UINT16 NumTotalEntries;
      union {
        UINT8 AsUint8;
        struct {
          UINT8 ReturnCount : 7;
          UINT8 OverrunFlag : 1;
        } Separated;
      } ReturnInfo;
      UINT8 LogEntries[125];
    } FIS_1_2;
    struct {
      UINT16 ReturnCount;           //!< Number of log entries returned
      UINT8 LogEntries[126];        //!< Media log entry table
    } FIS_1_3;
  } Params;
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
    Opcode:    0x08h (Get Log Page)
    Sub-Opcode:  0xFAh (Failure Analysis Data)
**/
typedef struct {
  UINT8  Action;
  UINT8  Reserved1[3];
  UINT32 ID;
  UINT32 Offset;
  UINT8  Reserved2[116];
} PT_INPUT_PAYLOAD_GET_FAILURE_ANALYSIS_DATA;

enum GetFailureAnalysisDataAction {
  ActionGetFAInventory = 0x00,
  ActionGetFABlobHeader = 0x01,
  ActionGetFABlobSmallPayload = 0x02,
  ActionGetFABlobLargePayload = 0x03
};

typedef struct {
  UINT32 MaxFATokenID;
  UINT8  Reserved[124];
} PT_OUTPUT_PAYLOAD_GET_FA_INVENTORY;

typedef struct {
  UINT32 Version;
  UINT32 Size;
  UINT32 TokenID;
  UINT8  DimmID[9];
  UINT8  Sha256[32];
  UINT8  SessionKey[16];
  UINT8  SesessionIV[16];
  UINT8  Reserved[43];
} PT_OUTPUT_PAYLOAD_GET_FA_BLOB_HEADER;

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
  **/
  UINT8 DdrtTrainingStatus; //!<Designates training has been completed by BIOS.
  UINT8 Reserved[126];
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
	* 0x00 - Intel_Reserved
	* 0x01 - 2LM
	* 0x02 - App Direct
	* 0x03 - Storage
	* 0x04 - Patrol scrub (Memory Transaction type)
	* 0xFF - 0x05 - Intel Reserved
	*/
	UINT8 Memory;
	UINT8 Reserved2;
	UINT64 DpaAddress; /* Address to set the poison bit for */
	UINT8 Reserved3[116];
} PT_INPUT_PAYLOAD_INJECT_POISON;

/*
* Passthrough Payload:
*		Opcode:		0x0Ah (Inject Error)
*		Sub-Opcode:	0x03h (Software Triggers)
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

	UINT8 Reserved1;

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
	UINT8 SpareBlockPercentageTrigger;

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
*		Opcode:		0x0Ah (Inject Error)
*		Sub-Opcode:	0x00h (Enable Injection)
*	Small Input Payload
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

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* _NVMDIMMPASSTHRU_H_ */
