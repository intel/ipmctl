/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowDimmsCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <Convert.h>
#include <NvmInterface.h>
#include "Common.h"
#include "NvmDimmCli.h"
#include <NvmWorkarounds.h>
#include <ShowTopologyCommand.h>
#include <NvmHealth.h>
#include <DataSet.h>
#include <Printer.h>
#include <ReadRunTimePreferences.h>

#define DS_ROOT_PATH                      L"/DimmList"
#define DS_DIMM_PATH                      L"/DimmList/Dimm"
#define DS_DIMM_INDEX_PATH                L"/DimmList/Dimm[%d]"


/*
 *  PRINT LIST ATTRIBUTES
 *  ---DimmId=0x0001---
 *     Capacity=125.7 GiB
 *     LockState=Locked
 *     HealthState=Healthy
 *     ...
 */
PRINTER_LIST_ATTRIB ShowDimmListAttributes =
{
 {
    {
      DIMM_NODE_STR,                                        //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                           //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                           //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

#ifdef OS_BUILD
/*
*  PRINTER TABLE ATTRIBUTES (6 columns)
*   DimmID | Capacity | HealthState | ActionRequired | LockState | FWVersion
*   ========================================================================
*   0x0001 | X        | X           | X              | X         | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowDimmTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                         //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR     //COLUMN DATA PATH
    },
    {
      CAPACITY_STR,                               //COLUMN HEADER
      CAPACITY_MAX_STR_WIDTH,                     //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM CAPACITY_STR    //COLUMN DATA PATH
    },
    {
      HEALTH_STR,                                 //COLUMN HEADER
      HEALTH_MAX_STR_WIDTH,                       //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM HEALTH_STR      //COLUMN DATA PATH
    },
    {
      ACTION_REQUIRED_STR,                                  //COLUMN HEADER
      AR_MAX_STR_WIDTH,                                     //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM ACTION_REQUIRED_STR       //COLUMN DATA PATH
    },
    {
      SECURITY_STR,                               //COLUMN HEADER
      SECURITY_MAX_STR_WIDTH,                     //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM SECURITY_STR    //COLUMN DATA PATH
    },
    {
      FW_VER_STR,                                 //COLUMN HEADER
      FW_VERSION_MAX_STR_WIDTH,                   //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM FW_VER_STR      //COLUMN DATA PATH
    }
  }
};
#else
/*
*  PRINTER TABLE ATTRIBUTES (6 columns)
*   DimmID | Capacity | LockState | HealthState | FWVersion
*   ========================================================
*   0x0001 | X        | X         | X           | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowDimmTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                         //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR     //COLUMN DATA PATH
    },
    {
      CAPACITY_STR,                               //COLUMN HEADER
      CAPACITY_MAX_STR_WIDTH,                     //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM CAPACITY_STR    //COLUMN DATA PATH
    },
    {
      SECURITY_STR,                               //COLUMN HEADER
      SECURITY_MAX_STR_WIDTH,                     //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM SECURITY_STR    //COLUMN DATA PATH
    },
    {
      HEALTH_STR,                                 //COLUMN HEADER
      HEALTH_MAX_STR_WIDTH,                       //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM HEALTH_STR      //COLUMN DATA PATH
    },
    {
      FW_VER_STR,                                 //COLUMN HEADER
      FW_VERSION_MAX_STR_WIDTH,                   //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM FW_VER_STR      //COLUMN DATA PATH
    }
  }
};
#endif

PRINTER_DATA_SET_ATTRIBS ShowDimmDataSetAttribs =
{
  &ShowDimmListAttributes,
  &ShowDimmTableAttributes
};

/* Command syntax definition */
struct Command ShowDimmsCommand =
{
  SHOW_VERB,                                        //!< verb
  /**
    options
  **/
  {
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {ALL_OPTION_SHORT, ALL_OPTION, L"", L"",HELP_ALL_DETAILS_TEXT, FALSE, ValueEmpty},
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES,HELP_DISPLAY_DETAILS_TEXT, FALSE, ValueRequired},
    {UNITS_OPTION_SHORT, UNITS_OPTION, L"", UNITS_OPTION_HELP,HELP_UNIT_DETAILS_TEXT, FALSE, ValueRequired}
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#endif
  },
  /**
    targets
  **/
  {
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional},
    {SOCKET_TARGET, L"", HELP_TEXT_SOCKET_IDS, FALSE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                //!< properties
  L"Show information about one or more DIMMs.",           //!< help
  ShowDimms,                                              //!< run function
  TRUE,                                                   //!< enable print control support
};

CHAR16 *mppAllowedShowDimmsDisplayValues[] =
{
  DIMM_ID_STR,
  SOCKET_ID_STR,
  FW_VER_STR,
  FW_API_VER_STR,
  INTERFACE_FORMAT_CODE_STR,
  CAPACITY_STR,
  MANAGEABILITY_STR,
  SECURITY_STR,
  HEALTH_STR,
  HEALTH_STATE_REASON_STR,
  FORM_FACTOR_STR,
  VENDOR_ID_STR,
  MANUFACTURER_ID_STR,
  DEVICE_ID_STR,
  REVISION_ID_STR,
  SUBSYSTEM_VENDOR_ID_STR,
  SUBSYSTEM_DEVICE_ID_STR,
  SUBSYSTEM_REVISION_ID_STR,
  CONTROLLER_REVISION_ID_STR,
  MANUFACTURING_INFO_VALID,
  MANUFACTURING_LOCATION,
  MANUFACTURING_DATE,
  PART_NUMBER_STR,
  SERIAL_NUMBER_STR,
  DEVICE_LOCATOR_STR,
  MEMORY_CONTROLLER_STR,
  DATA_WIDTH_STR,
  TOTAL_WIDTH_STR,
  SPEED_STR,
  MEMORY_MODE_CAPACITY_STR,
  APPDIRECT_MODE_CAPACITY_STR,
  UNCONFIGURED_CAPACITY_STR,
  PACKAGE_SPARING_ENABLED_STR,
  PACKAGE_SPARING_CAPABLE_STR,
  PACKAGE_SPARES_AVAILABLE_STR,
  IS_NEW_STR,
  BANK_LABEL_STR,
  MEMORY_TYPE_STR,
  AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY,
  MANUFACTURER_STR,
  CHANNEL_ID_STR,
  SLOT_ID_STR,
  CHANNEL_POS_STR,
  PEAK_POWER_BUDGET_STR,
  AVG_POWER_LIMIT_STR,
  AVG_POWER_TIME_CONSTANT_STR,
  TURBO_MODE_STATE_STR,
  TURBO_POWER_LIMIT_STR,
  MAX_AVG_POWER_LIMIT_STR,
  MAX_TURBO_MODE_POWER_CONSUMPTION_STR,
  MAX_AVERAGE_POWER_TIME_CONSTANT,
  AVERAGE_POWER_TIME_CONSTANT_STEP,
  LATCHED_LAST_SHUTDOWN_STATUS_STR,
  UNLATCHED_LAST_SHUTDOWN_STATUS_STR,
  DIMM_HANDLE_STR,
  DIMM_UID_STR,
  MODES_SUPPORTED_STR,
  SECURITY_CAPABILITIES_STR,
  MASTER_PASS_ENABLED_STR,
  DIMM_CONFIG_STATUS_STR,
  SKU_VIOLATION_STR,
  ARS_STATUS_STR,
  OVERWRITE_STATUS_STR,
  LAST_SHUTDOWN_TIME_STR,
  INACCESSIBLE_CAPACITY_STR,
  RESERVED_CAPACITY_STR,
  VIRAL_POLICY_STR,
  VIRAL_STATE_STR,
  AIT_DRAM_ENABLED_STR,
  BOOT_STATUS_STR,
  PHYSICAL_ID_STR,
  ERROR_INJECT_ENABLED_STR,
  MEDIA_TEMP_INJ_ENABLED_STR,
  SW_TRIGGERS_ENABLED_STR,
  SW_TRIGGER_ENABLED_DETAILS_STR,
  POISON_ERR_INJ_CTR_STR,
  POISON_ERR_CLR_CTR_STR,
  MEDIA_TEMP_INJ_CTR_STR,
  SW_TRIGGER_CTR_STR,
  BOOT_STATUS_REGISTER_STR,
#ifdef OS_BUILD
  ACTION_REQUIRED_STR,
  ACTION_REQUIRED_EVENTS_STR,
#endif
  DCPMM_AVERAGE_POWER_STR,
  AVERAGE_12V_POWER_STR,
  AVERAGE_1_2V_POWER_STR
};

CHAR16 *mppAllowedShowDimmsConfigStatuses[] = {
  CONFIG_STATUS_VALUE_VALID,
  CONFIG_STATUS_VALUE_NOT_CONFIG,
  CONFIG_STATUS_VALUE_BAD_CONFIG,
  CONFIG_STATUS_VALUE_BROKEN_INTERLEAVE,
  CONFIG_STATUS_VALUE_REVERTED,
  CONFIG_STATUS_VALUE_UNSUPPORTED,
};

CHAR16 *pOnlyManageableAllowedDisplayValues[] = {
  MANUFACTURER_ID_STR,
  MEMORY_MODE_CAPACITY_STR,
  APPDIRECT_MODE_CAPACITY_STR,
  UNCONFIGURED_CAPACITY_STR,
  INACCESSIBLE_CAPACITY_STR,
  RESERVED_CAPACITY_STR,
  PACKAGE_SPARING_CAPABLE_STR,
  PACKAGE_SPARING_ENABLED_STR,
  PACKAGE_SPARES_AVAILABLE_STR,
  IS_NEW_STR,
  AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY,
  VIRAL_POLICY_STR,
  VIRAL_STATE_STR,
  PEAK_POWER_BUDGET_STR,
  AVG_POWER_LIMIT_STR,
  AVG_POWER_TIME_CONSTANT_STR,
  TURBO_MODE_STATE_STR,
  TURBO_POWER_LIMIT_STR,
  MAX_AVG_POWER_LIMIT_STR,
  MAX_TURBO_MODE_POWER_CONSUMPTION_STR,
  MAX_AVERAGE_POWER_TIME_CONSTANT,
  AVERAGE_POWER_TIME_CONSTANT_STEP,
  LATCHED_LAST_SHUTDOWN_STATUS_STR,
  UNLATCHED_LAST_SHUTDOWN_STATUS_STR,
  LAST_SHUTDOWN_TIME_STR,
  MODES_SUPPORTED_STR,
  SECURITY_CAPABILITIES_STR,
  MASTER_PASS_ENABLED_STR,
  DIMM_CONFIG_STATUS_STR,
  SKU_VIOLATION_STR,
  ARS_STATUS_STR,
  OVERWRITE_STATUS_STR,
  AIT_DRAM_ENABLED_STR,
  BOOT_STATUS_STR,
  ERROR_INJECT_ENABLED_STR,
  MEDIA_TEMP_INJ_ENABLED_STR,
  SW_TRIGGERS_ENABLED_STR,
  SW_TRIGGER_ENABLED_DETAILS_STR,
  POISON_ERR_INJ_CTR_STR,
  POISON_ERR_CLR_CTR_STR,
  MEDIA_TEMP_INJ_CTR_STR,
  SW_TRIGGER_CTR_STR,
  BOOT_STATUS_REGISTER_STR,
  DCPMM_AVERAGE_POWER_STR,
  AVERAGE_12V_POWER_STR,
  AVERAGE_1_2V_POWER_STR,
  EXTENDED_ADR_ENABLED_STR,
  PPC_EXTENDED_ADR_ENABLED_STR
};
/* local functions */
STATIC CHAR16 *ManageabilityToString(UINT8 ManageabilityState);
STATIC CHAR16 *FormFactorToString(UINT8 FormFactor);
STATIC CHAR16 *OverwriteDimmStatusToStr(UINT8 OverwriteDimmStatus);

/*
 * Register the show dimms command
 */
EFI_STATUS
RegisterShowDimmsCommand(
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVDIMM_ENTRY();
  Rc = RegisterCommand(&ShowDimmsCommand);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Execute the show dimms command
**/
EFI_STATUS
ShowDimms(
  IN     struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT32 DimmCount = 0;
  UINT32 UninitializedDimmCount = 0;
  DIMM_INFO *pDimms = NULL;
  DIMM_INFO *pUninitializedDimms = NULL;
  DIMM_INFO *pAllDimms = NULL;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketsNum = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pSocketsValue = NULL;
  CHAR16 *pSecurityStr = NULL;
  CHAR16 *pHealthStr = NULL;
  CHAR16 *pHealthStateReasonStr = NULL;
  CHAR16 *pManageabilityStr = NULL;
  CHAR16 *pFormFactorStr = NULL;
  CHAR16 *pDimmsValue = NULL;
  CHAR16 TmpFwVerString[MAX(FW_VERSION_LEN, FW_API_VERSION_LEN)];
  UINT32 DimmIndex = 0;
  UINT32 Index1 = 0;
  UINT32 Index2 = 0;
  UINT32 Index3 = 0;
  UINT16 UnitsOption = DISPLAY_SIZE_UNIT_UNKNOWN;
  UINT16 UnitsToDisplay = FixedPcdGet32(PcdDcpmmCliDefaultCapacityUnit);
  BOOLEAN Found = FALSE;
  BOOLEAN ShowAll = FALSE;
  BOOLEAN ContainSocketTarget = FALSE;
  COMMAND_STATUS *pCommandStatus = NULL;
  CHAR16 *pAttributeStr = NULL;
  CHAR16 *pCapacityStr = NULL;
  CHAR16 *pDimmErrStr = NULL;
  LAST_SHUTDOWN_STATUS_DETAILS_COMBINED LatchedLastShutdownStatusDetails;
  LAST_SHUTDOWN_STATUS_DETAILS_COMBINED UnlatchedLastShutdownStatusDetails;
  DISPLAY_PREFERENCES DisplayPreferences;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  BOOLEAN ByteAddressable = FALSE;
  BOOLEAN BlockAddressable = FALSE;
  UINT16  BootStatusBitMask = 0;
  UINT64  BootStatusRegister = 0;
  CHAR16 *pSteppingStr = NULL;
  CMD_DISPLAY_OPTIONS *pDispOptions = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  BOOLEAN FIS_2_0 = FALSE;
  BOOLEAN volatile DimmIsOkToDisplay[MAX_DIMMS];

  NVDIMM_ENTRY();
  ZeroMem(TmpFwVerString, sizeof(TmpFwVerString));
  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));
  ZeroMem(DimmStr, sizeof(DimmStr));
  ZeroMem(&LatchedLastShutdownStatusDetails, sizeof(LatchedLastShutdownStatusDetails));
  ZeroMem(&UnlatchedLastShutdownStatusDetails, sizeof(UnlatchedLastShutdownStatusDetails));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  for (Index1 = 0; Index1 < MAX_DIMMS; Index1++) {
    DimmIsOkToDisplay[Index1] = FALSE;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  pDispOptions = AllocateZeroPool(sizeof(CMD_DISPLAY_OPTIONS));
  if (NULL == pDispOptions) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  ReturnCode = CheckAllAndDisplayOptions(pCmd, mppAllowedShowDimmsDisplayValues,
    ALLOWED_DISP_VALUES_COUNT(mppAllowedShowDimmsDisplayValues), pDispOptions);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckAllAndDisplayOptions has returned error. Code " FORMAT_EFI_STATUS "\n", ReturnCode);
    goto Finish;
  }

  ContainSocketTarget = ContainTarget(pCmd, SOCKET_TARGET);

  /**
    if sockets were specified
  **/
  if (ContainSocketTarget) {
    pSocketsValue = GetTargetValue(pCmd, SOCKET_TARGET);
    ReturnCode = GetUintsFromString(pSocketsValue, &pSocketIds, &SocketsNum);
    if (EFI_ERROR(ReturnCode)) {
      /** Error Code returned by function above **/
      NVDIMM_DBG("GetUintsFromString returned error");
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_TARGET_SOCKET);
      goto Finish;
    }
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

  /** make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimmCount(pNvmDimmConfigProtocol, &UninitializedDimmCount);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  if (0 == DimmCount && 0 == UninitializedDimmCount) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_DIMMS);
    goto Finish;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);
  pUninitializedDimms = AllocateZeroPool(sizeof(*pUninitializedDimms) * UninitializedDimmCount);

  if (pDimms == NULL || pUninitializedDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  /** retrieve the DIMM list, populated for the minimal show -dimm call for now **/
  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
    DIMM_INFO_CATEGORY_SECURITY | DIMM_INFO_CATEGORY_SMART_AND_HEALTH, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetUninitializedDimms(pNvmDimmConfigProtocol, UninitializedDimmCount,
    pUninitializedDimms);

  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found thru SMBUS");
    goto Finish;
  }

  /** if a specific DIMM pid was passed in, set it **/
  if (pCmd->targets[0].pTargetValueStr && StrLen(pCmd->targets[0].pTargetValueStr) > 0) {
    pAllDimms = AllocateZeroPool(sizeof(*pAllDimms) * (DimmCount + UninitializedDimmCount));
    if (NULL == pAllDimms) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    CopyMem_S(pAllDimms, sizeof(*pAllDimms) * (DimmCount), pDimms, sizeof(*pDimms) * DimmCount);
    CopyMem_S(&pAllDimms[DimmCount], sizeof(*pAllDimms) * (UninitializedDimmCount), pUninitializedDimms, sizeof(*pUninitializedDimms) * UninitializedDimmCount);
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pDimmsValue, pAllDimms, DimmCount + UninitializedDimmCount, &pDimmIds,
      &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }

    /*Mark each dimm as ok to display based on the dimms passed by the user*/
    for (Index1 = 0; Index1 < DimmCount + UninitializedDimmCount; Index1++) {
      for (Index2 = 0; Index2 < DimmIdsNum; Index2++) {
        if (pAllDimms[Index1].DimmID == pDimmIds[Index2]) {
          DimmIsOkToDisplay[Index1] = TRUE;
        }
      }
    }
  }
  else {
    /*Since no dimms were specified, mark them all as ok to display*/
    for (DimmIndex = 0; DimmIndex < MAX_DIMMS; DimmIndex++) {
      DimmIsOkToDisplay[DimmIndex] = TRUE;
    }
  }

  if (SocketsNum > 0) {
    Found = FALSE;
    /*Only display sockets which match the dimms that the user has indicated*/
    for (DimmIndex = 0; DimmIndex < DimmCount; DimmIndex++) {
      if (DimmIsOkToDisplay[DimmIndex] == TRUE &&
        ContainUint(pSocketIds, SocketsNum, pDimms[DimmIndex].SocketId)) {
        Found = TRUE;
        break;
      }
    }

    if (!Found) {
      ReturnCode = EFI_NOT_FOUND;
      if (DimmIdsNum > 0) {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_SPECIFIED_DIMMS_ON_SPECIFIED_SOCKET);
      }
      else {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_DIMMS_ON_SOCKET);
      }

      NVDIMM_DBG("No DIMMs on provided Socket");
      goto Finish;
    }
  }

  /** display a summary table of all dimms **/
  if (!pDispOptions->AllOptionSet && !pDispOptions->DisplayOptionSet) {

    for (DimmIndex = 0; DimmIndex < DimmCount; DimmIndex++) {
      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pDimms[DimmIndex].SocketId)) {
        continue;
      }

      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[DimmIndex].DimmID)) {
        continue;
      }

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex);

      ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
      pHealthStr = HealthToString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].HealthState);

      if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_SECURITY_INFO) {
        pSecurityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
      }
      else {
        pSecurityStr = SecurityStateBitmaskToString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].SecurityStateBitmask);
      }

      ConvertFwVersion(TmpFwVerString, pDimms[DimmIndex].FwVer.FwProduct,
        pDimms[DimmIndex].FwVer.FwRevision, pDimms[DimmIndex].FwVer.FwSecurityVersion, pDimms[DimmIndex].FwVer.FwBuild);

      ReturnCode = GetPreferredDimmIdAsString(pDimms[DimmIndex].DimmHandle, pDimms[DimmIndex].DimmUid,
        DimmStr, MAX_DIMM_UID_LENGTH);
      pDimmErrStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);

      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, HEALTH_STR, pHealthStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SECURITY_STR, pSecurityStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_VER_STR, TmpFwVerString);

