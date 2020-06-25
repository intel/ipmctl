/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file nvm_types.h
 * @brief This file defines standard types used in the Native Management API.
 */

#ifndef NVM_TYPES_H_
#define NVM_TYPES_H_

#include <stddef.h>
#include <limits.h>
#include <NvmSharedDefs.h>

#ifdef _MSC_VER
#include <stdlib.h>
#define PATH_MAX _MAX_PATH
#endif

#define DIMM_ACPI_EVENT_SMART_HEALTH_MASK 1 << ACPI_SMART_HEALTH
#define DIMM_ACPI_EVENT_UNCORRECTABLE_MASK  1 << ACPI_UNCORRECTABLE

#define MAX_IN_MB_SIZE          (1 << 20)   //!< Size of the OS mailbox large input payload
#define MAX_OUT_MB_SIZE         (1 << 20)   //!< Size of the OS mailbox large output payload
#define MAX_IN_PAYLOAD_SIZE     (128)       //!< Total size of the input payload registers
#define MAX_OUT_PAYLOAD_SIZE    (128)       //!< Total size of the output payload registers

/**
byte swap
*/
#define SWAP_BYTES_U16(u16) (((u16 >> 8)& 0x00FF) | ((u16 << 8)& 0xFF00))

#define NVM_PRODUCT_NAME "Intel(R) Optane(TM) DC persistent memory Software\0"
#define NVM_SYSLOG_SOURCE          "NVM_MGMT"

#define NVM_COMPUTERNAME_LEN 256 ///< Length of host string
#define NVM_OSNAME_LEN  256 ///< Length of host OS string
#define NVM_OSVERSION_LEN 256 ///< Length of host OS version number
#define NVM_PROCESSOR_LEN 256 ///< Length of host processor string
#define NVM_VERSION_LEN 25  ///< Length of version string
#define NVM_ERROR_LEN 256 ///< Length of return code description
#define NVM_MAX_HANDLE_LEN 11 ///< Max length of a uint32 in decimal + '\0'
#define NVM_MAX_UID_LEN 22 ///< Max Length of Unique ID
#define NVM_MAX_DIMMID_STR_LEN 7 ///< Max length of PMem module ID string
#define NVM_MANUFACTURER_LEN  2 ///< Number of bytes in the manufacturer ID
#define NVM_MANUFACTURERSTR_LEN 256 ///< Manufacturer string length
#define NVM_SERIAL_LEN  4 ///< Number of bytes in the serial number
#define NVM_SERIALSTR_LEN 11 ///< Serial number string length including '\0' and prefix "0x"
#define NVM_PASSPHRASE_LEN  32 ///< Length of security passphrase
#define NVM_MAX_DEVICE_SENSORS  11 ///< Maximum number of sensors
#define NVM_EVENT_MSG_LEN 1024 ///< Length of event message string
#define NVM_EVENT_ARG_LEN 1024 ///< Length of event argument string
#define NVM_MAX_EVENT_ARGS  3 ///< Maximum number of event arguments
#define NVM_PATH_LEN  PATH_MAX ///< Max length of file or directory path string (OS specific)
#define NVM_DEVICE_LOCATOR_LEN  128 ///< Length of the device locator string
#define NVM_BANK_LABEL_LEN  128 ///< Length of the bank label string
#define NVM_NAMESPACE_NAME_LEN  64 ///< Length of namespace friendly name string
#define NVM_NAMESPACE_PURPOSE_LEN 64 ///< Length of namespace purpose string
#define NVM_MAX_SOCKET_DIGIT_COUNT  4 ///< Maximum number of digits in a socket count
#define NVM_MEMORY_CONTROLLER_CHANNEL_COUNT 3 ///< expected number of channels per iMC
#define NVM_MAX_INTERLEAVE_SETS_PER_DIMM  2 ///< Max number of App Direct interleave sets per PMem module
#define NVM_MAX_POOLS_PER_NAMESPACE 128 ///< Maximum number of pools for a namespace
#define NVM_PART_NUM_LEN  21 ///< Length of device part number string
// TODO -guessing and interleave formats size. HSD-20363 should address this.
#define NVM_INTERLEAVE_FORMATS  32 ///< Maximum number of memory interleave formats
#define NVM_MAX_DEVICES_PER_POOL  128 ///< Maximum number of PMem modules that can be used in a pool
// This number of devices that can go on a socket may go up on future architectures
// so this is something to keep an eye on. 24 should be good for a while
#define NVM_MAX_DEVICES_PER_SOCKET  24 ///< Maximum number of PMem modules that can be on a socket
#define NVM_LOG_MESSAGE_LEN 2048 ///< Length of log message string
#define NVM_MAX_BLOCK_SIZES_PER_POOL  16
#define NVM_MAX_BLOCK_SIZES 16 ///< maximum number of block sizes supported by the driver
#define NVM_MAX_TOPO_SIZE 96 ///< Maximum number of PMem modules possible for a given memory topology
#define NVM_THRESHOLD_STR_LEN 1024 ///< Max threshold string value len
#define NVM_VOLATILE_POOL_SOCKET_ID -1 ///< Volatile pools are system wide and not tied to a socket
#define NVM_MAX_CONFIG_LINE_LEN 512 ///< Maximum line size for config data in a dump file
#define NVM_DIE_SPARES_MAX  4 ///< Maximum number of spare dies
#define NVM_COMMIT_ID_LEN 41
#define NVM_BUILD_CONFIGURATION_LEN 17
#define NVM_MAX_IFCS_PER_DIMM 9
#define NVM_REQUEST_MAX_AVAILABLE_BLOCK_COUNT 0
#define NVM_MIN_EAFD_FILES 1
#define NVM_MAX_EAFD_FILES 10
#define NVM_TRUE 1
#define NVM_FALSE 0
#define NVM_ARG0 0
#define NVM_ARG1 1
#define NVM_ARG2 2
#define MAX_IS_PER_DIMM 2

