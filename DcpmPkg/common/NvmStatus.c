/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Types.h>
#include <Utility.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HiiLib.h>
#include "NvmTypes.h"
#include <Debug.h>
#include "NvmStatus.h"
#include "Convert.h"

OBJECT_STATUS gAllErrorNvmStatuses;
OBJECT_STATUS gAllWarningNvmStatuses;

/**
  Translate ObjectType from CLI command into Unicode string representing its brief description.

  @param[in] HiiHandle handle to the HII database that contains NvmStatusStrings
  @param[in] ObjectType the status code returned from a NVM command.

  @retval Pointer to a decoded string. Memory is dynamically allocated. It should be freed by caller.
**/

STATIC
CHAR16 *
GetObjectTypeString(
  IN     EFI_HANDLE HiiHandle,
  IN     UINT8 ObjectType
  )
{
  CHAR16 *pObjectTypeString = NULL;

  switch (ObjectType) {
  case ObjectTypeSocket:
    pObjectTypeString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_SOCKET), NULL);
    break;
  case ObjectTypeDimm:
    pObjectTypeString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_PMM), NULL);
    break;
  case ObjectTypeNamespace:
    pObjectTypeString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_NAMESPACE), NULL);
    break;
  case ObjectTypeRegion:
    pObjectTypeString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_REGION), NULL);
    break;
  default:
    pObjectTypeString = CatSPrint(NULL, L"");
    break;
  }
  return pObjectTypeString;
}


