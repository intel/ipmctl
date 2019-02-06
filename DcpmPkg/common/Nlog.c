/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "Nlog.h"

VOID
decode_nlog_binary(
  struct Command *pCmd,
  CHAR16* decoded_file_name,
  UINT8* nlogbytes,
  UINT64 size,
  UINT32 dict_version,
  nlog_dict_entry* dict_head
)
{
  EFI_STATUS status;
  BOOLEAN inv2section = FALSE;
  UINT32 expectedMagicNum = 11928997;
  UINT64 x = 0;
  UINT64 y = 0;
  UINT64 z = 0;
  nlog_dict_entry* entry = NULL;
  nlog_record* record = NULL;
  nlog_record* head = NULL;
  nlog_record* tail = NULL;
  UINT64 total_formatted_string_size = 0;
  CHAR8* total_formatted_string = NULL;
  UINT64 elements = 0;
  CHAR8** append_strs = NULL;
  UINT32** old_args = NULL;
  UINT64 old_arg_count = 0;
  nlog_version_v1 v1;
  nlog_version_v2 v2;
  CHAR16* kernel_str = NULL;
  CHAR8* ascii_kernel_str = NULL;
  CHAR8* system_time_set_log = "System Time Set";
  UINTN ascii_kernel_str_size = 0;
  UINT32 system_time_set = 0;
  CHAR8* old_format_str = NULL;
  CHAR8* decode_header = NULL;
  UINT64 header_length = 0;
  UINT32 value;
  UINT64 node_count = 0;
  CHAR8* formatted_string_head = NULL;
  UINT64 bytes_needed = 0;
  UINT64 appended_count = 0;
  BOOLEAN dictionary_entry_was_allocated_here = FALSE;
  BOOLEAN hash_not_found = FALSE;
  nlog_record* next;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (pCmd != NULL) {
    pPrinterCtx = pCmd->pPrintCtx;
  }

  node_count = 0;
  for (x = 0; x < size; x += 4)
  {
    record = NULL;
    entry = NULL;
    dictionary_entry_was_allocated_here = FALSE;
    hash_not_found = FALSE;
    value = bytes_to_u32(&nlogbytes[x]);
    if (x % 256 == 0)
    {
      inv2section = FALSE;
      v2.rawData = value;
      if (v2.data.magic_number == expectedMagicNum &&
        v2.data.version == dict_version)
      {
        inv2section = TRUE;
        continue;
      }
    }

    if (0 == value)
    {
      continue; //this is not a valid record
    }

    /*
    check for V2 dictionary entry if this is a V2 section. If there isn't one, create a fake one for logging purposes

    If this is a V1 section and the value is valid for a V1 section, create a fake entry for logging purposes
    */
    if (inv2section)
    {
      entry = get_nlog_entry(value, dict_head);
      if (entry == NULL)
      {
        hash_not_found = TRUE;
        entry = AllocateZeroPool(sizeof(nlog_dict_entry));
        if (NULL == entry)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }

        dictionary_entry_was_allocated_here = TRUE;

        entry->Hash = value;
        entry->Args = 1;
        entry->LogLevel = string_copy("-");
        entry->FileName = string_copy("-");
        entry->LogString = string_copy("Hash %d not found in dictionary");
        entry->next = NULL;
        entry->prev = NULL;

        record = AllocateZeroPool(sizeof(nlog_record));
        if (NULL == record)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }

        record->DictEntry = entry;
        record->KernelTime = 0;
        record->ArgValues = AllocateZeroPool(record->DictEntry->Args * sizeof(UINT32*));
        if (NULL == record->ArgValues)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }
        record->ArgValues[0] = AllocateZeroPool(sizeof(UINT32));
        if (NULL == record->ArgValues[0])
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }
        *record->ArgValues[0] = entry->Hash;
        record->KernelTime = 0;
        record->FormattedString = NULL;
        record->prev = NULL;
        record->next = NULL;
      }
    }
    else
    {
      v1.rawData = value;
      if (0 == v1.data.args && 0 == v1.data.line_number && 0 == v1.data.module_id)
      {
        continue;
      }

      entry = AllocateZeroPool(sizeof(nlog_dict_entry));
      if (NULL == entry)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
        goto Finish;
      }

      dictionary_entry_was_allocated_here = TRUE;

      entry->Args = v1.data.args;
      entry->Hash = 0;
      entry->LogLevel = string_copy("-");
      entry->FileName = string_copy("-");
      entry->LogString = NULL;
      entry->next = NULL;
      entry->prev = NULL;
    }

    //Move the pointer index to the next U32 and allocate space for a record
    if (record == NULL)
    {
      record = AllocateZeroPool(sizeof(nlog_record));
      if (NULL == record)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
        goto Finish;
      }

      record->id = node_count;
      record->ArgValues = NULL;
      record->FormattedString = NULL;
      record->prev = NULL;
      record->next = NULL;
      record->KernelTime = 0;
    }

    if (record->DictEntry == NULL && entry != NULL) {
      record->DictEntry = entry;
    }

    if (head == NULL) {
      head = record;
      tail = record;
    }
    else {
      tail->next = record;
      record->prev = tail;
      tail = record;
    }

    //get the timestamp
    if (FALSE == hash_not_found)
    {
      x += 4;
      if (x >= size)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Unexpected end of buffer. 1\n");
        goto Finish;
      }
      record->KernelTime = bytes_to_u32(&nlogbytes[x]);

      /*
      Gather the arument U32s according to the discovered count
      */
      if (record->DictEntry->Args > 0)
      {
        record->ArgValues = AllocateZeroPool(record->DictEntry->Args * sizeof(UINT32*));
        if (NULL == record->ArgValues)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }

        for (y = 0; y < record->DictEntry->Args && x < size; y++)
        {
          x += 4;
          if (x >= size)
          {
            PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Unexpected end of buffer. 3\n");
            goto Finish;
          }
          record->ArgValues[y] = AllocateZeroPool(sizeof(UINT32));
          if (NULL == record->ArgValues[y])
          {
            PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
            goto Finish;
          }

          *record->ArgValues[y] = bytes_to_u32(&nlogbytes[x]);
        }
      }


      if (FALSE == inv2section)
      {
        elements = 3 + record->DictEntry->Args;
        append_strs = AllocateZeroPool(sizeof(CHAR8*) * elements);
        if (NULL == append_strs)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }

        append_strs[0] = string_copy("V1 Log module: 0x%X, ");
        append_strs[1] = string_copy("line: %d, ");
        append_strs[2] = string_copy("args: ");
        for (z = 3; z < elements; z++)
        {
          append_strs[z] = string_copy("0x%X ");
        }

        old_args = record->ArgValues;
        old_arg_count = record->DictEntry->Args;

        record->DictEntry->Args = old_arg_count + 2;
        record->ArgValues = AllocateZeroPool(record->DictEntry->Args * sizeof(UINT32*));
        if (NULL == record->ArgValues)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }

        record->ArgValues[0] = AllocateZeroPool(sizeof(UINT32));
        if (NULL == record->ArgValues[0])
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }
        record->ArgValues[1] = AllocateZeroPool(sizeof(UINT32));
        if (NULL == record->ArgValues[1])
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
          goto Finish;
        }
        *record->ArgValues[0] = v1.data.module_id;
        *record->ArgValues[1] = v1.data.line_number;


        for (z = 0; z < old_arg_count; z++)
        {
          record->ArgValues[z + 2] = AllocateZeroPool(sizeof(UINT32));
          *record->ArgValues[z + 2] = *old_args[z];
        }

        for (z = 0; z < old_arg_count; z++)
        {
          FREE_POOL_SAFE(old_args[z]);
        }

        FREE_POOL_SAFE(old_args);

        record->DictEntry->LogString = string_array_concat(append_strs, elements, TRUE, &z);
      }
    }

    record->FormattedString = nlog_format(record->DictEntry->LogString, record->ArgValues, record->DictEntry->Args);

    if (TRUE == hash_not_found)
    {
      elements = 2;
      old_format_str = record->FormattedString;
      append_strs = AllocateZeroPool(sizeof(CHAR8*) * elements);
      if (NULL == append_strs)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
        goto Finish;
      }
      append_strs[0] = old_format_str;
      append_strs[1] = string_copy("\n");
      record->FormattedString = string_array_concat(append_strs, elements, FALSE, &record->FormattedStringLen);
    }
    else
    {
      elements = 8;
      old_format_str = record->FormattedString;
      append_strs = AllocateZeroPool(sizeof(CHAR8*) * elements);
      if (NULL == append_strs)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
        goto Finish;
      }

      // Look for log eg: \"System Time Set at boot. Time: 0x0_55bbb4a6\". Convert to time format string only for real kernel time and not system ticks.
      if (((system_time_set != 0) && (record->KernelTime >= system_time_set)) || (AsciiStrnCmp(record->FormattedString + 1, system_time_set_log, string_length(system_time_set_log)) == 0))
      {
        system_time_set = record->KernelTime;
        kernel_str = GetTimeFormatString((UINT64)record->KernelTime);
        if (NULL == kernel_str)
        {
          PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to convert the timestamp into readable string format\n");
          goto Finish;
        }
        ascii_kernel_str_size = StrLen(kernel_str) + 1;
        ascii_kernel_str = AllocateZeroPool(sizeof(CHAR8) * ascii_kernel_str_size);
        UnicodeStrToAsciiStrS(kernel_str, ascii_kernel_str, ascii_kernel_str_size);
        append_strs[0] = pad_left(ascii_kernel_str, 28, ' ', TRUE);
      }
      else
      {
        ascii_kernel_str = u32_to_a(record->KernelTime, FALSE, 0, FALSE);
        append_strs[0] = pad_left(ascii_kernel_str, 28, ' ', TRUE);
      }
      append_strs[1] = string_copy(" :: ");
      append_strs[2] = pad_left(record->DictEntry->FileName, 27, ' ', FALSE);
      append_strs[3] = string_copy(" :: ");
      append_strs[4] = pad_left(record->DictEntry->LogLevel, 7, ' ', FALSE);
      append_strs[5] = string_copy(" :: ");
      append_strs[6] = record->FormattedString;
      append_strs[7] = string_copy("\n");

      record->FormattedString = string_array_concat(append_strs, elements, FALSE, &record->FormattedStringLen);
    }

    total_formatted_string_size += record->FormattedStringLen;

    FREE_POOL_SAFE(kernel_str);
    if (append_strs)
    {
      for (z = 0; z < elements; z++)
      {
        FREE_POOL_SAFE(append_strs[z]);
      }
      FREE_POOL_SAFE(append_strs);
    }

    if (record->ArgValues)
    {
      for (z = 0; z < record->DictEntry->Args; z++)
      {
        FREE_POOL_SAFE(record->ArgValues[z]);
      }

      FREE_POOL_SAFE(record->ArgValues);
      record->ArgValues = NULL;
    }

    if (entry != NULL && dictionary_entry_was_allocated_here)
    {
      FREE_POOL_SAFE(entry->FileName);
      FREE_POOL_SAFE(entry->LogLevel);
      FREE_POOL_SAFE(entry->LogString);
      FREE_POOL_SAFE(entry);
      entry = NULL;
      record->DictEntry = NULL;
    }

    node_count++;
  }

  /*
  build the output string and dump it to the file
  */
  decode_header = string_copy("TIMESTAMP ::              FILE           ::   LEVEL :: LOG\n=====================================================================================\n");
  header_length = string_length(decode_header);
  total_formatted_string_size += header_length;

  bytes_needed = (sizeof(CHAR8*) * total_formatted_string_size) + sizeof(CHAR8*);

  total_formatted_string = AllocateZeroPool(bytes_needed);
  if (NULL == total_formatted_string)
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate %lu bytes to dump the decoded output\n", bytes_needed);
    goto Finish;
  }

  formatted_string_head = total_formatted_string;
  appended_count = 0;
  if (header_length > 0)
  {
    MyMemCopy(formatted_string_head, header_length, decode_header);
    formatted_string_head = formatted_string_head + header_length;
  }
  FREE_POOL_SAFE(decode_header);

  record = head;
  while (record)
  {
    if (record->FormattedStringLen > 0)
    {
      MyMemCopy(formatted_string_head, record->FormattedStringLen, record->FormattedString);
      formatted_string_head = formatted_string_head + record->FormattedStringLen;
    }

    FREE_POOL_SAFE(record->FormattedString);
    record->FormattedString = NULL;
    appended_count++;
    record = record->next;
  }

  status = DumpToFile(decoded_file_name, total_formatted_string_size, total_formatted_string, TRUE);
  if (EFI_ERROR(status))
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to write record to file (%lu)\n", status);
    goto Finish;
  }
  PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Decoded %lu records to file (" FORMAT_STR ")\n", node_count, decoded_file_name);

