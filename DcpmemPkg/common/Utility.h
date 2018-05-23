/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <Version.h>
#include <Types.h>
#include <NvmTypes.h>
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include "NvmHealth.h"

#ifdef OS_BUILD
#include <os_efi_preferences.h>
#endif

#ifdef _MSC_VER
int _fltused();
#endif


/**
  The Config Protocol version bytes definition
**/
#pragma pack(push, 1)
typedef struct {
  UINT32 Major : 16;
  UINT32 Minor : 15;
  UINT32 BuildType : 1;
} CpVersionSeparated;
#pragma pack(pop)

typedef union {
  UINT32 AsUint32;
  CpVersionSeparated Separated;
} CONFIG_PROTOCOL_VERSION;

#define MAX_CONFIG_DUMP_FILE_SIZE MIB_TO_BYTES(1)

#define MAX_LINE_CHAR_LENGTH 400
#define MAX_LINE_BYTE_LENGTH (MAX_LINE_CHAR_LENGTH * sizeof(CHAR8))

#define COUNT_TO_INDEX_OFFSET 1
#define FIRST_CHAR_INDEX 0

#define UTF_16_BOM L'\xFEFF'

#define SKU_MEMORY_MODE_FLAG      (BIT0)
#define SKU_STORAGE_MODE_FLAG     (BIT1)
#define SKU_APP_DIRECT_MODE_FLAG  (BIT2)
#define SKU_MODES_MASK  (SKU_MEMORY_MODE_FLAG | SKU_STORAGE_MODE_FLAG | SKU_APP_DIRECT_MODE_FLAG)

#define SKU_ENCRYPTION_MASK                  (BIT17)

//Long operation timer defines
#define LONG_OP_POLL_EVENT_TIMER 0                               //timer used for polling
#define LONG_OP_POLL_TIMER_INTERVAL EFI_TIMER_PERIOD_SECONDS(1)  //defines how often to poll long op status
#define LONG_OP_POLL_EVENT_TIMEOUT 1                             //timer used for timeout event
#define LONG_OP_POLL_EVENT_SIZE 2                                //total number of events to wait for

#define LONG_OP_FW_UPDATE_TIMEOUT EFI_TIMER_PERIOD_SECONDS(10)   //Average time is 2-3 seconds

//Firmware update opcodes to match long operation status
#define FW_UPDATE_OPCODE    0x09
#define FW_UPDATE_SUBOPCODE 0x00

// Last shutdown status string values
#define LAST_SHUTDOWN_STATUS_PM_ADR_STR               L"PM ADR Command"
#define LAST_SHUTDOWN_STATUS_PM_S3_STR                L"PM S3"
#define LAST_SHUTDOWN_STATUS_PM_S5_STR                L"PM S5"
#define LAST_SHUTDOWN_STATUS_DDRT_POWER_FAIL_STR      L"DDRT Power Fail Command"
#define LAST_SHUTDOWN_STATUS_PMIC_12V_POWER_FAIL_STR  L"PMIC 12V Power Fail"
#define LAST_SHUTDOWN_STATUS_PM_WARM_RESET_STR        L"PM Warm Reset"
#define LAST_SHUTDOWN_STATUS_THERMAL_SHUTDOWN_STR     L"Thermal Shutdown"
#define LAST_SHUTDOWN_STATUS_FW_FLUSH_COMPLETE_STR    L"FW Flush Complete"
#define LAST_SHUTDOWN_STATUS_UNKNOWN_STR              L"Unknown"

// Last shutdown status extended string values
#define LAST_SHUTDOWN_STATUS_VIRAL_INTERRUPT_STR                 L"Viral Interrupt"
#define LAST_SHUTDOWN_STATUS_SURPRISE_CLOCK_STOP_INTERRUPT_STR   L"Surprise Clock Stop"
#define LAST_SHUTDOWN_STATUS_WRITE_DATA_FLUSH_COMPLETE_STR       L"Write Data Flush Complete"
#define LAST_SHUTDOWN_STATUS_S4_POWER_STATE_STR                  L"PM S4"

// Memory modes supported string values
#define MODES_SUPPORTED_MEMORY_MODE_STR      L"Memory Mode"
#define MODES_SUPPORTED_APP_DIRECT_MODE_STR  L"App Direct"

// Security capabilities string values
#define SECURITY_CAPABILITIES_ENCRYPTION  L"Encryption"
#define SECURITY_CAPABILITIES_ERASE       L"Erase"
#define SECURITY_CAPABILITIES_NONE        L"None"

// ARS status string values
#define ARS_STATUS_UNKNOWN_STR       L"Unknown"
#define ARS_STATUS_NOT_STARTED_STR   L"Not Started"
#define ARS_STATUS_IN_PROGRESS_STR   L"In Progress"
#define ARS_STATUS_COMPLETED_STR     L"Completed"
#define ARS_STATUS_ABORTED_STR       L"Aborted"

// Overwrite DIMM status string values
#define OVERWRITE_DIMM_STATUS_UNKNOWN_STR      L"Unknown"
#define OVERWRITE_DIMM_STATUS_NOT_STARTED_STR  L"Not started"
#define OVERWRITE_DIMM_STATUS_IN_PROGRESS_STR  L"In progress"
#define OVERWRITE_DIMM_STATUS_COMPLETED_STR    L"Completed"

// Memory type string values
#define MEMORY_TYPE_AEP_STR      L"AEP DIMM"
#define MEMORY_TYPE_DDR4_STR     L"DDR4"
#define MEMORY_TYPE_UNKNOWN_STR  L"Unknown"