/**
  Create command status as with specified command message.
  Function displays per DIMM status if such exists and
  summarizing status for whole command. Memory allocated
  for status message and command status is freed after
  status is displayed.

  @param[in] HiiHandle handle to the HII database that contains NvmStatusStrings
  @param[in] pStatusMessage String with command information
  @param[in] pStatusPreposition String with preposition
  @param[in] pCommandStatus Command status data
  @param[in] ObjectIdNumberPreferred Use Object ID number if true, use Object ID string otherwise
  @param[out] ppOutputMessage buffer where output will be saved

  Warning: ppOutputMessage - should be freed in caller.

  @retval EFI_INVALID_PARAMETER pCommandStatus is NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
CreateCommandStatusString(
  IN     EFI_HANDLE HiiHandle,
  IN     CONST CHAR16 *pStatusMessage,
  IN     CONST CHAR16 *pStatusPreposition,
  IN     COMMAND_STATUS *pCommandStatus,
  IN     BOOLEAN ObjectIdNumberPreferred,
     OUT CHAR16 **ppOutputMessage
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pObjectStatusNode = NULL;
  OBJECT_STATUS *pObjectStatus = NULL;
  CHAR16 *pObjectTypeString = NULL;
  CHAR16 *pSingleStatusCodeMessage = NULL;
  CHAR16 *pAllStatusCodeMessages = NULL;
  CHAR16 *pPrefixString = NULL;
  CHAR16 *pCurrentString = NULL;
  CHAR16 ObjectStr[MAX_OBJECT_ID_STR_LEN];
  CHAR16 *pFailedString = NULL;
  CHAR16 *pErrorString = NULL;
  CHAR16 *pExecuteSuccessString = NULL;

  ZeroMem(ObjectStr, sizeof(ObjectStr));

  NVDIMM_ENTRY();

  if (pCommandStatus == NULL || ppOutputMessage == NULL) {
    goto Finish;
  }

  pObjectTypeString = GetObjectTypeString(HiiHandle, pCommandStatus->ObjectType);

  if (pCommandStatus->ObjectStatusCount == 0) {
    if (!NVM_ERROR(pCommandStatus->GeneralStatus)) {
      pExecuteSuccessString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_EXECUTE_SUCCESS), NULL);
      pCurrentString = CatSPrint(pCurrentString, FORMAT_STR_SPACE FORMAT_STR_NL,
        pStatusMessage,
        pExecuteSuccessString);
      FREE_POOL_SAFE(pExecuteSuccessString);
    } else {
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(HiiHandle, pCommandStatus->GeneralStatus);
      if ((pCommandStatus->GeneralStatus == NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND) ||
        (pCommandStatus->GeneralStatus == NVM_ERR_DIMM_NOT_FOUND)) {
        pCurrentString = CatSPrint(pCurrentString, FORMAT_STR_NL, pSingleStatusCodeMessage);
      } else {
        pFailedString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_FAILED), NULL);
        pErrorString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERROR), NULL);
        pCurrentString = CatSPrint(pCurrentString, FORMAT_STR_SPACE FORMAT_STR L": " FORMAT_STR L" (%d) - " FORMAT_STR_NL,
          pStatusMessage,
          pFailedString,
          pErrorString,
          pCommandStatus->GeneralStatus,
          pSingleStatusCodeMessage);
        FREE_POOL_SAFE(pFailedString);
        FREE_POOL_SAFE(pErrorString);
      }
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
    }
  } else {
    LIST_FOR_EACH(pObjectStatusNode, &pCommandStatus->ObjectStatusList) {
      pObjectStatus = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);

      ReturnCode = GetPreferredValueAsString(
          pObjectStatus->ObjectId,
          (pObjectStatus->IsObjectIdStr) ? pObjectStatus->ObjectIdStr : NULL,
          ObjectIdNumberPreferred,
          ObjectStr,
          MAX_OBJECT_ID_STR_LEN
          );
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      pPrefixString = CatSPrint(NULL, FORMAT_STR FORMAT_STR FORMAT_STR FORMAT_STR L": ",
          pStatusMessage,
          pStatusPreposition,
          pObjectTypeString,
          ObjectStr);

      pAllStatusCodeMessages = GetAllNvmStatusCodeMessages(HiiHandle, pObjectStatus, pPrefixString);
      pCurrentString = CatSPrintClean(pCurrentString, FORMAT_STR, pAllStatusCodeMessages);
    }
  }

  *ppOutputMessage = pCurrentString;
  ReturnCode = EFI_SUCCESS;
Finish:
  FREE_POOL_SAFE(pObjectTypeString);
  FREE_POOL_SAFE(pAllStatusCodeMessages);
  FREE_POOL_SAFE(pPrefixString);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Fill global variables containing all Error/Warning NVM Statuses
**/
VOID
InitErrorAndWarningNvmStatusCodes()
{
  NVDIMM_ENTRY();
  // Warnings:
  SetNvmStatus(&gAllWarningNvmStatuses, NVM_WARN_BLOCK_MODE_DISABLED);
  SetNvmStatus(&gAllWarningNvmStatuses, NVM_WARN_SMART_NONCRITICAL_HEALTH_ISSUE);
  SetNvmStatus(&gAllWarningNvmStatuses, NVM_WARN_2LM_MODE_OFF);
  SetNvmStatus(&gAllWarningNvmStatuses, NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU);
  SetNvmStatus(&gAllWarningNvmStatuses, NVM_WARN_IMC_DDR_PMM_NOT_PAIRED);

  // Errors:
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_OPERATION_NOT_STARTED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_OPERATION_FAILED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_INVALID_PARAMETER);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_CONFIG_NOT_SUPPORTED_BY_CURRENT_SKU);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_DIMM_NOT_FOUND);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_DIMM_ID_DUPLICATED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SOCKET_ID_NOT_VALID);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SOCKET_ID_DUPLICATED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_PASSPHRASES_DO_NOT_MATCH);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_PASSPHRASE_TOO_LONG);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_ENABLE_SECURITY_NOT_ALLOWED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_INVALID_SECURITY_STATE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_INVALID_SECURITY_OPERATION);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_INCONSISTENT_SECURITY_STATE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_INVALID_PASSPHRASE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SECURITY_COUNT_EXPIRED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FILENAME_NOT_PROVIDED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_IMAGE_EXAMINE_INVALID);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_IMAGE_FILE_NOT_VALID);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SENSOR_NOT_VALID);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_MEDIA_DISABLED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_CONF_APPLYING_FAILED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_NOT_FOUND);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_DUMP_NO_CONFIGURED_DIMMS);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_DUMP_FILE_OPERATION_FAILED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_LOAD_VERSION);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_LOAD_INVALID_DATA_IN_FILE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_LOAD_IMPROPER_CONFIG_IN_FILE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_LOAD_DIMM_COUNT_MISMATCH);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_UNSUPPORTED_BLOCK_SIZE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_INVALID_NAMESPACE_CAPACITY);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NONE_DIMM_FULFILLS_CRITERIA);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NOT_ENOUGH_FREE_SPACE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NOT_ENOUGH_FREE_SPACE_BTT);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FAILED_TO_UPDATE_BTT);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_BADALIGNMENT);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SMBIOS_DIMM_ENTRY_NOT_FOUND_IN_NFIT);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NAMESPACE_DOES_NOT_EXIST);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NAMESPACE_COULD_NOT_UNINSTALL);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NAMESPACE_COULD_NOT_INSTALL);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NAMESPACE_READ_ONLY);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_RENAME_NAMESPACE_NOT_SUPPORTED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FAILED_TO_INIT_NS_LABELS);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FAILED_TO_FETCH_ERROR_LOG);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FAILED_TO_GET_DIMM_INFO);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SMART_CRITICAL_HEALTH_ISSUE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SMART_FATAL_HEALTH_ISSUE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_SMART_UNKNOWN_HEALTH_ISSUE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_NOT_HEALTHY);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_BLOCK_NAMESPACE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_PM_NAMESPACE);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_GOAL_NO_EXISTS_ON_DIMM);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_GOAL_NAMESPACE_EXISTS);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_GOAL_AUTO_PROV_ENABLED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_REGION_REMAINING_SIZE_NOT_IN_LAST_PROPERTY);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS);

  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_IMAGE_FILE_NOT_COMPATIBLE_TO_CTLR_STEPPING);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FIRMWARE_API_NOT_VALID);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FIRMWARE_VERSION_NOT_VALID);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_NAMESPACE_TOO_SMALL_FOR_BTT);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_DIMM_SKU_MODE_MISMATCH);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_DIMM_SKU_SECURITY_MISMATCH);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_GET_PCD_FAILED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FIRMWARE_ALREADY_LOADED);
  SetNvmStatus(&gAllErrorNvmStatuses, NVM_ERR_FIRMWARE_FAILED_TO_STAGE);

  NVDIMM_EXIT();
}

