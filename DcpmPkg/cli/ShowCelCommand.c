/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* TODO: Include headers */
#include <Library/BaseMemoryLib.h>
#include "ShowCelCommand.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "LoadCommand.h"
#include "Debug.h"
#include "Convert.h"

#define DS_ROOT_PATH                      L"/CelList"
#define DS_DIMM_PATH                      L"/CelList/Dimm"
#define DS_DIMM_INDEX_PATH                L"/CelList/Dimm[%d]"
#define DS_CEL_PATH                       L"/CelList/Dimm/Cel"
#define DS_CEL_INDEX_PATH                 L"/CelList/Dimm[%d]/Cel[%d]"

 /**
   show -cel syntax definition
 **/
struct Command ShowCelCommandSyntax =
{
  SHOW_VERB,                                                           //!< verb
  {                                                                    //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", LARGE_PAYLOAD_OPTION, L"", L"", HELP_LPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SMALL_PAYLOAD_OPTION, L"", L"", HELP_SPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT,FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", L"",FALSE, ValueOptional}
#endif
  },
  {
    {CEL_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueEmpty},
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                            //!< properties
  L"Show command effect log for given DIMM",                          //!< help
  ShowCelCommand,                                                   //!< run function
  TRUE
};

// Table heading names
#define OPCODE_STR          L"Opcode"
#define SUBOPCODE_STR       L"SubOpcode"
#define CE_DESCRIPTION_STR  L"CE Description"

/*
*  SHOW CAP ATTRIBUTES (4 columns)
*   DimmID | Opcode | SubOpcode | CE Description
*   ===========================================
*   0x0001 | X      | X         | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowCelTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                      //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                               //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR           //COLUMN DATA PATH
    },
    {
      OPCODE_STR,                                       //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(OPCODE_STR),              //COLUMN MAX STR WIDTH
      DS_CEL_PATH PATH_KEY_DELIM OPCODE_STR             //COLUMN DATA PATH
    },
    {
      SUBOPCODE_STR,                                    //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(SUBOPCODE_STR),           //COLUMN MAX STR WIDTH
      DS_CEL_PATH PATH_KEY_DELIM SUBOPCODE_STR          //COLUMN DATA PATH
    },
    {
      CE_DESCRIPTION_STR,                               //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(CE_DESCRIPTION_STR),      //COLUMN MAX STR WIDTH
      DS_CEL_PATH PATH_KEY_DELIM CE_DESCRIPTION_STR     //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowCmdEffectLogDataSetAttribs =
{
  NULL,
  &ShowCelTableAttributes
};

/**
  Create the string from Command Effect Bits.

  param[in] Value is the value to be printed.
  param[in] SensorType - type of sensor
**/
STATIC
CHAR16 *
GetCommandEffectDescriptionStr(
  IN     COMMAND_EFFECT_LOG_ENTRY CelEntry
)
{
  CHAR16 *pReturnBuffer = NULL;

  // Return immediately if opcode not supported
  if (0 == CelEntry.EffectName.AsUint32) {
    pReturnBuffer = CatSPrintClean(pReturnBuffer, OPCODE_NOT_SUPPORTED);
    return pReturnBuffer;
  }

  if (1 == CelEntry.EffectName.Separated.NoEffects) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, NO_EFFECTS);
  }

  if (1 == CelEntry.EffectName.Separated.SecurityStateChange) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, SECURITY_STATE_CHANGE);
  }

  if (1 == CelEntry.EffectName.Separated.DimmConfigChangeAfterReboot) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, DIMM_CONFIGURATION_CHANGE_AFTER_REBOOT);
  }

  if (1 == CelEntry.EffectName.Separated.ImmediateDimmConfigChange) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, IMMEDIATE_DIMM_CONFIGURATION_CHANGE);
  }

  if (1 == CelEntry.EffectName.Separated.QuiesceAllIo) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, QUIESCE_ALL_IO);
  }

  if (1 == CelEntry.EffectName.Separated.ImmediateDimmDataChange) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, IMMEDIATE_DIMM_DATA_CHANGE);
  }

  if (1 == CelEntry.EffectName.Separated.TestMode) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, TEST_MODE);
  }

  if (1 == CelEntry.EffectName.Separated.DebugMode) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, DEBUG_MODE);
  }

  if (1 == CelEntry.EffectName.Separated.ImmediateDimmPolicyChange) {
    if (NULL != pReturnBuffer) {
      pReturnBuffer = CatSPrintClean(pReturnBuffer, L", ");
    }
    pReturnBuffer = CatSPrintClean(pReturnBuffer, IMMEDIATE_DIMM_POLICY_CHANGE);
  }

  return pReturnBuffer;
}

