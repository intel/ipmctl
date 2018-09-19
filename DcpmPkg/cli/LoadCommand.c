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
    ,{ L"", RECOVERY_TARGET, L"", HELP_TEXT_FLASH_SPI, FALSE, ValueRequired }
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pFileName = NULL;
  CHAR16 *pRelativeFileName = NULL;
  CHAR16 *pTargetValue = NULL;
  CONST CHAR16 *pWorkingDirectory = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIndex = 0;
  UINT32 DimmIdsCount = 0;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN Examine = FALSE;
  BOOLEAN Force = FALSE;
  FW_IMAGE_INFO *pFwImageInfo = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  BOOLEAN Confirmation = 0;
  UINT32 Index = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  EFI_EVENT ProgressEvent = NULL;
  BOOLEAN FlashSpi = FALSE;
  CHAR16 *pOptionsValue = NULL;
  UINT32 StagingFailures = 0;
  BOOLEAN Recovery = FALSE;

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

  Recovery = containsOption(pCmd, RECOVERY_TARGET);
  Examine = containsOption(pCmd, EXAMINE_OPTION) || containsOption(pCmd, EXAMINE_OPTION_SHORT);

  if (Recovery) {
    // Populate the list of DIMM_INFO structures with relevant information
    ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimmCount(pNvmDimmConfigProtocol, &DimmCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (DimmCount == 0) {
      Print(FORMAT_STR_NL, CLI_INFO_NO_NON_FUNCTIONAL_DIMMS);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }

    pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
    if (pDimms == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }

    ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, DimmCount, pDimms);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  } else {
    // Populate the list of DIMM_INFO structures with relevant information
    ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      if (Recovery) {
        Print(FORMAT_STR_NL, CLI_INFO_LOAD_RECOVER_INVALID_DIMM);
      }
      goto Finish;
    }

    if (!Recovery) {
      if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
        Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
    } else {
      if (DimmCount > 0) {
        Print(CLI_RECOVER_DIMM_PROMPT_STR);
        DimmIdsCount = DimmCount;
        pDimmIds = AllocatePool(sizeof(UINT16) * DimmCount);
        for (Index = 0; Index < DimmIdsCount; Index++) {
          pDimmIds[Index] = pDimms[Index].DimmID;
          ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
            DimmStr, MAX_DIMM_UID_LENGTH);
          Print(L"%s ", DimmStr);
        }

        Print(L"\n");
      } else {
        Print(FORMAT_STR_NL, CLI_INFO_NO_NON_FUNCTIONAL_DIMMS);
        ReturnCode = EFI_NOT_FOUND;
        goto Finish;
      }
    }
  }

  if (!Recovery && DimmIdsCount == 0) {
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

  if (Recovery && !Examine) {
    // Warn about disabling TSOD for SMBUS operations
    Print(CLI_RECOVER_DIMM_TSOD_REMINDER_STR);

    ReturnCode = PromptYesNo(&Confirmation);
    if (EFI_ERROR(ReturnCode) || !Confirmation) {
      ReturnCode = EFI_NOT_STARTED;
      goto Finish;
    }
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

  Force = containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT);
  if (Recovery) {
    pOptionsValue = getOptionValue(pCmd, RECOVERY_TARGET);

    if (pOptionsValue == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    } else {

      if (StrICmp(pOptionsValue, RECOVER_OPTION_FLASH_SPI) == 0) {
        FlashSpi = TRUE;
      }
      else if (StrLen(pOptionsValue) == 0) {
        FlashSpi = FALSE;
      } else {
        ReturnCode = EFI_INVALID_PARAMETER;
        Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_OPTION_RECOVER);
        goto Finish;
      }
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
    // Create callback that will print progress
    gBS->CreateEvent((EVT_TIMER | EVT_NOTIFY_SIGNAL), PRINT_PRIORITY, PrintProgress, pCommandStatus, &ProgressEvent);
    gBS->SetTimer(ProgressEvent, TimerPeriodic, PROGRESS_EVENT_TIMEOUT);

    ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pRelativeFileName,
      (CHAR16 *)pWorkingDirectory, Examine, FALSE, TRUE, FlashSpi, pFwImageInfo, pCommandStatus);

    if (!EFI_ERROR(ReturnCode)) {
      Print(L"\n");
    }

    gBS->CloseEvent(ProgressEvent);
  } else { // Not Recovery or this is Examine
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    for (Index = 0; Index < DimmIdsCount; Index++) {

      TempReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmIds[Index], 1, pRelativeFileName,
        (CHAR16 *)pWorkingDirectory, Examine, Force, Recovery, FlashSpi, pFwImageInfo, pCommandStatus);

      if (TRUE == Examine) {
        if (Index == 0) {
          if (pFwImageInfo != NULL) {

            Print(L"(" FORMAT_STR L"): %02d.%02d.%02d.%04d\n",
              pFileName,
              pFwImageInfo->ImageVersion.ProductNumber.Version,
              pFwImageInfo->ImageVersion.RevisionNumber.Version,
              pFwImageInfo->ImageVersion.SecurityVersionNumber.Version,
              pFwImageInfo->ImageVersion.BuildNumber.Build);
          } else {
            Print(L"(" FORMAT_STR L")" FORMAT_STR_NL, pFileName, CLI_ERR_VERSION_RETRIEVE);
          }
        }

        continue;
      }

      if (pCommandStatus->GeneralStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {

        TempReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
        if (EFI_ERROR(TempReturnCode)) {
          ReturnCode = TempReturnCode;
          NVDIMM_DBG("Failed to get dimm handle");
          pCommandStatus->GeneralStatus = NVM_ERR_DIMM_NOT_FOUND;
          goto Finish;
        }

        TempReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(TempReturnCode)) {
          ReturnCode = TempReturnCode;
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimms[Index], NVM_ERR_INVALID_PARAMETER, TRUE);
          goto Finish;
        }

        Print(CLI_DOWNGRADE_PROMPT L"\n", DimmStr);
        TempReturnCode = PromptYesNo(&Confirmation);
        if (EFI_ERROR(TempReturnCode) || !Confirmation) {
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimms[Index], NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED, TRUE);
          continue;
        }

        TempReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmIds[Index], 1, pRelativeFileName,
          (CHAR16 *)pWorkingDirectory, FALSE, TRUE, FALSE, FALSE, pFwImageInfo, pCommandStatus);
        if (EFI_ERROR(TempReturnCode)) {
          ReturnCode = TempReturnCode;
          continue;
        }
      }
      else if (EFI_ERROR(TempReturnCode))
      {
        ReturnCode = TempReturnCode;
        continue;
      }

      TempReturnCode = PollLongOpStatus(pNvmDimmConfigProtocol, pDimmIds[Index],
        FW_UPDATE_OPCODE, FW_UPDATE_SUBOPCODE, LONG_OP_FW_UPDATE_TIMEOUT);
      if (EFI_ERROR(TempReturnCode)) {
        ReturnCode = TempReturnCode;
        if (TempReturnCode == (EFI_INCOMPATIBLE_VERSION) || (TempReturnCode == EFI_UNSUPPORTED)) {
          NVDIMM_DBG("Long operation status for FwUpdate not supported");
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimms[Index], NVM_SUCCESS_FW_RESET_REQUIRED, TRUE);
        } else {
          SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimms[Index], NVM_ERR_FIRMWARE_FAILED_TO_STAGE, TRUE);
          StagingFailures++;
        }
      } else {
        SetObjStatusForDimmInfoWithErase(pCommandStatus, &pDimms[Index], NVM_SUCCESS_FW_RESET_REQUIRED, TRUE);
      }
    }
  } //for loop

Finish:
  DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
  
  if (StagingFailures > 0) {
    pCommandStatus->GeneralStatus = NVM_ERR_FIRMWARE_FAILED_TO_STAGE;
  }

  if (ReturnCode == EFI_SUCCESS && pCommandStatus->GeneralStatus != NVM_SUCCESS) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  }

  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pFileName);
  FREE_POOL_SAFE(pFwImageInfo);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  if (Recovery) {
    FREE_POOL_SAFE(pOptionsValue);
  }
  return ReturnCode;
  }
