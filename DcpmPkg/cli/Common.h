/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <Base.h>
#include "CommandParser.h"
#include "NvmStatus.h"

extern OBJECT_STATUS gAllErrorNvmStatuses;
extern OBJECT_STATUS gAllWarningNvmStatuses;
extern EFI_GUID gNvmDimmConfigProtocolGuid;
extern EFI_GUID gNvmDimmPbrProtocolGuid;

typedef struct _CMD_DISPLAY_OPTIONS {
  BOOLEAN DisplayOptionSet;
  BOOLEAN AllOptionSet;
  CHAR16 *pDisplayValues;
}CMD_DISPLAY_OPTIONS;

/** common display options **/
#define SOCKET_ID_STR               L"SocketID"
#define DIE_ID_STR                  L"DieID"
#define DIMM_ID_STR                 L"DimmID"
#define TAG_ID_STR                  L"TagID"
#define EXIT_CODE_STR               L"RC"
#define CLI_ARGS_STR                L"Args"
#define TAG_STR                     L"Tag"
#define DDR_STR                     L"DDR"
#define DCPMM_STR                   L"DCPMM"
#define TOTAL_STR                   L"Total"
#define MEMORY_TYPE_STR             L"MemoryType"

/** common display values**/
#define NA_STR                      L"N/A"
#define DASH_STR                    L"-"

#define MAX_FILE_PATH_LEN           512
#define MAX_FILE_SYSTEM_STRUCT_SIZE 4096
#define MAX_SHELL_PROTOCOL_HANDLES  2

#define PROGRESS_EVENT_TIMEOUT    EFI_TIMER_PERIOD_SECONDS(1)
#define PRINT_PRIORITY            8

// FW log level string values
#define FW_LOG_LEVEL_DISABLED_STR   L"Disabled"
#define FW_LOG_LEVEL_ERROR_STR      L"Error"
#define FW_LOG_LEVEL_WARNING_STR    L"Warning"
#define FW_LOG_LEVEL_INFO_STR       L"Info"
#define FW_LOG_LEVEL_DEBUG_STR      L"Debug"
#define FW_LOG_LEVEL_UNKNOWN_STR    L"Unknown"


// Parser CLI messages
#define CLI_PARSER_DID_YOU_MEAN              L"Did you mean:"
#define CLI_PARSER_ERR_UNEXPECTED_TOKEN      L"Syntax Error: Invalid or unexpected token " FORMAT_STR L"."
#define CLI_PARSER_ERR_INVALID_COMMAND       L"Syntax Error: Invalid or unsupported command."
#define CLI_PARSER_ERR_INVALID_OPTION_VALUES L"Syntax Error: Invalid option values input."
#define CLI_PARSER_ERR_MUTUALLY_EXCLUSIVE_OPTIONS L"Syntax Error: Mutually exclusive options " FORMAT_STR L" and " FORMAT_STR L"."
#define CLI_PARSER_ERR_INVALID_TARGET_VALUES L"Syntax Error: Invalid target values input."
#define CLI_PARSER_ERR_VERB_EXPECTED         L"Syntax Error: First token must be a verb, '" FORMAT_STR L"' is not a supported verb."
#define CLI_PARSER_DETAILED_ERR_OPTION_VALUE_REQUIRED     L"The option " FORMAT_STR L" requires a value."
#define CLI_PARSER_DETAILED_ERR_OPTION_VALUE_UNEXPECTED   L"The option " FORMAT_STR L" does not accept a value."
#define CLI_PARSER_DETAILED_ERR_TARGET_VALUE_REQUIRED     L"The target " FORMAT_STR L" requires a value."
#define CLI_PARSER_DETAILED_ERR_TARGET_VALUE_UNEXPECTED   L"The target " FORMAT_STR L" does not accept a value."
#define CLI_PARSER_DETAILED_ERR_PROPERTY_VALUE_REQUIRED   L"The property " FORMAT_STR L" requires a value."
#define CLI_PARSER_DETAILED_ERR_PROPERTY_VALUE_UNEXPECTED L"The property " FORMAT_STR L" does not accept a value."
#define CLI_PARSER_DETAILED_ERR_OPTION_REQUIRED           L"Missing required option " FORMAT_STR L"."
#define CLI_PARSER_DETAILED_ERR_PROPERTY_REQUIRED         L"Missing required property " FORMAT_STR L"."

// Common CLI error messages defined in specification
#define CLI_ERR_OPENING_CONFIG_PROTOCOL            L"Error: Communication with the device driver failed."
#define CLI_ERR_FAILED_TO_FIND_PROTOCOL       L"Error: DCPMM_CONFIG2_PROTOCOL not found."
#define CLI_ERR_INVALID_REGION_ID             L"Error: The region identifier is not valid."
#define CLI_ERR_INVALID_NAMESPACE_ID          L"Error: The namespace identifier is not valid."
#define CLI_ERR_NO_DIMMS_ON_SOCKET            L"Error: There are no DIMMs on the specified socket(s)."
#define CLI_ERR_NO_SPECIFIED_DIMMS_ON_SPECIFIED_SOCKET            L"Error: None of the specified dimm(s) belong to the specified socket(s)."
#define CLI_ERR_INVALID_SOCKET_ID             L"Error: The socket identifier is not valid."
#define CLI_ERR_OUT_OF_MEMORY                 L"Error: There is not enough memory to complete the requested operation."
#define CLI_ERR_WRONG_FILE_PATH               L"Error: Wrong file path."
#define CLI_ERR_WRONG_FILE_DATA               L"Error: Wrong data in the file."
#define CLI_ERR_INTERNAL_ERROR                L"Error: Internal function error."
#define CLI_ERR_PROMPT_INVALID                L"Error: Invalid data input."
#define CLI_ERR_WRONG_DIAGNOSTIC_TARGETS      L"Error: Invalid diagnostics target, valid values are: " FORMAT_STR
#define CLI_ERR_WRONG_REGISTER                L"Error: Register not found"
#define CLI_ERR_INVALID_PASSPHRASE_FROM_FILE  L"Error: The file contains empty or bad formatted passphrases."
#define CLI_ERR_UNMANAGEABLE_DIMM             L"Error: The specified device is not manageable by the driver."
#define CLI_ERR_POPULATION_VIOLATION          L"Error: The specified device is in population violation."
#define CLI_ERR_REGION_TO_SOCKET_MAPPING      L"The specified region id might not exist on the specified Socket(s).\n"
#define CLI_ERR_PCD_CORRUPTED                 L"Error: Unable to complete operation due to existing PCD Configuration partition corruption. Use create -f -goal to override current PCD and create goal."
#define CLI_ERR_OPENING_PBR_PROTOCOL            L"Error: Communication with the device driver failed.  Failed to obtain PBR protocol."
#define CLI_ERR_NMFM_LOWER_VIOLATION          L"WARNING! The requested memory mode size for 2LM goal is below the recommended NM:FM limit of 1:%d"
#define CLI_ERR_NMFM_UPPER_VIOLATION          L"WARNING! The requested memory mode size for 2LM goal is above the recommended NM:FM limit of 1:%d"