typedef size_t NVM_SIZE; ///</< String length size
typedef char NVM_INT8; ///< 8 bit signed integer
typedef signed short NVM_INT16; ///< 16 bit signed integer
typedef signed int  NVM_INT32; ///< 32 bit signed integer
typedef unsigned char NVM_BOOL; ///< 8 bit unsigned integer as a boolean
typedef unsigned char NVM_UINT8; ///< 8 bit unsigned integer
typedef unsigned short NVM_UINT16; ///< 16 bit unsigned integer
typedef unsigned int NVM_UINT32; ///< 32 bit unsigned integer
typedef unsigned long long NVM_UINT64; ///< 64 bit unsigned integer
typedef long long NVM_INT64; ///< 64 bit integer
typedef float NVM_REAL32; ///< 32 bit floating point number
typedef char NVM_VERSION[NVM_VERSION_LEN]; ///< Version number string
typedef char NVM_ERROR_DESCRIPTION[NVM_ERROR_LEN]; ///< Return code description
typedef char NVM_UID[NVM_MAX_UID_LEN]; ///< Unique ID
typedef char NVM_PASSPHRASE[NVM_PASSPHRASE_LEN]; ///< Security passphrase
typedef char NVM_EVENT_MSG[NVM_EVENT_MSG_LEN]; ///< Event message string
typedef char NVM_EVENT_ARG[NVM_EVENT_ARG_LEN]; ///< Event argument string
typedef char NVM_PATH[NVM_PATH_LEN]; ///< TFile or directory path
typedef unsigned char NVM_MANUFACTURER[NVM_MANUFACTURER_LEN]; ///< Manufacturer identifier
typedef unsigned char NVM_SERIAL_NUMBER[NVM_SERIAL_LEN]; ///< Serial Number
typedef char NVM_NAMESPACE_NAME[NVM_NAMESPACE_NAME_LEN]; ///< Namespace name
typedef char NVM_PREFERENCE_KEY[NVM_THRESHOLD_STR_LEN]; ///< Config value property name
typedef char NVM_PREFERENCE_VALUE[NVM_THRESHOLD_STR_LEN]; ///< Config value property value
typedef char NVM_LOG_MSG[NVM_LOG_MESSAGE_LEN]; ///< Event message string

typedef union
{
  struct device_handle_parts
  {
    NVM_UINT32 mem_channel_dimm_num:4;
    NVM_UINT32 mem_channel_id:4;
    NVM_UINT32 memory_controller_id:4;
    NVM_UINT32 socket_id:4;
    NVM_UINT32 node_controller_id:12;
    NVM_UINT32 rsvd:4;
  } parts;
  NVM_UINT32 handle;
} NVM_NFIT_DEVICE_HANDLE;

/**
 * Type of region.
 */
enum region_type
{
  REGION_TYPE_UNKNOWN = 0,
  REGION_TYPE_PERSISTENT = 1, ///< REGION type is non-mirrored App Direct.
  REGION_TYPE_VOLATILE = 2, ///< Volatile.
  REGION_TYPE_PERSISTENT_MIRROR = 3, ///< Persistent.
};

/**
 * Rolled-up health of the underlying PMem modules from which the REGION is created.
 */
enum region_health
{
  REGION_HEALTH_NORMAL  = 1, ///< All underlying PMem module Persistent memory capacity is available.
  REGION_HEALTH_ERROR   = 2, ///< There is an issue with some or all of the underlying
                             ///< PMem module capacity.
  REGION_HEALTH_UNKNOWN = 3, ///< The REGION health cannot be determined.

  REGION_HEALTH_PENDING = 4, ///< A new memory allocation goal has been created but not applied.

  REGION_HEALTH_LOCKED  = 5  ///< One or more of the underlying PMem modules are locked.
};

/**
 * Health of an individual interleave set.
 */
enum interleave_set_health
{
  INTERLEAVE_HEALTH_UNKNOWN  = 0,  ///< Health cannot be determined.
  INTERLEAVE_HEALTH_NORMAL   = 1,  ///< Available and underlying PMem modules have good health.
  INTERLEAVE_HEALTH_DEGRADED = 2,  ///< In danger of failure, may have degraded performance.
  INTERLEAVE_HEALTH_FAILED   = 3   ///< Interleave set has failed and is unavailable.
};

