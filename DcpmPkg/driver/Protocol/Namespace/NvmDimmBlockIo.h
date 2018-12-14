/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /**
 * @file NvmDimmBlockIo.h
 * @brief Implementation of the EFI_BLOCK_IO_PROTOCOL for
 * Intel(R) Optane(TM) DC Persistent Memory Module Namespaces.
 */

#ifndef _NVM_DIMM_BLOCKIO_H_
#define _NVM_DIMM_BLOCKIO_H_

#include "NvmDimmDriver.h"
#include "NvmTypes.h"
#include "Namespace.h"

#define GET_NAMESPACE_INSANCE(InstanceAddress) BASE_CR(InstanceAddress, NAMESPACE, BlockIoInstance)

/**
  Read BufferSize bytes from Lba into Buffer.

  @param  pThis       Indicates a pointer to the calling context.
  @param  MediaId    Id of the media, changes every time the media is replaced.
  @param  Lba        The starting Logical Block Address to read from
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  pBuffer     A pointer to the destination buffer for the data. The caller is
   responsible for either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the read.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The read request contains LBAs that are not valid,
   or the buffer is not on proper alignment.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverBlockIoReadBlocks(
  IN     EFI_BLOCK_IO_PROTOCOL *pThis,
  IN     UINT32 MediaId,
  IN     EFI_LBA Lba,
  IN     UINTN BufferSize,
     OUT VOID *pBuffer
);

/**
  Write BufferSize bytes from Lba into Buffer.

  @param  pThis       Indicates a pointer to the calling context.
  @param  MediaId    The media ID that the write request is for.
  @param  Lba        The starting logical block address to be written. The caller is
   responsible for writing to only legitimate locations.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  pBuffer     A pointer to the source buffer for the data.

  @retval EFI_SUCCESS           The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED   The device can not be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not matched the current device.
  @retval EFI_BAD_BUFFER_SIZE   The Buffer was not a multiple of the block size of the device.
  @retval EFI_INVALID_PARAMETER The write request contains LBAs that are not valid,
   or the buffer is not on proper alignment.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverBlockIoWriteBlocks(
  IN     EFI_BLOCK_IO_PROTOCOL *pThis,
  IN     UINT32 MediaId,
  IN     EFI_LBA Lba,
  IN     UINTN BufferSize,
  IN     VOID *pBuffer
);

/**
  Flush the Block Device.

  @param  pThis              Indicates a pointer to the calling context.

  @retval EFI_SUCCESS       All outstanding data was written to the device
  @retval EFI_DEVICE_ERROR  The device reported an error while writing back the data
  @retval EFI_NO_MEDIA      There is no media in the device.
  @retval EFI_UNSUPPORTED   Not supported
**/
EFI_STATUS
EFIAPI
NvmDimmDriverBlockIoFlushBlocks(
  IN     EFI_BLOCK_IO_PROTOCOL *pThis
);

/**
  Reset the block device hardware.

  @param[in]  pThis                 Indicates a pointer to the calling context.
  @param[in]  ExtendedVerification Indicates that the driver may perform a more
   exhaustive verification operation of the device
  during reset.

  @retval EFI_SUCCESS          The device was reset.
  @retval EFI_DEVICE_ERROR     The device is not functioning properly and could
   not be reset.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverBlockIoReset(
  IN     EFI_BLOCK_IO2_PROTOCOL *pThis,
  IN     BOOLEAN ExtendedVerification
);

extern EFI_GUID gNvmDimmBlockIoProtocolGuid;

#endif /** _NVM_DIMM_BLOCKIO_H_ **/