/**
  Translate NVM operation return code into
  Unicode string representing its brief description.

  @param[in] EFI HANDLE the HiiHandle to the HII database that contains NvmStatus strings
  @param[in] NvmStatusCode the status code returned from
    a NVM command.

  @retval Pointer to a decoded string. Memory is dynamically allocated. It should be freed by caller.
**/
CHAR16 *
GetSingleNvmStatusCodeMessage(
  IN     EFI_HANDLE    HiiHandle,
  IN     NvmStatusCode NvmStatusCodeVar
  )
{
  CHAR16 *pTempString = NULL;
  CHAR16 *pTempString1 = NULL;

  switch (NvmStatusCodeVar) {

  case NVM_SUCCESS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_SUCCESS), NULL);
  case NVM_SUCCESS_FW_RESET_REQUIRED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_SUCCESS_FW_RESET_REQUIRED), NULL);
  case NVM_ERR_OPERATION_NOT_STARTED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_OPERATION_NOT_STARTED), NULL);
  case NVM_ERR_OPERATION_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_OPERATION_FAILED), NULL);
  case NVM_ERR_INVALIDPARAMETER:
  case NVM_ERR_INVALID_PARAMETER:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_PARAMETER), NULL);
  case NVM_ERR_FORCE_REQUIRED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FORCE_REQUIRED), NULL);
  case NVM_ERR_DIMM_NOT_FOUND:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DIMM_NOT_FOUND), NULL);
  case NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_MANAGEABLE_DIMM_NOT_FOUND), NULL);
  case NVM_ERR_FIRMWARE_API_NOT_VALID:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FIRMWARE_API_NOT_VALID), NULL);
  case NVM_ERR_DIMM_ID_DUPLICATED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DIMM_ID_DUPLICATED), NULL);
  case NVM_ERR_SOCKET_ID_NOT_VALID:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SOCKET_ID_NOT_VALID), NULL);
  case NVM_ERR_SOCKET_ID_DUPLICATED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SOCKET_ID_DUPLICATED), NULL);
  case NVM_ERR_INVALID_PASSPHRASE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_PASSPHRASE), NULL);
  case NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU), NULL);
  case NVM_ERR_CONFIG_NOT_SUPPORTED_BY_CURRENT_SKU:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_CONFIG_NOT_SUPPORTED_BY_CURRENT_SKU), NULL);
  case NVM_ERR_SECURITY_COUNT_EXPIRED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SECURITY_COUNT_EXPIRED), NULL);
  case NVM_ERR_RECOVERY_ACCESS_NOT_ENABLED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_RECOVERY_ACCESS_NOT_ENABLED), NULL);
  case NVM_ERR_SECURE_ERASE_NAMESPACE_EXISTS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SECURE_ERASE_NAMESPACE_EXISTS), NULL);
  case NVM_ERR_PASSPHRASE_TOO_LONG:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PASSPHRASE_TOO_LONG), NULL);
  case NVM_ERR_PASSPHRASE_NOT_PROVIDED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PASSPHRASE_NOT_PROVIDED), NULL);
  case NVM_ERR_PASSPHRASES_DO_NOT_MATCH:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PASSPHRASES_DO_NOT_MATCH), NULL);
  case NVM_ERR_SENSOR_NOT_VALID:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SENSOR_NOT_VALID), NULL);
  case NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE:
    pTempString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_CONTROLLER_OUT_OF_RANGE),  NULL);
    if (pTempString == NULL) {
      return pTempString;
    }
    pTempString1 = CatSPrintClean(NULL, pTempString, TEMPERATURE_THRESHOLD_MIN, TEMPERATURE_CONTROLLER_THRESHOLD_MAX);
    FREE_POOL_SAFE(pTempString);
    return pTempString1;
  case NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE:
    pTempString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_MEDIA_OUT_OF_RANGE),  NULL);

    if (pTempString == NULL) {
      return pTempString;
    }
    pTempString1 = CatSPrintClean(NULL, pTempString, TEMPERATURE_THRESHOLD_MIN, TEMPERATURE_MEDIA_THRESHOLD_MAX);
    FREE_POOL_SAFE(pTempString);
    return pTempString1;
  case NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE:
    pTempString = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_CAPACITY_OUT_OF_RANGE),  NULL);
    if (pTempString == NULL) {
      return pTempString;
    }
    pTempString1 = CatSPrintClean(NULL, pTempString, CAPACITY_THRESHOLD_MIN, CAPACITY_THRESHOLD_MAX);
    FREE_POOL_SAFE(pTempString);
    return pTempString1;
  case NVM_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE), NULL);
  case NVM_ERR_MEDIA_DISABLED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_MEDIA_DISABLED_VALUE), NULL);
  case NVM_ERR_ENABLE_SECURITY_NOT_ALLOWED:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_ENABLE_SECURITY_NOT_ALLOWED), NULL);
  case NVM_ERR_CREATE_GOAL_NOT_ALLOWED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_CREATE_GOAL_NOT_ALLOWED), NULL);
  case NVM_ERR_INVALID_SECURITY_STATE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_SECURITY_STATE), NULL);
  case NVM_ERR_INVALID_SECURITY_OPERATION:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_SECURITY_OPERATION), NULL);
  case NVM_ERR_UNABLE_TO_GET_SECURITY_STATE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_UNABLE_TO_GET_SECURITY_STATE), NULL);
  case NVM_ERR_INCONSISTENT_SECURITY_STATE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INCONSISTENT_SECURITY_STATE), NULL);

  case NVM_WARN_2LM_MODE_OFF:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_WARN_2LM_MODE_OFF), NULL);
  case NVM_WARN_IMC_DDR_PMM_NOT_PAIRED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_WARN_MM_PMM_DDR_NOT_PAIRED), NULL);
  case NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_WARN_REDUCED_CAPACITY_DUE_TO_SKU), NULL);
  case NVM_ERR_NAMESPACE_TOO_SMALL_FOR_BTT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NAMESPACE_TOO_SMALL_FOR_BTT), NULL);
  case NVM_ERR_PCD_BAD_DEVICE_CONFIG:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PCD_BAD_DEVICE_CONFIG), NULL);
  case NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM), NULL);
  case NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM), NULL);
  case NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM), NULL);
  case NVM_ERR_REGION_CONF_APPLYING_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_CONF_APPLYING_FAILED), NULL);
  case NVM_ERR_REGION_CONF_UNSUPPORTED_CONFIG:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_CONF_UNSUPPORTED_CONFIG), NULL);
  case NVM_ERR_REGION_NOT_FOUND:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_NOT_FOUND), NULL);
  case NVM_ERR_PLATFORM_NOT_SUPPORT_MANAGEMENT_SOFT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PLATFORM_NOT_SUPPORT_MANAGEMENT_SOFT), NULL);
  case NVM_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE), NULL);
  case NVM_ERR_PLATFORM_NOT_SUPPORT_PM_MODE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PLATFORM_NOT_SUPPORT_PM_MODE), NULL);
  case NVM_ERR_REGION_CURR_CONF_EXISTS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_CURR_CONF_EXISTS), NULL);

  case NVM_ERR_REGION_SIZE_TOO_SMALL_FOR_INT_SET_ALIGNMENT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_SIZE_TOO_SMALL_FOR_INT_SET_ALIGNMENT), NULL);
  case NVM_ERR_PLATFORM_NOT_SUPPORT_SPECIFIED_INT_SIZES:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PLATFORM_NOT_SUPPORT_SPECIFIED_INT_SIZES), NULL);
  case NVM_ERR_PLATFORM_NOT_SUPPORT_DEFAULT_INT_SIZES:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PLATFORM_NOT_SUPPORT_DEFAULT_INT_SIZES), NULL);
  case NVM_ERR_REGION_NOT_HEALTHY:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_NOT_HEALTHY), NULL);
  case NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_BLOCK_NAMESPACE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_NOT_ENOUGH_SPACE_FOR_BLOCK_NAMESPACE), NULL);
  case NVM_ERR_REGION_NOT_ENOUGH_SPACE_FOR_PM_NAMESPACE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_NOT_ENOUGH_SPACE_FOR_PM_NAMESPACE), NULL);
  case NVM_ERR_REGION_GOAL_NO_EXISTS_ON_DIMM:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_GOAL_NO_EXISTS_ON_DIMM), NULL);
  case NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS), NULL);
  case NVM_ERR_REGION_GOAL_NAMESPACE_EXISTS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_GOAL_NAMESPACE_EXISTS), NULL);
  case NVM_ERR_REGION_GOAL_AUTO_PROV_ENABLED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_CREATE_GOAL_AUTO_PROV_ENABLED), NULL);

  case NVM_ERR_REGION_REMAINING_SIZE_NOT_IN_LAST_PROPERTY:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_REGION_REMAINING_SIZE_NOT_IN_LAST_PROPERTY), NULL);
  case NVM_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS), NULL);

  case NVM_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED), NULL);
  case NVM_ERR_DUMP_NO_CONFIGURED_DIMMS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DUMP_NO_CONFIGURED_DIMMS), NULL);
  case NVM_ERR_DUMP_FILE_OPERATION_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DUMP_FILE_OPERATION_FAILED), NULL);
  case NVM_ERR_LOAD_VERSION:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_LOAD_VERSION), NULL);
  case NVM_ERR_LOAD_INVALID_DATA_IN_FILE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_LOAD_INVALID_DATA_IN_FILE), NULL);
  case NVM_ERR_LOAD_IMPROPER_CONFIG_IN_FILE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_LOAD_IMPROPER_CONFIG_IN_FILE), NULL);
  case NVM_ERR_LOAD_DIMM_COUNT_MISMATCH:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_LOAD_DIMM_COUNT_MISMATCH), NULL);

  case NVM_ERR_IMAGE_EXAMINE_INVALID:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_IMAGE_EXAMINE_INVALID), NULL);
  case NVM_SUCCESS_IMAGE_EXAMINE_OK:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_SUCCESS_IMAGE_EXAMINE_OK), NULL);
  case NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_IMAGE_EXAMINE_LOWER_VERSION), NULL);
  case NVM_ERR_IMAGE_FILE_NOT_VALID:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_IMAGE_FILE_NOT_VALID), NULL);

  case NVM_ERR_DIMM_SKU_MODE_MISMATCH:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DIMM_SKU_MODE_MISMATCH), NULL);
  case NVM_ERR_DIMM_SKU_SECURITY_MISMATCH:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DIMM_SKU_SECURITY_MISMATCH), NULL);

  case NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU), NULL);

  case NVM_ERR_NONE_DIMM_FULFILLS_CRITERIA:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NONE_DIMM_FULFILLS_CRITERIA), NULL);
  case NVM_ERR_UNSUPPORTED_BLOCK_SIZE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_UNSUPPORTED_BLOCK_SIZE), NULL);
  case NVM_ERR_INVALID_NAMESPACE_CAPACITY:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_NAMESPACE_CAPACITY), NULL);
  case NVM_ERR_NAMESPACE_DOES_NOT_EXIST:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NAMESPACE_DOES_NOT_EXIST), NULL);
  case NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NAMESPACE_CONFIGURATION_BROKEN), NULL);
  case NVM_ERR_NOT_ENOUGH_FREE_SPACE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NOT_ENOUGH_FREE_SPACE), NULL);
  case NVM_ERR_NOT_ENOUGH_FREE_SPACE_BTT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NOT_ENOUGH_FREE_SPACE_BTT), NULL);
  case NVM_ERR_FAILED_TO_UPDATE_BTT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FAILED_TO_UPDATE_BTT), NULL);
  case NVM_ERR_PLATFORM_NOT_SUPPORT_BLOCK_MODE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_PLATFORM_NOT_SUPPORT_BLOCK_MODE), NULL);
  case NVM_WARN_BLOCK_MODE_DISABLED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_WARN_BLOCK_MODE_DISABLED), NULL);
  case NVM_ERR_BADALIGNMENT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_BADALIGNMENT), NULL);
  case NVM_ERR_RENAME_NAMESPACE_NOT_SUPPORTED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_RENAME_NAMESPACE_NOT_SUPPORTED), NULL);
  case NVM_ERR_FAILED_TO_INIT_NS_LABELS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FAILED_TO_INIT_NS_LABELS), NULL);

  case NVM_ERR_SMART_FAILED_TO_GET_SMART_INFO:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SMART_FAILED_TO_GET_SMART_INFO), NULL);
  case NVM_WARN_SMART_NONCRITICAL_HEALTH_ISSUE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_WARN_SMART_NONCRITICAL_HEALTH_ISSUE), NULL);
  case NVM_ERR_SMART_CRITICAL_HEALTH_ISSUE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SMART_CRITICAL_HEALTH_ISSUE), NULL);
  case NVM_ERR_SMART_FATAL_HEALTH_ISSUE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SMART_FATAL_HEALTH_ISSUE), NULL);
  case NVM_ERR_SMART_READ_ONLY_HEALTH_ISSUE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SMART_READ_ONLY_HEALTH_ISSUE), NULL);
  case NVM_ERR_SMART_UNKNOWN_HEALTH_ISSUE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SMART_UNKNOWN_HEALTH_ISSUE), NULL);

  case NVM_ERR_FAILED_TO_GET_DIMM_INFO:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FAILED_TO_GET_DIMM_INFO), NULL);

  case NVM_ERR_FW_SET_OPTIONAL_DATA_POLICY_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FW_SET_OPTIONAL_DATA_POLICY_FAILED), NULL);
  case NVM_ERR_INVALID_OPTIONAL_DATA_POLICY_STATE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_OPTIONAL_DATA_POLICY_STATE), NULL);

  case NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FAILED_TO_GET_DIMM_REGISTERS), NULL);
  case NVM_ERR_SMBIOS_DIMM_ENTRY_NOT_FOUND_IN_NFIT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SMBIOS_DIMM_ENTRY_NOT_FOUND_IN_NFIT), NULL);
  case NVM_OPERATION_IN_PROGRESS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_OPERATION_IN_PROGRESS), NULL);

  case NVM_ERR_GET_PCD_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_GET_PCD_FAILED), NULL);

  case NVM_ERR_ARS_IN_PROGRESS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_ARS_IN_PROGRESS), NULL);

  case NVM_ERR_APPDIRECT_IN_SYSTEM:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_APPDIRECT_IN_SYSTEM), NULL);

  case NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FIRMWARE_DOWNGRADE), NULL);

  case NVM_ERR_FIRMWARE_ALREADY_LOADED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FIRMWARE_ALREADY_LOADED), NULL);

  case NVM_ERR_FIRMWARE_FAILED_TO_STAGE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FIRMWARE_FAILED_TO_STAGE), NULL);

  case NVM_ERR_IMAGE_FILE_NOT_COMPATIBLE_TO_CTLR_STEPPING:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_FIRMWARE_INCOMPATIBLE_TO_CTLR_STEPPING), NULL);

  case NVM_ERR_FIRMWARE_VERSION_NOT_VALID:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_VIEW_FIRMWARE_VERSION_NOT_VALID), NULL);

  case NVM_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE), NULL);

  case NVM_ERR_FW_DBG_SET_LOG_LEVEL_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_FW_SET_LOG_LEVEL_FAILED), NULL);

  case NVM_INFO_FW_DBG_LOG_NO_LOGS_TO_FETCH:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_INFO_FW_DBG_LOG_NO_LOGS_TO_FETCH), NULL);


  case NVM_ERR_API_NOT_SUPPORTED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_API_NOT_SUPPORTED), NULL);

  case NVM_ERR_UNKNOWN:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_UNKNOWN), NULL);

  case NVM_ERR_INVALID_PERMISSIONS:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INVALID_PERMISSIONS), NULL);

  case NVM_ERR_BAD_DEVICE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_BAD_DEVICE), NULL);

  case NVM_ERR_BUSY_DEVICE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_BUSY_DEVICE), NULL);

  case NVM_ERR_GENERAL_OS_DRIVER_FAILURE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_GENERAL_OS_DRIVER_FAILURE), NULL);

  case NVM_ERR_NO_MEM:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NO_MEM), NULL);

  case NVM_ERR_BAD_SIZE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_BAD_SIZE), NULL);

  case NVM_ERR_TIMEOUT:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_TIMEOUT), NULL);

  case NVM_ERR_DATA_TRANSFER:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DATA_TRANSFER), NULL);

  case NVM_ERR_GENERAL_DEV_FAILURE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_GENERAL_DEV_FAILURE), NULL);

  case NVM_ERR_BAD_FW:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_BAD_FW), NULL);

  case NVM_ERR_DRIVER_FAILED:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_DRIVERFAILED), NULL);

  case NVM_ERR_OPERATION_NOT_SUPPORTED:
      return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_NOT_SUPPORTED), NULL);


  case NVM_ERR_SPD_NOT_ACCESSIBLE:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_SPD_NOT_ACCESSIBLE), NULL);

  case NVM_ERR_INCOMPATIBLE_HARDWARE_REVISION:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERR_INCOMPATIBLE_HARDWARE_REVISION), NULL);


  default:
    return HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_DEFAULT), NULL);
  }
}