#define CLI_WARNING_CLI_DRIVER_VERSION_MISMATCH               L"Warning: There is a CLI and Driver version mismatch. Behavior is undefined."

// Common CLI error messages defined in specification
#define CLI_ERR_NO_COMMAND                                    L"Syntax Error: No input."
#define CLI_ERR_INCOMPLETE_SYNTAX                             L"Syntax Error: Incomplete syntax."
#define CLI_ERR_UNSUPPORTED_COMMAND_SYNTAX                    L"Syntax Error: Invalid or unsupported command."
#define CLI_ERR_INCORRECT_VALUE_POISON_TYPE                   L"Syntax Error: Incorrect value for property PoisonType."
#define CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY                L"Syntax Error: Incorrect value for option -d|-display."
#define CLI_ERR_INCORRECT_VALUE_OPTION_UNITS                  L"Syntax Error: Incorrect value for option -units."
#define CLI_ERR_INCORRECT_VALUE_OPTION_RECOVER                L"Syntax Error: Incorrect value for option -recover."
#define CLI_ERR_INCORRECT_VALUE_TARGET_REGISTER               L"Syntax Error: Incorrect value for target -register."
#define CLI_ERR_INCORRECT_VALUE_TARGET_DIMM                   L"Syntax Error: Incorrect value for target -dimm."
#define CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET                 L"Syntax Error: Incorrect value for target -socket."
#define CLI_ERR_INCORRECT_VALUE_TARGET_NAMESPACE              L"Syntax Error: Incorrect value for target -namespace."
#define CLI_ERR_INCORRECT_VALUE_TARGET_REGION                 L"Syntax Error: Incorrect value for target -region."
#define CLI_ERR_INCORRECT_VALUE_TARGET_ERROR                  L"Syntax Error: Incorrect value for target -error."
#define CLI_ERR_INCORRECT_VALUE_TARGET_SMBIOS                 L"Syntax Error: Incorrect value for target -smbios."
#define CLI_ERR_INCORRECT_VALUE_TARGET_SENSOR                 L"Syntax Error: Incorrect value for target -sensor."
#define CLI_ERR_INCORRECT_VALUE_TARGET_PCD                    L"Syntax Error: Incorrect value for target -pcd."
#define CLI_ERR_INCORRECT_VALUE_TARGET_PERFORMANCE            L"Syntax Error: Incorrect value for target -performance."
#define CLI_ERR_INCORRECT_VALUE_TARGET_EVENT                  L"Syntax Error: Incorrect value for target -event."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_MEMORY_MODE          L"Syntax Error: Incorrect value for property MemoryMode."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_PERSISTENT_MEM_TYPE  L"Syntax Error: Incorrect value for property PersistentMemoryType."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_RESERVED             L"Syntax Error: Incorrect value for property Reserved."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_BLOCK_SIZE           L"Syntax Error: Incorrect value for property BlockSize."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_BLOCK_COUNT          L"Syntax Error: Incorrect value for property BlockCount."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_CAPACITY             L"Syntax Error: Incorrect value for property Capacity."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_NAME                 L"Syntax Error: Incorrect value for property Name."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_ERASE_CAPABLE        L"Syntax Error: Incorrect value for property EraseCapable."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_ENCRYPTION           L"Syntax Error: Incorrect value for property Encryption."
#define CLI_ERR_INCORRECT_PROPERTY_VALUE_MODE                 L"Syntax Error: Incorrect value for property Mode."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_RAW_CAPACITY         L"Syntax Error: Incorrect value for property Size."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_ERASE_TYPE           L"Syntax Error: Incorrect value for property EraseType."
#define CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY_AVG_PWR_REPORTING_TIME_CONSTANT_MULT L"Syntax Error: Incorrect value for property AveragePowerReportingTimeConstantMultiplier."
#define CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY_AVG_PWR_REPORTING_TIME_CONSTANT      L"Syntax Error: Incorrect value for property AveragePowerReportingTimeConstant."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_LEVEL                L"Syntax Error: Incorrect value for property Level."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT                L"Syntax Error: Incorrect value for property Count."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_CATEGORY             L"Syntax Error: Incorrect value for property Category."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_SEQ_NUM              L"Syntax Error: Incorrect value for property SequenceNumber."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_ALARM_THRESHOLD      L"Syntax Error: Incorrect value for property AlarmThreshold."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_ENABLED_STATE        L"Syntax Error: Incorrect value for property AlarmThreshold."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_NS_LABEL_VERSION     L"Syntax Error: Incorrect value for property NamespaceLabelVersion."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_CONFIG               L"Syntax Error: Incorrect value for property Config."
#define CLI_ERR_INCORRECT_VALUE_PROPERTY_CCONFIG              L"Syntax Error: Incorrect value for property Config."
#define CLI_ERR_INCORRECT_VALUE_TARGET_TOKEN_ID               L"Syntax Error: Incorrect value for target -tokens."
#define CLI_ERROR_POISON_TYPE_WITHOUT_ADDRESS                 L"Syntax Error: Poison type property should be followed by poison address."
#define CLI_ERROR_CLEAR_PROPERTY_NOT_COMBINED                 L"Syntax Error: Clear property should be given in combination with other error injection properties."
#define CLI_ERR_MISSING_VALUE_PROPERTY_TYPE                   L"Syntax Error: Type property not provided."
#define CLI_ERR_INCORRECT_VALUE_FOR_PROPERTY                  L" is not a valid setting for the property."
#define CLI_SYNTAX_ERROR                                      L" Syntax Error: "

