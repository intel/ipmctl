/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Types.h>
#include "CommandParser.h"
#include "ShowPerformanceCommand.h"
#include "Common.h"
#include "Convert.h"
#include "NvmTypes.h"

#define DS_ROOT_PATH                        L"/DimmPerformanceList"
#define DS_SOCKET_PATH                      L"/DimmPerformanceList/DimmPerformance"
#define DS_SOCKET_INDEX_PATH                L"/DimmPerformanceList/DimmPerformance[%d]"

 /*
 *  PRINT LIST ATTRIBUTES
 *  ---DimmId=0x0001---
 *     MediaReads=0x000000000000000000000000cc3bb004
 *     MediaWrites=0x00000000000000000000000049437ab4
 *     ReadRequests=0x000000000000000000000000000c0008
 *     ...
 */
PRINTER_LIST_ATTRIB ShowPerformanceListAttributes =
{
 {
    {
      L"DimmPerformance",                                   //GROUP LEVEL TYPE
      L"---" DIMM_ID_STR L"=$(" DIMM_ID_STR L")---",        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                           //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                           //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowPerformanceDataSetAttribs =
{
  &ShowPerformanceListAttributes,
  NULL
};

EFI_STATUS
ShowPerformance(IN struct Command *pCmd);

/**
Command syntax definition
**/
struct Command ShowPerformanceCommand =
{
    SHOW_VERB,                                                          //!< verb
    {                                                                   //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"", L"",FALSE, ValueOptional}
#endif
    },
    {                                                                   //!< targets
        { DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional },
        { PERFORMANCE_TARGET, L"", HELP_TEXT_PERFORMANCE_CAT, TRUE, ValueOptional }
    },
    {                                                                   //!< properties
        { L"", L"", L"", FALSE, ValueOptional },
    },
    L"Show performance statistics of one or more " PMEM_MODULES_STR L".",              //!< help
    ShowPerformance,
    TRUE,                                                               //!< enable print control support
};

CHAR16 *mppAllowedShowPerformanceDisplayValues[] =
{
  DCPMM_PERFORMANCE_MEDIA_READS,
  DCPMM_PERFORMANCE_MEDIA_WRITES,
  DCPMM_PERFORMANCE_READ_REQUESTS,
  DCPMM_PERFORMANCE_WRITE_REQUESTS,
  DCPMM_PERFORMANCE_TOTAL_MEDIA_READS,
  DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES,
  DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS,
  DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS
};

#define PERFORMANCE_DATA_FORMAT    L"0x"FORMAT_UINT64_HEX FORMAT_UINT64_HEX


EFI_STATUS GetDimmIdorDimmHandleToPrint(UINT16 DimmId, DIMM_INFO *AllDimmInfos,
    UINT32 DimmCount, UINT32 *HandleToPrint)
{
  UINT32 Index = 0;

  for (Index = 0; Index < DimmCount; ++Index) {
    if (AllDimmInfos[Index].DimmID == DimmId) {
      *HandleToPrint = AllDimmInfos[Index].DimmHandle;
      return EFI_SUCCESS;
    }
  }
  return EFI_INVALID_PARAMETER;
}

STATIC
VOID
PrintPerformanceData(PRINT_CONTEXT *pPrinterCtx, UINT16 *DimmId, UINT32 DimmIdsNum, DIMM_INFO *AllDimmInfos,
    UINT32 DimmCount, DIMM_PERFORMANCE_DATA *pDimmsPerformanceData,
    BOOLEAN AllOptionSet, BOOLEAN DisplayOptionSet, CHAR16 *pDisplayOptionValue)
{
  UINT32 AllDimmsIndex = 0;
  UINT32 InfoIndex = 0;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  UINT32 DimmIndex = 0;
  CHAR16 *pPath = NULL;
  BOOLEAN InfoFound = FALSE;

  // Account for multiple or no input dimms given
  for (AllDimmsIndex = 0; AllDimmsIndex < DimmCount; AllDimmsIndex++) {

    if (DimmIdsNum > 0 && !ContainUint(DimmId, DimmIdsNum,
        pDimmsPerformanceData[AllDimmsIndex].DimmId)) {
      continue;
    }

    // find info record that corresponds to the performance data
    InfoFound = FALSE;
    for (InfoIndex = 0; InfoIndex < DimmCount; InfoIndex++) {
      if (AllDimmInfos[InfoIndex].DimmID == pDimmsPerformanceData[AllDimmsIndex].DimmId) {
        InfoFound = TRUE;
        break;
      }
    }

    // skip record with no matching info record
    if (!InfoFound) {
      continue;
    }

    // Print the DimmID
    ReturnCode = GetPreferredDimmIdAsString(AllDimmInfos[InfoIndex].DimmHandle,
      AllDimmInfos[InfoIndex].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      continue;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_SOCKET_INDEX_PATH, DimmIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

    /** MediaReads **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_MEDIA_READS))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_MEDIA_READS, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].MediaReads.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].MediaReads.Uint64);
    }

    /** MediaWrites **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_MEDIA_WRITES))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_MEDIA_WRITES, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].MediaWrites.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].MediaWrites.Uint64);
    }

    /** ReadRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_READ_REQUESTS))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_READ_REQUESTS, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].ReadRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].ReadRequests.Uint64);
    }

    /** WriteRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_WRITE_REQUESTS))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_WRITE_REQUESTS, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].WriteRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].WriteRequests.Uint64);
    }

    /** TotalMediaReads **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_MEDIA_READS))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_TOTAL_MEDIA_READS, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaReads.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaReads.Uint64);
    }

    /** TotalMediaWrites **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaWrites.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaWrites.Uint64);
    }

    /** TotalReadRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalReadRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalReadRequests.Uint64);
    }

    /** TotalWriteRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS))) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS, PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalWriteRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalWriteRequests.Uint64);
    }

    ++DimmIndex;
  }

  FREE_POOL_SAFE(pPath);
}

/**
Execute the Show Performance command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowPerformance(
  IN     struct Command *pCmd
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT32 DimmCount;
  DIMM_PERFORMANCE_DATA *pDimmsPerformanceData = NULL;
  UINT32 DimmIdsNum = 0;
  CHAR16 *pDimmsValue = NULL;
  UINT32 DimmsCount = 0;
  DIMM_INFO *pDimms = NULL;
  UINT16 *pDimmIds = NULL;
  // Print all unless some are specified
  BOOLEAN AllOptionSet = TRUE;
  BOOLEAN DisplayOptionSet = FALSE;
  CHAR16 *pPerformanceValueStr = NULL;
  UINT16 Index;
  PRINT_CONTEXT *pPrinterCtx = NULL;

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_DBG("pCmd parameter is NULL.\n");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  // Make sure we can access the config protocol
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmsCount);
  if (EFI_ERROR(ReturnCode)) {
    if(ReturnCode == EFI_NOT_FOUND) {
        PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_FUNCTIONAL_DIMMS);
    }
    goto Finish;
  }

  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pDimmsValue, pDimms, DimmsCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmsCount, pDimmIds, DimmIdsNum)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  // So, instead of parsing the -d parameter with CheckAllAndDisplayOptions(),
  // we're going to maintain the existing implementation for now of just taking
  // a comma separated list after the performance parameter
  // Get any specified parameters after "-performance"
  // TODO: Decide what we want to do long term and move to Common.c
  pPerformanceValueStr = GetTargetValue(pCmd, PERFORMANCE_TARGET);

  // Check if any valid strings exist in the performance value string
  // TODO: Add invalid value checking
  for (Index = 0; Index < ALLOWED_DISP_VALUES_COUNT(mppAllowedShowPerformanceDisplayValues); Index++) {
    if (ContainsValue(pPerformanceValueStr, mppAllowedShowPerformanceDisplayValues[Index])) {
      AllOptionSet = FALSE;
      DisplayOptionSet = TRUE;
      break; // If we find a match, leave the loop
    }
  }

  // Get the performance data
  ReturnCode = pNvmDimmConfigProtocol->GetDimmsPerformanceData(pNvmDimmConfigProtocol,
      &DimmCount, &pDimmsPerformanceData);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Print the data out
  PrintPerformanceData(pPrinterCtx, pDimmIds, DimmIdsNum, pDimms, DimmCount, pDimmsPerformanceData,
      AllOptionSet, DisplayOptionSet, pPerformanceValueStr);

  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowPerformanceDataSetAttribs);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimmsPerformanceData);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/*
* Register the show dimms command
*/
EFI_STATUS
RegisterShowPerformanceCommand(
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVDIMM_ENTRY();
  Rc = RegisterCommand(&ShowPerformanceCommand);

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}