#ifdef OS_BUILD
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ACTION_REQUIRED_STR, FORMAT_DEC, pDimms[DimmIndex].ActionRequired);
#endif
      if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_UID) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, pDimmErrStr);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      }
      FREE_POOL_SAFE(pDimmErrStr);
      FREE_POOL_SAFE(pHealthStr);
      FREE_POOL_SAFE(pSecurityStr);
      FREE_POOL_SAFE(pCapacityStr);
    }

    /** show dimms from Uninitialized list **/

    for (DimmIndex = 0; DimmIndex < UninitializedDimmCount; DimmIndex++) {
      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pUninitializedDimms[DimmIndex].SmbusAddress.Cpu)) {
        continue;
      }

      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pUninitializedDimms[DimmIndex].DimmID)) {
        continue;
      }

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmCount + DimmIndex);

      pHealthStr = HealthToString(gNvmDimmCliHiiHandle, pUninitializedDimms[DimmIndex].HealthState);

      ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmCliHiiHandle,
        pUninitializedDimms[DimmIndex].HealthStatusReason, &pHealthStateReasonStr);
      if (pHealthStateReasonStr == NULL || EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      ReturnCode = GetPreferredDimmIdAsString(pUninitializedDimms[DimmIndex].DimmHandle, pUninitializedDimms[DimmIndex].DimmUid, DimmStr,
        MAX_DIMM_UID_LENGTH);
      pDimmErrStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);

      ConvertFwVersion(TmpFwVerString, pUninitializedDimms[DimmIndex].FwVer.FwProduct,
        pUninitializedDimms[DimmIndex].FwVer.FwRevision, pUninitializedDimms[DimmIndex].FwVer.FwSecurityVersion,
        pUninitializedDimms[DimmIndex].FwVer.FwBuild);

      TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pUninitializedDimms[DimmIndex].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
      KEEP_ERROR(ReturnCode, TempReturnCode);

      if (pUninitializedDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_UID) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, pDimmErrStr);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      }

      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, HEALTH_STR, pHealthStr);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SECURITY_STR, NOT_APPLICABLE_SHORT_STR);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_VER_STR, TmpFwVerString);

