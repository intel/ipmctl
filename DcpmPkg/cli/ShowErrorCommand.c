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
  {                                                                    //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", HELP_ALL_DETAILS_TEXT,FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", LARGE_PAYLOAD_OPTION, L"", L"", HELP_LPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SMALL_PAYLOAD_OPTION, L"", L"", HELP_SPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT,FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", L"",FALSE, ValueOptional}
#endif
  },
  {
    {ERROR_TARGET, L"", HELP_TEXT_ERROR_LOG, TRUE, ValueRequired},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional}
  },
  {
    {SEQUENCE_NUM_PROPERTY, L"", HELP_TEXT_ERROR_LOG_SEQ_NUM_PROPERTY, FALSE, ValueRequired},
    {LEVEL_PROPERTY, L"", HELP_TEXT_ERROR_LOG_LEVEL_PROPERTY, FALSE, ValueRequired},
    {COUNT_PROPERTY, L"", HELP_TEXT_ERROR_LOG_COUNT_PROPERTY, FALSE, ValueRequired}
  },                                                                  //!< properties
  L"Show error log for given DIMM",                                   //!< help
  ShowErrorCommand,                                                   //!< run function
  TRUE
};

#define DS_ROOT_PATH                        L"/ErrorList"
#define DS_DIMM_PATH                        L"/ErrorList/Dimm"
#define DS_DIMM_INDEX_PATH                  L"/ErrorList/Dimm[%d]"
#define DS_ERROR_PATH                       L"/ErrorList/Dimm/Error"
#define DS_ERROR_INDEX_PATH                 L"/ErrorList/Dimm[%d]/Error[%d]"

/**
  List heading names
**/
#define ERROR_STR L"Error"

CHAR16 *mppAllowedShowErrorDisplayValues[] =
{
  DIMM_ID_STR,
  ERROR_STR
};


/*
*  SHOW MEDIA ERROR ATTRIBUTES (3 columns)
*   DimmID | System Timestamp    | Error Type
*   ==========================================
*   0x0001 | 01/01/1998 00:03:30 |      x
*   ...
*/
PRINTER_TABLE_ATTRIB ShowMediaErrorTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                                          //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                                   //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR                               //COLUMN DATA PATH
    },
    {
      ERROR_SYSTEM_TIMESTAMP_STR,                                           //COLUMN HEADER
      SENSOR_VALUE_MAX_STR_WIDTH,                                           //COLUMN MAX STR WIDTH
      DS_ERROR_PATH PATH_KEY_DELIM ERROR_SYSTEM_TIMESTAMP_STR               //COLUMN DATA PATH
    },
    {
      ERROR_MEDIA_ERROR_TYPE_STR,                                           //COLUMN HEADER
      ERROR_MAX_STR_WIDTH,                                                  //COLUMN MAX STR WIDTH
      DS_ERROR_PATH PATH_KEY_DELIM ERROR_MEDIA_ERROR_TYPE_STR               //COLUMN DATA PATH
    },
  }
};

/*
*  SHOW THERMAL ERROR ATTRIBUTES (4 columns)
*   DimmID | System Timestamp     | Temperature | Reported
*   =======================================================
*   0x0001 |             x        |      x      |    x
*   ...
*/

PRINTER_TABLE_ATTRIB ShowThermalErrorTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                                          //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                                   //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR                               //COLUMN DATA PATH
    },
    {
      ERROR_SYSTEM_TIMESTAMP_STR,                                           //COLUMN HEADER
      SENSOR_VALUE_MAX_STR_WIDTH,                                           //COLUMN MAX STR WIDTH
      DS_ERROR_PATH PATH_KEY_DELIM ERROR_SYSTEM_TIMESTAMP_STR               //COLUMN DATA PATH
    },
    {
      ERROR_THERMAL_TEMPERATURE_STR,                                        //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(ERROR_THERMAL_TEMPERATURE_STR),               //COLUMN MAX STR WIDTH
      DS_ERROR_PATH PATH_KEY_DELIM ERROR_THERMAL_TEMPERATURE_STR            //COLUMN DATA PATH
    },
    {
      ERROR_THERMAL_REPORTED_STR,                                           //COLUMN HEADER
      SENSOR_STATE_MAX_STR_WIDTH,                                           //COLUMN MAX STR WIDTH
      DS_ERROR_PATH PATH_KEY_DELIM ERROR_THERMAL_REPORTED_STR               //COLUMN DATA PATH
    },
  }
};