Finish:

  if (NULL != old_args)
  {
    for (z = 0; z < old_arg_count; z++)
    {
      FREE_POOL_SAFE(old_args[z]);
    }
    FREE_POOL_SAFE(old_args);
  }

  if (NULL != total_formatted_string)
  {
    FREE_POOL_SAFE(total_formatted_string);
  }

  if (NULL != append_strs)
  {
    for (z = 0; z < elements; z++)
    {
      FREE_POOL_SAFE(append_strs[z]);
    }
    FREE_POOL_SAFE(append_strs);
  }

  while (head != NULL)
  {
    next = head->next;
    if (head->ArgValues)
    {
      for (z = 0; z < head->DictEntry->Args; z++)
      {
        FREE_POOL_SAFE(head->ArgValues[z]);
      }

      FREE_POOL_SAFE(head->ArgValues);
    }

    if (head->FormattedString)
    {
      FREE_POOL_SAFE(head->FormattedString);
    }

    FREE_POOL_SAFE(head);
    head = next;
  }

  if (entry != NULL && dictionary_entry_was_allocated_here)
  {
    FREE_POOL_SAFE(entry->FileName);
    FREE_POOL_SAFE(entry->LogLevel);
    FREE_POOL_SAFE(entry->LogString);
    FREE_POOL_SAFE(entry);
  }
}

