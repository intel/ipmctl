/**
  @file
  @internal
  @copyright
  Copyright 2019 Intel Corporation All Rights Reserved.

  INTEL CONFIDENTIAL

  The source code contained or described herein and all documents related to
  the source code ("Material") are owned by Intel Corporation or its suppliers
  or licensors. Title to the Material remains with Intel Corporation or its
  suppliers and licensors. The Material may contain trade secrets and
  proprietary and confidential information of Intel Corporation and its
  suppliers and licensors, and is protected by worldwide copyright and trade
  secret laws and treaty provisions. No part of the Material may be used,
  copied, reproduced, modified, published, uploaded, posted, transmitted,
  distributed, or disclosed in any way without Intel's prior express written
  permission.

  No license under any patent, copyright, trade secret or other intellectual
  property right is granted to or conferred upon you by disclosure or delivery
  of the Materials, either expressly, by implication, inducement, estoppel or
  otherwise. Any license under such intellectual property rights must be
  express and approved by Intel in writing.

  Unless otherwise agreed by Intel in writing, you may not remove or alter
  this notice or any other notice embedded in Materials by Intel or Intel's
  suppliers or licensors in any way.
  @endinternal
**/

#include "ReadRunTimePreferences.h"
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
  UINT8 CapacityUnit;

  NVDIMM_ENTRY();

  if (NULL == pDisplayPreferences) {
    NVDIMM_DBG("One or more parameters are NULL");
    goto Finish;
  }

  if (DISPLAY_HII_INFO == DisplayRequest) {
    CapacityUnit = FixedPcdGet32(PcdDcpmmHiiDefaultCapacityUnit);
  }
  else if (DISPLAY_CLI_INFO == DisplayRequest) {
    CapacityUnit = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
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
