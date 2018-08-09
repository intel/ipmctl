/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Debug build only
#ifndef MDEPKG_NDEBUG

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include "Debug.h"
#include "Types.h"
#include "Utility.h"
#include "NvmDimmCli.h"
#include "NvmInterface.h"
#include "CommandParser.h"
#include "ShowCmdAccessPolicyCommand.h"
#include "Common.h"
#include "Convert.h"

STATIC EFI_STATUS
UpdateCmdCtx(struct Command *pCmd);

/**
  Command syntax definition
**/
struct Command ShowCmdAccessPolicyCommand =
{
  SHOW_VERB,                                        //!< verb
  /**
   options
  **/
  {
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}                         //!< options
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
  UpdateCmdCtx
};

// Table heading names
#define OPCODE_STR        L"Opcode"
#define SUBOPCODE_STR     L"SubOpcode"
#define RESTRICTED_STR    L"Restricted"

// Dataset names for printer context
#define ROOT_DL_DS      L"RootDimmListDS"
#define DIMM_DS         L"DimmDS"
#define OPCODE_DS       L"OpcodeDS"

/*
*  SHOW CAP ATTRIBUTES (4 columns)
*   DimmID | Opcode | SubOpcode | Restricted
*   ===========================================
*   0x0001 | X    | X            | X
*   ...
*/
SHOW_TABLE_ATTRIB ShowCapTableAttributes =
{
  {
    {
      DIMM_ID_STR,                                                          //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(DIMM_ID_STR),                                 //COLUMN WIDTH
      L"/" ROOT_DL_DS L"/" DIMM_DS L"." DIMM_ID_STR                         //COLUMN DATA PATH
    },
    {
      OPCODE_STR,                                                           //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(OPCODE_STR),                                  //COLUMN WIDTH
      L"/" ROOT_DL_DS L"/" DIMM_DS L"/" OPCODE_DS L"." OPCODE_STR           //COLUMN DATA PATH
    },
    {
      SUBOPCODE_STR,                                                        //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(SUBOPCODE_STR),                               //COLUMN WIDTH
      L"/" ROOT_DL_DS L"/" DIMM_DS L"/" OPCODE_DS L"." SUBOPCODE_STR        //COLUMN DATA PATH
    },
    {
      RESTRICTED_STR,                                                       //COLUMN HEADER
      TABLE_MIN_HEADER_LENGTH(RESTRICTED_STR),                              //COLUMN WIDTH
      L"/" ROOT_DL_DS L"/" DIMM_DS L"/" OPCODE_DS L"." RESTRICTED_STR       //COLUMN DATA PATH
    }
  }
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
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  SOCKET_INFO *pCmdAccessPolicy = NULL;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  CHAR16 *pTargetValue = NULL;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  COMMAND_ACCESS_POLICY_ENTRY *pCapEntries = NULL;
  DATA_SET_CONTEXT *RootDataSet = NULL;

  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ShowCmdError(NULL, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // check targets
  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmIdsFromString");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  /** If no dimm IDs are specified get IDs from all dimms **/
  if (DimmIdsCount == 0) {
    ReturnCode = GetManageableDimmsNumberAndId(&DimmIdsCount, &pDimmIds);
    if (EFI_ERROR(ReturnCode)) {
      ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }

    if (DimmIdsCount == 0) {
      ReturnCode = EFI_NOT_FOUND;
      ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_INFO_NO_MANAGEABLE_DIMMS);
      goto Finish;
    }
  }

/**
  Retrieve the count of access policy entries.
**/
  UINT32 CapCount = 0;
  ReturnCode = pNvmDimmConfigProtocol->GetCommandAccessPolicy(pNvmDimmConfigProtocol, pDimmIds[0], &CapCount, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    goto Finish;
  }

  pCapEntries = AllocateZeroPool(sizeof(*pCapEntries) * CapCount * DimmIdsCount);
  if (pCapEntries == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    goto Finish;
  }

  for (Index = 0; Index < DimmIdsCount; Index++) {
    /**
      Retrieve all CAP for each DIMM
    **/
    ReturnCode = pNvmDimmConfigProtocol->GetCommandAccessPolicy(pNvmDimmConfigProtocol, pDimmIds[Index], &CapCount, &pCapEntries[Index]);
    if (EFI_ERROR(ReturnCode)) {
      ShowCmdError(pCmd->pShowCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
      goto Finish;
    }
  }

  // Build the datasets for printing
  RootDataSet = CreateDataSet(NULL, ROOT_DL_DS, NULL);

  for (Index = 0; Index < DimmIdsCount; Index++) {
    DATA_SET_CONTEXT *DimmDataSet = CreateDataSet(RootDataSet, DIMM_DS, NULL);
    if (EFI_SUCCESS != (ReturnCode = SetKeyValueUint16(DimmDataSet, DIMM_ID_STR, pDimmIds[Index], HEX))) {
      goto Finish;
    }

    for (Index2 = 0; Index2 < CapCount; Index2++) {
      DATA_SET_CONTEXT *CapDataSet = CreateDataSet(DimmDataSet, OPCODE_DS, NULL);
      if (EFI_SUCCESS != (ReturnCode = SetKeyValueUint8(CapDataSet, OPCODE_STR, (pCapEntries + Index)[Index2].Opcode, HEX))) {
        goto Finish;
      }

      if (EFI_SUCCESS != (ReturnCode = SetKeyValueUint8(CapDataSet, SUBOPCODE_STR, (pCapEntries + Index)[Index2].SubOpcode, HEX))) {
        goto Finish;
      }

      if (EFI_SUCCESS != (ReturnCode = SetKeyValueUint8(CapDataSet, RESTRICTED_STR, (pCapEntries + Index)[Index2].Restricted, DECIMAL))) {
        goto Finish;
      }
    }
  }

  //Switch text output type to display as a table
  SET_FORMAT_TABLE_FLAG(pCmd->pShowCtx);

  //Specify table attributes
  SET_TABLE_ATTRIBUTES(pCmd->pShowCtx, ShowCapTableAttributes);

  ShowCmdData(RootDataSet, pCmd->pShowCtx);

Finish:
  FreeDataSet(RootDataSet);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pCmdAccessPolicy);
  FREE_POOL_SAFE(pCapEntries);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Executes right before execution of the actual CMD handler.
This gives an opportunity to modify values in the Command
context (struct Command).

@param[in] pCmd command from CLI

@retval EFI_STATUS
**/
STATIC EFI_STATUS
UpdateCmdCtx(struct Command *pCmd) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  return ReturnCode;
}

#endif // !MDEPKG_NDEBUG