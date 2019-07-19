/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /*
 * This file contains the implementation of the common ini file parser.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Debug.h>
#include "ini.h"
#include <s_str.h>
#include "os.h"
#include "os_str.h"

#if defined(__LINUX__) || defined(__ESX__)
#define APP_DATA_FILE_PATH    ""
#ifndef INI_INSTALL_FILEPATH
  #define INI_INSTALL_FILEPATH	"/usr/share/ipmctl/"
#endif // INI_INSTALL_FILEPATH
#else
char * get_win_appdata_ptr(void)
{
  char * p_appdata = NULL;
  if ((p_appdata = getenv("APPDATA")) == NULL)
    return "";
  else
    return p_appdata;
}

#define APP_DATA_FILE_PATH    get_win_appdata_ptr()
#define INI_INSTALL_FILEPATH	"\\Intel\\ipmctl\\"
#define W_OK			0x2
#endif

/**
@brief  The default ini file content defined by the ipmctl_default.conf file
*/
#if defined(__LINUX__) || defined(__ESX__)
#define TEMP_FILE_PATH "/var/log/ipmctl/"
#else
#define TEMP_FILE_PATH "%APPDATA%\\Intel\\ipmctl\\"
#endif
const char p_g_ini_file[] = {
#include "ipmctl_default.h"
};

/**
@brief Allocate memory and initialize it to zero; if the size is 1 or less
means we have either token only or end of line or nothing.
An extra byte is for terminating zero purpose.
*/
#define NVM_INI_CALLOC(size) (size > 0) ? (char *) calloc(1, size+1) : NULL 

/**
@brief Allocate memory and initialize it to zero; if the size is 1 or less
means we have either token only or end of line or nothing
*/
#define NVM_INI_FREE(pointer) if(NULL != pointer) { \
    free(pointer); \
    pointer = NULL; \
  }

/**
@brief Copy the src to dst strings and strip out the new line, leading space
and ending spac characters form the string
*/
static void nvm_str_cat(char *dst_str, char *src_str, size_t src_size)
{
  size_t start_src_size = strlen(src_str);

  if (start_src_size > src_size) {
    return;
  }
  src_size = start_src_size;

  while (src_size) {
    if ('\n' != *src_str) {
      if (((' ' != *src_str) && ('\t' != *src_str)) || ((start_src_size != src_size) && (1 != src_size))) {
        *dst_str = *src_str;
        dst_str++;
      }
    }
    src_str++;
    src_size--;
  }
  *dst_str = 0; // zero termination
}

/**
*/
inline static int nvm_set_value(char **pp_dest_val, const char *p_src_val, size_t string_size)
{
  // Free the destination pointer if needed
  NVM_INI_FREE(*pp_dest_val);
  // Allocate new memory
  *pp_dest_val = NVM_INI_CALLOC(string_size);
  if (*pp_dest_val) {
    // Copy the source to the destination
    nvm_str_cat(*pp_dest_val, (char *)p_src_val, string_size);
    return 0;
  }
  return -1;
}

/**
@brief Find the key in the dictionary and reuturn the value
*/
inline static const char * nvm_dictionary_get_set_value(dictionary *p_dict, const char *p_key, const char *p_value)
{
  size_t string_size;

  if ((NULL == p_key) || (NULL == p_dict)) {
    return NULL;
  }
  while (p_dict) {
    string_size = (size_t)(strlen(p_key));
    if (p_dict->p_key && (0 == strncmp(p_key, p_dict->p_key, string_size))) {
      if (NULL == p_value) {
        // Get function call
        return p_dict->p_value;
      }
      else {
        // Set function call
        string_size = (size_t)(strlen(p_value));
        if (0 == nvm_set_value(&p_dict->p_value, p_value, string_size)) {
          // Value set successfuly
          return p_dict->p_value;
        }
        return NULL;
      }
    }
    else {
      p_dict = p_dict->p_next;
    }
  }
  return NULL;
}

