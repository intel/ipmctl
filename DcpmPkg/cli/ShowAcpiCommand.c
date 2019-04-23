/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include "ShowAcpiCommand.h"
#include <Debug.h>
#include <Types.h>
#include <Utility.h>
#include <NvmInterface.h>
#include <NvmTables.h>
#include "Common.h"
#include <ShowAcpi.h>

 /*
     *  PRINT LIST ATTRIBUTES (2 levels: Target-->Type)
     *  ---NVDIMM Firmware Interface Table---
     *     ---TableType=0X0
     *        Length: 56 bytes
     *        TypeEquals: SpaRange
     *        ...
     */
PRINTER_LIST_ATTRIB ShowAcpiListAttributes =
{
 {
    {
      ACPI_MODE_STR,                                                              //GROUP LEVEL TYPE
      L"---$(" SYSTEM_TARGET_STR L")---",                                         //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                                //NULL or KEY VAL FORMAT STR
      SYSTEM_TARGET_STR                                                           //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      ACPITYPE_MODE_STR,                                                          //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" ACPI_TYPE_STR L"=$(" ACPI_TYPE_STR L")",             //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                //NULL or KEY VAL FORMAT STR
      ACPI_TYPE_STR                                                               //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowAcpiDataSetAttribs =
{
  &ShowAcpiListAttributes,
  NULL
};

/* Command syntax definition */
struct Command showAcpiCommand =
{
  SHOW_VERB,                                                           //!< verb
  {                                                                    //!< options
    {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT,FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP,HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
    {L"", L"", L"", L"",L"", FALSE, ValueOptional}
#endif
  },                        //!< options
  {{SYSTEM_TARGET, L"", SYSTEM_ACPI_TARGETS, TRUE, ValueOptional}},    //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                             //!< properties
  L"Show the ACPI tables related to the DIMMs in the system.",//!< help
  showAcpi,                                                            //!< run function
  TRUE                                                                 //!< enable print control support
};

/*
 * Register the show acpi command
 */
EFI_STATUS registerShowAcpiCommand() {
  EFI_STATUS rc = EFI_SUCCESS;
  NVDIMM_ENTRY();

  rc = RegisterCommand(&showAcpiCommand);

  NVDIMM_EXIT_I64(rc);
  return rc;
}

/*
 * Execute the show acpi command
 */
EFI_STATUS showAcpi(struct Command *pCmd) {
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  ParsedFitHeader *pNFit = NULL;
  ParsedPcatHeader *pPcat = NULL;
  PMTT_TABLE *pPMTT = NULL;
  CHAR16 *pSystemTargetValue = NULL;
  CHAR16 **ppStringElements = NULL;
  UINT32 ElementsCount = 0;
  UINT8 ChosenAcpiSystem = AcpiUnknown;
  UINT16 Index = 0;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  NVDIMM_ENTRY();

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  if (!ContainTarget(pCmd, SYSTEM_TARGET)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pSystemTargetValue = GetTargetValue(pCmd, SYSTEM_TARGET);

  ppStringElements = StrSplit(pSystemTargetValue, L',', &ElementsCount);
  if (ppStringElements != NULL) {
    for (Index = 0; Index < ElementsCount; ++Index) {
      if (StrICmp(ppStringElements[Index], NFIT_TARGET_VALUE) == 0) {
        ChosenAcpiSystem |= AcpiNfit;
      } else if (StrICmp(ppStringElements[Index], PCAT_TARGET_VALUE) == 0) {
        ChosenAcpiSystem |= AcpiPcat;
      } else if (StrICmp(ppStringElements[Index], PMTT_TARGET_VALUE) == 0) {
        ChosenAcpiSystem |= AcpiPMTT;
      } else {
        PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"The provided system ACPI Table: " FORMAT_STR L" is not valid\n", pSystemTargetValue);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
    }
  } else {
    /* If no target value is mentioned, then print all */
    ChosenAcpiSystem = AcpiAll;
  }

  if (ChosenAcpiSystem == AcpiAll || ChosenAcpiSystem == AcpiNfit) {
    ReturnCode = pNvmDimmConfigProtocol->GetAcpiNFit(pNvmDimmConfigProtocol, &pNFit);
    if (EFI_ERROR(ReturnCode) || pNFit == NULL) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: Failed to find the NVDIMM Firmware Interface ACPI tables\n");
      ReturnCode = EFI_ABORTED;
    } else {
      PrintNFit(pNFit, pPrinterCtx);
    }
  }

  if (ChosenAcpiSystem == AcpiAll || ChosenAcpiSystem == AcpiPcat) {
    ReturnCode = pNvmDimmConfigProtocol->GetAcpiPcat(pNvmDimmConfigProtocol, &pPcat);
    if (EFI_ERROR(ReturnCode) || pPcat == NULL) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: Failed to find the PCAT tables\n");
    } else {
      PrintPcat(pPcat, pPrinterCtx);
    }
  }

  if (ChosenAcpiSystem == AcpiAll || ChosenAcpiSystem == AcpiPMTT) {
    ReturnCode = pNvmDimmConfigProtocol->GetAcpiPMTT(pNvmDimmConfigProtocol, &pPMTT);
    if (ReturnCode == EFI_NOT_FOUND) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"PMTT table not found.\n");
      ReturnCode = EFI_SUCCESS;
    } else if (EFI_ERROR(ReturnCode)|| pPMTT == NULL) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: Failed to find the PMTT tables\n");
    } else {
      PrintPMTT(pPMTT, pPrinterCtx);
      FREE_POOL_SAFE(pPMTT);
    }
  }

  //Specify list attributes & display dataset as a list
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowAcpiDataSetAttribs);
  PRINTER_ENABLE_LIST_TABLE_FORMAT(pPrinterCtx);
Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  if (ppStringElements != NULL) {
    FreeStringArray(ppStringElements, ElementsCount);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