/**
 * Security related definition of interleave set or namespace.
 */
enum encryption_status
{
  NVM_ENCRYPTION_OFF = 0,
  NVM_ENCRYPTION_ON = 1,
  NVM_ENCRYPTION_IGNORE = 2
};

/**
 * Erase capable definition of interleave set or namespace.
 */
enum erase_capable_status
{
  NVM_ERASE_CAPABLE_FALSE = 0,
  NVM_ERASE_CAPABLE_TRUE = 1,
  NVM_ERASE_CAPABLE_IGNORE = 2
};

/**
 * Details about a specific interleave format supported by memory
 */
enum interleave_size
{
  INTERLEAVE_SIZE_NONE = 0x00,
  INTERLEAVE_SIZE_64B  = 0x01,
  INTERLEAVE_SIZE_128B = 0x02,
  INTERLEAVE_SIZE_256B = 0x04,
  INTERLEAVE_SIZE_4KB  = 0x40,
  INTERLEAVE_SIZE_1GB  = 0x80
};

enum interleave_ways
{
  INTERLEAVE_WAYS_0  = 0x00,
  INTERLEAVE_WAYS_1  = 0x01,
  INTERLEAVE_WAYS_2  = 0x02,
  INTERLEAVE_WAYS_3  = 0x04,
  INTERLEAVE_WAYS_4  = 0x08,
  INTERLEAVE_WAYS_6  = 0x10,
  INTERLEAVE_WAYS_8  = 0x20,
  INTERLEAVE_WAYS_12 = 0x40,
  INTERLEAVE_WAYS_16 = 0x80,
  INTERLEAVE_WAYS_24 = 0x100
};

enum interleave_type
{
  INTERLEAVE_TYPE_DEFAULT = 0,
  INTERLEAVE_TYPE_INTERLEAVED = 1,
  INTERLEAVE_TYPE_NOT_INTERLEAVED = 2,
  INTERLEAVE_TYPE_MIRRORED  = 3
};

struct interleave_format
{
  NVM_BOOL recommended; ///< is this format a recommended format
  enum interleave_size channel; ///< channel interleave of this format
  enum interleave_size imc; ///< memory controller interleave of this format
  enum interleave_ways ways; ///< number of ways for this format
};

enum acpi_get_event_result
{
  ACPI_EVENT_SIGNALLED_RESULT = 0,
  ACPI_EVENT_TIMED_OUT_RESULT,
  ACPI_EVENT_UNKNOWN_RESULT
};

enum acpi_event_state
{
  ACPI_EVENT_SIGNALLED = 0,
  ACPI_EVENT_NOT_SIGNALLED,
  ACPI_EVENT_UNKNOWN
};

enum acpi_event_type
{
  ACPI_SMART_HEALTH = 0,
  ACPI_UNCORRECTABLE
};

#define MAX_ERROR_LOG_SZ 64

/**
 * Describes an error log.
 */
typedef struct _ERROR_LOG {
  NVM_UINT16 DimmID;                        ///< The PMem module ID
  NVM_UINT64 SystemTimestamp;               ///< Unix epoch time of log entry
  NVM_UINT8 ErrorType;                      ///< 0: Thermal, 1: Media
  NVM_UINT8 OutputData[MAX_ERROR_LOG_SZ];   ///< Either THERMAL_ERROR_LOG or MEDIA_ERROR_LOG (see ErrorType)
} ERROR_LOG;

/**
 * Describes a thermal error log.
 */
typedef struct _THERMAL_ERROR_LOG_PER_DIMM {
  NVM_INT16   Temperature;        ///< In celsius
  NVM_UINT8   Reported;           ///< Temperature being reported
  NVM_UINT8   Type;               ///< Which device the temperature is for
  NVM_UINT16  SequenceNum;        ///< Sequence number
  NVM_UINT8   Reserved[1];        ///< Reserved
} THERMAL_ERROR_LOG;

/**
 * Describes a media error log.
 */
typedef struct _MEDIA_ERROR_LOG_PER_DIMM {
  NVM_UINT64  Dpa;                ///< Specifies DPA address of error
  NVM_UINT64  Pda;                ///< Specifies PDA address of the failure
  NVM_UINT8   Range;              ///< Specifies the length in address space of this error.
  NVM_UINT8   ErrorType;          ///< Indicates what kind of error was logged.
  NVM_UINT8   PdaValid;           ///< Indicates the PDA address is valid.
  NVM_UINT8   DpaValid;           ///< Indicates the DPA address is valid.
  NVM_UINT8   Interrupt;          ///< Indicates this error generated an interrupt packet
  NVM_UINT8   Viral;              ///< Indicates Viral was signaled for this error
  NVM_UINT8   TransactionType;    ///< Transaction tpye
  NVM_UINT16  SequenceNum;        ///< Sequence number
  NVM_UINT8   Reserved[2];        ///< Reserved
} MEDIA_ERROR_LOG;


#endif /* NVM_TYPES_H_ */