nlog_dict_entry*
get_nlog_entry(
  UINT32 hashVal,
  nlog_dict_entry* head
)
{
  nlog_dict_entry* ret = head;
  while (NULL != ret)
  {
    if (ret->Hash == hashVal)
    {
      return ret;
    }

    ret = ret->next;
  }

  return NULL;
}

/*
Loads test binary dumps for the purpose of decoding them
*/
VOID **
LoadBinaryFile(
  CHAR16 * pLoadUserPath,
  OUT  UINT64 *bytes_read)
{
  EFI_STATUS status;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  VOID *buffer = NULL;

  CHAR16 * pDictPath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDictPath));
  if (pDictPath == NULL) {
    Print(L"Failed to allocate memory for the path\n");
    goto Finish;
  }

  status = GetDeviceAndFilePath(pLoadUserPath, pDictPath, &pDevicePathProtocol);
  if (EFI_ERROR(status))
  {
    Print(L"GetDeviceAndFilePath Failed\n");
    goto Finish;
  }

  status = FileRead(pDictPath, pDevicePathProtocol, 0x1FFFFFFF, bytes_read, (VOID **)&buffer);
  if (EFI_ERROR(status) || NULL == buffer)
  {
    Print(L"FileRead Failed\n");
    *bytes_read = 0;
    goto Finish;
  }
