/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef OS_BUILD
#include <time.h>
#endif // OS_BUILD
#include "FwDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#define DEFAULT_FW_LOG_LEVEL_VALUE FW_LOG_LEVEL_ERROR

#ifdef OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) SendTheEventAndAppendToDiagnosticsResult(pDimm,pStr,StateMask,__COUNTER__,SYSTEM_EVENT_CAT_FW,ppResult,pState)
#else // OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) AppendToDiagnosticsResult(pStr,StateMask,ppResult,pState)
#endif // OS_BUILD

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pTmpStr = NULL;

  NVDIMM_ENTRY();

  if (ppResult == NULL || pDiagState == NULL || DimmCount > MAX_DIMMS) {
    NVDIMM_DBG("The firmware consistency and settings diagnostics test aborted due to an internal error.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmCount == 0 || ppDimms == NULL) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NO_MANAGEABLE_DIMMS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
    goto Finish;
  }

  if (*ppResult != NULL) {
    NVDIMM_DBG("The passed result string for firmware diagnostics tests is not empty");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  ReturnCode = CheckFwConsistency(ppDimms, DimmCount, DimmIdPreference, ppResult, pDiagState);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for firmware consistency failed.");
    if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
      goto FinishError;
    }
  }

#ifdef OS_BUILD
  UINT16 Index = 0;
  for (Index = 0; Index < DimmCount; Index++) {
    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
      goto Finish;
    }

    ReturnCode = ThresholdsCheck(ppDimms[Index], ppResult, pDiagState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The check for firmware threshold settings failed. Dimm handle 0x%04x.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
        goto FinishError;
      }
    }

    ReturnCode = SystemTimeCheck(ppDimms[Index], ppResult, pDiagState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The check for Dimm's system time failed. Dimm handle 0x%04x.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
        goto FinishError;
      }
    }

    ReturnCode = FwLogLevelCheck(ppDimms[Index], ppResult, pDiagState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The check for Dimm's FW log level failed. Dimm handle 0x%04x.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((*pDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
        goto FinishError;
      }
    }
  }
