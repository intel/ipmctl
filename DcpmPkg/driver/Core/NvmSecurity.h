/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVM_SECURITY_H_
#define _NVM_SECURITY_H_

#include <Uefi.h>
#include <Dimm.h>

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
     OUT UINT8 *pSecurityState
  );

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
  );

/**
  Convert security bitmask to a defined state

  @param[in] SecurityFlag - mask from DIMM structure
  @param[out] pSecurityState - pointer to output with defined Security State
**/
VOID
ConvertSecurityBitmask(
  IN     UINT8 SecurityFlag,
     OUT UINT8 *pSecurityState
  );

/**
  Convert security state to information if configuring is allowed

  @param[in] SecurityFlag Security mask from FW

  @retval TRUE if configuring is allowed
  @retval FALSE if configuring is not allowed
**/
BOOLEAN
IsConfiguringAllowed(
  IN     UINT8 SecurityFlag
  );

/**
  Convert security state to information if configuring for create goal is allowed

  @param[in] SecurityFlag Security mask from FW

  @retval TRUE if configuring for create goal is allowed
  @retval FALSE if configuring for create goal is not allowed
**/
BOOLEAN
IsConfiguringForCreateGoalAllowed(
  IN     UINT8 SecurityFlag
  );

#endif /** _NVM_SECURITY_H_ **/
