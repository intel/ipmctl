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
#include "PbrOs.h"
#include "PbrDcpmm.h"
#include <os.h>
#ifdef _MSC_VER
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <time.h>
#include <wchar.h>
#include <string.h>
extern int registry_volatile_write(const char *key, unsigned int dword_val);
extern int registry_read(const char *key, unsigned int *dword_val, unsigned int default_val);
#else
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#include <safe_str_lib.h>
#include <safe_mem_lib.h>
#include <safe_lib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#define _read read
#define _getch getchar
#endif

#define PBR_CTX_FILE_NAME         "pbr_ctx.tmp"
#define PBR_MAIN_FILE_NAME        "pbr_main.tmp"
#define FILE_READ_OPTS            "rb"
#define FILE_WRITE_OPTS           "wb"

VOID SerializePbrMode(UINT32 mode);
VOID DeserializePbrMode(UINT32 *pMode, UINT32 defaultMode);

/**Memory buffer serialization**/
#define SerializeBuffer(file, buffer, size) \
  if (0 != fopen_s(&pFile, file, FILE_WRITE_OPTS)) \
  { \
    NVDIMM_ERR("Failed to open the PBR file: %s\n", file); \
    ReturnCode = EFI_NOT_FOUND; \
    goto Finish; \
  } \
  BytesWritten = fwrite(buffer, size, 1, pFile); \
  if (1 != BytesWritten) \
  { \
    NVDIMM_ERR("Failed to serialize the PBR file: %s\n", file); \
    ReturnCode = EFI_END_OF_FILE; \
    goto Finish; \
  } \
  if (pFile) { \
    fclose(pFile); \
    pFile = NULL; \
  }

/**Memory buffer deserialization**/
#define DeserializeBuffer(file, buffer, size) \
  if (0 != fopen_s(&pFile, file, FILE_READ_OPTS)) \
  { \
    NVDIMM_ERR("Failed to open the PBR file: %s\n", file); \
    ReturnCode = EFI_END_OF_FILE; \
    goto Finish; \
  } \
  fseek(pFile, 0L, SEEK_END); \
  fseek(pFile, 0L, SEEK_SET); \
  buffer = malloc(size); \
  if(NULL == buffer) { \
    NVDIMM_ERR("Failed to allocate memory for deserializing buffer\n"); \
    ReturnCode = EFI_OUT_OF_RESOURCES; \
    goto Finish; \
  } \
  if (1 != fread(buffer, size, 1, pFile)) \
  { \
    NVDIMM_ERR("Failed to read the PBR file: %s\n", file); \
    ReturnCode = EFI_END_OF_FILE; \
    goto Finish; \
  } \
  if (pFile) { \
      fclose(pFile); \
      pFile = NULL; \
  }

#define DeserializeBufferEx(file, buffer, size) \
  if (0 != fopen_s(&pFile, file, FILE_READ_OPTS)) \
  { \
    NVDIMM_ERR("Failed to open the PBR file: %s\n", file); \
  } \
  else { \
  fseek(pFile, 0L, SEEK_END); \
  fseek(pFile, 0L, SEEK_SET); \
  buffer = malloc(size); \
  if(NULL == buffer) { \
    NVDIMM_ERR("Failed to allocate memory for deserializing buffer\n"); \
  } \
  if (1 != fread(buffer, size, 1, pFile)) \
  { \
    NVDIMM_ERR("Failed to read the PBR file: %s\n", file); \
  } \
  if (pFile) { \
      fclose(pFile); \
      pFile = NULL; \
  } \
  }

/**
  Helper that restores the context.  Note, effort has been taken to NOT preserve the
  session pbr mode across boots.
**/
EFI_STATUS PbrSerializeCtx(
  PbrContext *ctx,
  BOOLEAN Force
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  FILE* pFile = NULL;
  size_t BytesWritten = 0;
  char pbr_dir[100];
  char pbr_filename[100];
  UINT32 CtxIndex = 0;

  if (NULL == ctx) {
    NVDIMM_DBG("ctx is null\n");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  //create temp directory (buffers serialized into files that reside here)
  AsciiSPrint(pbr_dir, sizeof(pbr_dir), PBR_TMP_DIR);
  os_mkdir(pbr_dir);

  SerializePbrMode(ctx->PbrMode);

  if (ctx->PbrMode == PBR_NORMAL_MODE && !Force) {
    return ReturnCode;
  }

  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    if (PBR_INVALID_SIG != ctx->PartitionContexts[CtxIndex].PartitionSig) {
      AsciiSPrint(pbr_filename, sizeof(pbr_filename), "%x.pbr", ctx->PartitionContexts[CtxIndex].PartitionSig);
      AsciiSPrint(pbr_dir, sizeof(pbr_dir), "%s%s", PBR_TMP_DIR, pbr_filename);
      SerializeBuffer(pbr_dir, ctx->PartitionContexts[CtxIndex].PartitionData, ctx->PartitionContexts[CtxIndex].PartitionSize);
    }
  }

  /**Serialize the PBR context struct**/
  SerializeBuffer(PBR_TMP_DIR PBR_CTX_FILE_NAME, ctx, sizeof(PbrContext));
  /**Serialize the PBR main header**/
  SerializeBuffer(PBR_TMP_DIR PBR_MAIN_FILE_NAME, ctx->PbrMainHeader, sizeof(PbrHeader));

Finish:
  if (pFile) {
    fclose(pFile);
  }
  return ReturnCode;
}