#define CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER             L"Syntax Error: Options -a|-all and -d|-display can not be used together."
#define CLI_ERR_OPTIONS_EXAMINE_USED_TOGETHER                 L"Syntax Error: Options -x and -examine can not be used together."
#define CLI_ERR_OPTIONS_FORCE_USED_TOGETHER                   L"Syntax Error: Options -f and -force can not be used together."
#define CLI_ERR_VALUES_APPDIRECT_SIZE_USED_TOGETHER           L"Syntax Error: Option values AppDirectSize and AppDirect1Size can not be used together."
#define CLI_ERR_VALUES_APPDIRECT_INDECES_USED_TOGETHER        L"Syntax Error: Option values AppDirectIndex and AppDirect1Index can not be used together."
#define CLI_ERR_PROPERTIES_CAPACITY_BLOCKCOUNT_USED_TOGETHER  L"Syntax Error: Properties Capacity and BlockCount can not be used together."

#define CLI_ERR_PROPERTIES_MEMORYMODE_RESERVED_TOO_LARGE      L"Syntax Error: Properties MemoryMode and Reserved cannot sum greater than 100%%%%" //%%%% because format string is processed twice

#define CLI_INFO_NO_DIMMS                                     L"No DIMMs in the system."
#define CLI_INFO_NO_FUNCTIONAL_DIMMS                          L"No functional DIMMs in the system."
#define CLI_INFO_NO_REGIONS                                   L"There are no Regions defined in the system."
#define CLI_INFO_NO_MANAGEABLE_DIMMS                          L"No manageable DIMMs in the system."
#define CLI_INFO_NO_NON_FUNCTIONAL_DIMMS                      L"No non-functional DIMMs in the system."
#define CLI_INFO_SHOW_REGION                                  L"Show Region"
#define CLI_INFO_NO_NAMESPACES_DEFINED                        L"No Namespaces defined in the system."
#define CLI_INFO_SHOW_NAMESPACE                               L"Show Namespace"
#define CLI_INFO_SET_NAMESPACE                                L"Set Namespace"
#define CLI_INFO_DELETE_NAMESPACE                             L"Delete Namespace"
#define CLI_INFO_DUMP_DEBUG_LOG                               L"Dump Debug Log"
#define CLI_INFO_LOAD_GOAL                                    L"Load Goal"
#define CLI_INFO_LOAD_GOAL_CONFIRM_PROMPT                     L"Load the configuration goal from '" FORMAT_STR L"' which will delete existing data and provision the capacity of the DIMMs on the next reboot."
#define CLI_INFO_SHOW_REGISTER                                L"Show Register"

#define CLI_ERR_MASTER_PASSPHRASE_NOT_ENABLED                     L"Master Passphrase not enabled on specified dimms."
#define CLI_ERR_MISSING_PASSPHRASE_PROPERTY                       L"Syntax Error: Passphrase property not provided."
#define CLI_ERR_DEFAULT_OPTION_NOT_COMBINED                       L"Syntax Error: Default option should be given in combination with master option."
#define CLI_ERR_DEFAULT_OPTION_PASSPHRASE_PROPERTY_USED_TOGETHER  L"Syntax Error: Passphrase property and default option cannot be used together."

#define CLI_ERR_FORCE_REQUIRED                                    L"Error: This command requires force option."
#define CLI_ERR_INVALID_BLOCKSIZE_FOR_CAPACITY                    L"Error: Capacity property can only be used with 512 or 4096 bytes block size."
#define CLI_ERR_INVALID_NAMESPACE_CAPACITY                        L"Error: Invalid value for namespace capacity."
#define CLI_ERR_SOME_VALUES_NOT_SUPPORTED                         L"Error: One or more of the fields specified are not supported on all the DIMMs."
#define CLI_ERR_VERSION_RETRIEVE                                  L": Unable to retrieve version from FW image."
#define CLI_ERR_PRINTING_DIAGNOSTICS_RESULTS                      L"Error: Printing of diagnostics results failed."
#define CLI_INJECT_ERROR_FAILED                                   L"Error: Inject error command failed"
#define CLI_ERR_NOT_UTF16                                         L"Error: The file is not in UTF16 format, BOM header missing.\n"
#define CLI_ERR_EMPTY_FILE                                        L"Error: The file is empty.\n"
#define CLI_ERR_NO_SOCKET_SKU_SUPPORT                             L"Platform does not support socket SKU limits.\n"
#define CLI_ERR_SOCKET_NOT_FOUND                                  L"Socket not found. Invalid SocketID: %d\n"
#define CLI_ERR_CAPACITY_STRING                                   L"Error: Failed creating the capacity string."

#define CLI_INFO_LOAD_FW                                      L"Load FW"
#define CLI_INFO_LOAD_RECOVER_FW                              L"Load recovery FW"
#define CLI_INFO_LOAD_RECOVER_INVALID_DIMM                    L"The specified dimm does not exist or is not in a non-functional state."
#define CLI_INFO_ON                                           L" on"
#define CLI_PROGRESS_STR                                      L"\rOperation on DIMM 0x%04x Progress: %d%%"

#define CLI_LOAD_MFG_FW                                       L"MFG Load Prod FW"
#define CLI_INJECT_MFG                                        L"MFG Inject command"
#define CLI_MEM_INFO_MFG                                      L"MFG Mem Info page"