//Units string vaules
#define UNITS_B_STR  L"B"
#define UNITS_GB_STR  L"GB"
#define UNITS_GIB_STR L"GiB"
#define UNITS_MB_STR  L"MB"
#define UNITS_MIB_STR  L"MiB"
#define UNITS_TB_STR  L"TB"
#define UNITS_TIB_STR  L"TiB"

//Number of digits after point
#define ONE_DIGIT_AFTER_POINT    1
#define TWO_DIGITS_AFTER_POINT   2
#define THREE_DIGITS_AFTER_POINT 3

//Namespace healthstates
#define HEALTHSTATE_OK             L"Healthy"
#define HEALTHSTATE_WARNING        L"Warning"
#define HEALTHSTATE_CRITICAL       L"Critical"
#define HEALTHSTATE_UNKNOWN        L"Unknown"
#define HEALTHSTATE_UNSUPPORTED    L"Unsupported"
#define HEALTHSTATE_LOCKED         L"Locked"

//Persistent Memory Type
#define PERSISTENT_MEM_TYPE_AD_STR       L"AppDirect"
#define PERSISTENT_MEM_TYPE_AD_NI_STR    L"AppDirectNotInterleaved"

// Interface Format Code
#define FORMAT_CODE_APP_DIRECT_STR  L"(Non-Energy Backed Byte Addressable)"
#define FORMAT_CODE_STORAGE_STR     L"(Non-Energy Backed Block Addressable)"



/**
  We define the EFI Shell Protocol Guid locally, so that we won't include Shell Package headers in the driver.

  The #define name is slightly different than in the UDK, so that we will avoid symbol redefinitions.
**/
#define EFI_SHELL_PROTOCOL_GUID \
  { 0x6302d008, 0x7f9b, 0x4f30, { 0x87, 0xac, 0x60, 0xc9, 0xfe, 0xf5, 0xda, 0x4e } }

#ifdef _MSC_VER
#  define INLINE __inline
#elif defined(__GNUC__)
#  define INLINE __attribute__ ((gnu_inline)) inline
#else
#  define INLINE inline
#endif

/**
  BCD - Binary coded decimals macros
  Macros are used to compact two digits into single 8bit variable and vice versa
**/
#define BAD_POINTER ((VOID *) 0xAFAFAFAFAFAFAFAF)
#define BCD_TO_TWO_DEC(BUFF) ((((BUFF) >> 4) * 10) + ((BUFF) & 0xF))
#define TWO_DEC_TO_BCD(A, B) (((A) << 4) + (B))

#define INTERVAL(MIN, MAX) L"<" DEFINE_TO_STRING(MIN) L"," DEFINE_TO_STRING(MAX) L">"
#define INTERVAL_OPEN(MIN, MAX) L"(" DEFINE_TO_STRING(MIN) L"," DEFINE_TO_STRING(MAX) L")"

#define BIT_ON(var, bit_number) {(var) |= (1 << (bit_number)); }
#define BIT_OFF(var, bit_number) {(var) &= (~(1 << (bit_number))); }
#define BIT_GET(var, bit_number) ((var) & (1 << bit_number))

#define IS_BIT_SET_VAR(var, bit_mask) (((var) & (bit_mask)) != 0)

#define FIRST_POOL_GOAL  0
#define SECOND_POOL_GOAL 1

#define BYTES_TO_KIB(Size)        ((Size)>>10)
#define BYTES_TO_KB(Size)         ((Size)/1000)
#define BYTES_TO_MIB(Size)        ((Size)>>20)
#define BYTES_TO_MB(Size)         ((Size)/1000/1000)
#define BYTES_TO_GIB(Size)        ((Size)>>30)
#define BYTES_TO_TIB(Size)        ((Size)>>40)
#define BYTES_TO_TB(Size)         ((Size)/1000/1000/1000/1000)
#define BYTES_TO_GB(Size)         ((Size)/1000/1000/1000)
#define KIB_TO_BYTES(Size)        ((Size) * BYTES_IN_KIB)
#define MIB_TO_BYTES(Size)        ((Size) * BYTES_IN_MEBIBYTE)
#define GIB_TO_BYTES(Size)        ((Size) * BYTES_IN_GIBIBYTE)
#define KIB_TO_MIB(Size)          ((Size)>>10)
#define GIB_TO_MIB(Size)          ((Size)<<10)
#define MIB_TO_GIB(Size)          ((Size)>>10)


#define BYTES_TO_TIB_DOUBLE(Size) (((double)Size)/((double)BYTES_IN_MEBIBYTE))/((double)BYTES_IN_MEBIBYTE)
#define BYTES_TO_MIB_DOUBLE(Size) (((double)Size)/((double)BYTES_IN_MEBIBYTE))
#define BYTES_TO_GIB_DOUBLE(Size) (((double)Size)/((double)BYTES_IN_GIBIBYTE))

#define BYTES_TO_TB_DOUBLE(Size) (((double)Size)/((double)BYTES_IN_MEGABYTE))/((double)BYTES_IN_MEGABYTE)
#define BYTES_TO_MB_DOUBLE(Size) (((double)Size)/((double)BYTES_IN_MEGABYTE))
#define BYTES_TO_GB_DOUBLE(Size) (((double)Size)/((double)BYTES_IN_GIGABYTE))

#define GIB_TO_GB(Size) (BYTES_TO_GB(GIB_TO_BYTES(Size)))
#define GB_TO_GIB(Size) (BYTES_TO_GIB(GB_TO_BYTES(Size)))
#define GB_TO_BYTES(Size)         ((Size)*1000*1000*1000)

