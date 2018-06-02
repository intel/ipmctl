/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "Common.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "CreateGoalCommand.h"
#include "NvmDimmCli.h"
#include "ShowGoalCommand.h"
#include <Convert.h>

#define CREATE_GOAL_COMMAND_STATUS_HEADER       L"Create region configuration goal"
#define CREATE_GOAL_COMMAND_STATUS_CONJUNCTION  L" on"

/**
  Command syntax definition
**/
struct Command CreateGoalCommand =
{
  CREATE_VERB,                                                   //!< verb
  {                                                              //!< options
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired}
  },
  {                                                              //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueRequired},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired}
  },
  {                                                              //!< properties
    {MEMORY_MODE_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {PERSISTENT_MEM_TYPE_PROPERTY, L"", HELP_TEXT_PERSISTENT_MEM_TYPE, FALSE, ValueRequired},
    {RESERVED_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {CONFIG_PROPERTY, L"", HELP_TEXT_CONFIG_PROPERTY, FALSE, ValueRequired},
    {NS_LABEL_VERSION_PROPERTY, L"", HELP_TEXT_NS_LABEL_VERSION, FALSE, ValueRequired}
  },
  L"Provision capacity on one or more DIMMs into regions",     //!< help
  CreateGoal
};

STATIC
EFI_STATUS
GetPersistentMemTypeValue(
  IN     CHAR16 *pStringValue,
     OUT UINT8 *pPersistentMemType
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (pStringValue == NULL || pPersistentMemType == NULL) {
    goto Finish;
  }

  if (StrICmp(pStringValue, PERSISTENT_MEM_TYPE_AD_STR) == 0) {
    *pPersistentMemType = PM_TYPE_AD;
  } else if (StrICmp(pStringValue, PERSISTENT_MEM_TYPE_AD_NI_STR) == 0) {
    *pPersistentMemType = PM_TYPE_AD_NI;
  } else {
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}

STATIC
EFI_STATUS
GetLabelVersionFromStr(
  IN     CHAR16 *pLabelVersionStr,
     OUT UINT16 *pMajor,
     OUT UINT16 *pMinor
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 **ppSplitVersion = NULL;
  UINT32 NumOfElements = 0;
  UINT64 Major = 0;
  UINT64 Minor = 0;


  if (pLabelVersionStr == NULL || pMajor == NULL || pMinor == NULL) {
    ReturnCode =  EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ppSplitVersion = StrSplit(pLabelVersionStr, L'.', &NumOfElements);
  if (ppSplitVersion == NULL || NumOfElements != 2) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = CrStrDecimalToUint64(ppSplitVersion[0], &Major, FALSE);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  ReturnCode = CrStrDecimalToUint64(ppSplitVersion[1], &Minor, FALSE);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  *pMajor = (UINT16) Major;
  *pMinor = (UINT16) Minor;

Finish:
  if (ppSplitVersion != NULL) {
    FreeStringArray(ppSplitVersion, NumOfElements);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check and confirm alignments using prompt

  Send user capacities to driver and retrieve alignments that will have to be done. Display these alignments and
  confirm using prompt mechanism.

  @param[in] pNvmDimmConfigProtocol is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatilePercent Volatile region size in percents
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percents
  @param[in] ReserveDimm Reserve one DIMM for use as a Storage or not interleaved AppDirect memory
  @param[in] UnitsToDisplay The units to be used to display capacity
  @param[out] pConfirmation Confirmation from prompt

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
STATIC
EFI_STATUS
CheckAndConfirmAlignments(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     UINT8 PersistentMemType,
  IN     UINT32 VolatilePercent,
  IN     UINT32 ReservedPercent,
  IN     UINT8 ReserveDimm,
  IN     UINT16 UnitsToDisplay,
     OUT BOOLEAN *pConfirmation
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT32 VolatilePercentAligned = 0;
  UINT32 PercentDiff = 0;
  REGION_GOAL_PER_DIMM_INFO RegionConfigsInfo[MAX_DIMMS];
  UINT32 RegionConfigsCount = 0;
  UINT32 Index = 0;
  BOOLEAN CapacityReducedForSKU = FALSE;
  CHAR16 *pSingleStatusCodeMessage = NULL;

  NVDIMM_ENTRY();

  ZeroMem(RegionConfigsInfo, sizeof(RegionConfigsInfo[0]) * MAX_DIMMS);

  if (pNvmDimmConfigProtocol == NULL || pConfirmation == NULL) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  *pConfirmation = FALSE;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  /** Make a copy of user's values. They will be needed in a next call. **/
  VolatilePercentAligned = VolatilePercent;

  ReturnCode = pNvmDimmConfigProtocol->GetActualRegionsGoalCapacities(
    pNvmDimmConfigProtocol,
    pDimmIds,
    DimmIdsCount,
    pSocketIds,
    SocketIdsCount,
    PersistentMemType,
    &VolatilePercentAligned,
    ReservedPercent,
    ReserveDimm,
    RegionConfigsInfo,
    &RegionConfigsCount,
    pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(CREATE_GOAL_COMMAND_STATUS_HEADER, CLI_INFO_ON, pCommandStatus);
    goto Finish;
  }

  if (VolatilePercent >= VolatilePercentAligned) {
    PercentDiff = VolatilePercent - VolatilePercentAligned;
  } else {
    PercentDiff = VolatilePercentAligned - VolatilePercent;
  }

  NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, FORMAT_STR_NL, CLI_CREATE_GOAL_PROMPT_HEADER);
  NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"\n");

#ifdef OS_BUILD
  ReturnCode = ShowGoalPrintTableView(RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, NULL, FALSE);
#else // OS_BUILD
  ReturnCode = ShowGoalPrintTableView(RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, FALSE);
#endif // OSBUILD

  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  for (Index = 0; Index < pCommandStatus->ObjectStatusCount; Index++) {
    CapacityReducedForSKU = IsSetNvmStatusForObject(pCommandStatus, Index, NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU);
    if (CapacityReducedForSKU) {
      break;
    }
  }

  if (pCommandStatus->GeneralStatus == NVM_WARN_2LM_MODE_OFF) {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_2LM_MODE_OFF);
    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"\n" FORMAT_STR_NL, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  if (pCommandStatus->GeneralStatus == NVM_WARN_IMC_DDR_PMM_NOT_PAIRED) {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_IMC_DDR_PMM_NOT_PAIRED);
    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"\n" FORMAT_STR_NL, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  if (PercentDiff > PROMPT_ALIGN_PERCENTAGE) {
     NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"\n");
     NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, CLI_CREATE_GOAL_PROMPT_VOLATILE L"\n");
  }

  if (CapacityReducedForSKU) {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU);
    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"\n" FORMAT_STR_NL, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  ReturnCode = PromptYesNo(pConfirmation);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_PROMPT_INVALID);
    NVDIMM_DBG("Failed on PromptedInput");
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the Create Goal command

  @param[in] pCmd command from CLI
  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
CreateGoal(
  IN     struct Command *pCmd
  )
{
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  BOOLEAN Force = FALSE;
  CHAR16 *pTargetValue = NULL;
  CHAR16 *pPropertyValue = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketIdsCount = 0;
  UINT64 PropertyValue = 0;
  UINT32 VolatileMode = 0;
  UINT32 ReservedPercent = 0;
  UINT8 ReserveDimm = RESERVE_DIMM_NONE;
  UINT8 PersistentMemType = PM_TYPE_AD;
  COMMAND_INPUT ShowGoalCmdInput;
  COMMAND ShowGoalCmd;
  BOOLEAN Confirmation = FALSE;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  BOOLEAN Valid = FALSE;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  CHAR16 *pCommandStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT16 LabelVersionMajor = 0;
  UINT16 LabelVersionMinor = 0;

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
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
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

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
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
      NVDIMM_DBG("Failed on GetSocketsFromString");
      goto Finish;
    }
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  // Check quick config first
  ReturnCode = ContainsProperty(pCmd, CONFIG_PROPERTY);
  if (!EFI_ERROR(ReturnCode)) {
      ReturnCode = GetPropertyValue(pCmd, CONFIG_PROPERTY, &pPropertyValue);
      if (EFI_ERROR(ReturnCode)) {
          Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
          goto Finish;
      }
      if (StrICmp(pPropertyValue, PROPERTY_CONFIG_VALUE_MM) == 0) {
          VolatileMode = 100;
          ReservedPercent = 0;
      } else if (StrICmp(pPropertyValue, PROPERTY_CONFIG_VALUE_AD) == 0) {
          PersistentMemType = PM_TYPE_AD;
          ReservedPercent = 0;
      } else if (StrICmp(pPropertyValue, PROPERTY_CONFIG_VALUE_MM_AD) == 0) {
          VolatileMode = 25;
          PersistentMemType = PM_TYPE_AD;
          ReservedPercent = 0;
      } else {
          ReturnCode = EFI_INVALID_PARAMETER;
          NVDIMM_WARN("Invalid Config. Supported categories %s ", HELP_TEXT_CONFIG_PROPERTY);
          Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_CONFIG);
          goto Finish;
      }
  } else {
      ReturnCode = ContainsProperty(pCmd, MEMORY_MODE_PROPERTY);
      if (!EFI_ERROR(ReturnCode)) {
          Valid = PropertyToUint64(pCmd, MEMORY_MODE_PROPERTY, &PropertyValue);
          /** Make sure it is in 0-100 percent range. **/
          if (Valid && PropertyValue <= 100) {
              VolatileMode = (UINT32)PropertyValue;
          } else {
              Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_MEMORY_MODE);
              ReturnCode = EFI_INVALID_PARAMETER;
              goto Finish;
          }
      } else {
          VolatileMode = 0;
      }

      if ((ReturnCode = ContainsProperty(pCmd, PERSISTENT_MEM_TYPE_PROPERTY)) != EFI_NOT_FOUND) {
          if (EFI_ERROR(ReturnCode)) {
              Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
              goto Finish;
          }
          ReturnCode = GetPropertyValue(pCmd, PERSISTENT_MEM_TYPE_PROPERTY, &pPropertyValue);
          if (EFI_ERROR(ReturnCode)) {
              Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
              goto Finish;
          }
          ReturnCode = GetPersistentMemTypeValue(pPropertyValue, &PersistentMemType);
          if (EFI_ERROR(ReturnCode)) {
              Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_PERSISTENT_MEM_TYPE);
              goto Finish;
          }
      }

      ReturnCode = ContainsProperty(pCmd, RESERVED_PROPERTY);
      if (!EFI_ERROR(ReturnCode)) {
          Valid = PropertyToUint64(pCmd, RESERVED_PROPERTY, &PropertyValue);
          /** Make sure it is in 0-100 percent range. **/
          if (Valid && PropertyValue <= 100) {
              ReservedPercent = (UINT32)PropertyValue;
          } else {
              Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_RESERVED);
              ReturnCode = EFI_INVALID_PARAMETER;
              goto Finish;
          }
      } else {
          ReservedPercent = 0;
      }
  }

  if (ReservedPercent + VolatileMode > 100) {
    Print(FORMAT_STR_NL, CLI_ERR_PROPERTIES_MEMORYMODE_RESERVED_TOO_LARGE);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** If Volatile and Reserved Percent sum to 100 then never map Appdirect even if alignment would allow it **/
  if (ReservedPercent + VolatileMode == 100) {
    PersistentMemType = PM_TYPE_STORAGE;
  }

  ReturnCode = ContainsProperty(pCmd, NS_LABEL_VERSION_PROPERTY);
  if (!EFI_ERROR(ReturnCode)) {
    ReturnCode = GetPropertyValue(pCmd, NS_LABEL_VERSION_PROPERTY, &pPropertyValue);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = GetLabelVersionFromStr(pPropertyValue, &LabelVersionMajor, &LabelVersionMinor);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (LabelVersionMajor != NSINDEX_MAJOR) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_NS_LABEL_VERSION);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    if ((LabelVersionMinor != NSINDEX_MINOR_1) &&
        (LabelVersionMinor != NSINDEX_MINOR_2)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_PROPERTY_NS_LABEL_VERSION);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

  } else if (ReturnCode == EFI_NOT_FOUND) {
    /** Default to 1.2 labels **/
    LabelVersionMajor = NSINDEX_MAJOR;
    LabelVersionMinor = NSINDEX_MINOR_2;
  } else {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  if (!Force) {
    ReturnCode = CheckAndConfirmAlignments(pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount,
        PersistentMemType, VolatileMode, ReservedPercent, ReserveDimm, UnitsToDisplay, &Confirmation);
    if (EFI_ERROR(ReturnCode) || !Confirmation) {
      goto Finish;
    }
  }
  ReturnCode = pNvmDimmConfigProtocol->CreateGoalConfig(pNvmDimmConfigProtocol, FALSE, pDimmIds, DimmIdsCount,
    pSocketIds, SocketIdsCount, PersistentMemType, VolatileMode, ReservedPercent, ReserveDimm,
    LabelVersionMajor, LabelVersionMinor, pCommandStatus);

  if (!EFI_ERROR(ReturnCode)) {
#ifdef OS_BUILD
    if (Force) {
      if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
          pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR_SPACE FORMAT_STR L" " FORMAT_STR L" " FORMAT_STR L" " FORMAT_STR, L"show", UNITS_OPTION, UnitsToStr(UnitsToDisplay),
              ACTION_REQ_OPTION, L"-goal");
      } else {
          pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR, L"show -ar -goal");
      }
    } else
#endif // OS_BUILD
    if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
      pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR_SPACE FORMAT_STR L" " FORMAT_STR L" " FORMAT_STR, L"show", UNITS_OPTION, UnitsToStr(UnitsToDisplay),
                        L"-goal");
    } else {
      pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR, L"show -goal");
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
    NVDIMM_BUFFER_CONTROLLED_MSG(FALSE, L"Created following region configuration goal\n");
    ShowGoalCmd.run(&ShowGoalCmd);
    FreeCommandInput(&ShowGoalCmdInput);
    FREE_POOL_SAFE(pCommandStr);
  } else {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    DisplayCommandStatus(CREATE_GOAL_COMMAND_STATUS_HEADER, CLI_INFO_ON, pCommandStatus);
  }

Finish:
  FreeCommandStructure(&ShowGoalCmd);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Register the create dimm command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterCreateGoalCommand(
  )
{
  EFI_STATUS ReturnCode;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&CreateGoalCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
