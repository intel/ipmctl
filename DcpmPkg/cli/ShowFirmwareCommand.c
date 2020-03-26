/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Library/ShellLib.h>
#include "ShowFirmwareCommand.h"
#include "NvmDimmCli.h"

#define ACTIVE_FW_VERSION_STR                 L"ActiveFWVersion"
#define STAGED_FW_VERSION_STR                 L"StagedFWVersion"
#define STAGED_FW_ACTIVATABLE_STR             L"StagedFWActivatable"
#define FW_UPDATE_STATUS_STR                  L"FWUpdateStatus"
#define FW_IMAGE_MAX_SIZE_STR                 L"FWImageMaxSize"
#define QUIESCE_REQUIRED_STR                  L"QuiesceRequired"
#define ACTIVATION_TIME_STR                   L"ActivationTime"

#define DS_ROOT_PATH                          L"/DimmFirmwareList"
#define DS_DIMM_FW_PATH                       L"/DimmFirmwareList/DimmFirmware"
#define DS_DIMM_FW_INDEX_PATH                 L"/DimmFirmwareList/DimmFirmware[%d]"
#define DS_GROUP_STR                          L"DimmFirmware"

 /*
  *  PRINT LIST ATTRIBUTES
  *  ---DimmId=0x0001---
  *     ActiveFwVersion=X
  *     StagedFwVersion=X
  *     ...
  */