#ifdef OS_BUILD
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ACTION_REQUIRED_STR, NOT_APPLICABLE_SHORT_STR);
#endif
      FREE_POOL_SAFE(pDimmErrStr);
      FREE_POOL_SAFE(pHealthStr);
      FREE_POOL_SAFE(pCapacityStr);
      FREE_POOL_SAFE(pHealthStateReasonStr);
    }
  }

  /** display detailed view **/
  else {
    // Collect all properties if the user calls "show -a -dimm"
    ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
      DIMM_INFO_CATEGORY_ALL, pDimms);
    ShowAll = (!pDispOptions->AllOptionSet && !pDispOptions->DisplayOptionSet) || pDispOptions->AllOptionSet;

    /** show dimms from Initialized list **/
    for (DimmIndex = 0; DimmIndex < DimmCount; DimmIndex++) {
      /** matching pid **/
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[DimmIndex].DimmID)) {
        continue;
      }

      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pDimms[DimmIndex].SocketId)) {
        continue;
      }

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex);

      if (pDimms[DimmIndex].FwVer.FwApiMajor >= 2) {
        FIS_2_0 = TRUE;
      }

      /** always print the DimmID **/
      ReturnCode = GetPreferredDimmIdAsString(pDimms[DimmIndex].DimmHandle, pDimms[DimmIndex].DimmUid, DimmStr,
        MAX_DIMM_UID_LENGTH);
      if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_UID) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, UNKNOWN_ATTRIB_VAL);
      }
      else {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
      }

      /** Capacity **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CAPACITY_STR))) {
        ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /** Security State **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SECURITY_STR))) {
        if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_SECURITY_INFO) {
          pSecurityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
        }
        else {
          pSecurityStr = SecurityStateBitmaskToString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].SecurityStateBitmask);
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SECURITY_STR, pSecurityStr);
        FREE_POOL_SAFE(pSecurityStr);
      }

      /** Health State **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, HEALTH_STR))) {
        pHealthStr = HealthToString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].HealthState);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, HEALTH_STR, pHealthStr);
        FREE_POOL_SAFE(pHealthStr);
      }

      /** Health State Reason**/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, HEALTH_STATE_REASON_STR))) {
        ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmCliHiiHandle,
          pDimms[DimmIndex].HealthStatusReason, &pHealthStateReasonStr);
        if (pHealthStateReasonStr == NULL || EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, HEALTH_STATE_REASON_STR, pHealthStateReasonStr);
        FREE_POOL_SAFE(pHealthStateReasonStr);
      }

      /** FwVersion **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FW_VER_STR))) {
        ConvertFwVersion(TmpFwVerString, pDimms[DimmIndex].FwVer.FwProduct, pDimms[DimmIndex].FwVer.FwRevision,
          pDimms[DimmIndex].FwVer.FwSecurityVersion, pDimms[DimmIndex].FwVer.FwBuild);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_VER_STR, TmpFwVerString);
      }

      /** FwApiVersion **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FW_API_VER_STR))) {
        ConvertFwApiVersion(TmpFwVerString, pDimms[DimmIndex].FwVer.FwApiMajor, pDimms[DimmIndex].FwVer.FwApiMinor);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_API_VER_STR, TmpFwVerString);
      }

      /** InterfaceFormatCode **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, INTERFACE_FORMAT_CODE_STR))) {
        if (pDimms[DimmIndex].InterfaceFormatCodeNum <= MAX_IFC_NUM) {
          CHAR16 *tmpIfc = NULL;
          for (Index2 = 0; Index2 < pDimms[DimmIndex].InterfaceFormatCodeNum; Index2++) {
            if (pDimms[DimmIndex].InterfaceFormatCode[Index2] == DCPMM_FMT_CODE_APP_DIRECT) {
              ByteAddressable = TRUE;
            }
            else if (pDimms[DimmIndex].InterfaceFormatCode[Index2] == DCPMM_FMT_CODE_STORAGE) {
              BlockAddressable = TRUE;
            }
          }

          if (ByteAddressable) {
            tmpIfc = CatSPrint(tmpIfc, FORMAT_HEX L" ", DCPMM_FMT_CODE_APP_DIRECT);
            tmpIfc = CatSPrint(tmpIfc, FORMAT_CODE_APP_DIRECT_STR);
          }

          if (pDimms[DimmIndex].InterfaceFormatCodeNum > 1) {
            tmpIfc = CatSPrint(tmpIfc, L", ");
          }

          if (BlockAddressable) {
            tmpIfc = CatSPrint(tmpIfc, FORMAT_HEX L" ", FORMAT_CODE_STORAGE_STR);
            tmpIfc = CatSPrint(tmpIfc, FORMAT_CODE_STORAGE_STR);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, INTERFACE_FORMAT_CODE_STR, tmpIfc);
          FREE_POOL_SAFE(tmpIfc);
        }
      }

      /** Manageability **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MANAGEABILITY_STR))) {
        pManageabilityStr = ManageabilityToString(pDimms[DimmIndex].ManageabilityState);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MANAGEABILITY_STR, pManageabilityStr);
        FREE_POOL_SAFE(pManageabilityStr);
      }

      /** PhysicalID **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PHYSICAL_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PHYSICAL_ID_STR, FORMAT_HEX, pDimms[DimmIndex].DimmID);
      }

      /** DimmHandle **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DIMM_HANDLE_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DIMM_HANDLE_STR, FORMAT_HEX, pDimms[DimmIndex].DimmHandle);
      }

      /** DimmUID **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DIMM_UID_STR))) {
        if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_UID) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_UID_STR, UNKNOWN_ATTRIB_VAL);
        }
        else {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_UID_STR, pDimms[DimmIndex].DimmUid);
        }
      }

      /** SocketId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SOCKET_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pDimms[DimmIndex].SocketId);
      }

      /** MemoryControllerId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEMORY_CONTROLLER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, FORMAT_HEX, pDimms[DimmIndex].ImcId);
      }

      /** ChannelID **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CHANNEL_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_ID_STR, FORMAT_HEX, pDimms[DimmIndex].ChannelId);
      }

      /** ChannelPos **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CHANNEL_POS_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_POS_STR, FORMAT_INT32, pDimms[DimmIndex].ChannelPos);
      }

      /** MemoryType **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEMORY_TYPE_STR))) {
        pAttributeStr = MemoryTypeToStr(pDimms[DimmIndex].MemoryType);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_TYPE_STR, pAttributeStr);
        FREE_POOL_SAFE(pAttributeStr);
      }

      /** ManufacturerStr **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MANUFACTURER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MANUFACTURER_STR, pDimms[DimmIndex].ManufacturerStr);
      }

      /** VendorId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, VENDOR_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, VENDOR_ID_STR, FORMAT_HEX, EndianSwapUint16(pDimms[DimmIndex].VendorId));
      }

      /** DeviceId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DEVICE_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DEVICE_ID_STR, FORMAT_HEX, EndianSwapUint16(pDimms[DimmIndex].DeviceId));
      }

      /** RevisionId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, REVISION_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, REVISION_ID_STR, FORMAT_HEX, pDimms[DimmIndex].Rid);
      }

      /** SubsytemVendorId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SUBSYSTEM_VENDOR_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SUBSYSTEM_VENDOR_ID_STR, FORMAT_HEX, EndianSwapUint16(pDimms[DimmIndex].SubsystemVendorId));
      }

      /** SubsytemDeviceId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SUBSYSTEM_DEVICE_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SUBSYSTEM_DEVICE_ID_STR, FORMAT_HEX, pDimms[DimmIndex].SubsystemDeviceId);
      }

      /** SubsytemRevisionId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SUBSYSTEM_REVISION_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SUBSYSTEM_REVISION_ID_STR, FORMAT_HEX, pDimms[DimmIndex].SubsystemRid);
      }

      /** DeviceLocator **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DEVICE_LOCATOR_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DEVICE_LOCATOR_STR, pDimms[DimmIndex].DeviceLocator);
      }

      /** ManufacturingInfoValid **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MANUFACTURING_INFO_VALID))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MANUFACTURING_INFO_VALID, FORMAT_INT32, pDimms[DimmIndex].ManufacturingInfoValid);
      }

      /** ManufacturingLocation **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MANUFACTURING_LOCATION))) {
        if (pDimms[DimmIndex].ManufacturingInfoValid) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MANUFACTURING_LOCATION, FORMAT_HEX_PREFIX FORMAT_UINT8_HEX, pDimms[DimmIndex].ManufacturingLocation);
        }
        else {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MANUFACTURING_LOCATION, NOT_APPLICABLE_SHORT_STR);
        }
      }

      /** ManufacturingDate **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MANUFACTURING_DATE))) {
        if (pDimms[DimmIndex].ManufacturingInfoValid) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MANUFACTURING_DATE, FORMAT_SHOW_DIMM_MANU_DATE, pDimms[DimmIndex].ManufacturingDate & 0xFF, (pDimms[DimmIndex].ManufacturingDate >> 8) & 0xFF);
        }
        else {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MANUFACTURING_DATE, NOT_APPLICABLE_SHORT_STR);
        }
      }

      /** SerialNumber **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SERIAL_NUMBER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SERIAL_NUMBER_STR, FORMAT_HEX_PREFIX FORMAT_UINT32_HEX, EndianSwapUint32(pDimms[DimmIndex].SerialNumber));
      }

      /** PartNumber **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PART_NUMBER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PART_NUMBER_STR, pDimms[DimmIndex].PartNumber);
      }

      /** BankLabel **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, BANK_LABEL_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BANK_LABEL_STR, pDimms[DimmIndex].BankLabel);
      }

      /** DataWidth **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DATA_WIDTH_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DATA_WIDTH_STR, FORMAT_INT32 L" " BYTE_STR, pDimms[DimmIndex].DataWidth);
      }

      /** TotalWidth **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, TOTAL_WIDTH_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, TOTAL_WIDTH_STR, FORMAT_INT32 L" " BYTE_STR, pDimms[DimmIndex].TotalWidth);
      }

      /** Speed **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SPEED_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SPEED_STR, FORMAT_INT32 L" " MEGA_TRANSFERS_PER_SEC_STR, pDimms[DimmIndex].Speed);
      }

      /** FormFactor **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FORM_FACTOR_STR))) {
        pFormFactorStr = FormFactorToString(pDimms[DimmIndex].FormFactor);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FORM_FACTOR_STR, pFormFactorStr);
        FREE_POOL_SAFE(pFormFactorStr);
      }

      /** If Dimm is Manageable, print rest of the attributes **/
      if (pDimms[DimmIndex].ManageabilityState) {
        /** ManufacturerId **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MANUFACTURER_ID_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MANUFACTURER_ID_STR, FORMAT_HEX, EndianSwapUint16(pDimms[DimmIndex].ManufacturerId));
        }

        /** ControllerRevisionId **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CONTROLLER_REVISION_ID_STR))) {
          pSteppingStr = ControllerRidToStr(pDimms[DimmIndex].ControllerRid);
          if (pSteppingStr != NULL) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CONTROLLER_REVISION_ID_STR, pSteppingStr);
            FREE_POOL_SAFE(pSteppingStr);
          }
        }

        /** VolatileCapacity **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEMORY_MODE_CAPACITY_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].VolatileCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEMORY_MODE_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** AppDirectCapacity **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, APPDIRECT_MODE_CAPACITY_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].AppDirectCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, APPDIRECT_MODE_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** UnconfiguredCapacity **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, UNCONFIGURED_CAPACITY_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].UnconfiguredCapacity, UnitsToDisplay, TRUE,
              &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, UNCONFIGURED_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** InaccessibleCapacity **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, INACCESSIBLE_CAPACITY_STR))) {
          KEEP_ERROR(ReturnCode, TempReturnCode);
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].InaccessibleCapacity, UnitsToDisplay, TRUE,
              &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, INACCESSIBLE_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** ReservedCapacity **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, RESERVED_CAPACITY_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_CAPACITY) {
            pCapacityStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            TempReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pDimms[DimmIndex].ReservedCapacity, UnitsToDisplay, TRUE, &pCapacityStr);
            KEEP_ERROR(ReturnCode, TempReturnCode);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, RESERVED_CAPACITY_STR, pCapacityStr);
          FREE_POOL_SAFE(pCapacityStr);
        }

        /** PackageSparingCapable **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PACKAGE_SPARING_CAPABLE_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PACKAGE_SPARING_CAPABLE_STR, FORMAT_INT32, pDimms[DimmIndex].PackageSparingCapable);
        }

        if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_PACKAGE_SPARING) {
          /** PackageSparingEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PACKAGE_SPARING_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PACKAGE_SPARING_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PackageSparesAvailable **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PACKAGE_SPARES_AVAILABLE_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PACKAGE_SPARES_AVAILABLE_STR, UNKNOWN_ATTRIB_VAL);
          }
        }
        else {
          /** PackageSparingEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PACKAGE_SPARING_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PACKAGE_SPARING_ENABLED_STR, FORMAT_INT32, pDimms[DimmIndex].PackageSparingEnabled);
          }

          /** PackageSparesAvailable **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PACKAGE_SPARES_AVAILABLE_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PACKAGE_SPARES_AVAILABLE_STR, FORMAT_INT32, pDimms[DimmIndex].PackageSparesAvailable);
          }
        }

        /** IsNew **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, IS_NEW_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, IS_NEW_STR, FORMAT_INT32, pDimms[DimmIndex].IsNew);
        }

        if ((pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_OPTIONAL_CONFIG_DATA) || (!FIS_2_0)) {
          /** AveragePowerReportingTimeConstantMultiplier **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY, UNKNOWN_ATTRIB_VAL);
          }
        } else {
          /** AveragePowerReportingTimeConstantMultiplier **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVG_PWR_REPORTING_TIME_CONSTANT_MULT_PROPERTY, FORMAT_INT32, pDimms[DimmIndex].AvgPowerReportingTimeConstantMultiplier);
          }
        }

        if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_VIRAL_POLICY) {
          /** ViralPolicyEnable **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, VIRAL_POLICY_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VIRAL_POLICY_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** ViralStatus **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, VIRAL_STATE_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, VIRAL_STATE_STR, UNKNOWN_ATTRIB_VAL);
          }
        }
        else {
          /** ViralPolicyEnable **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, VIRAL_POLICY_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, VIRAL_POLICY_STR, FORMAT_INT32, pDimms[DimmIndex].ViralPolicyEnable);
          }

          /** ViralStatus **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, VIRAL_STATE_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, VIRAL_STATE_STR, FORMAT_INT32, pDimms[DimmIndex].ViralStatus);
          }
        }

        /** PeakPowerBudget **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PEAK_POWER_BUDGET_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PEAK_POWER_BUDGET_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].PeakPowerBudget, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** AvgPowerLimit **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVG_POWER_LIMIT_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVG_POWER_LIMIT_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].AvgPowerLimit, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** TurboModeState **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, TURBO_MODE_STATE_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, TURBO_MODE_STATE_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].TurboModeState, FORMAT_HEX_NOWIDTH));
        }

        /** TurboPowerLimit **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, TURBO_POWER_LIMIT_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, TURBO_POWER_LIMIT_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].TurboPowerLimit, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** AvgPowerTimeConstant **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVG_POWER_TIME_CONSTANT_STR))) {
          if (2 == pDimms[DimmIndex].FwVer.FwApiMajor && 0 == pDimms[DimmIndex].FwVer.FwApiMinor) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVG_POWER_TIME_CONSTANT_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].AveragePowerTimeConstant_2_0, FORMAT_HEX_NOWIDTH));
          }
          else {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVG_POWER_TIME_CONSTANT_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].AveragePowerTimeConstant_2_1, FORMAT_UINT64 TIME_MSR_MS));
          }
        }

        /** MaxAveragePowerLimit **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MAX_AVG_POWER_LIMIT_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MAX_AVG_POWER_LIMIT_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].MaxAveragePowerLimit, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** MaxTurboModePowerConsumption **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MAX_TURBO_MODE_POWER_CONSUMPTION_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MAX_TURBO_MODE_POWER_CONSUMPTION_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].MaxTurboModePowerConsumption, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** MaxAveragePowerTimeConstant **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MAX_AVERAGE_POWER_TIME_CONSTANT))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MAX_AVERAGE_POWER_TIME_CONSTANT, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].MaxAveragePowerTimeConstant, FORMAT_INT32));
        }

        /** AveragePowerTimeConstantStep **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVERAGE_POWER_TIME_CONSTANT_STEP))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVERAGE_POWER_TIME_CONSTANT_STEP, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].AveragePowerTimeConstantStep, FORMAT_INT32));
        }

        /** DcpmmAveragePower **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DCPMM_AVERAGE_POWER_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_AVERAGE_POWER_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].DcpmmAveragePower, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** AveragePower12V **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVERAGE_12V_POWER_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVERAGE_12V_POWER_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].AveragePower12V, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** AveragePower1_2V **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AVERAGE_1_2V_POWER_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AVERAGE_1_2V_POWER_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].AveragePower1_2V, FORMAT_INT32 L" " MILI_WATT_STR));
        }

        /** LatchedLastShutdownStatusDetails **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, LATCHED_LAST_SHUTDOWN_STATUS_STR))) {
          LatchedLastShutdownStatusDetails.AsUint32 = pDimms[DimmIndex].LatchedLastShutdownStatusDetails;
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            pAttributeStr = LastShutdownStatusToStr(LatchedLastShutdownStatusDetails);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, LATCHED_LAST_SHUTDOWN_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** UnlatchedLastShutdownStatusDetails **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, UNLATCHED_LAST_SHUTDOWN_STATUS_STR))) {
          UnlatchedLastShutdownStatusDetails.AsUint32 = pDimms[DimmIndex].UnlatchedLastShutdownStatusDetails;
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            pAttributeStr = LastShutdownStatusToStr(UnlatchedLastShutdownStatusDetails);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, UNLATCHED_LAST_SHUTDOWN_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** LastShutdownTime **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, LAST_SHUTDOWN_TIME_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            pAttributeStr = GetTimeFormatString(pDimms[DimmIndex].LastShutdownTime, TRUE);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, LAST_SHUTDOWN_TIME_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** ModesSupported **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MODES_SUPPORTED_STR))) {
          pAttributeStr = ModesSupportedToStr(pDimms[DimmIndex].ModesSupported);
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MODES_SUPPORTED_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** SecurityCapabilities **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SECURITY_CAPABILITIES_STR))) {
          pAttributeStr = SecurityCapabilitiesToStr(pDimms[DimmIndex].SecurityCapabilities);
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SECURITY_CAPABILITIES_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** MasterPassphraseEnabled **/
        if (ShowAll || (pDispOptions->DisplayOptionSet &&
          ContainsValue(pDispOptions->pDisplayValues, MASTER_PASS_ENABLED_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MASTER_PASS_ENABLED_STR, FORMAT_INT32,
            pDimms[DimmIndex].MasterPassphraseEnabled);
        }

        /** ConfigurationStatus **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, DIMM_CONFIG_STATUS_STR))) {
          pAttributeStr = mppAllowedShowDimmsConfigStatuses[pDimms[DimmIndex].ConfigStatus];
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_CONFIG_STATUS_STR, pAttributeStr);
        }

        /** SKUViolation **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SKU_VIOLATION_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SKU_VIOLATION_STR, FORMAT_INT32, pDimms[DimmIndex].SKUViolation);
        }

        /** ARSStatus **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ARS_STATUS_STR))) {
          pAttributeStr = ARSStatusToStr(pDimms[DimmIndex].ARSStatus);
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ARS_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** OverwriteDimmStatus **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, OVERWRITE_STATUS_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_OVERWRITE_STATUS) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            pAttributeStr = OverwriteDimmStatusToStr(pDimms[DimmIndex].OverwriteDimmStatus);
          }
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, OVERWRITE_STATUS_STR, pAttributeStr);
          FREE_POOL_SAFE(pAttributeStr);
        }

        /** AitDramEnabled **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, AIT_DRAM_ENABLED_STR))) {
          if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_SMART_AND_HEALTH) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, AIT_DRAM_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, AIT_DRAM_ENABLED_STR, FORMAT_INT32, pDimms[DimmIndex].AitDramEnabled);
          }
        }

        /** Boot Status and/or Boot Status Register **/
        if (ShowAll || (pDispOptions->DisplayOptionSet &&
          (ContainsValue(pDispOptions->pDisplayValues, BOOT_STATUS_STR) ||
            ContainsValue(pDispOptions->pDisplayValues, BOOT_STATUS_REGISTER_STR)))) {

          ReturnCode = pNvmDimmConfigProtocol->GetBSRAndBootStatusBitMask(pNvmDimmConfigProtocol, pDimms[DimmIndex].DimmID, &BootStatusRegister, &BootStatusBitMask);
          if (EFI_ERROR(ReturnCode)) {
            pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
          }
          else {
            pAttributeStr = BootStatusBitmaskToStr(gNvmDimmCliHiiHandle, BootStatusBitMask);
          }

          if (ShowAll || (pDispOptions->DisplayOptionSet &&
            ContainsValue(pDispOptions->pDisplayValues, BOOT_STATUS_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BOOT_STATUS_STR, pAttributeStr);
            FREE_POOL_SAFE(pAttributeStr);
          }

          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, BOOT_STATUS_REGISTER_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, BOOT_STATUS_REGISTER_STR,
              FORMAT_HEX_PREFIX FORMAT_UINT32_HEX L"_" FORMAT_UINT32_HEX, ((BootStatusRegister >> 32) & 0xFFFFFFFF), (BootStatusRegister & 0xFFFFFFFF));
          }
        }

        /** ExtendedAdrEnabled **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, EXTENDED_ADR_ENABLED_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, EXTENDED_ADR_ENABLED_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].ExtendedAdrEnabled, FORMAT_INT32));
        }

        /** PpcExtendedAdrEnabled **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PPC_EXTENDED_ADR_ENABLED_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PPC_EXTENDED_ADR_ENABLED_STR, ConvertDimmInfoAttribToString((VOID*)&pDimms[DimmIndex].PrevPwrCycleExtendedAdrEnabled, FORMAT_INT32));
        }

        if (pDimms[DimmIndex].ErrorMask & DIMM_INFO_ERROR_MEM_INFO_PAGE) {
          /** ErrorInjectionEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_INJECT_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ERROR_INJECT_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** MediaTemperatureInjectionEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEDIA_TEMP_INJ_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEDIA_TEMP_INJ_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** SoftwareTriggersEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SW_TRIGGERS_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SW_TRIGGERS_ENABLED_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** SoftwareTriggersEnabledDetails **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SW_TRIGGER_ENABLED_DETAILS_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SW_TRIGGER_ENABLED_DETAILS_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PoisonErrorInjectionsCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, POISON_ERR_INJ_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, POISON_ERR_INJ_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** PoisonErrorClearCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, POISON_ERR_CLR_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, POISON_ERR_CLR_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** MediaTemperatureInjectionsCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEDIA_TEMP_INJ_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, MEDIA_TEMP_INJ_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }

          /** SoftwareTriggersCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SW_TRIGGER_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SW_TRIGGER_CTR_STR, UNKNOWN_ATTRIB_VAL);
          }
        }
        else {
          /** ErrorInjectionEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ERROR_INJECT_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ERROR_INJECT_ENABLED_STR, FORMAT_INT32, pDimms[DimmIndex].ErrorInjectionEnabled);
          }

          /** MediaTemperatureInjectionEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEDIA_TEMP_INJ_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEDIA_TEMP_INJ_ENABLED_STR, FORMAT_INT32, pDimms[DimmIndex].MediaTemperatureInjectionEnabled);
          }

          /** SoftwareTriggersEnabled **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SW_TRIGGERS_ENABLED_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SW_TRIGGERS_ENABLED_STR, FORMAT_INT32, pDimms[DimmIndex].SoftwareTriggersEnabled);
          }

          /** SoftwareTriggersEnabledDetails **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SW_TRIGGER_ENABLED_DETAILS_STR))) {
            pAttributeStr = SoftwareTriggersEnabledToStr(pDimms[DimmIndex].SoftwareTriggersEnabledDetails);
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SW_TRIGGER_ENABLED_DETAILS_STR, pAttributeStr);
            FREE_POOL_SAFE(pAttributeStr);
          }

          /** PoisonErrorInjectionsCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, POISON_ERR_INJ_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, POISON_ERR_INJ_CTR_STR, FORMAT_INT32, pDimms[DimmIndex].PoisonErrorInjectionsCounter);
          }

          /** PoisonErrorClearCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, POISON_ERR_CLR_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, POISON_ERR_CLR_CTR_STR, FORMAT_INT32, pDimms[DimmIndex].PoisonErrorClearCounter);
          }

          /** MediaTemperatureInjectionsCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEDIA_TEMP_INJ_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEDIA_TEMP_INJ_CTR_STR, FORMAT_INT32, pDimms[DimmIndex].MediaTemperatureInjectionsCounter);
          }

          /** SoftwareTriggersCounter **/
          if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SW_TRIGGER_CTR_STR))) {
            PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SW_TRIGGER_CTR_STR, FORMAT_INT32, pDimms[DimmIndex].SoftwareTriggersCounter);
          }


        }