#define CLI_INFO_SET_FW_LOG_LEVEL                             L"Set firmware log level"
#define CLI_INFO_PACKAGE_SPARING_INJECT_ERROR                 L"Trigger package sparing"
#define CLI_INFO_POISON_INJECT_ERROR                          L"Poison address " FORMAT_STR
#define CLI_INFO_PERCENTAGE_REMAINING_INJECT_ERROR            L"Trigger a percentage remaining"
#define CLI_INFO_FATAL_MEDIA_ERROR_INJECT_ERROR               L"Create a media fatal error"
#define CLI_INFO_DIRTY_SHUT_DOWN_INJECT_ERROR                 L"Trigger a dirty shut down"
#define CLI_INFO_TEMPERATURE_INJECT_ERROR                     L"Set temperature"
#define CLI_INFO_CLEAR_PACKAGE_SPARING_INJECT_ERROR           L"Clear injected package sparing"
#define CLI_INFO_CLEAR_POISON_INJECT_ERROR                    L"Clear injected poison of address " FORMAT_STR
#define CLI_INFO_CLEAR_PERCENTAGE_REMAINING_INJECT_ERROR      L"Clear injected percentage remaining"
#define CLI_INFO_CLEAR_FATAL_MEDIA_ERROR_INJECT_ERROR         L"Clear injected media fatal error"
#define CLI_INFO_CLEAR_DIRTY_SHUT_DOWN_INJECT_ERROR           L"Clear dirty shut down"
#define CLI_INFO_CLEAR_TEMPERATURE_INJECT_ERROR               L"Clear injected temperature"

#define PROMPT_CONTINUE_QUESTION                              L"Do you want to continue? [y/n] "

#define CLI_CREATE_GOAL_PROMPT_VOLATILE                       L"The requested goal was adjusted more than 10%% to find a valid configuration."
#define CLI_CREATE_GOAL_PROMPT_HEADER                         L"The following configuration will be applied:"
#define CLI_WARN_GOAL_CREATION_SECURITY_UNLOCKED              L"WARNING: Goal will not be applied unless security is disabled prior to platform firmware (BIOS) provisioning!"
#define CLI_ERR_CREATE_GOAL_AUTO_PROV_ENABLED                 L"Error: Automatic provisioning is enabled. Please disable to manually create goals."

#define CLI_CREATE_NAMESPACE_PROMPT_ROUNDING_CAPACITY         L"The requested namespace capacity %lld B will be rounded up to %lld B to align properly."

#define REGION_FOUND         L"REGION FOUND."

#define CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE                  L"Unable to retrieve user display preferences."

#define CLI_DOWNGRADE_PROMPT                                  L"Downgrade firmware on DIMM " FORMAT_STR L"?"

#define CLI_RECOVER_DIMM_PROMPT_STR                           L"Recover dimm:"

#define CLI_FORMAT_DIMM_REBOOT_REQUIRED_STR                   L"A power cycle is required after a device format."
#define CLI_FORMAT_DIMM_PROMPT_STR                            L"This operation will take several minutes to complete and will erase all data on DIMM "
#define CLI_INFO_START_FORMAT                                 L"Format"
#define CLI_FORMAT_DIMM_STARTING_FORMAT                       L"Formatting DIMM(s)..."

#define CLI_INFO_DUMP_SUPPORT_SUCCESS                         L"Dump support data successfully written to " FORMAT_STR L"."
#define CLI_INFO_DUMP_CONFIG_SUCCESS                          L"Successfully dumped system configuration to file: " FORMAT_STR_NL

#define CLI_ERR_INJECT_FATAL_ERROR_UNSUPPORTED_ON_OS          L"Injecting a Fatal Media error is unsupported on this OS.\nPlease contact your OSV for assistance in performing this action."
#define CLI_ERR_TRANSPORT_PROTOCOL_UNSUPPORTED_ON_OS          L"The following protocol " FORMAT_STR L" is unsupported on this OS. Please use " FORMAT_STR L" instead.\n"

#define PRINT_SETTINGS_FORMAT_FOR_SHOW_SYS_CAP_CMD  1
#define PRINT_SETTINGS_FORMAT_FOR_SHOW_REGION_CMD     2

#define CLI_ERR_FAILED_TO_GET_PBR_MODE                        L"Failed to get the current PBR mode."
#define CLI_ERR_FAILED_TO_SET_PBR_MODE                        L"Failed to configure the playback and record mode to: " FORMAT_STR
#define CLI_ERR_FAILED_NO_PBR_SESSION_LOADED                  L"Failed to configure the playback mode.  No PBR session loaded."
#define CLI_ERR_FAILED_TO_SET_SESSION_TAG                     L"Failed to set the current tag location."
#define CLI_ERR_FAILED_TO_GET_SESSION_TAG                     L"Failed to get the current tag location."
#define CLI_ERR_FAILED_TO_GET_SESSION_TAG_COUNT               L"Failed to get the session tag count."
#define CLI_ERR_FAILED_DURING_DRIVER_INIT                     L"Driver binding start failed."
#define CLI_ERR_FAILED_DURING_DRIVER_UNINIT                   L"Driver binding stop failed."
#define CLI_ERR_FAILED_DURING_CMD_EXECUTION                   L"Executing CMD during playback failed."
#define CLI_ERR_UNKNOWN_MODE                                  L"Unknown PBR mode."
#define CLI_ERR_FAILED_TO_GET_SESSION_BUFFER                  L"Failed to get the current session buffer."
#define CLI_ERR_FAILED_TO_DUMP_SESSION_TO_FILE                L"Failed to dump contents of session buffer to a file."
#define CLI_ERR_FAILED_TO_GET_FILE_PATH                       L"Failed to get file path " FORMAT_EFI_STATUS
#define CLI_ERR_FAILED_TO_READ_FILE                           L"Failed to read pbr file."
#define CLI_ERR_FAILED_TO_SET_SESSION_BUFFER                  L"Failed to set session buffer."
#define CLI_ERR_FAILED_TO_RESET_SESSION                       L"Failed to reset the session."
#define CLI_ERR_CMD_FAILED_NOT_ADMIN                          L"Error: The ipmctl command you have attempted to execute requires administrator privileges."

#define ERROR_CHECKING_MIXED_SKU    L"Error: Could not check if SKU is mixed."
#define WARNING_DIMMS_SKU_MIXED     L"Warning: Mixed SKU detected. Driver functionalities limited.\n"