/**
  Translate all NVM Statuses of operation into
  Unicode string representing its brief description.

  @param[in] HiiHandle - Handle to the Hii Database that contains the NvmStatus strings
  @param[in] pObjectStatus - Object status containing BitField with NVM Statuses
  @param[in] pPrefixString - prefix for all lines of statuses

  @retval Pointer to a decoded string. Memory is dynamically allocated. It should be freed by caller.
**/
CHAR16 *
GetAllNvmStatusCodeMessages(
  IN     EFI_HANDLE    HiiHandle,
  IN     OBJECT_STATUS *pObjectStatus,
  IN     CONST CHAR16 *pPrefixString
  )
{
  CHAR16 *pStatusStr = NULL;
  CHAR16 *pSingleStatusStr = NULL;
  CHAR16 *pErrorLevelStr = NULL;
  UINT64 Index = NVM_LAST_STATUS_VALUE;
  BOOLEAN CodeSet = FALSE;

  NVDIMM_ENTRY();
  if (pObjectStatus == NULL || pPrefixString == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < NVM_LAST_STATUS_VALUE; ++Index) {
    CodeSet = IsSetNvmStatus(pObjectStatus, Index);
    if (CodeSet) {
      pSingleStatusStr = GetSingleNvmStatusCodeMessage(HiiHandle, Index);
      if (Index == NVM_SUCCESS || Index == NVM_SUCCESS_FW_RESET_REQUIRED ||
      Index == NVM_ERR_IMAGE_EXAMINE_INVALID || Index == NVM_SUCCESS_IMAGE_EXAMINE_OK ||
      Index == NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION ||
      Index == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {
         pStatusStr = CatSPrintClean(pStatusStr, L"" FORMAT_STR L"" FORMAT_STR_NL, pPrefixString, pSingleStatusStr);
      } else {
         pErrorLevelStr = HiiGetString(HiiHandle, STRING_TOKEN(STR_DCPMM_STATUS_ERROR), NULL);
         pStatusStr = CatSPrintClean(pStatusStr, L"" FORMAT_STR L"" FORMAT_STR L" (%d) - " FORMAT_STR_NL,
            pPrefixString,
            pErrorLevelStr,
            Index,
            pSingleStatusStr);
      }

      FREE_POOL_SAFE(pSingleStatusStr);
    }
  }
Finish:
  FREE_POOL_SAFE(pErrorLevelStr);
  NVDIMM_EXIT();
  return pStatusStr;
}


/**
  Clear Nvm status code for given object ID

  @param[in/out] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] ObjectId - object for clearing status
  @param[in] NvmStatusCode code to clear

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
VOID
ClearNvmStatusForObject(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     NvmStatusCode Code
  )
{

  OBJECT_STATUS *pObjectStatus = NULL;

  if (pCommandStatus == NULL) {
    NVDIMM_DBG("pCommandStatus = NULL, Invalid parameter");
  }
  else {
      pObjectStatus = GetObjectStatus(pCommandStatus, ObjectId);
      if (pObjectStatus != NULL) {
          ClearNvmStatus(pObjectStatus, Code);
      }
  }
}

/**
  Clear Nvm status code

  @param[in/out] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] NvmStatusCode code to clear

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
VOID
ClearNvmStatus(
  IN OUT OBJECT_STATUS *pObjectStatus,
  IN     NvmStatusCode Code
  )
{
  CONST UINT32 Index = Code / 64;
  CONST UINT64 Mod = Code % 64;
  CONST UINT64 Bit = (UINT64)1 << Mod;

  if (pObjectStatus != NULL) {
      pObjectStatus->StatusBitField.BitField[Index] &= ~Bit;
  }
}

/**
  Set proper code in Object status

  @param[in/out] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] NvmStatusCode code to set
**/
VOID
SetNvmStatus(
  IN OUT OBJECT_STATUS *pObjectStatus,
  IN     NvmStatusCode Code
  )
{
  CONST UINT32 Index = Code / 64;
  CONST UINT64 Mod = Code % 64;
  CONST UINT64 Bit = (UINT64)1 << Mod;

  if (pObjectStatus != NULL) {
      pObjectStatus->StatusBitField.BitField[Index] |= Bit;
  }
}

/**
  Check if Object status got proper NVM status code set.

  @param[in, out] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] ObjectId - object for checking status
  @param[in] NvmStatusCode code to check if is set

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
BOOLEAN
IsSetNvmStatusForObject(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     NVM_STATUS Status
  )
{
  OBJECT_STATUS *pObjectStatus = NULL;
  BOOLEAN IsSetObjectStatus = FALSE;
  NVDIMM_ENTRY();

  if (pCommandStatus == NULL) {
    NVDIMM_DBG("pCommandStatus = NULL, Invalid parameter");
    goto Finish;
  }

  pObjectStatus = GetObjectStatus(pCommandStatus, ObjectId);
  if (pObjectStatus == NULL) {
    goto Finish;
  }
  IsSetObjectStatus = IsSetNvmStatus(pObjectStatus, Status);

Finish:
  NVDIMM_EXIT();
  return IsSetObjectStatus;
}

/**
  Check if Object status got proper NVM status code set.

  @param[in] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] NvmStatusCode code to check if is set

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
BOOLEAN
IsSetNvmStatus(
  IN     OBJECT_STATUS *pObjectStatus,
  IN     NvmStatusCode Code
  )
{
  CONST UINT32 Index = Code / 64;
  CONST UINT64 Mod = Code % 64;
  CONST UINT64 Bit = (UINT64)1 << Mod;
  BOOLEAN IsSet = FALSE;

  NVDIMM_ENTRY();
  if (pObjectStatus == NULL) {
    goto Finish;
  }

  IsSet = (pObjectStatus->StatusBitField.BitField[Index] & Bit) != 0;
Finish:
  NVDIMM_EXIT();
  return IsSet;
}

/**
  Initialize command status structure.
  Allocate memory and assign default values.

  @param[in, out] ppCommandStatus pointer to address of the structure

  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
InitializeCommandStatus(
  IN OUT COMMAND_STATUS **ppCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  NVDIMM_ENTRY();

  if (ppCommandStatus == NULL) {
    goto Finish;
  }

  pCommandStatus = (COMMAND_STATUS *) AllocateZeroPool(sizeof(*pCommandStatus));
  if (pCommandStatus == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pCommandStatus->GeneralStatus = NVM_ERR_OPERATION_NOT_STARTED;
  pCommandStatus->ObjectStatusCount = 0;
  pCommandStatus->ObjectType = ObjectTypeUnknown;

  InitializeListHead(&pCommandStatus->ObjectStatusList);

  *ppCommandStatus = pCommandStatus;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Free previously allocated and initialized command status structure

  @param[in] ppCommandStatus pointer to address of the structure
**/
VOID
FreeCommandStatus(
  IN COMMAND_STATUS **ppCommandStatus
  )
{
  VOID *pTemporaryNode = NULL;
  LIST_ENTRY *pObjectStatusNode = NULL;
  LIST_ENTRY *pObjectStatusNextNode = NULL;
  NVDIMM_ENTRY();

  if (ppCommandStatus != NULL) {
    if (*ppCommandStatus != NULL) {
      LIST_FOR_EACH_SAFE(pObjectStatusNode, pObjectStatusNextNode, &((*ppCommandStatus)->ObjectStatusList)) {
        pTemporaryNode = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);
        FREE_POOL_SAFE(pTemporaryNode);
      }
    }
    FREE_POOL_SAFE(*ppCommandStatus);
  }

  NVDIMM_EXIT();
}

