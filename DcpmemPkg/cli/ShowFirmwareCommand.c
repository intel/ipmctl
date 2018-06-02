/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Library/ShellLib.h>
#include "ShowFirmwareCommand.h"
#include "NvmDimmCli.h"

#define DIMM_ID_ATTR                        L"DimmID"
#define ACTIVE_FW_VER_ATTR                  L"ActiveFWVersion"
#define ACTIVE_FW_TYPE_ATTR                 L"ActiveFWType"
#define ACTIVE_FW_COMMIT_ID_ATTR            L"ActiveFWCommitID"
#define ACTIVE_FW_BUILD_CONFIGURATION_ATTR  L"ActiveFWBuildConfiguration"
#define STAGED_FW_VER_ATTR                  L"StagedFWVersion"
#define FW_UPDATE_STATUS_ATTR               L"FWUpdateStatus"

/** Command syntax definition **/
struct Command ShowFirmwareCommand =
{
  SHOW_VERB,                                                        //!< verb
  {                                                                 //!< options
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired}
  },
  {                                                                 //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {FIRMWARE_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                          //!< properties
  L"Show information about firmware on one or more DCPMEM DIMMs.",  //!< help
  ShowFirmware                                                      //!< run function
};

CHAR16 *mppAllowedShowFirmwareDisplayValues[] = {
  DIMM_ID_ATTR,
  ACTIVE_FW_VER_ATTR,
  ACTIVE_FW_TYPE_ATTR,
  ACTIVE_FW_COMMIT_ID_ATTR,
  ACTIVE_FW_BUILD_CONFIGURATION_ATTR,
  STAGED_FW_VER_ATTR,
  FW_UPDATE_STATUS_ATTR
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;

  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  CHAR16 *pOptionsValues = NULL;
  CHAR16 *pTargetValue = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  UINT32 Index = 0;
  CHAR16 *pFwType = NULL;
  CHAR16 *pFwUpdateStatusString = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** Make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_FW_IMAGE_INFO, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Initialize status structure **/
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  /** Get DCPMEM DIMMs list **/
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  /** If no dimm IDs are specified get IDs from all dimms **/
  if (DimmIdsCount == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(&DimmIdsCount, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    if (DimmIdsCount == 0) {
      Print(FORMAT_STR_NL, CLI_INFO_NO_MANAGEABLE_DIMMS);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
  }

  /** Checking ALL option **/
  if (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT)) {
    AllOptionSet = TRUE;
  }

  /** Checking DISPLAY option **/
  if (containsOption(pCmd, DISPLAY_OPTION) || containsOption(pCmd, DISPLAY_OPTION_SHORT)) {
    if ((pOptionsValues = getOptionValue(pCmd, DISPLAY_OPTION)) == NULL) {
      pOptionsValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    }
    DisplayOptionSet = TRUE;
    /** Check that the display parameters are correct (when display option is set) **/
    ReturnCode = CheckDisplayList(pOptionsValues, mppAllowedShowFirmwareDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowFirmwareDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  /** Make sure they didn't specify both the ALL and DISPLAY options **/
  if (AllOptionSet && DisplayOptionSet) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    goto Finish;
  }

  /** Display default values for show firmware **/
  if (!AllOptionSet && !DisplayOptionSet) {
    /** Print table header **/
    Print(FORMAT_FW_UPDATE_HEADER,
        DIMM_ID_ATTR,
        ACTIVE_FW_VER_ATTR,
        STAGED_FW_VER_ATTR);

    /** Print table **/
    for (Index = 0; Index < DimmCount; Index++) {
      if (!ContainUint(pDimmIds, DimmIdsCount, pDimms[Index].DimmID)) {
        continue;
      }

      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      Print(FORMAT_FW_UPDATE_VERSION,
          DimmStr,
          pDimms[Index].FwVer.FwProduct,
          pDimms[Index].FwVer.FwRevision,
          pDimms[Index].FwVer.FwSecurityVersion,
          pDimms[Index].FwVer.FwBuild);
      if (IsFwStaged(pDimms[Index].StagedFwVersion)) {
        Print(L"%02d.%02d.%02d.%04d\n",
            pDimms[Index].StagedFwVersion.FwProduct,
            pDimms[Index].StagedFwVersion.FwRevision,
            pDimms[Index].StagedFwVersion.FwSecurityVersion,
            pDimms[Index].StagedFwVersion.FwBuild);
      } else {
        Print(L"N/A\n");
      }
    }
  } else {
    for (Index = 0; Index < DimmCount; Index++) {
      if (!ContainUint(pDimmIds, DimmIdsCount, pDimms[Index].DimmID)) {
        continue;
      }

      ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_ATTR, DimmStr);

      if (AllOptionSet || (DisplayOptionSet && ContainsValue(pOptionsValues, ACTIVE_FW_VER_ATTR))) {
        Print(L"   " FORMAT_STR L"=%02d.%02d.%02d.%04d\n", ACTIVE_FW_VER_ATTR,
            pDimms[Index].FwVer.FwProduct,
            pDimms[Index].FwVer.FwRevision,
            pDimms[Index].FwVer.FwSecurityVersion,
            pDimms[Index].FwVer.FwBuild);
      }

      if (AllOptionSet || (DisplayOptionSet && ContainsValue(pOptionsValues, ACTIVE_FW_TYPE_ATTR))) {
        pFwType = FirmwareTypeToString(pDimms[Index].ActiveFwType);
        if (pFwType == NULL) {
          Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
          ReturnCode = EFI_OUT_OF_RESOURCES;
          goto Finish;
        }
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, ACTIVE_FW_TYPE_ATTR, pFwType);
        FREE_POOL_SAFE(pFwType);
      }

      if (AllOptionSet || (DisplayOptionSet && ContainsValue(pOptionsValues, ACTIVE_FW_COMMIT_ID_ATTR))) {
        if (pDimms[Index].ActiveFwCommitId[0] == L'\0') {
          Print(L"   " FORMAT_STR L"=N/A\n", ACTIVE_FW_COMMIT_ID_ATTR);
        } else {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, ACTIVE_FW_COMMIT_ID_ATTR, pDimms[Index].ActiveFwCommitId);
        }
      }

      if (AllOptionSet || (DisplayOptionSet && ContainsValue(pOptionsValues, ACTIVE_FW_BUILD_CONFIGURATION_ATTR))) {
        if (pDimms[Index].ActiveFwBuild[0] == L'\0') {
          Print(L"   " FORMAT_STR L"=N/A\n", ACTIVE_FW_BUILD_CONFIGURATION_ATTR);
        } else {
          Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, ACTIVE_FW_BUILD_CONFIGURATION_ATTR, pDimms[Index].ActiveFwBuild);
         }
      }

      if (AllOptionSet || (DisplayOptionSet && ContainsValue(pOptionsValues, STAGED_FW_VER_ATTR))) {
        if (IsFwStaged(pDimms[Index].StagedFwVersion)) {
          Print(L"   " FORMAT_STR L"=%02d.%02d.%02d.%04d\n", STAGED_FW_VER_ATTR,
              pDimms[Index].StagedFwVersion.FwProduct,
              pDimms[Index].StagedFwVersion.FwRevision,
              pDimms[Index].StagedFwVersion.FwSecurityVersion,
              pDimms[Index].StagedFwVersion.FwBuild);
        } else {
          Print(L"   " FORMAT_STR L"=N/A\n", STAGED_FW_VER_ATTR);
        }
      }

      if (AllOptionSet || (DisplayOptionSet && ContainsValue(pOptionsValues, FW_UPDATE_STATUS_ATTR))) {
        pFwUpdateStatusString = LastFwUpdateStatusToString(gNvmDimmCliHiiHandle, pDimms[Index].LastFwUpdateStatus);
        if (pFwUpdateStatusString == NULL) {
          Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
          ReturnCode = EFI_OUT_OF_RESOURCES;
          goto Finish;
        }
        Print(FORMAT_SPACE_SPACE_SPACE_STR_EQ_STR_NL, FW_UPDATE_STATUS_ATTR, pFwUpdateStatusString);
        FREE_POOL_SAFE(pFwUpdateStatusString);
      }
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