/**
  sizeof returns the number of bytes that the array uses.
  We need to divide it by the length of a single pointer to get the number of elements.
**/
#define ALLOWED_DISP_VALUES_COUNT(A) (sizeof(A)/sizeof(CHAR16*))

/**
  GUID for NvmDimmCli Variables for Get/Set via runtime services.
**/
#define NVMDIMM_CLI_NGNVM_VARIABLE_GUID \
  { 0x11c64219, 0xbfa2, 0x42ce, {0x99, 0xb1, 0x17, 0x0b, 0x4a, 0x2b, 0xe0, 0x8e}}

/**
  Retrieve a populated array and count of DIMMs in the system. The caller is
  responsible for freeing the returned array

  @param[in] pNvmDimmConfigProtocol A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pCmd A pointer to a COMMAND struct.  Used to obtain the Printer context.
             printed to stdout, otherwise will be directed to the printer module.
  @param[in] dimmInfoCategories Categories that will be populated in
             the DIMM_INFO struct.
  @param[out] ppDimms A pointer to the dimm list found in NFIT.
  @param[out] pDimmCount A pointer to the number of DIMMs found in NFIT.

  @retval EFI_SUCCESS  the dimm list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetDimmList(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN     struct Command *pCmd,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO **ppDimms,
     OUT UINT32 *pDimmCount
  );

/**
  Retrieve a populated array and count of all DCPMMs (functional and non-functional)
  in the system. The caller is responsible for freeing the returned array

  @param[in] pNvmDimmConfigProtocol A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pCmd A pointer to a COMMAND struct.  Used to obtain the Printer context.
             printed to stdout, otherwise will be directed to the printer module.
  @param[in] dimmInfoCategories Categories that will be populated in
             the DIMM_INFO struct.
  @param[out] ppDimms A pointer to a combined DCPMM list (initialized and
              uninitialized) from NFIT.
  @param[out] pDimmCount A pointer to the total number of DCPMMs found in NFIT.

  @retval EFI_SUCCESS  the dimm list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetAllDimmList(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN     struct Command *pCmd,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
  OUT DIMM_INFO **ppDimms,
  OUT UINT32 *pDimmCount
);

/**
  Parse the string and return the array of unsigned integers

  Example
    String: "1,3,7"
    Array[0]: 1
    Array[1]: 3
    Array[2]: 7

  @param[in] pString string to parse
  @param[out] ppUints allocated, filled array with the uints
  @param[out] pUintsNum size of uints array

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER inputs are null, the format of string is not proper, duplicated Dimm IDs
  @retval EFI_NOT_FOUND Dimm not found
**/
EFI_STATUS
GetUintsFromString(
  IN     CHAR16 *pString,
     OUT UINT16 **ppUints,
     OUT UINT32 *pUintsNum
  );

/**
  Parses the dimm target string (which can contain DimmIDs as SMBIOS type-17 handles and/or DimmUIDs),
  and returns an array of DimmIDs in the SMBIOS physical-id forms.
  Also checks for invalid DimmIDs and duplicate entries.

  Example
    String: "8089-00-0000-13325476,30,0x0022"
    Array[0]: 28
    Array[1]: 30
    Array[2]: 34

  @param[in] pCmd A pointer to a COMMAND struct.  Used to obtain the Printer context.
  @param[in] pDimmString The dimm target string to parse.
  @param[in] pDimmInfo The dimm list found in NFIT.
  @param[in] DimmCount Size of the pDimmInfo array.
  @param[out] ppDimmIds Pointer to the array allocated and filled with the SMBIOS DimmIDs.
  @param[out] pDimmIdsCount Size of the pDimmIds array.

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER the format of string is not proper
  @retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetDimmIdsFromString(
  IN     struct Command *pCmd,
  IN     CHAR16 *pDimmString,
  IN     DIMM_INFO *pDimmInfo,
  IN     UINT32 DimmCount,
     OUT UINT16 **ppDimmIds,
     OUT UINT32 *pDimmIdsCount
 );

/**
Parses the dimm target string (which can contain DimmIDs as SMBIOS type-17 handles and/or DimmUIDs),
and returns a DimmUid.

Example
String: "8089-00-0000-13325476" or "30" or "0x0022"

@param[in] pDimmString The dimm target string to parse.
@param[in] pDimmInfo The dimm list found in NFIT.
@param[in] DimmCount Size of the pDimmInfo array.
@param[out] pDimmUid Pointer to the NVM_UID buffer.

@retval EFI_SUCCESS
@retval EFI_OUT_OF_RESOURCES memory allocation failure
@retval EFI_INVALID_PARAMETER the format of string is not proper
@retval EFI_NOT_FOUND dimm not found
**/
EFI_STATUS
GetDimmUidFromString(
    IN     CHAR16 *pDimmString,
    IN     DIMM_INFO *pDimmInfo,
    IN     UINT32 DimmCount,
    OUT    CHAR8 *pDimmUid
);

/**
  Check if the uint is in the uints array

  @param[in] pUints - array of the uints
  @param[in] UintsNum number of uints in the array
  @param[in] UintToFind searched uint

  @retval TRUE if the uint has been found
  @retval FALSE if the uint has not been found
**/
BOOLEAN
ContainUint(
  IN     UINT16 *pSockets,
  IN     UINT32 SocketNum,
  IN     UINT16 Socket
  );

/**
  Check if the Guid is in the Guids array

  @param[in] ppGuids array of the Guid pointers
  @param[in] GuidsNum number of Guids in the array
  @param[in] pGuidToFind pointer to GUID with information to find

  @retval TRUE if table contains guid with same data as *pGuidToFind
  @retval FALSE
**/
BOOLEAN
ContainGuid(
  IN GUID **ppGuids,
  IN UINT32 GuidsNum,
  IN GUID *pGuidToFind
  );