/**
  Register the show -cel command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowCelCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowCelCommandSyntax);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get command effect log command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
ShowCelCommand(
  IN    struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 *pDimmsValue = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  UINT32 DimmIndex = 0;
  COMMAND_EFFECT_LOG_ENTRY *pCelEntry = NULL;
  UINT32 EntryCount = 0;
  UINT32 DimmHandle = 0;
  UINT32 DimmIdIndex = 0;
  UINT32 CelEntryIndex = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *pPath = NULL;
  CHAR16 *pCommandEffectDescription = NULL;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NOT_FOUND) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pDimmsValue, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsNum)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
    if (!AllDimmsInListInSupportedConfig(pDimms, DimmCount, pDimmIds, DimmIdsNum)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_POPULATION_VIOLATION);
      goto Finish;
    }
  }

  // If no dimm IDs are specified get IDs from all dimms
  if (DimmIdsNum == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(pNvmDimmConfigProtocol, TRUE, &DimmIdsNum, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (DimmIdsNum == 0) {
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_MANAGEABLE_DIMMS);
      goto Finish;
    }
  }

  // Traverse each DIMM
  for (DimmIndex = 0; DimmIndex < DimmCount; DimmIndex++) {
    if (!ContainUint(pDimmIds, DimmIdsNum, pDimms[DimmIndex].DimmID)) {
      continue;
    }

    if ((MANAGEMENT_VALID_CONFIG != pDimms[DimmIndex].ManageabilityState)
        || (TRUE == pDimms[DimmIndex].IsInPopulationViolation)){
      continue;
    }

    // Get CEL table for each DIMM
    ReturnCode = pNvmDimmConfigProtocol->GetCommandEffectLog(pNvmDimmConfigProtocol, pDimms[DimmIndex].DimmID, &pCelEntry, &EntryCount);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    // Retrieve DimmHandle and DimmIdindex for given DimmId
    ReturnCode = GetDimmHandleByPid(pDimms[DimmIndex].DimmID, pDimms, DimmCount, &DimmHandle, &DimmIdIndex);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    // Retrieve DimmId as string based on preferences
    ReturnCode = GetPreferredDimmIdAsString(pDimms[DimmIdIndex].DimmHandle, pDimms[DimmIdIndex].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

    for (CelEntryIndex = 0; CelEntryIndex < EntryCount; CelEntryIndex++) {
      PRINTER_BUILD_KEY_PATH(pPath, DS_CEL_INDEX_PATH, DimmIndex, CelEntryIndex);
      PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, OPCODE_STR, (UINT8)pCelEntry[CelEntryIndex].Opcode.Separated.Opcode, HEX);
      PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, SUBOPCODE_STR, (UINT8)pCelEntry[CelEntryIndex].Opcode.Separated.SubOpcode, HEX);
      pCommandEffectDescription = GetCommandEffectDescriptionStr(pCelEntry[CelEntryIndex]);
      PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CE_DESCRIPTION_STR, pCommandEffectDescription);
      FREE_POOL_SAFE(pCommandEffectDescription);
    }
  }

  //Switch text output type to display as a table
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(pPrinterCtx);
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowCmdEffectLogDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pCommandEffectDescription);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pCelEntry);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
