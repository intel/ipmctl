/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_PCD_COMMAND_
#define _SHOW_PCD_COMMAND_

/**
  Register the Show PCD command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowPcdCommand(
  );

/**
  Execute the Show PCD command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
**/
EFI_STATUS
ShowPcd(
  IN     struct Command *pCmd
  );

/**
  Print Platform Config Data table header

  @param[in] pHeader table header
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdTableHeader(
  IN     TABLE_HEADER *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print Platform Config Data PCAT table header

  @param[in] pHeader PCAT table header
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdPcatTableHeader(
  IN     PCAT_TABLE_HEADER *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print Platform Config Data Partition Size Change table

  @param[in] pPartitionSizeChange Partition Size Change table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdPartitionSizeChange(
  IN     NVDIMM_PARTITION_SIZE_CHANGE *pPartitionSizeChange,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print  Platform Config Data Identification Information table

  @param[in] pIdentificationInfo Identification Information table
  @param[in] PcdConfigTableRevision Revision of the PCD Config tables
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdIdentificationInformation(
  IN     VOID *pIdentificationInfo,
  IN     ACPI_REVISION PcdConfigTableRevision,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print Platform Config Data Interleave Information table and its extension tables

  @param[in] pInterleaveInfo Interleave Information table
  @param[in] PcdConfigTableRevision Revision of the PCD Config tables
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdInterleaveInformation(
  IN     PCAT_TABLE_HEADER *pInterleaveInfo,
  IN     ACPI_REVISION PcdConfigTableRevision,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print Platform Config Data Current Config table and its PCAT tables

  @param[in] pCurrentConfig Current Config table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdCurrentConfig(
  IN     NVDIMM_CURRENT_CONFIG *pCurrentConfig,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print Platform Config Data Config Input table and its PCAT tables

  @param[in] pConfigInput Config Input table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdConfInput(
  IN     NVDIMM_PLATFORM_CONFIG_INPUT *pConfigInput,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
  Print Platform Config Data Config Output table and its PCAT tables

  @param[in] pConfigOutput Config Output table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/

VOID
PrintPcdConfOutput(
  IN     NVDIMM_PLATFORM_CONFIG_OUTPUT *pConfigOutput,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
   Print Platform Config Data Configuration Header table and all subtables

   @param[in] pConfHeader Configuration Header table
   @param[in] pPrinterCtx pointer for printer
   @param[in] pPath
**/

VOID
PrintPcdConfigurationHeader(
  IN     NVDIMM_CONFIGURATION_HEADER *pConfHeader,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
  );

/**
   Print Namespace Index

   @param[in] pNamespaceIndex Namespace Index
**/
VOID
PrintNamespaceIndex(
  IN     NAMESPACE_INDEX *pNamespaceIndex
);

/**
   Print Namespace Label

   @param[in] pNamespaceLabel Namespace Label
   @param[in] pPrinterCtx pointer for printer
   @param[in] pPath
**/
VOID
PrintNamespaceLabel(
  IN     NAMESPACE_LABEL *pNamespaceLabel,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
);

/**
   Print Label Storage Area and all subtables

   @param[in] pLba Label Storage Area
   @param[in] pPrinterCtx pointer for printer
   @param[in] pPath
**/
VOID
PrintLabelStorageArea(
  IN     LABEL_STORAGE_AREA *pLba,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
);

/**
  Function that allows for nicely formatted HEX & ASCII console output.
  It can be used to inspect memory objects without a need for debugger or dumping raw DIMM data.

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/

VOID
PrintLsaHex(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
);

  #endif /* _SHOW_PCD_COMMAND_ */
