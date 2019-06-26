/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Debug.h>
#include <Types.h>
#include <Convert.h>
#include "Pbr.h"
#include "PbrDcpmm.h"
#ifdef OS_BUILD
#include "PbrOs.h"
#else
STATIC EFI_STATUS PbrSerializeCtx(PbrContext *ctx, BOOLEAN Force);
STATIC EFI_STATUS PbrDeserializeCtx(PbrContext * ctx);
#endif
//local helper function prototypes
STATIC EFI_STATUS PbrCheckBufferIntegrity(PbrContext *ctx);
STATIC EFI_STATUS PbrComposeSession(PbrContext *pContext, VOID **ppBufferAddress, UINT32 *pBufferSize);
STATIC EFI_STATUS PbrDecomposeSession(PbrContext *pContext, VOID *pPbrImg, UINT32 PbrImgSize);
STATIC EFI_STATUS PbrCreateSessionContext(PbrContext * ctx);
STATIC UINT32 PbrPartitionCount();
STATIC EFI_STATUS PbrGetPartition(UINT32 Signature, PbrPartitionContext **ppPartition);
STATIC EFI_STATUS PbrCopyChunks(VOID *pDest, UINT32 pDestSz, VOID *pSource, UINT32 pSourceSz);

PbrContext gPbrContext;
//used for setting volatile/non-volatile uefi variables
extern EFI_GUID gIntelDimmPbrVariableGuid;
extern EFI_GUID gIntelDimmPbrTagIdVariableguid;

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
)
{
  UINT32 CtxIndex = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();
  PbrPartitionLogicalDataItem *pDataItem = NULL;

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    ReturnCode = EFI_NOT_READY;
    goto Finish;
  }

  //find the partition associated input param Signature
  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    //partition signature check
    if (Signature == pContext->PartitionContexts[CtxIndex].PartitionSig) {
      //caller wants the data object to be a singleton (only one logical data associated with this specific partition)
      if (Singleton) {
        //is the size previously allocated for this partition big enough?
        if (Size > pContext->PartitionContexts[CtxIndex].PartitionSize) {
          //no it isn't, let's free anything previously allocated
          if (pContext->PartitionContexts[CtxIndex].PartitionData) {
            FreePool(pContext->PartitionContexts[CtxIndex].PartitionData);
          }
          //allocate just enough to add our new singleton data object
          pDataItem = AllocateZeroPool(Size+sizeof(PbrPartitionLogicalDataItem));
          if (NULL == pDataItem) {
            ReturnCode = EFI_OUT_OF_RESOURCES;
            NVDIMM_DBG("Failed to allocate memory for partition buffer\n");
            goto Finish;
          }
          pContext->PartitionContexts[CtxIndex].PartitionData = pDataItem;
          //update our internal context with the new partition size
          pContext->PartitionContexts[CtxIndex].PartitionSize = Size + sizeof(PbrPartitionLogicalDataItem);
          //now that we have memory allocated, let's copy caller data into it
          //note, caller has option to not provide data.
          if (pData) {
            PbrCopyChunks(pDataItem->Data,
              Size,
              pData,
              Size);
          }
          pContext->PartitionContexts[CtxIndex].PartitionLogicalDataCnt = 1;
          pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset = Size + sizeof(PbrPartitionLogicalDataItem);
          pContext->PartitionContexts[CtxIndex].PartitionEndOffset = 0; //not used yet
          //individual data objects within a partition are signed generically as PBR_LOGICAL_DATA_SIG
          //only the partition itself contains the specific signature associated with the data (each data signature has a partition associated with it)
          pDataItem->Signature = PBR_LOGICAL_DATA_SIG;
          pDataItem->Size = Size;
          goto Finish;
        }
        else {
          pDataItem = (PbrPartitionLogicalDataItem*)(pContext->PartitionContexts[CtxIndex].PartitionData);
          pDataItem->Signature = PBR_LOGICAL_DATA_SIG;
          pDataItem->Size = Size;
          if (pData) {
            PbrCopyChunks(pDataItem->Data,
              pContext->PartitionContexts[CtxIndex].PartitionSize,
              pData,
              Size);
          }
          goto Finish;
        }
      }
      else {
        //allocate more memory if needed
        if (pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset + (Size + sizeof(PbrPartitionLogicalDataItem)) > pContext->PartitionContexts[CtxIndex].PartitionSize) {
          pContext->PartitionContexts[CtxIndex].PartitionData = ReallocatePool(pContext->PartitionContexts[CtxIndex].PartitionSize,
            pContext->PartitionContexts[CtxIndex].PartitionSize + ((Size + sizeof(PbrPartitionLogicalDataItem)) * PARTITION_GROW_SZ_MULTIPLIER),
            pContext->PartitionContexts[CtxIndex].PartitionData);

          if (NULL == pContext->PartitionContexts[CtxIndex].PartitionData) {
            ReturnCode = EFI_OUT_OF_RESOURCES;
            NVDIMM_DBG("Failed to allocate memory for partition buffer\n");
            goto Finish;
          }
          pContext->PartitionContexts[CtxIndex].PartitionSize += ((Size + sizeof(PbrPartitionLogicalDataItem)) * PARTITION_GROW_SZ_MULTIPLIER);
        }
        pDataItem = (PbrPartitionLogicalDataItem*)((UINTN)pContext->PartitionContexts[CtxIndex].PartitionData + (UINTN)pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset);
        pDataItem->Signature = PBR_LOGICAL_DATA_SIG;
        pDataItem->Size = Size;
        //now that we have memory allocated, let's copy caller data into it
        //note, caller has option to not provide data.
        if (pData) {
          PbrCopyChunks(pDataItem->Data,
            pContext->PartitionContexts[CtxIndex].PartitionSize - pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset,
            pData,
            Size);
        }
        //keep track of how many data objects copied to each partition
        pContext->PartitionContexts[CtxIndex].PartitionLogicalDataCnt++;
        //next position to copy data to

        pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset += (Size + sizeof(PbrPartitionLogicalDataItem));
      }
      goto Finish;
    }
    //if we haven't found a previously allocated partition associated with Signature then create one
    else if (PBR_INVALID_SIG == pContext->PartitionContexts[CtxIndex].PartitionSig) {
      break;
    }
  }

  //Need to create a new partition
  if (CtxIndex == MAX_PARTITIONS) {
    NVDIMM_ERR("Ran out of PBR partition space.  Increase MAX_PARTITIONS\n");
    return EFI_OUT_OF_RESOURCES;
  }
  pContext->PartitionContexts[CtxIndex].PartitionSig = Signature;
  pContext->PartitionContexts[CtxIndex].PartitionSize = Singleton ? (Size + sizeof(PbrPartitionLogicalDataItem)) : (Size + sizeof(PbrPartitionLogicalDataItem))*PARTITION_GROW_SZ_MULTIPLIER;
  pContext->PartitionContexts[CtxIndex].PartitionLogicalDataCnt = 1;
  pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset = 0;
  pContext->PartitionContexts[CtxIndex].PartitionEndOffset = 0;
  pContext->PartitionContexts[CtxIndex].PartitionData = pDataItem = AllocateZeroPool(pContext->PartitionContexts[CtxIndex].PartitionSize);

  if (NULL == pContext->PartitionContexts[CtxIndex].PartitionData) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    NVDIMM_DBG("Failed to allocate memory for partition buffer\n");
    goto Finish;
  }

  pDataItem->Signature = PBR_LOGICAL_DATA_SIG;
  pDataItem->Size = Size;

  //now that we have a new partition and memory allocated, let's copy caller data into it
  //note, caller has option to not provide data.
  if (pData) {
    PbrCopyChunks(pDataItem->Data,
      pContext->PartitionContexts[CtxIndex].PartitionSize - sizeof(PbrPartitionLogicalDataItem),
      pData,
      Size);
  }
  //advance next recording offset pointer
  pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset += (Size + sizeof(PbrPartitionLogicalDataItem));
