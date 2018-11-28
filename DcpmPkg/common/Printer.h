/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _PRINTER_H_
#define _PRINTER_H_

#include <Uefi.h>
#include <Debug.h>
#include <Types.h>
#include <DataSet.h>

#define MAX_HEADER_NAME_SZ            100
#define MAX_LIST_LEVELS               10
#define MAX_TABLE_COLUMNS             15

#define SHOW_LIST_IDENT               L"   "

#define ID_MAX_STR_WIDTH                    10
#define DIMM_MAX_STR_WIDTH                  22
#define CAPACITY_MAX_STR_WIDTH              25
#define HEALTH_MAX_STR_WIDTH                19 //worst-case - "Noncritical failure"
#define HEALTH_SHORT_MAX_STR_WIDTH          17
#define AR_MAX_STR_WIDTH                    15
#define SECURITY_MAX_STR_WIDTH              16 //worst-case - "Unlocked, Frozen"
#define FW_VERSION_MAX_STR_WIDTH            13
#define ACTIVE_FW_VERSION_MAX_STR_WIDTH     16
#define STAGED_FW_VERSION_MAX_STR_WIDTH     16
#define SENSOR_TYPE_MAX_STR_WIDTH           30
#define SENSOR_VALUE_MAX_STR_WIDTH          20
#define SENSOR_STATE_MAX_STR_WIDTH          15
#define SOCKET_MAX_STR_WIDTH                9
#define MAPPED_MEMORY_LIMIT_MAX_STR_WIDTH   18
#define TOTAL_MAPPED_MEMORY_MAX_STR_WIDTH   18
#define ISET_ID_MAX_STR_WIDTH               20
#define REGION_ID_MAX_STR_WIDTH             8
#define PMEM_TYPE_MAX_STR_WIDTH             24
#define FREE_CAPACITY_MAX_STR_WIDTH         25
#define MEMORY_SIZE_MAX_STR_WIDTH           18
#define MEMORY_TYPE_MAX_STR_WIDTH           30
#define DEVICE_LOCATOR_MAX_STR_WIDTH        15


#define ON  1
#define OFF 0

// Helper to calculate the column width of a CHAR16 string literal
#define TABLE_MIN_HEADER_LENGTH(Header)     ((sizeof(Header) / sizeof(CHAR16)))

typedef enum {
  TEXT,
  XML
}PRINT_FORMAT_TYPE;

typedef enum {
  ASCII_STR,
  UNICODE_STR
}STR_TYPE;

typedef enum {
  BUFF_STR_TYPE,
  BUFF_DATA_SET_TYPE,
  BUFF_COMMAND_STATUS_TYPE
}BUFFERED_TYPE;

typedef struct _FLAGS {
  UINTN Buffered  : 1;
  UINTN Table     : 1;
  UINTN List      : 1;
  UINTN EsxKeyVal : 1;
  UINTN EsxCustom : 1;
  UINTN Verbose   : 1;
}FLAGS;

typedef union _PRINT_FORMAT_TYPE_FLAGS {
  FLAGS Flags;
  UINTN Val;
}PRINT_FORMAT_TYPE_FLAGS;

typedef struct _BUFFERED_PRINTER_OBJECT {
  LIST_ENTRY Link;
  BUFFERED_TYPE Type;
  EFI_STATUS Status;
  VOID *Obj;
}BUFFERED_PRINTER_OBJECT;

typedef struct _BUFFERED_STR {
  UINTN StrSize; //bytes
  STR_TYPE StrType;
  VOID *pStr;
}BUFFERED_STR;

typedef struct _BUFFERED_DATA_SET {
  DATA_SET_CONTEXT *pDataSet;
}BUFFERED_DATA_SET;

typedef struct _DATA_SET_LOOKUP_ITEM {
  LIST_ENTRY Link;
  CHAR16 *DsPath;
  DATA_SET_CONTEXT *pDataSet;
}DATA_SET_LOOKUP_ITEM;

