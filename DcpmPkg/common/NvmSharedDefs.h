/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/**
 * @file NvmSharedDefs.h
 * @brief Definition of the NVM status codes
 */

#ifndef _NVM_SHARED_DEFS_H_
#define _NVM_SHARED_DEFS_H_

 /**
 * @brief NVM_API return codes
 */
typedef enum _NvmStatusCode {
  NVM_SUCCESS                                       = 0,    ///< Success
  NVM_SUCCESS_FW_RESET_REQUIRED                     = 1,    ///< Success, but FW reset required
  NVM_ERR_OPERATION_NOT_STARTED                     = 2,    ///< Error: Operation not started
  NVM_ERR_OPERATION_FAILED                          = 3,    ///< Error: Operation failed
  NVM_ERR_FORCE_REQUIRED                            = 4,    ///< Error: Force parameter required
  NVM_ERR_INVALID_PARAMETER                         = 5,    ///< Error: Invalid paramter
  NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU         = 9,    ///< Error: Commnand not supported by this SKU

  NVM_ERR_DIMM_NOT_FOUND                            = 11,   ///< Error: DIMM not found
  NVM_ERR_DIMM_ID_DUPLICATED                        = 12,   ///< Error: DIMM ID duplicated
  NVM_ERR_SOCKET_ID_NOT_VALID                       = 13,   ///< Error: Socket ID not valid
  NVM_ERR_SOCKET_ID_DUPLICATED                      = 15,   ///< Error: Socket ID duplicated
  NVM_ERR_CONFIG_NOT_SUPPORTED_BY_CURRENT_SKU       = 16,   ///< Error: Config Not supproted by current SKU
  NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND                 = 17,   ///< Error: Manageable DIMM not found

  NVM_ERR_PASSPHRASE_NOT_PROVIDED                   = 30,   ///< Error: Passphrase not provided
  NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED               = 31,   ///< Error: New passphrase not provided
  NVM_ERR_PASSPHRASES_DO_NOT_MATCH                  = 32,   ///< Error: Passphrases do not match
  NVM_ERR_PASSPHRASE_TOO_LONG                       = 34,   ///< Error: Passphrase too long
  NVM_ERR_ENABLE_SECURITY_NOT_ALLOWED               = 35,   ///< Error: Enable security not allowed
  NVM_ERR_CREATE_GOAL_NOT_ALLOWED                   = 36,   ///< Error: Create goal not allowed
  NVM_ERR_INVALID_SECURITY_STATE                    = 37,   ///< Error: Invalid security state
  NVM_ERR_INVALID_SECURITY_OPERATION                = 38,   ///< Error: Invalid security operation
  NVM_ERR_UNABLE_TO_GET_SECURITY_STATE              = 39,   ///< Error: Unable to get security state
  NVM_ERR_INCONSISTENT_SECURITY_STATE               = 40,   ///< Error: Inconsistent security state
  NVM_ERR_INVALID_PASSPHRASE                        = 41,   ///< Error: Invalid passphrase
  NVM_ERR_SECURITY_USER_PP_COUNT_EXPIRED            = 42,   ///< Error: Security count for user passphrase expired
  NVM_ERR_RECOVERY_ACCESS_NOT_ENABLED               = 43,   ///< Error: Recovery access not enabled
  NVM_ERR_SECURE_ERASE_NAMESPACE_EXISTS             = 44,   ///< Error: Namespace exists - cannot execute request
  NVM_ERR_SECURITY_MASTER_PP_COUNT_EXPIRED          = 45,   ///< Error: Security count for master passphrase expired

  NVM_ERR_IMAGE_FILE_NOT_COMPATIBLE_TO_CTLR_STEPPING     = 59,   ///< Error: Image not compatible with this DCPMM
  NVM_ERR_FILENAME_NOT_PROVIDED                     = 60,   ///< Error: Filename not provided
  NVM_SUCCESS_IMAGE_EXAMINE_OK                      = 61,   ///< Success: Image OK
  NVM_ERR_IMAGE_FILE_NOT_VALID                      = 62,   ///< Error: Image file not valid
  NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION               = 63,   ///< Error: Image examine lower version invalid
  NVM_ERR_IMAGE_EXAMINE_INVALID                     = 64,   ///< Error: Image invalid
  NVM_ERR_FIRMWARE_API_NOT_VALID                    = 65,   ///< Error: Firmware API version not valid
  NVM_ERR_FIRMWARE_VERSION_NOT_VALID                = 66,   ///< Error: Firmware version not valid
  NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED           = 67,   ///< Error: Firmware version to low. Force option required
  NVM_ERR_FIRMWARE_ALREADY_LOADED                   = 68,   ///< Error: Firmware already loaded
  NVM_ERR_FIRMWARE_FAILED_TO_STAGE                  = 69,   ///< Error: Firmware failed to stage

  NVM_ERR_SENSOR_NOT_VALID                          = 70,   ///< Error: Sensor not valid
  NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE            = 71,   ///< Error: Sensor media temperature out of range
  NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE       = 72,   ///< Error: Sensor controller temperature out of range
  NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE              = 73,   ///< Error: Capacity out of range
  NVM_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE        = 74,   ///< Error: Sensor invalid value

  NVM_ERR_MEDIA_DISABLED                            = 90,   ///< Error: Media disabled

  NVM_WARN_GOAL_CREATION_SECURITY_UNLOCKED              = 97,   ///< Warning: Goal will not be applied unless security is disabled prior to UEFI FW provisioning!
  NVM_WARN_REGION_MAX_PM_INTERLEAVE_SETS_EXCEEDED       = 98,   ///< Warning: Interleave Sets cannot exceed MaxPMInterleaveSetsPerDie per Socket due to platform limitation
  NVM_WARN_REGION_MAX_AD_PM_INTERLEAVE_SETS_EXCEEDED    = 99,   ///< Warning: Interleave Sets cannot exceed MaxPMInterleaveSetsPerDie per Socket due to platform limitation for AD Interleaved mode
  NVM_WARN_REGION_MAX_AD_NI_PM_INTERLEAVE_SETS_EXCEEDED = 100,  ///< Warning: Interleave Sets cannot exceed MaxPMInterleaveSetsPerDie per Socket due to platform limitation for AD Non-Interleaved mode
  NVM_WARN_REGION_AD_NI_PM_INTERLEAVE_SETS_REDUCED      = 101,  ///< Warning: Reducing the number of AppDirect2 (AD non-interleaved) regions created in AD interlaeved mode request when MaxPMInterleaveSetsPerDie limit exceeeded
  NVM_ERR_REGION_MAX_PM_INTERLEAVE_SETS_EXCEEDED        = 102,  ///< Error: Interleave Sets cannot exceed MaxPMInterleaveSetsPerDie per Socket due to platform limitation (error if existing regions + new region goals for specific DIMMs greater then MaxPMInterleaveSetsPerDie limit)
  NVM_WARN_2LM_MODE_OFF                             = 103,  ///< Error: MemoryMode off
  NVM_WARN_IMC_DDR_PMM_NOT_PAIRED                   = 104,  ///< Error: PMM and DDR4 missing on iMC
  NVM_ERR_PCD_BAD_DEVICE_CONFIG                     = 105,  ///< Error: Bad PCD config
  NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM      = 106,  ///< Error: Goal config affects unspecified DIMM
  NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM      = 107,  ///< Error: Current config affects unspecified DIMM
  NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM = 108,  ///< Error: Current and goal config affects unspecified DIMM
  NVM_ERR_REGION_CONF_APPLYING_FAILED               = 109,  ///< Error: Failed to apply goal
  NVM_ERR_REGION_CONF_UNSUPPORTED_CONFIG            = 110,  ///< Error: Unsupported config

  NVM_ERR_REGION_NOT_FOUND                          = 111,  ///< Error: Region not found
  NVM_ERR_PLATFORM_NOT_SUPPORT_MANAGEMENT_SOFT      = 112,  ///< Error: Platform does not support PMM software
  NVM_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE             = 113,  ///< Error: Platform does not support MemoryMode
  NVM_ERR_PLATFORM_NOT_SUPPORT_PM_MODE              = 114,  ///< Error: Platform does not support persistent memory mode
  NVM_ERR_REGION_CURR_CONF_EXISTS                   = 115,  ///< Error: Current config exists
  NVM_ERR_REGION_SIZE_TOO_SMALL_FOR_INT_SET_ALIGNMENT = 116, ///< Error: Region size too small for interleave set alignment
  NVM_ERR_PLATFORM_NOT_SUPPORT_SPECIFIED_INT_SIZES  = 117,   ///< Error: Platform does not support specified interleave sizes
  NVM_ERR_PLATFORM_NOT_SUPPORT_DEFAULT_INT_SIZES    = 118,   ///< Error: Platform does not support default interleave sizes
  NVM_ERR_REGION_NOT_HEALTHY                          = 119, ///< Error: Region not healthy
  NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_BLOCK_NAMESPACE = 120, ///< Error: Not enough space for block namespace
  NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_PM_NAMESPACE    = 121, ///< Error: Not enough space for persistent namesapce
  NVM_ERR_REGION_NO_GOAL_EXISTS_ON_DIMM               = 122, ///< Error: Goal does not exist on DIMM
  NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS  = 123,   ///< Error: Reserve DIMM requires at least 2 DIMMs
  NVM_ERR_REGION_GOAL_NAMESPACE_EXISTS                = 124, ///< Error: Namespace exists
  NVM_ERR_REGION_REMAINING_SIZE_NOT_IN_LAST_PROPERTY  = 125, ///< Error: Remaining size not in last property
  NVM_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS     = 126,  ///< Error: Persistent memory must be applied to all DIMMs
  NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU        = 127,  ///< Warning: Mapped memory reduced due to CPU SKU limit
  NVM_ERR_REGION_GOAL_AUTO_PROV_ENABLED             = 128,  ///< Error: Automatic provision enabled
  NVM_ERR_CREATE_NAMESPACE_NOT_ALLOWED              = 129,  ///< Error: Create namespace not allowed

  NVM_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED          = 130,  ///< Error: Failed to open file with write mode
  NVM_ERR_DUMP_NO_CONFIGURED_DIMMS                  = 131,  ///< Error: No configured DIMMs
  NVM_ERR_DUMP_FILE_OPERATION_FAILED                = 132,  ///< Error: File IO failed

  NVM_ERR_LOAD_VERSION                              = 140,  ///< Error: Invalid version
  NVM_ERR_LOAD_INVALID_DATA_IN_FILE                 = 141,  ///< Error: Invalid data in file
  NVM_ERR_LOAD_IMPROPER_CONFIG_IN_FILE              = 142,  ///< Error: Improper config in file
  NVM_ERR_LOAD_DIMM_COUNT_MISMATCH                  = 148,  ///< Error: Mismatch in DIMMs

  NVM_ERR_DIMM_SKU_MODE_MISMATCH                    = 151,  ///< Error: SKU mode mismatch
  NVM_ERR_DIMM_SKU_SECURITY_MISMATCH                = 152,  ///< Error: SKU security mismatch

  NVM_ERR_NONE_DIMM_FULFILLS_CRITERIA               = 168,  ///< Error: No DIMM matches request
  NVM_ERR_UNSUPPORTED_BLOCK_SIZE                    = 171,  ///< Error: Unsupported block size
  NVM_ERR_INVALID_NAMESPACE_CAPACITY                = 174,  ///< Error: Invalid namespace capacity
  NVM_ERR_NOT_ENOUGH_FREE_SPACE                     = 175,  ///< Error: Not enough free space
  NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN            = 176,  ///< Error: Namespace config broken
  NVM_ERR_NAMESPACE_DOES_NOT_EXIST                  = 177,  ///< Error: Namespace does not exist
  NVM_ERR_NAMESPACE_COULD_NOT_UNINSTALL             = 178,  ///< Error: Namespace could not uninstall
  NVM_ERR_NAMESPACE_COULD_NOT_INSTALL               = 179,  ///< Error: Namespace could not install
  NVM_ERR_NAMESPACE_READ_ONLY                       = 180,  ///< Error: Namespace read only
  NVM_ERR_PLATFORM_NOT_SUPPORT_BLOCK_MODE           = 181,  ///< Error: Platform does not support block mode
  NVM_WARN_BLOCK_MODE_DISABLED                      = 182,  ///< Warning: Block mode disabled
  NVM_ERR_NAMESPACE_TOO_SMALL_FOR_BTT               = 183,  ///< Error: Namespace too small for BTT
  NVM_ERR_NOT_ENOUGH_FREE_SPACE_BTT                 = 184,  ///< Error: Not enough free space for BTT
  NVM_ERR_FAILED_TO_UPDATE_BTT                      = 185,  ///< Error: Failed to update BTT
  NVM_ERR_BADALIGNMENT                              = 186,  ///< Error: Bad alignment
  NVM_ERR_RENAME_NAMESPACE_NOT_SUPPORTED            = 187,  ///< Error: Rename namespace not supported
  NVM_ERR_FAILED_TO_INIT_NS_LABELS                  = 188,  ///< Error: Failed to initialize namespace labels

  NVM_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE             = 195,  ///< Error: Failed to get log size
  NVM_ERR_FW_DBG_SET_LOG_LEVEL_FAILED               = 196,  ///< Error: Failed to set log level
  NVM_INFO_FW_DBG_LOG_NO_LOGS_TO_FETCH              = 197,  ///< Error: No debug logs

  NVM_ERR_FAILED_TO_FETCH_ERROR_LOG                 = 200,  ///< Error: Failed to fetch error log
  NVM_SUCCESS_NO_ERROR_LOG_ENTRY                    = 201,  ///< Info: Request to retrieve entry was successful, however log was empty
  NVM_ERR_SMART_FAILED_TO_GET_SMART_INFO            = 220,  ///< Error: Failed to get smart info
  NVM_WARN_SMART_NONCRITICAL_HEALTH_ISSUE           = 221,  ///< Warning: Non-critical health issue
  NVM_ERR_SMART_CRITICAL_HEALTH_ISSUE               = 222,  ///< Error: Critical health issue
  NVM_ERR_SMART_FATAL_HEALTH_ISSUE                  = 223,  ///< Error: Fatal health issue
  NVM_ERR_SMART_READ_ONLY_HEALTH_ISSUE              = 224,  ///< Error: Read-only health issue
  NVM_ERR_SMART_UNKNOWN_HEALTH_ISSUE                = 225,  ///< Error: Unknown health issue

  NVM_ERR_FW_SET_OPTIONAL_DATA_POLICY_FAILED        = 230,  ///< Error: Set data policy failed
  NVM_ERR_INVALID_OPTIONAL_DATA_POLICY_STATE        = 231,  ///< Error: Invalid data policy state

  NVM_ERR_FAILED_TO_GET_DIMM_INFO                   = 235,  ///< Error: Failed to get DIMM info

  NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS              = 240,  ///< Error: Failed to get DIMM registers
  NVM_ERR_SMBIOS_DIMM_ENTRY_NOT_FOUND_IN_NFIT       = 241,  ///< Error:  SMBIOS entry not found in NFIT

  NVM_OPERATION_IN_PROGRESS                         = 250,  ///< Error: Operation in progress

  NVM_ERR_GET_PCD_FAILED                            = 260,  ///< Error: Get PCD failed

  NVM_ERR_ARS_IN_PROGRESS                           = 261,  ///< Error: ARS in progress
  NVM_ERR_APPDIRECT_IN_SYSTEM                       = 262,  ///< Error: AppDirect in system
  NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU      = 263,  ///< Error: Operation not supported by mixed SKUs

  NVM_ERR_FW_GET_FA_UNSUPPORTED                     = 264,  ///< Error:
  NVM_ERR_FW_GET_FA_DATA_FAILED                     = 265,  ///< Error:

  NVM_ERR_API_NOT_SUPPORTED                         = 266,  ///< Error: API not supported
  NVM_ERR_UNKNOWN                                   = 267,  ///< Error: Unkown
  NVM_ERR_INVALID_PERMISSIONS                       = 268,  ///< Error: Invalid permissions
  NVM_ERR_BAD_DEVICE                                = 269,  ///< Error: Bad device
  NVM_ERR_BUSY_DEVICE                               = 270,  ///< Error: Busy device
  NVM_ERR_GENERAL_OS_DRIVER_FAILURE                 = 271,  ///< Error: General OS driver failure
  NVM_ERR_NO_MEM                                    = 272,  ///< Error: No memory
  NVM_ERR_BAD_SIZE                                  = 273,  ///< Error: Bad size
  NVM_ERR_TIMEOUT                                   = 274,  ///< Error: Timeout
  NVM_ERR_DATA_TRANSFER                             = 275,  ///< Error: Data transfer failed
  NVM_ERR_GENERAL_DEV_FAILURE                       = 276,  ///< Error: General device failure
  NVM_ERR_BAD_FW                                    = 277,  ///< Error: Bad FW
  NVM_ERR_DRIVER_FAILED                             = 288,  ///< Error: Driver failed
  NVM_ERR_DRIVERFAILED                              = 288,  ///< Error: Obsolete: Use NVM_ERR_DRIVER_FAILED
  NVM_ERR_INVALIDPARAMETER                          = 289,  ///< Error: Obsolete: Use NVM_ERR_INVALID_PARAMETER
  NVM_ERR_OPERATION_NOT_SUPPORTED                   = 290,  ///< Error: Operation not supported
  NVM_ERR_RETRY_SUGGESTED                           = 291,  ///< Error: Retry suggested

  NVM_ERR_SPD_NOT_ACCESSIBLE                        = 300,  ///< Error: SPD not accessible
  NVM_ERR_INCOMPATIBLE_HARDWARE_REVISION            = 301,  ///< Error: Incompatible hardware revision

  NVM_SUCCESS_NO_EVENT_FOUND                        = 302,  ///< Error: No events found in the event log
  NVM_ERR_FILE_NOT_FOUND                            = 303,  ///< Error: No events found in the event log
  NVM_ERR_OVERWRITE_DIMM_IN_PROGRESS                = 304,  ///< Error: No events found in the event log
  NVM_ERR_FWUPDATE_IN_PROGRESS                      = 305,  ///< Error: No events found in the event log
  NVM_ERR_UNKNOWN_LONG_OP_IN_PROGRESS               = 306,  ///< Error: No events found in the event log
  NVM_ERR_LONG_OP_ABORTED_OR_REVISION_FAILURE       = 307,  ///< Error: long op was aborted
  NVM_ERR_FW_UPDATE_AUTH_FAILURE                    = 308,  ///< Error: fw image authentication failed
  NVM_ERR_UNSUPPORTED_COMMAND                       = 309,  ///< Error: unsupported command
  NVM_ERR_DEVICE_ERROR                              = 310,  ///< Error: device error
  NVM_ERR_TRANSFER_ERROR                            = 311,  ///< Error: transfer error
  NVM_ERR_UNABLE_TO_STAGE_NO_LONGOP                 = 312,  ///< Error: the FW was unable to stage and no long op code was recoverable
  NVM_ERR_LONG_OP_UNKNOWN                           = 313,  ///< Error: a long operation code is unknown
  NVM_ERR_PCD_DELETE_DENIED                         = 314,  ///< Error: API not supported
  NVM_ERR_MIXED_GENERATIONS_NOT_SUPPORTED           = 315,  ///< Error: Operation does not work when dimm that are different generations
  NVM_LAST_STATUS_VALUE
} NvmStatusCode;