Finish:
  if (pDictPath != NULL) {
    FREE_POOL_SAFE(pDictPath);
  }

  return buffer;
}

nlog_dict_entry*
load_nlog_dict(
  struct Command *pCmd,
  CHAR16 * pLoadUserPath,
  UINT32 * version,
  UINT64 * node_count
)
{
  nlog_dict_entry* head = NULL;
  CHAR8** string_splits = NULL;
  CHAR8** file_lines = NULL;
  CHAR8* file_buffer = NULL;
  UINT64 bytes_read;
  UINT64 found_elements = 0;
  UINT64 line_count = 0;
  UINT32 x = 0;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  EFI_STATUS status;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (pCmd != NULL) {
    pPrinterCtx = pCmd->pPrintCtx;
  }

  *node_count = 0;

  CHAR16 * pDictPath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pDictPath));
  if (pDictPath == NULL) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    return NULL;
  }

  status = GetDeviceAndFilePath(pLoadUserPath, pDictPath, &pDevicePathProtocol);
  if (EFI_ERROR(status))
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to locate the file: " FORMAT_STR L" (%lu)\n", pLoadUserPath, status);
    goto Finish;
  }

  status = FileRead(pDictPath, pDevicePathProtocol, MAX_CONFIG_DUMP_FILE_SIZE, &bytes_read, (VOID **)&file_buffer);
  if (EFI_ERROR(status) || NULL == file_buffer)
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to open or read the file: " FORMAT_STR L" (%lu)\n", pLoadUserPath, status);
    goto Finish;
  }

  if (bytes_read > 0)
  {
    while (bytes_read >= 0 &&
      (file_buffer[bytes_read - 1] == '\n' ||
        file_buffer[bytes_read - 1] == '\r' ||
        file_buffer[bytes_read - 1] == '\t' ||
        file_buffer[bytes_read - 1] == ' '))
    {
      file_buffer[bytes_read - 1] = 0;
      bytes_read--;
    }
  }

  file_lines = string_split(file_buffer, '\n', 0, &line_count);
  if (NULL == file_lines || line_count <= 1)
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Dictionary passed does not contain enough content.\n");
    goto Finish;
  }

  string_splits = string_split(file_lines[0], NLOG_DICT_VERSION_SPLIT_CHAR, 2, &found_elements);
  if (NULL == string_splits)
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error in dict on line 1 - Found 0 elements, expected %lu\n", 2);
    goto Finish;
  }

  if (found_elements != 2)
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error in dict on line 1 - Found %lu elements, expected %lu\n", found_elements, 2);
    goto Finish;
  }

  *version = a_to_u32(string_splits[1]);
  if (*version != 2)
  {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Only version 2 dictionaries supported.\n");
    goto Finish;
  }

  head = load_nlog_dict_v2(pCmd, &file_lines[1], (line_count - 1), node_count);