/**
  Add (or update!) status for specified ID in command status list

  @param[in, out] pCommandStatus - command status
  @param[in] ObjectId - Id for specified object
  @param[in] pObjectIdStr - Id for specified object as string representation, OPTIONAL
  @param[in] ObjectIdStrLength - Max length of pObjectIdStr, OPTIONAL
  @param[in] Status - status for update/set
**/
VOID
SetObjStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     CHAR16 *pObjectIdStr OPTIONAL,
  IN     UINT32 ObjectIdStrLength OPTIONAL,
  IN     NVM_STATUS Status
  )
{
  OBJECT_STATUS *pObjectStatus = NULL;
  NVDIMM_ENTRY();

  if (pCommandStatus == NULL) {
    NVDIMM_DBG("pCommandStatus = NULL, Invalid parameter");
    goto Finish;
  }

  if (!IsListInitialized(pCommandStatus->ObjectStatusList)) {
    InitializeListHead(&pCommandStatus->ObjectStatusList);
  }

  pObjectStatus = GetObjectStatus(pCommandStatus, ObjectId);
  if (pObjectStatus != NULL) {
    SetNvmStatus(pObjectStatus, Status);
    ClearNvmStatus(pObjectStatus, NVM_ERR_OPERATION_NOT_STARTED);
    goto Finish;
  }

  pObjectStatus = AllocateZeroPool(sizeof(*pObjectStatus));
  if (pObjectStatus == NULL) {
    NVDIMM_ERR("Out of memory");
    goto Finish;
  }

  pObjectStatus->ObjectId = ObjectId;

  if (pObjectIdStr != NULL && StrLen(pObjectIdStr) > 0) {
    pObjectStatus->IsObjectIdStr = TRUE;

    StrnCpyS(pObjectStatus->ObjectIdStr, MAX_OBJECT_ID_STR_LEN, pObjectIdStr, MIN(ObjectIdStrLength, MAX_OBJECT_ID_STR_LEN) - 1);
  } else {
    pObjectStatus->IsObjectIdStr = FALSE;
  }

  SetNvmStatus(pObjectStatus, Status);
  pObjectStatus->Signature = OBJECT_STATUS_SIGNATURE;
  pObjectStatus->Progress=0;
  InitializeListHead(&pObjectStatus->ObjectStatusNode);
  InsertTailList(&pCommandStatus->ObjectStatusList, &pObjectStatus->ObjectStatusNode);
  pCommandStatus->ObjectStatusCount++;
  pCommandStatus->GeneralStatus = Status;

Finish:
  NVDIMM_EXIT();
}