#endif // OS_BUILD

  if ((*pDiagState & DIAG_STATE_MASK_ALL) <= DIAG_STATE_MASK_OK) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_SUCCESS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_OK, ppResult, pDiagState);
  }
  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishError:
  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_ABORTED_INTERNAL_ERROR), NULL);
  APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Populate the list of unique subsytem device IDs across all
  the specified DIMMs

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[out] SubsystemDeviceIdList Array of the unique subsytem device IDs
  @param[out] pSubsystemDeviceIdListCount Pointer to the count of unique subsytem device IDs
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 TmpSubsystemDeviceIdListCount = 0;
  BOOLEAN DeviceIdExists = FALSE;
  UINTN Index = 0;
  UINTN Index1 = 0;

  NVDIMM_ENTRY();

  if (ppDimms == NULL || SubsystemDeviceIdList == NULL || pSubsystemDeviceIdListCount == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; Index++) {
    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
      goto Finish;
    }

    for (Index1 = 0; Index1 < TmpSubsystemDeviceIdListCount; Index1++) {
       if (SubsystemDeviceIdList[Index1] == ppDimms[Index]->SubsystemDeviceId) {
         DeviceIdExists = TRUE;
         break;
       }
       DeviceIdExists = FALSE;
    }
    if (!DeviceIdExists) {
      SubsystemDeviceIdList[TmpSubsystemDeviceIdListCount] = ppDimms[Index]->SubsystemDeviceId;
      TmpSubsystemDeviceIdListCount++;
    }
  }

  *pSubsystemDeviceIdListCount = TmpSubsystemDeviceIdListCount;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  FIRMWARE_VERSION InitialFwVer;
  UINT16 Index = 0;
  UINT16 DimmIndex = 0;

  NVDIMM_ENTRY();

  ZeroMem(&InitialFwVer, sizeof(InitialFwVer));

  if (ppDimms == NULL || DimmCount == 0 || pOptimumFwVer == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; Index++) {
    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
      goto Finish;
    }

    if (ppDimms[Index]->SubsystemDeviceId == SubsystemDeviceId) {
      if (InitialFwVer.FwProduct < ppDimms[Index]->FwVer.FwProduct) {
        InitialFwVer = ppDimms[Index]->FwVer;
        DimmIndex = Index;
      }
      else if (InitialFwVer.FwProduct == ppDimms[Index]->FwVer.FwProduct) {
        if (InitialFwVer.FwRevision < ppDimms[Index]->FwVer.FwRevision) {
          InitialFwVer = ppDimms[Index]->FwVer;
          DimmIndex = Index;
        } else if (InitialFwVer.FwRevision == ppDimms[Index]->FwVer.FwRevision) {
	  if (InitialFwVer.FwSecurityVersion < ppDimms[Index]->FwVer.FwSecurityVersion) {
	    InitialFwVer = ppDimms[Index]->FwVer;
            DimmIndex = Index;
	  } else if (InitialFwVer.FwSecurityVersion == ppDimms[Index]->FwVer.FwSecurityVersion) {
	    if (InitialFwVer.FwBuild < ppDimms[Index]->FwVer.FwBuild) {
	      InitialFwVer = ppDimms[Index]->FwVer;
              DimmIndex = Index;
            }
          }
        }
      }
    }
  }

  if (InitialFwVer.FwProduct != 0 || InitialFwVer.FwRevision != 0 ||
      InitialFwVer.FwSecurityVersion != 0 || InitialFwVer.FwBuild != 0) {
    *pOptimumFwVer = ppDimms[DimmIndex]->FwVer;
  } else {
    ReturnCode = EFI_DEVICE_ERROR;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
  UINT16 SubsystemDeviceIdListCount = 0;
  UINT16 SubsystemDeviceIdList[MAX_DIMMS];
  FIRMWARE_VERSION OptimumFwVersion[MAX_DIMMS];
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *pAppendedDimmsStr = NULL;
  CHAR16 OptimumFwVersionStr[FW_VERSION_LEN];
  CHAR16 TmpFwVerStr[FW_VERSION_LEN];
  UINTN Index = 0;
  UINTN Index1 = 0;

  NVDIMM_ENTRY();

  ZeroMem(SubsystemDeviceIdList, sizeof(SubsystemDeviceIdList));
  ZeroMem(OptimumFwVersion, sizeof(OptimumFwVersion));
  ZeroMem(OptimumFwVersionStr, sizeof(OptimumFwVersionStr));
  ZeroMem(TmpFwVerStr, sizeof(TmpFwVerStr));

  if (DimmCount == 0 || ppDimms == NULL || DimmCount > MAX_DIMMS ||
       ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    goto Finish;
  }

  ReturnCode = PopulateSubsystemDeviceIdList(ppDimms, DimmCount, SubsystemDeviceIdList, &SubsystemDeviceIdListCount, pDiagState);
  if (EFI_ERROR(ReturnCode) || (SubsystemDeviceIdListCount == 0)) {
    NVDIMM_DBG("The functioanlity to populate subsystem device Id list failed.");
    goto Finish;
  }

  for (Index = 0; Index < SubsystemDeviceIdListCount; ++Index) {
    ReturnCode = GetOptimumFwVersion(ppDimms, DimmCount, SubsystemDeviceIdList[Index], &OptimumFwVersion[Index], pDiagState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ConvertFwVersion(OptimumFwVersionStr, OptimumFwVersion[Index].FwProduct,  OptimumFwVersion[Index].FwRevision,
      OptimumFwVersion[Index].FwSecurityVersion, OptimumFwVersion[Index].FwBuild);
    for (Index1 = 0; Index1 < DimmCount; Index1++) {
      if (ppDimms[Index1] == NULL) {
        ReturnCode = EFI_INVALID_PARAMETER;
        *pDiagState |= DIAG_STATE_MASK_ABORTED;
        goto Finish;
      }

      if (ppDimms[Index1]->SubsystemDeviceId == SubsystemDeviceIdList[Index]) {
        ConvertFwVersion(TmpFwVerStr, ppDimms[Index1]->FwVer.FwProduct, ppDimms[Index1]->FwVer.FwRevision,
          ppDimms[Index1]->FwVer.FwSecurityVersion, ppDimms[Index1]->FwVer.FwBuild);
        if (StrCmp(OptimumFwVersionStr, TmpFwVerStr) != 0) {
          ReturnCode = GetDimmUid(ppDimms[Index1], DimmUid, MAX_DIMM_UID_LENGTH);
          if (EFI_ERROR(ReturnCode)) {
            goto Finish;
          }
          ReturnCode = GetPreferredValueAsString(ppDimms[Index1]->DeviceHandle.AsUint32, DimmUid, DimmIdPreference == DISPLAY_DIMM_ID_HANDLE,
            DimmStr, MAX_DIMM_UID_LENGTH);
          if (EFI_ERROR(ReturnCode)) {
            goto Finish;
          }
          pAppendedDimmsStr = CatSPrintClean(pAppendedDimmsStr, (pAppendedDimmsStr == NULL) ? FORMAT_STR : L", " FORMAT_STR, DimmStr);
        }
      }
    }

    if (pAppendedDimmsStr != NULL) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_INCONSISTENT), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pAppendedDimmsStr, SubsystemDeviceIdList[Index], OptimumFwVersionStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
      FREE_POOL_SAFE(pAppendedDimmsStr);
    }
  }

