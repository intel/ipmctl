/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "QuickDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#ifdef OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) SendTheEventAndAppendToDiagnosticsResult(pDimm,pStr,StateMask,__COUNTER__,SYSTEM_EVENT_CAT_QUICK,ppResult,pState)
#else // OS_BUILD
#define APPEND_RESULT_TO_THE_LOG(pDimm,pStr,StateMask,ppResult,pState) AppendToDiagnosticsResult(pStr,StateMask,ppResult,pState)
#endif // OS_BUILD

/**
  Run quick diagnostics for list of DIMMs, and appropriately
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
  UINT8 TmpDiagState = 0;
  UINT16 Index = 0;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(DimmUid, sizeof(DimmUid));

 if (ppResult == NULL || pDiagState == NULL || DimmCount > MAX_DIMMS) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("The quick diagnostics test aborted due to an internal error.");
    goto Finish;
  }

  if (ppDimms == NULL || DimmCount == 0) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NO_MANAGEABLE_DIMMS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
    goto Finish;
  }

  if (*ppResult != NULL) {
    NVDIMM_DBG("The passed result string for quick diagnostics tests is not empty");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  for (Index = 0; Index < DimmCount; ++Index) {
    *pDiagState |= TmpDiagState;
    TmpDiagState = 0;

    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      continue;
    }

    ReturnCode = GetDimmUid(ppDimms[Index], DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetDimmUid function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = GetPreferredValueAsString(ppDimms[Index]->DeviceHandle.AsUint32, DimmUid, DimmIdPreference == DISPLAY_DIMM_ID_HANDLE,
       DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetPreferredValueAsString function for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = DiagnosticsManageabilityCheck(ppDimms[Index], DimmStr, ppResult, &TmpDiagState);
    if (EFI_ERROR(ReturnCode) || (!IsDimmManageable(ppDimms[Index]))) {
      NVDIMM_DBG("The check for manageability for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    ReturnCode = BootStatusDiagnosticsCheck(ppDimms[Index], DimmStr, ppResult, &TmpDiagState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The BSR check for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((TmpDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
        pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_QUICK_ABORTED_DIMM_INTERNAL_ERROR), NULL);
        pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr);
        FREE_POOL_SAFE(pTmpStr);
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_ABORTED, ppResult, &TmpDiagState);
      }
      continue;
    }

    ReturnCode = SmartAndHealthCheck(ppDimms[Index], DimmStr, ppResult, &TmpDiagState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The smart and health check for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((TmpDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
        pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_QUICK_ABORTED_DIMM_INTERNAL_ERROR), NULL);
        pTmpStr1 = CatSPrint(NULL, pTmpStr, DimmStr);
        FREE_POOL_SAFE(pTmpStr);
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], pTmpStr1, DIAG_STATE_MASK_ABORTED, ppResult, &TmpDiagState);
      }
      continue;
    }
  }

  //Updating the overall test-state with the last dimm test-state
  *pDiagState |= TmpDiagState;

  if ((*pDiagState & DIAG_STATE_MASK_ALL) <= DIAG_STATE_MASK_OK) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_QUICK_SUCCESS), NULL);
    APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_OK, ppResult, pDiagState);
  }
  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishError:
  pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_QUICK_ABORTED_INTERNAL_ERROR), NULL);
  APPEND_RESULT_TO_THE_LOG(NULL, pTmpStr, DIAG_STATE_MASK_ABORTED, ppResult, pDiagState);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
  CHAR16 TmpFwApiVerStr[FW_API_VERSION_LEN];

  NVDIMM_ENTRY();

  if (pDimm == NULL || pDimmStr == NULL || ppResultStr == NULL || pDiagState == NULL) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    if (SPD_INTEL_VENDOR_ID != pDimm->SubsystemVendorId) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_UNMANAGEBALE_DIMM_SUBSYSTEM_VENDOR_ID), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, EndianSwapUint16(pDimm->SubsystemVendorId));
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_ABORTED, ppResultStr, pDiagState);
    }

    if (!IsSubsystemDeviceIdSupported(pDimm)) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_UNMANAGEBALE_DIMM_SUBSYSTEM_DEVICE_ID), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, EndianSwapUint16(pDimm->SubsystemDeviceId));
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_ABORTED, ppResultStr, pDiagState);
    }

    if (!IsFwApiVersionSupported(pDimm)) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_UNMANAGEBALE_DIMM_FW_API_VERSION), NULL);
      ConvertFwApiVersion(TmpFwApiVerStr, pDimm->FwVer.FwApiMajor, pDimm->FwVer.FwApiMinor);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, TmpFwApiVerStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_ABORTED, ppResultStr, pDiagState);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  SENSOR_INFO SensorInfo;
  INT16 MediaTemperatureThreshold = 0;
  INT16 ControllerTemperatureThreshold = 0;
  INT16 SpareBlockThreshold = 0;
  UINT8 AitDramEnabled = 0;
  BOOLEAN FIS_1_3 = FALSE;
  DIMM_INFO DimmInfo;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
  CHAR16 *pActualHealthStr = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&SensorInfo, sizeof(SensorInfo));
  ZeroMem(&DimmInfo, sizeof(DimmInfo));

  if (pDimm == NULL || pDimmStr == NULL || ppResultStr == NULL || pDiagState == NULL) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
     ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetSmartAndHealth(NULL, pDimm->DimmID, &SensorInfo, NULL, NULL, &AitDramEnabled);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to Get SMART Info from Dimm 0x%x", pDimm->DeviceHandle.AsUint32);
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  }
  if (SensorInfo.LastShutdownStatus) {
    // LastShutdownStatus != 0 - Dirty Shutdown
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONFIG_UNSAFE_SHUTDOWN), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimm->DeviceHandle.AsUint32);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_OK, ppResultStr, pDiagState);
  }

  if (SensorInfo.HealthStatus != CONTROLLER_HEALTH_NORMAL) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BAD_HEALTH_STATE), NULL);

    if ((SensorInfo.HealthStatus & ControllerHealthStatusFatal) != 0) {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_NVMDIMM_HEALTH_FATAL_FAILURE), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, pActualHealthStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
    } else if ((SensorInfo.HealthStatus & ControllerHealthStatusCritical) != 0) {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_NVMDIMM_HEALTH_CRITICAL_FAILURE), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, pActualHealthStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
    } else if ((SensorInfo.HealthStatus & ControllerHealthStatusNoncritical) != 0) {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_NVMDIMM_HEALTH_NON_CRITICAL_FAILURE), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, pActualHealthStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
    } else {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_NVMDIMM_HEALTH_UNKNOWN), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, pActualHealthStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
    }
    FREE_POOL_SAFE(pActualHealthStr);
  }

  ReturnCode = GetDimm(&gNvmDimmData->NvmDimmConfig, pDimm->DimmID,
      DIMM_INFO_CATEGORY_DIE_SPARING |
      DIMM_INFO_CATEGORY_OPTIONAL_CONFIG_DATA_POLICY |
      DIMM_INFO_CATEGORY_FW_IMAGE_INFO,
      &DimmInfo);

  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_DBG("Failed to get DIMM info for DimmID 0x%x", pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  //Last Fw Update Status
  if (DimmInfo.LastFwUpdateStatus == FW_UPDATE_STATUS_FAILED) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_FW_LOAD_FAILED), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
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
    NVDIMM_DBG("Failed to get %s alarm threshold DimmID 0x%x", MEDIA_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (SensorInfo.MediaTemperature > MediaTemperatureThreshold) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_MEDIA_TEMP_EXCEEDS_ALARM_THR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, SensorInfo.MediaTemperature, MediaTemperatureThreshold);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

  ReturnCode = GetAlarmThresholds(NULL,
      pDimm->DimmID,
      SENSOR_TYPE_CONTROLLER_TEMPERATURE,
      &ControllerTemperatureThreshold,
      NULL,
      NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_DBG("Failed to get %s alarm threshold DimmID 0x%x", CONTROLLER_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (SensorInfo.ControllerTemperature > ControllerTemperatureThreshold) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_CONTROLLER_TEMP_EXCEEDS_ALARM_THR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, SensorInfo.ControllerTemperature, ControllerTemperatureThreshold);
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
    NVDIMM_DBG("Failed to get %s alarm threshold DimmID 0x%x", SPARE_CAPACITY_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (SensorInfo.SpareCapacity < SpareBlockThreshold) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_SPARE_CAPACITY_BELOW_ALARM_THR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, SensorInfo.SpareCapacity, SpareBlockThreshold);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

  if ((SensorInfo.PercentageUsedValid) && (SensorInfo.PercentageUsed > EMULATOR_DIMM_PERCENTAGE_USED_THR)) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_PERCENTAGE_USED_EXCEEDS_THR), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr, SensorInfo.PercentageUsed, EMULATOR_DIMM_PERCENTAGE_USED_THR);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

  //Die spare availability check
  if ((DimmInfo.DieSparingCapable == DIE_SPARING_CAPABLE) && (DimmInfo.DieSparesAvailable == DIE_SPARES_NOT_AVAILABLE)) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_NO_SPARE_DIE_AVAILABLE), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
  }

  //Viral state check
  if (DimmInfo.ViralStatus) {
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_VIRAL_STATE), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
  }

  if (pDimm->FwVer.FwApiMajor == 1 && pDimm->FwVer.FwApiMinor <= 3) {
    FIS_1_3 = TRUE;
  }

  //AIT DRAM disbaled check
  if (!FIS_1_3) {
    if (AitDramEnabled == AIT_DRAM_DISABLED) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_AIT_DRAM_DISABLED), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_BSR Bsr;
  CHAR16 *pTmpStr = NULL;
  CHAR16 *pTmpStr1 = NULL;
  BOOLEAN FIS_1_4 = FALSE;
  UINT8 DdrtTrainingStatus = DDRT_TRAINING_UNKNOWN;

  NVDIMM_ENTRY();

  ZeroMem(&Bsr, sizeof(Bsr));

  if (pDimm == NULL || pDimmStr == NULL || ppResultStr == NULL || pDiagState == NULL) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pDimm->pHostMailbox == NULL || pDimm->pHostMailbox->pBsr == NULL) {
    ReturnCode = EFI_DEVICE_ERROR;
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  }

  if (pDimm->FwVer.FwApiMajor == 1 && pDimm->FwVer.FwApiMinor <= 4) {
    FIS_1_4 = TRUE;
  }

  //CopyMem(&Bsr.AsUint64, (VOID *) pDimm->pHostMailbox->pBsr, sizeof(Bsr));
  Bsr.AsUint64 = BSR(pDimm);
  if ((Bsr.AsUint64 == MAX_UINT64_VALUE) || (Bsr.AsUint64 == 0)) {
    ReturnCode = EFI_DEVICE_ERROR;
    NVDIMM_WARN("Unable to get the DIMMs BSR.");
    pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_NOT_READABLE), NULL);
    pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
    FREE_POOL_SAFE(pTmpStr);
    APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
  } else {

    if (Bsr.Separated_Current_FIS.Major == DIMM_BSR_MAJOR_NO_POST_CODE) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_BIOS_POST_TRAINING_FAILED), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    } else if (Bsr.Separated_Current_FIS.Major == DIMM_BSR_MAJOR_CHECKPOINT_INIT_FAILURE) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_FW_NOT_INITIALIZED), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    } else if (Bsr.Separated_Current_FIS.Major == DIMM_BSR_MAJOR_CHECKPOINT_CPU_EXCEPTION) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_CPU_EXCEPTION), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }

    GetDdrtIoInitInfo(NULL, pDimm->DimmID, &DdrtTrainingStatus);
    if (DdrtTrainingStatus == DDRT_TRAINING_UNKNOWN) {
      NVDIMM_DBG("Could not retrieve DDRT training status");
    }
    if (DdrtTrainingStatus != DDRT_TRAINING_COMPLETE) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_DDRT_IO_NOT_COMPLETE), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if (Bsr.Separated_Current_FIS.MBR == DIMM_BSR_MAILBOX_NOT_READY) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_MAILBOX_NOT_READY), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if (Bsr.Separated_Current_FIS.Assertion == DIMM_BSR_FW_ASSERT) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_FW_ASSERT), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if (Bsr.Separated_Current_FIS.MI_Stalled == DIMM_BSR_MEDIA_INTERFACE_ENGINE_STALLED) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_MEDIA_ENGINE_STALLED), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if ((FIS_1_4 && (Bsr.Separated_FIS_1_4.DR != DIMM_BSR_AIT_DRAM_READY)) ||
        (!FIS_1_4 && (Bsr.Separated_Current_FIS.DR != DIMM_BSR_AIT_DRAM_TRAINED_LOADED_READY))) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_AIT_DRAM_NOT_READY), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if (Bsr.Separated_Current_FIS.MR == DIMM_BSR_MEDIA_NOT_TRAINED) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_MEDIA_NOT_READY), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if (Bsr.Separated_Current_FIS.MR == DIMM_BSR_MEDIA_ERROR) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_MEDIA_ERROR), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
    if (Bsr.Separated_Current_FIS.MD == DIMM_BSR_MEDIA_DISABLED) {
      pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_DIAGNOSTIC_BSR_MEDIA_DISABLED), NULL);
      pTmpStr1 = CatSPrint(NULL, pTmpStr, pDimmStr);
      FREE_POOL_SAFE(pTmpStr);
      APPEND_RESULT_TO_THE_LOG(pDimm, pTmpStr1, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

