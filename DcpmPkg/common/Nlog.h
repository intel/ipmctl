/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NLOG_H_
#define _NLOG_H_

#include "Strings.h"

#define NLOG_DICT_FIELDCOUNT 5
#define NLOG_DICT_SPLIT_CHAR ','
#define NLOG_DICT_VERSION_SPLIT_CHAR '='

typedef union {
  struct {
    UINT32 magic_number : 24;
    UINT32 version : 8;
  } data;
  UINT32 rawData;
} nlog_version_v2;

typedef union {
  struct {
    UINT32 args : 8;
    UINT32 line_number : 16;
    UINT32 module_id : 8;
  } data;
  UINT32 rawData;
} nlog_version_v1;


typedef struct {
  UINT32 Hash;
  UINT64 Args;
  CHAR8* LogLevel;
  CHAR8* FileName;
  CHAR8* LogString;
  VOID* next;
  VOID* prev;
} nlog_dict_entry;

typedef struct {
  UINT64 id;
  UINT32 KernelTime;
  UINT32** ArgValues;
  nlog_dict_entry* DictEntry;
  CHAR8* FormattedString;
  UINT64 FormattedStringLen;
  VOID* next;
  VOID* prev;
} nlog_record;

/*
decode_nlog_binary command

@param[in] decoded_file_name - the file to append records to
@param[in] nlogbytes - the blob returned from the dump command
@param[in] size - the number of bytes in the blob
@param[in] dict_version - the version of the loaded dictionary
@param[in] dict_head - the head to the dictionary linked list
@param[out] node_count - the number of decoded entries
*/
VOID
decode_nlog_binary(
  struct Command *pCmd,
  CHAR16* decoded_file_name,
  UINT8* nlogbytes,
  UINT64 size,
  UINT32 dict_version,
  nlog_dict_entry* dict_head
);

/*
get_nlog_entry command

@param[in] hashVal - the hash to locate
@param[in] head - the head to the dictionary linked list

@retval the discovered node, or NULL
*/
nlog_dict_entry*
get_nlog_entry(
  IN UINT32 hashVal, 
  IN nlog_dict_entry* head
);

/*
load_nlog_dict command

@param[in] pDictPath - the path to the dictionary file
@param[out] version - the version of the dictionary as detected
@param[out] node_count - the number of nodes in the linked list

@retval the head to the dictionary linked list
*/
nlog_dict_entry*
load_nlog_dict(
  struct Command *pCmd,
  IN CHAR16 * pDictPath,
  OUT UINT32 * version,
  OUT UINT64 * node_count
);

/*
load_nlog_dict_v2 command

@param[in] lines - the array of strings to convert to structs
@param[in] line_count - the length of the array of strings to convert to structs
@param[out] node_count - the number of nodes in the linked list

@retval the head to the dictionary linked list
*/
nlog_dict_entry*
load_nlog_dict_v2(
  struct Command *pCmd,
  IN CHAR8 ** lines,
  IN UINT64 line_count,
  OUT UINT64 * node_count
);

/*
Loads test binary dumps for the purpose of decoding them
*/
VOID **
LoadBinaryFile(
  CHAR16 * pLoadUserPath,
  OUT  UINT64 *bytes_read);
#endif
