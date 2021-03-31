/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <NvmTypes.h>
#include <Uefi.h>
#include "NvmInterface.h"
#include "NvmHealth.h"

#ifdef _MSC_VER
int _fltused();
#endif

#define EFI_FILE_MODE_BINARY  0x8000000000000064ULL

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


#define SOCKET_ID_ALL MAX_UINT16

#define BLOCKSIZE_4K 4096

#define SPD_INTEL_VENDOR_ID 0x8980
#define SPD_DEVICE_ID 0x0000
#define SPD_DEVICE_ID_10 0x097A
#define SPD_DEVICE_ID_15 0x097B
#define SPD_DEVICE_ID_20 0x097C

#define CONTROLLER_REVISION_BASE_STEP_MASK 0x30
#define CONTROLLER_REVISION_METAL_STEP_MASK 0x03

#define CONTROLLER_REVISION_A_STEP 0x00
#define CONTROLLER_REVISION_S_STEP 0x10
#define CONTROLLER_REVISION_B_STEP 0x20
#define CONTROLLER_REVISION_C_STEP 0x30

#define CONTROLLER_REVISION_A_STEP_STR L"A"
#define CONTROLLER_REVISION_S_STEP_STR L"S"
#define CONTROLLER_REVISION_B_STEP_STR L"B"
#define CONTROLLER_REVISION_C_STEP_STR L"C"

#define CONTROLLER_STEPPING_UNKNOWN_STR L"Unknown stepping"

#define MAX_CONFIG_DUMP_FILE_SIZE OUT_MB_SIZE

// Picking a theoretical max string length so we don't run out of
// memory and kill something that is continuously running (our driver).
// Also it's good development practice.
#define MAX_LINE_CHAR_LENGTH 400
#define MAX_LINE_BYTE_LENGTH (MAX_LINE_CHAR_LENGTH * sizeof(CHAR8))
#define MAX_STRING_LENGTH (16384)

#define COUNT_TO_INDEX_OFFSET 1
#define FIRST_CHAR_INDEX 0

#define UTF_16_BOM L'\xFEFF'

#define SKU_MEMORY_MODE_FLAG      (BIT0)
#define SKU_APP_DIRECT_MODE_FLAG  (BIT2)
#define SKU_MODES_MASK  (SKU_MEMORY_MODE_FLAG | SKU_APP_DIRECT_MODE_FLAG)

#define SKU_ENCRYPTION_MASK                  (BIT17)

//Long operation timer defines
#define LONG_OP_POLL_EVENT_TIMER 0                               //timer used for polling
#define LONG_OP_POLL_TIMER_INTERVAL EFI_TIMER_PERIOD_SECONDS(1)  //defines how often to poll long op status
#define LONG_OP_POLL_EVENT_TIMEOUT 1                             //timer used for timeout event
#define LONG_OP_POLL_EVENT_SIZE 2                                //total number of events to wait for

#define FW_UPDATE_TIMEOUT_SECONDS 20
#define LONG_OP_FW_UPDATE_TIMEOUT EFI_TIMER_PERIOD_SECONDS(FW_UPDATE_TIMEOUT_SECONDS)

//Firmware update opcodes to match long operation status
#define FW_UPDATE_OPCODE    0x09
#define FW_UPDATE_SUBOPCODE 0x00

// Product name string values
#define PMEM_MODULE_STR             L"PMem module"
#define PMEM_MODULES_STR            PMEM_MODULE_STR L"s"
#define PMEM_MODULE_PASCAL_CASE_STR L"PMemModule"