/**
  Checks if the provided display list string contains only the valid values.

  @param[in] pDisplayValues pointer to the Unicode string containing the user
    input display list.
  @param[in] ppAllowedDisplayValues pointer to an array of Unicode strings
    that define the valid display values.
  @param[in] Count is the number of valid display values in ppAllowedDisplayValues.

  @retval EFI_SUCCESS if all of the provided display values are valid.
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
  @retval EFI_INVALID_PARAMETER if one or more of the provided display values
    is not a valid one. Or if pDisplayValues or ppAllowedDisplayValues is NULL.
**/
EFI_STATUS
CheckDisplayList(
  IN     CHAR16 *pDisplayValues,
  IN     CHAR16 **ppAllowedDisplayValues,
  IN     UINT16 Count
  );

/**
  Gets number of Manageable and supported Dimms and their IDs and Handles

  @param[in] pNvmDimmConfigProtocol A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] CheckSupportedConfigDimm If true, include dimms in unmapped set of dimms (non-POR) in
                                      returned dimm list. If false, skip these dimms from returned list.
  @param[out] DimmIdsCount  is the pointer to variable, where number of dimms will be stored.
  @param[out] ppDimmIds is the pointer to variable, where IDs of dimms will be stored.

  @retval EFI_NOT_FOUND if the connection with NvmDimmProtocol can't be estabilished
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
  @retval EFI_INVALID_PARAMETER if number of dimms or dimm IDs have not been assigned properly.
  @retval EFI_SUCCESS if succefully assigned number of dimms and IDs to variables.
**/
EFI_STATUS
GetManageableDimmsNumberAndId(
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN  BOOLEAN CheckSupportedConfigDimm,
  OUT UINT32 *pDimmIdsCount,
  OUT UINT16 **ppDimmIds
);

/**
  Checks if user has specified the options -a|-all and -d|-display.
  Those two flags exclude each other so the function also checks
  if the user didn't provide them both.
  If the -d|-display option has been found, the its values are checked
  against the allowed values for this parameter.

  @param[in] pCommand is the pointer to a Command structure that is tested
    for the options presence.
  @param[in] ppAllowedDisplayValues is a pointer to an array of Unicode
    strings considered as the valid values for the -d|-display option.
  @param[in] AllowedDisplayValuesCount is a UINT32 value that represents
    the number of elements in the array pointed by ppAllowedDisplayValues.
  @param[out] pDispOptions contains the following.
    A BOOLEAN value that will
    represent the presence of the -a|-all option in the Command pointed
    by pCommand.
    A BOOLEAN value that will
    represent the presence of the -d|-display option in the Command pointed
    by pCommand.
    A pointer to an Unicode string. If the -d|-display option is present, this pointer will
    be set to the option value Unicode string.

  @retval EFI_SUCCESS the check went fine, there were no errors
  @retval EFI_INVALID_PARAMETER if the user provided both options,
    the display option has been provided and has some invalid values or
    if at least one of the input pointer parameters is NULL.
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
**/
EFI_STATUS
CheckAllAndDisplayOptions(
  IN     struct Command *pCommand,
  IN     CHAR16 **ppAllowedDisplayValues,
  IN     UINT32 AllowedDisplayValuesCount,
  OUT CMD_DISPLAY_OPTIONS *pDispOptions
);

/**
  Display command status with specified command message.
  Function displays per DIMM status if such exists and
  summarizing status for whole command. Memory allocated
  for status message and command status is freed after
  status is displayed.

  @param[in] pStatusMessage String with command information
  @param[in] pStatusPreposition String with preposition
  @param[in] pCommandStatus Command status data

  @retval EFI_INVALID_PARAMETER pCommandStatus is NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
DisplayCommandStatus (
  IN     CONST CHAR16 *pStatusMessage,
  IN     CONST CHAR16 *pStatusPreposition,
  IN     COMMAND_STATUS *pCommandStatus
  );

/**
  Retrieve property by name and assign its value to UINT64.

  @param[in] pCmd Command containing the property
  @param[in] pPropertyName String with property name

  @param[out] pOutValue target UINT64 value

  @retval FALSE if there was no such property or it doesn't contain
    a valid value
**/
BOOLEAN
PropertyToUint64 (
  IN     struct Command *pCmd,
  IN     CHAR16 *pPropertyName,
     OUT UINT64 *pOutValue
  );

/**
  Retrieve property by name and assign its value to double

  @param[in] pCmd Command containing the property
  @param[in] pPropertyName String with property name
  @param[out] pOutValue Target double value

  @retval EFI_INVALID_PARAMETER Property not found or no valid value inside
  @retval EFI_SUCCESS Conversion successful
**/
EFI_STATUS
PropertyToDouble (
  IN     struct Command *pCmd,
  IN     CHAR16 *pPropertyName,
     OUT double *pOutValue
  );

/**
  Extracts working directory path from file path

  @param[in] pUserFilePath Pointer to string with user specified file path
  @param[out] pOutFilePath Pointer to actual file path
  @param[out] ppDevicePath Pointer to where to store device path

  @retval EFI_SUCCESS Extraction success
  @retval EFI_INVALID_PARAMETER Invalid parameter
  @retval EFI_OUT_OF_RESOURCES Out of resources
**/
EFI_STATUS
GetDeviceAndFilePath(
  IN     CHAR16 *pUserFilePath,
  IN OUT CHAR16 *pOutFilePath,
  IN OUT EFI_DEVICE_PATH_PROTOCOL **ppDevicePath
  );

/**
  Match driver command status to CLI return code

  @param[in] Status - NVM_STATUS returned from driver

  @retval - Appropriate EFI return code
**/
EFI_STATUS
MatchCliReturnCode (
  IN     NVM_STATUS Status
 );

/**
  Get free space of volume from given path

  @param[in] pFileHandle - file handle protocol
  @param[out] pFreeSpace - free space

  @retval - Appropriate EFI return code
**/
EFI_STATUS
GetVolumeFreeSpace(
  IN      EFI_FILE_HANDLE pFileHandle,
     OUT  UINT64  *pFreeSpace
  );


/**
  Check if file exists

  @param[in] pDumpUserPath - destination file path
  @param[out] pExists - pointer to whether or not destination file already exists

  @retval - Appropriate EFI return code
**/
EFI_STATUS
FileExists (
  IN     CHAR16* pDumpUserPath,
     OUT BOOLEAN* pExists
  );

