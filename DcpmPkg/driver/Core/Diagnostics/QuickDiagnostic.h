/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QUICK_DIAGNOSTICS_H_
#define QUICK_DIAGNOSTICS_H_

#include "CoreDiagnostics.h"

/**
  Run quick diagnostics for the list of DIMMs, and appropriately
  populate the result messages, and test-state.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the result string of quick diagnostics message
  @param[out] pDiagState Pointer to the quick diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_DEVICE_ERROR Test wasn't executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
RunQuickDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
     OUT CHAR16 **ppResult,
     OUT UINT8 *pDiagState
  );

/**
  Check manageability for a DIMM, and accordingly append to
  the quick diagnostics result.
  Also, accordingly modifies the test-state.

  @param[in] pDimm Pointer to the DIMM
  @param[in] pDimmStr Dimm string to be used in result messages
  @param[out] ppResult Pointer to the result string of quick diagnostics message
  @param[out] pDiagState Pointer to the quick diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
DiagnosticsManageabilityCheck(
  IN     DIMM *pDimm,
  IN     CHAR16 *pDimmStr,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
  );

/**
  Run SMART and health check for a DIMM, and accordingly append to
  the quick diagnostics result.
  Also, accordingly modifies the test-state.

  @param[in] pDimm Pointer to the DIMM
  @param[in] pDimmStr Dimm string to be used in result messages
  @param[out] ppResult Pointer to the result string of quick diagnostics message
  @param[out] pDiagState Pointer to the quick diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
SmartAndHealthCheck(
  IN     DIMM *pDimm,
  IN     CHAR16 *pDimmStr,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
  );

/**
  Run boot status register check for a DIMM, and accordingly append to
  the quick diagnostics result.
  Also, accordingly modifies the test-state.

  @param[in] pDimm Pointer to the DIMM
  @param[in] pDimmStr Dimm string to be used in result messages
  @param[out] ppResult Pointer to the result string of quick diagnostics message
  @param[out] pDiagState Pointer to the quick diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_DEVICE_ERROR Internal device error
**/
EFI_STATUS
BootStatusDiagnosticsCheck(
  IN     DIMM *pDimm,
  IN     CHAR16 *pDimmStr,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
  );
#endif
