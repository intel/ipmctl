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

 /**
   Command syntax definition
 **/
struct Command LoadCommand =
{
  LOAD_VERB,                                                          //!< verb
  {                                                                   //!< options
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, L"Firmware Image required to use Firmware Update.", FALSE, ValueRequired},
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {EXAMINE_OPTION_SHORT, EXAMINE_OPTION, L"", EXAMINE_OPTION_HELP, HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", FORCE_OPTION_HELP, HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty}

#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueRequired }
#endif
#ifndef OS_BUILD
    ,{ L"", RECOVER_OPTION, L"", HELP_TEXT_FLASH_SPI,HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueOptional }
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
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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
  volatile UINT32 Index = 0;
  volatile UINT32 Index2 = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  EFI_EVENT ProgressEvent = NULL;
  BOOLEAN FlashSPI = FALSE;
  CHAR16 *pOptionsValue = NULL;
  BOOLEAN Recovery = FALSE;
  DIMM_INFO *pDimmTargets = NULL;
  UINT16 *pDimmIds = NULL;
  UINT16 *pDimmTargetIds = NULL;
  UINT32 NonFunctionalDimmCount = 0;
  UINT32 FunctionalDimmCount = 0;
  UINT32 StagedFwUpdates = 0;
  DIMM_INFO *pAllDimms = NULL;
  DIMM_INFO *pNonFunctionalDimms = NULL;
  DIMM_INFO *pFunctionalDimms = NULL;
  DIMM_INFO *pCandidateList = NULL;
  UINT32 DimmTotalCount = 0;
  UINT32 DimmTargetsNum = 0;
  UINT32 CandidateListCount = 0;
  BOOLEAN TargetsIsNewList = FALSE;
  BOOLEAN Confirmation = 0;
  EFI_STATUS ReturnCodes[MAX_DIMMS];
  NVM_STATUS NvmCodes[MAX_DIMMS];
  NVM_STATUS generalNvmStatus = NVM_SUCCESS;

#ifndef OS_BUILD
  EFI_SHELL_PROTOCOL *pEfiShell = NULL;
  UINTN HandlesCount = 0;
  EFI_HANDLE *pHandles = NULL;
#endif


  NVDIMM_ENTRY();
  SetDisplayInfo(L"LoadFw", ResultsView, NULL);

  for (Index = 0; Index < MAX_DIMMS; Index++) {
    ReturnCodes[Index] = EFI_SUCCESS;
    NvmCodes[Index] = NVM_SUCCESS;
  }

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto FinishNoCommandStatus;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto FinishNoCommandStatus;
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

    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pCandidateList, CandidateListCount, &pDimmIds, &DimmTargetsNum);
    if (pDimmIds == NULL) {
      goto Finish;
    }

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }

    TargetsIsNewList = TRUE;
    pDimmTargets = AllocateZeroPool(sizeof(*pDimmTargets) * DimmTargetsNum);
    pDimmTargetIds = AllocateZeroPool(sizeof(*pDimmTargetIds) * DimmTargetsNum);
    if (pDimmTargets == NULL || pDimmTargetIds == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }


    for (Index = 0; Index < DimmTargetsNum; Index++) {
      for (Index2 = 0; Index2 < CandidateListCount; Index2++) {
        if (pCandidateList[Index2].DimmID == pDimmIds[Index]) {
          pDimmTargets[Index] = pCandidateList[Index2];
          pDimmTargetIds[Index] = pCandidateList[Index2].DimmID;
          break;
        }
      }
    }
  } else {
    DimmTargetsNum = CandidateListCount;
    pDimmTargets = pCandidateList;
    pDimmTargetIds = AllocateZeroPool(sizeof(*pDimmTargetIds) * DimmTargetsNum);
    if (pDimmTargetIds == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    for (Index = 0; Index < DimmTargetsNum; Index++) {
      pDimmTargetIds[Index] = pDimmTargets[Index].DimmID;
    }
  }

  if (!Recovery) {
    if (!AllDimmsInListAreManageable(pDimmTargets, DimmTargetsNum, pDimmTargetIds, DimmTargetsNum)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
    if (!AllDimmsInListInSupportedConfig(pDimmTargets, DimmTargetsNum, pDimmTargetIds, DimmTargetsNum)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      Print(FORMAT_STR_NL, CLI_ERR_POPULATION_VIOLATION);
      goto Finish;
    }
  } else {
    Print(CLI_RECOVER_DIMM_PROMPT_STR);
    for (Index = 0; Index < DimmTargetsNum; Index++) {
      ReturnCode = GetPreferredDimmIdAsString(pDimmTargets[Index].DimmHandle, pDimmTargets[Index].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
      Print(L"%s ", DimmStr);
    }

    Print(L"\n");
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

  pCommandStatus->ObjectType = ObjectTypeDimm;
  if (Recovery && !Examine) {

    // Create callback that will print progress
    gBS->CreateEvent((EVT_TIMER | EVT_NOTIFY_SIGNAL), PRINT_PRIORITY, PrintProgress, pCommandStatus, &ProgressEvent);
    gBS->SetTimer(ProgressEvent, TimerPeriodic, PROGRESS_EVENT_TIMEOUT);

    for (Index = 0; Index < DimmTargetsNum; Index++) {
      ReturnCodes[Index] = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
        (CHAR16 *)pWorkingDirectory, Examine, FALSE, TRUE, FlashSPI, pFwImageInfo, pCommandStatus);
      NvmCodes[Index] = pCommandStatus->GeneralStatus;

      if (!EFI_ERROR(ReturnCodes[Index])) {
        GetPreferredDimmIdAsString(pDimmTargets[Index].DimmHandle, pDimmTargets[Index].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
        Print(L"\rLoad firmware on DIMM " FORMAT_STR L" Progress: 100%%", DimmStr);
        ReturnCodes[Index] = EFI_SUCCESS;
        NvmCodes[Index] = NVM_SUCCESS_FW_RESET_REQUIRED;
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NVM_SUCCESS_FW_RESET_REQUIRED, TRUE);
      }
    }

    gBS->CloseEvent(ProgressEvent);
    Print(L"\n");

  } else { // Not Recovery or this is Examine
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    if (!Examine) {
      Print(L"Starting update on %d dimm(s)...\n", DimmTargetsNum);
    }

    for (Index = 0; Index < DimmTargetsNum; Index++) {

      pCommandStatus->GeneralStatus = NVM_SUCCESS; //ensure that only the last error gets reported

      //if the FW is already staged and this isn't an examine operation, the outcome is already known
      if (FALSE == Examine && TRUE == FwHasBeenStaged(pCmd, pNvmDimmConfigProtocol, pDimmTargets[Index].DimmID)) {
        pCommandStatus->GeneralStatus = NVM_ERR_FIRMWARE_ALREADY_LOADED;
        NvmCodes[Index] = pCommandStatus->GeneralStatus;
        ReturnCodes[Index] = MatchCliReturnCode(NvmCodes[Index]);
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NvmCodes[Index], TRUE);
        continue;
      }

      ReturnCodes[Index] = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
          (CHAR16 *)pWorkingDirectory, Examine, Force, Recovery, FlashSPI, pFwImageInfo, pCommandStatus);
      NvmCodes[Index] = pCommandStatus->GeneralStatus;

      if (Examine) {
        if (NvmCodes[Index] == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {
          ReturnCodes[Index] = EFI_SUCCESS;
          NvmCodes[Index] = NVM_SUCCESS;
        }
        continue;
      }

      if (pCommandStatus->GeneralStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {

        ReturnCodes[Index] = GetDimmHandleByPid(pDimmTargetIds[Index], pDimmTargets, DimmTargetsNum, &DimmHandle, &DimmIndex);
        if (EFI_ERROR(ReturnCodes[Index])) {
          NVDIMM_DBG("Failed to get dimm handle");
          NvmCodes[Index] = NVM_ERR_DIMM_NOT_FOUND;
          goto Finish;
        }

        ReturnCodes[Index] = GetPreferredDimmIdAsString(DimmHandle, pDimmTargets[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCodes[Index])) {
          NvmCodes[Index] = NVM_ERR_INVALID_PARAMETER;
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NvmCodes[Index], TRUE);
          goto Finish;
        }

        Print(CLI_DOWNGRADE_PROMPT L"\n", DimmStr);
        ReturnCodes[Index] = PromptYesNo(&Confirmation);
        if (EFI_ERROR(ReturnCodes[Index]) || !Confirmation) {
          NvmCodes[Index] = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
          ReturnCodes[Index] = EFI_ABORTED;
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NvmCodes[Index], TRUE);
          continue;
        }

        ReturnCodes[Index] = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
          (CHAR16 *)pWorkingDirectory, FALSE, TRUE, FALSE, FALSE, pFwImageInfo, pCommandStatus);
        if (EFI_ERROR(ReturnCodes[Index])) {
          continue;
        }
      } else if (EFI_ERROR(ReturnCodes[Index])) {
        continue;
      }

      StagedFwUpdates++;
    } //for loop

    if (Examine) {
      if (pFwImageInfo != NULL) {

        //only print non 0.0.0.0 versions...
        if (pFwImageInfo->ImageVersion.ProductNumber.Version != 0 ||
          pFwImageInfo->ImageVersion.RevisionNumber.Version != 0 ||
          pFwImageInfo->ImageVersion.SecurityRevisionNumber.Version != 0 ||
          pFwImageInfo->ImageVersion.BuildNumber.Build != 0) {
          Print(FORMAT_STR L": %02d.%02d.%02d.%04d\n",
            pFileName,
            pFwImageInfo->ImageVersion.ProductNumber.Version,
            pFwImageInfo->ImageVersion.RevisionNumber.Version,
            pFwImageInfo->ImageVersion.SecurityRevisionNumber.Version,
            pFwImageInfo->ImageVersion.BuildNumber.Build);
        }
      }
      else {
        Print(FORMAT_STR L" " FORMAT_STR_NL, pFileName, CLI_ERR_VERSION_RETRIEVE);
      }
    } else if(StagedFwUpdates > 0) {
      /*
      At this point, all indications are that the FW is on the way to being staged.
      Loop until they all report a staged version
      */
      TempReturnCode = BlockForFwStage(pCmd, pCommandStatus, pNvmDimmConfigProtocol,
        &ReturnCodes[0], &NvmCodes[0], &pDimmTargets[0], DimmTargetsNum);
      if (EFI_ERROR(TempReturnCode)) {
        ReturnCode = TempReturnCode;
        goto Finish;
      }
    }
  }

  ReturnCode = EFI_SUCCESS;
  pCommandStatus->GeneralStatus = NVM_SUCCESS;
  for (Index = 0; Index < DimmTargetsNum; Index++) {
    TempReturnCode = GetDimmReturnCode(Examine, ReturnCodes[Index], NvmCodes[Index], &generalNvmStatus);

    //the 'EFI_ALREADY_STARTED' return code is considered a minor success
    //so if another error is present then prefer to report that error instead
    //(in other words, if 'ReturnCode' has been set to something other than 'EFI_ALREADY_STARTED' or 'EFI_SUCCESS'
    // then don't let it be altered)
    if (TempReturnCode == EFI_SUCCESS ||
        (TempReturnCode == EFI_ALREADY_STARTED && ReturnCode != EFI_SUCCESS && ReturnCode != TempReturnCode))
    {
      continue;
    }

    ReturnCode = TempReturnCode;
    pCommandStatus->GeneralStatus = generalNvmStatus;
  }