/**
  Delete file

  @param[in] pDumpUserPath - file path to delete

  @retval - Appropriate EFI return code
**/
EFI_STATUS
DeleteFile (
  IN     CHAR16* pDumpUserPath
);

/**
  Dump data to file

  @param[in] pDumpUserPath - destination file path
  @param[in] BufferSize - data size to write
  @param[in] pBuffer - pointer to buffer
  @param[in] Overwrite - enforce overwriting file

  @retval - Appropriate EFI return code
**/
EFI_STATUS
DumpToFile (
  IN     CHAR16* pDumpUserPath,
  IN     UINT64 BufferSize,
  IN     VOID* pBuffer,
  IN     BOOLEAN Overwrite
  );

/**
  Prints supported or recommended appdirect settings

  @param[in] pInterleaveFormatList pointer to variable length interleave formats array
  @param[in] FormatNum number of the appdirect settings formats
  @param[in] pInterleaveSize pointer to Channel & iMc interleave size
  @param[in] PrintRecommended if TRUE Recommended settings will be printed
             if FALSE Supported settings will be printed
  @param[in] Mode Set mode to print different format
  @retval String representing AppDirect settings.  Null on error.
**/
CHAR16*
PrintAppDirectSettings(
  IN    VOID *pInterleaveFormatList,
  IN    UINT16 FormatNum,
  IN    INTERLEAVE_SIZE *pInterleaveSize,
  IN    BOOLEAN PrintRecommended,
  IN    UINT8 Mode
  );

/**
  Read source file and return current passphrase to unlock device.

  @param[in] pCmd A pointer to a COMMAND struct.  Used to obtain the Printer context.
  @param[in] pFileHandle File handler to read Passphrase from
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the
                          physical device or logical device. The device path describes the location of the device
                          the handle is for.
  @param[out] Current passphrase

  @retval EFI_SUCCESS File load and parse success
  @retval EFI_INVALID_PARAMETER Invalid Parameter during load
  @retval other Return Codes from TrimLineBuffer,
                GetLoadPoolData, GetLoadDimmData, GetLoadValue functions
**/
EFI_STATUS
ParseSourcePassFile(
  IN     struct Command *pCmd,
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT CHAR16 **ppCurrentPassphrase OPTIONAL,
     OUT CHAR16 **ppNewPassphrase OPTIONAL
  );

/**
  Prompted input request

  @param[in] pPrompt - information about expected input
  @param[in] ShowInput - Show characters written by user
  @param[in] OnlyAlphanumeric - Allow only for alphanumeric characters
  @param[out] ppReturnValue - is a pointer to a pointer to the 16-bit character string
        that will contain the return value

  @retval - Appropriate CLI return code
**/
EFI_STATUS
PromptedInput(
  IN     CHAR16 *pPrompt,
  IN     BOOLEAN ShowInput,
  IN     BOOLEAN OnlyAlphanumeric,
     OUT CHAR16 **ppReturnValue
  );

/**
  Display "yes/no" question and retrieve reply using prompt mechanism

  @param[out] pConfirmation Confirmation from prompt

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
PromptYesNo(
     OUT BOOLEAN *pConfirmation
  );

/**
  Read input from console
  @param[in] ShowInput - Show characters written by user
  @param[in] OnlyAlphanumeric - Allow only for alphanumeric characters
  @param[in, out] ppReturnValue - is a pointer to a pointer to the 16-bit character
        string without null-terminator that will contain the return value
  @param[in, out] pBufferSize - is a pointer to the Size in bytes of the return buffer

  @retval - Appropriate CLI return code
**/
EFI_STATUS
ConsoleInput(
  IN     BOOLEAN ShowInput,
  IN     BOOLEAN OnlyAlphanumeric,
  IN OUT CHAR16 **ppReturnValue,
  IN OUT UINTN *pBufferSize OPTIONAL
  );

/**
  Check all dimms if SKU conflict occurred.

  @param[out] pSkuMixedMode is a pointer to a BOOLEAN value that will
    represent the presence of SKU mixed mode

  @retval EFI_INVALID_PARAMETER Input parameter was NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
IsSkuMixed(
     OUT BOOLEAN *pSkuMixedMode
  );

/**
  Print Load Firmware progress for all DIMMs

  @param[in] ProgressEvent EFI Event
  @param[in] pContext context pointer
**/
VOID
EFIAPI
PrintProgress(
  IN     EFI_EVENT ProgressEvent,
  IN     VOID *pContext
  );

/**
  Get relative path from absolute path

  @param[in] pAbsolutePath Absolute path
  @param[out] ppRelativePath Relative path

  @retval EFI_INVALID_PARAMETER Input parameter was NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
GetRelativePath(
  IN     CHAR16 *pAbsolutePath,
     OUT CHAR16 **ppRelativePath
  );

/**
  Check if all dimms in the specified pDimmIds list are manageable.
  This helper method assumes all the dimms in the list exist.
  This helper method also assumes the parameters are non-null.

  @param[in] pDimmInfo The dimm list found in NFIT.
  @param[in] DimmCount Size of the pDimmInfo array.
  @param[in] pDimmIds Pointer to the array of DimmIDs to check.
  @param[in] pDimmIdsCount Size of the pDimmIds array.

  @retval TRUE if all Dimms in pDimmIds list are manageable
  @retval FALSE if at least one DIMM is not manageable
**/
BOOLEAN
AllDimmsInListAreManageable(
  IN     DIMM_INFO *pAllDimms,
  IN     UINT32 AllDimmCount,
  IN     UINT16 *pDimmsListToCheck,
  IN     UINT32 DimmsToCheckCount
 );

/**
  Check if all dimms in the specified pDimmIds list are in supported
  config. This helper method assumes all the dimms in the list exist.
  This helper method also assumes the parameters are non-null.

  @param[in] pDimmInfo The dimm list found in NFIT.
  @param[in] DimmCount Size of the pDimmInfo array.
  @param[in] pDimmIds Pointer to the array of DimmIDs to check.
  @param[in] pDimmIdsCount Size of the pDimmIds array.

  @retval TRUE if all Dimms in pDimmIds list are manageable
  @retval FALSE if at least one DIMM is not manageable
**/
BOOLEAN
AllDimmsInListInSupportedConfig(
  IN     DIMM_INFO *pAllDimms,
  IN     UINT32 AllDimmCount,
  IN     UINT16 *pDimmsListToCheck,
  IN     UINT32 DimmsToCheckCount
);

