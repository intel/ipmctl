/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "LoadGoalCommand.h"
#include "Common.h"
#include "NvmDimmCli.h"

/**
  Command syntax definition
**/
struct Command LoadGoalCommand =
{
  LOAD_VERB,                                                            //!< verb
  {                                                                     //!< options
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty},
    {L"", SOURCE_OPTION, L"", SOURCE_OPTION_HELP, TRUE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },
  {                                                                     //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                              //!< properties
  L"Load stored configuration goal for specific DIMMs",            //!< help
  LoadGoal
};


/**
  Execute the Load Goal command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
  @retval EFI_NO_RESPONSE FW busy on one or more dimms
**/
EFI_STATUS
LoadGoal(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketIdsCount = 0;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pLoadUserPath = NULL;
  CHAR16 *pLoadFilePath = NULL;
  CHAR16 *pTargetValue = NULL;
  CHAR8 *pFileString = NULL;
  CHAR16 *pCommandStr = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathProtocol = NULL;
  BOOLEAN Force = FALSE;
  COMMAND_INPUT ShowGoalCmdInput;
  COMMAND ShowGoalCmd;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  CHAR16 *pUnitsStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT32 SocketIndex = 0;
  BOOLEAN Confirmation = FALSE;
  INTEL_DIMM_CONFIG *pIntelDIMMConfig = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  ZeroMem(&ShowGoalCmdInput, sizeof(ShowGoalCmdInput));
  ZeroMem(&ShowGoalCmd, sizeof(ShowGoalCmd));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  UnitsToDisplay = DisplayPreferences.SizeUnit;

  ReturnCode = GetUnitsOption(pCmd, &UnitsOption);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Any valid units option will override the preferences **/
  if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
    UnitsToDisplay = UnitsOption;
  }

  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  pLoadFilePath = AllocateZeroPool(OPTION_VALUE_LEN * sizeof(*pLoadFilePath));
  if (pLoadFilePath == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Check -source option
  if (containsOption(pCmd, SOURCE_OPTION)) {
    pLoadUserPath = getOptionValue(pCmd, SOURCE_OPTION);
    if (pLoadUserPath == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Could not get -source value. Out of memory");
      Print(FORMAT_STR_NL, CLI_ERR_OUT_OF_MEMORY);
      goto Finish;
    }
  }

  ReturnCode = GetDeviceAndFilePath(pLoadUserPath, pLoadFilePath, &pDevicePathProtocol);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get file path (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }

  // check if auto provision is enabled
  RetrieveIntelDIMMConfig(&pIntelDIMMConfig);
  if(pIntelDIMMConfig != NULL) {
    if (pIntelDIMMConfig->ProvisionCapacityMode == PROVISION_CAPACITY_MODE_AUTO) {
      ReturnCode = EFI_INVALID_PARAMETER;
      Print(FORMAT_STR_NL, CLI_ERR_CREATE_GOAL_AUTO_PROV_ENABLED);
      FreePool(pIntelDIMMConfig);
      goto Finish;
    } else {
      FreePool(pIntelDIMMConfig);
    }
  }

  // Check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pTargetValue, &pSocketIds, &SocketIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      NVDIMM_DBG("Failed on GetUintsFromString");
      goto Finish;
    }
  }

  // Initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ReturnCode = ParseSourceDumpFile(pLoadFilePath, pDevicePathProtocol, &pFileString);
  if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_LOAD_INVALID_DATA_IN_FILE);
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(CLI_INFO_LOAD_GOAL, L"", pCommandStatus);
    goto Finish;
  }

  if (!Force) {

    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, CLI_INFO_LOAD_GOAL_CONFIRM_PROMPT, pLoadFilePath);
    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"\n");
    ReturnCode = PromptYesNo(&Confirmation);

    if (!Confirmation || EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  ReturnCode = pNvmDimmConfigProtocol->LoadGoalConfig(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pSocketIds,
      SocketIdsCount, pFileString, pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
      ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
      DisplayCommandStatus(CLI_INFO_LOAD_GOAL, L"", pCommandStatus);
      goto Finish;
    }
  }

  if (!EFI_ERROR(ReturnCode)) {
    if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
      CHECK_RESULT(UnitsToStr(gNvmDimmCliHiiHandle, UnitsToDisplay, &pUnitsStr), Finish);
      pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR_SPACE FORMAT_STR L" " FORMAT_STR L" " FORMAT_STR, L"show", UNITS_OPTION, pUnitsStr,
                        L"-goal");
    } else {
      pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR, L"show -goal");
    }
    // Only print the socket applied
    if (SocketIdsCount > 0) {
      pCommandStr = CatSPrintClean(pCommandStr, L" " FORMAT_STR L" ", L"-socket");
      for (SocketIndex = 0; SocketIndex < SocketIdsCount; SocketIndex++) {
        if (SocketIndex == 0) {
          pCommandStr = CatSPrintClean(pCommandStr, L"%d", pSocketIds[SocketIndex]);
        } else {
          pCommandStr = CatSPrintClean(pCommandStr, L",%d", pSocketIds[SocketIndex]);
        }
      }
    }

    FillCommandInput(pCommandStr, &ShowGoalCmdInput);
    ReturnCode = Parse(&ShowGoalCmdInput, &ShowGoalCmd);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed parsing command input");
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    if (ShowGoalCmd.run == NULL) {
      NVDIMM_WARN("Couldn't find show -goal command");
      Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"Loaded following pool configuration goal\n");
    ExecuteCmd(&ShowGoalCmd);
    FREE_POOL_SAFE(pCommandStr);
  }

Finish:
  FreeCommandInput(&ShowGoalCmdInput);
  FreeCommandStructure(&ShowGoalCmd);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pLoadFilePath);
  FREE_POOL_SAFE(pFileString);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pCommandStr);
  FREE_POOL_SAFE(pLoadUserPath);
  FREE_POOL_SAFE(pUnitsStr);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Read and parse source file with Pool Goal structures to be loaded.

  @param[in] pFilePath Name is a pointer to a load file path
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the
                          physical device or logical device. The device path describes the location of the device
                          the handle is for.
  @param[out] pFileString Buffer for Pool Goal configuration from file

  @retval EFI_SUCCESS File read and parse success
  @retval EFI_INVALID_PARAMETER At least one of parameters is NULL or format of configuration in file is not proper
  @retval EFI_INVALID_PARAMETER memory allocation failure
**/
EFI_STATUS
ParseSourceDumpFile(
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT CHAR8 **pFileString
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 FileBufferSize = 0;
  UINT8 *pFileBuffer = NULL;
  UINT32 NumberOfChars = 0;

  if (pFilePath == NULL || pFileString == NULL) {
    NVDIMM_DBG("Invalid Pointer");
    goto Finish;
  }
#ifndef OS_BUILD
  if (pDevicePath == NULL) {
     NVDIMM_DBG("Invalid Pointer");
     goto Finish;
  }
#endif
  ReturnCode = FileRead(pFilePath, pDevicePath, MAX_CONFIG_DUMP_FILE_SIZE, &FileBufferSize, (VOID **) &pFileBuffer);
  if (EFI_ERROR(ReturnCode) || pFileBuffer == NULL) {
    goto Finish;
  }

  /** Prepare memory for end of string char **/
  *pFileString = ReallocatePool(FileBufferSize, FileBufferSize + sizeof('\0'), pFileBuffer);
  if (*pFileString == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  NumberOfChars = (UINT32)FileBufferSize / sizeof(**pFileString);

  /** Make read data from file as string **/
  (*pFileString)[NumberOfChars] = '\0';

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the Load Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterLoadGoalCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&LoadGoalCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