Finish:

  if (NULL != string_splits)
  {
    for (x = 0; x < found_elements; x++)
    {
      FREE_POOL_SAFE(string_splits[x]);
    }

    FREE_POOL_SAFE(string_splits);
  }

  if (NULL != file_lines)
  {
    for (x = 0; x < line_count; x++)
    {
      FREE_POOL_SAFE(file_lines[x]);
    }

    FREE_POOL_SAFE(file_lines);
  }

  FREE_POOL_SAFE(file_buffer);
  return head;
}

nlog_dict_entry*
load_nlog_dict_v2(
  struct Command *pCmd,
  CHAR8 ** lines,
  UINT64 line_count,
  UINT64 * node_count
)
{
  UINT32 x = 0;
  nlog_dict_entry* head = NULL;
  nlog_dict_entry* tail = NULL;
  CHAR8* line = NULL;
  UINT64 found_elements = 0;
  CHAR8** parts = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (pCmd != NULL) {
    pPrinterCtx = pCmd->pPrintCtx;
  }

  *node_count = 0;

  for (x = 0; x < line_count; x++)
  {
    if (!lines[x])
    {
      break;
    }

    line = lines[x];
    parts = string_split(line, NLOG_DICT_SPLIT_CHAR, NLOG_DICT_FIELDCOUNT, &found_elements);
    if (NULL == parts)
    {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error in dict on line %lu - Found NULL elements, expected %lu.\n", x + 1, NLOG_DICT_FIELDCOUNT);
      goto Finish;
    }

    if (found_elements != NLOG_DICT_FIELDCOUNT)
    {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error in dict on line %lu - Found %lu elements, expected %lu.\n", x + 1, found_elements, NLOG_DICT_FIELDCOUNT);
      goto Finish;
    }

    *node_count = *node_count + 1;

    if (head == NULL)
    {
      head = AllocateZeroPool(sizeof(nlog_dict_entry));
      if (NULL == head)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
        goto Finish;
      }
      tail = head;
      tail->prev = NULL;
      tail->next = NULL;
    }
    else
    {
      tail->next = AllocateZeroPool(sizeof(nlog_dict_entry));
      if (NULL == tail->next)
      {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to allocate space for decoded records\n");
        goto Finish;
      }
      ((nlog_dict_entry*)tail->next)->prev = tail;
      tail = tail->next;
    }

    tail->Hash = a_to_u32(parts[0]);
    tail->Args = a_to_u32(parts[1]);

    FREE_POOL_SAFE(parts[0]);
    FREE_POOL_SAFE(parts[1]);

    tail->LogLevel = parts[2];
    tail->FileName = parts[3];
    tail->LogString = parts[4];
    tail->next = NULL;
    FREE_POOL_SAFE(parts);
  }

Finish:
  if (parts != NULL) {
    FREE_POOL_SAFE(parts[0]);
    FREE_POOL_SAFE(parts[1]);
    FREE_POOL_SAFE(parts);
  }
  return head;
}

