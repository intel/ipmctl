/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FILETRAVERSE_H_
#define _FILETRAVERSE_H_

#include <Types.h>
#include <NvmTypes.h>
#include <Uefi.h>
#include <Debug.h>

/**
  Returns the Unicode file name for the given EFI_FILE_PROTOCOL file instance.

  @param[in] pFile pointer to the EFI_FILE_PROTOCOL instance
  @param[out] ppFileName pointer to the memory, where the result file name will be stored

  The memory for the result is allocated in this function and it is the callers responsibility
  to free the memory with FreePool.

  @retval EFI_SUCCESS on successful name retrieve.
  @retval EFI_NOT_FOUND when the file information could not be read.
  @retval EFI_OUT_OF_RESOURCES when the memory allocation failed.
**/
EFI_STATUS
GetFileName(
  IN     EFI_FILE_PROTOCOL *pFile,
     OUT CHAR16 **ppFileName
  );

/**
  Checks if the provided file is a directory.

  @param[in] pFile pointer to the EFI_FILE_PROTOCOL that needs to be checked

  @retval EFI_SUCCESS if the file is a directory
  @retval EFI_INVALID_PARAMETER if the file could not be processed
  @retval EFI_NOT_FOUND if the file is processed properly, but it is not a directory
**/
EFI_STATUS
IsDirectory(
  IN     EFI_FILE_PROTOCOL *pFile
  );

/**
  Returns an array of the Simple File System Protocols found in the system

  If the ppSimpleFS is passed as NULL, then only the count will be returned.

  @param[out] ppSimpleFS the result array where the pointers will be stored OPTIONAL
  @param[in, out] pArraySize size of the input array size/required array size

  @retval EFI_SUCCESS everything works properly
  @retval EFI_INVALID_PARAMETER when pArraySize equals NULL
  @retval EFI_NOT_FOUND no Simple File System Protocols could be found in the system
  @retval EFI_BUFFER_TOO_SMALL the specified array size is too small, the required value is stored in the pArraySize
  @retval EFI_DEVICE_ERROR some Simple File System Protocol instances were found but they could not be opened
**/
EFI_STATUS
GetSfsList(
     OUT EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **ppSimpleFS OPTIONAL,
  IN OUT UINT32 *pArraySize
  );

/**
  Converts a File Info Protocol array into a File Protocol array by opening all of the files in read mode.

  If ppFilesArray is NULL, then only the pFilesCountArraySize will be returned.

  @param[in] pRootDir
  @param[out] ppFilesArray OPTIONAL
  @param[in, out] pFilesCountArraySize

  @retval EFI_SUCCESS everything went successfully
  @retval EFI_INVALID_PARAMETER at least one of the non-optional parameters equals NULL
  @retval EFI_OUT_OF_RESOURCES a memory allocation failed
  @retval EFI_DEVICE_ERROR reading the file system failed
  @retval EFI_BUFFER_TOO_SMALL the pFilesCountArraySize reports an array too small for the actual array size
                                the required size will be stored in pFilesCountArraySize after the call
**/
EFI_STATUS
GetSubFileProtocolFromDir(
  IN     EFI_FILE_PROTOCOL *pRootDir,
     OUT EFI_FILE_PROTOCOL **ppFilesArray OPTIONAL,
  IN OUT UINT32 *pFilesCountArraySize
  );

/**
  Opens the root file system directory for the given Simple File System Protocol

  @param[in] pSfsProtocol the Simple File System Protocol instance to open the root directory from
  @param[out] ppRootDir the result root directory

  @retval EFI_SUCCESS everything worked successfully
  @retval EFI_INVALID_PARAMETER at least one of the input parameters equals NULL
  Other return codes from the EFI_SIMPLE_FILE_SYSTEM_PROTOCOL->OpenVolume function
**/
EFI_STATUS
GetRootDirFromSfs(
  IN     EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pSfsProtocol,
     OUT EFI_FILE_PROTOCOL **ppRootDir
  );

#endif /** _FILETRAVERSE_H_ **/
