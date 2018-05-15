/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * The file describes the entry points of the Native Management API.
 * It is intended to be used by clients of the Native Management API
 * in order to perform management actions.
 *
 * To compile applications using the Native Management API, include this header file
 * and link with the -lixpdimm option.
 */

#ifndef	_NVM_MANAGEMENT_OUTPUT_PARSING_H_
#define	_NVM_MANAGEMENT_OUTPUT_PARSING_H_
#include <stdio.h>
#include <CommandParser.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif
#define NEW_BRANCH_IDENTIFIER                   L"---"
#define TABLE_TOK_DELIM                         L" \t"
#define TABBED_TABLE_TOK_DELIM                  L"\t"
#define KEY_VALUE_TOK_DELIM                     L"=:"
#define XML_ROOT_LIST_BEGIN_TAG                 L"<%lsList>\n"
#define XML_ROOT_LIST_END_TAG                   L"</%lsList>\n"
#define XML_ROOT_KEY_VALS_BEGIN_TAG             L"<%ls>\n"
#define XML_ROOT_KEY_VALS_END_TAG               L"</%ls>\n"
#define XML_KEY_VAL_PAIR_TAGS                   L"\t<%ls>%ls</%ls>\n"
#define XML_RESULT_BEGIN                        L"<Results>\n<Result>\n"
#define XML_RESULT_END                          L"</Result>\n</Results>\n"
#define XML_ERROR_BEGIN                         L"<Error Type=\"%d\">"
#define XML_ERROR_END                           L"<Error/>\n"
#define ESX_XML_FILE_BEGIN                      L"<?xml version=\"1.0\"?><output xmlns=\"http://www.vmware.com/Products/ESX/5.0/esxcli/\">"
#define ESX_XML_FILE_END                        L"</output>"
#define ESX_XML_LIST_STRING_BEGIN               L"<list type=\"string\">"
#define ESX_XML_LIST_STRING_END                 L"</list>"
#define ESX_XML_LIST_STRUCT_BEGIN               L"<list type=\"structure\">"
#define ESX_XML_LIST_STRUCT_END                 L"</list>"
#define ESX_XML_KEY_VAL_TYPE_STRUCT_BEGIN       L"<structure typeName=\"KeyValue\">"
#define ESX_XML_STRUCT_BEGIN                    L"<structure typeName=\"%ls\">"
#define ESX_XML_KEY_VAL_TYPE_STRUCT_END         L"</structure>"
#define ESX_XML_FIELD_ATTRIB_NAME_BEGIN         L"<field name = \"Attribute Name\">"
#define ESX_XML_FIELD_VALUE_BEGIN               L"<field name = \"Value\">"
#define ESX_XML_FIELD_BEGIN                     L"<field name = \"%ls\">"
#define ESX_XML_FIELD_END                       L"</field>"
#define ESX_XML_STRING_BEGIN_AND_END            L"<string>%ls</string>"
#ifdef __ESX__
#define NUM_DICTIONARIES_MAX                    100
#else
#define NUM_DICTIONARIES_MAX                    1024
#endif
#define PAIR_NAME_SZ                            256
#define PAIR_VALUE_SZ                           1024
#define READ_FD_LINE_SZ                         512
#define COLUMN_HEADER_SZ                        256
#define ROW_VALUE_SZ                            1024
#define MAX_NUMBER_COLUMNS                      100
#define MAX_KEY_VALUE_PAIRS                     1024

struct column
{
   wchar_t header_name[COLUMN_HEADER_SZ];
   wchar_t *row_value[ROW_VALUE_SZ];
   int row_cnt;
};

struct table
{
   wchar_t name[COLUMN_HEADER_SZ];
   struct column columns[MAX_NUMBER_COLUMNS];
   int column_cnt;
};

struct pair
{
   wchar_t name[PAIR_NAME_SZ];
   wchar_t value[PAIR_VALUE_SZ];
};

struct dict
{
   struct pair items[MAX_KEY_VALUE_PAIRS];
   int item_cnt;
};

enum OutputType
{
   NvmXmlType = 0,
   EsxXmlType = 1,
   UnknownType = 2
};

int process_output(
   enum DisplayType view_type,
   wchar_t *display_name,
   int rc,
   FILE *fd,
   int argc,
   char *argv[]);
#ifdef __cplusplus
}
#endif

#endif  /* _NVM_MANAGEMENT_OUTPUT_PARSING_H_ */