Finish:
  DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
  FreeCommandStatus(&pCommandStatus);

FinishNoCommandStatus:
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


/**
  For a given DIMM, this will evaluate what the return code should be
  @param[in] examine - if the examin flag was sent by the user
  @param[in] dimmReturnCode - the return code returned by the call to update the FW
  @param[in] dimmNvmStatus - the NVM status returned by the call to update the FW
  @param[out] generalNvmStatus - the NVM status to be applied to the general command status

  @retval the return code
**/
EFI_STATUS
GetDimmReturnCode(
  IN     BOOLEAN Examine,
  IN EFI_STATUS dimmReturnCode,
  IN NVM_STATUS dimmNvmStatus,
  OUT NVM_STATUS * pGeneralNvmStatus
) {

  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVM_STATUS NvmStatus = NVM_SUCCESS;
  NVDIMM_ENTRY();

  *pGeneralNvmStatus = NVM_SUCCESS;
  if (Examine && (dimmNvmStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED || dimmNvmStatus == NVM_SUCCESS)) {
    //these are both considered success when in examine
    goto Finish;
  }

  *pGeneralNvmStatus = dimmNvmStatus;
  NvmStatus = dimmNvmStatus;
Finish:
  ReturnCode = MatchCliReturnCode(NvmStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  For the lists provided, this will block until all dimms indicated in the StagedFwDimmIds report a non-zero staged FW image. This
  is intended to be run after a non-recovery (normal) FW update.

  @param[in] pCmd - The command object
  @param[in] pCommandStatus - The command status object
  @param[in] pNvmDimmConfigProtocol - The open config protocol
  @param[in] pReturnCodes - The current list of return codes for each DIMM
  @param[in] pNvmCodes - The current list of NVM codes for the FW work of each DIMM
  @param[in] pDimmTargets - The list of DIMMs for which a FW update was attempted
  @param[in] pDimmTargetsNum - The list length of the pDimmTargets list

  @retval EFI_SUCCESS - All dimms staged their fw as expected.
  @retval EFI_xxxx - One or more DIMMS did not stage their FW as expected.
**/
EFI_STATUS
BlockForFwStage(
  IN   struct Command *pCmd,
  IN   COMMAND_STATUS *pCommandStatus,
  IN   EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN   EFI_STATUS *pReturnCodes,
  IN   NVM_STATUS *pNvmCodes,
  IN   DIMM_INFO *pDimmTargets,
  IN   UINT32 pDimmTargetsNum
)
{
  EFI_STATUS      ReturnCode = EFI_SUCCESS;
  UINT32          CurrentStageCheck = 0;
  EFI_STATUS      FwStagedLongOpCodes[MAX_DIMMS];
  BOOLEAN         FwStageDone[MAX_DIMMS];
  UINT16          FwStagedPendingCount = 0;
  UINT16          FwStagedCompleteCount = 0;
  UINT8 CmdOpcode = 0;
  UINT8 CmdSubOpcode = 0;
  UINT8 PtUpdateFw = 0x09;
  UINT8 SubopUpdateFw = 0x0;
  EFI_STATUS LongOpEfiStatus = EFI_SUCCESS;
  UINT8 TransmitFwNeverHappened = 0xFF;
  UINT8 UnknownStatus = 0xFD;
  volatile UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pDimmTargetsNum > MAX_DIMMS) {
    NVDIMM_DBG("Number of target DIMMs is greater than max DIMMs number.");
    goto Finish;
  }

  //initialize
  for (Index = 0; Index < MAX_DIMMS; Index++) {
    FwStagedLongOpCodes[Index] = UnknownStatus;
    FwStageDone[Index] = FALSE;
  }

  //more initialize
  for (Index = 0; Index < pDimmTargetsNum; Index++) {
    if (pReturnCodes[Index] != EFI_SUCCESS) {
      //this DIMM didn't transmit an image. Don't expect a long op code
      FwStagedLongOpCodes[Index] = TransmitFwNeverHappened;
      FwStagedCompleteCount++;
      FwStageDone[Index] = TRUE;
      NVDIMM_DBG("Error with FW stage on dimm %d: an existing error exists - NvmCode=[%d], ReturnCode=[%d]",
        pDimmTargets[Index].DimmID, pNvmCodes[Index], pReturnCodes[Index]);
      continue;
    }

    FwStagedPendingCount++;
    ReturnCode = pNvmDimmConfigProtocol->GetLongOpStatus(pNvmDimmConfigProtocol, pDimmTargets[Index].DimmID,
      &CmdOpcode, &CmdSubOpcode, NULL, NULL, &LongOpEfiStatus);
    if (CmdOpcode == PtUpdateFw && CmdSubOpcode == SubopUpdateFw)
    {
      FwStagedLongOpCodes[Index] = LongOpEfiStatus;
    }
  }

  if (FwStagedPendingCount == 0) {
    NVDIMM_DBG("No DIMMs have images expected to stage. Exiting.");
    goto Finish;
  }

  while (CurrentStageCheck < MAX_CHECKS_FOR_SUCCESSFUL_STAGING) {
    CurrentStageCheck++;

    //Check each DIMM that had a image staged to see if it reports a staged version
    for (Index = 0; Index < pDimmTargetsNum; Index++)
    {
      //dont perform unnecessary checks or repeat checks that already took place
      if (FwStagedLongOpCodes[Index] == TransmitFwNeverHappened || FwStageDone[Index] == TRUE) {
        continue;
      }

      if (FwHasBeenStaged(pCmd, pNvmDimmConfigProtocol, pDimmTargets[Index].DimmID) == TRUE) {
        pNvmCodes[Index] = NVM_SUCCESS_FW_RESET_REQUIRED;
        pReturnCodes[Index] = EFI_SUCCESS;
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], pNvmCodes[Index], TRUE);
        FwStagedCompleteCount++;
        FwStageDone[Index] = TRUE;
        FwStagedLongOpCodes[Index] = FW_SUCCESS;
        NVDIMM_DBG("FW stage detected for dimm %d", pDimmTargets[Index].DimmID);
        continue;
      }

      ReturnCode = pNvmDimmConfigProtocol->GetLongOpStatus(pNvmDimmConfigProtocol, pDimmTargets[Index].DimmID,
        &CmdOpcode, &CmdSubOpcode, NULL, NULL, &LongOpEfiStatus);

      if (CmdOpcode == PtUpdateFw && CmdSubOpcode == SubopUpdateFw) {
        if (LongOpEfiStatus != EFI_NO_RESPONSE && LongOpEfiStatus != EFI_SUCCESS) {

          NVDIMM_DBG("Error with FW stage on dimm %d: Long operation failed - LongOpEfiStatus=[%d]",
            pDimmTargets[Index].DimmID, LongOpEfiStatus);

          if (LongOpEfiStatus == EFI_DEVICE_ERROR) {
            pNvmCodes[Index] = NVM_ERR_DEVICE_ERROR;
          } else if (LongOpEfiStatus == EFI_UNSUPPORTED) {
            pNvmCodes[Index] = NVM_ERR_UNSUPPORTED_COMMAND;
          } else if (LongOpEfiStatus == EFI_SECURITY_VIOLATION) {
            pNvmCodes[Index] = NVM_ERR_FW_UPDATE_AUTH_FAILURE;
          } else if (LongOpEfiStatus == EFI_ABORTED) {
            pNvmCodes[Index] = NVM_ERR_LONG_OP_ABORTED_OR_REVISION_FAILURE;
          } else {
            pNvmCodes[Index] = NVM_ERR_LONG_OP_UNKNOWN;
          }

          pReturnCodes[Index] = LongOpEfiStatus;
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], pNvmCodes[Index], TRUE);
          FwStagedCompleteCount++;
          FwStageDone[Index] = TRUE;
          FwStagedLongOpCodes[Index] = LongOpEfiStatus;
        }
      }
    }

    if (FwStagedCompleteCount == pDimmTargetsNum) {
      break; //while loop
    }

    gBS->Stall(MICROSECONDS_PERIOD_BETWEEN_STAGING_CHECKS);
  }

  //mark any DIMM which was pending an update but never confirmed it as a failure
  for (Index = 0; Index < pDimmTargetsNum; Index++) {
    if (FwStageDone[Index] == FALSE) {
      NVDIMM_DBG("Error with FW stage on dimm %d: Long operation status unknown, image never staged", pDimmTargets[Index].DimmID);
      pNvmCodes[Index] = NVM_ERR_UNABLE_TO_STAGE_NO_LONGOP;
      pReturnCodes[Index] = EFI_ABORTED;
      SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], pNvmCodes[Index], TRUE);
      ReturnCode = EFI_ABORTED;
    }
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Check to see if a FW has already been staged on a DIMM

@param[in] pCmd - The command object
@param[in] pNvmDimmConfigProtocol - The open config protocol
@param[in] DimmID - The ID of the dimm to check. Must be a functional DIMM
**/
BOOLEAN
FwHasBeenStaged(
  IN   struct Command *pCmd,
  IN   EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN   UINT16 DimmID
) {

  BOOLEAN         RetBool = FALSE;
  EFI_STATUS      ReturnCode = EFI_SUCCESS;
  UINT32          FunctionalDimmCount = 0;
  UINT32          Index = 0;
  DIMM_INFO *     pFunctionalDimms = NULL;
  NVDIMM_ENTRY();

  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_FW_IMAGE_INFO,
    &pFunctionalDimms, &FunctionalDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to obtain DIMM list");
    goto Finish;
  }

  for (Index = 0; Index < FunctionalDimmCount; Index++) {
    if (pFunctionalDimms[Index].DimmID != DimmID) {
      //not a the same DIMM as passed
      continue;
    }

    if (FALSE == FW_VERSION_UNDEFINED(pFunctionalDimms[Index].StagedFwVersion)) {
      RetBool = TRUE;
    }

    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return RetBool;
}