#define BYTES_IN_TERABYTE (BYTES_IN_GIGABYTE * BYTES_IN_KB)
#define BYTES_IN_GIGABYTE (BYTES_IN_MEGABYTE * BYTES_IN_KB)
#define BYTES_IN_MEGABYTE (BYTES_IN_KB * BYTES_IN_KB)
#define BYTES_IN_KB (UINT64)(1000)

#define BYTES_IN_TEBIBYTE (BYTES_IN_GIBIBYTE * BYTES_IN_KIB)
#define BYTES_IN_GIBIBYTE (BYTES_IN_MEBIBYTE * BYTES_IN_KIB)
#define BYTES_IN_MEBIBYTE (BYTES_IN_KIB * BYTES_IN_KIB)
#define BYTES_IN_KIB (UINT64)(1 << 10)

#define MEBIBYTES_IN_GIBIBYTE (1024)

#define GET_VOID_PTR_OFFSET(ptr, n) ((VOID *) ((UINT8 *) (ptr) + (n)))

#define IS_WHITE_UNICODE(Char) ((Char) == L' ' || (Char) == L'\t' || (Char) == L'\r' || (Char) == L'\n')
#define IS_WHITE_ASCII(Char) ((Char) == ' ' || (Char) == '\t' || (Char) == '\r' || (Char) == '\n')

#define IS_IN_RANGE(X, A, B) ((X) >= (A) && (X) <= (B))

#define FREE_HII_POINTER(pBuffer) { \
    if ((VOID *) pBuffer != NULL) { \
    FreePool((VOID*) pBuffer); \
    pBuffer = (UINT64) NULL; \
    } \
};

#define FREE_POOL_SAFE(pBuffer) { \
  if (pBuffer != NULL) { \
    FreePool((VOID *)pBuffer); \
    pBuffer = NULL; \
  } \
};

#define FREE_ALIGNED_POOL_SAFE(pBuffer, NumberOfPages) { \
  if (pBuffer != NULL) { \
    FreeAlignedPages((VOID *)pBuffer, NumberOfPages); \
    pBuffer = NULL; \
  } \
};

/**
  Persist the first error encountered.
  @param[in,out] ReturnCode
     Return code to be returned if indicating an error
  @param[in] ReturnCodeNew
     Return code to be returned if ReturnCode does not indicate error
**/
#define KEEP_ERROR(ReturnCode, ReturnCodeNew) \
  ReturnCode = (ReturnCode > EFI_SUCCESS) ? ReturnCode : ReturnCodeNew;

/**
  Linked lists iterators
**/
#define LIST_FOR_EACH(Entry, ListHead) \
  for (Entry = (ListHead)->ForwardLink; Entry != (ListHead); Entry = Entry->ForwardLink)
#define LIST_FOR_EACH_REVERSE(Entry, ListHead) \
  for (Entry = (ListHead)->BackLink; Entry != (ListHead); Entry = Entry->BackLink)
#define LIST_FOR_EACH_SAFE(Entry, NextEntry, ListHead) \
  for (Entry = (ListHead)->ForwardLink, NextEntry = Entry->ForwardLink; \
      Entry != (ListHead); Entry = NextEntry, NextEntry = Entry->ForwardLink)
/**
  Iterates over list entries in forward direction until element of number index is met
  First element has index 0
**/
#define LIST_FOR_UNTIL_INDEX(Entry, ListHead, Index, Iterator) \
  for (Entry = (ListHead)->ForwardLink, Iterator = 0; Entry != (ListHead) && Iterator < Index; Entry = Entry->ForwardLink, Iterator++)
/**
  The maximum buffer size for the Print function.
  This value depends on the UDK, it may change with a new version.
**/
#define PCD_UEFI_LIB_MAX_PRINT_BUFFER_SIZE 320

/**
  Macros to get the number of days in a particular year
 **/
#define IS_LEAP_YEAR(Year)              (!((Year) % 4) && (((Year) % 100) || !((Year) % 400)))
#define DAYS_IN_YEAR(Year)              (IS_LEAP_YEAR(Year) ? 366 : 365)

/**
  Get a variable from UEFI RunTime services.

  This will use the Runtime Services call GetVariable to get a variable.

  @param VarName                The name of the variable in question
  @param VendorGuid             A unique identifier for the vendor
  @param BufferSize             Pointer to the UINTN size of Buffer
  @param Buffer                 Pointer buffer to get variable value into

  @retval EFI_SUCCESS           The variable's value was retrieved successfully
  @retval other                 An error occurred
**/
#ifndef OS_BUILD
#define GET_VARIABLE(VarName,VendorGuid,BufferSize,Buffer)      \
  (gRT->GetVariable((CHAR16*)VarName,                           \
  &VendorGuid,                                                  \
  0,                                                            \
  BufferSize,                                                   \
  Buffer))
#else
#define GET_VARIABLE(VarName,VendorGuid,BufferSize,Buffer) preferences_get_var(VarName,VendorGuid,Buffer,BufferSize)
#define GET_VARIABLE_STR(VarName,VendorGuid,BufferSize,Buffer) preferences_get_var_string_wide(VarName,VendorGuid,Buffer,BufferSize)
#endif

/**
  Set a Non-Volatile UEFI RunTime variable.

  This will use the Runtime Services call SetVariable to set a non-volatile variable.

  @param VarName                The name of the variable in question
  @param VendorGuid             A unique identifier for the vendor
  @param BufferSize             UINTN size of Buffer
  @param Buffer                 Pointer to value to set variable to

  @retval EFI_SUCCESS           The variable was changed successfully
  @retval other                 An error occurred
**/
#ifndef OS_BUILD
#define SET_VARIABLE_NV(VarName,VendorGuid,BufferSize,Buffer)  \
  (gRT->SetVariable((CHAR16*)VarName,                          \
  &VendorGuid,                                            \
  EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS,      \
  BufferSize,                                                     \
  (VOID*)Buffer))