/**
@brief Convert the ascii string value, hex or decimal to int.
Int value returned on success or -1 otherwise.
*/
inline static int nvm_convert_string_to_int(char *p_value, int fail_value)
{
  BOOLEAN negative_value = FALSE;

  // Ignore '-' character cause negative values are not supported
  if ('-' == *p_value) {
    p_value++;
    negative_value = TRUE;
  }
  // Ignore non number strings
  if (('0' > *p_value) || ('9' < *p_value)) {
    return fail_value;
  }
  // Check if it is hex or decimal value
  if (('0' == *p_value) && (('x' == *(p_value + 1)) || ('X' == *(p_value + 1)))) {
    return (int)AsciiStrHexToUintn(p_value);
  }
  else {
    if (negative_value) {
      unsigned int value = (unsigned int)AsciiStrDecimalToUintn(p_value);
      return 0 - value;
    }
    else {
      return (int)AsciiStrDecimalToUintn(p_value);
    }
  }
}

/**
@brief Copy line form the buffer - fgets for buffer
*/
inline static char *sgets(char *dest, int len, char *src)
{
  if ((NULL == dest) || (NULL == src) || (0 == len)) {
    return NULL;
  }

  // Copy data
  while ((*src != '\n') && (*src != 0) && (len > 0)) {
    *dest++ = *src++;
    len--;
  }

  if (*src == '\n') {
    if (len > 0) {
      *dest++ = *src;
      len--;
    }
    if (len == 0) {
      dest--; // Destination buffer is too small,
              // replace the last char with null terminator
    }
    *dest = 0;
    // Skip end of line character
    src++;
  }
  else if ((*src == 0) || (len == 0)) {
    *dest = 0;
    // End of the buffer
    src = NULL;
  }

  return src;
}

/**
Global definitions and typedefs and macros
*/
#define NVM_INI_VALUE_TOKEN   "="
#define NVM_INI_COMMENT_TOKEN "#"
#define NVM_INI_ENTRY_LEN     1024
#define NVM_INI_PATH_FILE_LEN 1024
typedef char NVM_INI_ENTRY[NVM_INI_ENTRY_LEN]; // Ini entry string
typedef char NVM_INI_FILENAME[NVM_INI_PATH_FILE_LEN]; // Ini entry string
typedef wchar_t NVM_INI_FILENAME_W[NVM_INI_PATH_FILE_LEN];