typedef struct _PMON_REGISTERS {
  /**
  This will specify whether or not to return the extra smart data along with the PMON
  Counter data.
  - 0x0 - No Smart Data DDRT or Media.
  - 0x1 - DDRT Data only to be returned.
  - 0x2 - Media Data only to be returned.
  - 0x3 - DDRT & Media Data to be returned.
  - All other values reserved.
  **/
  unsigned char       SmartDataMask;
  unsigned char       Reserved1[3];
  /**
  This will specify which group that is currently enabled. If no groups are enabled Group
  F will be returned.
  **/
  unsigned char       GroupEnabled;
  unsigned char       Reserved2[19];
  unsigned int        PMON4Counter;
  unsigned int        PMON5Counter;
  unsigned char       Reserved3[4];
  unsigned int        PMON7Counter;
  unsigned int        PMON8Counter;
  unsigned int        PMON9Counter;
  unsigned char       Reserved4[16];
  unsigned int        PMON14Counter;
  unsigned char       Reserved5[4];
  /**
  DDRT Reads for current power cycle
  **/
  unsigned long long  DDRTRD;
  /**
  DDRT Writes for current power cycle
  **/
  unsigned long long  DDRTWR;
  /**
  Media Reads for current power cycle
  **/
  unsigned long long  MERD;
  /**
  Media Writes for current power cycle
  **/
  unsigned long long  MEWR;
  /**
  Current Media temp
  **/
  unsigned short      MTP;
  /**
  Current Controller temp
  **/
  unsigned short      CTP;
  unsigned char       Reserved[20];
}PMON_REGISTERS;


#endif /** _NVM_SHARED_DEFS_H_ **/
