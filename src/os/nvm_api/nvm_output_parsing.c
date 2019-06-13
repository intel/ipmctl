/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "nvm_output_parsing.h"
#include <Library/ShellCommandLib.h>
#include <CommandParser.h>
#include <s_str.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>

#if defined(__LINUX__)
#include <safe_str_lib.h>
#endif

#define MAX_TABLE_ITEM_LEN  256



wchar_t * normalize_tok(
   wchar_t *tok)
{
   wchar_t *pos;
   if ((pos = wcschr(tok, '\n')) != NULL)
      *pos = '\0';
   return tok;
}

int special_tok(
   wchar_t *tok)
{
   wchar_t *t = normalize_tok(tok);

   if (0 == wcscmp(t, UNITS_OPTION_B)  ||
       0 == wcscmp(t, UNITS_OPTION_MB) ||
       0 == wcscmp(t, UNITS_OPTION_MIB)||
       0 == wcscmp(t, UNITS_OPTION_GB) ||
       0 == wcscmp(t, UNITS_OPTION_GIB)||
       0 == wcscmp(t, UNITS_OPTION_TB) ||
       0 == wcscmp(t, UNITS_OPTION_TIB))
   {
      return 1;
   }
   return 0;
}

enum DisplayType display_view_type(
   enum DisplayType dt,
   int rc)
{
   if (0 != rc)
   {
      return ErrorView;
   }
   return dt;
}

int starts_with(
   wchar_t *str,
   wchar_t *prefix)
{
   wchar_t *beginning_str = str;
   int start_str_len = (int)wcsnlen(prefix, READ_FD_LINE_SZ);
   int total_str_len = (int)wcsnlen(str, READ_FD_LINE_SZ);
   for(int i = 0; i < total_str_len; ++i)
   {
      if(str[i] == L' ' || str[i] == L'\t')
         ++beginning_str;
      else break;
   }
   return wcsncmp(beginning_str, prefix, start_str_len);
}

int ends_with(
   const wchar_t *str,
   const wchar_t *suffix)
{
   if (!str || !suffix)
      return 1;
   int lenstr = (int)wcsnlen(str, READ_FD_LINE_SZ);
   int lensuffix = (int)wcsnlen(suffix, READ_FD_LINE_SZ);
   if (lensuffix >  lenstr)
      return 1;
   return wcsncmp(str + lenstr - lensuffix, suffix, lensuffix);
}

wchar_t *trim_list_suffix(
   wchar_t * str,
   wchar_t *suffix)
{
   wchar_t* s = wcsstr(str, suffix);
   if (NULL == s)
      return str;
   *s = 0;
   return str;
}

wchar_t *trimwhitespace(
   wchar_t *str)
{
   wchar_t *end;
   // Trim leading space
   while (iswspace((wchar_t)*str)) str++;
   // All spaces?
   if (*str == 0)
      return str;
   // Trim trailing space
   end = str + wcsnlen(str, READ_FD_LINE_SZ) - 1;
   while (end > str && iswspace((wchar_t)*end)) end--;
   // Write new null terminator
   *(end + 1) = 0;
   return str;
}

wchar_t *trimchar(
   wchar_t *str,
   wchar_t c)
{
   wchar_t *end;
   // Trim leading space
   while (*str == c) str++;
   // All spaces?
   if (*str == 0)
      return str;
   // Trim trailing space
   end = str + wcsnlen(str, READ_FD_LINE_SZ) - 1;
   while (end > str && (*end == c)) end--;
   // Write new null terminator
   *(end + 1) = 0;
   return str;
}


int tokenize_and_copy_key_value_pair(
   wchar_t *line,
   wchar_t *key,
   int key_sz,
   wchar_t *val,
   int val_sz)
{
   wchar_t *tok = line;
   int index = 0;
   int vindex = 0;

   memset(key, 0, key_sz);
   memset(val, 0, val_sz);

   while (tok[index] != L'\0' && ((tok[index] != L'=') && (tok[index] != L':')))
   {
     if(index < (key_sz-1))
      key[index] = tok[index];
     ++index;
   }

   if(tok[index] == L'\0')
    return -1;

   ++index;//shift past delim

   while (tok[index] != L'\0')
   {
     if (index < (val_sz - 1))
       val[vindex] = tok[index];
     ++index;
     ++vindex;
   }

   return 0;
}

