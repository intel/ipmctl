/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_ACPI_H_
#define _SHOW_ACPI_H_

/**
  PrintPcatHeader - prints the header of the parsed NFit table.

  @param[in] pPcat pointer to the parsed PCAT header.
**/
VOID
PrintAcpiHeader(
  IN     TABLE_HEADER *pHeader
  );

/**
  PrintPcatTable - prints the subtable of the parsed PCAT table.

  @param[in] pTable pointer to the PCAT subtable.
**/
VOID
PrintPcatTable(
  IN     PCAT_TABLE_HEADER *pTable
  );

/**
  PrintPcat - prints the header and all of the tables in the parsed PCAT table.

  @param[in] pPcat pointer to the parsed PCAT.
**/
VOID
PrintPcat(
  IN     ParsedPcatHeader *pPcat
  );

/**
  PrintFitTable - prints the subtable of the parsed NFit table.

  @param[in] pTable pointer to the NFit subtable.
**/
VOID
PrintFitTable(
  IN     SubTableHeader *pTable
  );

/**
  PrintNFit - prints the header and all of the tables in the parsed NFit table.

  @param[in] pHeader pointer to the parsed NFit header.
**/
VOID
PrintNFit(
  IN     ParsedFitHeader *pHeader
  );

/**
PrintPMTT - prints the header and all of the tables in the parsed PMTT table.

@param[in] pPMTT pointer to the parsed PMTT.
**/
VOID
PrintPMTT(
  IN     PMTT_TABLE *pPMTT
);
#endif
