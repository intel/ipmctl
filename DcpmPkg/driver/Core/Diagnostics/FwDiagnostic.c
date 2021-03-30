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

#define FW_CONSIST_TEST_INDEX 0
#define VIRAL_POLICY_CONSIST_TEST_INDEX 1
#define THRESHOLD_TEST_INDEX 2
#define SYS_TIME_TEST_INDEX 3

/**
  Run Fw diagnostics for the list of DIMMs, and appropriately
  populate the result in diagnostic structure.

  @param[in] ppDimms The DIMM pointers list
  @param[in] DimmCount DIMMs count
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] pResult Pointer of structure with diagnostics test result

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
  OUT DIAG_INFO *pResult
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 Index = 0;

  NVDIMM_ENTRY();

  if (pResult == NULL || DimmCount > MAX_DIMMS) {
    NVDIMM_DBG("The firmware consistency and settings diagnostics test aborted due to an internal error.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmCount == 0 || ppDimms == NULL || GetManageableDimmsCount(ppDimms, DimmCount) == 0) {
    ReturnCode = EFI_SUCCESS;
    APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_NO_MANAGEABLE_DIMMS), EVENT_CODE_901, DIAG_STATE_MASK_OK, &pResult->Message, &pResult->StateVal);
    goto Finish;
  }

  pResult->SubTestName[FW_CONSIST_TEST_INDEX] = CatSPrint(NULL, L"FW Consistency");
  ReturnCode = CheckFwConsistency(ppDimms, DimmCount, DimmIdPreference, &pResult->SubTestMessage[FW_CONSIST_TEST_INDEX], &pResult->SubTestStateVal[FW_CONSIST_TEST_INDEX]);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for firmware consistency failed.");
    if ((pResult->SubTestStateVal[FW_CONSIST_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_ABORTED_INTERNAL_ERROR), EVENT_CODE_910, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[FW_CONSIST_TEST_INDEX], &pResult->SubTestStateVal[FW_CONSIST_TEST_INDEX]);
      goto Finish;
    }
  }

  pResult->SubTestName[VIRAL_POLICY_CONSIST_TEST_INDEX] = CatSPrint(NULL, L"Viral Policy");
  ReturnCode = CheckViralPolicyConsistency(ppDimms, DimmCount, &pResult->SubTestMessage[VIRAL_POLICY_CONSIST_TEST_INDEX], &pResult->SubTestStateVal[VIRAL_POLICY_CONSIST_TEST_INDEX]);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("The check for viral policy settings consistency failed");
    if ((pResult->SubTestStateVal[VIRAL_POLICY_CONSIST_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_ABORTED_INTERNAL_ERROR), EVENT_CODE_910, DIAG_STATE_MASK_ABORTED,
        &pResult->SubTestMessage[VIRAL_POLICY_CONSIST_TEST_INDEX], &pResult->SubTestStateVal[VIRAL_POLICY_CONSIST_TEST_INDEX]);
      goto Finish;
    }
  }

  pResult->SubTestName[THRESHOLD_TEST_INDEX] = CatSPrint(NULL, L"Threshold check");
  pResult->SubTestName[SYS_TIME_TEST_INDEX] = CatSPrint(NULL, L"System Time");
  for (Index = 0; Index < DimmCount; Index++) {
    if (ppDimms[Index] == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      pResult->SubTestStateVal[THRESHOLD_TEST_INDEX] |= DIAG_STATE_MASK_ABORTED;
      goto Finish;
    }

    if (!IsDimmManageable(ppDimms[Index]))
    {
      continue;
    }

    ReturnCode = ThresholdsCheck(ppDimms[Index], &pResult->SubTestMessage[THRESHOLD_TEST_INDEX], &pResult->SubTestStateVal[THRESHOLD_TEST_INDEX]);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("The check for firmware threshold settings failed. Dimm handle 0x%04x.", ppDimms[Index]->DeviceHandle.AsUint32);
      if ((pResult->SubTestStateVal[THRESHOLD_TEST_INDEX] & DIAG_STATE_MASK_ABORTED) != 0) {
        APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_ABORTED_INTERNAL_ERROR), EVENT_CODE_910, DIAG_STATE_MASK_ABORTED,
          &pResult->SubTestMessage[THRESHOLD_TEST_INDEX], &pResult->SubTestStateVal[THRESHOLD_TEST_INDEX]);
        goto Finish;
      }
    }
  }

  ReturnCode = EFI_SUCCESS;
  goto Finish;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
    NVDIMM_DBG("The functionality to populate subsystem device Id list failed.");
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
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_INCONSISTENT), EVENT_CODE_902, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState, pAppendedDimmsStr, SubsystemDeviceIdList[Index], OptimumFwVersionStr);
      FREE_POOL_SAFE(pAppendedDimmsStr);
    }
  }

Finish:
  FREE_POOL_SAFE(pAppendedDimmsStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO *pDimms = NULL;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINTN Index = 0;
  UINT8 ViralPolicyState = 0;

  NVDIMM_ENTRY();

  if (DimmCount == 0 || ppDimms == NULL || DimmCount > MAX_DIMMS ||
    ppResultStr == NULL || pDiagState == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
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

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
  if (pDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    NVDIMM_ERR("Could not allocate memory");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
    DIMM_INFO_CATEGORY_VIRAL_POLICY, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  for (Index = 0; Index < DimmCount; Index++) {
    /** ViralPolicyState equals to state of the first DIMM, rest of DIMMs must be in the same state **/
    if (Index == 0) {
      ViralPolicyState = pDimms[0].ViralPolicyEnable;
    }
    if (pDimms[Index].ViralPolicyEnable != ViralPolicyState) {
      APPEND_RESULT_TO_THE_LOG(NULL, STRING_TOKEN(STR_FW_INCONSISTENT_VIRAL_POLICY), EVENT_CODE_906, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState);
      goto Finish;
    }
  }

