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

/**
  Command syntax definition
**/
struct Command ShowRegisterCommand =
{
  SHOW_VERB,                                                  //!< verb
  {
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", FALSE, ValueOptional}                         //!< options
#endif
  },
  {                                                           //!< targets
    {DIMM_TARGET, L"", L"DimmIDs", TRUE, ValueOptional},
    {REGISTER_TARGET, L"", L"Register", TRUE, ValueOptional},
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                    //!< properties
  L"Show Key DIMM Registers.",                                //!< help
  ShowRegister
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
  @retval EFI_ABORTED invoking CONFIG_PROTOGOL function failurepPmCapableString
**/
EFI_STATUS
ShowRegister(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pRegisterValues = NULL;
  CHAR16 *pDimmValues = NULL;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN ShowAllRegisters = FALSE;
  DIMM_BSR Bsr;
  UINT64 FwMailboxStatus = 0;
  UINT64 FwMailboxOutput = 0;
  UINT32 DimmCount = 0;
  DIMM_INFO *pDimms = NULL;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  BOOLEAN FIS_1_4 = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(DimmStr, sizeof(DimmStr));

  Bsr.AsUint64 = 0;

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    Print(FORMAT_STR_NL, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /** Make sure we can access the config protocol **/
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
    Print(FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
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
    Print(FORMAT_STR_NL, CLI_ERR_INCOMPLETE_SYNTAX);
    goto Finish;
  }

  InitializeCommandStatus(&pCommandStatus);

  /** check that the register parameters are correct if provided**/
  if (!ShowAllRegisters) {
    ReturnCode = CheckDisplayList(pRegisterValues, mppAllowedShowDimmRegistersValues,
      ALLOWED_DISP_VALUES_COUNT(mppAllowedShowDimmRegistersValues));
    if (EFI_ERROR(ReturnCode)) {
      Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_REGISTER);
      goto Finish;
    }
  }

  /** Get and print registers for each requested dimm **/
  for (Index = 0; Index < DimmCount; Index++) {
    if (DimmIdsNum > 0 && !ContainUint(pDimmIds, DimmIdsNum, pDimms[Index].DimmID)) {
      NVDIMM_WARN("Dimm 0x%x not found", pDimms[Index].DimmHandle);
      continue;
    }

    // @todo Remove FIS 1.4 backwards compatibility workaround
    if (pDimms[Index].FwVer.FwApiMajor == 1 && pDimms[Index].FwVer.FwApiMinor <= 4) {
      FIS_1_4 = TRUE;
    }

    ReturnCode = pNvmDimmConfigProtocol->RetrieveDimmRegisters(pNvmDimmConfigProtocol,
        pDimms[Index].DimmID, &Bsr.AsUint64, &FwMailboxStatus,
        FW_MB_SMALL_OUTPUT_REG_USED, &FwMailboxOutput, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
        goto Finish;
    }

    ReturnCode = GetPreferredDimmIdAsString(pDimms[Index].DimmHandle, pDimms[Index].DimmUid,
          DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
        goto Finish;
    }

    Print(L"---" FORMAT_STR L"=" FORMAT_STR L" Registers---\n", DIMM_ID_STR, DimmStr);
    if (ContainsValue(pRegisterValues, REGISTER_BSR_STR) || ShowAllRegisters) {
      Print(L"Boot Status:                0x%016lx\n", Bsr.AsUint64);
      // @todo Remove FIS 1.4 backwards compatibility workaround
      Print(L"  [07:00] MajorCheckpoint ------ = 0x%x\n", Bsr.Separated_Current_FIS.Major);
      Print(L"  [15:08] MinorCheckpoint ------ = 0x%x\n", Bsr.Separated_Current_FIS.Minor);
      Print(L"  [17:16] MR (Media Ready) ----- = 0x%x (00:notReady; 1:Ready; 2:Error; 3:Rsv)\n", Bsr.Separated_Current_FIS.MR);
      Print(L"  [18:18] DT (DDRT IO Init Started)  = 0x%x (0:notStarted; 1:Training Started)\n", Bsr.Separated_Current_FIS.DT);
      Print(L"  [19:19] PCR (PCR access locked) = 0x%x (0:Unlocked; 1:Locked)\n", Bsr.Separated_Current_FIS.PCR);
      Print(L"  [20:20] MBR (Mailbox Ready) --- = 0x%x (0:notReady; 1:Ready)\n", Bsr.Separated_Current_FIS.MBR);
      Print(L"  [21:21] WTS (Watchdog Status)-- = 0x%x (0:noChange; 1:WT NMI generated)\n", Bsr.Separated_Current_FIS.WTS);
      Print(L"  [22:22] FRCF (First Fast Refresh Completed) = 0x%x (0:noChange; 1:1stRefreshCycleCompleted)\n", Bsr.Separated_Current_FIS.FRCF);
      Print(L"  [23:23] CR (Credit Ready) ---- = 0x%x (0:WDB notFlushed; 1:WDB Flushed)\n", Bsr.Separated_Current_FIS.CR);
      Print(L"  [24:24] MD (Media Disabled) -- = 0x%x (0:User Data is accessible; 1:User Data is not accessible)\n", Bsr.Separated_Current_FIS.MD);
      Print(L"  [25:25] OIE (SVN Downgrade Opt-In Enable) ----- = 0x%x (0:Not Enabled; 1:Enabled)\n", Bsr.Separated_Current_FIS.OIE);
      Print(L"  [26:26] OIWE (SVN Downgrade Opt-In Was Enabled) = 0x%x (0:Never Enabled; 1:Has Been Enabled)\n",
             Bsr.Separated_Current_FIS.OIWE);
      if (FIS_1_4) {
        Print(L"  [31:27] Rsvd ----------------- = 0x%x\n", Bsr.Separated_FIS_1_4.Rsvd);
        Print(L"  [32:32] Assertion ------------ = 0x%x (1:FW has hit an assert - debug only)\n", Bsr.Separated_FIS_1_4.Assertion);
        Print(L"  [33:33] MI_Stalled ----------- = 0x%x (1:FW has stalled media interface engine - debug only)\n",
               Bsr.Separated_FIS_1_4.MI_Stalled);
        Print(L"  [34:34] DR (DRAM Ready(AIT)) --= 0x%x (0:notReady; 1:Ready)\n", Bsr.Separated_FIS_1_4.DR);
        Print(L"  [63:35] Rsvd ----------------- = 0x%x\n\n", Bsr.Separated_FIS_1_4.Rsvd1);
      } else {
        Print(L"  [28:27] DR (DRAM Ready(AIT)) --= 0x%x (0:Not trained,Not Loaded; 1:Trained,Not Loaded; 2:Error; 3:Trained,Loaded(Ready))\n", Bsr.Separated_Current_FIS.DR);
        Print(L"  [29:29] RR (Reboot Required)= 0x%x (0:No reset is needed by the DIMM; 1:The DIMMs internal state requires a platform power cycle)\n", Bsr.Separated_Current_FIS.RR);
        Print(L"  [30:63] Rsvd ----------------- = 0x%x\n", Bsr.Separated_Current_FIS.Rsvd);
      }
    }
    if (ContainsValue(pRegisterValues, REGISTER_OS_STR) || ShowAllRegisters) {
      Print(L"FW Mailbox Status:          0x%016lx\n", FwMailboxStatus);
      for (Index2 = 0; Index2 < FW_MB_SMALL_OUTPUT_REG_USED; Index2++) {
        Print(L"FW Mailbox Small Output[%d]: 0x%016lx\n", Index2, FwMailboxStatus);
      }
    }
  }

  ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
  DisplayCommandStatus(L"ShowRegister", L" on", pCommandStatus);
Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pDimmIds);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
