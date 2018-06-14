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
    {EXAMINE_OPTION_SHORT, EXAMINE_OPTION, L"", EXAMINE_OPTION_HELP, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", FORCE_OPTION_HELP, FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, TRUE, ValueRequired}
  },
  {{DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional}},      //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Update the firmware on one or more DIMMs",                        //!< help
  Load                                                                //!< run function
};

/**
  Register the load command
**/
EFI_STATUS
RegisterLoadCommand (
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
  @retval EFI_UNSUPPORTED if the driver is not loaded or there are no DCPMEM modules in the system.
  @retval EFI_NOT_FOUND if there is no DIMM with the user specified PID
**/
EFI_STATUS
Load (
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
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
  CHAR16 *pFwType = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  BOOLEAN Confirmation = 0;
  UINT32 Index = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
#ifndef OS_BUILD
  EFI_SHELL_PROTOCOL *pEfiShell = NULL;
  UINTN HandlesCount = 0;
  EFI_HANDLE *pHandles = NULL;
#endif

  NVDIMM_ENTRY();

  SetDisplayInfo(L"LoadFw", ResultsView);

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
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto FinishWithError;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishWithError;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto FinishWithError;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto FinishWithError;
    }
  }

  if (DimmIdsCount == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(&DimmIdsCount, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      goto FinishWithError;
    }
    if (DimmIdsCount == 0) {
      Print(FORMAT_STR_NL, CLI_INFO_NO_MANAGEABLE_DIMMS);
      ReturnCode = EFI_NOT_FOUND;
      goto FinishWithError;
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
  Examine = containsOption(pCmd, EXAMINE_OPTION) || containsOption(pCmd, EXAMINE_OPTION_SHORT);

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
    ReturnCode = gBS->OpenProtocol(pHandles[0], &gEfiShellProtocolGuid, (VOID *) &pEfiShell, NULL, NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (!EFI_ERROR(ReturnCode)) {
      pWorkingDirectory = pEfiShell->GetCurDir(NULL);
      if (pWorkingDirectory == NULL) {
        NVDIMM_WARN("Error while getting the Working Directory.");
      }
    } else {
      NVDIMM_WARN("Error while opening the shell protocol. Code: %r", ReturnCode);
    }
  } else {
    NVDIMM_WARN("Error while opening the shell protocol. Code: %r", ReturnCode);
    /**
      We can still try to open the file. If it is in the root directory, we will be able to open it.
    **/
  }
#endif
  if (Examine) {
    pFwImageInfo = AllocateZeroPool(sizeof(*pFwImageInfo));
    if (pFwImageInfo == NULL) {
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pRelativeFileName,
        (CHAR16 *) pWorkingDirectory, Examine, FALSE, FALSE, FALSE, pFwImageInfo, pCommandStatus);

   if (pFwImageInfo != NULL && pCommandStatus->GeneralStatus == NVM_SUCCESS && ReturnCode == EFI_SUCCESS) {
     pFwType = FirmwareTypeToString(pFwImageInfo->FirmwareType);
     if (pFwType == NULL) {
       Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
       ReturnCode = EFI_OUT_OF_RESOURCES;
       goto Finish;
     }

     Print(L"(" FORMAT_STR L"): %02d.%02d.%02d.%04d (" FORMAT_STR L")\n",
       pFileName,
       pFwImageInfo->ImageVersion.ProductNumber.Version,
       pFwImageInfo->ImageVersion.RevisionNumber.Version,
       pFwImageInfo->ImageVersion.SecurityVersionNumber.Version,
       pFwImageInfo->ImageVersion.BuildNumber.Build,
       pFwType);
     FREE_POOL_SAFE(pFwType);
   } else {
     Print(L"(" FORMAT_STR L")" FORMAT_STR_NL, pFileName, CLI_ERR_VERSION_RETRIEVE);
     goto FinishWithError;
   }
   DisplayCommandStatus(L"",L"", pCommandStatus);
   ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
   goto Finish;

  } else {
    for (Index = 0; Index < DimmIdsCount; Index++) {

      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);

      ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmIds[Index], 1, pRelativeFileName,
          (CHAR16 *) pWorkingDirectory, FALSE, Force, FALSE, FALSE, pFwImageInfo, pCommandStatus);

      if (ReturnCode == EFI_ABORTED &&
        pCommandStatus->GeneralStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {

        ReturnCode = GetDimmHandleByPid(pDimmIds[Index], pDimms, DimmCount, &DimmHandle, &DimmIndex);
        if (EFI_ERROR(ReturnCode)) {
          goto Finish;
        }

        ReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmIndex].DimmUid,
            DimmStr, MAX_DIMM_UID_LENGTH);
        if (EFI_ERROR(ReturnCode)) {
          goto Finish;
        }

        Print(CLI_DOWNGRADE_PROMPT L"\n", DimmStr);
        ReturnCode = PromptYesNo(&Confirmation);
        if (!EFI_ERROR(ReturnCode) && Confirmation) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);

          ReturnCode = pNvmDimmConfigProtocol->UpdateFw(pNvmDimmConfigProtocol, &pDimmIds[Index], 1, pRelativeFileName,
               (CHAR16 *) pWorkingDirectory, FALSE, TRUE, FALSE, FALSE, pFwImageInfo, pCommandStatus);
        } else {
          DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
          continue;
        }
      }

      if (EFI_ERROR(ReturnCode)) {
        goto FinishWithError;
      }

      ReturnCode = PollLongOpStatus(pNvmDimmConfigProtocol, pDimmIds[Index],
        FW_UPDATE_OPCODE, FW_UPDATE_SUBOPCODE, LONG_OP_FW_UPDATE_TIMEOUT);
      if (EFI_ERROR(ReturnCode)) {
        if (ReturnCode == (EFI_INCOMPATIBLE_VERSION) || (ReturnCode == EFI_UNSUPPORTED)) {
          NVDIMM_DBG("Long operation status for FwUpdate not supported");
          ReturnCode = EFI_SUCCESS;
        } else {
          ResetCmdStatus(pCommandStatus, NVM_ERR_FIRMWARE_FAILED_TO_STAGE);
          goto FinishWithError;
        }
      }
      DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
    }
    goto Finish;
  }

FinishWithError:
  DisplayCommandStatus(CLI_INFO_LOAD_FW, CLI_INFO_ON, pCommandStatus);
  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pFileName);
  FREE_POOL_SAFE(pFwImageInfo);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