/*
* Output everything from parsed data structs to stdout wrapped in xml (per nvmxml dtd)
* The return is 0 on success
*/
int output_to_nvm_xml_list(
   wchar_t *struct_name,
   struct dict *dictionaries,
   int num_dictionaries)
{
  wchar_t name[PAIR_NAME_SZ];
  wchar_t value[PAIR_VALUE_SZ];

   //Transform data structures to XML
   wprintf(XML_ROOT_LIST_BEGIN_TAG, struct_name);

   for (int i = 0; i < num_dictionaries; ++i)
   {
      wprintf(XML_ROOT_KEY_VALS_BEGIN_TAG, struct_name);
      for (int j = 0; j < dictionaries[i].item_cnt; ++j)
      {
        wcsncpy_s(name, PAIR_NAME_SZ, dictionaries[i].items[j].name, PAIR_NAME_SZ);
        wcsncpy_s(value, PAIR_VALUE_SZ, dictionaries[i].items[j].value, PAIR_VALUE_SZ);
        wprintf(XML_KEY_VAL_PAIR_TAGS, name, value, name);
      }
      wprintf(XML_ROOT_KEY_VALS_END_TAG, struct_name);
   }
   wprintf(XML_ROOT_LIST_END_TAG, struct_name);
   return 0;
}

/*
* Output everything from parsed data structs to stdout wrapped in xml (per nvmxml dtd)
* The return is 0 on success
*/
int output_to_nvm_xml_key_val_pairs(
   wchar_t *struct_name,
   struct dict *dictionary)
{
  wchar_t name[PAIR_NAME_SZ];
  wchar_t value[PAIR_VALUE_SZ];
   //Transform data structures to XML
   wprintf(XML_ROOT_KEY_VALS_BEGIN_TAG, struct_name);
   for (int j = 0; j < dictionary->item_cnt; ++j)
   {
     wcsncpy_s(name, PAIR_NAME_SZ, trimwhitespace(dictionary->items[j].name), PAIR_NAME_SZ);
     wcsncpy_s(value, PAIR_VALUE_SZ, trimwhitespace(dictionary->items[j].value), PAIR_VALUE_SZ);
     wprintf(XML_KEY_VAL_PAIR_TAGS, name, value, name);
   }
   wprintf(XML_ROOT_KEY_VALS_END_TAG, struct_name);
   return 0;
}

/*
* Output everything from the filestream to stdout wrapped in xml (per nvmxml dtd)
* The return is 0 on success
*/
int output_to_nvm_xml_results(
   FILE *fd)
{
   wint_t c;
   wprintf(XML_RESULT_BEGIN);
   while ((c = getwc(fd)) != WEOF)
      putwchar(c);
   wprintf(XML_RESULT_END);
   return 0;
}

/*
* Output everything from the filestream to stdout wrapped in xml (per nvmxml dtd)
* The return is 0 on success
*/
int output_to_nvm_xml_error(
   FILE *fd,
   int rc)
{
   wint_t c;
   wprintf(XML_ERROR_BEGIN, rc);
   while ((c = getwc(fd)) != WEOF)
      putwchar(c);
   wprintf(XML_ERROR_END);
   return 0;
}

/*
* Take parsed data structs and convert to ESX XML
* The return is 0 on success
*/
int output_to_esx_xml_key_val_pairs(
   wchar_t *struct_name,
   struct dict *dictionary)
{
  wchar_t name[PAIR_NAME_SZ];
  wchar_t value[PAIR_VALUE_SZ];
   //Transform data structures to XML
   wprintf(ESX_XML_FILE_BEGIN);
   wprintf(ESX_XML_LIST_STRUCT_BEGIN);
   for (int j = 0; j < dictionary->item_cnt; ++j)
   {
      wprintf(ESX_XML_KEY_VAL_TYPE_STRUCT_BEGIN);

      wprintf(ESX_XML_FIELD_ATTRIB_NAME_BEGIN);
      wcsncpy_s(name, PAIR_NAME_SZ, trimwhitespace(dictionary->items[j].name), PAIR_NAME_SZ);
      wprintf(ESX_XML_STRING_BEGIN_AND_END, name);
      wprintf(ESX_XML_FIELD_END);

      wprintf(ESX_XML_FIELD_VALUE_BEGIN);
      wcsncpy_s(value, PAIR_VALUE_SZ, trimwhitespace(dictionary->items[j].value), PAIR_VALUE_SZ);
      wprintf(ESX_XML_STRING_BEGIN_AND_END, value);
      wprintf(ESX_XML_FIELD_END);

      wprintf(ESX_XML_KEY_VAL_TYPE_STRUCT_END);
   }
   wprintf(ESX_XML_LIST_STRUCT_END);
   wprintf(ESX_XML_FILE_END);
   return 0;
}

