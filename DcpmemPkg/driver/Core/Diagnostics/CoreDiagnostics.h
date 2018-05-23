/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _CORE_DIAGNOSTICS_H_
#define _CORE_DIAGNOSTICS_H_

#include "Uefi.h"
#include "Dimm.h"
#include "NvmTypes.h"
#include <NvmInterface.h>
#include "NvmDimmDriver.h"
#include <NvmTables.h>
#include <Convert.h>
#include "FwUtility.h"
#include "Utility.h"
#include "NvmSecurity.h"
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Guid/Acpi.h>
#include "NvmDimmDriverData.h"
#include <Library/PrintLib.h>
#include <Protocol/Smbios.h>
#include <Library/HiiLib.h>

/** Diagnostics State bitmasks **/
#define DIAG_STATE_MASK_OK         BIT0
#define DIAG_STATE_MASK_WARNING    BIT1
#define DIAG_STATE_MASK_FAILED     BIT2
#define DIAG_STATE_MASK_ABORTED    BIT3
#define DIAG_STATE_MASK_ALL (DIAG_STATE_MASK_OK |  DIAG_STATE_MASK_WARNING |  DIAG_STATE_MASK_FAILED |  DIAG_STATE_MASK_ABORTED)

typedef enum {
  QuickDiagnosticIndex,
  ConfigDiagnosticIndex,
  SecurityDiagnosticIndex,
  FwDiagnosticIndex
} DiagnosticTestIndex;

/**
  The fundamental core diagnostics function that is used by both
  the NvmDimmConfig protocol and the DriverDiagnostic protoocls.

  It runs the specified diagnotsics tests on the list of specified dimms,
  and returns a single combined test result message

  @param[in] ppDimms The platform DIMM pointers list
  @param[in] DimmsNum Platform DIMMs count
  @param[in] pDimmIds Pointer to an array of user-specified DIMM IDs
  @param[in] DimmIdsCount Number of items in the array of user-specified DIMM IDs
  @param[in] DiagnosticsTest The selected tests bitmask
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the combined result string

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
CoreStartDiagnostics(
  IN     DIMM **ppDimms,
  IN     UINT32 DimmsNum,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 DiagnosticsTest,
  IN     UINT8 DimmIdPreference,
     OUT CHAR16 **ppResult
);

/**
  Function to create a string, using the EFI_STRING_ID and the variable number
  of arguments for the format string

  Storage for the formatted Unicode string returned is allocated using
  AllocatePool(). The pointer to the created string is returned.  The caller
  is responsible for freeing the returned string.

  @param[in] StringId  ID of string
  @param[in] NumOfArgs Number of agruments passed
  @param[in] ...       The variable argument list

  @retval NULL    There was not enough available memory.
  @return         Null-terminated formatted Unicode string.
**/
CHAR16 *
CreateDiagnosticStr (
  IN     EFI_STRING_ID StringId,
  IN     UINT16 NumOfArgs,
  ...
  );

/**
  Append to the results string for a paricular diagnostic test, and modify
  the test state as per the message being appended.

  @param[in] pStrToAppend Pointer to the message string to be appended
  @param[in] DiagStateMask State corresonding to the string that is being appended
  @param[in out] ppResult Pointer to the result string of the particular test-suite's messages
  @param[in out] pDiagState Pointer to the particular test state

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
AppendToDiagnosticsResult (
  IN     CHAR16 *pStrToAppend,
  IN     UINT8 DiagStateMask,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
  );

#ifdef OS_BUILD
#include "event.h"

#define ACTION_REQUIRED_NOT_SET 0xff

/**
Append to the results string for a paricular diagnostic test, modify
the test state as per the message being appended and send the event
to the event log.

@param[in] pDimm Pointer to the DIMM structure
@param[in] pStrToAppend Pointer to the message string to be appended
@param[in] DiagStateMask State corresonding to the string that is being appended
@param[in out] ppResult Pointer to the result string of the particular test-suite's messages
@param[in out] pDiagState Pointer to the particular test state

@retval EFI_SUCCESS Success
@retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
SendTheEventAndAppendToDiagnosticsResult(
    IN     DIMM *pDimm,
    IN     UINT8 ActionReqSet,
    IN     CHAR16 *pStrToAppend,
    IN     UINT8 DiagStateMask,
    IN     UINT8 UniqeNumber,
    IN     enum system_event_category Category,
    IN OUT CHAR16 **ppResultStr,
    IN OUT UINT8 *pDiagState
);
#endif // OS_BUILD

/**
  Add headers to the message results from all the tests that were run,
  and then append those messages into one single Diagnostics result string

  @param[in] pBuffer Array of the result messages for all tests
  @param[in] DiagState Array of the result state for all tests
  @param[out] ppResult Pointer to the final result string for all tests that were run

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
CombineDiagnosticsTestResults(
  IN     CHAR16 *pBuffer[],
  IN     UINT8 DiagState[],
     OUT CHAR16 **ppResult
  );
#endif