// List view for Media error
PRINTER_LIST_ATTRIB ShowMediaErrorListAttributes =
{
 {
    {
      DIMM_NODE_STR,                                                        //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",                        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,                           //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                                           //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      ERROR_STR,                                                            //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" ERROR_STR L"=$(" ERROR_STR L")",               //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,           //NULL or KEY VAL FORMAT STR
      ERROR_STR                                                             //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

// List view for Thermal error
PRINTER_LIST_ATTRIB ShowThermalErrorListAttributes =
{
 {
    {
      DIMM_NODE_STR,                                                        //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",                        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,                           //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                                           //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      ERROR_STR,                                                            //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" ERROR_STR L"=$(" ERROR_STR L")",               //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,           //NULL or KEY VAL FORMAT STR
      ERROR_STR                                                             //NULL or IGNORE KEY LIST (K1;K2)
    }
  }

};
PRINTER_DATA_SET_ATTRIBS ShowMediaErrorDataSetAttribs =
{
  &ShowMediaErrorListAttributes,
  &ShowMediaErrorTableAttributes
};

PRINTER_DATA_SET_ATTRIBS ShowThermalErrorDataSetAttribs =
{
  &ShowThermalErrorListAttributes,
  &ShowThermalErrorTableAttributes
};

/**
  Register the show -error command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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
  UINT32 RequestedCount = ERROR_LOG_MAX_COUNT;   // By default get all logs
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
  CHAR16 *pTempStr = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;
  BOOLEAN FoundMediaErrorFlag = FALSE;
  UINT8 MediaErrorInfoCount = 0;

  NVDIMM_ENTRY();

  ZeroMem(ErrorsArray, sizeof(ErrorsArray));
  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /**
    Printing will still work via compability mode if NULL so no need to check for NULL.
  **/
  pPrinterCtx = pCmd->pPrintCtx;

  pDispOptions = AllocateZeroPool(sizeof(CMD_DISPLAY_OPTIONS));
  if (NULL == pDispOptions) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowErrorDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowErrorDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  /** Get value of "error" target **/
  pTargetValue = GetTargetValue(pCmd, ERROR_TARGET);
  if (pTargetValue == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  if (StrICmp(pTargetValue, ERROR_TARGET_THERMAL_VALUE) == 0) {
    ThermalError = TRUE;
  }
  else if (StrICmp(pTargetValue, ERROR_TARGET_MEDIA_VALUE) == 0) {
    ThermalError = FALSE;
  }
  else {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_ERROR);
    goto Finish;
  }

  /** Open Config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NOT_FOUND) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  /** if a specific DIMM pid was passed in, set it **/
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsNum)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  ReturnCode = GetPropertyValue(pCmd, SEQUENCE_NUM_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If sequence number property exists, check it validity
    IsNumber = GetU64FromString(pPropertyValue, &ParsedNumber);
    if (!IsNumber) {
      NVDIMM_WARN("Sequence number value is not a number");
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_SEQ_NUM);
      goto Finish;
    }
    else if (ParsedNumber > ERROR_LOG_MAX_SEQUENCE_NUMBER) {
      NVDIMM_WARN("Sequence number value %d is greater than maximum %d", ParsedNumber, ERROR_LOG_MAX_SEQUENCE_NUMBER);
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_SEQ_NUM);
      goto Finish;
    }
    SequenceNum = (UINT16)ParsedNumber;
  }
  else {
    // If sequence number property doesn't exists is ok, it is optional param, using default value
    ReturnCode = EFI_SUCCESS;
  }

  ReturnCode = GetPropertyValue(pCmd, LEVEL_PROPERTY, &pPropertyValue);
  if (!EFI_ERROR(ReturnCode)) {
    // If level property exists, check it validity
    if (StrICmp(pPropertyValue, LEVEL_HIGH_PROPERTY_VALUE) == 0) {
      HighLevel = TRUE;
    }
    else if (StrICmp(pPropertyValue, LEVEL_LOW_PROPERTY_VALUE) == 0) {
      HighLevel = FALSE;
    }
    else {
      ReturnCode = EFI_INVALID_PARAMETER;
      NVDIMM_WARN("Invalid Error Level. Error Level can be %s or %s", LEVEL_HIGH_PROPERTY_VALUE, LEVEL_LOW_PROPERTY_VALUE);
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_LEVEL);
      goto Finish;
    }
  }
  else {
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
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
      goto Finish;
    }
    else if (ParsedNumber > ERROR_LOG_MAX_COUNT) {
      NVDIMM_WARN("Count value %d is greater than maximum %d", ParsedNumber, ERROR_LOG_MAX_COUNT);
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_COUNT);
      goto Finish;
    }
    RequestedCount = (UINT32)ParsedNumber;
  }
  else {
    // If count property doesn't exists is ok, it is optional param, using default value
    ReturnCode = EFI_SUCCESS;
  }

  if (DimmIdsNum == 0) {
    DimmIdsNum = DimmCount;
    FREE_POOL_SAFE(pDimmIds);
    pDimmIds = AllocateZeroPool(sizeof(*pDimmIds) * DimmIdsNum);
    if (pDimmIds == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
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

  for (Index = 0; Index < DimmIdsNum; Index++) {
    ReturnedCount = RequestedCount;
    ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
      DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
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
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Failed to get error logs from DIMM " FORMAT_STR L"\n", DimmStr);
      continue;
    }
    if (ReturnedCount == 0) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"No errors found on DIMM " FORMAT_STR L"\n", DimmStr);
    }
    else {
      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, Index);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

      for (Index2 = 0; Index2 < ReturnedCount; Index2++) {
        pErrorType = (ErrorsArray[Index2].ErrorType == THERMAL_ERROR ?
          ERROR_THERMAL_OCCURRED_STR : ERROR_MEDIA_OCCURRED_STR);

        PRINTER_BUILD_KEY_PATH(pPath, DS_ERROR_INDEX_PATH, Index, Index2);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_STR, pErrorType);

        pTempStr = GetTimeFormatString(ErrorsArray[Index2].SystemTimestamp, FALSE);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_SYSTEM_TIMESTAMP_STR, pTempStr);

        FREE_POOL_SAFE(pTempStr);

        if (ErrorsArray[Index2].ErrorType == THERMAL_ERROR) {
          pThermalErrorInfo = (THERMAL_ERROR_LOG_INFO *)ErrorsArray[Index2].OutputData;
          PRINTER_SET_KEY_VAL_UINT16(pPrinterCtx, pPath, ERROR_THERMAL_TEMPERATURE_STR, pThermalErrorInfo->Temperature, DECIMAL);

          // Thermal Reported
          if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_USER_ALARM) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_REPORTED_USER_ALARM_STR);
          }
          else if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_LOW) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_REPORTED_LOW_STR);
          }
          else if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_HIGH) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_REPORTED_HIGH_STR);
          }
          else if (pThermalErrorInfo->Reported == ERROR_THERMAL_REPORTED_CRITICAL) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_REPORTED_CRITICAL_STR);
          }
          else {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_REPORTED_UNKNOWN_STR);
          }
          // Temperature Type
          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_SEQUENCE_NUMBER))) {
            if (pThermalErrorInfo->Type == ERROR_THERMAL_TYPE_MEDIA) {
              PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_TYPE_MEDIA_STR);
            }
            else if (pThermalErrorInfo->Type == ERROR_THERMAL_TYPE_CONTROLLER) {
              PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_TYPE_CONTROLLER_STR);
            }
            else {
              PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_THERMAL_REPORTED_STR, ERROR_THERMAL_TYPE_UNKNOWN_STR);
            }
          }
          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_SEQUENCE_NUMBER))) {
            PRINTER_SET_KEY_VAL_UINT16(pPrinterCtx, pPath, ERROR_SEQUENCE_NUMBER, pThermalErrorInfo->SequenceNum, DECIMAL);
          }
        }
        else {

          pMediaErrorInfo = (MEDIA_ERROR_LOG_INFO *)ErrorsArray[Index2].OutputData;

          // Error Type
          switch (pMediaErrorInfo->ErrorType) {
          case ERROR_TYPE_UNCORRECTABLE:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_UNCORRECTABLE, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_UNCORRECTABLE_STR);
            break;
          case ERROR_TYPE_DPA_MISMATCH:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_DPA_MISMATCH, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_DPA_MISMATCH_STR);
            break;
          case ERROR_TYPE_AIT_ERROR:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_AIT_ERROR, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_AIT_ERROR_STR);
            break;
          case ERROR_TYPE_DATA_PATH_ERROR:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_DATA_PATH_ERROR, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_DATA_PATH_ERROR_STR);
            break;
          case ERROR_TYPE_LOCKED_ILLEGAL_ACCESS:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_LOCKED_ILLEGAL_ACCESS, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_LOCKED_ILLEGAL_ACCESS_STR);
            break;
          case ERROR_TYPE_PERCENTAGE_REMAINING:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_PERCENTAGE_REMAINING, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_PERCENTAGE_REMAINING_STR);
            break;
          case ERROR_TYPE_SMART_CHANGE:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_SMART_CHANGE, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_SMART_CHANGE_STR);
            break;
          case ERROR_TYPE_PERSISTENT_WRITE_ECC:
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_PERSISTENT_WRITE_ECC, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_PERSISTENT_WRITE_ECC_STR);
            break;
          default:
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_TYPE_STR, ERROR_TYPE_UNKNOWN_STR);
            break;
          }

          // Transaction Type
          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_MEDIA_TRANSACTION_TYPE_STR))) {
            switch (pMediaErrorInfo->TransactionType) {
            case TRANSACTION_TYPE_2LM_READ:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_2LM_READ, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_2LM_READ_STR);
              break;
            case TRANSACTION_TYPE_2LM_WRITE:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_2LM_WRITE, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_2LM_WRITE_STR);
              break;
            case TRANSACTION_TYPE_PM_READ:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_PM_READ, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_PM_READ_STR);
              break;
            case TRANSACTION_TYPE_PM_WRITE:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_PM_WRITE, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_PM_WRITE_STR);
              break;
            case TRANSACTION_TYPE_AIT_READ:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_AIT_READ, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_AIT_READ_STR);
              break;
            case TRANSACTION_TYPE_AIT_WRITE:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_AIT_WRITE, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_AIT_WRITE_STR);
              break;
            case TRANSACTION_TYPE_WEAR_LEVEL_MOVE:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_WEAR_LEVEL_MOVE, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_WEAR_LEVEL_MOVE_STR);
              break;
            case TRANSACTION_TYPE_PATROL_SCRUB:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_PATROL_SCRUB, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_PATROL_SCRUB_STR);
              break;
            case TRANSACTION_TYPE_CSR_READ:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_CSR_READ, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_CSR_READ_STR);
              break;
            case TRANSACTION_TYPE_CSR_WRITE:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_CSR_WRITE, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_CSR_WRITE_STR);
              break;
            case TRANSACTION_TYPE_ARS:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_ARS, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_ARS_STR);
              break;
            case TRANSACTION_TYPE_UNAVAILABLE:
              PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_UNAVAILABLE, HEX);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, ERROR_MSG_EXTRA_SPACE);
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_UNAVAILABLE_STR);
              break;
            default:
              PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_TRANSACTION_TYPE_STR, TRANSACTION_TYPE_UNKNOWN_STR);
              break;
            }
          }

          // Media Error info flags
          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_MEDIA_ERROR_FLAGS_STR))) {
            if (pMediaErrorInfo->PdaValid) {
              MediaErrorInfoCount = MediaErrorInfoCount | pMediaErrorInfo->PdaValid;
            }
            if (pMediaErrorInfo->DpaValid) {
              MediaErrorInfoCount = MediaErrorInfoCount | (pMediaErrorInfo->DpaValid << 1);
            }
            if (pMediaErrorInfo->Interrupt) {
              MediaErrorInfoCount = MediaErrorInfoCount | (pMediaErrorInfo->Interrupt << 2);
            }
            PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, MediaErrorInfoCount, HEX);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, ERROR_MSG_EXTRA_SPACE);

            if (pMediaErrorInfo->PdaValid) {
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, ERROR_FLAGS_PDA_VALID_STR);
              FoundMediaErrorFlag = TRUE;
            }
            if (pMediaErrorInfo->DpaValid) {
              if (FoundMediaErrorFlag == TRUE) {
                PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, ERROR_MSG_COMA_CHAR);
              }
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, ERROR_FLAGS_DPA_VALID_STR);
              FoundMediaErrorFlag = TRUE;
            }
            if (pMediaErrorInfo->Interrupt) {
              if (FoundMediaErrorFlag == TRUE) {
                PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, ERROR_MSG_COMA_CHAR);
              }
              PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_ERROR_FLAGS_STR, ERROR_FLAGS_INTERRUPT_STR);
            }
            FoundMediaErrorFlag = FALSE;
            MediaErrorInfoCount = 0;
          }

          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_MEDIA_DPA_STR))) {
            PRINTER_SET_KEY_VAL_UINT64(pPrinterCtx, pPath, ERROR_MEDIA_DPA_STR, pMediaErrorInfo->Dpa, HEX);
          }
          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_MEDIA_PDA_STR))) {
            PRINTER_SET_KEY_VAL_UINT64(pPrinterCtx, pPath, ERROR_MEDIA_PDA_STR, pMediaErrorInfo->Pda, HEX);
          }
          // Range in bytes
          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_MEDIA_RANGE_STR))) {
            RangeInBytes = Pow(2, pMediaErrorInfo->Range);
            PRINTER_SET_KEY_VAL_UINT64(pPrinterCtx, pPath, ERROR_MEDIA_RANGE_STR, RangeInBytes, DECIMAL);
            PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_MEDIA_RANGE_STR, ERROR_MSG_BYTE_CHAR);
          }

          if (pDispOptions->AllOptionSet || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_SEQUENCE_NUMBER))) {
            PRINTER_SET_KEY_VAL_UINT64(pPrinterCtx, pPath, ERROR_SEQUENCE_NUMBER, pMediaErrorInfo->SequenceNum, DECIMAL);
          }
        }
      }
    }
  }

  if (ThermalError == FALSE) {
    PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowMediaErrorDataSetAttribs);
  }
  else {
    PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowThermalErrorDataSetAttribs);
  }
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  DisplayCommandStatus(L"Show error", L" on", pCommandStatus);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
