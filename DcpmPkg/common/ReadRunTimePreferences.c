/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ReadRunTimePreferences.h"
#ifdef OS_BUILD
#include <os_efi_preferences.h>
#else
extern EFI_RUNTIME_SERVICES  *gRT;
#endif
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Debug.h>

/**
  Retrieve the User Preferences from RunTime Services.

  @param[in, out] pDisplayPreferences pointer to the current driver preferences.
  @param[in] DisplayRequest enum of whether information from hii or cli will be displayed.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
ReadRunTimePreferences(
  IN  OUT DISPLAY_PREFERENCES *pDisplayPreferences,
  IN BOOLEAN DisplayRequest
) {
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINTN VariableSize = 0;
  UINT16 CapacityUnit;

  NVDIMM_ENTRY();

  if (NULL == pDisplayPreferences) {
    NVDIMM_DBG("One or more parameters are NULL");
    goto Finish;
  }

  if (DISPLAY_HII_INFO == DisplayRequest) {
    CapacityUnit = FixedPcdGet16(PcdDcpmmHiiDefaultCapacityUnit);
  }
  else if (DISPLAY_CLI_INFO == DisplayRequest) {
    CapacityUnit = FixedPcdGet16(PcdDcpmmCliDefaultCapacityUnit);
  }
  else {
    NVDIMM_DBG("Invalid display information requested");
    goto Finish;
  }

  ZeroMem(pDisplayPreferences, sizeof(*pDisplayPreferences));

  VariableSize = sizeof(pDisplayPreferences->DimmIdentifier);
  ReturnCode = GET_VARIABLE(
    DISPLAY_DIMM_ID_VARIABLE_NAME,
    gNvmDimmVariableGuid,
    &VariableSize,
    &pDisplayPreferences->DimmIdentifier);

  if (EFI_NOT_FOUND == ReturnCode) {
    pDisplayPreferences->DimmIdentifier = DISPLAY_DIMM_ID_DEFAULT;
    ReturnCode = EFI_SUCCESS;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve DimmID Display Variable");
    goto Finish;
  }

  VariableSize = sizeof(pDisplayPreferences->SizeUnit);
  ReturnCode = GET_VARIABLE(
    DISPLAY_SIZE_VARIABLE_NAME,
    gNvmDimmVariableGuid,
    &VariableSize,
    &pDisplayPreferences->SizeUnit);

  if (EFI_NOT_FOUND == ReturnCode) {
    pDisplayPreferences->SizeUnit = CapacityUnit;
    ReturnCode = EFI_SUCCESS;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve Size Display Variable");
    goto Finish;
  }

  if (pDisplayPreferences->SizeUnit >= DISPLAY_SIZE_MAX_SIZE ||
    pDisplayPreferences->DimmIdentifier >= DISPLAY_DIMM_ID_MAX_SIZE) {
    NVDIMM_DBG("Parameters retrieved from RT services are invalid, setting defaults");
    pDisplayPreferences->SizeUnit = CapacityUnit;
    pDisplayPreferences->DimmIdentifier = DISPLAY_DIMM_ID_DEFAULT;
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
