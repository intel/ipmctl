/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
* This file contains the implementation of the common ini file parser.
*/

#ifndef _INI_H_
#define _INI_H_

#include <export_api.h>

/**
@brief Ini dictionary main object, contains all values and keys stored
in the ini file.
*/
typedef struct _dictionary {
  struct _dictionary  *p_next;          // Pointer to the next entry
  int                 numb_of_entries;  // Number of entries in dictionary
  char                *p_value;         // Pointer to string - value
  char                *p_key;           // Pointer to string - key
  char                *p_comment;       // Pointer to string - comment
} dictionary;

/**
@brief    Open/Create ini file and parse it
@param    pp_dictionray Pointer to the dictionary context pointer
@param    p_ini_file_name Pointer to the name of the ini file to read
@return   Pointer to newly allocated dictionary - DO NOT MODIFY IT!
          In case of error a NULL pointer is being returned
*/
dictionary *nvm_ini_load_dictionary(dictionary **pp_dictionary, const char *p_ini_file_name);

/**
@brief    Free the dictionary structure
@param    p_dictionary Dictionary to free
@return   void
*/
NVM_API void nvm_ini_free_dictionary(dictionary *p_dictionary);

/**
@brief    Get the string associated to a key and convert to an int
@param    p_dictionary Pointer to the dictionary
@param    p_key Pointer to the key string to look for
@param    fail_value Value returned in case of error
@return   int value if Ok, fail_value otherwise
*/
NVM_API int nvm_ini_get_int_value(dictionary *p_dictionary, const char *p_key, int fail_value);

/**
@brief    Get the string associated to a key
@param    p_dictionary Pointer to the dictionary
@param    p_key Pointer to the key string to look for
@return   pointer to statically allocated character string - DO NOT CHANGE IT!
*/
NVM_API const char * nvm_ini_get_string(dictionary *p_dictionary, const char *p_key);

/**
@brief    Modify value in the dictionary
@param    p_dictionary Pointer to the dictionary
@param    p_key Pointer to the key string to modify
@param    p_value Pointer to new value to be set
@return   int 0 if Ok, -1 otherwise
*/
NVM_API int nvm_ini_set_value(dictionary *p_dictionary, const char *p_key, const char *p_value);

/**
@brief    Dump the dictionary to the file
@param    p_dictionary Pointer to the dictionary
@param    p_filename Pointer to the file name
@return   int 0 if Ok, -1 otherwise
*/
NVM_API int nvm_ini_dump_to_file(dictionary *p_dictionary, const char *p_filename, int force_file_update);

#endif // !_INI_H_
