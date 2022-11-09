/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>

#include "ShowRegisterCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include <Library/BaseMemoryLib.h>
#include "Common.h"

#define DIMM_ID_STR               L"DimmID"
#define REGISTER_TARGET_STR       L"RegType"
#define DS_ROOT_PATH              L"/RegList"
#define DS_DIMM_PATH              L"/RegList/Dimm"
#define DS_DIMM_INDEX_PATH        L"/RegList/Dimm[%d]"
#define DS_REGISTER_PATH          L"/RegList/Dimm/Register"
#define DS_REGISTER_INDEX_PATH    L"/RegList/Dimm[%d]/Register[%d]"

 /*
      *  PRINT LIST ATTRIBUTES
      *  ---DimmID=0x0011 Registers---
      *     ---RegType=BSR
      *        Boot Status: 00000000181D00F0
      *          [07:00] MajorCheckpoint ------: 0xF0
      *          [15:08] MinorCheckpoint ------: 0x0
      *        ...
      */
PRINTER_LIST_ATTRIB ShowRegisterListAttributes =
{
 {
    {
      DIMM_NODE_STR,                                                                  //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L") Registers---",                        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L"=" FORMAT_STR,                                     //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                                                     //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      REGISTER_MODE_STR,                                                              //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" REGISTER_TARGET_STR L"=$(" REGISTER_TARGET_STR L")",     //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                    //NULL or KEY VAL FORMAT STR
      REGISTER_TARGET_STR                                                             //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowRegisterDataSetAttribs =
{
  &ShowRegisterListAttributes,
  NULL
};

/**
  Command syntax definition
**/
struct Command ShowRegisterCommand =
{
  SHOW_VERB,                                                  //!< verb
  {                                                           //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"",HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"",L"", FALSE, ValueOptional}
#endif
  },
  {                                                           //!< targets
    {DIMM_TARGET, L"", L"DimmIDs", TRUE, ValueOptional},
    {REGISTER_TARGET, L"", L"Register", TRUE, ValueOptional},
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                    //!< properties
  L"Show Key " PMEM_MODULE_STR L" Registers.",                                //!< help
  ShowRegister,
  TRUE                                                        //!< enable print control support
};

CHAR16 *mppAllowedShowDimmRegistersValues[] = {
  REGISTER_BSR_STR,
  REGISTER_OS_STR
};

/**
  Register the show register command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowRegisterCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowRegisterCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the show pools command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED Failure invoking CONFIG_PROTOCOL function
**/
EFI_STATUS
ShowRegister(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pRegisterValues = NULL;
  CHAR16 *pDimmValues = NULL;
  UINT32 Index = 0;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN ShowAllRegisters = FALSE;
  DIMM_BSR Bsr;
  UINT32 DimmCount = 0;
  DIMM_INFO *pDimms = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  UINT32 DimmIndex = 0;
  UINT32 RegIndex = 0;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  Bsr.AsUint64 = 0;

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  /** Make sure we can access the config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
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

  /** if a specific DIMM pid was passed in, set it **/
  pDimmValues = GetTargetValue(pCmd, DIMM_TARGET);
  if (pDimmValues != NULL) {
    if (StrLen(pDimmValues) > 0) {
      ReturnCode = GetDimmIdsFromString(pCmd, pDimmValues, pDimms, DimmCount, &pDimmIds, &DimmIdsNum);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Target value is not a valid DIMM ID");
        goto Finish;
      }
    } else {
      DimmIdsNum = 0;
    }
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Missing Dimm target");
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  if (ContainTarget(pCmd, REGISTER_TARGET)) {
    pRegisterValues = GetTargetValue(pCmd, REGISTER_TARGET);
    if (StrLen(pRegisterValues) == 0) {
      ShowAllRegisters = TRUE;
    } else {
      ShowAllRegisters = FALSE;
    }
  } else {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  CHECK_RESULT(InitializeCommandStatus(&pCommandStatus), Finish);

  /** check that the register parameters are correct if provided**/
  if (!ShowAllRegisters) {
    ReturnCode = CheckDisplayList(pRegisterValues, mppAllowedShowDimmRegistersValues,
      ALLOWED_DISP_VALUES_COUNT(mppAllowedShowDimmRegistersValues));
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_TARGET_REGISTER);
      goto Finish;
    }
  }

  /** Get and print registers for each requested dimm **/
  for (Index = 0; Index < DimmCount; Index++) {
    if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
      NVDIMM_WARN("Dimm 0x%x not found", pDimms[Index].DimmHandle);
      continue;
    }

    ReturnCode = pNvmDimmConfigProtocol->RetrieveDimmRegisters(pNvmDimmConfigProtocol,
        pDimms[Index].DimmID, &Bsr.AsUint64, NULL, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to retrieve Dimm Registers");
        goto Finish;
    }

    ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
        goto Finish;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, DimmIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);
    if (ContainsValue(pRegisterValues, REGISTER_BSR_STR) || ShowAllRegisters) {
      PRINTER_BUILD_KEY_PATH(pPath, DS_REGISTER_INDEX_PATH, DimmIndex, RegIndex);
      RegIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, REGISTER_TARGET_STR, REGISTER_BSR_STR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Boot Status", FORMAT_HEX_PREFIX FORMAT_UINT64_HEX, Bsr.AsUint64);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [07:00] MajorCheckpoint --------------------------", FORMAT_HEX_NOWIDTH, Bsr.Separated_Current_FIS.Major);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [15:08] MinorCheckpoint --------------------------", FORMAT_HEX_NOWIDTH, Bsr.Separated_Current_FIS.Minor);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [17:16] MR (Media Ready) -------------------------", FORMAT_HEX_NOWIDTH L" (00:notReady; 1:Ready; 2:Error; 3:Rsv)", Bsr.Separated_Current_FIS.MR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [18:18] DT (DDRT IO Init Started) ----------------", FORMAT_HEX_NOWIDTH L" (0:notStarted; 1:Training Started)", Bsr.Separated_Current_FIS.DT);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [19:19] PCR (PCR access locked) ------------------", FORMAT_HEX_NOWIDTH L" (0:Unlocked; 1:Locked)", Bsr.Separated_Current_FIS.PCR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [20:20] MBR (Mailbox Ready) ----------------------", FORMAT_HEX_NOWIDTH L" (0:notReady; 1:Ready)", Bsr.Separated_Current_FIS.MBR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [21:21] WTS (Watchdog Status)---------------------", FORMAT_HEX_NOWIDTH L" (0:noChange; 1:WT NMI generated)", Bsr.Separated_Current_FIS.WTS);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [22:22] FRCF (First Fast Refresh Completed) ------", FORMAT_HEX_NOWIDTH L" (0:noChange; 1:1stRefreshCycleCompleted)", Bsr.Separated_Current_FIS.FRCF);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [23:23] CR (Credit Ready) ------------------------", FORMAT_HEX_NOWIDTH L" (0:WDB notFlushed; 1:WDB Flushed)", Bsr.Separated_Current_FIS.CR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [24:24] MD (Media Disabled) ----------------------", FORMAT_HEX_NOWIDTH L" (0:User Data is accessible; 1:User Data is not accessible)", Bsr.Separated_Current_FIS.MD);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [25:25] SVNDE (SVN Downgrade Opt-In Enable) ------", FORMAT_HEX_NOWIDTH L" (0:Not Enabled; 1:Enabled)", Bsr.Separated_Current_FIS.SVNDE);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [26:26] SVNCOIS (SVN Downgrade Opt-In Was Enabled)", FORMAT_HEX_NOWIDTH L" (0:Never Enabled; 1:Has Been Enabled)", Bsr.Separated_Current_FIS.SVNCOIS);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [28:27] DR (DRAM Ready(AIT)) ---------------------", FORMAT_HEX_NOWIDTH L" (0:Not trained,Not Loaded; 1:Trained,Not Loaded; 2:Error; 3:Trained,Loaded(Ready))", Bsr.Separated_Current_FIS.DR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [29:29] RR (Reboot Required) ---------------------", FORMAT_HEX_NOWIDTH L" (0:No reset is needed by the " PMEM_MODULE_STR L"; 1:The " PMEM_MODULES_STR L" internal state requires a platform power cycle)", Bsr.Separated_Current_FIS.RR);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [30:30] LFOPB ------------------------------------", FORMAT_HEX_NOWIDTH L" (0:No Error; 1:Fatal Link Error)", Bsr.Separated_Current_FIS.LFOPB);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [31:31] SVNWC ------------------------------------", FORMAT_HEX_NOWIDTH L" (0:The SVN Opt-In Window is open; 1:The SVN Opt-In Window is closed)", Bsr.Separated_Current_FIS.SVNWC);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [33:32] Rsvd -------------------------------------", FORMAT_HEX_NOWIDTH L" (Rsvd)", Bsr.Separated_Current_FIS.Rsvd);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [35:34] DTS --------------------------------------", FORMAT_HEX_NOWIDTH L" (00:Training Not Complete; 1:Training Complete; 2:Training Failure; 3:S3 Complete)", Bsr.Separated_Current_FIS.DTS);
      if ((pDimms[DimmIndex].FwVer.FwApiMajor > 2) || (pDimms[DimmIndex].FwVer.FwApiMajor == 2 && pDimms[DimmIndex].FwVer.FwApiMinor >= 3)) { // Current FIS version
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [36:36] FAC --------------------------------------", FORMAT_HEX_NOWIDTH L" (0:FW Activate has not completed; 1:FW Activate has completed)\n", Bsr.Separated_Current_FIS.FAC);
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [63:37] Rsvd1 ------------------------------------", FORMAT_HEX_NOWIDTH L" (Rsvd1)\n", Bsr.Separated_Current_FIS.Rsvd1);
      }
      else { // FIS version 2.2 or earlier detected
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"  [63:36] Rsvd1 ------------------------------------", FORMAT_HEX_NOWIDTH L" (Rsvd1)\n", Bsr.Separated_FIS_1_15.Rsvd1);
      }
    }
    DimmIndex++;
  }

  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CLI_INFO_SHOW_REGISTER, L" on", pCommandStatus);

  //Specify DataSet attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowRegisterDataSetAttribs);

  //Force as list
  PRINTER_ENABLE_LIST_TABLE_FORMAT(pPrinterCtx);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pPath);
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDimmIds);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