// Last shutdown status string values
#define LAST_SHUTDOWN_STATUS_PM_ADR_STR               L"PM ADR Command Received"
#define LAST_SHUTDOWN_STATUS_PM_S3_STR                L"PM S3 Received"
#define LAST_SHUTDOWN_STATUS_PM_S5_STR                L"PM S5 Received"
#define LAST_SHUTDOWN_STATUS_DDRT_POWER_FAIL_STR      L"DDRT Power Fail Command Received"
#define LAST_SHUTDOWN_STATUS_PMIC_POWER_LOSS_STR      L"PMIC 12V/DDRT 1.2V Power Loss (PLI)"
#define LAST_SHUTDOWN_STATUS_PM_WARM_RESET_STR        L"PM Warm Reset Received"
#define LAST_SHUTDOWN_STATUS_THERMAL_SHUTDOWN_STR     L"Thermal Shutdown Received"
#define LAST_SHUTDOWN_STATUS_FW_FLUSH_COMPLETE_STR    L"Controller's FW State Flush Complete"
#define LAST_SHUTDOWN_STATUS_UNKNOWN_STR              L"Unknown"

// Last shutdown status extended string values
#define LAST_SHUTDOWN_STATUS_VIRAL_INTERRUPT_STR                           L"Viral Interrupt Received"
#define LAST_SHUTDOWN_STATUS_SURPRISE_CLOCK_STOP_INTERRUPT_STR             L"Surprise Clock Stop Received"
#define LAST_SHUTDOWN_STATUS_WRITE_DATA_FLUSH_COMPLETE_STR                 L"Write Data Flush Complete"
#define LAST_SHUTDOWN_STATUS_S4_POWER_STATE_STR                            L"PM S4 Received"
#define LAST_SHUTDOWN_STATUS_PM_IDLE_STR                                   L"PM Idle Received"
#define LAST_SHUTDOWN_STATUS_SURPRISE_RESET_STR                            L"DDRT Surprise Reset Received"
#define LAST_SHUTDOWN_STATUS_ENHANCED_ADR_FLUSH_COMPLETE_STR               L"Extended Flush Complete"
#define LAST_SHUTDOWN_STATUS_ENHANCED_ADR_FLUSH_NOT_COMPLETE_STR           L"Extended Flush Not Complete"
#define LAST_SHUTDOWN_STATUS_ENHANCED_SX_EXTENDED_FLUSH_COMPLETE_STR       L"Sx Extended Flush Complete"
#define LAST_SHUTDOWN_STATUS_ENHANCED_SX_EXTENDED_FLUSH_NOT_COMPLETE_STR   L"Sx Extended Flush Not Complete"

// Memory modes supported string values
#define MODES_SUPPORTED_MEMORY_MODE_STR      L"Memory Mode"
#define MODES_SUPPORTED_APP_DIRECT_MODE_STR  L"App Direct"

// Software triggers enabled string values
#define SW_TRIGGERS_ENABLED_NONE_STR L"None"
#define SW_TRIGGERS_ENABLED_BIT0_STR L"Package Sparing"
#define SW_TRIGGERS_ENABLED_BIT1_STR L"Reserved"
#define SW_TRIGGERS_ENABLED_BIT2_STR L"Fatal Error"
#define SW_TRIGGERS_ENABLED_BIT3_STR L"Percentage Remaining"
#define SW_TRIGGERS_ENABLED_BIT4_STR L"Dirty Shutdown"

// Security capabilities string values
#define SECURITY_CAPABILITIES_ENCRYPTION  L"Encryption"
#define SECURITY_CAPABILITIES_ERASE       L"Erase"
#define SECURITY_CAPABILITIES_NONE        L"None"

// ARS status string values
#define ARS_STATUS_NOT_SUPPORTED_STR L"Not supported in Memory Mode"
#define ARS_STATUS_UNKNOWN_STR       L"Unknown"
#define ARS_STATUS_IDLE_STR          L"Idle"
#define ARS_STATUS_IN_PROGRESS_STR   L"In progress"
#define ARS_STATUS_COMPLETED_STR     L"Completed"
#define ARS_STATUS_ABORTED_STR       L"Aborted"
#define ARS_STATUS_ERROR_STR         L"Error"