#else
#define SET_VARIABLE_NV(VarName,VendorGuid,BufferSize,Buffer) preferences_set_var(VarName,VendorGuid,(void*)Buffer,BufferSize)
#define SET_STR_VARIABLE_NV(VarName,VendorGuid,VarVal) preferences_set_var_string_wide(VarName,VendorGuid, VarVal)
#endif

/**
  Returns the value of the environment variable with the given name.

  @param[in] pVarName Unicode name of the variable to retrieve

  @retval NULL if the shell protocol could not be located or if the variable is not defined in the system
  @retval pointer to the Unicode string containing the variable value
**/
CHAR16 *
GetEnvVariable(
  IN     CHAR16 *pVarName
  );

/**
  Checks if the Config Protocol version is right.

  @param[in] *pConfigProtocol, instance of the protocol to check

  @retval EFI_SUCCESS if the version matches.
  @retval EFI_INVALID_PARAMETER if the passed parameter equals to NULL.
  @retval EFI_INCOMPATIBLE_VERSION when the version is wrong.
**/
EFI_STATUS
CheckConfigProtocolVersion(
  IN     EFI_NVMDIMM_CONFIG_PROTOCOL *pConfigProtocol
  );

/**
  Generates namespace type string, caller must free it

  @param[in] Type, value corresponding to namespace type.

  @retval Pointer to type string
**/
CHAR16*
NamespaceTypeToString(
  IN     UINT8 Type
  );

/**
  Generates pointer to string with value corresponding to health state
  Caller is responsible for FreePool on this pointer
**/
CHAR16*
NamespaceHealthToString(
  IN     UINT16 Health
  );

/**
  Check if LIST_ENTRY list is initialized

  @param[in] ListHead list head

  @retval BOOLEAN list initialization status
**/
BOOLEAN
IsListInitialized(
  IN     LIST_ENTRY ListHead
  );

/**
  Calculate checksum using Fletcher64 algorithm and compares it at the given offset.
  The length parameter must be aligned to 4 (32bit).

  @param[in] pAddress Starting address of area to calculate checksum on
  @param[in] Length Length of area over which checksum is calculated
  @param[in, out] pChecksum, the pointer where the checksum lives in
  @param[in] Insert, flag telling if the checksum should be inserted at the specified address or just compared to it

  @retval TRUE if the compared checksums are equal
  @retval FALSE if the checksums differ or the input parameters are invalid
    (a NULL was passed or the length is not aligned)
**/
BOOLEAN
ChecksumOperations(
  IN     VOID *pAddress,
  IN     UINT64 Length,
  IN OUT UINT64 *pChecksum,
  IN     BOOLEAN Insert
  );

/**
  Compares the two provided 128bit unsigned ints.

  @param[in] LeftValue is the first 128bit uint.
  @param[in] RightValue is the second 128bit uint.

  @retval -1 when the LeftValue is smaller than
    the RightValue
  @retval 0 when the provided values are the same
  @retval 1 when the LeftValue is bigger than
    the RithValue
**/
INT8
CompareUint128(
  IN     UINT128 LeftValue,
  IN     UINT128 RightValue
  );

/**
  The Print function is not able to print long strings.
  This function is dividing the input string to safe lengths
  and prints all of the parts.

  @param[in] pString - the Unicode string to be printed
**/
VOID
LongPrint(
  IN     CHAR16 *pString
  );

/**
  Tokenize a string by the specified delimiter and update
  the input to the remainder.
  NOTE:  Returned token needs to be freed by the caller
**/
CHAR16 *StrTok(CHAR16 **input, CONST CHAR16 delim);

/**
  Tokenize provided ASCII string

  @param[in] Input     Input string
  @param[in] Delimiter Delimiter character

  @retval Pointer to token string
**/
CHAR8*
AsciiStrTok(
    IN     CHAR8** Input,
    IN     CONST CHAR8 Delimiter
  );

/**
  Split a string by the specified delimiter and return the split string as a string array.

  The caller is responsible for a memory deallocation of the returned array and its elements.

  @param[in] pInput the input string to split
  @param[in] Delimiter delimiter to split the string
  @param[out] pArraySize array size will be put here

  @retval NULL at least one of parameters is NULL or memory allocation failure
  @retval the split input string as an array
**/
CHAR16 **
StrSplit(
  IN     CHAR16 *pInput,
  IN     CHAR16 Delimiter,
     OUT UINT32 *pArraySize
  );

/**
  Split an ASCII string by the specified delimiter and return the split string as a string array.

  The caller is responsible for a memory deallocation of the returned array and its elements.

  @param[in] pInput the input string to split
  @param[in] Delimiter delimiter to split the string
  @param[out] pArraySize array size will be put here

  @retval NULL at least one of parameters is NULL or memory allocation failure
  @retval the split input string as an array
**/
CHAR8 **
AsciiStrSplit(
  IN     CHAR8 *pInput,
  IN     CHAR8 Delimiter,
     OUT UINT32 *pArraySize
  );

/**
  First free elements of array and then free the array

  @param[in,out] ppStringArray array of strings
  @param[in] ArraySize number of strings
**/
VOID
FreeStringArray(
  IN OUT CHAR16 **ppStringArray,
  IN     UINT32 ArraySize
  );