Finish:
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  SMART_AND_HEALTH_INFO HealthInfo;
  UINT8 AlarmEnabled = 0;
  INT16 MediaTemperatureThreshold = 0;
  INT16 ControllerTemperatureThreshold = 0;
  INT16 PercentageRemainingThreshold = 0;

  NVDIMM_ENTRY();

  ZeroMem(&HealthInfo, sizeof(HealthInfo));

  if ((NULL == pDimm) || (NULL == pDiagState) || (NULL == ppResultStr)) {
    if (pDiagState != NULL) {
      *pDiagState |= DIAG_STATE_MASK_ABORTED;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetSmartAndHealth(NULL, pDimm->DimmID, &HealthInfo);
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
    &AlarmEnabled,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get %s alarm threshold Dimm handle 0x%x", MEDIA_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (FALSE != AlarmEnabled && HealthInfo.MediaTempShutdownThresh < MediaTemperatureThreshold) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_FW_MEDIA_TEMPERATURE_THRESHOLD_ERROR), EVENT_CODE_903, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimm->DeviceHandle.AsUint32, MediaTemperatureThreshold, HealthInfo.MediaTempShutdownThresh);
  }

  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_CONTROLLER_TEMPERATURE,
    &ControllerTemperatureThreshold,
    &AlarmEnabled,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get %s alarm threshold Dimm handle 0x%x", CONTROLLER_TEMPERATURE_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (FALSE != AlarmEnabled && HealthInfo.ContrTempShutdownThresh < ControllerTemperatureThreshold) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_FW_CONTROLLER_TEMPERATURE_THRESHOLD_ERROR), EVENT_CODE_904, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimm->DeviceHandle.AsUint32, ControllerTemperatureThreshold, HealthInfo.ContrTempShutdownThresh);
  }

  ReturnCode = GetAlarmThresholds(NULL,
    pDimm->DimmID,
    SENSOR_TYPE_PERCENTAGE_REMAINING,
    &PercentageRemainingThreshold,
    &AlarmEnabled,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    *pDiagState |= DIAG_STATE_MASK_ABORTED;
    NVDIMM_ERR("Failed to get %s alarm threshold Dimm handle 0x%x", SPARE_CAPACITY_STR, pDimm->DeviceHandle.AsUint32);
    goto Finish;
  }

  if (FALSE != AlarmEnabled && HealthInfo.PercentageRemainingValid && HealthInfo.PercentageRemaining < PercentageRemainingThreshold) {
    APPEND_RESULT_TO_THE_LOG(pDimm, STRING_TOKEN(STR_FW_SPARE_BLOCK_THRESHOLD_ERROR), EVENT_CODE_905, DIAG_STATE_MASK_WARNING, ppResultStr, pDiagState,
      pDimm->DeviceHandle.AsUint32, HealthInfo.PercentageRemaining, PercentageRemainingThreshold);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
