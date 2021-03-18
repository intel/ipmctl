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

#define APPEND_RESULT_TO_THE_LOG(pDimm,String,Code,StateMask,ppResult,pState,...) { \
  CHAR16 *pTempHiiString = HiiGetString(gNvmDimmData->HiiHandle, String, NULL); \
  CHAR16 *pTempHiiString1 = CatSPrintClean(NULL, pTempHiiString, ## __VA_ARGS__); \
  FREE_POOL_SAFE(pTempHiiString); \
  if (pTempHiiString1) \
    AppendToDiagnosticsResult(pDimm, Code, pTempHiiString1, StateMask, ppResult, pState); \
}

 /** Diagnostics State bitmasks **/
#define DIAG_STATE_MASK_OK         BIT0
#define DIAG_STATE_MASK_WARNING    BIT1
#define DIAG_STATE_MASK_FAILED     BIT2
#define DIAG_STATE_MASK_ABORTED    BIT3
#define DIAG_STATE_MASK_ALL (DIAG_STATE_MASK_OK |  DIAG_STATE_MASK_WARNING |  DIAG_STATE_MASK_FAILED |  DIAG_STATE_MASK_ABORTED)

/** Event Codes' definitions **/
/* Diagnostic Quick Eve nts **/
#define EVENT_CODE_500      500
#define EVENT_CODE_501      501
#define EVENT_CODE_502      502
#define EVENT_CODE_503      503
#define EVENT_CODE_504      504
#define EVENT_CODE_505      505
#define EVENT_CODE_506      506
#define EVENT_CODE_507      507
#define EVENT_CODE_511      511
#define EVENT_CODE_513      513
#define EVENT_CODE_514      514
#define EVENT_CODE_515      515
#define EVENT_CODE_519      519
#define EVENT_CODE_520      520
#define EVENT_CODE_521      521
#define EVENT_CODE_522      522
#define EVENT_CODE_523      523
#define EVENT_CODE_529      529
#define EVENT_CODE_530      530
#define EVENT_CODE_533      533
#define EVENT_CODE_534      534
#define EVENT_CODE_535      535
#define EVENT_CODE_536      536
#define EVENT_CODE_537      537
#define EVENT_CODE_538      538
#define EVENT_CODE_539      539
#define EVENT_CODE_540      540
#define EVENT_CODE_541      541
#define EVENT_CODE_542      542
#define EVENT_CODE_543      543
#define EVENT_CODE_544      544
#define EVENT_CODE_545      545
/* Diagnostic Config Platform Events **/
#define EVENT_CODE_600      600
#define EVENT_CODE_601      601
#define EVENT_CODE_606      606
#define EVENT_CODE_608      608
#define EVENT_CODE_609      609
#define EVENT_CODE_618      618
#define EVENT_CODE_621      621
#define EVENT_CODE_622      622
#define EVENT_CODE_623      623
#define EVENT_CODE_624      624
#define EVENT_CODE_625      625
#define EVENT_CODE_626      626
#define EVENT_CODE_627      627
#define EVENT_CODE_628      628
#define EVENT_CODE_629      629
#define EVENT_CODE_630      630
#define EVENT_CODE_631      631
#define EVENT_CODE_632      632
#define EVENT_CODE_633      633
/* Diagnostic Security Events **/
#define EVENT_CODE_800      800
#define EVENT_CODE_801      801
#define EVENT_CODE_802      802
#define EVENT_CODE_804      804
#define EVENT_CODE_805      805
/* Diagnostic Firmware Events **/
#define EVENT_CODE_900      900
#define EVENT_CODE_901      901
#define EVENT_CODE_902      902
#define EVENT_CODE_903      903
#define EVENT_CODE_904      904
#define EVENT_CODE_905      905
#define EVENT_CODE_906      906
#define EVENT_CODE_910      910
#define EVENT_CODE_911      911
typedef enum {
  QuickDiagnosticIndex,
  ConfigDiagnosticIndex,
  SecurityDiagnosticIndex,
  FwDiagnosticIndex
} DiagnosticTestIndex;

/**
  The fundamental core diagnostics function that is used by both
  the NvmDimmConfig protocol and the DriverDiagnostic protocols.

  It runs the specified diagnostics tests on the list of specified dimms,
  and returns a single combined test result message

  @param[in] ppDimms The platform DIMM pointers list
  @param[in] DimmsNum Platform DIMMs count
  @param[in] pDimmIds Pointer to an array of user-specified DIMM IDs
  @param[in] DimmIdsCount Number of items in the array of user-specified DIMM IDs
  @param[in] DiagnosticsTest The selected tests bitmask
  @param[in] DimmIdPreference Preference for Dimm ID display (UID/Handle)
  @param[out] ppResult Pointer to the structure with information about test result

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
  OUT DIAG_INFO **ppResult
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
  Append to the results string for a particular diagnostic test, and modify
  the test state as per the message being appended.

  @param[in] pStrToAppend Pointer to the message string to be appended
  @param[in] DiagStateMask State corresponding to the string that is being appended
  @param[in out] ppResult Pointer to the result string of the particular test-suite's messages
  @param[in out] pDiagState Pointer to the particular test state

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL
**/
EFI_STATUS
AppendToDiagnosticsResult (
  IN     DIMM *pDimm OPTIONAL,
  IN     UINT32 Code OPTIONAL,
  IN     CHAR16 *pStrToAppend,
  IN     UINT8 DiagStateMask,
  IN OUT CHAR16 **ppResultStr,
  IN OUT UINT8 *pDiagState
  );

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

/**
  This function should be used to update status of the test based on information stored
  inside diagnostic information structure.

  @param[in] pBuffer Pointer to Diagnostic information structure
  @param[in] DiagnosticTestIndex Test Index

  @retval EFI_SUCCESS Test executed correctly
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
**/
EFI_STATUS
UpdateTestState(
  IN   DIAG_INFO *pBuffer,
  IN   UINT8 DiagnosticTestIndex
);
#endif
