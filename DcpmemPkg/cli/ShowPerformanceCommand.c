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

EFI_STATUS
ShowPerformance(IN struct Command *pCmd);


#define DCPMM_PERFORMANCE_MEDIA_READS             L"MediaReads"
#define DCPMM_PERFORMANCE_MEDIA_WRITES            L"MediaWrites"
#define DCPMM_PERFORMANCE_READ_REQUESTS           L"ReadRequests"
#define DCPMM_PERFORMANCE_WRITE_REQUESTS          L"WriteRequests"
#define DCPMM_PERFORMANCE_TOTAL_MEDIA_READS       L"TotalMediaReads"
#define DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES      L"TotalMediaWrites"
#define DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS     L"TotalReadRequests"
#define DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS    L"TotalWriteRequests"

#define HELP_TEXT_PERFORMANCE_CATEGORIES  L""DCPMM_PERFORMANCE_MEDIA_READS \
                                          L"|"DCPMM_PERFORMANCE_MEDIA_WRITES \
                                          L"|"DCPMM_PERFORMANCE_READ_REQUESTS \
                                          L"|"DCPMM_PERFORMANCE_WRITE_REQUESTS \
                                          L"|"DCPMM_PERFORMANCE_TOTAL_MEDIA_READS \
                                          L"|"DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES \
                                          L"|"DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS \
                                          L"|"DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS

/**
Command syntax definition
**/
struct Command ShowPerformanceCommand =
{
    SHOW_VERB,                                                          //!< verb
    {                                                                   //!< options
        { L"", L"", L"", L"", FALSE, ValueOptional },
    },
    {                                                                   //!< targets
        { DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, TRUE, ValueOptional },
        { PERFORMANCE_TARGET, L"", HELP_TEXT_PERFORMANCE_CATEGORIES, TRUE, ValueOptional }
    },
    {                                                                   //!< properties
        { L"", L"", L"", FALSE, ValueOptional },
    },
    L"Show performance statistics per DIMM",                            //!< help
    ShowPerformance
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

#define PERFORMANCE_DATA_FORMAT    L"=0x%016Lx%016Lx\n"


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
PrintPerformanceData(UINT16 *DimmId, UINT32 DimmIdsNum, DIMM_INFO *AllDimmInfos,
    UINT32 DimmCount, DIMM_PERFORMANCE_DATA *pDimmsPerformanceData,
    BOOLEAN AllOptionSet, BOOLEAN DisplayOptionSet, CHAR16 *pDisplayOptionValue)
{
  UINT32 AllDimmsIndex = 0;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 DimmStr[MAX_DIMM_UID_LENGTH];

  SetDisplayInfo(L"DimmPerformance", ListView);

  // Account for multiple or no input dimms given
  for (AllDimmsIndex = 0; AllDimmsIndex < DimmCount; AllDimmsIndex++) {

    if (DimmIdsNum > 0 && !ContainUint(DimmId, DimmIdsNum,
        pDimmsPerformanceData[AllDimmsIndex].DimmId)) {
      continue;
    }

    // Print the DimmID
    ReturnCode = GetPreferredDimmIdAsString(AllDimmInfos[AllDimmsIndex].DimmHandle,
      AllDimmInfos[AllDimmsIndex].DimmUid, DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      continue;
    }

    Print(L"---" FORMAT_STR L"=" FORMAT_STR L"---\n", DIMM_ID_STR, DimmStr);

    /** MediaReads **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_MEDIA_READS))) {
      Print(DCPMM_PERFORMANCE_MEDIA_READS PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].MediaReads.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].MediaReads.Uint64);
    }

    /** MediaWrites **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_MEDIA_WRITES))) {
      Print(DCPMM_PERFORMANCE_MEDIA_WRITES PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].MediaWrites.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].MediaWrites.Uint64);
    }

    /** ReadRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_READ_REQUESTS))) {
      Print(DCPMM_PERFORMANCE_READ_REQUESTS PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].ReadRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].ReadRequests.Uint64);
    }

    /** WriteRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_READ_REQUESTS))) {
      Print(DCPMM_PERFORMANCE_READ_REQUESTS PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].WriteRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].WriteRequests.Uint64);
    }

    /** TotalMediaReads **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_MEDIA_READS))) {
      Print(DCPMM_PERFORMANCE_TOTAL_MEDIA_READS PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaReads.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaReads.Uint64);
    }

    /** TotalMediaWrites **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES))) {
      Print(DCPMM_PERFORMANCE_TOTAL_MEDIA_WRITES PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaWrites.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalMediaWrites.Uint64);
    }

    /** TotalReadRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS))) {
      Print(DCPMM_PERFORMANCE_TOTAL_READ_REQUESTS PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalReadRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalReadRequests.Uint64);
    }

    /** TotalWriteRequests **/
    if (AllOptionSet || (DisplayOptionSet && ContainsValue(pDisplayOptionValue, DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS))) {
      Print(DCPMM_PERFORMANCE_TOTAL_WRITE_REQUESTS PERFORMANCE_DATA_FORMAT,
                  pDimmsPerformanceData[AllDimmsIndex].TotalWriteRequests.Uint64_1,
                  pDimmsPerformanceData[AllDimmsIndex].TotalWriteRequests.Uint64);
    }
  }
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
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
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

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_ERR("pCmd parameter is NULL.\n");
    goto Finish;
  }

  // Make sure we can access the config protocol
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    NVDIMM_ERR("Communication with the device driver failed; ReturnCode 0x%x", ReturnCode);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Initialize status structure
  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    Print(FORMAT_STR_NL, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_ERR("Failed on InitializeCommandStatus; ReturnCode 0x%x", ReturnCode);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetDimmList(pNvmDimmConfigProtocol, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmsCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pDimmsValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pDimmsValue, pDimms, DimmsCount, &pDimmIds, &DimmIdsNum);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Target value is not a valid Dimm ID");
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmsCount, pDimmIds, DimmIdsNum)) {
      Print(FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      ReturnCode = EFI_INVALID_PARAMETER;
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
    Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
    NVDIMM_ERR("");
    goto Finish;
  }

  // Print the data out
  PrintPerformanceData(pDimmIds, DimmIdsNum, pDimms, DimmCount, pDimmsPerformanceData,
      AllOptionSet, DisplayOptionSet, pPerformanceValueStr);

Finish:
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimmsPerformanceData);
  FreeCommandStatus(&pCommandStatus);
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
