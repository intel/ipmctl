/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "LoadCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <FwUtility.h>
#include <NvmInterface.h>
#include <Library/UefiShellLib/UefiShellLib.h>
#include <Protocol/EfiShell.h>
#include "NvmDimmCli.h"
#include "Common.h"

#define PROGRESS_EVENT_TIMEOUT    EFI_TIMER_PERIOD_SECONDS(1)
#define PRINT_PRIORITY            8

 /**
   Command syntax definition
 **/
struct Command LoadCommand =
{
  LOAD_VERB,                                                          //!< verb
  {                                                                   //!< options
    {EXAMINE_OPTION_SHORT, EXAMINE_OPTION, L"", EXAMINE_OPTION_HELP, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", FORCE_OPTION_HELP, FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, TRUE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
#ifndef OS_BUILD
    ,{ L"", RECOVER_OPTION, L"", HELP_TEXT_FLASH_SPI, FALSE, ValueOptional }
#endif
},
  {                                                                   //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Update the firmware on one or more DIMMs",                        //!< help
  Load                                                                //!< run function
};

/**
  Register the load command
**/
EFI_STATUS
RegisterLoadCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&LoadCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the load command
  @param[in] pCmd the command structure that contains the user input data.

  @retval EFI_SUCCESS if everything went OK - including the firmware load process.
  @retval EFI_INVALID_PARAMETER if the user input is invalid or the file validation fails
  @retval EFI_UNSUPPORTED if the driver is not loaded or there are no DCPMMs in the system.
  @retval EFI_NOT_FOUND if there is no DIMM with the user specified PID
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
EFI_STATUS
Load(
  IN     struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pFileName = NULL;
  CHAR16 *pRelativeFileName = NULL;
  CHAR16 *pTargetValue = NULL;
  CONST CHAR16 *pWorkingDirectory = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN Examine = FALSE;
  BOOLEAN Force = FALSE;
  FW_IMAGE_INFO *pFwImageInfo = NULL;
  BOOLEAN Confirmation = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  EFI_EVENT ProgressEvent = NULL;
  BOOLEAN FlashSPI = FALSE;
  CHAR16 *pOptionsValue = NULL;
  UINT32 StagingFailures = 0;
  BOOLEAN Recovery = FALSE;
  DIMM_INFO *pDimmTargets = NULL;
  UINT16 *pDimmIds = NULL;
  UINT16 *pDimmTargetIds = NULL;
  UINT32 NonFunctionalDimmCount = 0;
  UINT32 FunctionalDimmCount = 0;
  DIMM_INFO *pAllDimms = NULL;
  DIMM_INFO *pNonFunctionalDimms = NULL;
  DIMM_INFO *pFunctionalDimms = NULL;
  DIMM_INFO *pCandidateList = NULL;
  UINT32 DimmTotalCount = 0;
  UINT32 DimmTargetCount = 0;
  UINT32 CandidateListCount = 0;
  BOOLEAN TargetsIsNewList = FALSE;

#ifndef OS_BUILD
  EFI_SHELL_PROTOCOL *pEfiShell = NULL;
  UINTN HandlesCount = 0;
  EFI_HANDLE *pHandles = NULL;
#endif


  NVDIMM_ENTRY();
  SetDisplayInfo(L"LoadFw", ResultsView, NULL);

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // check options
  pFileName = getOptionValue(pCmd, SOURCE_OPTION);
  if (pFileName == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_WRONG_FILE_PATH);
    goto Finish;
  }

  if (containsOption(pCmd, EXAMINE_OPTION) && containsOption(pCmd, EXAMINE_OPTION_SHORT)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_EXAMINE_USED_TOGETHER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (containsOption(pCmd, FORCE_OPTION) && containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPTIONS_FORCE_USED_TOGETHER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Recovery = containsOption(pCmd, RECOVER_OPTION);
  Examine = containsOption(pCmd, EXAMINE_OPTION) || containsOption(pCmd, EXAMINE_OPTION_SHORT);
  Force = containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT);
  //check for the kind of recovery this might be
  pOptionsValue = getOptionValue(pCmd, RECOVER_OPTION);
  if (pOptionsValue != NULL) {
    if (StrICmp(pOptionsValue, RECOVER_OPTION_FLASH_SPI) == 0) {
      FlashSPI = TRUE;
    }
    else if (StrLen(pOptionsValue) > 0) {
      ReturnCode = EFI_INVALID_PARAMETER;
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_RECOVER);
      goto Finish;
    }
  }

  /*Get the count of the non-functional dimms*/
  DimmTotalCount = 0;
  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimmCount(pNvmDimmConfigProtocol, &NonFunctionalDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (NonFunctionalDimmCount > 0) {
    pNonFunctionalDimms = AllocateZeroPool(sizeof(*pNonFunctionalDimms) * NonFunctionalDimmCount);
    if (pNonFunctionalDimms == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, NonFunctionalDimmCount, pNonFunctionalDimms);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  DimmTotalCount += NonFunctionalDimmCount;

  /*Get the list of functional and non-functional dimms*/
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pFunctionalDimms, &FunctionalDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NOT_FOUND) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    else {
      goto Finish;
    }
  }

  DimmTotalCount += FunctionalDimmCount;

  if (DimmTotalCount == 0) {
    ReturnCode = EFI_NOT_STARTED;
    Print(FORMAT_STR_NL, CLI_INFO_NO_DIMMS);
    goto Finish;
  }

  /*Identify the candidate list*/
  pCandidateList = pFunctionalDimms;
  CandidateListCount = FunctionalDimmCount;
  if (Recovery)
  {
    if (FlashSPI)
    {
      pCandidateList = pNonFunctionalDimms;
      CandidateListCount = NonFunctionalDimmCount;
    }
    else
    {
      CandidateListCount = FunctionalDimmCount + NonFunctionalDimmCount;
      pAllDimms = AllocateZeroPool(sizeof(*pAllDimms) * CandidateListCount);
      if (pAllDimms == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
        goto Finish;
      }

      for (Index = 0; Index < FunctionalDimmCount; Index++) {
        pAllDimms[Index] = pFunctionalDimms[Index];
      }

      for (Index = 0; Index < NonFunctionalDimmCount; Index++) {
        pAllDimms[FunctionalDimmCount + Index] = pNonFunctionalDimms[Index];
      }

      pCandidateList = pAllDimms;
    }
  }
  /*If it is FlashSPI and there are no nonfunctional dimms */
  if (FlashSPI && CandidateListCount == 0) {
    ReturnCode = EFI_NOT_FOUND;
    Print(FORMAT_STR_NL, CLI_INFO_NO_NON_FUNCTIONAL_DIMMS);
    goto Finish;
  }
  /*Screen for user specific IDs*/
  pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
  if (pTargetValue != NULL && StrLen(pTargetValue) > 0) {

    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pCandidateList, CandidateListCount, &pDimmIds, &DimmTargetCount);
    if (pDimmIds == NULL) {
      goto Finish;
    }

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }

    TargetsIsNewList = TRUE;
    pDimmTargets = AllocateZeroPool(sizeof(*pDimmTargets) * DimmTargetCount);
    pDimmTargetIds = AllocateZeroPool(sizeof(*pDimmTargetIds) * DimmTargetCount);
    if (pDimmTargets == NULL || pDimmTargetIds == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }


    for (Index = 0; Index < DimmTargetCount; Index++) {
      for (Index2 = 0; Index2 < CandidateListCount; Index2++) {
        if (pCandidateList[Index2].DimmID == pDimmIds[Index]) {
          pDimmTargets[Index] = pCandidateList[Index2];
          pDimmTargetIds[Index] = pCandidateList[Index2].DimmID;
          break;
        }
      }
    }
  } else {
    DimmTargetCount = CandidateListCount;
    pDimmTargets = pCandidateList;
    pDimmTargetIds = AllocateZeroPool(sizeof(*pDimmTargetIds) * DimmTargetCount);
    if (pDimmTargetIds == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    for (Index = 0; Index < DimmTargetCount; Index++) {
      pDimmTargetIds[Index] = pDimmTargets[Index].DimmID;
    }
  }

  if (!Recovery) {
    if (!AllDimmsInListAreManageable(pDimmTargets, DimmTargetCount, pDimmTargetIds, DimmTargetCount)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  } else {
    Print(CLI_RECOVER_DIMM_PROMPT_STR);
    for (Index = 0; Index < DimmTargetCount; Index++) {
      ReturnCode = GetPreferredDimmIdAsString(pDimmTargets[Index].DimmHandle, pDimmTargets[Index].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
      Print(L"%s ", DimmStr);
    }

    Print(L"\n");
  }


  if (Recovery && !Examine) {
    // Warn about disabling TSOD for SMBUS operations
    Print(CLI_RECOVER_DIMM_TSOD_REMINDER_STR);

    ReturnCode = PromptYesNo(&Confirmation);
    if (EFI_ERROR(ReturnCode) || !Confirmation) {
      ReturnCode = EFI_NOT_STARTED;
      goto Finish;
    }
  }


  /**
    In this case the user could have typed "FS0:\..."
    We are searching for the file on all FS so we need to remove the first chars until we have a "\"
  **/

#ifdef OS_BUILD
  pRelativeFileName = pFileName;
#else
  ReturnCode = GetRelativePath(pFileName, &pRelativeFileName);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, &gEfiShellProtocolGuid, NULL, &HandlesCount, &pHandles);

  if (!EFI_ERROR(ReturnCode) && HandlesCount < MAX_SHELL_PROTOCOL_HANDLES) {
    ReturnCode = gBS->OpenProtocol(pHandles[0], &gEfiShellProtocolGuid, (VOID *)&pEfiShell, NULL, NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (!EFI_ERROR(ReturnCode)) {
      pWorkingDirectory = pEfiShell->GetCurDir(NULL);
      if (pWorkingDirectory == NULL) {
        NVDIMM_WARN("Error while getting the Working Directory.");
      }
    } else {
      NVDIMM_WARN("Error while opening the shell protocol. Code: " FORMAT_EFI_STATUS "", ReturnCode);
    }
  } else {
    NVDIMM_WARN("Error while opening the shell protocol. Code: " FORMAT_EFI_STATUS "", ReturnCode);
    /**
      We can still try to open the file. If it is in the root directory, we will be able to open it.
    **/
}
#endif

  pFwImageInfo = AllocateZeroPool(sizeof(*pFwImageInfo));
  if (pFwImageInfo == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (Recovery && !Examine) {
    pCommandStatus->GeneralStatus = NVM_SUCCESS;
    ReturnCode = EFI_SUCCESS;

    // Create callback that will print progress
    gBS->CreateEvent((EVT_TIMER | EVT_NOTIFY_SIGNAL), PRINT_PRIORITY, PrintProgress, pCommandStatus, &ProgressEvent);
    gBS->SetTimer(ProgressEvent, TimerPeriodic, PROGRESS_EVENT_TIMEOUT);

    for (Index = 0; Index < DimmTargetCount; Index++) {
      ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
        (CHAR16 *)pWorkingDirectory, Examine, FALSE, TRUE, FlashSPI, pFwImageInfo, pCommandStatus);

      if (!EFI_ERROR(ReturnCode)) {
        ReturnCode = GetPreferredDimmIdAsString(pDimmTargets[Index].DimmHandle, pDimmTargets[Index].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
        Print(L"\rLoad firmware on DIMM (" FORMAT_STR L") Progress: 100%%", DimmStr);
        ReturnCode = EFI_SUCCESS;
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_SUCCESS_FW_RESET_REQUIRED, TRUE);
      }
    }

    gBS->CloseEvent(ProgressEvent);
    Print(L"\n");

  } else { // Not Recovery or this is Examine
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    if (!Examine) {
      Print(L"Starting update on %d dimm(s)...\n", DimmTargetCount);
    }

    for (Index = 0; Index < DimmTargetCount; Index++) {

      pCommandStatus->GeneralStatus = NVM_SUCCESS; //ensure that only the last error gets reported
      ReturnCode = EFI_SUCCESS;

      ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
        (CHAR16 *)pWorkingDirectory, Examine, Force, Recovery, FlashSPI, pFwImageInfo, pCommandStatus);

      if (Examine) {
        continue;
      }

      if (pCommandStatus->GeneralStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {

        ReturnCode = GetDimmHandleByPid(pDimmTargetIds[Index], pDimmTargets, DimmTargetCount, &DimmHandle, &DimmIndex);
        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_DBG("Failed to get dimm handle");
          pCommandStatus->GeneralStatus = NVM_ERR_DIMM_NOT_FOUND;
          goto Finish;
        }

        ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimmTargets[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCode)) {
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_ERR_INVALID_PARAMETER, TRUE);
          goto Finish;
        }

        Print(CLI_DOWNGRADE_PROMPT L"\n", DimmStr);
        ReturnCode = PromptYesNo(&Confirmation);
        if (EFI_ERROR(ReturnCode) || !Confirmation) {
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED, TRUE);
          continue;
        }

        ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
          (CHAR16 *)pWorkingDirectory, FALSE, TRUE, FALSE, FALSE, pFwImageInfo, pCommandStatus);
        if (EFI_ERROR(ReturnCode)) {
          continue;
        }
      } else if (EFI_ERROR(ReturnCode)) {
        continue;
      }

      ReturnCode = PollLongOpStatus(pNvmDimmConfigProtocol, pDimmTargetIds[Index],
        FW_UPDATE_OPCODE, FW_UPDATE_SUBOPCODE, LONG_OP_FW_UPDATE_TIMEOUT);
      if (EFI_ERROR(ReturnCode)) {
        if (ReturnCode == (EFI_INCOMPATIBLE_VERSION) || (ReturnCode == EFI_UNSUPPORTED)) {
          NVDIMM_DBG("Long operation status for FwUpdate not supported");
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_SUCCESS_FW_RESET_REQUIRED, TRUE);
        } else {
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_ERR_FIRMWARE_FAILED_TO_STAGE, TRUE);
          StagingFailures++;
        }
      } else {
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_SUCCESS_FW_RESET_REQUIRED, TRUE);
      }
    }
  } //for loop


  if (Examine) {
    if (pFwImageInfo != NULL) {

      //only print non 0.0.0.0 versions...
      if (pFwImageInfo->ImageVersion.ProductNumber.Version != 0 ||
        pFwImageInfo->ImageVersion.RevisionNumber.Version != 0 ||
        pFwImageInfo->ImageVersion.SecurityRevisionNumber.Version != 0 ||
        pFwImageInfo->ImageVersion.BuildNumber.Build != 0) {
        Print(L"(" FORMAT_STR L"): %02d.%02d.%02d.%04d\n",
          pFileName,
          pFwImageInfo->ImageVersion.ProductNumber.Version,
          pFwImageInfo->ImageVersion.RevisionNumber.Version,
          pFwImageInfo->ImageVersion.SecurityRevisionNumber.Version,
          pFwImageInfo->ImageVersion.BuildNumber.Build);
      }
    }
    else {
      Print(L"(" FORMAT_STR L")" FORMAT_STR_NL, pFileName, CLI_ERR_VERSION_RETRIEVE);
    }
  }

Finish:
  DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);

  if (StagingFailures > 0) {
    pCommandStatus->GeneralStatus = NVM_ERR_FIRMWARE_FAILED_TO_STAGE;
  }

  if (ReturnCode == EFI_SUCCESS && pCommandStatus->GeneralStatus != NVM_SUCCESS) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  }

  if (Examine && pCommandStatus->GeneralStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {
    ReturnCode = EFI_SUCCESS;
  }

  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pFileName);
  FREE_POOL_SAFE(pFwImageInfo);
  FREE_POOL_SAFE(pNonFunctionalDimms);
  FREE_POOL_SAFE(pFunctionalDimms);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pAllDimms);
  FREE_POOL_SAFE(pOptionsValue);
  FREE_POOL_SAFE(pDimmTargetIds);

  if (TargetsIsNewList) {
    FREE_POOL_SAFE(pDimmTargets);
  }

  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pOptionsValue);
  return ReturnCode;
}