// Overwrite DIMM status string values
#define OVERWRITE_DIMM_STATUS_UNKNOWN_STR      L"Unknown"
#define OVERWRITE_DIMM_STATUS_IDLE_STR         L"Idle"
#define OVERWRITE_DIMM_STATUS_IN_PROGRESS_STR  L"In progress"
#define OVERWRITE_DIMM_STATUS_COMPLETED_STR    L"Completed"
#define OVERWRITE_DIMM_STATUS_ABORTED_STR      L"Aborted"
#define OVERWRITE_DIMM_STATUS_ERROR_STR        L"Error"

// Memory type string values
#define MEMORY_TYPE_DCPM_STR     L"Logical Non-Volatile Device"
#define MEMORY_TYPE_DDR4_STR     L"DDR4"
#define MEMORY_TYPE_DDR5_STR     L"DDR5"
#define MEMORY_TYPE_UNKNOWN_STR  L"Unknown"

//Units string values
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

//Namespace health states
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
#define MB_TO_BYTES(Size)         ((Size) * BYTES_IN_MEGABYTE)
#define GIB_TO_BYTES(Size)        ((Size) * BYTES_IN_GIBIBYTE)
#define GB_TO_BYTES(Size)         ((Size) * BYTES_IN_GIGABYTE)
#define TIB_TO_BYTES(Size)        ((Size) * BYTES_IN_TEBIBYTE)
#define TB_TO_BYTES(Size)         ((Size) * BYTES_IN_TERABYTE)
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

#define FREE_CMD_DISPLAY_OPTIONS_SAFE(pCmdOptions) { \
  if (pCmdOptions != NULL) { \
    if(pCmdOptions->pDisplayValues != NULL) { \
      FreePool((VOID *)pCmdOptions->pDisplayValues); \
      pCmdOptions->pDisplayValues = NULL; \
    } \
    FreePool((VOID *)pCmdOptions); \
    pCmdOptions = NULL; \
  } \
};

#ifdef OS_BUILD
#ifdef _MSC_VER
#define CHECK_WIN_ADMIN_PERMISSIONS() { \
if (NVM_SUCCESS != os_check_admin_permissions()) { \
  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_CMD_FAILED_NOT_ADMIN); \
  ReturnCode = EFI_UNSUPPORTED; \
  goto Finish; \
} \
};
#else // MSC_VER
#define CHECK_WIN_ADMIN_PERMISSIONS()
#endif // MSC_VER
#else // OS_BUILD
#define CHECK_WIN_ADMIN_PERMISSIONS()
#endif // OS_BUILD

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
  Iterates over list entries in forward direction and counts the no of elements in list
**/
#define LIST_COUNT(Entry, ListHead, Count) \
  for (Entry = (ListHead)->ForwardLink, Count = 0;Entry != (ListHead); Entry = Entry->ForwardLink, Count++)
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

// Helper macros to streamline the reading of code
// NVDIMM_ERR will print out the line number and file, so no need to have
// specific messages
#define CHECK_RETURN_CODE(ReturnCode, Label)                  \
  do {                                                        \
    if (EFI_ERROR(ReturnCode)) {                              \
      NVDIMM_ERR("Failure on function: %d", ReturnCode);      \
      goto Label;                                             \
    }                                                         \
  } while (0)