/**
  Helper that saves the context.  Note, effort has been taken to NOT preserve the
  session pbr mode across boots.
**/
EFI_STATUS PbrDeserializeCtx(
  PbrContext *ctx
)
{

  EFI_STATUS ReturnCode = EFI_SUCCESS;
  FILE* pFile = NULL;
  UINT32 PbrMode = PBR_NORMAL_MODE;
  char pbr_dir[100];
  char pbr_filename[100];
  UINT32 CtxIndex = 0;

  if (NULL == ctx) {
    NVDIMM_DBG("ctx is null\n");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  //create temp directory (buffers serialized into files that reside here)
  AsciiSPrint(pbr_dir, sizeof(pbr_dir), PBR_TMP_DIR);
  os_mkdir(pbr_dir);

  DeserializePbrMode(&PbrMode, PBR_NORMAL_MODE);

  NVDIMM_DBG("PBR MODE from shared memory: %d\n", PbrMode);

  AsciiSPrint(pbr_filename, sizeof(pbr_filename), "%x.pbr", PBR_PASS_THRU_SIG);
  AsciiSPrint(pbr_dir, sizeof(pbr_dir), "%s%s", PBR_TMP_DIR, pbr_filename);

  /**Deserialize the PBR context struct**/
  if (0 != fopen_s(&pFile, PBR_TMP_DIR PBR_CTX_FILE_NAME, FILE_READ_OPTS))
  {
    NVDIMM_DBG("pbr_ctx.tmp not found, setting to default value\n");
    ctx->PbrMode = PBR_NORMAL_MODE;
    return EFI_SUCCESS;
  }

  if (1 != fread(ctx, sizeof(PbrContext), 1, pFile))
  {
    NVDIMM_ERR("Failed to read the PBR context\n");
    ReturnCode = EFI_END_OF_FILE;
    goto Finish;
  }

  fclose(pFile);

  /**Deserialize the PBR main header**/

  DeserializeBuffer(PBR_TMP_DIR PBR_MAIN_FILE_NAME, ctx->PbrMainHeader, sizeof(PbrHeader));

  for (CtxIndex = 0; CtxIndex < MAX_PARTITIONS; ++CtxIndex) {
    if (PBR_INVALID_SIG != ctx->PartitionContexts[CtxIndex].PartitionSig) {
      AsciiSPrint(pbr_filename, sizeof(pbr_filename), "%x.pbr", ctx->PartitionContexts[CtxIndex].PartitionSig);
      AsciiSPrint(pbr_dir, sizeof(pbr_dir), "%s%s", PBR_TMP_DIR, pbr_filename);
      DeserializeBufferEx(pbr_dir, ctx->PartitionContexts[CtxIndex].PartitionData, ctx->PartitionContexts[CtxIndex].PartitionSize);
    }
  }

  ctx->PbrMode = PbrMode;

Finish:
  if (pFile) {
    fclose(pFile);
  }

  return ReturnCode;
}

/**
  Helper that serializes pbr mode to a volatile store.  We should not be maintaining
  sessions across system reboots
**/
VOID SerializePbrMode(
  UINT32 mode
)
{
#if _MSC_VER
  registry_volatile_write("pbr_mode", mode);
#else
  UINT32 ShmId;
  key_t Key;
  UINT32 *pPbrMode = NULL;
  Key = ftok(PBR_TMP_DIR, 'h');
  ShmId = shmget(Key, sizeof(*pPbrMode), IPC_CREAT | 0666);
  if (-1 == ShmId) {
    NVDIMM_DBG("Failed to shmget\n");
    return;
  }
  pPbrMode = (UINT32*)shmat(ShmId, NULL, 0);
  if ((VOID*)pPbrMode == (VOID*)-1) {
    NVDIMM_DBG("Failed to shmat\n");
  }
  else
  {
    *pPbrMode = mode;
    NVDIMM_DBG("Writing to shared memory: %d\n", *pPbrMode);
    shmdt(pPbrMode);
  }
#endif
}

/**
  Helper that deserializes pbr mode from a volatile store.  We should not be maintaining
  sessions across system reboots
**/
VOID DeserializePbrMode(
  UINT32 *pMode,
  UINT32 defaultMode
)
{
#if _MSC_VER
  registry_read("pbr_mode", pMode, defaultMode);
#else
  UINT32 ShmId;
  key_t Key;
  UINT32 *pPbrMode = NULL;
  Key = ftok(PBR_TMP_DIR, 'h');
  ShmId = shmget(Key, sizeof(*pPbrMode), IPC_CREAT | 0666);
  if (-1 == ShmId) {
    NVDIMM_DBG("Failed to shmget\n");
    return;
  }
  pPbrMode = (UINT32*)shmat(ShmId, NULL, 0);
  if ((VOID*)pPbrMode == (VOID*)-1) {
    NVDIMM_DBG("Failed to shmat\n");
    *pMode = defaultMode;
  }
  else
  {
    *pMode = *pPbrMode;
    shmdt(pPbrMode);
  }
#endif
}