PRINTER_LIST_ATTRIB ShowFirmwareListAttributes =
{
 {
    {
      DS_GROUP_STR,                                         //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                           //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                           //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

/*
*  PRINTER TABLE ATTRIBUTES (3 columns)
*   DimmID   | ActiveFwVersion | StagedFwVersion
*   ================================================
*   0x0001   | aa.bb.cc.dddd   | aa.bb.cc.dddd
*   ...
*/
PRINTER_TABLE_ATTRIB ShowFirmwareTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                            //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                     //COLUMN MAX STR WIDTH
      DS_DIMM_FW_PATH PATH_KEY_DELIM DIMM_ID_STR              //COLUMN DATA PATH
    },
    {
      ACTIVE_FW_VERSION_STR,                                  //COLUMN HEADER
      ACTIVE_FW_VERSION_MAX_STR_WIDTH,                        //COLUMN MAX STR WIDTH
      DS_DIMM_FW_PATH PATH_KEY_DELIM ACTIVE_FW_VERSION_STR    //COLUMN DATA PATH
    },
    {
      STAGED_FW_VERSION_STR,                                  //COLUMN HEADER
      STAGED_FW_VERSION_MAX_STR_WIDTH,                        //COLUMN MAX STR WIDTH
      DS_DIMM_FW_PATH PATH_KEY_DELIM STAGED_FW_VERSION_STR    //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowFirmwareDataSetAttribs =
{
  &ShowFirmwareListAttributes,
  &ShowFirmwareTableAttributes
};


/** Command syntax definition **/
struct Command ShowFirmwareCommand =
{
  SHOW_VERB,                                                        //!< verb
  {                                                                 //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"",HELP_ALL_DETAILS_TEXT, FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES,HELP_DISPLAY_DETAILS_TEXT, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  {                                                                 //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {FIRMWARE_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                          //!< properties
  L"Show information about firmware on one or more " PMEM_MODULES_STR L".",        //!< help
  ShowFirmware,                                                     //!< run function
  TRUE,                                                             //!< enable print control support
};

CHAR16 *mppAllowedShowFirmwareDisplayValues[] = {
  DIMM_ID_STR,
  ACTIVE_FW_VERSION_STR,
  STAGED_FW_VERSION_STR,
  STAGED_FW_ACTIVATABLE_STR,
  FW_UPDATE_STATUS_STR,
  FW_IMAGE_MAX_SIZE_STR,
  QUIESCE_REQUIRED_STR,
  ACTIVATION_TIME_STR
};

/**
  Register the show firmware command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowFirmwareCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();
  ReturnCode = RegisterCommand(&ShowFirmwareCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the show firmware command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
ShowFirmware(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pTargetValue = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  UINT32 Index = 0;
  CHAR16 *pFwUpdateStatusString = NULL;
  CHAR16 *pQuiesceRequiredString = NULL;
  CHAR16 *pStagedFwActivatableString = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;
  BOOLEAN ShowAll = FALSE;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  CHAR16 FwVerStr[FW_VERSION_LEN];
  UINT8 ManageableDimmsCount = 0;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL || pCmd->pPrintCtx == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  pDispOptions = AllocateZeroPool(sizeof(CMD_DISPLAY_OPTIONS));
  if (NULL == pDispOptions) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowFirmwareDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowFirmwareDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  ShowAll = (!pDispOptions->AllOptionSet && !pDispOptions->DisplayOptionSet) || pDispOptions->AllOptionSet;

  /** Make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  CHECK_RESULT(GetAllDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_FW_IMAGE_INFO, &pDimms, &DimmCount), Finish);

  /** Initialize status structure **/
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  /** Get DCPMMs list **/
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  /** Print table **/
  for (Index = 0; Index < DimmCount; Index++) {
    if (DimmIdsCount > 0) {
      if (!ContainUint(pDimmIds, DimmIdsCount, pDimms[Index].DimmID)) {
        continue;
      }
    }

    if (pDimms[Index].ManageabilityState != MANAGEMENT_VALID_CONFIG) {
      continue;
    }
    ManageableDimmsCount++;

    ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
      DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_FW_INDEX_PATH, Index);

    /** DimmID **/
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

    /** ActiveFwVersion **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ACTIVE_FW_VERSION_STR))) {
      ConvertFwVersion(FwVerStr, pDimms[Index].FwVer.FwProduct, pDimms[Index].FwVer.FwRevision,
        pDimms[Index].FwVer.FwSecurityVersion, pDimms[Index].FwVer.FwBuild);

      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ACTIVE_FW_VERSION_STR, FwVerStr);
    }

    /** StagedFwVersion **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, STAGED_FW_VERSION_STR))) {
      ConvertFwVersion(FwVerStr, pDimms[Index].StagedFwVersion.FwProduct, pDimms[Index].StagedFwVersion.FwRevision,
        pDimms[Index].StagedFwVersion.FwSecurityVersion, pDimms[Index].StagedFwVersion.FwBuild);

      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, STAGED_FW_VERSION_STR, FwVerStr);
    }

    /** StagedFwActivatable **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, STAGED_FW_ACTIVATABLE_STR))) {
      pStagedFwActivatableString = StagedFwActivatableToString(gNvmDimmCliHiiHandle, pDimms[Index].StagedFwActivatable);
      if (pStagedFwActivatableString == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
        goto Finish;
      }
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, STAGED_FW_ACTIVATABLE_STR, pStagedFwActivatableString);
      FREE_POOL_SAFE(pStagedFwActivatableString);
    }

    /** FwUpdateStatus **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FW_UPDATE_STATUS_STR))) {
      pFwUpdateStatusString = LastFwUpdateStatusToString(gNvmDimmCliHiiHandle, pDimms[Index].LastFwUpdateStatus);
      if (pFwUpdateStatusString == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
        goto Finish;
      }
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_UPDATE_STATUS_STR, pFwUpdateStatusString);
      FREE_POOL_SAFE(pFwUpdateStatusString);
    }

    /** FwImageMaxSize **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FW_IMAGE_MAX_SIZE_STR))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, FW_IMAGE_MAX_SIZE_STR, FORMAT_UINT32, pDimms[Index].FWImageMaxSize);
    }

    /** Quiesce Required **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, QUIESCE_REQUIRED_STR))) {
      pQuiesceRequiredString = QuiesceRequiredToString(gNvmDimmCliHiiHandle, pDimms[Index].QuiesceRequired);
      if (pQuiesceRequiredString == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
        goto Finish;
      }
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, QUIESCE_REQUIRED_STR, pQuiesceRequiredString);
      FREE_POOL_SAFE(pQuiesceRequiredString);
    }

    /** Activation Time **/
    if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ACTIVATION_TIME_STR))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ACTIVATION_TIME_STR, FORMAT_UINT32, pDimms[Index].ActivationTime);
    }
  }

  if (ManageableDimmsCount == 0) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_MANAGEABLE_DIMMS);
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowFirmwareDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_CMD_DISPLAY_OPTIONS_SAFE(pDispOptions);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
