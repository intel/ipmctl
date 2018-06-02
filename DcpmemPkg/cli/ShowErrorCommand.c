/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include "ShowErrorCommand.h"
#include "DumpDebugCommand.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "LoadCommand.h"
#include "Debug.h"
#include "Convert.h"

/**
  show -error syntax definition
**/
struct Command ShowErrorCommandSyntax =
{
  SHOW_VERB,                                                           //!< verb
  {
      {L"", L"", L"", L"", FALSE, ValueOptional},                      //!< options
  },
  {
    {ERROR_TARGET, L"", HELP_TEXT_ERROR_LOG, TRUE, ValueRequired},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}
  },
  {
    {SEQUENCE_NUM_PROPERTY, L"", HELP_TEXT_ERROR_LOG_SEQ_NUM_PROPERTY, FALSE, ValueRequired},
    {LEVEL_PROPERTY, L"", HELP_TEXT_ERROR_LOG_LEVEL_PROPERTY, FALSE, ValueRequired},
    {COUNT_PROPERTY, L"", HELP_TEXT_ERROR_LOG_COUNT_PROPERTY, FALSE, ValueRequired}
  },                                                                  //!< properties
  L"Show error log for given DIMM",                                   //!< help
  ShowErrorCommand                                                    //!< run function
};