Finish:
  //if no errors, and user wants a pointer to the allocated recording location
  if (EFI_SUCCESS == ReturnCode && ppData) {
    *ppData = (VOID*)(&pDataItem->Data[0]);
  }
  //every data object recorded is wrapped in a logical data structure, which retains the data object index
  //within the partition
  if (EFI_SUCCESS == ReturnCode) {
    pDataItem->LogicalIndex = pContext->PartitionContexts[CtxIndex].PartitionLogicalDataCnt - 1;
  }
  //caller wants to know the data object index of this particular data object.
  if (EFI_SUCCESS == ReturnCode && pLogicalIndex) {
    *pLogicalIndex = pDataItem->LogicalIndex;
  }
  return ReturnCode;
}

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
)
{
  UINT32 CtxIndex = 0;
  UINT32 PartitionCtxIndex = 0;
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  PbrContext *pContext = PBR_CTX();
  PbrPartitionLogicalDataItem *pDataItem = NULL;

  //find the partition associated input param Signature
  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    //if signature matches
    if (Signature == pContext->PartitionContexts[CtxIndex].PartitionSig) {
      //caller wants the next data object within the playback session
      if(GET_NEXT_DATA_INDEX == Index){
        //get the next logical data item
        pDataItem = (PbrPartitionLogicalDataItem *)((UINTN)pContext->PartitionContexts[CtxIndex].PartitionData + (UINTN)pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset);
        //verify the data item is valid, if not return EFI_NOT_FOUND
        if (PBR_LOGICAL_DATA_SIG != pDataItem->Signature) {
          goto Finish;
        }
        //found it, now advance the current pbr offset so the next time this is called the next logical data item is returned
        pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset += (sizeof(PbrPartitionLogicalDataItem) + pDataItem->Size);
        ReturnCode = EFI_SUCCESS;
      }
      else {
        //caller wants a specific indexed data item 
        pDataItem = (PbrPartitionLogicalDataItem *)(pContext->PartitionContexts[CtxIndex].PartitionData);
        //iterate through all items
        while (PBR_LOGICAL_DATA_SIG == pDataItem->Signature) {
          if ((UINT32)Index == PartitionCtxIndex) {
            ReturnCode = EFI_SUCCESS;
            break;
          }
          pDataItem = (PbrPartitionLogicalDataItem *)((UINTN)pDataItem + (UINTN)pDataItem->Size + sizeof(PbrPartitionLogicalDataItem));
          ++PartitionCtxIndex;
        }
      }
      //if indexed item was located, allocate memory and copy the data to the caller
      if (EFI_SUCCESS == ReturnCode) {
        *ppData = AllocateZeroPool(pDataItem->Size);
        if (NULL == *ppData) {
          ReturnCode = EFI_OUT_OF_RESOURCES;
          NVDIMM_DBG("Failed to allocate memory for partition buffer\n");
          goto Finish;
        }
        *pSize = pDataItem->Size;
        PbrCopyChunks(*ppData, *pSize, pDataItem->Data, pDataItem->Size);
        ReturnCode = EFI_SUCCESS;
        goto Finish;
      }
    }
  }