/**
@brief    Open/Create ini file and parse it
@param    pp_dictionray Pointer to the dictionary context pointer
@param    p_ini_file_name Pointer to the name of the ini file to read
@return   Pointer to newly allocated dictionary - DO NOT MODIFY IT!
          In case of error a NULL pointer is being returned
*/
dictionary *nvm_ini_load_dictionary(dictionary **pp_dictionary, const char *p_ini_file_name)
{
  FILE *h_file = NULL;
  NVM_INI_ENTRY ini_entry_string = { 0 };
  char *p_key = NULL;
  char *p_value = NULL;
  char *p_comment = NULL;
  char *p_tok_context = NULL;
  dictionary *p_current_entry = NULL;
  size_t string_size = 0;
  NVM_INI_FILENAME ini_path_filename = { 0 };
  BOOLEAN no_conf_file = FALSE;
  char *ret_ptr = NULL;
  long file_size = 0;

  // Check inputs
  if ((NULL == pp_dictionary) || (NULL == p_ini_file_name)) {
    return NULL;
  }

  // Check if the dictionary is already loaded
  if (NULL != *pp_dictionary) {
    goto Finish;
  }

  // Try to open the file
  snprintf(ini_path_filename, sizeof(ini_path_filename), "%s", p_ini_file_name);
  h_file = fopen(ini_path_filename, "r");
  if (NULL == h_file) {
    snprintf(ini_path_filename, sizeof(ini_path_filename), "%s%s%s", APP_DATA_FILE_PATH, INI_INSTALL_FILEPATH, p_ini_file_name);
    h_file = fopen(ini_path_filename, "r");
    if (NULL == h_file) {
      // File does not exists, lets use hardcoded data
      ret_ptr = (char *) p_g_ini_file;
      no_conf_file = TRUE;
    }
  }

  // Create the dictionary context
  if (FALSE == no_conf_file) {
    // Check the file size
    fseek(h_file, 0, SEEK_END);
    file_size = ftell(h_file);
    fseek(h_file, 0, SEEK_SET);
    if (0 == file_size) {
      // File size improper, lets use hardcoded data
      ret_ptr = (char *) p_g_ini_file;
      no_conf_file = TRUE;
    }
    else {
      // Get line form the file
      ret_ptr = fgets(ini_entry_string, sizeof(ini_entry_string), h_file);
    }
  }
  if (TRUE == no_conf_file) {
    // Create dictionary context based on hardcoded data
    ret_ptr = sgets(ini_entry_string, sizeof(ini_entry_string), ret_ptr);
  }
  while (ret_ptr != NULL) {
    // Allocate next entry
    if (NULL == *pp_dictionary) {
      *pp_dictionary = (dictionary *)calloc(1, sizeof(dictionary));
      if (NULL == *pp_dictionary) {
        goto Finish;
      }
      p_current_entry = *pp_dictionary;
    }
    else {
      p_current_entry->p_next = (dictionary *)calloc(1, sizeof(dictionary));
      if (NULL == p_current_entry->p_next) {
        // Free already allocated memory
        nvm_ini_free_dictionary(*pp_dictionary);
        *pp_dictionary = NULL;
        goto Finish;
      }
      p_current_entry = p_current_entry->p_next;
    }
    // Parse the line
    p_key = os_strtok(ini_entry_string, NVM_INI_VALUE_TOKEN, &p_tok_context);
    if (NULL == p_key) {
      p_key = ini_entry_string;
      p_value = ini_entry_string;
    }
    else {
      p_value = os_strtok(NULL, NVM_INI_VALUE_TOKEN, &p_tok_context);
      if (NULL == p_value) {
        p_value = ini_entry_string;
      }
    }
    p_tok_context = NULL;
    p_comment = os_strtok(p_value, NVM_INI_COMMENT_TOKEN, &p_tok_context);
    if (NULL == p_comment) {
      p_comment = (char *)(p_value + strlen(p_value));
    }
    else if (p_value == p_comment) {
      p_comment = os_strtok(NULL, NVM_INI_COMMENT_TOKEN, &p_tok_context);
      if (NULL == p_comment) {
        p_comment = (char *)(p_value + strlen(p_value));
      }
    }
    string_size = (size_t)(p_value - p_key);
    nvm_set_value(&p_current_entry->p_key, p_key, string_size);
    string_size = (size_t)(p_comment - p_value);
    nvm_set_value(&p_current_entry->p_value, p_value, string_size);
    string_size = (size_t)(strlen(p_comment));
    nvm_set_value(&p_current_entry->p_comment, p_comment, string_size);
    // Update the total count and switch to next entry
    (*pp_dictionary)->numb_of_entries += 1;
    // Get another entry
    if (TRUE == no_conf_file) {
      // Get another line from hardcoded buffer
      ret_ptr = sgets(ini_entry_string, sizeof(ini_entry_string), ret_ptr);
    }
    else {
      // Get another line form the file
      ret_ptr = fgets(ini_entry_string, sizeof(ini_entry_string), h_file);
    }
  }

Finish:
  // Close the file
  if (NULL != h_file) {
    fclose(h_file);
  }

  return *pp_dictionary;
}

/**
@brief    Free the dictionary structure
@param    p_dictionary Dictionary to free
@return   void
*/
void nvm_ini_free_dictionary(dictionary *p_dictionary)
{
  dictionary *p_current_entry = p_dictionary;
  dictionary *p_previous_entry;

  while (p_current_entry) {
    NVM_INI_FREE(p_current_entry->p_key);
    NVM_INI_FREE(p_current_entry->p_value);
    NVM_INI_FREE(p_current_entry->p_comment);
    p_previous_entry = p_current_entry;
    p_current_entry = p_current_entry->p_next;
    NVM_INI_FREE(p_previous_entry);
  }
}

/**
@brief    Get the string associated to a key and convert to an int
@param    p_dictionary Pointer to the dictionary
@param    p_key Pointer to the key string to look for
@param    fail_value Value returned in case of error
@return   int value if Ok, fail_value otherwise
*/
int nvm_ini_get_int_value(dictionary *p_dictionary, const char *p_key, int fail_value)
{
  const char *p_value = NULL;

  // The nvm_dictionary_get_set_value function checks the inputs

  p_value = nvm_dictionary_get_set_value(p_dictionary, p_key, NULL);
  if (NULL == p_value) {
    return fail_value;
  }
  else {
    return nvm_convert_string_to_int((char *)p_value, fail_value);
  }
}