// Return if failure
#define CHECK_RESULT(Call, Label)                             \
  do {                                                        \
    ReturnCode = Call;                                        \
    if (EFI_ERROR(ReturnCode)) {                              \
      NVDIMM_ERR("Failure with %s. RC: %d", #Call, ReturnCode); \
      goto Label;                                             \
    }                                                         \
  } while (0)

// Return if failure
#define CHECK_RESULT_SAVE_RETURN_CODE(Call, Label)             \
  do {                                                        \
    TempReturnCode = ReturnCode;                              \
    ReturnCode = Call;                                        \
    if (EFI_ERROR(ReturnCode)) {                              \
      NVDIMM_ERR("Failure with %s. RC: %d", #Call, ReturnCode); \
      goto Label;                                             \
    }                                                         \
    ReturnCode = TempReturnCode;                              \
  } while (0)

// Return if failure
#define CHECK_RESULT_SET_NVM_STATUS(Call, pNvmStatus, NvmStatusCode, Label) \
  do {                                                                      \
    ReturnCode = Call;                                                      \
    if (EFI_ERROR(ReturnCode)) {                                            \
      NVDIMM_ERR("Failure with %s. RC: %d", #Call, ReturnCode);             \
      *pNvmStatus = NvmStatusCode;                                          \
      goto Label;                                                           \
    }                                                                       \
  } while (0)

// Return if success
#define CHECK_RESULT_SUCCESS(Call, Label)                                 \
  do {                                                                    \
    ReturnCode = Call;                                                    \
    if (ReturnCode == EFI_SUCCESS) {                                      \
      goto Label;                                                         \
    }                                                                     \
  } while (0)

// Ignore error code, but print it out
#define CHECK_RESULT_CONTINUE(Call)                                           \
  do {                                                                        \
    EFI_STATUS ReturnCodeTemp = Call;                                         \
    if (EFI_ERROR(ReturnCodeTemp)) {                                          \
      NVDIMM_WARN("Ignoring failure with %s. RC: %d", #Call, ReturnCodeTemp); \
    }                                                                         \
  } while (0)

#define CHECK_RESULT_MALLOC(Pointer, Call, Label)             \
  do {                                                        \
    Pointer = Call;                                           \
    if (Pointer == NULL) {                                    \
      NVDIMM_ERR("Failed to allocate memory to %s", #Pointer); \
      ReturnCode = EFI_OUT_OF_RESOURCES;                      \
      goto Label;                                             \
    }                                                         \
  } while (0)

#define CHECK_RESULT_FILE(Call, Label)                                    \
  do {                                                                    \
    ReturnCode = Call;                                                    \
    if (EFI_ERROR(ReturnCode)) {                                          \
      NVDIMM_ERR("Error in file operation %s", #Call);                    \
      ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_FILE_OPERATION_FAILED); \
      goto Label;                                                         \
    }                                                                     \
  } while (0)

// Go to Label if not true
#define CHECK_NOT_TRUE(Call, Label)                                        \
  do {                                                                    \
    if (TRUE != (Call)) {                                                   \
      NVDIMM_ERR("Statement %s is not true", #Call);                      \
      goto Label;                                                         \
    }                                                                     \
  } while (0)

#define CHECK_NULL_ARG(Argument, Label)                                   \
  do {                                                                    \
    if (NULL == (VOID *)Argument) {                                       \
      NVDIMM_ERR("Argument %s is NULL. Exiting", #Argument);              \
      ReturnCode = EFI_INVALID_PARAMETER;                                 \
      goto Label;                                                         \
    }                                                                     \
  } while (0)


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
#define SET_VARIABLE_NV(VarName,VendorGuid,BufferSize,Buffer) preferences_set_var(VarName,VendorGuid,(void*)Buffer,BufferSize); preferences_flush_the_file()
#define SET_STR_VARIABLE_NV(VarName,VendorGuid,VarVal) preferences_set_var_string_wide(VarName,VendorGuid, VarVal); preferences_flush_the_file()
#endif

/**
  Set a Volatile UEFI RunTime variable.

  This will use the Runtime Services call SetVariable to set a non-volatile variable.

  @param VarName                The name of the variable in question
  @param VendorGuid             A unique identifier for the vendor
  @param BufferSize             UINTN size of Buffer
  @param Buffer                 Pointer to value to set variable to

  @retval EFI_SUCCESS           The variable was changed successfully
  @retval other                 An error occurred
**/
#ifndef OS_BUILD
#define SET_VARIABLE(VarName,VendorGuid,BufferSize,Buffer)  \
  (gRT->SetVariable((CHAR16*)VarName,                          \
  &VendorGuid,                                            \
  EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS,      \
  BufferSize,                                                     \
  (VOID*)Buffer))
#else
#define SET_VARIABLE(VarName,VendorGuid,BufferSize,Buffer) preferences_set_var(VarName,VendorGuid,(void*)Buffer,BufferSize)
#define SET_STR_VARIABLE(VarName,VendorGuid,VarVal) preferences_set_var_string_wide(VarName,VendorGuid, VarVal)
#endif

/**
  Removes all whitespace from before, after, and inside a passed string

  @param[IN, OUT]  buffer - The string to remove whitespace from
**/
VOID RemoveAllWhiteSpace(
  CHAR16* buffer);

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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pConfigProtocol
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
  Generates string from diagnostic output to print and frees the diagnostic structure

  @param[in] Type, pointer to type structure

  @retval Pointer to type string
**/
CHAR16 *
DiagnosticResultToStr(
  IN    DIAG_INFO *pResult
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
    the RightValue
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
  No need to call close protocol because of the way it is opened.

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
  Open file or create new file in binary mode.

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
OpenFileBinary(
  IN     CHAR16 *pArgFilePath,
  OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag
);

/**
  Open file or create new file with the proper flags.

  @param[in] pArgFilePath path to a file that will be opened
  @param[out] pFileHandle output handler
  @param[in, optional] pCurrentDirectory is the current directory path to where
    we should start to search for the file.
  @param[in] CreateFileFlag - TRUE to create new file or FALSE to open
    existing file
  @param[in] binary - use binary open

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER pFilePath is NULL or empty or pFileHandle is NULL
  @retval EFI_PROTOCOL_ERROR if there is no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
**/
EFI_STATUS
OpenFileWithFlag(
  IN     CHAR16 *pArgFilePath,
  OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag,
  BOOLEAN binary
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
  Get Dimm Info by device handle
  Scan the dimm list for a DimmInfo identified by device handle

  @param[in] DeviceHandle Device Handle of the dimm
  @param[in] pDimmInfo Array of DimmInfo
  @param[in] DimmCount Size of DimmInfo array
  @param[out] ppRequestedDimmInfo Pointer to the request DimmInfo struct

  @retval EFI_INVALID_PARAMETER pDimmInfo or pRequestedDimmInfo is NULL
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetDimmInfoByHandle(
  IN     UINT32 DeviceHandle,
  IN     DIMM_INFO *pDimmInfo,
  IN     UINT32 DimmCount,
  OUT DIMM_INFO **ppRequestedDimmInfo
  );

/**
  Converts the Dimm IDs within a region to its  HII string equivalent
  @param[in] pRegionInfo The Region info with DimmID and DimmCount its HII string
  @param[in] pNvmDimmConfigProtocol A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmIdentifier Dimm identifier preference
  @param[out] ppDimmIdStr A pointer to the HII DimmId string. Dynamically allocated memory and must be released by calling function.

  @retval EFI_OUT_OF_RESOURCES if there is no space available to allocate memory for string
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
ConvertRegionDimmIdsToDimmListStr(
  IN     REGION_INFO *pRegionInfo,
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT8 DimmIdentifier,
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
  Appends a formatted Unicode string with arguments to a pre-allocated
  null-terminated Unicode string provided by the caller with length of
  DestStringMaxLength.

  @param[in] DestString          A Null-terminated Unicode string of size
                                 DestStringMaxLength
  @param[in] DestStringMaxLength The maximum number of CHAR16 characters
                                 that will fit into DestString
  @param[in] FormatString        A Null-terminated Unicode format string.
  @param[in] ...                 The variable argument list whose contents are
                                 accessed based on the format string specified by
                                 FormatString.
**/
EFI_STATUS
CatSPrintNCopy(
  IN OUT CHAR16 *pDestString,
  IN     UINT16 DestStringMaxLength,
  IN     CONST CHAR16 *pFormatString,
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
CHAR16 NvmToUpper(
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
FileRead(
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
  @param[out] pListSize  Counted number of items in the list

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

  @retval CLI string representation of last shutdown status
**/
CHAR16*
LastShutdownStatusToStr(
  IN     LAST_SHUTDOWN_STATUS_DETAILS_COMBINED LastShutdownStatus
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
  Convert software triggers enabled to string

  @param[in] SoftwareTriggersEnabled, bits define triggers that are enabled

  @retval CLI string representation of enabled triggers
**/
CHAR16*
SoftwareTriggersEnabledToStr(
  IN     UINT64 SoftwareTriggersEnabled
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
  Retrieve the number of bits set in a number
  Based on Brian Kernighan's Algorithm

  @param[in] Number Number in which number of bits set is to be counted
  @param[out] pNumOfBitsSet Number of bits set
**/
EFI_STATUS CountNumOfBitsSet(
  IN  UINT64 Number,
  OUT UINT8  *pNumOfBitsSet
);

/**
  Retrieve the bitmap for NumOfChannelWays

  @param[in] NumOfChannelWays Number of ChannelWays or Number of Dimms used in an Interleave Set
  @param[out] pBitField Bitmap based on PCAT 2.0 Type 1 Table for ChannelWays
**/
EFI_STATUS GetBitFieldForNumOfChannelWays(
  IN  UINT64 NumOfChannelWays,
  OUT UINT16  *pBitField
);

/**
  Convert dimm's security state bitmask to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] SecurityStateBitmask, bits define dimm's security state

  @retval String representation of Dimm's security state
**/
CHAR16*
SecurityStateBitmaskToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT32 SecurityStateBitmask
);
/**
  Convert dimm's SVN Downgrade Opt-In to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] SecurityOptIn, bits define dimm's security opt-in value

  @retval String representation of Dimm's SVN Downgrade opt-in
**/
CHAR16*
SVNDowngradeOptInToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT32 OptInValue
);
/**
  Convert dimm's Secure Erase Policy Opt-In to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] SecurityOptIn, bits define dimm's security opt-in value

  @retval String representation of Dimm's Secure erase policy opt-in
**/
CHAR16*
SecureErasePolicyOptInToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT32 OptInValue
);

/**
  Convert dimm's S3 Resume Opt-In to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] SecurityOptIn, bits define dimm's security opt-in value

  @retval String representation of Dimm's S3 Resume opt-in
**/
CHAR16*
S3ResumeOptInToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT32 OptInValue
);
/**
  Convert dimm's Fw Activate Opt-In to its respective string

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] SecurityOptIn, bits define dimm's security opt-in value

  @retval String representation of Dimm's Fw Activate opt-in
**/
CHAR16*
FwActivateOptInToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT32 OptInValue
);

/**
  Convert ARS status value to its respective string

  @param[in] ARS status value
  @param[in] AppDirect Capacity value (sum across all PMems)

  @retval CLI string representation of ARS status
**/
CHAR16*
ARSStatusToStr(
  IN     UINT8 ARSStatus,
  IN     UINT64 AppDirectCapacity
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

  @param[in] SkuInformation1 - first SkuInformation to compare
  @param[in] SkuInformation2 - second SkuInformation to compare

  @retval NVM_SUCCESS - if everything went fine
  @retval NVM_ERR_DIMM_SKU_MODE_MISMATCH - if mode conflict occurred
  @retval NVM_ERR_DIMM_SKU_SECURITY_MISMATCH - if security mode conflict occurred
**/
NvmStatusCode
SkuComparison(
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
  Populates the units string based on the particular capacity unit
  @param[in] pData A pointer to the main HII data structure
  @param[in] Units The input unit to be converted into its HII string
  @param[out] ppUnitsStr A pointer to the HII units string. Dynamically allocated memory and must be released by calling function.

  @retval EFI_OUT_OF_RESOURCES if there is no space available to allocate memory for units string
  @retval EFI_INVALID_PARAMETER if one or more input parameters are invalid
  @retval EFI_SUCCESS The conversion was successful
**/
EFI_STATUS
UnitsToStr (
  IN     EFI_HII_HANDLE HiiHandle,
  IN     UINT16 Units,
     OUT CHAR16 **ppUnitsStr
  );

/**
  Convert last firmware update status to string.
  The caller function is obligated to free memory of the returned string.

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Last Firmware update status value to convert

  @retval output string or NULL if memory allocation failed
**/
CHAR16 *
LastFwUpdateStatusToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 LastFwUpdateStatus
  );

/**
  Convert quiesce required value to string.
  The caller function is obligated to free memory of the returned string.

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Quiesce required value to convert

  @retval output string or NULL if memory allocation failed
**/
CHAR16 *
QuiesceRequiredToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 QuiesceRequired
);
/**
  Convert StagedFwActivatable to string.
  The caller function is obligated to free memory of the returned string.

  @param[in] HiiHandle handle to the HII database that contains i18n strings
  @param[in] Staged Fw activatable value to convert

  @retval output string or NULL if memory allocation failed
**/
CHAR16 *
StagedFwActivatableToString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 StagedFwActivatable
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
  @param[in] TimeInSeconds Number of seconds (EPOCH time)

  @retval Human readable time string
**/
CHAR16 *GetTimeFormatString (
    IN UINT64 TimeInSeconds,
    IN BOOLEAN verbose
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

  @param [in] pNvmDimmConfigProtocol Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param [in] DimmId Dimm ID of the dimm to poll status
  @param [in] OpcodeToPoll Specify an opcode to poll, 0 to poll regardless of opcode
  @param [in] SubOpcodeToPoll Specify an opcode to poll
  @param [in] Timeout for the background operation
**/
EFI_STATUS
PollLongOpStatus(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
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

/**
Copies a source buffer to a destination buffer, and returns the destination buffer.


@param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
@param  DestLength          The length in bytes of DestinationBuffer.
@param  SourceBuffer        The pointer to the source buffer of the memory copy.
@param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

@return DestinationBuffer.

**/
VOID *
CopyMem_S(
  OUT VOID       *DestinationBuffer,
  IN UINTN       DestLength,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
);

/**
  Retrieves Intel Dimm Config EFI vars

  User is responsible for freeing ppIntelDIMMConfig

  @param[out] pIntelDIMMConfig Pointer to struct to fill with EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
RetrieveIntelDIMMConfig(
     OUT INTEL_DIMM_CONFIG **ppIntelDIMMConfig
  );


/**
Get manageability state for Dimm

@param[in] SubsystemVendorId the SubsystemVendorId
@param[in] interfaceCodeNum the number of interface codes
@param[in] interfaceCodes the interface codes
@param[in] SubsystemDeviceId the subsystem device ID
@param[in] fwMajor the fw major version
@param[in] fwMinor the fw minor version


@retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmManageableByValues(
  IN  UINT16 SubsystemVendorId,
  IN  UINT32 interfaceCodeNum,
  IN  UINT16* interfaceCodes,
  IN  UINT16 SubsystemDeviceId,
  IN  UINT8 fwMajor,
  IN  UINT8 fwMinor
);

/**
Check if the dimm interface code of this DIMM is supported

@param[in] interfaceCodeNum the number of interface codes
@param[in] interfaceCodes the interface codes

@retval true if supported, false otherwise
**/
BOOLEAN
IsDimmInterfaceCodeSupportedByValues(
  IN  UINT32 interfaceCodeNum,
  IN  UINT16* interfaceCodes
);


/**
Check if the subsystem device ID of this DIMM is supported

@param[in] SubsystemDeviceId the subsystem device ID

@retval true if supported, false otherwise
**/
BOOLEAN
IsSubsystemDeviceIdSupportedByValues(
  IN UINT16 SubsystemDeviceId
);

/**
Check if current firmware API version is supported

@param[in] major the major version
@param[in] minor the minor version

@retval true if supported, false otherwise
**/
BOOLEAN
IsFwApiVersionSupportedByValues(
  IN   UINT8 major,
  IN   UINT8 minor
);

/**
  Convert controller revision id to string

  @param[in] Controller revision id

  @retval CLI string representation of the controller revision id
**/
CHAR16*
ControllerRidToStr(
  IN     UINT16 ControllerRid
  );

/**
Set object status for DIMM_INFO

@param[out] pCommandStatus Pointer to command status structure
@param[in] pDimm DIMM_INFO for which the object status is being set
@param[in] Status Object status to set
**/
VOID
SetObjStatusForDimmInfo(
  OUT COMMAND_STATUS *pCommandStatus,
  IN     DIMM_INFO *pDimm,
  IN     NVM_STATUS Status
);

/**
Set object status for DIMM_INFO

@param[out] pCommandStatus Pointer to command status structure
@param[in] pDimm DIMM_INFO for which the object status is being set
@param[in] Status Object status to set
@param[in] If TRUE - clear all other status before setting this one
**/
VOID
SetObjStatusForDimmInfoWithErase(
  OUT COMMAND_STATUS *pCommandStatus,
  IN     DIMM_INFO *pDimm,
  IN     NVM_STATUS Status,
  IN     BOOLEAN EraseFirst
);

/**
Serialize a Tag ID

@param[in] id the session tag id to be save/serialized
**/
EFI_STATUS PbrDcpmmSerializeTagId(
  UINT32 id
);

/**
Deserialize a Tag ID

@param[in] id - will contain the previously serialized tag id if exists, otherwise will contain the value
            of 'defaultId'
@param[in] defaultId - the value to assign input param 'id' in the case the TagId wasn't previously serialized.
**/
EFI_STATUS PbrDcpmmDeserializeTagId(
  UINT32 *id,
  UINT32 defaultId
);

/**
Converts a DIMM_INFO_ATTRIB_X attribute to a string

@param[in] pAttrib - a DIMM_INFO_ATTRIB_X attribute to convert
@param[in] pFormatStr - optional format string to use for conversion
**/
CHAR16 * ConvertDimmInfoAttribToString(
  VOID *pAttrib,
  CHAR16* pFormatStr OPTIONAL
);

/**
  Create a duplicate of a string without parsing any format strings.
  Caller is responsible for freeing the returned string.
  Max string length is MAX_STRING_LENGTH

  @param[in] StringToDuplicate - String to duplicate
  @param[out] pDuplicateString - Allocated copy of StringToDuplicate
**/
EFI_STATUS
DuplicateString(
  IN     CHAR16 *StringToDuplicate,
     OUT CHAR16 **pDuplicateString
);

/**
  Wrap the string (add \n) at the specified WrapPos by replacing a space character (' ')
  with a newline character. Used for the HII popup window.
  Make a copy of the MessageString so we can modify it if needed.

  @param[in] WrapPos - Line length limit (inclusive). Does not include "\n" or "\0"
  @param[in] MessageString - Original message string, is not modified
  @param[out] pWrappedString - Allocated copy of MessageString that is wrapped with "\n"
**/
EFI_STATUS
WrapString(
  IN     UINT8 WrapPos,
  IN     CHAR16 *MessageString,
     OUT CHAR16 **pWrappedString
  );


 /**
  Guess an appropriate NVM_STATUS code from EFI_STATUS. For use when
  pCommandStatus is not an argument to a lower level function.

  Used currently to get specific errors relevant to the user out to
  the CLI but not many (especially lower-level) functions have
  pCommandStatus. Also the CLI printer doesn't use ReturnCode,
  only pCommandStatus.

  @param[in] ReturnCode - EFI_STATUS returned from function call
  @retval - Appropriate guess at the NVM_STATUS code
**/
NVM_STATUS
GuessNvmStatusFromReturnCode(
  IN EFI_STATUS ReturnCode
);
#ifndef OS_BUILD
/**
  Find serial attributes from SerialProtocol and set on
  serial driver

  @retval - Status of operation
**/
EFI_STATUS
SetSerialAttributes(
  VOID
);
#endif
#endif /** _UTILITY_H_ **/