/**
  Register syntax of show -error
**/
EFI_STATUS
RegisterShowErrorCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowErrorCommandSyntax);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get error log command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
ShowErrorCommand(
  IN    struct Command *pCmd
)
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BOOLEAN ThermalError = FALSE;
  BOOLEAN HighLevel = FALSE;
  BOOLEAN IsNumber = FALSE;
  CHAR16 *pTargetValue = NULL;
  CHAR16 *pPropertyValue = NULL;
  CHAR16 *pErrorType = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  UINT16 Index = 0;
  UINT16 Index2 = 0;
  UINT32 DimmIdsNum = 0;
  UINT16 SequenceNum = ERROR_LOG_DEFAULT_SEQUENCE_NUMBER; // By default start from first log
  UINT32 RequestedCount = ERROR_LOG_DEFAULT_COUNT;   // By default get all logs
  UINT32 ReturnedCount = 0;
  UINT64 ParsedNumber = 0;
  ERROR_LOG_INFO ErrorsArray[ERROR_LOG_MAX_COUNT];
  MEDIA_ERROR_LOG_INFO *pMediaErrorInfo = NULL;
  THERMAL_ERROR_LOG_INFO *pThermalErrorInfo = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  UINT16 ManageableListIndex = 0;
  UINT64 RangeInBytes = 0;

  NVDIMM_ENTRY();

  ZeroMem(ErrorsArray, sizeof(ErrorsArray));
  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** Get value of "error" target **/
  pTargetValue = GetTargetValue(pCmd, ERROR_TARGET);
  if (pTargetValue == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  if (StrICmp(pTargetValue, ERROR_TARGET_THERMAL_VALUE) == 0) {
    ThermalError = TRUE;
  } else if (StrICmp(pTargetValue, ERROR_TARGET_MEDIA_VALUE) == 0) {
    ThermalError = FALSE;
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_ERROR);
    goto Finish;
  }

  /** Open Config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** if a specific DIMM pid was passed in, set it **/
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsNum)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  ReturnCode = GetPropertyValue(pCmd, SEQUENCE_NUM_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If sequence number property exists, check it validity
    IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
    if (!IsNumber) {
      NVDIMM_WARN("Sequence number value is not a number");
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_SEQ_NUM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    } else if (ParsedNumber > ERROR_LOG_MAX_SEQUENCE_NUMBER) {
      NVDIMM_WARN("Sequence number value %d is greater than maximum %d", ParsedNumber, ERROR_LOG_MAX_SEQUENCE_NUMBER);
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_SEQ_NUM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    SequenceNum = (UINT16)ParsedNumber;
  } else {
    // If sequence number property doesn't exists is ok, it is optional param, using default value
    ReturnCode = EFI_SUCCESS;
  }

  ReturnCode = GetPropertyValue(pCmd, LEVEL_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If level property exists, check it validity
    if (StrICmp(pPropertyValue, LEVEL_HIGH_PROPERTY_VALUE) == 0) {
      HighLevel = TRUE;
    } else if (StrICmp(pPropertyValue, LEVEL_LOW_PROPERTY_VALUE) == 0) {
      HighLevel = FALSE;
    } else {
      ReturnCode = EFI_INVALID_PARAMETER;
      NVDIMM_WARN("Invalid Error Level. Error Level can be %s or %s", LEVEL_HIGH_PROPERTY_VALUE, LEVEL_LOW_PROPERTY_VALUE);
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_LEVEL);
      goto Finish;
    }
  } else {
    // If level property doesn't exists is ok, it is optional param, using default value
    HighLevel = TRUE;
    ReturnCode = EFI_SUCCESS;
  }

  ReturnCode = GetPropertyValue(pCmd, COUNT_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If Count property exists, check it validity
    IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
    if (!IsNumber) {
      NVDIMM_WARN("Count value is not a number");
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    } else if (ParsedNumber > ERROR_LOG_MAX_COUNT) {
      NVDIMM_WARN("Count value %d is greater than maximum %d", ParsedNumber, ERROR_LOG_MAX_COUNT);
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    RequestedCount = (UINT32)ParsedNumber;
  } else {
    // If count property doesn't exists is ok, it is optional param, using default value
    ReturnCode = EFI_SUCCESS;
  }

  if (DimmIdsNum == 0) {
    DimmIdsNum = DimmCount;
    pDimmIds = AllocateZeroPool(sizeof(*pDimmIds) * DimmIdsNum);
    if (pDimmIds == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      NVDIMM_WARN("Failed on memory allocation.");
      goto Finish;
    }

    for (Index = 0; Index < DimmIdsNum; Index++) {
      if (pDimms[Index].ManageabilityState == MANAGEMENT_VALID_CONFIG) {
        pDimmIds[ManageableListIndex] = pDimms[Index].DimmID;
        ManageableListIndex++;
      }
    }
    DimmIdsNum = ManageableListIndex;
  }

  if (DimmIdsNum == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND);
    goto Finish;
  }

  for(Index = 0; Index < DimmIdsNum; Index++) {
    ReturnedCount = RequestedCount;
    ReturnCode = pNvmDimmConfigProtocol->GetErrorLog(pNvmDimmConfigProtocol,
      &pDimmIds[Index],
      1,
      ThermalError,
      SequenceNum,
      HighLevel,
      &ReturnedCount,
      ErrorsArray,
      pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
        ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
      }
      break;
    }

    ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (ReturnedCount == 0) {
      Print(L"No errors found on DIMM (" FORMAT_STR L")\n", DimmStr);
    }
    else {
      for (Index2 = 0; Index2 < ReturnedCount; Index2++) {
        pErrorType = (ErrorsArray[Index2].ErrorType == THERMAL_ERROR ?
        ERROR_THERMAL_OCCURRED_STR : ERROR_MEDIA_OCCURRED_STR);
        Print(FORMAT_STR_SPACE L"on DIMM (" FORMAT_STR L"):\n", pErrorType, DimmStr);
        Print(FORMAT_16STR L" : %lld\n", ERROR_SYSTEM_TIMESTAMP_STR, ErrorsArray[Index2].SystemTimestamp);

        if (ErrorsArray[Index2].ErrorType == THERMAL_ERROR) {
          pThermalErrorInfo = (THERMAL_ERROR_LOG_INFO *)ErrorsArray[Index2].OutputData;
          Print(FORMAT_16STR L" : %d" FORMAT_STR_NL, ERROR_THERMAL_TEMPERATURE_STR, pThermalErrorInfo->Temperature, TEMPERATURE_MSR);
          // Thermal Reported
          if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_USER_ALARM) {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_REPORTED_STR,
                pThermalErrorInfo->Reported, ERROR_THERMAL_REPORTED_USER_ALARM_STR);
          } else if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_LOW) {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_REPORTED_STR,
                pThermalErrorInfo->Reported, ERROR_THERMAL_REPORTED_LOW_STR);
          } else if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_HIGH) {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_REPORTED_STR,
                pThermalErrorInfo->Reported, ERROR_THERMAL_REPORTED_HIGH_STR);
          } else if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_CRITICAL) {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_REPORTED_STR,
                pThermalErrorInfo->Reported, ERROR_THERMAL_REPORTED_CRITICAL_STR);
          } else {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_REPORTED_STR,
                pThermalErrorInfo->Reported, ERROR_THERMAL_REPORTED_UNKNOWN_STR);
          }
          // Temperature Type
          if (pThermalErrorInfo->Type == ERROR_THERMAL_TYPE_MEDIA) {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_TYPE_STR,
                pThermalErrorInfo->Type, ERROR_THERMAL_TYPE_MEDIA_STR);
          } else if (pThermalErrorInfo->Type == ERROR_THERMAL_TYPE_CORE) {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_TYPE_STR,
                pThermalErrorInfo->Type, ERROR_THERMAL_TYPE_CORE_STR);
          } else {
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_THERMAL_TYPE_STR,
                pThermalErrorInfo->Type, ERROR_THERMAL_TYPE_UNKNOWN_STR);
          }
          Print(FORMAT_16STR L" : %d\n", ERROR_SEQUENCE_NUMBER, pThermalErrorInfo->SequenceNum);
        }
        else {
          pMediaErrorInfo = (MEDIA_ERROR_LOG_INFO *)ErrorsArray[Index2].OutputData;
          Print(FORMAT_16STR L" : 0x%08llx\n", ERROR_MEDIA_DPA_STR, pMediaErrorInfo->Dpa);
          Print(FORMAT_16STR L" : 0x%08llx\n", ERROR_MEDIA_PDA_STR, pMediaErrorInfo->Pda);
          // Range in bytes
          RangeInBytes = Pow(2, pMediaErrorInfo->Range);
          Print(FORMAT_16STR L" : %lldB\n", ERROR_MEDIA_RANGE_STR, RangeInBytes);
          // Error Type
          switch (pMediaErrorInfo->ErrorType) {
            case ERROR_TYPE_UNCORRECTABLE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_UNCORRECTABLE_STR);
              break;
            case ERROR_TYPE_DPA_MISMATCH:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_DPA_MISMATCH_STR);
              break;
            case ERROR_TYPE_AIT_ERROR:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_AIT_ERROR_STR);
              break;
            case ERROR_TYPE_DATA_PATH_ERROR:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_DATA_PATH_ERROR_STR);
              break;
            case ERROR_TYPE_LOCKED_ILLEGAL_ACCESS:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_LOCKED_ILLEGAL_ACCESS_STR);
              break;
            case ERROR_TYPE_PERCENTAGE_REMAINING:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_PERCENTAGE_REMAINING_STR);
              break;
            case ERROR_TYPE_SMART_CHANGE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_SMART_CHANGE_STR);
              break;
            default:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_ERROR_TYPE_STR,
                pMediaErrorInfo->ErrorType, ERROR_TYPE_UNKNOWN_STR);
              break;
          }
          // Error Flags
          Print(FORMAT_16STR L" : ", ERROR_MEDIA_ERROR_FLAGS_STR);
          if (pMediaErrorInfo->PdaValid) {
            Print(FORMAT_STR L" ", ERROR_FLAGS_PDA_VALID_STR);
          }
          if (pMediaErrorInfo->DpaValid) {
            Print(FORMAT_STR L" ", ERROR_FLAGS_DPA_VALID_STR);
          }
          if (pMediaErrorInfo->Interrupt) {
            Print(FORMAT_STR L" ", ERROR_FLAGS_INTERRUPT_STR);
          }
          if (pMediaErrorInfo->Viral) {
            Print(FORMAT_STR L" ", ERROR_FLAGS_VIRAL_STR);
          }
          Print(L"\n");
          // Transaction Type
          switch (pMediaErrorInfo->TransactionType) {
            case TRANSACTION_TYPE_2LM_READ:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_2LM_READ_STR);
              break;
            case TRANSACTION_TYPE_2LM_WRITE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_2LM_WRITE_STR);
              break;
            case TRANSACTION_TYPE_PM_READ:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_PM_READ_STR);
              break;
            case TRANSACTION_TYPE_PM_WRITE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_PM_WRITE_STR);
              break;
            case TRANSACTION_TYPE_AIT_READ:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_AIT_READ_STR);
              break;
            case TRANSACTION_TYPE_AIT_WRITE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_AIT_WRITE_STR);
              break;
            case TRANSACTION_TYPE_WEAR_LEVEL_MOVE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_WEAR_LEVEL_MOVE_STR);
              break;
            case TRANSACTION_TYPE_PATROL_SCRUB:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_PATROL_SCRUB_STR);
              break;
            case TRANSACTION_TYPE_CSR_READ:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_CSR_READ_STR);
              break;
            case TRANSACTION_TYPE_CSR_WRITE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_CSR_WRITE_STR);
              break;
            case TRANSACTION_TYPE_ARS:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_ARS_STR);
              break;
            case TRANSACTION_TYPE_UNAVAILABLE:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_UNAVAILABLE_STR);
              break;
            default:
              Print(FORMAT_16STR L" : %d - " FORMAT_STR L"\n", ERROR_MEDIA_TRANSACTION_TYPE_STR,
                pMediaErrorInfo->TransactionType, TRANSACTION_TYPE_UNKNOWN_STR);
              break;
          }
          Print(FORMAT_16STR L" : %d\n", ERROR_SEQUENCE_NUMBER, pMediaErrorInfo->SequenceNum);
        }
      }
    }
  }

  if (EFI_ERROR(ReturnCode)) {
    Print(L"Failed to get error logs.\n");
    goto Finish;
  }

Finish:
  DisplayCommandStatus(L"Show error", L" on", pCommandStatus);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
