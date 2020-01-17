/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HiiLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "ShowCmdAccessPolicyCommand.h"
#include "Common.h"
#include "Convert.h"

#define DS_ROOT_PATH                        L"/OpCodePolicyList"
#define DS_DIMM_PATH                        L"/OpCodePolicyList/Dimm"
#define DS_DIMM_INDEX_PATH                  L"/OpCodePolicyList/Dimm[%d]"
#define DS_OPCODE_PATH                      L"/OpCodePolicyList/Dimm/OpCode"
#define DS_OPCODE_INDEX_PATH                L"/OpCodePolicyList/Dimm[%d]/OpCode[%d]"

/**
  Command syntax definition
**/
struct Command ShowCmdAccessPolicyCommand =
{
  SHOW_VERB,                                        //!< verb
  /**
   options
  **/
  {                                                 //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT,FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", L"",FALSE, ValueOptional}
#endif
  },
  /**
   targets
  **/
  {
    { DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional },
    { CAP_TARGET, L"", L"", TRUE, ValueEmpty },
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                    //!< properties
  L"Show Command Access Policy Restrictions for DIMM(s).",    //!< help
  ShowCmdAccessPolicy,                                        //!< run function
  TRUE,                                                       //!< enable print control support
};

// Table heading names
#define OPCODE_STR        L"Opcode"
#define SUBOPCODE_STR     L"SubOpcode"
#define RESTRICTION_STR    L"Restriction"

/*
*  SHOW CAP ATTRIBUTES (4 columns)
*   DimmID | Opcode | SubOpcode | Restricted
*   ===========================================
*   0x0001 | X    | X            | X
*   ...
*/
PRINTER_TABLE_ATTRIB ShowCapTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                                          //COLUMN HEADER
      DIMM_MAX_STR_WIDTH,                                                   //COLUMN MAX STR WIDTH
      DS_DIMM_PATH PATH_KEY_DELIM DIMM_ID_STR                               //COLUMN DATA PATH
    },
    {
      OPCODE_STR,                                                           //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(OPCODE_STR),                                  //COLUMN MAX STR WIDTH
      DS_OPCODE_PATH PATH_KEY_DELIM OPCODE_STR                              //COLUMN DATA PATH
    },
    {
      SUBOPCODE_STR,                                                        //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(SUBOPCODE_STR),                               //COLUMN MAX STR WIDTH
      DS_OPCODE_PATH PATH_KEY_DELIM SUBOPCODE_STR                           //COLUMN DATA PATH
    },
    {
      RESTRICTION_STR,                                                       //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(RESTRICTION_STR),                              //COLUMN MAX STR WIDTH
      DS_OPCODE_PATH PATH_KEY_DELIM RESTRICTION_STR                          //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowCmdAccessPolicyDataSetAttribs =
{
  NULL,
  &ShowCapTableAttributes
};


/**
  Register the show cmd access policy command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowCmdAccessPolicyCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowCmdAccessPolicyCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the show cmd access policy command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_NOT_FOUND failed to open Config protocol, or run-time preferences could
                        not be retrieved
  @retval Other errors returned by the driver
**/
EFI_STATUS
ShowCmdAccessPolicy(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT32 DimmIndex = 0;
  UINT32 OpCodeIndex = 0;
  CHAR16 *pTargetValue = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  COMMAND_ACCESS_POLICY_ENTRY *pCapEntries = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  UINT32 DimmHandle = 0;
  UINT32 DimmIdIndex = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  CHAR16 *RestrictionStr = NULL;
  CHAR16 *pNone = NULL;
  CHAR16 *pBiosOnly = NULL;
  CHAR16 *pSMBusOnly = NULL;
  CHAR16 *pBiosSMBusOnly = NULL;
  CHAR16 *pInvalid = NULL;
  CHAR16* pUnsupported = NULL;
  UINT32 CapCount = 0;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  /**
    Make sure we can access the config protocol
  **/
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

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  /** If no dimm IDs are specified get IDs from all dimms **/
  if (DimmIdsCount == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(pNvmDimmConfigProtocol, FALSE, &DimmIdsCount, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (DimmIdsCount == 0) {
      ReturnCode = EFI_NOT_FOUND;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_INFO_NO_MANAGEABLE_DIMMS);
      goto Finish;
    }
  }

/**
  Retrieve the count of access policy entries.
**/
  ReturnCode = pNvmDimmConfigProtocol->GetCommandAccessPolicy(pNvmDimmConfigProtocol, pDimmIds[0], &CapCount, NULL);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  pCapEntries = AllocateZeroPool(sizeof(*pCapEntries) * CapCount * DimmIdsCount);
  if (pCapEntries == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  for (DimmIndex = 0; DimmIndex < DimmIdsCount; DimmIndex++) {
    /**
      Retrieve all CAP for each DIMM
    **/
    ReturnCode = pNvmDimmConfigProtocol->GetCommandAccessPolicy(pNvmDimmConfigProtocol, pDimmIds[DimmIndex], &CapCount, &pCapEntries[DimmIndex]);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to get the access policy for pDimmIds[%d] - ReturnCode=0x%x",
        DimmIndex, ReturnCode);
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    /*
      Retrieve DimmHandle and DimmIdindex for given DimmId
    */
    ReturnCode = GetDimmHandleByPid(pDimmIds[DimmIndex], pDimms, DimmCount, &DimmHandle, &DimmIdIndex);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    /*
      Retrieve DimmId as string based on preferences
     */
    ReturnCode = GetPreferredDimmIdAsString(pDimms[DimmIdIndex].DimmHandle, pDimms[DimmIdIndex].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
    PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
    pNone = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_RESTRICTION_NONE), NULL);
    pBiosOnly = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_RESTRICTION_BIOS_ONLY), NULL);
    pSMBusOnly = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_RESTRICTION_SMBUS_ONLY), NULL);
    pBiosSMBusOnly = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_RESTRICTION_BIOS_SMBUS_ONLY), NULL);
    pUnsupported = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_RESTRICTION_UNSUPPORTED), NULL);
    pInvalid = HiiGetString(gNvmDimmCliHiiHandle, STRING_TOKEN(STR_DCPMM_RESTRICTION_INVALID), NULL);
    // set max column width based on longest output string.  Should always be less than 80 (required param)
    ShowCapTableAttributes.ColumnAttribs[3].ColumnMaxStrLen = (UINT32)StrnSizeS(pBiosSMBusOnly,80);
    for (OpCodeIndex = 0; OpCodeIndex < CapCount; OpCodeIndex++) {
      PRINTER_BUILD_KEY_PATH(pPath, DS_OPCODE_INDEX_PATH, DimmIndex, OpCodeIndex);
      PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, OPCODE_STR, (pCapEntries + DimmIndex)[OpCodeIndex].Opcode, HEX);
      PRINTER_SET_KEY_VAL_UINT8(pPrinterCtx, pPath, SUBOPCODE_STR, (pCapEntries + DimmIndex)[OpCodeIndex].SubOpcode, HEX);
      switch ((pCapEntries + DimmIndex)[OpCodeIndex].Restriction) {
      case COMMAND_ACCESS_POLICY_RESTRICTION_NONE:
        RestrictionStr = pNone;
        break;
      case COMMAND_ACCESS_POLICY_RESTRICTION_BIOSONLY:
        RestrictionStr = pBiosOnly;
        break;
      case COMMAND_ACCESS_POLICY_RESTRICTION_SMBUSONLY:
        RestrictionStr = pSMBusOnly;
        break;
      case COMMAND_ACCESS_POLICY_RESTRICTION_BIOSSMBUSONLY:
        RestrictionStr = pBiosSMBusOnly;
        break;
      case COMMAND_ACCESS_POLICY_RESTRICTION_UNSUPPORTED:
        RestrictionStr = pUnsupported;
        break;
      default:
        RestrictionStr = pInvalid;
        break;
      }
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, RESTRICTION_STR, RestrictionStr);
    }
    FREE_POOL_SAFE(pNone);
    FREE_POOL_SAFE(pBiosOnly);
    FREE_POOL_SAFE(pSMBusOnly);
    FREE_POOL_SAFE(pBiosSMBusOnly);
    FREE_POOL_SAFE(pUnsupported);
    FREE_POOL_SAFE(pInvalid);
  }

  //Switch text output type to display as a table
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(pPrinterCtx);
  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowCmdAccessPolicyDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pNone);
  FREE_POOL_SAFE(pBiosOnly);
  FREE_POOL_SAFE(pSMBusOnly);
  FREE_POOL_SAFE(pBiosSMBusOnly);
  FREE_POOL_SAFE(pUnsupported);
  FREE_POOL_SAFE(pInvalid);
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pCapEntries);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
