/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "NvmDimmDriverData.h"
#include "NvmDimmPassThru.h"
#include "NvmSecurity.h"
#include "Namespace.h"
#include "Debug.h"
#include "Dimm.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

/**
  Get DIMM security state

  @param [in]  DimmPid Pointer to DIMM
  @param [in]  Timeout The timeout in 100ns units, to use for the PassThru protocol
  @param [out] pSecurityState DIMM security state

  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_DEVICE_ERROR Failed on PassThru protocol
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetDimmSecurityState(
  IN     DIMM *pDimm,
  IN     UINT64 Timeout,
     OUT UINT32 *pSecurityState
  )
{
  NVM_FW_CMD *pPassThruCommand = NULL;
  PT_GET_SECURITY_PAYLOAD *pSecurityPayload = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pSecurityState == NULL){
    goto Finish;
  }

  pPassThruCommand = AllocateZeroPool(sizeof(*pPassThruCommand));
  if (pPassThruCommand == NULL) {
    NVDIMM_ERR("Out of memory.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  pPassThruCommand->DimmID = pDimm->DimmID;
  pPassThruCommand->Opcode = PtGetSecInfo;
  pPassThruCommand->SubOpcode= SubopGetSecState;
  pPassThruCommand->OutputPayloadSize = sizeof(*pSecurityPayload);

  ReturnCode = PassThru(pDimm, pPassThruCommand, Timeout);
  NVDIMM_DBG("PtReturnCode=" FORMAT_EFI_STATUS ", FwReturnCode=%d", ReturnCode, pPassThruCommand->Status);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on PassThru.");
    if FW_ERROR(pPassThruCommand->Status) {
      FW_CMD_ERROR_TO_EFI_STATUS(pPassThruCommand, ReturnCode);
    }
    goto FinishFreeMem;
  }

  pSecurityPayload = (PT_GET_SECURITY_PAYLOAD*) &pPassThruCommand->OutPayload;

  *pSecurityState = pSecurityPayload->SecurityStatus.AsUint32;
  ReturnCode = EFI_SUCCESS;

FinishFreeMem:
  FREE_POOL_SAFE(pPassThruCommand);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set DIMM security state

  @param [in]  pDimm Pointer to DIMM
  @param [in]  Opcode PassThru command opcode
  @param [in]  Subopcode PassThru command subopcode
  @param [in]  PayloadBufferSize Size of PassThru command payload
  @param [in]  PayloadBuffer Input buffer location
  @param [in]  Timeout The timeout in 100ns units, to use for the PassThru protocol

  @retval EFI_INVALID_PARAMETER Input parameters are not correct
  @retval EFI_DEVICE_ERROR Failed on PassThru protocol
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
SetDimmSecurityState(
  IN     DIMM *pDimm,
  IN     UINT8 Opcode,
  IN     UINT8 SubOpcode,
  IN     UINT16 PayloadBufferSize,
  IN     VOID *pPayloadBuffer OPTIONAL,
  IN     UINT64 Timeout
  )
{
  NVM_FW_CMD *pPassThruCommand = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if((pPayloadBuffer != NULL) && (PayloadBufferSize > IN_PAYLOAD_SIZE)) {
    NVDIMM_DBG("Buffer size exceeds input payload size.");
    goto Finish;
  }

  //Only SetSecurity Opcode supported
  if(Opcode != PtSetSecInfo) {
    goto Finish;
  }

  pPassThruCommand = AllocateZeroPool(sizeof(*pPassThruCommand));
  if (pPassThruCommand == NULL) {
    NVDIMM_ERR("Out of memory.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pPassThruCommand->DimmID = pDimm->DimmID;
  pPassThruCommand->Opcode = Opcode;
  pPassThruCommand->SubOpcode= SubOpcode;
  if (pPayloadBuffer != NULL) {
    CopyMem_S(&pPassThruCommand->InputPayload, sizeof(pPassThruCommand->InputPayload), pPayloadBuffer, PayloadBufferSize);
    pPassThruCommand->InputPayloadSize = PayloadBufferSize;
  }

  ReturnCode = PassThru(pDimm, pPassThruCommand, Timeout);
  NVDIMM_DBG("PtReturnCode=" FORMAT_EFI_STATUS ", FwReturnCode=%d", ReturnCode, pPassThruCommand->Status);
  if(EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on PassThru");
    if (FW_ERROR(pPassThruCommand->Status)) {
      FW_CMD_ERROR_TO_EFI_STATUS(pPassThruCommand, ReturnCode);
    }
    goto FinishFreeMem;
  }

  ReturnCode = EFI_SUCCESS;

FinishFreeMem:
  FREE_POOL_SAFE(pPassThruCommand);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert security bitmask to a defined state

  @param[in] SecurityFlag - mask from DIMM structure
  @param[out] pSecurityState - pointer to output with defined Security State
**/
VOID
ConvertSecurityBitmask(
  IN     UINT32 SecurityFlag,
     OUT UINT8 *pSecurityState
  )
{
  NVDIMM_ENTRY();

  if (pSecurityState == NULL) {
    return;
  }

  if (SecurityFlag & SECURITY_MASK_NOT_SUPPORTED) {
    *pSecurityState = SECURITY_NOT_SUPPORTED;
  }
  else if  (SecurityFlag & SECURITY_MASK_FROZEN){
    *pSecurityState = SECURITY_FROZEN;
  }
  else if (SecurityFlag & SECURITY_MASK_ENABLED) {
    if (SecurityFlag & SECURITY_MASK_COUNTEXPIRED) {
      *pSecurityState = SECURITY_PW_MAX;
    } else if (SecurityFlag & SECURITY_MASK_MASTER_COUNTEXPIRED) {
      *pSecurityState = SECURITY_MASTER_PW_MAX;
    } else if (SecurityFlag & SECURITY_MASK_LOCKED) {
      *pSecurityState = SECURITY_LOCKED;
    } else {
      *pSecurityState = SECURITY_UNLOCKED;
    }
  } else {
    *pSecurityState = SECURITY_DISABLED;
  }

  NVDIMM_EXIT();
}

/**
  Convert security state to information if configuring is allowed

  @param[in] SecurityFlag Security mask from FW

  @retval TRUE if configuring is allowed
  @retval FALSE if configuring is not allowed
**/
BOOLEAN
IsConfiguringAllowed(
  IN     UINT32 SecurityFlag
  )
{
  BOOLEAN IsAllowed = FALSE;

  IsAllowed =
    (
      !(SecurityFlag & SECURITY_MASK_ENABLED) ||
      !(SecurityFlag & SECURITY_MASK_LOCKED) ||
      (SecurityFlag & SECURITY_MASK_NOT_SUPPORTED)
    );

  return IsAllowed;
}

/**
  Convert security state to information if configuring for create goal is allowed

  @param[in] SecurityFlag Security mask from FW

  @retval TRUE if configuring for create goal is allowed
  @retval FALSE if configuring for create goal is not allowed
**/
BOOLEAN
IsConfiguringForCreateGoalAllowed(
  IN     UINT32 SecurityFlag
  )
{
    BOOLEAN IsAllowed = FALSE;

    IsAllowed = !(SecurityFlag & SECURITY_MASK_ENABLED)
      || ((SecurityFlag & SECURITY_MASK_ENABLED) && !(SecurityFlag & SECURITY_MASK_LOCKED));

    return IsAllowed;
}