/*
* Take parsed data structs and convert to ESX XML
* The return is 0 on success
*/
int output_to_esx_xml_list(
   wchar_t *struct_name,
   struct dict *dictionaries,
   int num_dictionaries)
{
  wchar_t name[PAIR_NAME_SZ];
  wchar_t value[PAIR_VALUE_SZ];

   //Transform data structures to XML
   wprintf(ESX_XML_FILE_BEGIN);
   wprintf(ESX_XML_LIST_STRUCT_BEGIN);
   for (int i = 0; i < num_dictionaries; ++i)
   {
      wprintf(ESX_XML_STRUCT_BEGIN, struct_name);
      for (int j = 0; j < dictionaries->item_cnt; ++j)
      {
         wcsncpy_s(name, PAIR_NAME_SZ, trimwhitespace(dictionaries[i].items[j].name), PAIR_NAME_SZ);
         wcsncpy_s(value, PAIR_VALUE_SZ, trimwhitespace(dictionaries[i].items[j].value), PAIR_VALUE_SZ);
         wprintf(ESX_XML_FIELD_BEGIN, name);
         wprintf(ESX_XML_STRING_BEGIN_AND_END, value);
         wprintf(ESX_XML_FIELD_END);
      }
      wprintf(ESX_XML_KEY_VAL_TYPE_STRUCT_END);
   }
   wprintf(ESX_XML_LIST_STRUCT_END);
   wprintf(ESX_XML_FILE_END);
   return 0;
}

/*
* Output everything from the filestream to stdout wrapped in xml
* The return is 0 on success
*/
int output_to_esx_xml_results(
   FILE *fd)
{
   wint_t c;
   wprintf(ESX_XML_FILE_BEGIN);
   wprintf(ESX_XML_STRING_BEGIN);
   while ((c = getwc(fd)) != WEOF)
      putwchar(c);
   wprintf(ESX_XML_STRING_END);
   wprintf(ESX_XML_FILE_END);
   return 0;
}

/*
* Output everything from the filestream to stdout.
* The return is 0 on success
*/
int output_to_esx_xml_error(
   FILE *fd,
   int rc)
{
   wint_t c;
   wprintf(L"ERROR: ");
   while ((c = getwc(fd)) != WEOF)
   {
      if(c != '\n')
        putwchar(c);
   }
   return 0;
}

#define XML_OPTION_ESX_STR  "esx"
#define XML_OPTION_ESX_TABLE_STR "esxtable"
#define XML_OPTION_NVM_STR  "nvmxml"
//temp WA
enum OutputType output_type(int argc, char *argv[])
{
  for (int i = 0; i < argc; i++)
  {
    if (0 == s_strncmpi(argv[i], XML_OPTION_ESX_STR, strlen(XML_OPTION_ESX_STR)))
    {
      return EsxXmlType;
    }
    else if (0 == s_strncmpi(argv[i], XML_OPTION_ESX_TABLE_STR, strlen(XML_OPTION_ESX_TABLE_STR)))
    {
      return EsxXmlType;
    }
    else if(0 == s_strncmpi(argv[i], XML_OPTION_NVM_STR, strlen(XML_OPTION_NVM_STR)))
    {
      return NvmXmlType;
    }
  }

  return UnknownType;
}