#ifdef OS_BUILD
        /** ActionRequired **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ACTION_REQUIRED_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, ACTION_REQUIRED_STR, FORMAT_INT32, pDimms[DimmIndex].ActionRequired);
        }

        /** ActionRequiredEvents **/
        if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, ACTION_REQUIRED_EVENTS_STR))) {
          PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, ACTION_REQUIRED_EVENTS_STR, NOT_APPLICABLE_SHORT_STR);
        }
#endif
      }
      else {
        for (Index3 = 0; Index3 < ALLOWED_DISP_VALUES_COUNT(pOnlyManageableAllowedDisplayValues); Index3++) {
          if (ContainsValue(pDispOptions->pDisplayValues, pOnlyManageableAllowedDisplayValues[Index3])) {
            PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, pOnlyManageableAllowedDisplayValues[Index3], NOT_APPLICABLE_SHORT_STR);
          }
        }
      }
    }

    /** show dimms from Uninitialized list **/

    for (DimmIndex = 0; DimmIndex < UninitializedDimmCount; DimmIndex++) {
      /** matching pid **/
      if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pUninitializedDimms[DimmIndex].DimmID)) {
        continue;
      }

      if (SocketsNum > 0 && !ContainUint(pSocketIds, SocketsNum, pUninitializedDimms[DimmIndex].SmbusAddress.Cpu)) {
        continue;
      }

      PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex + DimmCount);

      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DIMM_ID_STR, FORMAT_HEX, pUninitializedDimms[DimmIndex].DimmHandle);

      /** Capacity **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CAPACITY_STR))) {
        ReturnCode = MakeCapacityString(gNvmDimmCliHiiHandle, pUninitializedDimms[DimmIndex].Capacity, UnitsToDisplay, TRUE, &pCapacityStr);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CAPACITY_STR, pCapacityStr);
        FREE_POOL_SAFE(pCapacityStr);
      }

      /** Health State **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, HEALTH_STR))) {
        pHealthStr = HealthToString(gNvmDimmCliHiiHandle, pUninitializedDimms[DimmIndex].HealthState);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, HEALTH_STR, pHealthStr);
        FREE_POOL_SAFE(pHealthStr);
      }
      /** Health State reason**/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, HEALTH_STATE_REASON_STR))) {
        ReturnCode = ConvertHealthStateReasonToHiiStr(gNvmDimmCliHiiHandle,
          pUninitializedDimms[DimmIndex].HealthStatusReason, &pHealthStateReasonStr);
        if (pHealthStateReasonStr == NULL || EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, HEALTH_STATE_REASON_STR, pHealthStateReasonStr);
        FREE_POOL_SAFE(pHealthStateReasonStr);
      }

      // TODO: Order of Attributes need to be defined in spec still
      /** SubsytemDeviceId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SUBSYSTEM_DEVICE_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SUBSYSTEM_DEVICE_ID_STR, FORMAT_HEX, pUninitializedDimms[DimmIndex].SubsystemDeviceId);
      }

      /** SubsytemRevisionId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SUBSYSTEM_REVISION_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SUBSYSTEM_REVISION_ID_STR, FORMAT_HEX, pUninitializedDimms[DimmIndex].SubsystemRid);
      }

      /** SocketId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SOCKET_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SOCKET_ID_STR, FORMAT_HEX, pUninitializedDimms[DimmIndex].SocketId);
      }

      /** MemoryControllerId **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, MEMORY_CONTROLLER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, MEMORY_CONTROLLER_STR, FORMAT_HEX, pUninitializedDimms[DimmIndex].ImcId);
      }

      /** ChannelID **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CHANNEL_ID_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_ID_STR, FORMAT_HEX, pUninitializedDimms[DimmIndex].ChannelId);
      }

      /** ChannelPos **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, CHANNEL_POS_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, CHANNEL_POS_STR, FORMAT_INT32, pUninitializedDimms[DimmIndex].ChannelPos);
      }

      /** Boot Status **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, BOOT_STATUS_STR))) {

        ReturnCode = pNvmDimmConfigProtocol->GetBSRAndBootStatusBitMask(pNvmDimmConfigProtocol, pUninitializedDimms[DimmIndex].DimmID, &BootStatusRegister, &BootStatusBitMask);
        if (EFI_ERROR(ReturnCode)) {
          pAttributeStr = CatSPrint(NULL, FORMAT_STR, UNKNOWN_ATTRIB_VAL);
        }
        else {
          pAttributeStr = BootStatusBitmaskToStr(gNvmDimmCliHiiHandle, BootStatusBitMask);
        }
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, BOOT_STATUS_STR, pAttributeStr);
        FREE_POOL_SAFE(pAttributeStr);
      }

      /** Boot Status Register **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, BOOT_STATUS_REGISTER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, BOOT_STATUS_REGISTER_STR, FORMAT_HEX_PREFIX FORMAT_UINT32_HEX L"_" FORMAT_UINT32_HEX, ((BootStatusRegister >> 32) & 0xFFFFFFFF), (BootStatusRegister & 0xFFFFFFFF));
      }

      /** SerialNumber **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, SERIAL_NUMBER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, SERIAL_NUMBER_STR, FORMAT_HEX_PREFIX FORMAT_UINT32_HEX, EndianSwapUint32(pUninitializedDimms[DimmIndex].SerialNumber));
      }

      /** FwVersion **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FW_VER_STR))) {
        ConvertFwVersion(TmpFwVerString, pUninitializedDimms[DimmIndex].FwVer.FwProduct,
          pUninitializedDimms[DimmIndex].FwVer.FwRevision, pUninitializedDimms[DimmIndex].FwVer.FwSecurityVersion,
          pUninitializedDimms[DimmIndex].FwVer.FwBuild);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_VER_STR, TmpFwVerString);
      }

      /** FwApiVersion **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, FW_API_VER_STR))) {
        ConvertFwApiVersion(TmpFwVerString, pUninitializedDimms[DimmIndex].FwVer.FwApiMajor,
          pUninitializedDimms[DimmIndex].FwVer.FwApiMinor);
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, FW_API_VER_STR, TmpFwVerString);
      }

      /** PartNumber **/
      if (ShowAll || (pDispOptions->DisplayOptionSet && ContainsValue(pDispOptions->pDisplayValues, PART_NUMBER_STR))) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PART_NUMBER_STR, pUninitializedDimms[DimmIndex].PartNumber);
      }
    }
  }
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowDimmDataSetAttribs);

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_CMD_DISPLAY_OPTIONS_SAFE(pDispOptions);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pAllDimms);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pUninitializedDimms);
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Convert manageability state to a string
**/
STATIC
CHAR16*
ManageabilityToString(
  IN     UINT8 ManageabilityState
)
{
  CHAR16 *pManageabilityString = NULL;

  switch (ManageabilityState) {
  case MANAGEMENT_VALID_CONFIG:
    pManageabilityString = CatSPrint(NULL, FORMAT_STR, L"Manageable");
    break;
  case MANAGEMENT_INVALID_CONFIG:
  default:
    pManageabilityString = CatSPrint(NULL, FORMAT_STR, L"Unmanageable");
    break;
  }
  return pManageabilityString;
}