/**
  Open the specified protocol.
  If the user does not provide a handle, the function will try
  to match the driver or the controller handle based on the
  provided protocol GUID.
  No need to call close protocol because of the way it's opened.

  @param[in] Guid is the EFI GUID of the protocol we want to open.
  @param[out] ppProtocol is the pointer to a pointer where the opened
    protocol instance address will be returned.
  @param[in] pHandle a handle that we want to open the protocol on. OPTIONAL

  @retval EFI_SUCCESS if everything went successfully.
  @retval EFI_INVALID_ARGUMENT if ppProtocol is NULL.

  Other return values from functions:
    getControllerHandle
    getDriverHandle
    gBS->OpenProtocol
**/
EFI_STATUS
OpenNvmDimmProtocol(
  IN     EFI_GUID Guid,
     OUT VOID **ppProtocol,
  IN     EFI_HANDLE pHandle OPTIONAL
  );

/**
  Return a first found handle for specified protocol.

  @param[in] pProtocolGuid protocol that EFI handle will be found for.
  @param[out] pDriverHandle is the pointer to the result handle.

  @retval EFI_INVALID_PARAMETER if one or more input parameters are NULL.
  @retval all of the LocateHandleBuffer return values.
**/
EFI_STATUS
GetDriverHandle(
  IN     EFI_GUID *pProtocolGuid,
     OUT EFI_HANDLE *pDriverHandle
  );

/**
  Open file or create new file.

  @param[in] pArgFilePath path to a file that will be opened
  @param[out] pFileHandle output handler
  @param[in, optional] pCurrentDirectory is the current directory path to where
    we should start to search for the file.
  @param[in] CreateFileFlag TRUE to create new file or FALSE to open
    existing file

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER pFilePath is NULL or empty or pFileHandle is NULL
  @retval EFI_PROTOCOL_ERROR if there is no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
**/
EFI_STATUS
OpenFile(
  IN     CHAR16 *pArgFilePath,
     OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag
  );

/**
  Open file or create new file based on device path protocol.

  @param[in] pArgFilePath Pointer to path to a file that will be opened
  @param[in] pDevicePath Pointer to instance of device path protocol
  @param[in] CreateFileFlag TRUE to create new file or FALSE to open
    existing file
  @param[out] pFileHandle Output file handler

  @retval EFI_SUCCESS File opened or created
  @retval EFI_INVALID_PARAMETER Input parameter is invalid
  @retval Others From LocateDevicePath, OpenProtocol, OpenVolume and Open
**/
EFI_STATUS
OpenFileByDevice(
  IN     CHAR16 *pArgFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  IN     BOOLEAN CreateFileFlag,
     OUT EFI_FILE_HANDLE *pFileHandle
  );

/**
  Converts the dimm health state reason to its  HII string equivalent
  @param[in] @param[in] HiiHandle - handle for hii
  @param[in] HealthStateReason The health state reason to be converted into its HII string
  @param[out] ppHealthStateStr A pointer to the HII health state string. Dynamically allocated memory and must be released by calling function.

  @retval EFI_OUT_OF_RESOURCES if there is no space available to allocate memory for string
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
ConvertHealthStateReasonToHiiStr(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT16 HealthStatusReason,
  OUT CHAR16 **ppHealthStatusReasonStr
);

/**
  Converts the dimm Id to its  HII string equivalent
  @param[in] pRegionInfo The Region info with DimmID and Dimmcount its HII string
  @param[out] ppDimmIdStr A pointer to the HII DimmId string. Dynamically allocated memory and must be released by calling function.

  @retval EFI_OUT_OF_RESOURCES if there is no space available to allocate memory for string
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
  ConvertDimmIdToDimmListStr(
    IN     REGION_INFO *pRegionInfo,
    OUT CHAR16 **ppDimmIdStr
  );


/**
  Open file handle of root directory from given path

  @param[in] pDevicePath - path to file
  @param[out] pFileHandle - root directory file handle

**/
EFI_STATUS
OpenRootFileVolume(
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT EFI_FILE_HANDLE *pRootDirHandle
  );

/**
  Returns the size of the specified file.

  @param[in] FileHandle - handle to the opened file that we want to get the size for.
  @param[out] pFileSize - the result file size on bytes.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one of the input parameters is a NULL.

  Other return values associated with the GetInfo callback.
**/
EFI_STATUS
GetFileSize(
  IN     EFI_FILE_HANDLE FileHandle,
     OUT UINT64 *pFileSize
  );

/**
  Return the NvmDimmController handle.

  @param[out] pControllerHandle is the pointer to the result handle.

  @retval EFI_INVALID_PARAMETER if the pControllerHandle is NULL.
  @retval all of the LocateHandleBuffer return values.
**/
EFI_STATUS
GetControllerHandle(
     OUT EFI_HANDLE *pControllerHandle
  );

/**
  Convert all Interleave settings to string
  WARNING! *ppIoString can be reallocated. Calling function is responsible for its freeing.
  Additionally *ppIoString must be dynamically allocated.

  @param[in] PersistentSize - Persistent size of interleave set in DIMM
  @param[in] NumberOfInterleavedDimms - Number of interleaved DIMMs
  @param[in] ImcInterleaving - iMC interleaving bit map
  @param[in] ChannelInterleaving - Channel interleaving bit map

  @param[out] ppString - output string.
**/
VOID
InterleaveSettingsToString(
  IN     UINT64 PersistentSize,
  IN     UINT8 NumberOfInterleavedDimms,
  IN     UINT8 ImcInterleaving,
  IN     UINT8 ChannelInterleaving,
     OUT CHAR16 **ppString
  );