Finish:
  //if caller has requested the data item index
  if (EFI_SUCCESS == ReturnCode && pLogicalIndex) {
    *pLogicalIndex = pDataItem->LogicalIndex;
  }
  return ReturnCode;
}

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
)
{
  UINT32 CtxIndex = 0;
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  PbrContext *pContext = PBR_CTX();
  PbrPartitionContext *pPartition = NULL;

  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    if (Signature == pContext->PartitionContexts[CtxIndex].PartitionSig) {
      pPartition = &pContext->PartitionContexts[CtxIndex];
      *pTotalDataItems = pPartition->PartitionLogicalDataCnt;
      *pTotalDataSize = pPartition->PartitionSize;
      *pCurrentPlaybackDataOffset = pPartition->PartitionCurrentOffset;
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }
  return ReturnCode;
}

 /**
   Sets the playback/recording mode

   @param[in] PbrMode: 0x0 - Normal, 0x1 - Recording, 0x2 - Playback

   @retval EFI_SUCCESS if the table was found and is properly returned.
   @retval EFI_NOT_READY if PbrMode is set to Playback and session isn't loaded
 **/
EFI_STATUS
EFIAPI
PbrSetMode(
  IN     UINT32 PbrMode
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    ReturnCode = EFI_NOT_READY;
    goto Finish;
  }

  //if playback, ensure a session has been loaded
  if (PBR_PLAYBACK_MODE == PbrMode) {
    if (NULL == pContext->PbrMainHeader) {
      return EFI_NOT_READY;
    }
  }
  //if normal, free buffers
  else if (PBR_NORMAL_MODE == PbrMode) {
    PbrFreeSession();
    ZeroMem(pContext, sizeof(PbrContext));
  }
  //set the desired mode
  PBR_SET_MODE(pContext, PbrMode);

  //mode has changed, ensure the context gets serialized
#ifndef OS_BUILD
  ReturnCode = PbrSerializeCtx(pContext, TRUE);
  if (!EFI_ERROR(ReturnCode)) {
    PBR_SET_MODE(pContext, PbrMode);
  }
  else {
    NVDIMM_DBG("Failed to set PBR mode variable!\n");
  }
#endif
Finish:
  return ReturnCode;
}

