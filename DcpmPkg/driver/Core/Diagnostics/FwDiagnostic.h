/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FW_DIAGNOSTICS_H_
#define FW_DIAGNOSTICS_H_

#include "CoreDiagnostics.h"

/**
  Run Fw diagnostics for the list of DIMMs, and appropriately
  populate the result messages, and test-state.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the result string of fw diagnostics message
  @param[out] pDiagState Pointer to the fw diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_DEVICE_ERROR Test wasn't executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
  @retval EFI_OUT_OF_RESOURCES when memory allocation fails.
**/
EFI_STATUS
RunFwDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
     OUT CHAR16 **ppResult,
     OUT UINT8 *pDiagState
  );

/**
  Run Fw diagnostics for the list of DIMMs, and appropriately
  populate the result messages, and test-state.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] pResult Pointer to the result string of fw diagnostics message

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_DEVICE_ERROR Test wasn't executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
  @retval EFI_OUT_OF_RESOURCES when memory allocation fails.
**/
EFI_STATUS
RunFwDiagnosticsDetail(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  OUT DIAG_INFO *pResult
);
/**
  Check firmware consistency for the specified DIMMs, and accordingly append to
  the fw diagnostics result.
  Also, accordingly modifies the test-state.

  @param[in] pDimm Pointer to the DIMM
  @param[in] pDimmStr Dimm string to be used in result messages
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[in out] ppResult Pointer to the result string of fw diagnostics message
  @param[out] pDiagState Pointer to the fw diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
  @retval EFI_OUT_OF_RESOURCES when memory allocation fails.
**/
EFI_STATUS
CheckFwConsistency(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  IN OUT CHAR16 **ppResultStr,
     OUT UINT8 *pDiagState
  );

/**
Check viral policy consistency for the specified DIMMs, and accordingly append to
the fw diagnostics result.
Also, accordingly modifies the test-state.

@param[in] ppDimms The DIMM pointers list
@param[in] DimmCount DIMMs count
@param[in out] ppResultStr Pointer to the result string of fw diagnostics message
@param[out] pDiagState Pointer to the fw diagnostics test state. Possible states:
            DIAG_STATE_MASK_OK, DIAG_STATE_MASK_WARNING, DIAG_STATE_MASK_FAILED,
            DIAG_STATE_MASK_ABORTED

@retval EFI_SUCCESS Test executed correctly
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
@retval EFI_OUT_OF_RESOURCES when memory allocation fails.
**/
EFI_STATUS
CheckViralPolicyConsistency(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN OUT CHAR16 **ppResultStr,
  OUT UINT8 *pDiagState
);

/**
  Populate the list of unique subsystem device IDs across all
  the specified DIMMs

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[out] SubsystemDeviceIdList Array of the unique subsystem device IDs
  @param[out] pSubsystemDeviceIdListCount Pointer to the count of unique subsystem device IDs
  @param[out] pDiagState Pointer to the fw diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
PopulateSubsystemDeviceIdList(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
     OUT UINT16 SubsystemDeviceIdList[],
     OUT UINT16 *pSubsystemDeviceIdListCount,
     OUT UINT8 *pDiagState
  );

/**
  Determines the optimum firmware version for the specified list of DIMMs,
  for a particular subsystem device ID.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] SubsystemDeviceId Specified subsystem device ID, to select the DIMMs
             for which to determine the optimum firmware version
  @param[out] pOptimumFwVer Pointer to the optimum firmware version
  @param[out] pDiagState Pointer to the fw diagnostics test state

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
  @retval EFI_DEVICE_ERROR Test wasn't executed correctly
**/
EFI_STATUS
GetOptimumFwVersion(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT16 SubsystemDeviceId,
     OUT FIRMWARE_VERSION *pOptimumFwVer,
     OUT UINT8 *pDiagState
  );

/**
Get the smart and health data and checks the Media Temperature,
Controller Temperature and Spare Block thresholds.
Log proper events in case of any error.

@param[in] pDimm Pointer to the DIMM
@param[in out] ppResult Pointer to the result string of fw diagnostics message
@param[out] pDiagState Pointer to the quick diagnostics test state

@retval EFI_SUCCESS Test executed correctly
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
ThresholdsCheck(
  IN     DIMM *pDimm,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
);

#ifdef OS_BUILD
/**
Get the DIMMs system time and compare it to the local system time.
Log proper events in case of any error.

@param[in] pDimm Pointer to the DIMM
@param[in out] ppResult Pointer to the result string of fw diagnostics message
@param[out] pDiagState Pointer to the quick diagnostics test state

@retval EFI_SUCCESS Test executed correctly
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
SystemTimeCheck(
  IN     DIMM *pDimm,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
);
#endif // OS_BUILD
#endif
