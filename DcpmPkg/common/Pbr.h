/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PBR_H_
#define _PBR_H_

#include <Types.h>
#include <PbrTypes.h>

 /**
    Gets data from the playback session

    @param[in] Signature: Specifies which data type to get
    @param[in] Index: GET_NEXT_DATA_INDEX gets the next data object within
       the playback session.  Otherwise, any positive value will result in
       getting the data object at position 'Index' (base 0).  If data associated
       with Signature is a Singleton, use Index '0'.
    @param[out] ppData: Newly allocated buffer that contains the data object.
       Caller is responsible for freeing it.
    @param[out] pSize: Size in bytes of ppData.
    @param[out] pLogicalIndex: May be NULL, otherwise will contain the
       logical index of the data object.
    @retval EFI_SUCCESS on success
  **/
EFI_STATUS
EFIAPI
PbrGetData(
  IN UINT32 Signature,
  IN INT32 Index,
  OUT VOID **ppData,
  OUT UINT32 *pSize,
  OUT UINT32 *pLogicalIndex
);

/**
   Adds data to the recording session

   @param[in] Signature: unique dword identifier that categorizes
      the data to be recorded
   @param[in] pData: Data to be recorded.  If NULL, a zeroed data buffer
      is allocated.  Usefull, when used with ppData.
   @param[in] Size: Byte size of pData
   @param[in] Singleton: Only one data object associated with Signature.
      Data previously set will be overriden with this data object.
   @param[out] - ppData - May be NULL, otherwise will contain a pointer
      to the memory allocated in the recording buffer for this data object.
      Warning, this pointer is only guaranteed to be valid until the next
      call to this function.
   @retval EFI_SUCCESS on success
 **/
EFI_STATUS
EFIAPI
PbrSetData(
  IN UINT32 Signature,
  IN VOID *pData,
  IN UINT32 Size,
  IN BOOLEAN Singleton,
  OUT VOID **ppData,
  OUT UINT32 *pLogicalIndex
);

/**
   Gets information pertaining to playback data associated with a specific
   data type (Signature).

   @param[in] Signature: Specifies data type interested in
   @param[out] pTotalDataItems: Number of logical data items of this Signature type that
      are available in the currently active playback session.
   @param[out] pTotalDataSize: Total size in bytes of all data associated with Signature type
   @param[out] pCurrentPlaybackDataOffset: Points to the next data item of Signature type
      that will be returned when PbrGetData is called with GET_NEXT_DATA_INDEX.
   @retval EFI_SUCCESS on success
 **/
EFI_STATUS
EFIAPI
PbrGetDataPlaybackInfo(
  IN UINT32 Signature,
  OUT UINT32 *pTotalDataItems,
  OUT UINT32 *pTotalDataSize,
  OUT UINT32 *pCurrentPlaybackDataOffset
);

/**
  Sets the playback/recording mode

  @param[in] PbrMode: 0x0 - Normal, 0x1 - Recording, 0x2 - Playback

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrSetMode(
  IN     UINT32 PbrMode
  );

/**
  Gets the current playback/recording mode

  @param[out] pPbrMode: 0x0 - Normal, 0x1 - Recording, 0x2 - Playback

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrGetMode(
  OUT     UINT32 *pPbrMode
);

/**
  Set the PBR session buffer to use

  @param[in] pBufferAddress: address of a buffer to use for playback or record mode
  @param[in] BufferSize: size in bytes of the buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrSetSession(
  IN     VOID *pBufferAddress,
  IN     UINT32 BufferSize
);

/**
  Get the PBR Buffer that is current being used

  @param[out] ppBufferAddress: address to the pbr buffer
  @param[out] pBufferSize: size in bytes of the buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrGetSession(
  IN     VOID **ppBufferAddress,
  IN     UINT32 *pBufferSize
);

/**
  Clear the dynamic portions of the PBR Buffer that is current being used

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrFreeSession(
);

/**
  Reset all playback buffers to align with the specified TagId

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrResetSession(
  IN     UINT32 TagId
);

/**
  Set a tag associated with the current recording buffer offset

  @param[in] Signature: Signature associated with the tag
  @param[in] pName: name associated with the tag
  @param[in] pDescription: description of the tab
  @param[out] pId: logical ID given to the tag (will be appended to the name)

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrSetTag(
  IN     UINT32 Signature,
  IN     CHAR16 *pName,
  IN     CHAR16 *pDescription,
  OUT    UINT32 *pId
);

/**
  Get the number of tags associated with the recording buffer

  @param[out] pCount: get the number of tags

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrGetTagCount(
  OUT    UINT32 *pCount
);

/**
  Get tag info

  @param[in] pId: tag identification
  @param[out] pSignature: signature associated with the tag
  @param[out] pName: name associated with the tag
  @param[out] ppDescription: description of the tab
  @param[out] ppTagPartitionInfo: array of TagPartitionInfo structs
  @param[out] pTagPartitionCnt: number of items in pTagPartitionInfo

  All out pointers need to be freed by caller.

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrGetTag(
  IN     UINT32 Id,
  OUT    UINT32 *pSignature,
  OUT    CHAR16 **ppName,
  OUT    CHAR16 **ppDescription,
  OUT    VOID **ppTagPartitionInfo,
  OUT    UINT32 *pTagPartitionCnt
);


/**
  Initialize data structures associated with PBR

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrInit(
);

/**
  Uninitialize data structures associated with PBR

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrUninit(
);




#endif //_PBR_H_