/**
  Convert Channel Interleaving value to output settings string

  @param[in] Interleaving - Channel Interleave BitMask

  @retval appropriate string
  @retval NULL - if Interleaving value is incorrect
**/
CONST CHAR16 *
ParseChannelInterleavingValue(
  IN     UINT8 Interleaving
  );

/**
  Convert iMC Interleaving value to output settings string

  @param[in] Interleaving - iMC Interleave BitMask

  @retval appropriate string
  @retval NULL - if Interleaving value is incorrect
**/
CONST CHAR16 *
ParseImcInterleavingValue(
  IN     UINT8 Interleaving
  );

/**
  Appends a formatted Unicode string to a Null-terminated Unicode string

  This function appends a formatted Unicode string to the Null-terminated
  Unicode string specified by String.   String is optional and may be NULL.
  Storage for the formatted Unicode string returned is allocated using
  AllocatePool().  The pointer to the appended string is returned.  The caller
  is responsible for freeing the returned string.

  This function also calls FreePool on the old pString buffer if it is not NULL.
  So the caller does not need to free the previous buffer.

  If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
  If FormatString is NULL, then ASSERT().
  If FormatString is not aligned on a 16-bit boundary, then ASSERT().

  @param[in] String         A Null-terminated Unicode string.
  @param[in] FormatString   A Null-terminated Unicode format string.
  @param[in] ...            The variable argument list whose contents are
                            accessed based on the format string specified by
                            FormatString.

  @retval NULL    There was not enough available memory.
  @return         Null-terminated Unicode string is that is the formatted
                  string appended to String.
**/
CHAR16*
EFIAPI
CatSPrintClean(
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  ...
  );

/**
  Function that allows for nicely formatted HEX & ASCII debug output.
  It can be used to inspect memory objects without a need for debugger

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/
VOID
HexDebugPrint(
    IN     VOID *pBuffer,
    IN     UINT32 Bytes
  );

/**
  Function that allows for nicely formatted HEX & ASCII console output.
  It can be used to inspect memory objects without a need for debugger or dumping raw DIMM data.

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/
VOID
HexPrint(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
  );

/**
  Case Insensitive StrCmp

  @param[in] pFirstString - first string for comparison
  @param[in] pSecondString - second string for comparison

  @retval Negative number if strings don't match and pFirstString < pSecondString
  @retval 0 if strings match
  @retval Positive number if strings don't match and pFirstString > pSecondString
**/
INTN
StrICmp(
  IN CONST CHAR16 *pFirstString,
  IN CONST CHAR16 *pSecondString
  );

/**
  Checks if all of the DIMMS are healthy.

  @param[out] pDimmsHeathy is the pointer to a BOOLEAN value,
    where the result status will be stored.
    If at least one DIMM status differs from healthy this
    will equal FALSE.

  @retval EFI_SUCCESS if there were no problems
  @retval EFI_INVALID_PARAMETER if pDimmStatus is NULL

  Other return values from functions:
    HealthProtocol->GetHealthStatus
    OpenNvmDimmProtocol
    getControllerHandle
**/
EFI_STATUS
CheckDimmsHealth(
     OUT BOOLEAN *pDimmsStatus
  );

/**
  Checks if the user-inputted desired ARS status matches with the
  current system-wide ARS status.

  @param[in] DesiredARSStatus Desired value of the ARS status to match against
  @param[out] pARSStatusMatched Pointer to a boolean value which shows if the
              current system ARS status matches the desired one.

  @retval EFI_SUCCESS if there were no problems
  @retval EFI_INVALID_PARAMETER if one of the input parameters is a NULL, or an invalid value.
**/
EFI_STATUS
MatchCurrentARSStatus(
  IN     UINT8 DesiredARSStatus,
     OUT BOOLEAN *pARSStatusMatched
  );

/**
  Function to write a line of unicode text to a file.

  If Handle is NULL, return error.
  If Buffer is NULL, return error.

  @param[in] Handle FileHandle to write to
  @param[in] Buffer Buffer to write

  @retval EFI_SUCCESS The data was written.
  @retval other Error codes from Write function.
**/
EFI_STATUS
EFIAPI
WriteAsciiLine(
  IN     EFI_FILE_HANDLE Handle,
  IN     VOID          *pBuffer
  );

/**
  Try to find a sought pointer in an array

  @param[in] pPointersArray Array of pointers
  @param[in] PointersNum Number of pointers in array
  @param[in] pSoughtPointer Sought pointer

  @retval TRUE if pSoughtPointer has been found in the array
  @retval FALSE otherwise
**/
BOOLEAN
IsPointerInArray(
  IN     VOID *pPointersArray[],
  IN     UINT32 PointersNum,
  IN     VOID *pSoughtPointer
  );

/**
  Check if given language is supported (is on supported language list)

  @param[in] pSupportedLanguages - list of supported languages
  @param[in] pLanguage - language to verify if is supported
  @param[in] Rfc4646Language - language abbreviation is compatible with Rfc4646 standard

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_UNSUPPORTED - language is not supported
  @retval EFI_SUCCESS Is supported
**/
EFI_STATUS
CheckIfLanguageIsSupported(
  IN    CONST CHAR8 *pSupportedLanguages,
  IN    CONST CHAR8 *pLanguage,
  IN    BOOLEAN Rfc4646Language
  );

/**
  Convert a character to upper case

  @param[in] InChar - character to up

  @retval - upper character
**/
CHAR16 ToUpper(
  IN      CHAR16 InChar
  );

/**
  Calculate a power of base.

  @param[in] Base base
  @param[in] Exponent exponent

  @retval Base ^ Exponent
**/
UINT64
Pow(
  IN     UINT64 Base,
  IN     UINT32 Exponent
  );

