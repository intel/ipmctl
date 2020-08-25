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
    {L"", LARGE_PAYLOAD_OPTION, L"", L"", HELP_LPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SMALL_PAYLOAD_OPTION, L"", L"", HELP_SPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
    {EXAMINE_OPTION_SHORT, EXAMINE_OPTION, L"", L"", EXAMINE_OPTION_DETAILS_TEXT, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FORCE_OPTION_DETAILS_TEXT, FALSE, ValueEmpty},
    { L"", RECOVER_OPTION, L"", L"", RECOVER_OPTION_DETAILS_TEXT, FALSE, ValueEmpty }
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif

},
  {                                                                   //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Update the firmware on one or more " PMEM_MODULES_STR L".",                       //!< help
  Load,                                                               //!< run function
  TRUE                                                                //!< Enable Printer
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
  BOOLEAN fileExists = FALSE;
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
  NVM_FW_IMAGE_INFO *pFwImageInfo = NULL;
  volatile UINT32 Index = 0;
  volatile UINT32 Index2 = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  EFI_EVENT ProgressEvent = NULL;
  CHAR16 *pOptionsValue = NULL;
  BOOLEAN Recovery = FALSE;
  DIMM_INFO *pDimmTargets = NULL;
  UINT16 *pDimmIds = NULL;
  UINT16 *pDimmTargetIds = NULL;
  UINT32 StagedFwUpdates = 0;
  DIMM_INFO *pDimms = NULL;
  DIMM_INFO *pCandidateList = NULL;
  UINT32 DimmCount = 0;
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

  if ((NULL != pCmd) && (NULL != pCmd->pPrintCtx)) {
    if (pCmd->pPrintCtx->FormatType == XML) {
      PRINTER_CONFIGURE_BUFFERING(pCmd->pPrintCtx, ON);
    }
    else {
      PRINTER_CONFIGURE_BUFFERING(pCmd->pPrintCtx, OFF);
    }
  }

  NVDIMM_ENTRY();
  SetDisplayInfo(L"LoadFw", ResultsView, NULL);

  for (Index = 0; Index < MAX_DIMMS; Index++) {
    ReturnCodes[Index] = EFI_SUCCESS;
    NvmCodes[Index] = NVM_SUCCESS;
  }

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PrinterSetMsg(NULL, ReturnCode, CLI_ERR_NO_COMMAND);
    goto FinishNoCommandStatus;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR); // if pCMD->pPrintCtx is NULL then will print to stdout
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto FinishNoCommandStatus;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // check options
  pFileName = getOptionValue(pCmd, SOURCE_OPTION);
  if (pFileName == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_WRONG_FILE_PATH);
    goto Finish;
  }

  if (containsOption(pCmd, EXAMINE_OPTION) && containsOption(pCmd, EXAMINE_OPTION_SHORT)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OPTIONS_EXAMINE_USED_TOGETHER);
    goto Finish;
  }

  if (containsOption(pCmd, FORCE_OPTION) && containsOption(pCmd, FORCE_OPTION_SHORT)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OPTIONS_FORCE_USED_TOGETHER);
    goto Finish;
  }

  Recovery = containsOption(pCmd, RECOVER_OPTION);
  Examine = containsOption(pCmd, EXAMINE_OPTION) || containsOption(pCmd, EXAMINE_OPTION_SHORT);
  Force = containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT);

  /*Get the list of functional and non-functional dimms*/
  CHECK_RESULT(GetAllDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_SMART_AND_HEALTH,
      &pDimms, &DimmCount), Finish);

  if (DimmCount == 0) {
    ReturnCode = EFI_NOT_STARTED;
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_DIMMS);
    goto Finish;
  }

  // Include all DCPMMs for fw update / spi flash update
  pCandidateList = pDimms;
  CandidateListCount = DimmCount;

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
      PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
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
      PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    for (Index = 0; Index < DimmTargetsNum; Index++) {
      pDimmTargetIds[Index] = pDimmTargets[Index].DimmID;
    }
  }

  if (NULL == pDimmTargets) {
    ReturnCode = EFI_NOT_FOUND;
    CHECK_RETURN_CODE(ReturnCode, Finish);
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

  ReturnCode = FileExists(pFileName, &fileExists);
  if (EFI_ERROR(ReturnCode) || FALSE == fileExists) {
    NVDIMM_DBG("OpenFile returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    ResetCmdStatus(pCommandStatus, NVM_ERR_FILE_NOT_FOUND);
    ReturnCode = MatchCliReturnCode(NVM_ERR_FILE_NOT_FOUND);
    goto Finish;
  }

  pFwImageInfo = AllocateZeroPool(sizeof(*pFwImageInfo));
  if (pFwImageInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;

  ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
  if (!Examine) {
    PrinterSetMsg(pCmd->pPrintCtx, EFI_SUCCESS, L"Starting update on %d " PMEM_MODULE_STR L"(s)...", DimmTargetsNum);
    // Create callback that will print progress
    gBS->CreateEvent((EVT_TIMER | EVT_NOTIFY_SIGNAL), PRINT_PRIORITY, PrintProgress, pCommandStatus, &ProgressEvent);
    gBS->SetTimer(ProgressEvent, TimerPeriodic, PROGRESS_EVENT_TIMEOUT);
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
        (CHAR16 *)pWorkingDirectory, Examine, Force, Recovery, FALSE, pFwImageInfo, pCommandStatus);
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

      NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, CLI_DOWNGRADE_PROMPT L"\n", DimmStr);
      ReturnCodes[Index] = PromptYesNo(&Confirmation);
      if (EFI_ERROR(ReturnCodes[Index]) || !Confirmation) {
        NvmCodes[Index] = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
        ReturnCodes[Index] = EFI_ABORTED;
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimmTargets[Index], NvmCodes[Index], TRUE);
        continue;
      }

      ReturnCodes[Index] = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmTargetIds[Index], 1, pRelativeFileName,
        (CHAR16 *)pWorkingDirectory, Examine, TRUE, Recovery, FALSE, pFwImageInfo, pCommandStatus);
      if (EFI_ERROR(ReturnCodes[Index])) {
        continue;
      }
    } else if (EFI_ERROR(ReturnCodes[Index])) {
      continue;
    }

    StagedFwUpdates++;
  } //for loop


  if (Examine) {
    //only print non 0.0.0.0 versions...
    if (pFwImageInfo->ImageVersion.ProductNumber.Version != 0 ||
      pFwImageInfo->ImageVersion.RevisionNumber.Version != 0 ||
      pFwImageInfo->ImageVersion.SecurityRevisionNumber.Version != 0 ||
      pFwImageInfo->ImageVersion.BuildNumber.Build != 0) {
      PrinterSetMsg(pCmd->pPrintCtx, ReturnCode, FORMAT_STR L": %02d.%02d.%02d.%04d",
        pFileName,
        pFwImageInfo->ImageVersion.ProductNumber.Version,
        pFwImageInfo->ImageVersion.RevisionNumber.Version,
        pFwImageInfo->ImageVersion.SecurityRevisionNumber.Version,
        pFwImageInfo->ImageVersion.BuildNumber.Build);
    }
  } else {
    gBS->CloseEvent(ProgressEvent);
    if (StagedFwUpdates > 0) {
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
  PRINTER_SET_COMMAND_STATUS(pCmd->pPrintCtx, ReturnCode, CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
  FreeCommandStatus(&pCommandStatus);

FinishNoCommandStatus:
  // if no PrintCtx then nothing can be buffered so no need to process it
  if ((NULL != pCmd)) {
    PRINTER_PROCESS_SET_BUFFER(pCmd->pPrintCtx);
  }
  FREE_POOL_SAFE(pFileName);
  FREE_POOL_SAFE(pFwImageInfo);
  FREE_POOL_SAFE(pDimmIds);
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
    CmdOpcode = 0;
    CmdSubOpcode = 0;
    pNvmDimmConfigProtocol->GetLongOpStatus(pNvmDimmConfigProtocol, pDimmTargets[Index].DimmID,
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
      //don't perform unnecessary checks or repeat checks that already took place
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

      CmdOpcode = 0;
      CmdSubOpcode = 0;
      pNvmDimmConfigProtocol->GetLongOpStatus(pNvmDimmConfigProtocol, pDimmTargets[Index].DimmID,
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
  DIMM_INFO Dimm;
  NVDIMM_ENTRY();

  CHECK_RESULT((pNvmDimmConfigProtocol->GetDimm(pNvmDimmConfigProtocol, DimmID, DIMM_INFO_CATEGORY_FW_IMAGE_INFO, &Dimm)), Finish);
  if (FALSE == FW_VERSION_UNDEFINED(Dimm.StagedFwVersion)) {
    RetBool = TRUE;
  }


Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return RetBool;
}