/**
@brief    Get the string associated to a key
@param    p_dictionary Pointer to the dictionary
@param    p_key Pointer to the key string to look for
@return   pointer to statically allocated character string - DO NOT CHANGE IT!
*/
const char * nvm_ini_get_string(dictionary *p_dictionary, const char *p_key)
{
  // The nvm_dictionary_get_set_value function checks the inputs

  return nvm_dictionary_get_set_value(p_dictionary, p_key, NULL);
}

/**
@brief    Modify value in the dictionary
@param    p_dictionary Pointer to the dictionary
@param    p_key Pointer to the key string to modify
@param    p_value Pointer to new value to be set
@return   int 0 if Ok, -1 otherwise
*/
int nvm_ini_set_value(dictionary *p_dictionary, const char *p_key, const char *p_value)
{
  const char *p_key_value = NULL;

  // The nvm_dictionary_get_set_value function checks the inputs

  p_key_value = nvm_dictionary_get_set_value(p_dictionary, p_key, p_value);
  if (NULL == p_key_value) {
    wprintf(L"Error: Could not find preference in configuration file\n");
    return -1;
  }

  return 0;
}

/**
@brief    Dump the dictionary to the file
@param    p_dictionary Pointer to the dictionary
@param    p_ini_file_name Pointer to the name of the ini file to read
@return   int 0 if Ok, -1 otherwise
*/
int nvm_ini_dump_to_file(dictionary *p_dictionary, const char *p_ini_file_name, int force_file_update)
{
  FILE *h_file;
  NVM_INI_FILENAME ini_path_filename = { 0 };

  // Check inputs
  if ((NULL == p_dictionary) || (NULL == p_ini_file_name)) {
    return -1;
  }

  // nothing to do if no entries or no modified values
  if (p_dictionary->numb_of_entries == 0) {
    return 0;
  }

  // Open and truncated the file
  // Try to open the file
  snprintf(ini_path_filename, sizeof(ini_path_filename), "%s", p_ini_file_name);
  h_file = fopen(ini_path_filename, "r");
  if ((NULL == h_file) || (NULL == (h_file = freopen(ini_path_filename, "w", h_file)))) {
    if (force_file_update) {
      snprintf(ini_path_filename, sizeof(ini_path_filename), "%s%s", APP_DATA_FILE_PATH, INI_INSTALL_FILEPATH);
      os_mkdir(ini_path_filename);
      snprintf(ini_path_filename, sizeof(ini_path_filename), "%s%s%s", APP_DATA_FILE_PATH, INI_INSTALL_FILEPATH, p_ini_file_name);
      h_file = fopen(ini_path_filename, "w");
    }
    else {
      snprintf(ini_path_filename, sizeof(ini_path_filename), "%s%s%s", APP_DATA_FILE_PATH, INI_INSTALL_FILEPATH, p_ini_file_name);
      h_file = fopen(ini_path_filename, "r");
    }
    if ((NULL == h_file) || (NULL == (h_file = freopen(ini_path_filename, "w", h_file)))) {
      // Hardcoded data used, nothing to save
      return -1;
    }
  }

  // Copy the dictionary to the file
  while (p_dictionary) {
    if (p_dictionary->p_key && p_dictionary->p_value && p_dictionary->p_comment) {
      fprintf(h_file, "%s = %s # %s\n", p_dictionary->p_key, p_dictionary->p_value, p_dictionary->p_comment);
    }
    else if (p_dictionary->p_key && p_dictionary->p_value) {
      fprintf(h_file, "%s = %s\n", p_dictionary->p_key, p_dictionary->p_value);
    }
    else if (p_dictionary->p_comment) {
      fprintf(h_file, "# %s\n", p_dictionary->p_comment);
    }
    else {
      fprintf(h_file, "\n");
    }
    p_dictionary = p_dictionary->p_next;
  }

  // Close the file
  fclose(h_file);

  return 0;
}