/**
  Set progress for specified ID in command status list

  @param[in, out] pCommandStatus - command status
  @param[in] ObjectId - Id for specified object
  @param[in] Progress - progress to set
**/
VOID
SetObjProgress(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     UINT8 Progress
  )
{
  OBJECT_STATUS *pObjectStatus = NULL;
  NVDIMM_ENTRY();

  if (pCommandStatus == NULL) {
    NVDIMM_DBG("pCommandStatus = NULL,Invalid parameter");
    goto Finish;
  }

  if (!IsListInitialized(pCommandStatus->ObjectStatusList)) {
    InitializeListHead(&pCommandStatus->ObjectStatusList);
  }

  pObjectStatus = GetObjectStatus(pCommandStatus, ObjectId);
  if (pObjectStatus != NULL) {
    SetNvmStatus(pObjectStatus, NVM_OPERATION_IN_PROGRESS);
    pObjectStatus->Progress = Progress;
    goto Finish;
  }

  pObjectStatus = AllocateZeroPool(sizeof(*pObjectStatus));
  if (pObjectStatus == NULL) {
    NVDIMM_ERR("Out of memory");
    goto Finish;
  }

  pObjectStatus->ObjectId = ObjectId;
  SetNvmStatus(pObjectStatus, NVM_OPERATION_IN_PROGRESS);
  pObjectStatus->Signature = OBJECT_STATUS_SIGNATURE;
  pObjectStatus->Progress = Progress;
  InitializeListHead(&pObjectStatus->ObjectStatusNode);
  InsertTailList(&pCommandStatus->ObjectStatusList, &pObjectStatus->ObjectStatusNode);
  pCommandStatus->ObjectStatusCount++;
  pCommandStatus->GeneralStatus = NVM_OPERATION_IN_PROGRESS;

Finish:
  NVDIMM_EXIT();
}

