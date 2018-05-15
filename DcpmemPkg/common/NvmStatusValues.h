/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * @file NvmStatusValues.h
 * @brief Definition of the NVM status codes
 */

#ifndef _NVM_STATUS_VALUES_H_
#define _NVM_STATUS_VALUES_H_

 
typedef enum _NvmStatusCode {
  NVM_SUCCESS                                       = 0,
  NVM_SUCCESS_FW_RESET_REQUIRED                     = 1,
  NVM_ERR_OPERATION_NOT_STARTED                     = 2,
  NVM_ERR_OPERATION_FAILED                          = 3,
  NVM_ERR_FORCE_REQUIRED                            = 4,
  NVM_ERR_INVALID_PARAMETER                         = 5,
  NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU         = 9,

  NVM_ERR_DIMM_NOT_FOUND                            = 11,
  NVM_ERR_DIMM_ID_DUPLICATED                        = 12,
  NVM_ERR_SOCKET_ID_NOT_VALID                       = 13,
  NVM_ERR_SOCKET_ID_DUPLICATED                      = 15,
  NVM_ERR_CONFIG_NOT_SUPPORTED_BY_CURRENT_SKU       = 16,
  NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND                 = 17,

  NVM_ERR_PASSPHRASE_NOT_PROVIDED                   = 30,
  NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED               = 31,
  NVM_ERR_PASSPHRASES_DO_NOT_MATCH                  = 32,
  NVM_ERR_PASSPHRASE_TOO_LONG                       = 34,
  NVM_ERR_ENABLE_SECURITY_NOT_ALLOWED               = 35,
  NVM_ERR_CREATE_GOAL_NOT_ALLOWED                   = 36,
  NVM_ERR_INVALID_SECURITY_STATE                    = 37,
  NVM_ERR_INVALID_SECURITY_OPERATION                = 38,
  NVM_ERR_UNABLE_TO_GET_SECURITY_STATE              = 39,
  NVM_ERR_INCONSISTENT_SECURITY_STATE               = 40,
  NVM_ERR_INVALID_PASSPHRASE                        = 41,
  NVM_ERR_SECURITY_COUNT_EXPIRED                    = 42,
  NVM_ERR_RECOVERY_ACCESS_NOT_ENABLED               = 43,
  NVM_ERR_SECURE_ERASE_NAMESPACE_EXISTS             = 44,

  NVM_ERR_FILENAME_NOT_PROVIDED                     = 60,
  NVM_SUCCESS_IMAGE_EXAMINE_OK                      = 61,
  NVM_ERR_IMAGE_FILE_NOT_VALID                      = 62,
  NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION               = 63,
  NVM_ERR_IMAGE_EXAMINE_INVALID                     = 64,
  NVM_ERR_FIRMWARE_API_NOT_VALID                    = 65,
  NVM_ERR_FIRMWARE_VERSION_NOT_VALID                = 66,
  NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED           = 67,
  NVM_ERR_FIRMWARE_ALREADY_LOADED                   = 68,
  NVM_ERR_FIRMWARE_FAILED_TO_STAGE                  = 69,

  NVM_ERR_SENSOR_NOT_VALID                          = 70,
  NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE            = 71,
  NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE       = 72,
  NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE              = 73,
  NVM_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE        = 74,

  NVM_ERR_MEDIA_DISABLED                            = 90,
  NVM_ERR_AIT_DRAM_NOT_READY                        = 91,
  NVM_ERR_MEDIA_INTERFACE_ENGINE_STALLED            = 92,

  NVM_WARN_2LM_MODE_OFF                             = 103,
  NVM_WARN_IMC_DDR_AEP_NOT_PAIRED                   = 104,
  NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM        = 106,
  NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM        = 107,
  NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM   = 108,
  NVM_ERR_REGION_CONF_APPLYING_FAILED                 = 109,
  NVM_ERR_REGION_CONF_UNSUPPORTED_CONFIG              = 110,

  NVM_ERR_REGION_NOT_FOUND                          = 111,
  NVM_ERR_PLATFORM_NOT_SUPPORT_MANAGEMENT_SOFT      = 112,
  NVM_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE             = 113,
  NVM_ERR_PLATFORM_NOT_SUPPORT_PM_MODE              = 114,
  NVM_ERR_REGION_CURR_CONF_EXISTS                   = 115,
  NVM_ERR_REGION_SIZE_TOO_SMALL_FOR_INT_SET_ALIGNMENT = 116,
  NVM_ERR_PLATFORM_NOT_SUPPORT_SPECIFIED_INT_SIZES  = 117,
  NVM_ERR_PLATFORM_NOT_SUPPORT_DEFAULT_INT_SIZES    = 118,
  NVM_ERR_REGION_NOT_HEALTHY                          = 119,
  NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_BLOCK_NAMESPACE = 120,
  NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_PM_NAMESPACE    = 121,
  NVM_ERR_REGION_GOAL_NO_EXISTS_ON_DIMM               = 122,
  NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS  = 123,
  NVM_ERR_REGION_GOAL_NAMESPACE_EXISTS                = 124,
  NVM_ERR_REGION_REMAINING_SIZE_NOT_IN_LAST_PROPERTY  = 125,
  NVM_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS     = 126,
  NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU        = 127,

  NVM_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED          = 130,
  NVM_ERR_DUMP_NO_CONFIGURED_DIMMS                  = 131,
  NVM_ERR_DUMP_FILE_OPERATION_FAILED                = 132,

  NVM_ERR_LOAD_VERSION                              = 140,
  NVM_ERR_LOAD_INVALID_DATA_IN_FILE                 = 141,
  NVM_ERR_LOAD_IMPROPER_CONFIG_IN_FILE              = 142,
  NVM_ERR_LOAD_DIMM_COUNT_MISMATCH                  = 148,

  NVM_ERR_DIMM_SKU_DIE_SPARING_MISMATCH             = 150,
  NVM_ERR_DIMM_SKU_MODE_MISMATCH                    = 151,
  NVM_ERR_DIMM_SKU_SECURITY_MISMATCH                = 152,

  NVM_ERR_NONE_DIMM_FULFILLS_CRITERIA               = 168,
  NVM_ERR_UNSUPPORTED_BLOCK_SIZE                    = 171,
  NVM_ERR_INVALID_NAMESPACE_CAPACITY                = 174,
  NVM_ERR_NOT_ENOUGH_FREE_SPACE                     = 175,
  NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN            = 176,
  NVM_ERR_NAMESPACE_DOES_NOT_EXIST                  = 177,
  NVM_ERR_NAMESPACE_COULD_NOT_UNINSTALL             = 178,
  NVM_ERR_NAMESPACE_COULD_NOT_INSTALL               = 179,
  NVM_ERR_NAMESPACE_READ_ONLY                       = 180,
  NVM_ERR_PLATFORM_NOT_SUPPORT_BLOCK_MODE           = 181,
  NVM_WARN_BLOCK_MODE_DISABLED                      = 182,
  NVM_ERR_NAMESPACE_TOO_SMALL_FOR_BTT               = 183,
  NVM_ERR_NOT_ENOUGH_FREE_SPACE_BTT                 = 184,
  NVM_ERR_FAILED_TO_UPDATE_BTT                      = 185,
  NVM_ERR_BADALIGNMENT                              = 186,
  NVM_ERR_RENAME_NAMESPACE_NOT_SUPPORTED            = 187,
  NVM_ERR_FAILED_TO_INIT_NS_LABELS                  = 188,

  NVM_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE             = 195,
  NVM_ERR_FW_DBG_SET_LOG_LEVEL_FAILED               = 196,
  NVM_INFO_FW_DBG_LOG_NO_LOGS_TO_FETCH              = 197,

  NVM_ERR_FAILED_TO_FETCH_ERROR_LOG                 = 200,

  NVM_ERR_SMART_FAILED_TO_GET_SMART_INFO            = 220,
  NVM_WARN_SMART_NONCRITICAL_HEALTH_ISSUE           = 221,
  NVM_ERR_SMART_CRITICAL_HEALTH_ISSUE               = 222,
  NVM_ERR_SMART_FATAL_HEALTH_ISSUE                  = 223,
  NVM_ERR_SMART_READ_ONLY_HEALTH_ISSUE              = 224,
  NVM_ERR_SMART_UNKNOWN_HEALTH_ISSUE                = 225,

  NVM_ERR_FW_SET_OPTIONAL_DATA_POLICY_FAILED        = 230,
  NVM_ERR_INVALID_OPTIONAL_DATA_POLICY_STATE        = 231,

  NVM_ERR_FAILED_TO_GET_DIMM_INFO                   = 235,

  NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS              = 240,
  NVM_ERR_SMBIOS_DIMM_ENTRY_NOT_FOUND_IN_NFIT       = 241,

  NVM_OPERATION_IN_PROGRESS                         = 250,

  NVM_ERR_GET_PCD_FAILED                            = 260,

  NVM_ERR_ARS_IN_PROGRESS                           = 261,
  NVM_ERR_APPDIRECT_IN_SYSTEM                       = 262,
  NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU      = 263,

  NVM_ERR_FW_GET_FA_UNSUPPORTED                     = 264,
  NVM_ERR_FW_GET_FA_DATA_FAILED                     = 265,

  NVM_ERR_API_NOT_SUPPORTED                         = 266,
  NVM_ERR_UNKNOWN                                   = 267,
  NVM_ERR_INVALID_PERMISSIONS                       = 268,
  NVM_ERR_BAD_DEVICE                                = 269,
  NVM_ERR_BUSY_DEVICE                               = 270,
  NVM_ERR_GENERAL_OS_DRIVER_FAILURE                 = 271,
  NVM_ERR_NO_MEM                                    = 272,
  NVM_ERR_BAD_SIZE                                  = 273,
  NVM_ERR_TIMEOUT                                   = 274,
  NVM_ERR_DATA_TRANSFER                             = 275,
  NVM_ERR_GENERAL_DEV_FAILURE                       = 276,
  NVM_ERR_BAD_FW                                    = 277,
  NVM_ERR_DRIVERFAILED                              = 288,
  NVM_ERR_INVALIDPARAMETER                          = 289,
  NVM_ERR_OPERATION_NOT_SUPPORTED                   = 290,
  NVM_LAST_STATUS_VALUE
} NvmStatusCode;


#endif /** _NVM_STATUS_VALUES_H_ **/