/**
   Get Dimm identifier preference

   @param[out] pDimmIdentifier Variable to store Dimm identerfier preference

   @retval EFI_SUCCESS Success
   @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
GetDimmIdentifierPreference(
     OUT UINT8 *pDimmIdentifier
  );

/**
  Get Dimm identifier as string based on user preference

  @param[in] DimmId Dimm ID as number
  @param[in] pDimmUid Dimmm UID as string
  @param[out] pResultString String representation of preferred value
  @param[in] ResultStringLen Length of pResultString

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
GetPreferredDimmIdAsString(
  IN     UINT32 DimmId,
  IN     CHAR16 *pDimmUid OPTIONAL,
     OUT CHAR16 *pResultString,
  IN     UINT32 ResultStringLen
  );

/**
  Retrieve Display DimmID Runtime Index from Property String

  @param[in] String to try to discover index for

  @retval DimmID Index of DimmID property string
  @retval Size of Array if not found
**/
UINT8 GetDimmIDIndex(
  IN  CHAR16 *pDimmIDStr
  );

/**
  Retrieve Display Size Runtime Index from Property String

  @param[in] String to try to discover index for

  @retval Display Size Index of Size property string
  @retval Size of Array if not found
**/
UINT8 GetDisplaySizeIndex(
  IN  CHAR16 *pSizeStr
  );
/**
  Retrieve Display DimmID String from RunTime variable index

  @param[in] Index to retrieve

  @retval NULL Index was invalid
  @retval DimmID String of user display preference
**/
CONST CHAR16 *GetDimmIDStr(
  IN  UINT8 DimmIDIndex
  );

/**
  Retrieve Display Size String from RunTime variable index

  @param[in] Index to retrieve

  @retval NULL Index was invalid
  @retval Size String of user display preference
**/
CONST CHAR16 *GetDisplaySizeStr(
  IN  UINT8 DisplaySizeIndex
  );

/**
Allocate and return string which is related with the binary RegionType value.
The caller function is obligated to free memory of the returned string.

@param[in] RegionType - region type

@retval - output string
**/
CHAR16 *
RegionTypeToString(
  IN     UINT8 RegionType
);

/**
  Gets the DIMM handle corresponding to Dimm PID and also the index

  @param[in] DimmId - DIMM ID
  @param[in] pDimms - List of DIMMs
  @param[in] DimmsNum - Number of DIMMs
  @param[out] pDimmHandle - The Dimm Handle corresponding to the DIMM ID
  @param[out] pDimmIndex - The Index of the found DIMM

  @retval - EFI_STATUS Success
  @retval - EFI_INVALID_PARAMETER Invalid parameter
  @retval - EFI_NOT_FOUND Dimm not found
**/
EFI_STATUS
GetDimmHandleByPid(
  IN     UINT16 DimmId,
  IN     DIMM_INFO *pDimms,
  IN     UINT32 DimmsNum,
     OUT UINT32 *pDimmHandle,
     OUT UINT32 *pDimmIndex
  );

/**
Retrieve the User Cli Display Preferences CMD line arguments.

@param[out] pDisplayPreferences pointer to the current driver preferences.

@retval EFI_INVALID_PARAMETER One or more parameters are invalid
@retval EFI_SUCCESS All ok
**/
EFI_STATUS
ReadCmdLinePrintOptions(
  IN OUT PRINT_FORMAT_TYPE *pFormatType,
  IN struct Command *pCmd
);

/**
  Helper to recreate -o args in string format

  @param[in] pCmd command from CLI
  @param[out] ppOutputStr resulting -o string
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd or ppOutputStr is NULL
**/
EFI_STATUS
CreateCmdLineOutputStr(
  IN     struct Command *pCmd,
  OUT     CHAR16 **ppOutputStr
);

/**
  Convert UEFI return codes to legacy OS return codes

  @param[in] UefiReturnCode - return code to Convert

  @retval - Converted OS ReturnCode
**/
EFI_STATUS UefiToOsReturnCode(EFI_STATUS UefiReturnCode);

/**
  Checks if user has incorrectly used master and default options. Also checks for
  invalid combinations of these options with the Passphrase property.

  @param[in] pCmd command from CLI
  @param[in] isPassphraseProvided TRUE if user provided passphrase
  @param[in] isMasterOptionSpecified TRUE if master option is specified
  @param[in] isDefaultOptionSpecified TRUE if default option is specified

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
CheckMasterAndDefaultOptions(
  IN struct Command *pCmd,
  IN BOOLEAN isPassphraseProvided,
  IN BOOLEAN isMasterOptionSpecified,
  IN BOOLEAN isDefaultOptionSpecified
);

/**
  Retrieves a list of Dimms that have at least one NS.

  @param[in,out] pDimmIds the dimm IDs which have NS
  @param[in,out] pDimmIdCount count of dimm IDs
  @param[in]     maxElements the maximum size of the dimm ID list

  @retval EFI_ABORTED Operation Aborted
  @retval EFI_OUT_OF_RESOURCES unable to allocate memory
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
GetDimmIdsWithNamespaces(
  IN OUT UINT16 *pDimmIds,
  IN OUT UINT32 *pDimmIdCount,
  IN UINT32 maxElements);

/**
  Adds an element to a element list without allowing duplication

  @param[in,out] pElementList the list
  @param[in,out] pElementCount size of the list
  @param[in]     newElement the new element to add
  @param[in]     maxElements the maximum size of the list

  @retval EFI_OUT_OF_RESOURCES unable to add any more elements
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS AddElement(
  IN OUT UINT16 *pElementList,
  IN OUT UINT32 *pElementCount,
  IN UINT16 newElement,
  IN UINT32 maxElements);

#endif /** _COMMON_H_ **/