/**
  Gets the current playback/recording mode

  @param[out] pPbrMode: 0x0 - Normal, 0x1 - Recording, 0x2 - Playback

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_NOT_READY if the pbr context is not available
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
EFIAPI
PbrGetMode(
  OUT     UINT32 *pPbrMode
)
{
  PbrContext *pContext = PBR_CTX();
  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }
  if (NULL == pPbrMode) {
    return EFI_INVALID_PARAMETER;
  }
  *pPbrMode = PBR_GET_MODE(pContext);
  return EFI_SUCCESS;
}

/**
  Set the PBR session buffer to use

  @param[in] pBufferAddress: address of a buffer to use for playback or record mode. If NULL new buffers
    will be created.
  @param[in] BufferSize: size in bytes of the buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_NOT_READY if the pbr context is not available
**/
EFI_STATUS
EFIAPI
PbrSetSession(
  IN     VOID *pBufferAddress,
  IN     UINT32 BufferSize
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }

  NVDIMM_DBG("PbrSetSession: Addr: 0x%x, Size: %d\n", (UINTN)pBufferAddress, BufferSize);

  //frees any existing session buffers
  ReturnCode = PbrFreeSession();
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to free session!");
    goto Finish;
  }

  //caller wants to create a new session
  if (NULL == pBufferAddress) {
    ReturnCode = PbrCreateSessionContext(pContext);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to create a new buffer!");
      goto Finish;
    }
  }
  else {
    //unravels PBR image and updates the context
    ReturnCode = PbrDecomposeSession(pContext, pBufferAddress, BufferSize);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to unstich img!");
      goto Finish;
    }
  }

  NVDIMM_DBG("About to do integrity check...\n");
  ReturnCode = PbrCheckBufferIntegrity(pContext);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Invalid PBR Buffer!");
    goto Finish;
  }
  NVDIMM_DBG("Done with integrity check\n");
  //created a new session, make sure to serialize the context
  ReturnCode = PbrSerializeCtx(pContext, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to set PBR Context variable\n");
    goto Finish;
  }

Finish:
  return ReturnCode;
}