/**
  Set general command status and zero status object counter

  @param[in, out] pCommandStatus - command status
  @param[in] Status - status for update/set
**/
VOID
ResetCmdStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     NVM_STATUS Status
  )
{
  OBJECT_STATUS *pObjectStatus = NULL;
  LIST_ENTRY *pObjectStatusNode = NULL;
  LIST_ENTRY *pObjectStatusNextNode = NULL;


  NVDIMM_ENTRY();
  if (pCommandStatus == NULL) {
    NVDIMM_DBG("pCommandStatus = NULL, Invalid parameter");
    goto Finish;
  }

  if (IsListInitialized(pCommandStatus->ObjectStatusList) && !IsListEmpty(&pCommandStatus->ObjectStatusList)) {
    /** Free object status memory **/
    LIST_FOR_EACH_SAFE(pObjectStatusNode, pObjectStatusNextNode, &pCommandStatus->ObjectStatusList) {
      pObjectStatus = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);
      RemoveEntryList(pObjectStatusNode);
      FREE_POOL_SAFE(pObjectStatus);
    }
  }
  pCommandStatus->ObjectStatusCount = 0;

  pCommandStatus->GeneralStatus = Status;

Finish:
  NVDIMM_EXIT();
}

/**
  Set general command status

  @param[in, out] pCommandStatus - command status
  @param[in] Status - status for update/set
**/
VOID
SetCmdStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     NVM_STATUS Status
  )
{
  NVDIMM_ENTRY();
  if (pCommandStatus == NULL) {
    NVDIMM_DBG("pCommandStatus = NULL, Invalid parameter");
    goto Finish;
  }

  pCommandStatus->GeneralStatus = Status;

Finish:
  NVDIMM_EXIT();
}