/**
  Convert type to string
**/
STATIC
CHAR16*
FormFactorToString(
  IN     UINT8 FormFactor
)
{
  CHAR16 *pFormFactorStr = NULL;
  switch (FormFactor) {
  case FORM_FACTOR_DIMM:
    pFormFactorStr = CatSPrint(NULL, FORMAT_STR, L"DIMM");
    break;
  case FORM_FACTOR_SODIMM:
    pFormFactorStr = CatSPrint(NULL, FORMAT_STR, L"SODIMM");
    break;
  default:
    pFormFactorStr = CatSPrint(NULL, FORMAT_STR, L"Other");
    break;
  }
  return pFormFactorStr;
}

/**
  Convert overwrite DIMM status value to string
**/
STATIC
CHAR16 *
OverwriteDimmStatusToStr(
  IN     UINT8 OverwriteDimmStatus
)
{
  CHAR16 *pOverwriteDimmStatusStr = NULL;

  NVDIMM_ENTRY();

  switch (OverwriteDimmStatus) {
  case OVERWRITE_DIMM_STATUS_COMPLETED:
    pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_COMPLETED_STR);
    break;
  case OVERWRITE_DIMM_STATUS_IN_PROGRESS:
    pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_IN_PROGRESS_STR);
    break;
  case OVERWRITE_DIMM_STATUS_NOT_STARTED:
    pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_NOT_STARTED_STR);
    break;
  case OVERWRITE_DIMM_STATUS_UNKNOWN:
  default:
    pOverwriteDimmStatusStr = CatSPrintClean(NULL, FORMAT_STR, OVERWRITE_DIMM_STATUS_UNKNOWN_STR);
    break;
  }

  NVDIMM_EXIT();
  return pOverwriteDimmStatusStr;
}