/**
  Read file to given buffer

  * WARNING * caller is responsible for freeing ppFileBuffer

  @param[in] pFilePath - file path
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the physical device
                          or logical device. The device path describes the location of the device the handle is for.
  @param[in] MaxFileSize - if file is bigger skip read and return error
  @param[out] pFileSize - number of bytes written to buffer
  @param[out] ppFileBuffer - output buffer

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NOT_STARTED Test was not executed
  @retval EFI_OUT_OF_RESOURCES if memory allocation fails.
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
ReadFile(
  IN      CHAR16 *pFilePath,
  IN      EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  IN      CONST UINT64  MaxFileSize,
     OUT  UINT64 *pFileSize,
     OUT  VOID **ppFileBuffer
  );

/**
  Read ASCII line from a file.

  The function ignores carriage return chars.

  @param FileHandle handle to a file
  @param pLine output buffer that will be filled with read line
  @param LineSize size of pLine buffer
  @param pEndOfFile output variable to report about end of file

  @retval EFI_SUCCESS
  @retval EFI_BUFFER_TOO_SMALL when pLine buffer is too small
  @retval EFI_INVALID_PARAMETER pLine or pEndOfFile is NULL
**/
EFI_STATUS
ReadAsciiLineFromFile(
  IN     EFI_FILE_HANDLE FileHandle,
     OUT CHAR8 *pLine,
  IN     INT32 LineSize,
     OUT BOOLEAN *pEndOfFile
  );

/**
  Clear memory containing string

  @param[in] pString - pointer to string to be cleared
**/
VOID
CleanStringMemory(
  IN    CHAR8 *pString
  );

/**
  Clear memory containing unicode string

  @param[in] pString - pointer to string to be cleared
**/
VOID
CleanUnicodeStringMemory(
  IN    CHAR16 *pString
  );

/**
  Get linked list size

  @param[in] pListHead   List head
  @parma[out] pListSize  Counted number of items in the list

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER At least one of the input parameters equals NULL
**/
EFI_STATUS
GetListSize(
  IN     LIST_ENTRY *pListHead,
     OUT UINT32 *pListSize
  );

/**
  Implementation of public algorithm to calculate least common multiple of two numbers

  @param[in] A  First number
  @param[in] B  Second number

  @retval Least common multiple
**/
UINT64
FindLeastCommonMultiple(
  IN     UINT64 A,
  IN     UINT64 B
  );

/**
  Trim white spaces from the begin and end of string

  @param[in, out] pString Null terminated string that will be trimmed

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameters is NULL
  @retval EFI_BAD_BUFFER_SIZE Size of input string is bigger than MAX_INT32
**/
EFI_STATUS
TrimString(
  IN OUT CHAR16 *pString
  );

/**
  Removes all white spaces from string

  @param[in] pInputBuffer Pointer to string to remove white spaces
  @param[out] pOutputBuffer Pointer to string with no white spaces
  @param[in, out] OutputBufferLength On input, length of buffer (in CHAR16),
                  on output, length of string with no white spaces, without null-terminator

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL or string length is 0
  @retval EFI_BUFFER_TOO_SMALL Output buffer is too small
**/
EFI_STATUS
RemoveWhiteSpaces(
  IN     CHAR8 *pInputBuffer,
     OUT CHAR8 *pOutputBuffer,
  IN OUT UINT64 *pOutputBufferLength
  );

/**
  Convert Last Shutdown Status to string

  @param[in] LastShutdownStatus structure

  @retval CLI string representation of last shudown status
**/
CHAR16*
LastShutdownStatusToStr(
  IN     LAST_SHUTDOWN_STATUS_COMBINED LastShutdownStatus
  );

/**
  Convert modes supported to string

  @param[in] ModesSupported, bits define modes supported

  @retval CLI string representation of modes supported
**/
CHAR16*
ModesSupportedToStr(
  IN     UINT8 ModesSupported
  );

/**
  Convert Security Capabilities to string

  @param[in] SecurityCapabilities, bits define capabilities

  @retval CLI string representation of security capabilities
**/
CHAR16*
SecurityCapabilitiesToStr(
  IN     UINT8 SecurityCapabilities
  );

/**
  Convert Dimm security state to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Dimm security state

  @retval String representation of Dimm's security state
**/
CHAR16*
SecurityToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 SecurityState
  );

/**
  Convert ARS status value to its respective string

  @param[in] ARS status value

  @retval CLI string representation of ARS status
**/
CHAR16*
ARSStatusToStr(
  IN     UINT8 ARSStatus
  );

/**
  Convert dimm's boot status bitmask to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] BootStatusBitmask, bits define the boot status

  @retval CLI/HII string representation of dimm's boot status
**/
CHAR16*
BootStatusBitmaskToStr(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT16 BootStatusBitmask
  );

/**
  Convert string value to double

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] pString String value to convert
  @param[out] pOutValue Target double value

  @retval EFI_INVALID_PARAMETER No valid value inside
  @retval EFI_SUCCESS Conversion successful
**/
EFI_STATUS
StringToDouble(
  IN     EFI_HANDLE HiiHandle,
  IN     CHAR16 *pString,
     OUT double *pOutValue
  );