/**
  Get the PBR Buffer that is current being used

  @param[out] ppBufferAddress: address to the pbr buffer
  @param[out] pBufferSize: size in bytes of the buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrGetSession(
  IN     VOID **ppBufferAddress,
  IN     UINT32 *pBufferSize
)
{
  PbrContext *pContext = PBR_CTX();
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    ReturnCode = EFI_NOT_READY;
    goto Finish;
  }

  if (NULL == ppBufferAddress || NULL == pBufferSize) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  //the session at this point is just a bunch of buffers,
  //PbrComposeSession stitches all of the buffers into a contiguous format...
  //something that can be loaded and decomposed in the future
  ReturnCode = PbrComposeSession(pContext, ppBufferAddress, pBufferSize);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to stitch image\n");
    goto Finish;
  }

Finish:
  return ReturnCode;
}


/**
  Free the PBR buffers that are currently being used

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrFreeSession(
)
{
  UINT32 CtxIndex = 0;
  PbrContext *pContext = PBR_CTX();
  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }

  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    if (PBR_INVALID_SIG != pContext->PartitionContexts[CtxIndex].PartitionSig) {
      if (pContext->PartitionContexts[CtxIndex].PartitionData) {
        FreePool(pContext->PartitionContexts[CtxIndex].PartitionData);
      }
      pContext->PartitionContexts[CtxIndex].PartitionSig = PBR_INVALID_SIG;
    }
  }

  FREE_POOL_SAFE(pContext->PbrMainHeader);
  return EFI_SUCCESS;
}

/**
  Reset all playback buffers to align with the specified TagId

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrResetSession(
  IN     UINT32 TagId
)
{
  Tag *pTag = NULL;
  UINT32 DataSize = 0;
  PbrContext *pContext = PBR_CTX();
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  TagPartitionInfo *pTagPartitions = NULL;
  UINT32 CtxIndex = 0;
  UINT32 TagPartIndex = 0;

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    ReturnCode = EFI_NOT_READY;
    goto Finish;
  }

  //get the actual tag data item
  //this will contain offsets for all data partitions that existed when the tag was set/created
  ReturnCode = PbrGetData(
    PBR_TAG_SIG,
    TagId,
    (VOID**)&pTag,
    &DataSize,
    NULL);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("No TAG ID!\n");
    goto Finish;
  }

  //immediately following the tag struct is a series of TagPartitionInfo objects
  //where each object describes one data partition
  pTagPartitions = (TagPartitionInfo*)((UINTN)pTag + sizeof(Tag));

  //need to reset each data partition to the offset specified in the tag
  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    //found a partition
    if (PBR_INVALID_SIG != pContext->PartitionContexts[CtxIndex].PartitionSig) {
      //default is to reset the current offset to the begining
      pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset = 0;
      //find the corresponding TagPartitionInfo associated with the partition signature
      for (TagPartIndex = 0; TagPartIndex < pTag->PartitionInfoCnt; ++TagPartIndex) {
        if (pTagPartitions[TagPartIndex].PartitionSignature == pContext->PartitionContexts[CtxIndex].PartitionSig) {
          pContext->PartitionContexts[CtxIndex].PartitionCurrentOffset = pTagPartitions[TagPartIndex].PartitionCurrentOffset;
          break;
        }
      }
    }
  }
Finish:
  FREE_POOL_SAFE(pTag);
  return ReturnCode;
}

/**
  Set a tag associated with the current recording buffer offset

  @param[in] Signature: Signature associated with the tag
  @param[in] pName: name associated with the tag
  @param[in] pDescription: description of the tab
  @param[out] pId: logical ID given to the tag (will be appended to the name)

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrSetTag(
  IN     UINT32 Signature,
  IN     CHAR16 *pName,
  IN     CHAR16 *pDescription,
  OUT    UINT32 *pId
) {
  UINT32 NewTagSize = 0;
  UINT32 DescriptionSize = 0;
  UINT32 NameSize = 0;
  CHAR8 *TagStrings = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();
  Tag *pTagData = NULL;
  UINT32 LogicalIndex = 0;
  UINT32 PartitionCount = 0;
  UINT32 Index = 0;
  TagPartitionInfo *pTagPartitionInfo = NULL;

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }

  if (PBR_RECORD_MODE != pContext->PbrMode) {
    return ReturnCode;
  }

  if (NULL == pName || NULL == pDescription) {
    NVDIMM_DBG("Invalid input params\n");
    return EFI_INVALID_PARAMETER;
  }

  //predetermine the full tag size
  DescriptionSize = (UINT32)(StrLen(pDescription) + 1) * sizeof(CHAR8);
  NameSize = (UINT32)(StrLen(pName) + 1) * sizeof(CHAR8);
  NewTagSize = sizeof(Tag);
  NewTagSize += (DescriptionSize + NameSize);
  //each partition gets a TagPartitionInfo
  PartitionCount = PbrPartitionCount();
  NewTagSize += (PartitionCount * sizeof(TagPartitionInfo));

  //reserve a chunk of buffer within the TAG partition
  //to be filled in later in this function
  ReturnCode = PbrSetData(
    PBR_TAG_SIG,
    NULL,
    NewTagSize,
    FALSE,
    (VOID**)&pTagData,
    &LogicalIndex);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to set tag data\n");
    goto Finish;
  }

  //basic tag info
  pTagData->Signature = PBR_TAG_SIG;
  pTagData->TagSignature = Signature;
  pTagData->TagId = LogicalIndex;
  pTagData->TagSize = NewTagSize;
  pTagData->PartitionInfoCnt = PartitionCount;
  //after the tag structure will be an array of TagPartitionInfo structs
  //one per partition
  pTagPartitionInfo = (TagPartitionInfo*)((UINTN)pTagData + sizeof(Tag));
  //fill in the TagPartitionInfo structs
  for (Index = 0; Index < MAX_PARTITIONS; ++Index) {
    if (PBR_INVALID_SIG != pContext->PartitionContexts[Index].PartitionSig) {
      pTagPartitionInfo->PartitionSignature = pContext->PartitionContexts[Index].PartitionSig;
      pTagPartitionInfo->PartitionCurrentOffset = pContext->PartitionContexts[Index].PartitionCurrentOffset;
      ++pTagPartitionInfo;
    }
  }
  //lastly, are the tag description/name strings
  TagStrings = (CHAR8*)((UINTN)pTagData + sizeof(Tag) + (PartitionCount * sizeof(TagPartitionInfo)));
  UnicodeStrToAsciiStrS(pName, TagStrings, NameSize);
  TagStrings = (CHAR8*)((UINTN)pTagData + sizeof(Tag) + (PartitionCount * sizeof(TagPartitionInfo)) + NameSize);
  UnicodeStrToAsciiStrS(pDescription, TagStrings, DescriptionSize);
  //if caller wants to know the logical index of this particular tag within the tag partition
  if (NULL != pId) {
    *pId = LogicalIndex;
  }

Finish:
  return ReturnCode;
}

/**
  Get the number of tags associated with the recording buffer

  @param[out] pCount: get the number of tags

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
EFIAPI
PbrGetTagCount(
  OUT    UINT32 *pCount
) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrPartitionContext *pPartition = NULL;
  PbrContext *pContext = PBR_CTX();

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }

  if (NULL == pCount) {
    return EFI_INVALID_PARAMETER;
  }

  PbrGetPartition(PBR_TAG_SIG, &pPartition);
  if (pPartition) {
    *pCount = pPartition->PartitionLogicalDataCnt;
  }
  else {
    *pCount = 0;
  }
  NVDIMM_DBG("Tag count: %d\n", *pCount);
  return ReturnCode;
}

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
) {
  Tag *pTag = NULL;
  CHAR8 *pTagStrs = NULL;
  UINT32 pTagStrsSize = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();
  UINT32 DataSize = 0;

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }

  if (NULL == ppName || NULL == ppDescription) {
    return EFI_INVALID_PARAMETER;
  }

  ReturnCode = PbrGetData(
    PBR_TAG_SIG,
    Id,
    (VOID**)&pTag,
    &DataSize,
    NULL);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("No TAG data!\n");
    goto Finish;
  }

  *pSignature = pTag->TagSignature;

  pTagStrs = (CHAR8*)((UINTN)pTag + sizeof(Tag) + (pTag->PartitionInfoCnt * sizeof(TagPartitionInfo)));
  pTagStrsSize = (UINT32)((AsciiStrLen(pTagStrs) + 1) * sizeof(CHAR16));
  *ppName = AllocateZeroPool(pTagStrsSize);
  SafeAsciiStrToUnicodeStr(pTagStrs, (UINT32)(AsciiStrLen(pTagStrs) + 1), *ppName);

  pTagStrs = (CHAR8*)((UINTN)pTagStrs + (UINTN)(AsciiStrLen(pTagStrs) + 1));
  pTagStrsSize = (UINT32)((AsciiStrLen(pTagStrs) + 1) * sizeof(CHAR16));
  *ppDescription = AllocateZeroPool(pTagStrsSize);
  SafeAsciiStrToUnicodeStr(pTagStrs, (UINT32)(AsciiStrLen(pTagStrs) + 1), *ppDescription);

  if (pTagPartitionCnt) {
    *pTagPartitionCnt = pTag->PartitionInfoCnt;
  }

  if (ppTagPartitionInfo && pTag->PartitionInfoCnt > 0) {
    *ppTagPartitionInfo = AllocateZeroPool(pTag->PartitionInfoCnt * sizeof(TagPartitionInfo));
    if (NULL == *ppTagPartitionInfo) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
    }
    PbrCopyChunks(*ppTagPartitionInfo,
      pTag->PartitionInfoCnt * sizeof(TagPartitionInfo),
      (VOID*)((UINTN)pTag + sizeof(Tag)),
      pTag->PartitionInfoCnt * sizeof(TagPartitionInfo));
  }

Finish:
  FREE_POOL_SAFE(pTag);
  return ReturnCode;
}


/**
  Initialize data structures associated with PBR

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
PbrInit(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();

  if (NULL == pContext) {
    NVDIMM_DBG("Failed to allocate memory for the PBR context\n");
    return EFI_OUT_OF_RESOURCES;
  }

  //initialize the context's mode property
  ReturnCode = PbrDeserializeCtx(pContext);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve PBR_MODE config value");
    goto Finish;
  }

  NVDIMM_DBG("PbrInit PBR MODE: %d\n", pContext->PbrMode);
  NVDIMM_DBG("PbrInit DONE\n");
Finish:
  return ReturnCode;
}


/**
  Uninitialize data structures associated with PBR

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
PbrUninit(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrContext *pContext = PBR_CTX();

  ReturnCode = PbrSerializeCtx(pContext, FALSE);
  //todo for OS free buffers in context
#ifdef OS_BUILD
  PbrFreeSession(pContext);
  ZeroMem(pContext, sizeof(PbrContext));
#endif
  return ReturnCode;
}

#ifndef OS_BUILD //Implementation for OS in PbrOs.c
/**
  Helper that restores the context.  At this point the context is only saved to a volatile store.
**/
EFI_STATUS
PbrSerializeCtx(
  PbrContext *ctx,
  BOOLEAN Force)
{
  UINTN VariableSize;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (NULL == ctx) {
    NVDIMM_DBG("ctx is null\n");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  VariableSize = sizeof(PbrContext);
  ReturnCode = SET_VARIABLE(
    PBR_CONTEXT_VAR,
    gIntelDimmPbrVariableGuid,
    VariableSize,
    (VOID*)ctx);

Finish:
  return ReturnCode;
}

/**
  Helper that saves the context.  At this point the context is only saved to a volatile store.
**/
EFI_STATUS
PbrDeserializeCtx(
  PbrContext *ctx) {
  PbrContext NewCtx;
  UINTN VariableSize;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (NULL == ctx) {
    NVDIMM_DBG("ctx is null\n");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  VariableSize = sizeof(PbrContext);
  ReturnCode = GET_VARIABLE(
    PBR_CONTEXT_VAR,
    gIntelDimmPbrVariableGuid,
    &VariableSize,
    &NewCtx);

  if (ReturnCode == EFI_NOT_FOUND) {
    NVDIMM_DBG("PBR_CTX param not found, setting to default value\n");
    ctx->PbrMode = PBR_NORMAL_MODE;
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve PBR_MODE config value");
    goto Finish;
  }

  PbrCopyChunks((VOID*)ctx, sizeof(PbrContext), &NewCtx, sizeof(PbrContext));
Finish:
  return ReturnCode;
}
#endif
/**
  Helper that decomposes/unstitches a PBR session
**/
STATIC
EFI_STATUS
PbrDecomposeSession(
  IN     PbrContext *pContext,
  IN     VOID *pPbrImg,
  IN     UINT32 PbrImgSize
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrPartitionTable *pPartitionTable = NULL;
  PbrHeader *pPbrHeader = NULL;
  UINT32 PartitionIndex = 0;


  ZeroMem(pContext->PartitionContexts, sizeof(pContext->PartitionContexts));

  //update context's file header
  pContext->PbrMainHeader = (PbrHeader*)AllocateZeroPool(sizeof(PbrHeader));
  if (NULL == pContext->PbrMainHeader) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  PbrCopyChunks(pContext->PbrMainHeader, sizeof(PbrHeader), pPbrImg, sizeof(PbrHeader));

  if (((PbrHeader*)pContext->PbrMainHeader)->Signature != PBR_HEADER_SIG) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("Invalid buffer contents, PBR master header not found!\n");
    goto Finish;
  }
  pPbrHeader = (PbrHeader*)pContext->PbrMainHeader;
  pPartitionTable = (PbrPartitionTable *)&(pPbrHeader->PartitionTable);

  for (PartitionIndex = 0; PartitionIndex < MAX_PARTITIONS; ++PartitionIndex) {
    if (PBR_INVALID_SIG != pPartitionTable->Partitions[PartitionIndex].Signature) {
      pContext->PartitionContexts[PartitionIndex].PartitionSig = pPartitionTable->Partitions[PartitionIndex].Signature;
      pContext->PartitionContexts[PartitionIndex].PartitionSize = pPartitionTable->Partitions[PartitionIndex].Size;
      pContext->PartitionContexts[PartitionIndex].PartitionLogicalDataCnt = pPartitionTable->Partitions[PartitionIndex].LogicalDataCnt;
      pContext->PartitionContexts[PartitionIndex].PartitionCurrentOffset = 0;
      pContext->PartitionContexts[PartitionIndex].PartitionEndOffset = 0;
      pContext->PartitionContexts[PartitionIndex].PartitionData = AllocateZeroPool(pPartitionTable->Partitions[PartitionIndex].Size);
      if (NULL == pContext->PartitionContexts[PartitionIndex].PartitionData) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        NVDIMM_DBG("Failed to allocate memory for partition buffer\n");
        goto Finish;
      }
      PbrCopyChunks(pContext->PartitionContexts[PartitionIndex].PartitionData,
        pPartitionTable->Partitions[PartitionIndex].Size,
        (VOID*)((UINTN)pPbrImg + pPartitionTable->Partitions[PartitionIndex].Offset),
        pPartitionTable->Partitions[PartitionIndex].Size);
    }
  }

Finish:
  return ReturnCode;
}

