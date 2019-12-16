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



/**
  Return the current FW_CMD from the playback buffer

  @param[in] pContext: Pbr context
  @param[in] pCmd: current FW_CMD from the playback buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
PbrGetPassThruRecord(
  IN    PbrContext *pContext,
  OUT   FW_CMD *pCmd,
  OUT   EFI_STATUS *pPassThruRc
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrPassThruReq *ptReq;
  PbrPassThruResp *ptResp;
  VOID *pData = NULL;
  UINT32 DataSize = 0;
  UINT32 CurDataPos = 0;

  if (PBR_PLAYBACK_MODE != pContext->PbrMode) {
    return EFI_SUCCESS;
  }

  ReturnCode = PbrGetData(
                PBR_PASS_THRU_SIG,
                GET_NEXT_DATA_INDEX,
                &pData,
                &DataSize,
                NULL);

  if (EFI_SUCCESS != ReturnCode) {
    Print(L"Failed to get data!!!!\n");
    return ReturnCode;
  }

  ptReq = (PbrPassThruReq *)pData;
  if (pCmd->Opcode != ptReq->Opcode) {
    NVDIMM_ERR("Get Passthru Opcode mismatch, expected 0x%x, received 0x%x\n", pCmd->Opcode, ptReq->Opcode);
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  if (pCmd->SubOpcode != ptReq->SubOpcode) {
    NVDIMM_ERR("Get Passthru SubOpcode mismatch, expected 0x%x, received 0x%x\n", pCmd->SubOpcode, ptReq->SubOpcode);
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  //skip past the pass through request header
  CurDataPos += sizeof(PbrPassThruReq);
  //verify we didn't run out of data
  if (CurDataPos > DataSize) {
    NVDIMM_ERR("Failed to skip past the pass through request\n");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  CurDataPos += ptReq->InputPayloadSize;
  //verify we didn't run out of data
  if (CurDataPos > DataSize) {
    NVDIMM_ERR("Failed to skip past the InputPayload\n");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  CurDataPos += ptReq->InputLargePayloadSize;
  //verify we didn't run out of data
  if (CurDataPos > DataSize) {
    NVDIMM_ERR("Failed to skip past the InputLargePayload\n");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  //should be pointing to the response header
  ptResp = (PbrPassThruResp *)((UINTN)pData + (UINTN)CurDataPos);

  pCmd->Status = ptResp->Status;
  pCmd->OutputPayloadSize = ptResp->OutputPayloadSize;
  pCmd->LargeOutputPayloadSize = ptResp->OutputLargePayloadSize;
  *pPassThruRc = ptResp->PassthruReturnCode;

  //skip past the response header
  CurDataPos += sizeof(PbrPassThruResp);
  //verify we didn't run out of data
  if (CurDataPos > DataSize) {
    NVDIMM_ERR("Failed to skip past the PbrPassThruResp\n");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  //there is an output payload
  if (ptResp->OutputPayloadSize) {
    CopyMem_S(pCmd->OutPayload,
      OUT_PAYLOAD_SIZE,
      (UINT8*)((UINTN)pData + (UINTN)CurDataPos),
      ptResp->OutputPayloadSize);
  }
  //skip past the response output payload
  CurDataPos += ptResp->OutputPayloadSize;
  //verify we didn't run out of data
  if (CurDataPos > DataSize) {
    NVDIMM_ERR("Failed to skip past the OutputPayload\n");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  //there is a large output payload
  if (ptResp->OutputLargePayloadSize) {
    CopyMem_S(pCmd->LargeOutputPayload,
      OUT_MB_SIZE,
      (UINT8*)pData + CurDataPos,
      ptResp->OutputLargePayloadSize);
  }

Finish:
  FREE_POOL_SAFE(pData);
  return ReturnCode;
}

/**
  Record a FW_CMD into the recording buffer

  @param[in] pContext: Pbr context
  @param[in] pCmd: current FW_CMD from the playback buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
PbrSetPassThruRecord(
  IN    PbrContext *pContext,
  OUT   FW_CMD *pCmd,
  EFI_STATUS PassthruReturnCode
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PbrPassThruReq *ptReq = NULL;
  PbrPassThruResp *ptResp = NULL;
  VOID *pData = NULL;
  UINT32 DataSize = 0;

  if (NULL == pContext || NULL == pCmd) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (PBR_RECORD_MODE != pContext->PbrMode) {
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  DataSize += sizeof(PbrPassThruReq);
  DataSize += pCmd->InputPayloadSize;
  DataSize += pCmd->LargeInputPayloadSize;
  DataSize += sizeof(PbrPassThruResp);
  DataSize += pCmd->OutputPayloadSize;
  DataSize += pCmd->LargeOutputPayloadSize;

  CHECK_RESULT(PbrSetData(
    PBR_PASS_THRU_SIG,
    NULL,
    DataSize,
    FALSE,
    &pData,
    NULL), Finish);

  ptReq = (PbrPassThruReq*)pData;
  ptReq->DimmId = pCmd->DimmID;
  ptReq->Opcode = pCmd->Opcode;
  ptReq->SubOpcode = pCmd->SubOpcode;
  ptReq->TotalMilliseconds = 0xDEADBEEF;// GetCurrentMilliseconds();
  ptReq->InputPayloadSize = pCmd->InputPayloadSize;
  ptReq->InputLargePayloadSize = pCmd->LargeInputPayloadSize;

  if (pCmd->InputPayloadSize)
  {
    CopyMem_S((VOID*)((UINTN)pData + (UINTN)sizeof(PbrPassThruReq)),
      DataSize - sizeof(PbrPassThruReq),
      pCmd->InputPayload,
      pCmd->InputPayloadSize);
  }

  if (pCmd->LargeInputPayloadSize)
  {
    CopyMem_S((VOID*)((UINTN)pData + (UINTN)sizeof(PbrPassThruReq) + (UINTN)pCmd->InputPayloadSize),
      DataSize - sizeof(PbrPassThruReq) - pCmd->InputPayloadSize,
      pCmd->LargeInputPayload,
      pCmd->LargeInputPayloadSize);
  }

  ptResp = (PbrPassThruResp*)((UINTN)pData + (UINTN)sizeof(PbrPassThruReq) + (UINTN)pCmd->InputPayloadSize + (UINTN)pCmd->LargeInputPayloadSize);
  ptResp->DimmId = pCmd->DimmID;
  ptResp->PassthruReturnCode = PassthruReturnCode;
  ptResp->Status = pCmd->Status;
  ptResp->TotalMilliseconds = 0x55AA55AA;
  ptResp->OutputPayloadSize = pCmd->OutputPayloadSize;
  ptResp->OutputLargePayloadSize = pCmd->LargeOutputPayloadSize;

  if (pCmd->OutputPayloadSize)
  {
    CopyMem_S((VOID*)((UINTN)pData + sizeof(PbrPassThruReq) + pCmd->InputPayloadSize + pCmd->LargeInputPayloadSize + sizeof(PbrPassThruResp)),
      DataSize - sizeof(PbrPassThruReq) - pCmd->InputPayloadSize - pCmd->LargeInputPayloadSize - sizeof(PbrPassThruResp),
      pCmd->OutPayload,
      pCmd->OutputPayloadSize);
  }

  if (pCmd->LargeOutputPayloadSize)
  {
    CopyMem_S((VOID*)((UINTN)pData + sizeof(PbrPassThruReq) + pCmd->InputPayloadSize + pCmd->LargeInputPayloadSize + sizeof(PbrPassThruResp) + pCmd->OutputPayloadSize),
      DataSize - sizeof(PbrPassThruReq) - pCmd->InputPayloadSize - pCmd->LargeInputPayloadSize - sizeof(PbrPassThruResp) - pCmd->OutputPayloadSize,
      pCmd->LargeOutputPayload,
      pCmd->LargeOutputPayloadSize);
  }
Finish:
  return ReturnCode;
}


/**
  Return the current table from the playback buffer

  @param[in] pContext: Pbr context
  @param[in] TableType: 1-smbios, 2-nfit, 3-pcat, 4-pmtt

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
PbrGetTableRecord(
  IN    PbrContext *pContext,
  IN    UINT32 TableType,
  OUT   VOID **ppTable,
  OUT   UINT32 *pTableSize
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Signature = PBR_INVALID_SIG;

  NVDIMM_DBG("GetTablerecord...\n");

  if (PBR_PLAYBACK_MODE != pContext->PbrMode) {
    return ReturnCode;
  }

  NVDIMM_DBG("GetTablerecord type: %d\n", TableType);

  //todo check signatures
  switch (TableType) {
  case PBR_RECORD_TYPE_SMBIOS:
    Signature = PBR_SMBIOS_SIG;
    break;
  case PBR_RECORD_TYPE_NFIT:
    Signature = PBR_NFIT_SIG;
    break;
  case PBR_RECORD_TYPE_PCAT:
    Signature = PBR_PCAT_SIG;
    break;
  case PBR_RECORD_TYPE_PMTT:
    Signature = PBR_PMTT_SIG;
    break;
  default:
    NVDIMM_DBG("Unknown table type: %d", TableType);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = PbrGetData(
    Signature,
    0,
    ppTable,
    pTableSize,
    NULL
  );
  if (ReturnCode != EFI_SUCCESS) {
    Print(L"Failed to GET smbios\n");
  }
  else {
    NVDIMM_DBG("GetTablerecord type: %d, size: %d bytes\n", TableType, *pTableSize);
  }

Finish:
  return ReturnCode;
}

/**
  Record a table into the recording buffer

  @param[in] pContext: Pbr context
  @param[in] TableType: 1-smbios, 2-nfit, 3-pcat, 4-pmtt

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
EFI_STATUS
PbrSetTableRecord(
  IN    PbrContext *pContext,
  IN    UINT32 TableType,
  IN    VOID *pTable,
  IN    UINT32 TableSize
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Signature = PBR_INVALID_SIG;

  NVDIMM_DBG("SetTablerecord...\n");
  if (PBR_RECORD_MODE != pContext->PbrMode) {
    return ReturnCode;
  }

  NVDIMM_DBG("SetTablerecord type: %d\n", TableType);
  switch (TableType) {
  case PBR_RECORD_TYPE_SMBIOS:
    NVDIMM_DBG("Set Table Record: SMBIOS: Table size: %d\n", TableSize);
    Signature = PBR_SMBIOS_SIG;
    break;
  case PBR_RECORD_TYPE_NFIT:
    NVDIMM_DBG("Set Table Record: NFIT: Table size: %d\n", TableSize);
    Signature = PBR_NFIT_SIG;
    break;
  case PBR_RECORD_TYPE_PCAT:
    NVDIMM_DBG("Set Table Record: PCAT: Table size: %d\n", TableSize);
    Signature = PBR_PCAT_SIG;
    break;
  case PBR_RECORD_TYPE_PMTT:
    NVDIMM_DBG("Set Table Record: PMTT: Table size: %d\n", TableSize);
    Signature = PBR_PMTT_SIG;
    break;
  default:
    NVDIMM_DBG("Unknown table type: %d", TableType);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = PbrSetData(
    Signature,
    pTable,
    TableSize,
    TRUE,
    NULL,
    NULL);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to set partition data (signature: %d)\n", Signature);
    goto Finish;
  }
Finish:
  return ReturnCode;
}
