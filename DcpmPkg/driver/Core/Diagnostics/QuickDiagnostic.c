/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "QuickDiagnostic.h"

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

#define MANAGEABILITY_TEST_INDEX 0
#define BOOTSTATUS_TEST_INDEX 1
#define SMARTHEALTH_TEST_INDEX 2

/**
  Run quick diagnostics for list of DIMMs, and appropriately
  populate the result messages, and test-state.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] pResult Pointer of structure with diagnostics test result

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_DEVICE_ERROR Test wasn't executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
RunQuickDiagnostics(
  IN     DIMM **ppDimms,
  IN     CONST UINT16 DimmCount,
  IN     UINT8 DimmIdPreference,
  OUT DIAG_INFO *pResult
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 DimmUid[MAX_DIMM_UID_LENGTH];
  UINT8 TmpDiagState = 0;
  UINT16 Index = 0;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(DimmUid, sizeof(DimmUid));

  if (pResult == NULL || DimmCount > MAX_DIMMS) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_ERR("The quick diagnostics test aborted due to an internal error.");
    goto Finish;
  }

  if (ppDimms == NULL || DimmCount == 0) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; ++Index) {
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

    pResult->SubTestName[MANAGEABILITY_TEST_INDEX] = CatSPrint(NULL, L"Manageability");
    ReturnCode = DiagnosticsManageabilityCheck(ppDimms[Index], DimmStr, &pResult->SubTestMessage[MANAGEABILITY_TEST_INDEX], &pResult->SubTestStateVal[MANAGEABILITY_TEST_INDEX]);
    if (EFI_ERROR(ReturnCode) || (!IsDimmManageable(ppDimms[Index]))) {
      NVDIMM_DBG("The check for manageability for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    pResult->SubTestName[BOOTSTATUS_TEST_INDEX] = CatSPrint(NULL, L"Boot status");
    ReturnCode = BootStatusDiagnosticsCheck(ppDimms[Index], DimmStr, &pResult->SubTestMessage[BOOTSTATUS_TEST_INDEX], &pResult->SubTestStateVal[BOOTSTATUS_TEST_INDEX]);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The BSR check for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((pResult->SubTestStateVal[BOOTSTATUS_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_QUICK_ABORTED_DIMM_INTERNAL_ERROR), EVENT_CODE_540, DIAG_STATE_MASK_ABORTED,
          &pResult->SubTestMessage[BOOTSTATUS_TEST_INDEX], &pResult->SubTestStateVal[BOOTSTATUS_TEST_INDEX], DimmStr);
      }
      continue;
    }

    pResult->SubTestName[SMARTHEALTH_TEST_INDEX] = CatSPrint(NULL, L"Health");
    ReturnCode = SmartAndHealthCheck(ppDimms[Index], DimmStr, &pResult->SubTestMessage[SMARTHEALTH_TEST_INDEX], &pResult->SubTestStateVal[SMARTHEALTH_TEST_INDEX]);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The smart and health check for DIMM ID 0x%x failed.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((TmpDiagState & DIAG_STATE_MASK_ABORTED) != 0) {
        APPEND_RESULT_TO_THE_LOG(ppDimms[Index], STRING_TOKEN(STR_QUICK_ABORTED_DIMM_INTERNAL_ERROR), EVENT_CODE_540, DIAG_STATE_MASK_ABORTED,
          &pResult->SubTestMessage[SMARTHEALTH_TEST_INDEX], &pResult->SubTestStateVal[SMARTHEALTH_TEST_INDEX], DimmStr);
      }
      continue;
    }
  }

  ReturnCode = EFI_SUCCESS;

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
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_UNMANAGEBALE_DIMM_SUBSYSTEM_VENDOR_ID), EVENT_CODE_501, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
        pDimmStr, EndianSwapUint16(pDimm->SubsystemVendorId));
    }

    if (!IsSubsystemDeviceIdSupported(pDimm)) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_UNMANAGEBALE_DIMM_SUBSYSTEM_DEVICE_ID), EVENT_CODE_502, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
        pDimmStr, EndianSwapUint16(pDimm->SubsystemDeviceId));
    }

    if (!IsFwApiVersionSupported(pDimm)) {
      ConvertFwApiVersion(TmpFwApiVerStr, pDimm->FwVer.FwApiMajor, pDimm->FwVer.FwApiMinor);
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_UNMANAGEBALE_DIMM_FW_API_VERSION), EVENT_CODE_503, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
        pDimmStr, TmpFwApiVerStr);
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
  SMART_AND_HEALTH_INFO HealthInfo;
  INT16 MediaTemperatureThreshold = 0;
  INT16 ControllerTemperatureThreshold = 0;
  INT16 PercentageRemainingThreshold = 0;
  UINT8 AlarmEnabled = 0;
  DIMM_INFO DimmInfo;
  CHAR16 *pActualHealthStr = NULL;
  CHAR16 *pActualHealthReasonStr = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&HealthInfo, sizeof(HealthInfo));
  ZeroMem(&DimmInfo, sizeof(DimmInfo));

  if (pDimm == NULL || pDimmStr == NULL || ppResultStr == NULL || pDiagState == NULL) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetSmartAndHealth(NULL, pDimm->DimmID, &HealthInfo);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_FW_BUSY), EVENT_CODE_541, DIAG_STATE_MASK_OK, ppResultStr, pDiagState,
        pDimm->DeviceHandle.AsUint32);
      goto Finish;
    }
    NVDIMM_DBG("Failed to Get SMART Info from Dimm 0x%x", pDimm->DeviceHandle.AsUint32);
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    goto Finish;
  }
  if (HealthInfo.LatchedLastShutdownStatus) {
    // LatchedLastShutdownStatus != 0 - Dirty Shutdown
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_DIRTY_SHUTDOWN), EVENT_CODE_530, DIAG_STATE_MASK_OK, ppResultStr, pDiagState,
      pDimm->DeviceHandle.AsUint32);
  }

  if (HealthInfo.HealthStatus != CONTROLLER_HEALTH_NORMAL) {
    if ((HealthInfo.HealthStatus & HealthStatusFatal) != 0) {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_FATAL_FAILURE), NULL);
    }
    else if ((HealthInfo.HealthStatus & HealthStatusCritical) != 0) {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CRITICAL_FAILURE), NULL);
    }
    else if ((HealthInfo.HealthStatus & HealthStatusNoncritical) != 0) {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_NON_CRITICAL_FAILURE), NULL);
    }
    else {
      pActualHealthStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_UNKNOWN), NULL);
    }

    if (HealthInfo.HealthStatusReason != HEALTH_STATUS_REASON_NONE) {
      ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmData->HiiHandle,
        HealthInfo.HealthStatusReason, &pActualHealthReasonStr);
      if (pActualHealthReasonStr == NULL || EFI_ERROR(ReturnCode)) {
        FREE_POOL_SAFE(pActualHealthStr);
        FREE_POOL_SAFE(pActualHealthReasonStr);
        NVDIMM_DBG("Error in converting health state reason to string");
        goto Finish;
      }

      pActualHealthStr = CatSPrintClean(pActualHealthStr, FORMAT_STR_WITH_PARANTHESIS, pActualHealthReasonStr);
    }
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BAD_HEALTH_STATE), EVENT_CODE_504, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimmStr, pActualHealthStr);

    FREE_POOL_SAFE(pActualHealthStr);
    FREE_POOL_SAFE(pActualHealthReasonStr);

  }
  else if ((pDimm->NvDimmStateFlags & BIT6) == BIT6) {
    // If BIT6 is set FW did not map a region to SPA on DIMM
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_ACPI_NVDIMM_SPA_NOT_MAPPED), EVENT_CODE_542, DIAG_STATE_MASK_OK, ppResultStr, pDiagState, pDimmStr);
  }

  ReturnCode = GetDimm(&gNvmDimmData->NvmDimmConfig, pDimm->DimmID,
    DIMM_INFO_CATEGORY_PACKAGE_SPARING |
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
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_FW_LOAD_FAILED), EVENT_CODE_536, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
      pDimmStr);
  }

  //Temperature and capacity checks
  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_MEDIA_TEMPERATURE,
    &MediaTemperatureThreshold,
    &AlarmEnabled,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_DBG("Failed to get %s alarm threshold DimmID 0x%x", MEDIA_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (FALSE != AlarmEnabled && HealthInfo.MediaTemperatureValid && HealthInfo.MediaTemperature > MediaTemperatureThreshold) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_MEDIA_TEMP_EXCEEDS_ALARM_THR), EVENT_CODE_505, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimmStr, HealthInfo.MediaTemperature, MediaTemperatureThreshold);
  }

  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_CONTROLLER_TEMPERATURE,
    &ControllerTemperatureThreshold,
    &AlarmEnabled,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_DBG("Failed to get %s alarm threshold DimmID 0x%x", CONTROLLER_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (FALSE != AlarmEnabled && HealthInfo.ControllerTemperatureValid && HealthInfo.ControllerTemperature > ControllerTemperatureThreshold) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_CONTROLLER_TEMP_EXCEEDS_ALARM_THR), EVENT_CODE_511, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimmStr, HealthInfo.ControllerTemperature, ControllerTemperatureThreshold);
  }

  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_PERCENTAGE_REMAINING,
    &PercentageRemainingThreshold,
    &AlarmEnabled,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_DBG("Failed to get %s alarm threshold DimmID 0x%x", SPARE_CAPACITY_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (FALSE != AlarmEnabled && HealthInfo.PercentageRemainingValid && HealthInfo.PercentageRemaining < PercentageRemainingThreshold) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_SPARE_CAPACITY_BELOW_ALARM_THR), EVENT_CODE_506, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimmStr, HealthInfo.PercentageRemaining, PercentageRemainingThreshold);
  }

  //Package spare availability check
  if ((DimmInfo.PackageSparingCapable == PACKAGE_SPARING_CAPABLE) && (DimmInfo.PackageSparesAvailable == PACKAGE_SPARES_NOT_AVAILABLE)) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_NO_PACKAGE_SPARES_AVAILABLE), EVENT_CODE_529, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimmStr);
  }

  //Viral state check
  if (DimmInfo.ViralStatus) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_VIRAL_STATE), EVENT_CODE_523, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState, pDimmStr);
  }

  //AIT DRAM disbaled check
  if (HealthInfo.AitDramEnabled == AIT_DRAM_DISABLED) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_AIT_DISABLED), EVENT_CODE_535, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState, pDimmStr);
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
  BOOLEAN FIS_GTE_1_14 = FALSE;
  BOOLEAN FIS_GTE_2_01 = FALSE;
  UINT8 DdrtTrainingStatus = DDRT_TRAINING_UNKNOWN;
  UINT16 BSRStatusBitmask = 0;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&Bsr, sizeof(Bsr));

  if (pDimm == NULL || pDimmStr == NULL || ppResultStr == NULL || pDiagState == NULL) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_DEVICE_ERROR;
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_WARN("Unable to access protocol.");
    goto Finish;
  }

  /* Check to make sure the FW Version is bigger than 1.14*/
  if ((pDimm->FwVer.FwApiMajor == 1 && pDimm->FwVer.FwApiMinor >= 14) || pDimm->FwVer.FwApiMajor > 1) {
    FIS_GTE_1_14 = TRUE;
  }

  /* Check to make sure the FW Version is bigger than 2.01*/
  if ((pDimm->FwVer.FwApiMajor == 2 && pDimm->FwVer.FwApiMinor >= 1) || pDimm->FwVer.FwApiMajor > 2) {
    FIS_GTE_2_01 = TRUE;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetBSRAndBootStatusBitMask(pNvmDimmConfigProtocol, pDimm->DimmID, &Bsr.AsUint64, &BSRStatusBitmask);

  if (EFI_ERROR(ReturnCode) || (BSRStatusBitmask & DIMM_BOOT_STATUS_UNKNOWN)) {
    ReturnCode = EFI_DEVICE_ERROR;
    NVDIMM_WARN("Unable to get the DIMMs BSR.");
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_NOT_READABLE), EVENT_CODE_513, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
      pDimmStr);
  }
  else {
    if (Bsr.Separated_Current_FIS.Major == DIMM_BSR_MAJOR_NO_POST_CODE) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_BIOS_POST_TRAINING_FAILED), EVENT_CODE_519, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    else if (Bsr.Separated_Current_FIS.Major == DIMM_BSR_MAJOR_CHECKPOINT_INIT_FAILURE) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_FW_NOT_INITIALIZED), EVENT_CODE_520, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr, Bsr.Separated_Current_FIS.Major, Bsr.Separated_Current_FIS.Minor);
    }
    else if (Bsr.Separated_Current_FIS.Major == DIMM_BSR_MAJOR_CHECKPOINT_CPU_EXCEPTION) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_CPU_EXCEPTION), EVENT_CODE_537, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr, Bsr.Separated_Current_FIS.Major, Bsr.Separated_Current_FIS.Minor);
    }
    if (Bsr.Separated_Current_FIS.DT == DIMM_BSR_DDRT_IO_INIT_NOT_STARTED) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_DDRT_IO_NOT_STARTED), EVENT_CODE_544, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    GetDdrtIoInitInfo(NULL, pDimm->DimmID, &DdrtTrainingStatus);
    if (DdrtTrainingStatus == DDRT_TRAINING_UNKNOWN) {
      NVDIMM_DBG("Could not retrieve DDRT training status");
    }
    if ((!FIS_GTE_2_01 && DdrtTrainingStatus != DDRT_TRAINING_COMPLETE && DdrtTrainingStatus != DDRT_S3_COMPLETE)
      || (FIS_GTE_2_01 && DdrtTrainingStatus != DDRT_TRAINING_COMPLETE && DdrtTrainingStatus != DDRT_S3_COMPLETE && DdrtTrainingStatus != NORMAL_MODE_COMPLETE)) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_DDRT_IO_NOT_COMPLETE), EVENT_CODE_538, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    if (Bsr.Separated_Current_FIS.MBR == DIMM_BSR_MAILBOX_NOT_READY) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_MAILBOX_NOT_READY), EVENT_CODE_539, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    if (Bsr.Separated_Current_FIS.DR != DIMM_BSR_AIT_DRAM_TRAINED_LOADED_READY) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_AIT_DRAM_NOT_READY), EVENT_CODE_533, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    if (FIS_GTE_1_14) {
      if ((Bsr.Separated_Current_FIS.DTS == DDRT_TRAINING_NOT_COMPLETE) ||
        (Bsr.Separated_Current_FIS.DTS == DDRT_TRAINING_FAILURE)) {
        APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_DDRT_TRAINING_NOT_COMPLETE_FAILED), EVENT_CODE_543, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
          pDimmStr);
      }
    }
    if (Bsr.Separated_Current_FIS.MR == DIMM_BSR_MEDIA_NOT_TRAINED) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_MEDIA_NOT_READY), EVENT_CODE_514, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    if (Bsr.Separated_Current_FIS.MR == DIMM_BSR_MEDIA_ERROR) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_MEDIA_ERROR), EVENT_CODE_515, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState, pDimmStr);
    }
    if (Bsr.Separated_Current_FIS.MD == DIMM_BSR_MEDIA_DISABLED) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_MEDIA_DISABLED), EVENT_CODE_534, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
    if (Bsr.Separated_Current_FIS.RR == DIMM_BSR_REBOOT_REQUIRED) {
      APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_QUICK_BSR_REBOOT_REQUIRED), EVENT_CODE_507, DIAG_STATE_MASK_FAILED, ppResultStr, pDiagState,
        pDimmStr);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