typedef struct _BUFFERED_COMMAND_STATUS {
  CHAR16 *pStatusMessage;
  CHAR16 *pStatusPreposition;
  COMMAND_STATUS *pCommandStatus;
}BUFFERED_COMMAND_STATUS;

typedef struct _PRINT_CONTEXT {
  PRINT_FORMAT_TYPE FormatType;
  PRINT_FORMAT_TYPE_FLAGS FormatTypeFlags;
  VOID * UserData;
  LIST_ENTRY BufferedObjectList;
  EFI_STATUS BufferedObjectLastError;
  UINTN BufferedMsgCnt;
  UINTN BufferedCmdStatusCnt;
  UINTN BufferedDataSetCnt;
  LIST_ENTRY DataSetLookup;
  LIST_ENTRY DataSetRootLookup;
}PRINT_CONTEXT;

typedef struct _LIST_LEVEL_ATTRIB {
  CONST CHAR16 *LevelType;
  CONST CHAR16 *LevelHeader;
  CONST CHAR16 *LevelKeyValFormatStr;
  CONST CHAR16 *IgnoreKeyValList;
}LIST_LEVEL_ATTRIB;

typedef struct _PRINTER_LIST_ATTRIB {
  LIST_LEVEL_ATTRIB LevelAttribs[MAX_LIST_LEVELS];
}PRINTER_LIST_ATTRIB;

typedef struct _TABLE_COLUMN_ATTRIB {
  CONST CHAR16 *ColumnHeader;
  UINT32 ColumnMaxStrLen;
  CONST CHAR16 *ColumnDataSetPath;
}TABLE_COLUMN_ATTRIB;

typedef struct _PRINTER_TABLE_ATTRIB {
  TABLE_COLUMN_ATTRIB ColumnAttribs[MAX_TABLE_COLUMNS];
}PRINTER_TABLE_ATTRIB;

typedef struct _PRINTER_DATA_SET_ATTRIBS
{
  PRINTER_LIST_ATTRIB *pListAttribs;
  PRINTER_TABLE_ATTRIB *pTableAttribs;
}PRINTER_DATA_SET_ATTRIBS;

/**Is running in ESX or ESXTable mode**/
#define PRINTER_ESX_FORMAT_ENABLED(Ctx) \
  (NULL != Ctx && (Ctx->FormatTypeFlags.Flags.EsxKeyVal || Ctx->FormatTypeFlags.Flags.EsxCustom)) \

/**Display dataset as a table (default list)**/
#define PRINTER_ENABLE_TEXT_TABLE_FORMAT(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Table = 1; \
  Ctx->FormatTypeFlags.Flags.List = 0; \
} \

/**Display dataset as a list (default list)**/
#define PRINTER_ENABLE_LIST_TABLE_FORMAT(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Table = 0; \
  Ctx->FormatTypeFlags.Flags.List = 1; \
} \

/**Display dataset as ESX XML (-o esx)**/
#define PRINTER_ENABLE_ESX_XML_FORMAT(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.EsxKeyVal = 1; \
  Ctx->FormatTypeFlags.Flags.EsxCustom = 0; \
} \

/**Display dataset as ESX XML (-o esxtable)**/
#define PRINTER_ENABLE_ESX_TABLE_XML_FORMAT(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.EsxKeyVal = 0; \
  Ctx->FormatTypeFlags.Flags.EsxCustom = 1; \
} \

/**Not yet supported (-o verbose)**/
#define PRINTER_ENABLE_VERBOSE(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Verbose = 1; \
} \

/**Set printer format attributes directly to a dataset obj**/
#define PRINTER_CONFIGURE_DATA_SET_ATTRIBS(DataSet, Attributes) \
if(NULL != DataSet && NULL != Attributes) { \
  SetDataSetUserData(DataSet, (VOID *)Attributes); \
} \

/**Buffer output control (ON - prints directly to stdout, OFF - pushed into set buffer**/
#define PRINTER_CONFIGURE_BUFFERING(Ctx, State) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Buffered = State; \
} \