/**
  Search ObjectStatus from command status object list by specified Id and return pointer.

  @param[in] pCommandStatus - command status
  @param[in] ObjectId - Id for specified object

  @retval pointer to OBJECT_STATUS.
  @retval NULL if object with specified Id not found.
**/
OBJECT_STATUS *
GetObjectStatus(
  IN     COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId
  )
{
  LIST_ENTRY *pObjectStatusNode = NULL;
  OBJECT_STATUS *pObjectStatus = NULL;
  OBJECT_STATUS *pSearchedObj = NULL;

  if (pCommandStatus == NULL) {
    goto Finish;
  }

  LIST_FOR_EACH(pObjectStatusNode, &pCommandStatus->ObjectStatusList) {
    pObjectStatus = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);

    if (pObjectStatus == NULL) {
      goto Finish;
    }
    if (pObjectStatus->ObjectId == ObjectId) {
      pSearchedObj = pObjectStatus;
      break;
    }
  }

Finish:
  NVDIMM_EXIT();
  return pSearchedObj;
}

/**
  Iterate all lit NVM status codes and count warnings and errors

  @param[in] pCommandStatus pointer to address of the structure
  @param[out] pNumberOfWarnings - output address to keep no warnings
  @param[out] pNumberOfErrors - output address to keep no errors

  @retval EFI_SUCCESS All Ok
  @retval EFI_INVALID_PARAMETER if the parameter is a NULL.
**/
EFI_STATUS
CountNumberOfErrorsAndWarnings(
  IN     COMMAND_STATUS *pCommandStatus,
  OUT    UINT64 *pNumberOfWarnings,
  OUT    UINT64 *pNumberOfErrors
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pNode = NULL;
  OBJECT_STATUS *pObjectStatus = NULL;
  LIST_ENTRY *pObjectStatusNextNode = NULL;
  UINT64 Index = 0;

  if (pCommandStatus == NULL || pNumberOfWarnings == NULL || pNumberOfErrors == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  *pNumberOfErrors = 0;
  *pNumberOfWarnings = 0;

  LIST_FOR_EACH_SAFE(pNode, pObjectStatusNextNode, &(pCommandStatus->ObjectStatusList)) {
    pObjectStatus = (OBJECT_STATUS *)pNode;
    for (Index = 0; Index < NVM_LAST_STATUS_VALUE; ++Index) {
      if (IsSetNvmStatus(pObjectStatus, Index)) {
        if (IsSetNvmStatus(&gAllErrorNvmStatuses, Index)) {
          (*pNumberOfErrors)++;
        } else if (IsSetNvmStatus(&gAllWarningNvmStatuses, Index)) {
          (*pNumberOfWarnings)++;
        }
      }
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
