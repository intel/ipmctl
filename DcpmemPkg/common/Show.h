/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _SHOW_H_
#define _SHOW_H_

#include <Uefi.h>
#include <Debug.h>
#include <Types.h>
#include <DataSet.h>

#define MAX_HEADER_NAME_SZ  100
#define MAX_LIST_LEVELS 10
#define MAX_TABLE_COLUMNS 15

typedef enum {
  TEXT,
  XML
}SHOW_FORMAT_TYPE;

typedef struct _FORMAT_FLAGS {
  UINTN Table     : 1;
  UINTN List      : 1;
  UINTN EsxKeyVal : 1;
  UINTN EsxCustom : 1;
  UINTN Verbose   : 1;
}FORMAT_FLAGS;

typedef union _SHOW_FORMAT_TYPE_FLAGS {
  FORMAT_FLAGS Flags;
  UINTN Val;
}SHOW_FORMAT_TYPE_FLAGS;

typedef struct _SHOW_CMD_CONTEXT {
  SHOW_FORMAT_TYPE FormatType;
  SHOW_FORMAT_TYPE_FLAGS FormatTypeFlags;
  VOID * FormatTypeAttribs;
  VOID * UserData;
}SHOW_CMD_CONTEXT;

typedef struct _LIST_LEVEL_ATTRIB {
  CONST CHAR16 *LevelType;
  CONST CHAR16 *LevelHeader;
  CONST CHAR16 *LevelKeyValFormatStr;
  CONST CHAR16 *IgnoreKeyValList;
}LIST_LEVEL_ATTRIB;

typedef struct _SHOW_LIST_ATTRIB {
  LIST_LEVEL_ATTRIB LevelAttribs[MAX_LIST_LEVELS];
}SHOW_LIST_ATTRIB;

typedef struct _TABLE_COLUMN_ATTRIB {
  CONST CHAR16 *ColumnHeader;
  CONST UINT32 ColumnWidth;
  CONST CHAR16 *ColumnDataSetPath;
}TABLE_COLUMN_ATTRIB;

typedef struct _SHOW_TABLE_ATTRIB {
  TABLE_COLUMN_ATTRIB ColumnAttribs[MAX_TABLE_COLUMNS];
}SHOW_TABLE_ATTRIB;

#define SET_FORMAT_TABLE_FLAG(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Table = 1; \
  Ctx->FormatTypeFlags.Flags.List = 0; \
} \

#define SET_FORMAT_LIST_FLAG(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Table = 0; \
  Ctx->FormatTypeFlags.Flags.List = 1; \
} \

#define SET_FORMAT_ESX_KV_FLAG(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.EsxKeyVal = 1; \
  Ctx->FormatTypeFlags.Flags.EsxCustom = 0; \
} \

#define SET_FORMAT_ESX_CUSTOM_FLAG(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.EsxKeyVal = 0; \
  Ctx->FormatTypeFlags.Flags.EsxCustom = 1; \
} \

#define SET_FORMAT_VERBOSE_FLAG(Ctx) \
if(NULL != Ctx) { \
  Ctx->FormatTypeFlags.Flags.Verbose = 1; \
} \

#define SET_TABLE_ATTRIBUTES(Ctx, Attributes) \
do { \
  Ctx->FormatTypeAttribs = (VOID*)&Attributes; \
} while(0); \

// Helper to calculate the column width of a CHAR16 string literal
#define TABLE_MIN_HEADER_LENGTH(Header)     ((sizeof(Header) / sizeof(CHAR16)) + 2)

/*
* Main entry point for displaying an error.
*/
VOID ShowCmdError(SHOW_CMD_CONTEXT *ShowCtx, EFI_STATUS CmdExitCode, CHAR16* Msg, ...);
/*
* Main entry point for displaying a hierarchical data.
* Currently supports: Nested lists as text (SHOW_LIST)
*                     Table as text (SHOW_TABLE)
*                     NVM XML (SHOW_NVM_XML)
*                     ESX XML (SHOW_ESX_XML)
*/
VOID ShowCmdData(DATA_SET_CONTEXT *DataSetCtx, SHOW_CMD_CONTEXT *ShowCtx);
#endif /** _SHOW_H_**/