/*
* Parse the file stream and convert to the appropriate output type (nvmxml, esx, etc.)
* The return is 0 on success, -1 on error.
*/
int process_output(
   enum DisplayType view_type,
   wchar_t *display_name,
   wchar_t *display_delims,
   int rc,
   FILE *fd,
   int argc,
   char *argv[])
{
   enum OutputType out_type = UnknownType;
   static struct table show_table;
   enum DisplayType type = display_view_type(view_type, rc);
   wchar_t *line = (wchar_t*)malloc(READ_FD_LINE_SZ * sizeof(wchar_t));
   wchar_t *tok;
   wchar_t *state;
   int num_dictionaries = 0;
   struct dict *dictionaries = (struct dict*)malloc(sizeof(struct dict) * NUM_DICTIONARIES_MAX);
   struct dict *cur_dict = &dictionaries[0];
   size_t line_sz_chars = READ_FD_LINE_SZ;

   if (NULL == dictionaries || NULL == line)
   {
      goto FinishError;
   }
   memset(dictionaries, 0, sizeof(struct dict) * NUM_DICTIONARIES_MAX);

   if (UnknownType == (out_type = output_type(argc, argv)))
   {
      goto FinishError;
   }

   fseek(fd, 0, SEEK_SET);
   //view_type = display_view_type(cmd, rc);
   if (ErrorView == type)
   {
      goto FinishError;
   }
   else if (TableView == type || TableTabView == type)
   {
      wchar_t * trimmed_line = NULL;
      int col = 0;
      int row = 0;
      show_table.column_cnt = 0;
      wcsncpy_s(show_table.name, COLUMN_HEADER_SZ, display_name, MAX_TABLE_ITEM_LEN);

      //*************************************************************************
      //Parse listview output format into dictionary data structs
      //*************************************************************************
      //
      //get column names (used as key names)
      if (fgetws(line, READ_FD_LINE_SZ, fd) != NULL)
      {
	trimmed_line = trimwhitespace(line);
         line_sz_chars = wcsnlen_s(trimmed_line, READ_FD_LINE_SZ);
         tok = s_wcstok(trimmed_line, &line_sz_chars, (TableView == type) ? TABLE_TOK_DELIM : TABBED_TABLE_TOK_DELIM, &state);
         if (NULL != tok)
         {
            wcsncpy_s(show_table.columns[col].header_name, COLUMN_HEADER_SZ, tok, MAX_TABLE_ITEM_LEN);
            ++col;
            show_table.column_cnt = col;
            while (NULL != (tok = s_wcstok(NULL, &line_sz_chars, (TableView == type) ? TABLE_TOK_DELIM : TABBED_TABLE_TOK_DELIM, &state)))
            {
              wcsncpy_s(show_table.columns[col].header_name, COLUMN_HEADER_SZ, tok, MAX_TABLE_ITEM_LEN);
               ++col;
            }
            show_table.column_cnt = col;
         }
      }
      //malformed table (no columns)
      if (col == 0)
         goto FinishError;

      //iterate through all of the rows in the table
      while (num_dictionaries < NUM_DICTIONARIES_MAX && NULL != fgetws(line, READ_FD_LINE_SZ, fd))
      {
         trimmed_line = trimwhitespace(line);
         line_sz_chars = wcsnlen_s(trimmed_line, READ_FD_LINE_SZ);
         //using the coresponding column header saved above as the key
         col = 0;
         tok = s_wcstok(trimmed_line, &line_sz_chars, (TableView == type) ? TABLE_TOK_DELIM : TABBED_TABLE_TOK_DELIM, &state);
         if (NULL != tok)
         {
            //get first column row-data
            wcsncpy_s(cur_dict->items[cur_dict->item_cnt].name, PAIR_NAME_SZ, show_table.columns[col].header_name, MAX_TABLE_ITEM_LEN);
            wcsncpy_s(cur_dict->items[cur_dict->item_cnt].value, PAIR_VALUE_SZ, tok, MAX_TABLE_ITEM_LEN);

            cur_dict->item_cnt++;
            col++;

            //start iterating through the rest of the columns in our row
            while (NULL != (tok = s_wcstok(NULL, &line_sz_chars, (TableView == type) ? TABLE_TOK_DELIM : TABBED_TABLE_TOK_DELIM, &state)))
            {
               //malformed table (no matching column header)
               if (col > show_table.column_cnt)
               {
                  goto Parsing_done;
               }

               if (special_tok(tok))
               {
                  wchar_t tmp_val[1024];
                  int cat_tok_size = (int)wcsnlen(cur_dict->items[cur_dict->item_cnt-1].value, READ_FD_LINE_SZ) * sizeof(wchar_t) + sizeof(wchar_t); //+1 for null term
                  cat_tok_size += (int)wcsnlen(tok, READ_FD_LINE_SZ) * sizeof(wchar_t) + sizeof(wchar_t); //+1 for whitespace
                  swprintf_s(tmp_val, 1024, L"%ls %ls", cur_dict->items[cur_dict->item_cnt-1].value, tok);
                  wcsncpy_s(cur_dict->items[cur_dict->item_cnt - 1].value, PAIR_VALUE_SZ, tmp_val, MAX_TABLE_ITEM_LEN);
               }
               else
               {
                  wcsncpy_s(cur_dict->items[cur_dict->item_cnt].name, PAIR_NAME_SZ, show_table.columns[col].header_name, MAX_TABLE_ITEM_LEN);
                  wcsncpy_s(cur_dict->items[cur_dict->item_cnt].value, PAIR_VALUE_SZ, tok, MAX_TABLE_ITEM_LEN);
                  ++col;
                  cur_dict->item_cnt++;
               }
            }
            ++row;
         }
         //new row, means a new dictionary...
         ++num_dictionaries;
         cur_dict = &dictionaries[num_dictionaries];
      }
   Parsing_done:
      if (NvmXmlType == out_type)
      {
         output_to_nvm_xml_list(display_name, dictionaries, num_dictionaries);
      }
      else if (EsxXmlType == out_type)
      {
         output_to_esx_xml_list(display_name, dictionaries, num_dictionaries);
      }
      free(dictionaries);
      return 0;
   }
   else if(ListView == type)
   {
      int is_nested_list = 0;

      //Parse listview output format into dictionary data structs
      while (num_dictionaries < NUM_DICTIONARIES_MAX && fgetws(line, READ_FD_LINE_SZ, fd) != NULL)
      {
         wchar_t * trimmed_line = trimwhitespace(line);
         wchar_t *delims;
         if (display_delims && wcslen(display_delims))
         {
           delims = display_delims;
         }
         else
         {
           delims = KEY_VALUE_TOK_DELIM;
         }
         line_sz_chars = wcsnlen_s(trimmed_line, READ_FD_LINE_SZ);

         if(0 == starts_with(trimmed_line, NEW_BRANCH_IDENTIFIER) && 0 == ends_with(trimmed_line, NEW_BRANCH_IDENTIFIER))
         {
            is_nested_list = 1;
            cur_dict = &dictionaries[num_dictionaries];
            ++num_dictionaries;
            trimmed_line = trimchar(trimmed_line, trimmed_line[0]);
         }
         tok = s_wcstok(trimmed_line, &line_sz_chars, delims, &state);
         if (NULL == tok)
         {
            continue;
         }
         wcsncpy_s(cur_dict->items[cur_dict->item_cnt].name, PAIR_NAME_SZ, tok, MAX_TABLE_ITEM_LEN);
         tok = s_wcstok(NULL, &line_sz_chars, delims, &state);
         if (NULL == tok)
         {
           continue;
         }
         wcsncpy_s(cur_dict->items[cur_dict->item_cnt].value, PAIR_VALUE_SZ, tok, MAX_TABLE_ITEM_LEN);
         cur_dict->item_cnt++;
      }

      //Transform data structures to XML
      if (num_dictionaries == 0)
         num_dictionaries = 1;

      if (is_nested_list)
      {
         if (NvmXmlType == out_type)
         {
            output_to_nvm_xml_list(display_name, dictionaries, num_dictionaries);
         }
         else if (EsxXmlType == out_type)
         {
            output_to_esx_xml_list(display_name, dictionaries, num_dictionaries);
         }
      }
      else
      {
         if (NvmXmlType == out_type)
         {
            output_to_nvm_xml_key_val_pairs(display_name, dictionaries);
         }
         else if (EsxXmlType == out_type)
         {
            output_to_esx_xml_key_val_pairs(display_name, dictionaries);
         }
      }
      free(dictionaries);
      return 0;
   }
   else if (ListView2L == type)
   {
      struct pair parent_node;
      //Parse listview output format into dictionary data structs
      while (num_dictionaries < NUM_DICTIONARIES_MAX && fgetws(line, READ_FD_LINE_SZ, fd) != NULL)
      {
         wchar_t * trimmed_line = trimwhitespace(line);
         if (0 == starts_with(trimmed_line, NEW_BRANCH_IDENTIFIER) && 0 == ends_with(trimmed_line, NEW_BRANCH_IDENTIFIER))
         {
            trimmed_line = trimchar(trimmed_line, trimmed_line[0]);
            //tokenize and temporarily store this node's key/value
            tokenize_and_copy_key_value_pair(
               trimmed_line,
               parent_node.name,
               PAIR_NAME_SZ,
               parent_node.value,
               PAIR_VALUE_SZ);
            continue;
         }
         else if (0 == starts_with(trimmed_line, NEW_BRANCH_IDENTIFIER) && 0 != ends_with(trimmed_line, NEW_BRANCH_IDENTIFIER))
         {
            cur_dict = &dictionaries[num_dictionaries];
            ++num_dictionaries;
            trimmed_line = trimchar(trimmed_line, trimmed_line[0]);
            //add parent node to current dictionary
            wcsncpy_s(cur_dict->items[cur_dict->item_cnt].name, PAIR_NAME_SZ, parent_node.name, MAX_TABLE_ITEM_LEN);
            wcsncpy_s(cur_dict->items[cur_dict->item_cnt].value, PAIR_VALUE_SZ, parent_node.value, MAX_TABLE_ITEM_LEN);

            //added parent to cur dictionary, so increment cnt
            cur_dict->item_cnt++;
         }

         tokenize_and_copy_key_value_pair(
            trimmed_line,
            cur_dict->items[cur_dict->item_cnt].name,
            PAIR_NAME_SZ,
            cur_dict->items[cur_dict->item_cnt].value,
            PAIR_VALUE_SZ);

         cur_dict->item_cnt++;
      }
      //Transform data structures to XML
      if (num_dictionaries == 0)
         num_dictionaries = 1;
      if (NvmXmlType == out_type)
      {
         output_to_nvm_xml_list(display_name, dictionaries, num_dictionaries);
      }
      else if (EsxXmlType == out_type)
      {
         output_to_esx_xml_list(display_name, dictionaries, num_dictionaries);
      }
      free(dictionaries);
      return 0;
   }
   else if (ResultsView == type)
   {
      if (NvmXmlType == out_type)
      {
         output_to_nvm_xml_results(fd);
      }
      else if (EsxXmlType == out_type)
      {
         output_to_esx_xml_results(fd);
      }
      return 0;
   }
   else if (DiagView == type)
   {
      //Parse listview output format into dictionary data structs
      while (num_dictionaries < NUM_DICTIONARIES_MAX && fgetws(line, READ_FD_LINE_SZ, fd) != NULL)
      {
         wchar_t * trimmed_line = trimwhitespace(line);
         line_sz_chars = wcsnlen_s(trimmed_line, READ_FD_LINE_SZ);
         if (0 == starts_with(trimmed_line, L"Message:"))
         {
            wcsncpy_s(cur_dict->items[cur_dict->item_cnt].name, PAIR_NAME_SZ, L"Message", MAX_TABLE_ITEM_LEN);
            while (fgetws(line, READ_FD_LINE_SZ, fd) != NULL)
            {
               trimmed_line = trimwhitespace(line);
               if (0 == starts_with(trimmed_line, L"TestName:"))
               {
                  cur_dict->item_cnt++;
                  goto TestName;
               }
               else
               {
                  size_t cur_len = wcsnlen(cur_dict->items[cur_dict->item_cnt].value, PAIR_VALUE_SZ) + 1;
                  wcsncat_s(
                     cur_dict->items[cur_dict->item_cnt].value,
                     PAIR_VALUE_SZ,
                     trimmed_line,
                     PAIR_VALUE_SZ - cur_len);
               }
            }
            cur_dict->item_cnt++;
            continue;
         }
TestName:
         if (0 == starts_with(trimmed_line, L"TestName:"))
         {
            cur_dict = &dictionaries[num_dictionaries];
            ++num_dictionaries;
         }
        
         state = NULL;
         line_sz_chars = wcsnlen(trimmed_line, READ_FD_LINE_SZ);
         tok = s_wcstok(trimmed_line, &line_sz_chars, KEY_VALUE_TOK_DELIM, &state);
         wcsncpy_s(cur_dict->items[cur_dict->item_cnt].name, PAIR_NAME_SZ, tok, MAX_TABLE_ITEM_LEN);
         tok = s_wcstok(NULL, &line_sz_chars, KEY_VALUE_TOK_DELIM, &state);
         wcsncpy_s(cur_dict->items[cur_dict->item_cnt].value, PAIR_VALUE_SZ, tok, MAX_TABLE_ITEM_LEN);
         cur_dict->item_cnt++;
      }
      //Transform data structures to XML
      if (num_dictionaries == 0)
         num_dictionaries = 1;

      if (NvmXmlType == out_type)
      {
         output_to_nvm_xml_list(display_name, dictionaries, num_dictionaries);
      }
      else if (EsxXmlType == out_type)
      {
         output_to_esx_xml_list(display_name, dictionaries, num_dictionaries);
      }
      free(dictionaries);
      return 0;
   }

//only go here if nothing has been printed out to stdout
FinishError:
   fseek(fd, 0, SEEK_SET);
   if (NvmXmlType == out_type)
   {
      output_to_nvm_xml_error(fd, rc);
   }
   else if (EsxXmlType == out_type)
   {
      output_to_esx_xml_error(fd, rc);
   }

   if (dictionaries)
   {
      free(dictionaries);
   }
   return -1;
}
