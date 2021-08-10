/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NvmDimmBlockIo.h"

EFI_GUID gNvmDimmBlockIoProtocolGuid = EFI_BLOCK_IO_PROTOCOL_GUID;

/**
  Block I/O Media structure
 **/
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_BLOCK_IO_MEDIA gNvmDimmDriverBlockIoMedia =
{
  0,      //!< MediaId
  FALSE,  //!< RemovableMedia
  TRUE,   //!< MediaPresent
  FALSE,  //!< LogicalPartition
  FALSE,  //!< ReadOnly
  TRUE,   //!< WriteCaching
  512,    //!< BlockSize - will be overwritten by the namespace actual size
  /**
    We do not require any particular IO buffers alignment.
    The alignment below is a standard one. If in future requested, this value can be changed.
  **/
  2,      //!< IoAlign
  0,      //!< LastBlock
  0,      //!< LowestAlignedLba
  1,      //!< LogicalBlocksPerPhysicalBlock
  512     //!< OptimalTransferLengthGranularity
};

/**
  Block I/O Protocol Instance
 **/
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_BLOCK_IO_PROTOCOL gNvmDimmDriverBlockIo =
{
  EFI_BLOCK_IO_PROTOCOL_REVISION3,              //!< Revision
  &gNvmDimmDriverBlockIoMedia,                  //!< Media
  (EFI_BLOCK_RESET) NvmDimmDriverBlockIoReset,  //!< Reset
  NvmDimmDriverBlockIoReadBlocks,               //!< ReadBlocks
  NvmDimmDriverBlockIoWriteBlocks,              //!< WriteBlocks
  NvmDimmDriverBlockIoFlushBlocks               //!< FlushBlocks
};

/**
  Block I/O 2 Protocol Instance
**/
/*GLOBAL_REMOVE_IF_UNREFERENCED
EFI_BLOCK_IO2_PROTOCOL gNvmDimmDriverBlockIo2 =
{
  &gNvmDimmDriverBlockIoMedia,        //!< Media
  NvmDimmDriverBlockIoReset,          //!< Reset
  NvmDimmDriverBlockIoReadBlocksEx,   //!< ReadBlocks
  NvmDimmDriverBlockIoWriteBlocksEx,  //!< WriteBlocks
  NvmDimmDriverBlockIoFlushBlocksEx   //!< FlushBlocks
};*/

/**
  Read BufferSize bytes from Lba into Buffer.

  @param  pThis       Indicates a pointer to the calling context.
  @param  MediaId    ID of the media, changes every time the media is replaced.
  @param  Lba        The starting LBA to read from.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  pBuffer     A pointer to the destination buffer for the data. The caller is
   responsible for either having implicit or explicit ownership of the buffer.

  @retval EFI_SUCCESS           The data was read correctly from the device.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the read.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not match the current device.
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NAMESPACE *pNamespace = NULL;
  UINT32 Index = 0;
  UINT64 BlocksToRead = 0;
  CHAR8 *pByteBuffer = pBuffer;

  if (pThis == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pThis->Media == NULL) {
    ReturnCode = EFI_NO_MEDIA;
    goto Finish;
  }

  if (MediaId != pThis->Media->MediaId) {
    ReturnCode = EFI_MEDIA_CHANGED;
    goto Finish;
  }

  if (BufferSize % pThis->Media->BlockSize != 0) {
    ReturnCode = EFI_BAD_BUFFER_SIZE;
    goto Finish;
  }

  // Zero-length operations are always success
  if (BufferSize == 0) {
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  BlocksToRead = BufferSize / pThis->Media->BlockSize;

  if ((Lba + BlocksToRead - 1) > pThis->Media->LastBlock || pBuffer == NULL ||
      ((UINT64) pBuffer % pThis->Media->IoAlign != 0)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pNamespace = GET_NAMESPACE_INSTANCE(pThis);

  if (pNamespace == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < BlocksToRead; Index++) {
    ReturnCode = ReadBlockDevice(pNamespace, Lba + Index, pByteBuffer + (Index *pThis->Media->BlockSize));

    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

Finish:
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Error in the read.");
  }
  return ReturnCode;
}

/**
  Write BufferSize bytes from LBA into Buffer.

  @param  pThis       Indicates a pointer to the calling context.
  @param  MediaId    The media ID that the write request is for.
  @param  Lba        The starting logical block address to be written. The caller is
   responsible for writing to only legitimate locations.
  @param  BufferSize Size of Buffer, must be a multiple of device block size.
  @param  pBuffer     A pointer to the source buffer for the data.

  @retval EFI_SUCCESS           The data was written correctly to the device.
  @retval EFI_WRITE_PROTECTED   The device cannot be written to.
  @retval EFI_DEVICE_ERROR      The device reported an error while performing the write.
  @retval EFI_NO_MEDIA          There is no media in the device.
  @retval EFI_MEDIA_CHANGED     The MediaId does not match the current device.
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NAMESPACE *pNamespace = NULL;
  UINT32 Index = 0;
  UINT64 BlocksToWrite = 0;
  CHAR8 *pByteBuffer = pBuffer;

  if (pThis == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pThis->Media == NULL) {
    ReturnCode = EFI_NO_MEDIA;
    goto Finish;
  }

  if (MediaId != pThis->Media->MediaId) {
    ReturnCode = EFI_MEDIA_CHANGED;
    goto Finish;
  }

  if (BufferSize % pThis->Media->BlockSize != 0) {
    ReturnCode = EFI_BAD_BUFFER_SIZE;
    goto Finish;
  }

  // Zero-length operations are always success
  if (BufferSize == 0) {
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  BlocksToWrite = BufferSize / pThis->Media->BlockSize;

  if ((Lba + BlocksToWrite - 1) > pThis->Media->LastBlock ||
      pBuffer == NULL || ((UINT64) pBuffer % pThis->Media->IoAlign != 0)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pNamespace = GET_NAMESPACE_INSTANCE(pThis);

  if (pNamespace == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < BlocksToWrite; Index++) {
    ReturnCode = WriteBlockDevice(pNamespace, Lba + Index, pByteBuffer + (Index * pThis->Media->BlockSize));

    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

Finish:
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Error in the write.");
  }
  return ReturnCode;
}

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
)
{
  return EFI_SUCCESS;
}

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
)
{
  return EFI_SUCCESS;
}