/**
  Helper that stitches together all buffers to make a full PBR image
**/
STATIC
EFI_STATUS
PbrComposeSession(
  IN     PbrContext *pContext,
  OUT    VOID **ppBufferAddress,
  OUT    UINT32 *pBufferSize
)
{
  PbrHeader *pPbrMainHeader = NULL;
  UINT32 BufferSize = 0;
  UINT8 *pTemp = NULL;
  UINT32 CtxIndex = 0;

  if (NULL == pContext) {
    NVDIMM_DBG("No PBR context\n");
    return EFI_NOT_READY;
  }

  if (NULL == ppBufferAddress || NULL == pBufferSize) {
    return EFI_INVALID_PARAMETER;
  }

  pPbrMainHeader = (PbrHeader *)pContext->PbrMainHeader;
  ZeroMem(&pPbrMainHeader->PartitionTable, sizeof(PbrPartitionTable));
  BufferSize = sizeof(PbrHeader);

  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    if (PBR_INVALID_SIG != pContext->PartitionContexts[CtxIndex].PartitionSig) {
      pPbrMainHeader->PartitionTable.Partitions[CtxIndex].Signature = pContext->PartitionContexts[CtxIndex].PartitionSig;
      pPbrMainHeader->PartitionTable.Partitions[CtxIndex].Size = pContext->PartitionContexts[CtxIndex].PartitionSize;
      pPbrMainHeader->PartitionTable.Partitions[CtxIndex].LogicalDataCnt = pContext->PartitionContexts[CtxIndex].PartitionLogicalDataCnt;
      pPbrMainHeader->PartitionTable.Partitions[CtxIndex].Offset = BufferSize;
      BufferSize += pContext->PartitionContexts[CtxIndex].PartitionSize;
    }
  }

  *ppBufferAddress = AllocateZeroPool(BufferSize);
  NVDIMM_DBG("StitchImg: buffersize = %d bytes\n", BufferSize);
  if (NULL != *ppBufferAddress) {
    pPbrMainHeader = (PbrHeader*)*ppBufferAddress;
    //copy the main pbr header to the buffer
    PbrCopyChunks(*ppBufferAddress, BufferSize, pContext->PbrMainHeader, sizeof(PbrHeader));
    //advance past the main header, this will be copied at the end
    pTemp = (VOID*)((UINTN)(*ppBufferAddress) + (UINTN)sizeof(PbrHeader));
    NVDIMM_DBG("Copying main header: %d bytes\n", sizeof(PbrHeader));

    for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
      if (PBR_INVALID_SIG != pContext->PartitionContexts[CtxIndex].PartitionSig) {
        PbrCopyChunks(pTemp, BufferSize, pContext->PartitionContexts[CtxIndex].PartitionData, pContext->PartitionContexts[CtxIndex].PartitionSize);
        pTemp += pContext->PartitionContexts[CtxIndex].PartitionSize;
      }
    }
    *pBufferSize = BufferSize;
  }
  else {
    return EFI_OUT_OF_RESOURCES;
  }
  return EFI_SUCCESS;
}

