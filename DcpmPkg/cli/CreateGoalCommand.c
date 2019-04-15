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
#include <ReadRunTimePreferences.h>


#define CREATE_GOAL_COMMAND_STATUS_HEADER       L"Create region configuration goal"
#define CREATE_GOAL_COMMAND_STATUS_CONJUNCTION  L" on"

/**
  Command syntax definition
**/
struct Command CreateGoalCommand =
{
  CREATE_VERB,                                                   //!< verb
  {                                                              //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_DDRT, L"", L"", L"", FALSE, ValueEmpty},
    {PROTOCOL_OPTION_SMBUS, L"", L"", L"", FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"", FALSE, ValueEmpty},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP, FALSE, ValueRequired},
#ifdef OS_BUILD
    {OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired}
#endif

  },
  {                                                              //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueRequired}
  },
  {                                                              //!< properties
    {MEMORY_MODE_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {PERSISTENT_MEM_TYPE_PROPERTY, L"", HELP_TEXT_PERSISTENT_MEM_TYPE, FALSE, ValueRequired},
    {RESERVED_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {NS_LABEL_VERSION_PROPERTY, L"", HELP_TEXT_NS_LABEL_VERSION, FALSE, ValueRequired}
  },
  L"Provision capacity on one or more DIMMs into regions",     //!< help
  CreateGoal,
  TRUE,                                               //!< enable print control support
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

  @param[in] pCmd command from CLI
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
  IN     struct Command *pCmd,
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
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  *pConfirmation = FALSE;

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
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
    if (EFI_VOLUME_CORRUPTED == ReturnCode) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_PCD_CORRUPTED);
    }
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    PRINTER_SET_COMMAND_STATUS(pCmd->pPrintCtx, ReturnCode, CREATE_GOAL_COMMAND_STATUS_HEADER, CLI_INFO_ON, pCommandStatus);
    goto Finish;
  }

  if (VolatilePercent >= VolatilePercentAligned) {
    PercentDiff = VolatilePercent - VolatilePercentAligned;
  } else {
    PercentDiff = VolatilePercentAligned - VolatilePercent;
  }

  PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, CLI_CREATE_GOAL_PROMPT_HEADER  L"\n");
  
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(pCmd->pPrintCtx);
  ReturnCode = ShowGoalPrintTableView(pCmd, RegionConfigsInfo, UnitsToDisplay, RegionConfigsCount, FALSE);

  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }
  else {
    PRINTER_PROCESS_SET_BUFFER_FORCE_TEXT_TABLE_MODE(pCmd->pPrintCtx);
  }

  for (Index = 0; Index < pCommandStatus->ObjectStatusCount; Index++) {
    CapacityReducedForSKU = IsSetNvmStatusForObject(pCommandStatus, Index, NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU);
    if (CapacityReducedForSKU) {
      break;
    }
  }

  if (pCommandStatus->GeneralStatus == NVM_WARN_2LM_MODE_OFF) {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_2LM_MODE_OFF);
    PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  if (pCommandStatus->GeneralStatus == NVM_WARN_IMC_DDR_PMM_NOT_PAIRED) {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_IMC_DDR_PMM_NOT_PAIRED);
    PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  if (PercentDiff > PROMPT_ALIGN_PERCENTAGE) {
     PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, L"\n" CLI_CREATE_GOAL_PROMPT_VOLATILE L"\n");
  }

  if (CapacityReducedForSKU) {
    pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU);
    PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, L"\n" FORMAT_STR_NL, pSingleStatusCodeMessage);
    FREE_POOL_SAFE(pSingleStatusCodeMessage);
  }

  ReturnCode = PromptYesNo(pConfirmation);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
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
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
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
  CHAR16 *pUnitsStr = NULL;
  CHAR16 *pCommandStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT16 LabelVersionMajor = 0;
  UINT16 LabelVersionMinor = 0;
  INTEL_DIMM_CONFIG *pIntelDIMMConfig = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pShowGoalOutputArgs = NULL;
  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  ZeroMem(&ShowGoalCmdInput, sizeof(ShowGoalCmdInput));
  ZeroMem(&ShowGoalCmd, sizeof(ShowGoalCmd));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  // NvmDimmConfigProtocol required
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
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

  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT)) {
    Force = TRUE;
  }

  ReturnCode = ReadRunTimePreferences(&DisplayPreferences, DISPLAY_CLI_INFO);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
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

  // check if auto provision is enabled
  RetrieveIntelDIMMConfig(&pIntelDIMMConfig);
  if(pIntelDIMMConfig != NULL) {
    if (pIntelDIMMConfig->ProvisionCapacityMode == PROVISION_CAPACITY_MODE_AUTO) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_CREATE_GOAL_AUTO_PROV_ENABLED);
      FreePool(pIntelDIMMConfig);
      goto Finish;
    } else {
      FreePool(pIntelDIMMConfig);
    }
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)){
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  if (ContainTarget(pCmd, SOCKET_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pTargetValue, &pSocketIds, &SocketIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      NVDIMM_DBG("Failed on GetSocketsFromString");
      goto Finish;
    }
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  ReturnCode = ContainsProperty(pCmd, MEMORY_MODE_PROPERTY);
  if (!EFI_ERROR(ReturnCode)) {
    Valid = PropertyToUint64(pCmd, MEMORY_MODE_PROPERTY, &PropertyValue);
    /** Make sure it is in 0-100 percent range. **/
    if (Valid && PropertyValue <= 100) {
      VolatileMode = (UINT32)PropertyValue;
    } else {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_MEMORY_MODE);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
     }
  } else {
      VolatileMode = 0;
  }

  if ((ReturnCode = ContainsProperty(pCmd, PERSISTENT_MEM_TYPE_PROPERTY)) != EFI_NOT_FOUND) {
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    ReturnCode = GetPropertyValue(pCmd, PERSISTENT_MEM_TYPE_PROPERTY, &pPropertyValue);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    ReturnCode = GetPersistentMemTypeValue(pPropertyValue, &PersistentMemType);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_PERSISTENT_MEM_TYPE);
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
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_RESERVED);
      goto Finish;
    }
  } else {
      ReservedPercent = 0;
  }

  if (ReservedPercent + VolatileMode > 100) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_PROPERTIES_MEMORYMODE_RESERVED_TOO_LARGE);
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
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (LabelVersionMajor != NSINDEX_MAJOR) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_NS_LABEL_VERSION);
      goto Finish;
    }

    if ((LabelVersionMinor != NSINDEX_MINOR_1) &&
        (LabelVersionMinor != NSINDEX_MINOR_2)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_PROPERTY_NS_LABEL_VERSION);
      goto Finish;
    }

  } else if (ReturnCode == EFI_NOT_FOUND) {
    /** Default to 1.2 labels **/
    LabelVersionMajor = NSINDEX_MAJOR;
    LabelVersionMinor = NSINDEX_MINOR_2;
  } else {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  if (!Force) {
    ReturnCode = CheckAndConfirmAlignments(pCmd, pNvmDimmConfigProtocol, pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount,
        PersistentMemType, VolatileMode, ReservedPercent, ReserveDimm, UnitsToDisplay, &Confirmation);
    if (EFI_ERROR(ReturnCode) || !Confirmation) {
      goto Finish;
    }
  }
  ReturnCode = pNvmDimmConfigProtocol->CreateGoalConfig(pNvmDimmConfigProtocol, FALSE, pDimmIds, DimmIdsCount,
    pSocketIds, SocketIdsCount, PersistentMemType, VolatileMode, ReservedPercent, ReserveDimm,
    LabelVersionMajor, LabelVersionMinor, pCommandStatus);

  if (!EFI_ERROR(ReturnCode)) {
    
    ReturnCode = CreateCmdLineOutputStr(pCmd, &pShowGoalOutputArgs);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (UnitsOption != DISPLAY_SIZE_UNIT_UNKNOWN) {
      CHECK_RESULT(UnitsToStr(gNvmDimmCliHiiHandle, UnitsToDisplay, &pUnitsStr), Finish);
      pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR_SPACE FORMAT_STR FORMAT_STR L" " FORMAT_STR L" " FORMAT_STR, SHOW_VERB, pShowGoalOutputArgs, UNITS_OPTION, pUnitsStr, GOAL_TARGET);
    } else {
      pCommandStr = CatSPrintClean(pCommandStr, FORMAT_STR_SPACE FORMAT_STR FORMAT_STR, SHOW_VERB, pShowGoalOutputArgs, GOAL_TARGET);
    }
    FillCommandInput(pCommandStr, &ShowGoalCmdInput);
    ReturnCode = Parse(&ShowGoalCmdInput, &ShowGoalCmd);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed parsing command input");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    if (ShowGoalCmd.run == NULL) {
      NVDIMM_WARN("Couldn't find show -goal command");
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    if (!Force) {
      PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, CLI_CREATE_SUCCESS_STATUS);
    }
    ExecuteCmd(&ShowGoalCmd);
    FREE_POOL_SAFE(pCommandStr);
    goto FinishSkipPrinterProcess;
  } else {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CREATE_GOAL_COMMAND_STATUS_HEADER, CLI_INFO_ON, pCommandStatus);
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
FinishSkipPrinterProcess:
  FreeCommandInput(&ShowGoalCmdInput);
  FreeCommandStructure(&ShowGoalCmd);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pShowGoalOutputArgs);
  FREE_POOL_SAFE(pUnitsStr);
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
