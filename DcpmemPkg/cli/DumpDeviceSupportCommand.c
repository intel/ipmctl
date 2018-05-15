/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include "DumpDeviceSupportCommand.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "LoadCommand.h"
#include "Debug.h"
#include "Convert.h"

/**
  Get FW debug log syntax definition
**/
struct Command DumpDeviceSupportCommandSyntax = {
  DUMP_VERB,                                                        //!< verb
  {                                                                 //!< options
    {L"", DESTINATION_OPTION, L"", DESTINATION_OPTION_HELP, TRUE, ValueRequired},
  },
  {                                                                 //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional},
    {SUPPORT_TARGET, L"", L"", TRUE, ValueEmpty}
  },
  {                                                                 //!< properties
    {L"", L"", L"", FALSE, ValueOptional}
  },
  L"Dump device support data",                                      //!< help
  DumpDeviceSupportCommand                                          //!< run function
};

/**
  Register syntax of dump -debug
**/
EFI_STATUS
RegisterDumpDeviceSupportCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&DumpDeviceSupportCommandSyntax);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Dump device support data command

  @param[in] pCmd Command from CLI

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pCmd NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL Function failure
**/
EFI_STATUS
DumpDeviceSupportCommand(
  IN    struct Command *pCmd
)
{
  EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pTargetValue = NULL;
  CHAR16 *pDumpUserPath = NULL;
  CHAR16 *pDumpAppendedPath = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmCount = 0;
  UINT32 DimmIdsCount = 0;
  UINT32 MaxTokenID = 0;
  UINT32 DimmHandle = 0;
  UINT32 DimmHandleIndex = 0;
  UINT32 DimmIndex = 0;
  UINT32 TokenIndex = 0;
  UINT64 BytesWritten = 0;
  VOID *pSupportBuffer = NULL;
  BOOLEAN fExists = FALSE;
  BOOLEAN DeviceSupportDataAvailable = FALSE;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  DIMM_INFO *pDimms = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];

  NVDIMM_ENTRY();
  ZeroMem(DimmStr, sizeof(DimmStr));

  SetDisplayInfo(L"DumpDeviceSupport", ResultsView);

  if (pCmd == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /* Open Config protocol */
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  /* Populate the list of DIMM_INFO structures with relevant information */
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /* Initialize status structure */
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  /* Get specific DIMM pid passed in, set it */
  pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
  ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on GetDimmIdsFromString");
    goto Finish;
  }

  /* If no dimm IDs are specified get IDs from all manageable dimms */
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

  /* Check -destination option */
  if (containsOption(pCmd, DESTINATION_OPTION)) {
    pDumpUserPath = getOptionValue(pCmd, DESTINATION_OPTION);
    if (pDumpUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -destination value. Out of memory.");
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (DimmIndex = 0; DimmIndex < DimmIdsCount; DimmIndex++) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    DeviceSupportDataAvailable = FALSE;

    TempReturnCode = GetDimmHandleByPid(pDimmIds[DimmIndex], pDimms, DimmCount, &DimmHandle,
        &DimmHandleIndex);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }

    TempReturnCode = GetPreferredDimmIdAsString(DimmHandle, pDimms[DimmHandleIndex].DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(TempReturnCode)) {
      KEEP_ERROR(ReturnCode, TempReturnCode);
      goto Finish;
    }

    TempReturnCode = pNvmDimmConfigProtocol->GetDeviceSupportDataInventory(pNvmDimmConfigProtocol,
       pDimmIds[DimmIndex], &MaxTokenID, pCommandStatus);
    if (EFI_ERROR(TempReturnCode)) {
      DisplayCommandStatus(CLI_INFO_DUMP_SUPPORT, CLI_INFO_FROM, pCommandStatus);
      KEEP_ERROR(ReturnCode, TempReturnCode);
      continue;
    }

    if (MaxTokenID == 0) {
      Print(CLI_INFO_NO_DEVICE_SUPPORT_DATA_SUPPORTED L"\n", DimmStr);
      continue;
    }

    Print(CLI_INFO_RETRIEVING_DEVICE_SUPPORT_DATA L"\n", DimmStr);
    for (TokenIndex = 1; TokenIndex <= MaxTokenID; TokenIndex++) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);

      pDumpAppendedPath = CatSPrint(NULL, FORMAT_STR L"_" FORMAT_STR L"_%d", pDumpUserPath, pDimms[DimmHandleIndex].DimmUid,
        TokenIndex);

      TempReturnCode = FileExists(pDumpAppendedPath, &fExists);
      if (EFI_ERROR(TempReturnCode) && TempReturnCode != EFI_NOT_FOUND) {
        KEEP_ERROR(ReturnCode, TempReturnCode);
        continue;
      }
      if (fExists) {
        Print(CLI_ERR_FILE_EXISTS L"\n", pDumpAppendedPath);
        KEEP_ERROR(ReturnCode, EFI_INVALID_PARAMETER);
        continue;
      }

      TempReturnCode = pNvmDimmConfigProtocol->DumpDeviceSupportData(pNvmDimmConfigProtocol,
          pDimmIds[DimmIndex], TokenIndex, &pSupportBuffer, &BytesWritten, pCommandStatus);
      if (EFI_ERROR(TempReturnCode)) {
        /* EFI_ABORTED means no data availible so just continue. Print all other errors  */
        NVDIMM_DBG("Error retrieving token %d from AEP DIMM (%s)", TokenIndex, DimmStr);
        if (TempReturnCode != EFI_ABORTED) {
          DisplayCommandStatus(CLI_INFO_DUMP_SUPPORT, CLI_INFO_FROM, pCommandStatus);
          KEEP_ERROR(ReturnCode, TempReturnCode);
        }
        BytesWritten = 0;
        FREE_POOL_SAFE(pSupportBuffer);
        FREE_POOL_SAFE(pDumpAppendedPath);
        continue;
      }

      TempReturnCode = DumpToFile(pDumpAppendedPath, BytesWritten, pSupportBuffer, FALSE);
      if (EFI_ERROR(TempReturnCode)) {
        if (TempReturnCode == EFI_VOLUME_FULL) {
          Print(FORMAT_STR_SPACE L"(" FORMAT_STR L").\n", CLI_ERR_OUT_OF_SPACE, pDumpAppendedPath);
        } else {
          Print(FORMAT_STR_SPACE L"(" FORMAT_STR L").\n", CLI_ERR_DUMP_FAILED, pDumpAppendedPath);
        }
        goto Finish;
      } else {
         DeviceSupportDataAvailable = TRUE;
         Print(CLI_INFO_DUMP_SUCCESS L"\n", DimmStr, pDumpAppendedPath);
      }
      KEEP_ERROR(ReturnCode, TempReturnCode);
      BytesWritten = 0;
      FREE_POOL_SAFE(pSupportBuffer);
      FREE_POOL_SAFE(pDumpAppendedPath);
    }
    if (!DeviceSupportDataAvailable) {
      Print(CLI_INFO_NO_DEVICE_SUPPORT_DATA_AVAILABLE L"\n", DimmStr);
    }
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDumpUserPath);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pSupportBuffer);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
