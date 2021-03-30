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
#define IS_DIMM_UNLOCKED(SecurityStateBitmask) ((SecurityStateBitmask & SECURITY_MASK_ENABLED) && !(SecurityStateBitmask & SECURITY_MASK_LOCKED))

/**
  Command syntax definition
**/
struct Command CreateGoalCommand =
{
  CREATE_VERB,                                                   //!< verb
  {                                                              //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT,FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {FORCE_OPTION_SHORT, FORCE_OPTION, L"", L"",HELP_FORCE_DETAILS_TEXT, FALSE, ValueEmpty},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP,HELP_UNIT_DETAILS_TEXT, FALSE, ValueRequired},
#ifdef OS_BUILD
    {OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired}
#endif

  },
  {                                                              //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {GOAL_TARGET, L"", L"", TRUE, ValueEmpty},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueOptional}
  },
  {                                                              //!< properties
    {MEMORY_MODE_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {PERSISTENT_MEM_TYPE_PROPERTY, L"", HELP_TEXT_PERSISTENT_MEM_TYPE, FALSE, ValueRequired},
    {RESERVED_PROPERTY, L"", HELP_TEXT_PERCENT, FALSE, ValueRequired},
    {NS_LABEL_VERSION_PROPERTY, L"", HELP_TEXT_NS_LABEL_VERSION, FALSE, ValueRequired}
  },
  L"Provision capacity on one or more " PMEM_MODULES_STR L" into regions.",     //!< help
  CreateGoal,
  TRUE,                                               //!< enable print control support
};

/**
  Traverse targeted dimms to determine if Security is enabled in Unlocked state

  @param[in] SecurityFlag Security mask from FW

  @retval TRUE if at least one targeted dimm has security enabled in unlocked state
  @retval FALSE if none of targeted dimms have security enabled in unlocked state
**/
EFI_STATUS
EFIAPI
AreRequestedDimmsSecurityUnlocked(
  IN     DIMM_INFO *pDimmInfo,
  IN     UINT32 DimmCount,
  IN     UINT16 *ppDimmIds,
  IN     UINT32 pDimmIdsCount,
  OUT BOOLEAN *isDimmUnlocked
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 i;
  UINT32 j;

  NVDIMM_ENTRY();

  if (NULL == pDimmInfo
    || NULL == isDimmUnlocked
    || (NULL == ppDimmIds && 0 < pDimmIdsCount))
  {
    goto Finish;
  }

  *isDimmUnlocked = FALSE;

  if (0 == pDimmIdsCount) {
    for (i = 0; i < DimmCount; i++)
    {
      if (IS_DIMM_UNLOCKED(pDimmInfo[i].SecurityStateBitmask)) {
        *isDimmUnlocked = TRUE;
        break;
      }
    }
  }
  else {
    for (i = 0; i < pDimmIdsCount; i++)
    {
      for (j = 0; j < DimmCount; j++)
      {
        if ((ppDimmIds[i] == pDimmInfo[j].DimmID) && IS_DIMM_UNLOCKED(pDimmInfo[i].SecurityStateBitmask)) {
          *isDimmUnlocked = TRUE;
          break;
        }
      }
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  @param[in] pNvmDimmConfigProtocol is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatilePercent Volatile region size in percents
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percents
  @param[in] ReserveDimm Reserve one DIMM for use as not interleaved AppDirect memory
  @param[in] UnitsToDisplay The units to be used to display capacity
  @param[out] pConfirmation Confirmation from prompt

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
STATIC
EFI_STATUS
CheckAndConfirmAlignments(
  IN     struct Command *pCmd,
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     UINT8 PersistentMemType,
  IN     UINT32 VolatilePercent,
  IN     UINT32 ReservedPercent,
  IN     UINT8 ReserveDimm,
  IN     UINT16 UnitsToDisplay
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT32 VolatilePercentAligned = 0;
  UINT32 PercentDiff = 0;
  REGION_GOAL_PER_DIMM_INFO RegionConfigsInfo[MAX_DIMMS];
  UINT32 RegionConfigsCount = 0;
  UINT32 Index = 0;
  CHAR16 *pSingleStatusCodeMessage = NULL;
  UINT32 AppDirect1Regions = 0;
  UINT32 AppDirect2Regions = 0;
  UINT32 NumOfDimmsTargeted = 0;
  UINT32 MaxPMInterleaveSetsPerDie = 0;

  NVDIMM_ENTRY();

  ZeroMem(RegionConfigsInfo, sizeof(RegionConfigsInfo[0]) * MAX_DIMMS);

  if (pNvmDimmConfigProtocol == NULL) {
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

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
    &NumOfDimmsTargeted,
    &MaxPMInterleaveSetsPerDie,
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

  if (pCommandStatus->ObjectStatusCount > 0) {
    PRINTER_PROMPT_COMMAND_STATUS(pCmd->pPrintCtx, EFI_SUCCESS, L"", L"", pCommandStatus);
  }

  switch (pCommandStatus->GeneralStatus) {
    case NVM_WARN_IMC_DDR_PMM_NOT_PAIRED:
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_IMC_DDR_PMM_NOT_PAIRED);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
      break;

    case NVM_WARN_NMFM_RATIO_LOWER_VIOLATION_1to3_6:
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_NMFM_RATIO_LOWER_VIOLATION_1to3_6);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage, TWOLM_NMFM_RATIO_LOWER_3_6_STR);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
      break;

    case NVM_WARN_NMFM_RATIO_LOWER_VIOLATION_1to2:
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_NMFM_RATIO_LOWER_VIOLATION_1to2);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage, TWOLM_NMFM_RATIO_LOWER_2_STR);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
      break;

    case NVM_WARN_NMFM_RATIO_UPPER_VIOLATION_1to16:
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_NMFM_RATIO_UPPER_VIOLATION_1to16);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage, TWOLM_NMFM_RATIO_UPPER_16);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
      break;

    case NVM_WARN_NMFM_RATIO_UPPER_VIOLATION_1to8:
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_NMFM_RATIO_UPPER_VIOLATION_1to8);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage, TWOLM_NMFM_RATIO_UPPER_8);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
      break;
  }

  if (PercentDiff > PROMPT_ALIGN_PERCENTAGE) {
     PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, L"\n" CLI_CREATE_GOAL_PROMPT_VOLATILE L"\n");
  }

  if ((pCommandStatus->GeneralStatus == NVM_WARN_REGION_MAX_PM_INTERLEAVE_SETS_EXCEEDED) && RegionConfigsCount > 0) {
    for (Index = 0; Index < RegionConfigsCount; ++Index) {
      if (RegionConfigsInfo[Index].AppDirectSize[APPDIRECT_1_INDEX] > 0) {
        AppDirect1Regions++;
      }

      if (RegionConfigsInfo[Index].AppDirectSize[APPDIRECT_2_INDEX] > 0) {
        AppDirect2Regions++;
      }
    }

    if (PersistentMemType == PM_TYPE_AD) {
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_REGION_MAX_AD_PM_INTERLEAVE_SETS_EXCEEDED);
      pSingleStatusCodeMessage = CatSPrintClean(NULL, pSingleStatusCodeMessage, MaxPMInterleaveSetsPerDie, AppDirect2Regions);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, L"\n" FORMAT_STR_NL, pSingleStatusCodeMessage);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
    }

    if (PersistentMemType == PM_TYPE_AD_NI) {
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_REGION_MAX_AD_NI_PM_INTERLEAVE_SETS_EXCEEDED);
      pSingleStatusCodeMessage = CatSPrintClean(NULL, pSingleStatusCodeMessage, MaxPMInterleaveSetsPerDie, (NumOfDimmsTargeted - AppDirect1Regions));
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, L"\n" FORMAT_STR_NL, pSingleStatusCodeMessage);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
    }
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
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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
  UINT16 UnitsToDisplay = FixedPcdGet16(PcdDcpmmCliDefaultCapacityUnit);
  CHAR16 *pUnitsStr = NULL;
  CHAR16 *pCommandStr = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  UINT16 LabelVersionMajor = 0;
  UINT16 LabelVersionMinor = 0;
  INTEL_DIMM_CONFIG *pIntelDIMMConfig = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pShowGoalOutputArgs = NULL;
  CHAR16 *pSingleStatusCodeMessage = NULL;
  UINT32 MaxPMInterleaveSetsPerDie = 0;
  BOOLEAN isDimmUnlocked = FALSE;
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
  ReturnCode = GetAllDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_SECURITY, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  if (containsOption(pCmd, FORCE_OPTION) || containsOption(pCmd, FORCE_OPTION_SHORT) || XML == pPrinterCtx->FormatType) {
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
    PersistentMemType = PM_TYPE_RESERVED;
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
        PersistentMemType, VolatileMode, ReservedPercent, ReserveDimm, UnitsToDisplay);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = AreRequestedDimmsSecurityUnlocked(pDimms, DimmCount, pDimmIds, DimmIdsCount, &isDimmUnlocked);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    // send warning if security unlocked for target dimms
    if (isDimmUnlocked) {
      PRINTER_PROMPT_MSG(pPrinterCtx, ReturnCode, CLI_WARN_GOAL_CREATION_SECURITY_UNLOCKED);
    }

    ReturnCode = PromptYesNo(&Confirmation);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_PROMPT_INVALID);
      NVDIMM_DBG("Failed on PromptedInput");
      goto Finish;
    }
    else if (!Confirmation) {
      goto Finish;
    }
  }
  ReturnCode = pNvmDimmConfigProtocol->CreateGoalConfig(pNvmDimmConfigProtocol, FALSE, pDimmIds, DimmIdsCount,
    pSocketIds, SocketIdsCount, PersistentMemType, VolatileMode, ReservedPercent, ReserveDimm,
    LabelVersionMajor, LabelVersionMinor, &MaxPMInterleaveSetsPerDie, pCommandStatus);

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

    if (pCommandStatus->GeneralStatus == NVM_WARN_REGION_AD_NI_PM_INTERLEAVE_SETS_REDUCED) {
      pSingleStatusCodeMessage = GetSingleNvmStatusCodeMessage(gNvmDimmCliHiiHandle, NVM_WARN_REGION_AD_NI_PM_INTERLEAVE_SETS_REDUCED);
      pSingleStatusCodeMessage = CatSPrintClean(NULL, pSingleStatusCodeMessage, MaxPMInterleaveSetsPerDie);
      PRINTER_PROMPT_MSG(pCmd->pPrintCtx, ReturnCode, pSingleStatusCodeMessage);
      FREE_POOL_SAFE(pSingleStatusCodeMessage);
    }

    goto FinishSkipPrinterProcess;
  }
  else {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CREATE_GOAL_COMMAND_STATUS_HEADER, CLI_INFO_ON, pCommandStatus);
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
FinishSkipPrinterProcess:
  FreeCommandInput(&ShowGoalCmdInput);
  FreeCommandStructure(&ShowGoalCmd);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pCommandStr);
  FREE_POOL_SAFE(pSocketIds);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pShowGoalOutputArgs);
  FREE_POOL_SAFE(pUnitsStr);
  FREE_POOL_SAFE(pCommandStr);
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