Finish:
  FREE_POOL_SAFE(pAppendedDimmsStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#ifdef OS_BUILD
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
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  SENSOR_INFO SensorInfo;
  INT16 MediaTemperatureThreshold = 0;
  INT16 ControllerTemperatureThreshold = 0;
  INT16 SpareBlockThreshold = 0;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&SensorInfo, sizeof(SensorInfo));

  if ((NULL == pDimm) || (NULL == pDiagState) || (NULL == ppResultStr)) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetSmartAndHealth(NULL, pDimm->DimmID, &SensorInfo, NULL, NULL, NULL);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to Get SMART Info from Dimm handle 0x%x", pDimm->DeviceHandle.AsUint32);
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  }

  //Temperature and capacity checks
  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_MEDIA_TEMPERATURE,
    &MediaTemperatureThreshold,
    NULL,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get %s alarm threshold Dimm handle 0x%x", MEDIA_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (SensorInfo.MediaTempShutdownThresh < MediaTemperatureThreshold) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_MEDIA_TEMPERATURE_THRESHOLD_ERROR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32, ControllerTemperatureThreshold, SensorInfo.MediaTempShutdownThresh);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_CONTROLLER_TEMPERATURE,
    &ControllerTemperatureThreshold,
    NULL,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get %s alarm threshold Dimm handle 0x%x", CONTROLLER_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (SensorInfo.ContrTempShutdownThresh < ControllerTemperatureThreshold) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_CORE_TEMPERATURE_THRESHOLD_ERROR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32, ControllerTemperatureThreshold, SensorInfo.ContrTempShutdownThresh);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_SPARE_CAPACITY,
    &SpareBlockThreshold,
    NULL,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get %s alarm threshold Dimm handle 0x%x", SPARE_CAPACITY_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (SensorInfo.SpareCapacity < SpareBlockThreshold) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_SPARE_BLOCK_THRESHOLD_ERROR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32, SpareBlockThreshold, SensorInfo.SpareCapacity);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  time_t raw_start_time;
  time_t raw_end_time;
  PT_SYTEM_TIME_PAYLOAD SystemTimePayload;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;

  NVDIMM_ENTRY();

  if ((NULL == pDimm) || (NULL == pDiagState) || (NULL == ppResultStr)) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Get the start test system time
  time(&raw_start_time);
  // Get the DIMM's time
  ReturnCode = FwCmdGetSystemTime(pDimm, &SystemTimePayload);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get system time Dimm handle 0x%x", pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }
  // Get the end test system time
  time(&raw_end_time);

  // Validate resulats
  if ((time_t) SystemTimePayload.UnixTime < raw_start_time) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_SYSTEM_TIME_LOWER_ERROR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32, (raw_start_time - SystemTimePayload.UnixTime));
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }
  else if ((time_t) SystemTimePayload.UnixTime > raw_end_time) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_SYSTEM_TIME_GREATER_ERROR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32, (SystemTimePayload.UnixTime - raw_end_time));
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }
  else {
    NVDIMM_DBG("Dimm 0x%x time veryfication diagnostic test: success", pDimm->DeviceHandle.AsUint32);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Get the DIMM's debug log level and compare it to the default value.
Log proper events in case of any error.

@param[in] pDimm Pointer to the DIMM
@param[in out] ppResult Pointer to the result string of fw diagnostics message
@param[out] pDiagState Pointer to the quick diagnostics test state

@retval EFI_SUCCESS Test executed correctly
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
FwLogLevelCheck(
  IN     DIMM *pDimm,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT8 FwLogLevel = 0;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;

  NVDIMM_ENTRY();

  if ((NULL == pDimm) || (NULL == pDiagState) || (NULL == ppResultStr)) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Get the DIMM's FW log level
  ReturnCode = FwCmdGetFWDebugLevel(pDimm, &FwLogLevel);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_WARN("Failed to get FW Debug log level for Dimm handle 0x%x.", pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  // Validate resulats
  if (FwLogLevel != DEFAULT_FW_LOG_LEVEL_VALUE) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_LOG_LEVEL_ERROR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32, FwLogLevel, DEFAULT_FW_LOG_LEVEL_VALUE);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#endif // OS_BUILD