/**
  Compare a PackageSparing capability, encryption, soft SKU capabilities and SKU mode types.

  @param[in] PackageSparingCapable1 - first PackageSparingCapable to compare
  @param[in] PackageSparingCapable2 - second PackageSparingCapable to compare
  @param[in] SkuInformation1 - first SkuInformation to compare
  @param[in] SkuInformation2 - second SkuInformation to compare

  @retval NVM_SUCCESS - if everything went fine
  @retval NVM_ERR_DIMM_SKU_PACKAGE_SPARING_MISMATCH - if Package Sparing conflict occurred
  @retval NVM_ERR_DIMM_SKU_MODE_MISMATCH - if mode conflict occurred
  @retval NVM_ERR_DIMM_SKU_SECURITY_MISMATCH - if security mode conflict occurred
**/
NvmStatusCode
SkuComparison(
  IN     BOOLEAN PackageSparingCapable1,
  IN     BOOLEAN PackageSparingCapable2,
  IN     UINT32 SkuInformation1,
  IN     UINT32 SkuInformation2
  );

/**
  Check if SKU conflict occurred.
  Any mixed modes between DIMMs are prohibited on a platform.

  @param[in] pDimmInfo1 - first DIMM_INFO to compare SKU mode
  @param[in] pDimmInfo2 - second DIMM_INFO to compare SKU mode
  @param[out] pSkuModeMismatch - pointer to a BOOLEAN value that will
    represent result of comparison

  @retval - Appropriate CLI return code
**/
EFI_STATUS
IsSkuModeMismatch(
  IN     DIMM_INFO *pDimmInfo1 OPTIONAL,
  IN     DIMM_INFO *pDimmInfo2 OPTIONAL,
     OUT BOOLEAN *pSkuModeMismatch
  );

/**
  Convert type to string

  @param[in] MemoryType, integer define type

  @retval CLI string representation of memory type
**/
CHAR16*
MemoryTypeToStr(
  IN     UINT8 MemoryType
  );

/**
  Sort Linked List by using Bubble Sort.

  @param[in, out] LIST HEAD to sort
  @param[in] Compare Pointer to function that is needed for items comparing. It should return:
                     -1 if "first < second"
                     0  if "first == second"
                     1  if "first > second"

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
BubbleSortLinkedList(
  IN OUT LIST_ENTRY *pList,
  IN     INT32 (*Compare) (VOID *first, VOID *second)
  );

/**
  Sort an array by using Bubble Sort.

  @param[in, out] pArray Array to sort
  @param[in] Count Number of items in array
  @param[in] ItemSize Size of item in bytes
  @param[in] Compare Pointer to function that is needed for items comparing. It should return:
                     -1 if "first < second"
                     0  if "first == second"
                     1  if "first > second"

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
BubbleSort(
  IN OUT VOID *pArray,
  IN     UINT32 Count,
  IN     UINT32 ItemSize,
  IN     INT32 (*Compare) (VOID *first, VOID *second)
  );

/**
  Convert from units type to a string

  @param[in] UnitsToDisplay The type of units to be used

  @retval String representation of the units type
**/
CHAR16*
UnitsToStr(
  IN     UINT16 UnitsToDisplay
  );

/**
  Convert last firmware update status to string.
  The caller function is obligated to free memory of the returned string.

  @param[in] Last Firmware update status value to convert

  @retval output string or NULL if memory allocation failed
**/
CHAR16 *
LastFwUpdateStatusToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 LastFwUpdateStatus
  );

/**
  Determines if an array, whose size is known in bytes has all elements as zero

  @param[in] pArray    Pointer to the input array
  @param[in] ArraySize Array size in bytes
  @param[out] pAllElementsZero Pointer to a boolean that stores the
    result whether all array elements are zero

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
AllElementsInArrayZero(
  IN OUT VOID *pArray,
  IN     UINT32 ArraySize,
     OUT BOOLEAN *pAllElementsZero
  );

/**
  Endian swap a uint32 value
  @param[in] OrigVal Value to modify

  @retval Value with the endian swap
**/
UINT32
EndianSwapUint32(
  IN UINT32 OrigVal
  );

/**
  Endian swap a uint16 value
  @param[in] OrigVal Value to modify

  @retval Value with the endian swap
**/
UINT16
EndianSwapUint16(
  IN UINT16 OrigVal
  );

/**
  Converts EPOCH time in number of seconds into a human readable time string
  @param[in] TimeInSesconds Number of seconds (EPOCH time)

  @retval Human readable time string
**/
CHAR16 *GetTimeFormatString (
    IN UINT64 TimeInSeconds
    );

/**
  Convert goal status bitmask to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Status bits that define the goal status

  @retval CLI/HII string representation of goal status
**/
CHAR16*
GoalStatusToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 Status
  );

/**
  Poll long operation status

  Polls the status of the background operation on the dimm.

  @param [in] pNvmDimmConfigProtocol Pointer to the EFI_NVMDIMM_CONFIG_PROTOCOL instance
  @param [in] DimmId Dimm ID of the dimm to poll status
  @param [in] OpcodeToPoll Specify an opcode to poll, 0 to poll regardless of opcode
  @param [in] SubOpcodeToPoll Specify an opcode to poll
  @param [in] Timeout for the background operation
**/
EFI_STATUS
PollLongOpStatus(
  IN     EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 DimmId,
  IN     UINT8 OpcodeToPoll OPTIONAL,
  IN     UINT8 SubOpcodeToPoll OPTIONAL,
  IN     UINT64 Timeout
  );

EFI_STATUS
GetNSLabelMajorMinorVersion(
  IN     UINT32 NamespaceLabelVersion,
     OUT UINT16 *pMajor,
     OUT UINT16 *pMinor
  );

/**
Get basic information about the host server

@param[out] pHostServerInfo pointer to a HOST_SERVER_INFO struct

@retval EFI_SUCCESS Success
@retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
GetHostServerInfo(
   OUT HOST_SERVER_INFO *pHostServerInfo
);
#endif /** _UTILITY_H_ **/