/**
  Helper that allocates new pbr buffers
**/
STATIC
EFI_STATUS
PbrCreateSessionContext(
  PbrContext * ctx
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrHeader *header = NULL;

  //allocate memory for the main pbr header
  if (ctx->PbrMainHeader) {
    FreePool(ctx->PbrMainHeader);
  }
  header = ctx->PbrMainHeader = (PbrHeader*)AllocateZeroPool(sizeof(PbrHeader));
  if (NULL == ctx->PbrMainHeader) {
    NVDIMM_DBG("Failed to create PBR header");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  header->Signature = PBR_HEADER_SIG;

Finish:
  return ReturnCode;
}

/**
  Helper that verifies the contents of a pbr session.
**/
STATIC
EFI_STATUS
PbrCheckBufferIntegrity(
  PbrContext *ctx
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrHeader *pMasterPbrHeader = (PbrHeader *)ctx->PbrMainHeader;

  if (NULL == pMasterPbrHeader) {
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  if (PBR_HEADER_SIG != pMasterPbrHeader->Signature) {
    ReturnCode = EFI_COMPROMISED_DATA;
    NVDIMM_ERR("Pbr integrity check failed: Master header signature invalid\n");
    goto Finish;
  }

Finish:
  return ReturnCode;
}

/**
  Helper that provdies the number of active data partitions
**/
STATIC
UINT32 
PbrPartitionCount(
)
{
  UINT32 Index = 0;
  PbrContext *pContext = PBR_CTX();
  UINT32 PartitionCount = 0;

  for (Index = 0; Index < MAX_PARTITIONS; ++Index) {
    if (PBR_INVALID_SIG != pContext->PartitionContexts[Index].PartitionSig) {
      ++PartitionCount;
    }
  }

  return PartitionCount;
}

/**
  Helper that finds a specific partition and provides a reference to it
**/
STATIC
EFI_STATUS
PbrGetPartition(
  IN UINT32 Signature,
  OUT PbrPartitionContext **ppPartition
)
{
  UINT32 CtxIndex = 0;
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  PbrContext *pContext = PBR_CTX();

  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    if (Signature == pContext->PartitionContexts[CtxIndex].PartitionSig) {
      *ppPartition = &pContext->PartitionContexts[CtxIndex];
      ReturnCode = EFI_SUCCESS;
    }
  }
  return ReturnCode;
}

#define COPY_CHUNK_SZ_BYTES   1024

/**
  Helper that breaks up large memory copies into managable sized chunks
**/
STATIC
EFI_STATUS
PbrCopyChunks(
  IN VOID *pDest,
  IN UINT32 pDestSz,
  IN VOID *pSource,
  IN UINT32 pSourceSz
)
{
  UINT32 NumChunks = 0;
  UINT32 Index = 0;
  UINT32 Offset = 0;
  UINT32 LeftOverBytes = 0;

  NumChunks = pSourceSz / COPY_CHUNK_SZ_BYTES;

  for (Index = 0; Index < NumChunks; ++Index) {
    Offset = (Index* COPY_CHUNK_SZ_BYTES);
    CopyMem_S((VOID*)((UINTN)pDest + Offset),
      COPY_CHUNK_SZ_BYTES,
      (VOID*)((UINTN)pSource + Offset),
      COPY_CHUNK_SZ_BYTES);
  }

  LeftOverBytes = pSourceSz % COPY_CHUNK_SZ_BYTES;

  if (LeftOverBytes) {
    CopyMem_S((VOID*)((UINTN)pDest + pSourceSz - LeftOverBytes),
      LeftOverBytes,
      (VOID*)((UINTN)pSource + pSourceSz - LeftOverBytes),
      LeftOverBytes);
  }

  return EFI_SUCCESS;
}
