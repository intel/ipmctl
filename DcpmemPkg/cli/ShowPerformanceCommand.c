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
    {																	//!< properties
        { L"", L"", L"", FALSE, ValueOptional },
    },
    L"Show performance statistics per dimm",         //!< help
    ShowPerformance
};

EFI_STATUS GetDimmIdorDimmHandleToPrint(UINT16 DimmId, DIMM_INFO *AllDimmInfos, UINT32 DimmCount, UINT32 *HandleToPrint)
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
PrintPerformanceData(UINT16 *DimmId, DIMM_INFO *AllDimmInfos, UINT32 PerformanceMask, UINT32 DimmCount, DIMM_PERFORMANCE_DATA *pDimmsPerformanceData)
{
    UINT32 Index = 0;
	UINT32 TargetIndex = 0;
	BOOLEAN SkipDimm = TRUE;
	UINT32 DimmHandle;

	SetDisplayInfo(L"DimmPerformance", TableTabView);

    // Print the header
    if (TARGET_PERFORMANCE_BYTES_READ_MASK == PerformanceMask) {
        Print(PERFORMANCE_BYTES_READ_HEADER);
    }
    else if (TARGET_PERFORMANCE_BYTES_WRITTEN_MASK == PerformanceMask) {
        Print(PERFORMANCE_BYTES_WRITTEN_HEADER);
    }
    else if (TARGET_PERFORMANCE_HOST_READS_MASK == PerformanceMask) {
        Print(PERFORMANCE_HOST_READS_HEADER);
    }
    else if (TARGET_PERFORMANCE_HOST_WRITES_MASK == PerformanceMask) {
        Print(PERFORMANCE_HOST_WRITES_HEADER);
    }
    else if (TARGET_PERFORMANCE_BLOCK_READS_MASK == PerformanceMask) {
        Print(PERFORMANCE_BLOCK_READS_HEADER);
    }
    else if (TARGET_PERFORMANCE_BLOCK_WRITES_MASK == PerformanceMask) {
        Print(PERFORMANCE_BLOCK_WRITES_HEADER);
    }
    else {
        Print(PERFORMANCE_ALL_HEADER);
    }
    // Print the data
    for (Index = 0; Index < DimmCount; Index++, pDimmsPerformanceData++)
    {
		if(NULL != DimmId)
		{
			SkipDimm = TRUE;
			for (TargetIndex = 0; TargetIndex < DimmCount; ++TargetIndex)
			{
				if ((DimmId[TargetIndex] ==  pDimmsPerformanceData->DimmId))
				{
					SkipDimm = FALSE;
					break;
				}
			}
			if (SkipDimm)
				continue;
		}

		GetDimmIdorDimmHandleToPrint(pDimmsPerformanceData->DimmId, AllDimmInfos, DimmCount, &DimmHandle);

        if (TARGET_PERFORMANCE_BYTES_READ_MASK == PerformanceMask) {
            Print(PERFORMANCE_SINGLE_TARGET_FORMAT, DimmHandle, pDimmsPerformanceData->TotalBytesRead.Uint64_1, pDimmsPerformanceData->TotalBytesRead.Uint64);
        }
        else if (TARGET_PERFORMANCE_BYTES_WRITTEN_MASK == PerformanceMask) {
            Print(PERFORMANCE_SINGLE_TARGET_FORMAT, DimmHandle, pDimmsPerformanceData->TotalBytesWritten.Uint64_1, pDimmsPerformanceData->TotalBytesWritten.Uint64);
        }
        else if (TARGET_PERFORMANCE_HOST_READS_MASK == PerformanceMask) {
            Print(PERFORMANCE_SINGLE_TARGET_FORMAT, DimmHandle, pDimmsPerformanceData->TotalReadRequests.Uint64_1, pDimmsPerformanceData->TotalReadRequests.Uint64);
        }
        else if (TARGET_PERFORMANCE_HOST_WRITES_MASK == PerformanceMask) {
            Print(PERFORMANCE_SINGLE_TARGET_FORMAT, DimmHandle, pDimmsPerformanceData->TotalWriteRequests.Uint64_1, pDimmsPerformanceData->TotalWriteRequests.Uint64);
        }
        else if (TARGET_PERFORMANCE_BLOCK_READS_MASK == PerformanceMask) {
            Print(PERFORMANCE_SINGLE_TARGET_FORMAT, DimmHandle, pDimmsPerformanceData->TotalBlockReadRequests.Uint64_1, pDimmsPerformanceData->TotalBlockReadRequests.Uint64);
        }
        else if (TARGET_PERFORMANCE_BLOCK_WRITES_MASK == PerformanceMask) {
            Print(PERFORMANCE_SINGLE_TARGET_FORMAT, DimmHandle, pDimmsPerformanceData->TotalBlockWriteRequests.Uint64_1, pDimmsPerformanceData->TotalBlockWriteRequests.Uint64);
        }
        else {
            Print(PERFORMANCE_ALL_FORMAT, DimmHandle,
                pDimmsPerformanceData->TotalBytesRead.Uint64_1, pDimmsPerformanceData->TotalBytesRead.Uint64,
                pDimmsPerformanceData->TotalBytesWritten.Uint64_1, pDimmsPerformanceData->TotalBytesWritten.Uint64,
                pDimmsPerformanceData->TotalReadRequests.Uint64_1, pDimmsPerformanceData->TotalReadRequests.Uint64,
                pDimmsPerformanceData->TotalWriteRequests.Uint64_1, pDimmsPerformanceData->TotalWriteRequests.Uint64,
                pDimmsPerformanceData->TotalBlockReadRequests.Uint64_1, pDimmsPerformanceData->TotalBlockReadRequests.Uint64,
                pDimmsPerformanceData->TotalBlockWriteRequests.Uint64_1, pDimmsPerformanceData->TotalBlockWriteRequests.Uint64);
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
    EFI_NVMDIMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
    CHAR16 *pTargetValueStr;
    UINT32 TargetPerformanceMask = 0;
    UINT32 DimmCount;
    DIMM_PERFORMANCE_DATA *pDimmsPerformanceData = NULL;
	UINT32 DimmIdsNum = 0;
	CHAR16 *pDimmsValue = NULL;
	UINT32 DimmsCount = 0;
	DIMM_INFO *pDimms = NULL;
	UINT16 *pDimmIds = NULL;

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

    // Get performance target
    pTargetValueStr = GetTargetValue(pCmd, PERFORMANCE_TARGET);
    if (*pTargetValueStr)
    {
        if (StrICmp(pTargetValueStr, TARGET_PERFORMANCE_BYTES_READ) == 0) {
            TargetPerformanceMask = TARGET_PERFORMANCE_BYTES_READ_MASK;
        }
        else if (StrICmp(pTargetValueStr, TARGET_PERFORMANCE_BYTES_WRITTEN) == 0) {
            TargetPerformanceMask = TARGET_PERFORMANCE_BYTES_WRITTEN_MASK;
        }
        else if (StrICmp(pTargetValueStr, TARGET_PERFORMANCE_HOST_READS) == 0) {
            TargetPerformanceMask = TARGET_PERFORMANCE_HOST_READS_MASK;
        }
        else if (StrICmp(pTargetValueStr, TARGET_PERFORMANCE_HOST_WRITES) == 0) {
            TargetPerformanceMask = TARGET_PERFORMANCE_HOST_WRITES_MASK;
        }
        else if (StrICmp(pTargetValueStr, TARGET_PERFORMANCE_BLOCK_READS) == 0) {
            TargetPerformanceMask = TARGET_PERFORMANCE_BLOCK_READS_MASK;
        }
        else if (StrICmp(pTargetValueStr, TARGET_PERFORMANCE_BLOCK_WRITES) == 0) {
            TargetPerformanceMask = TARGET_PERFORMANCE_BLOCK_WRITES_MASK;
        }
        else
        {
            Print(FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_PERFORMANCE);
            NVDIMM_ERR("Incorrect value for target -performance");
            ReturnCode = EFI_INVALID_PARAMETER;
            goto Finish;
        }
    }
    else {
        TargetPerformanceMask = TARGET_PERFORMANCE_ALL_MASK;
    }

    // Get the performance data
    ReturnCode = pNvmDimmConfigProtocol->GetDimmsPerformance(pNvmDimmConfigProtocol, &DimmCount, &pDimmsPerformanceData);
    if (EFI_ERROR(ReturnCode)) {
        Print(FORMAT_STR_NL, CLI_ERR_NO_CONFIG_PROTOCOL);
        NVDIMM_ERR("");
        goto Finish;
    }

    // Print the data out
    PrintPerformanceData(pDimmIds, pDimms, TargetPerformanceMask, DimmCount, pDimmsPerformanceData);

Finish:
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