/**Print message directly to screen, status is ignored**/
#define PRINTER_PROMPT_MSG(ctx, efi_status, fmt, ...) \
do { \
  EFI_STATUS rc; \
  PRINTER_CONFIGURE_BUFFERING(ctx, OFF); \
  if( EFI_SUCCESS != (rc = PrinterSetMsg(ctx, efi_status, fmt, ## __VA_ARGS__))) { \
    NVDIMM_CRIT("Failed to prompt a message! (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while(0)

/* Push message to the set buffer for later processing.
*  If error status will be saved as "last error".
*/
#define PRINTER_SET_MSG(ctx, efi_status, fmt, ...) \
do { \
  EFI_STATUS rc; \
  PRINTER_CONFIGURE_BUFFERING(ctx, ON); \
  if( EFI_SUCCESS != (rc = PrinterSetMsg(ctx, efi_status, fmt, ## __VA_ARGS__))) { \
    NVDIMM_CRIT("Failed to buffer a message! (" FORMAT_EFI_STATUS ")", rc); \
  } \
 \
} while(0)

/**Print data (table or list) directly to stdout**/
#define PRINTER_PROMPT_DATA(ctx, efi_status, data) \
do { \
  EFI_STATUS rc; \
  PRINTER_CONFIGURE_BUFFERING(ctx, OFF); \
  if( EFI_SUCCESS != (rc = PrinterSetData(ctx, efi_status, data))) { \
    NVDIMM_CRIT("Failed to prompt data! (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while(0)

/* Push dataset pointer to the set buffer for later processing.
*  If error status will be saved as "last error".
*  PRINTER_PROCESS_SET_BUFFER does not free the dataset.
*/
#define PRINTER_SET_DATA(ctx, efi_status, data) \
do { \
  EFI_STATUS rc; \
  PRINTER_CONFIGURE_BUFFERING(ctx, ON); \
  if( EFI_SUCCESS != (rc = PrinterSetData(ctx, efi_status, data))) { \
    NVDIMM_CRIT("Failed to buffer data! (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while(0)

/**Transform the command status object to a string and print it directly to stdout**/
#define PRINTER_PROMPT_COMMAND_STATUS(ctx, efi_status, status_msg, status_preposition, command_status) \
do { \
  EFI_STATUS rc; \
  PRINTER_CONFIGURE_BUFFERING(ctx, OFF); \
  if( EFI_SUCCESS != (rc = PrinterSetCommandStatus(ctx, efi_status, status_msg, status_preposition, command_status))) { \
    NVDIMM_CRIT("Failed to prompt a command status object! (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while(0)

/* Transform the command status obj to a string and store the result in the set buffer for later processing.
*  If error status will be saved as "last error".
*  PRINTER_PROCESS_SET_BUFFER free's the transformed string, but does not free the command status obj.
*/
#define PRINTER_SET_COMMAND_STATUS(ctx, efi_status, status_msg, status_preposition, command_status) \
do { \
  EFI_STATUS rc; \
  PRINTER_CONFIGURE_BUFFERING(ctx, ON); \
  if( EFI_SUCCESS != (rc = PrinterSetCommandStatus(ctx, efi_status, status_msg, status_preposition, command_status))) { \
    NVDIMM_CRIT("Failed to buffer a command status object! (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while(0)

/**Transform all objects in the "set buffer" and print the result to stdout**/
#define PRINTER_PROCESS_SET_BUFFER(ctx) \
do { \
  EFI_STATUS rc; \
  if( EFI_SUCCESS != (rc = PrinterProcessSetBuffer(ctx))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while (0)

/**Transform all objects in the "set buffer" and print the result to stdout**/
#define PRINTER_PROCESS_SET_BUFFER_FORCE_TEXT_TABLE_MODE(ctx) \
do { \
  EFI_STATUS rc; \
  UINTN SavedFlags = 0; \
  PRINT_FORMAT_TYPE SavedFormatType; \
  SavedFormatType = ctx->FormatType; \
  ctx->FormatType = TEXT; \
  SavedFlags = ctx->FormatTypeFlags.Val; \
  ctx->FormatTypeFlags.Val = 0; \
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(ctx); \
  if( EFI_SUCCESS != (rc = PrinterProcessSetBuffer(ctx))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
  ctx->FormatTypeFlags.Val = SavedFlags; \
  ctx->FormatType = SavedFormatType; \
} while (0)

/* Helper that builds a path to a key's parent node which can be used with all SET_KEY_VAL* macros.
*  If *path will be freed if not NULL.
*/
#define PRINTER_BUILD_KEY_PATH(path, fmt, ...) \
do { \
  FREE_POOL_SAFE(path); \
  path = BuildPath(fmt,  ## __VA_ARGS__); \
} while (0)

/**Set a wide str into a dataset that resides in the "set buffer"**/
#define PRINTER_SET_KEY_VAL_WIDE_STR(ctx, key_path, key_name, val) \
do { \
  EFI_STATUS rc; \
  DATA_SET_CONTEXT *pDataSet = NULL; \
  if( EFI_SUCCESS != (rc = LookupDataSet(ctx, key_path, &pDataSet))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
  if( EFI_SUCCESS != (rc = SetKeyValueWideStr(pDataSet, key_name, val))) { \
    NVDIMM_CRIT("Failed to Set Key (%ls) Val (%ls) ReturnCode (" FORMAT_EFI_STATUS ")", key_name, val, rc); \
  } \
} while (0)

/**Append a wide str into a dataset that resides in the "set buffer"**/
#define PRINTER_APPEND_KEY_VAL_WIDE_STR(ctx, key_path, key_name, val) \
do { \
  EFI_STATUS rc; \
  CHAR16 *prev_val = NULL; \
  CHAR16 *concat_val = NULL; \
  UINTN appendLen = 0; \
  UINTN prevLen = 0; \
  DATA_SET_CONTEXT *pDataSet = NULL; \
  if( EFI_SUCCESS != (rc = LookupDataSet(ctx, key_path, &pDataSet))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
  if( EFI_SUCCESS != (rc = GetKeyValueWideStr(pDataSet, key_name, &prev_val, L""))) { \
    NVDIMM_CRIT("Failed to Set Key (%ls) Val (%ls) ReturnCode (" FORMAT_EFI_STATUS ")", key_name, val, rc); \
  } \
  if(NULL != prev_val) { \
    prevLen = StrLen(prev_val); \
  } \
  if(NULL != (CHAR16 *) val) { \
    appendLen = StrLen(val); \
  } \
  concat_val = (CHAR16*)AllocateZeroPool((prevLen + appendLen + 1) * sizeof(CHAR16)); \
  if(NULL != concat_val) { \
    if(NULL != prev_val) { \
      StrCpyS(concat_val, prevLen + appendLen + 1, prev_val); \
    } \
    if(NULL != (CHAR16 *) val) { \
      StrCpyS(concat_val + prevLen, appendLen + 1, val); \
    } \
  } \
  if( EFI_SUCCESS != (rc = SetKeyValueWideStr(pDataSet, key_name, concat_val))) { \
    NVDIMM_CRIT("Failed to Set Key (%ls) Val (%ls) ReturnCode (" FORMAT_EFI_STATUS ")", key_name, val, rc); \
  } \
  FREE_POOL_SAFE(concat_val); \
} while (0)

/**Set a wide str into a dataset that resides in the "set buffer"**/
#define PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(ctx, key_path, key_name, val, ...) \
do { \
  EFI_STATUS rc; \
  DATA_SET_CONTEXT *pDataSet = NULL; \
  if( EFI_SUCCESS != (rc = LookupDataSet(ctx, key_path, &pDataSet))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
  if( EFI_SUCCESS != (rc = SetKeyValueWideStrFormat(pDataSet, key_name, val, ## __VA_ARGS__))) { \
    NVDIMM_CRIT("Failed to Set Key (%ls) Val (%ls) ReturnCode (" FORMAT_EFI_STATUS ")", key_name, val, rc); \
  } \
} while (0)


/**Set a UINT16 into a dataset that resides in the "set buffer"**/
#define PRINTER_SET_KEY_VAL_UINT16(ctx, key_path, key_name, val, base) \
do { \
  EFI_STATUS rc; \
  DATA_SET_CONTEXT *pDataSet = NULL; \
  if( EFI_SUCCESS != (rc = LookupDataSet(ctx, key_path, &pDataSet))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
  if( EFI_SUCCESS != (rc = SetKeyValueUint16(pDataSet, key_name, val, base))) { \
    NVDIMM_CRIT("Failed to Set KeyVal pair (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while (0)

/**Set a UINT8 into a dataset that resides in the "set buffer"**/
#define PRINTER_SET_KEY_VAL_UINT8(ctx, key_path, key_name, val, base) \
do { \
  EFI_STATUS rc; \
  DATA_SET_CONTEXT *pDataSet = NULL; \
  if( EFI_SUCCESS != (rc = LookupDataSet(ctx, key_path, &pDataSet))) { \
    NVDIMM_CRIT("Failed to process printer objects! (" FORMAT_EFI_STATUS ")", rc); \
  } \
  if( EFI_SUCCESS != (rc = SetKeyValueUint8(pDataSet, key_name, val, base))) { \
    NVDIMM_CRIT("Failed to Set KeyVal pair (" FORMAT_EFI_STATUS ")", rc); \
  } \
} while (0)

/**Specify table attributes for a particular dataset**/
#define PRINTER_CONFIGURE_DATA_ATTRIBUTES(ctx, root_path, attribs) \
do { \
  SetDataSetPrinterAttribs(ctx, root_path, (VOID *)attribs); \
} while (0)

/*
* Helper to lookup a dataset described by a path
*/
EFI_STATUS LookupDataSet(
  IN     PRINT_CONTEXT *pPrintCtx,
  IN     CHAR16 *pKeyPath,
  OUT    DATA_SET_CONTEXT **pDataSet
);

/*
* Helper that associates a table/list of attributes with a dataset
*/
EFI_STATUS SetDataSetPrinterAttribs(
  IN     PRINT_CONTEXT *pPrintCtx,
  IN     CHAR16 *pKeyPath,
  IN     PRINTER_DATA_SET_ATTRIBS *pAttribs
);

/*
* Helper that builds a path to a dataset node
*/
CHAR16 *
EFIAPI
BuildPath(
  CHAR16 *Format,
  ...
);

/*
* Create a printer context
*/
EFI_STATUS PrinterCreateCtx(
  OUT    PRINT_CONTEXT **ppPrintCtx
);

/*
* Destroys a printer context
*/
EFI_STATUS PrinterDestroyCtx(
  IN    PRINT_CONTEXT *pPrintCtx
);

/*
* Handle string messages
*/
EFI_STATUS
EFIAPI
PrinterSetMsg(
  IN    PRINT_CONTEXT *pPrintCtx,
  IN    EFI_STATUS Status,
  IN    CHAR16 *pMsg,
  ...
);

/*
* Handle dataset objects
*/
EFI_STATUS PrinterSetData(
  IN    PRINT_CONTEXT *pPrintCtx,
  IN    EFI_STATUS Status,
  IN    DATA_SET_CONTEXT *pDataSetCtx
);

/*
* Handle commandstatus objects
*/
EFI_STATUS PrinterSetCommandStatus(
  IN     PRINT_CONTEXT *pPrintCtx,
  IN     EFI_STATUS Status,
  IN     CHAR16 *pStatusMessage,
  IN     CHAR16 *pStatusPreposition,
  IN     COMMAND_STATUS *pCommandStatus
);

/*
* Process all objects int the "set buffer"
*/
EFI_STATUS PrinterProcessSetBuffer(
  IN     PRINT_CONTEXT *pPrintCtx
);

#endif /** _PRINTER_H_**/