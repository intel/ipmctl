/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_ACPI_H_
#define _SHOW_ACPI_H_

#include <Printer.h>

#define SYSTEM_TARGET_STR          L"Target"
#define ACPI_TYPE_STR              L"TableType"
#define DS_ROOT_PATH               L"/AcpiList"
#define DS_ACPI_PATH               L"/AcpiList/Acpi"
#define DS_ACPI_INDEX_PATH         L"/AcpiList/Acpi[%d]"
#define DS_ACPITYPE_PATH           L"/AcpiList/Acpi/Type"
#define DS_ACPITYPE_INDEX_PATH     L"/AcpiList/Acpi[%d]/Type[%d]"

/**
  PrintPcatHeader - prints the header of the parsed NFit table.

  @param[in] pPcat pointer to the parsed PCAT header.
  @param[in] pointer to command's printer context.
**/
VOID
PrintAcpiHeader(
  IN     TABLE_HEADER *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx
  );

/**
  PrintPcatTable - prints the subtable of the parsed PCAT table.

  @param[in] pTable pointer to the PCAT subtable.
  @param[in] pointer to command's printer context.
**/
VOID
PrintPcatTable(
  IN     PCAT_TABLE_HEADER *pTable,
  IN     PRINT_CONTEXT *pPrinterCtx
  );

/**
  PrintPcat - prints the header and all of the tables in the parsed PCAT table.

  @param[in] pPcat pointer to the parsed PCAT.
  @param[in] pointer to command's printer context.
**/
VOID
PrintPcat(
  IN     ParsedPcatHeader *pPcat,
  IN     PRINT_CONTEXT *pPrinterCtx
  );

/**
  PrintFitTable - prints the subtable of the parsed NFit table.

  @param[in] pTable pointer to the NFit subtable.
  @param[in] pointer to command's printer context.
**/
VOID
PrintFitTable(
  IN     SubTableHeader *pTable,
  IN     PRINT_CONTEXT *pPrinterCtx
  );

/**
  PrintNFit - prints the header and all of the tables in the parsed NFit table.

  @param[in] pHeader pointer to the parsed NFit header.
  @param[in] pointer to command's printer context.
**/
VOID
PrintNFit(
  IN     ParsedFitHeader *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx
  );

/**
PrintPMTT - prints the header and all of the tables in the parsed PMTT table.

@param[in] pPMTT pointer to the parsed PMTT.
@param[in] pointer to command's printer context.
**/
VOID
PrintPMTT(
  IN     PMTT_TABLE *pPMTT,
  IN     PRINT_CONTEXT *pPrinterCtx
);
#endif
