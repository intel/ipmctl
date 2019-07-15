/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Uefi.h>
#include <Debug.h>
#include <NvmTables.h>
#include <Utility.h>
#include <Convert.h>
#include <ShowAcpi.h>

CHAR16 *pPath = NULL;
UINT32 AcpiIndex = 0;
CHAR16 *pTypePath = NULL;
UINT32 TypeIndex = 0;
ACPI_REVISION PcatRevision;

/**
  DecodePcatMemoryModeCapabilities - decodes the MemoryModeCapabilities field of PCAT structure type: PlatformCapabilityInfoTable

  @param[in] pPcatMemoryModeCapabilities pointer to MemoryModeCapabilities

  @retval NULL if no memory mode capabilities supported
  @retval comma separated string of supported capabilities
**/
CHAR16*
DecodePcatMemoryModeCapabilities(
  IN     VOID *pPcatMemoryModeCapabilitiesField
  )
{
  CHAR16 *MemoryModeCapabilities = NULL;

  if (pPcatMemoryModeCapabilitiesField == NULL) {
    return MemoryModeCapabilities;
  }

  if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
    SUPPORTED_MEMORY_MODE *pPcatMemoryModeCapabilities = (SUPPORTED_MEMORY_MODE *)pPcatMemoryModeCapabilitiesField;
    if (pPcatMemoryModeCapabilities->MemoryModesFlags.OneLm) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"1LM");
    }

    if (pPcatMemoryModeCapabilities->MemoryModesFlags.Memory) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"2LM");
    }

    if (pPcatMemoryModeCapabilities->MemoryModesFlags.AppDirect) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"AppDirect");
    }

    if (pPcatMemoryModeCapabilities->MemoryModesFlags.Storage) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"Storage");
    }

    if (pPcatMemoryModeCapabilities->MemoryModesFlags.SubNUMAClster) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"SubNUMA Cluster");
    }
  } else if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
    SUPPORTED_MEMORY_MODE3 *pPcatMemoryModeCapabilities = (SUPPORTED_MEMORY_MODE3 *)pPcatMemoryModeCapabilitiesField;
    if (pPcatMemoryModeCapabilities->MemoryModesFlags.OneLm) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"1LM");
    }

    if (pPcatMemoryModeCapabilities->MemoryModesFlags.Memory) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"2LM");
    }

    if (pPcatMemoryModeCapabilities->MemoryModesFlags.AppDirect) {
      MemoryModeCapabilities = CatSPrintClean(MemoryModeCapabilities,
        ((MemoryModeCapabilities == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        L"AppDirect");
    }
  }

  if (MemoryModeCapabilities == NULL) {
    NVDIMM_DBG("DecodePcatMemoryModeCapabilities failed. Not enough resources!");
  }
  return MemoryModeCapabilities;
}

/**
  DecodePcatCurrentMemoryMode - decodes the CurrentMemoryMode field of PCAT structure type: PlatformCapabilityInfoTable

  @param[in] pPcatCurrentMemoryMode pointer to CurrentMemoryMode
  @param[in] pPcatMemoryModeCapabilities pointer to MemoryModeCapabilities

  @retval NULL if no current memory mode set
  @retval string showing the current memory mode
**/
CHAR16*
DecodePcatCurrentMemoryMode(
  IN     VOID *pPcatCurrentMemoryModeField,
  IN     SUPPORTED_MEMORY_MODE *pPcatMemoryModeCapabilities
)
{
  UINT8 mask = BIT0 | BIT1;
  CHAR16 *CurrentMemoryMode = NULL;

  if (pPcatCurrentMemoryModeField == NULL) {
    return CurrentMemoryMode;
  }

  if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
    CURRENT_MEMORY_MODE *pPcatCurrentMemoryMode = (CURRENT_MEMORY_MODE *)pPcatCurrentMemoryModeField;
    if ((pPcatCurrentMemoryMode->MemoryModeSplit.CurrentVolatileMode & mask) == 0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Current Volatile Memory Mode", L"1LM");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.CurrentVolatileMode & mask) == BIT0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Current Volatile Memory Mode", L"2LM");
    }

    if ((pPcatCurrentMemoryMode->MemoryModeSplit.PersistentMode & mask) == 0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Persistent Memory Mode", L"None");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.PersistentMode & mask) == BIT0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Persistent Memory Mode", L"AppDirect");
    }

    if ((pPcatCurrentMemoryMode->MemoryModeSplit.AllowedVolatileMode & mask) == 0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Volatile Memory Mode", L"1LM");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.AllowedVolatileMode & mask) == BIT0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Volatile Memory Mode", L"2LM");
    }

    //Check if SubNUMA Cluster Mode is enabled
    if (pPcatMemoryModeCapabilities->MemoryModesFlags.SubNUMAClster) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR FORMAT_STR,
        L"-SubNUMA Cluster Mode ", ((pPcatCurrentMemoryMode->MemoryModeSplit.SubNumaCluster) ? L"Enabled" : L"Disabled"));
    }
  } else if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
    CURRENT_MEMORY_MODE3 *pPcatCurrentMemoryMode = (CURRENT_MEMORY_MODE3 *)pPcatCurrentMemoryModeField;
    if ((pPcatCurrentMemoryMode->MemoryModeSplit.CurrentVolatileMode & mask) == 0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Current Volatile Memory Mode", L"1LM");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.CurrentVolatileMode & mask) == BIT0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Current Volatile Memory Mode", L"2LM");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.CurrentVolatileMode & mask) == BIT1) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Current Volatile Memory Mode", L"1LM + 2LM");
    }

    if ((pPcatCurrentMemoryMode->MemoryModeSplit.PersistentMode & mask) == 0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Persistent Memory Mode", L"None");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.PersistentMode & mask) == BIT0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Persistent Memory Mode", L"AppDirect");
    }

    if ((pPcatCurrentMemoryMode->MemoryModeSplit.AllowedVolatileMode & mask) == 0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Volatile Memory Mode", L"1LM");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.AllowedVolatileMode & mask) == BIT0) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Volatile Memory Mode", L"1LM or 2LM");
    }
    else if ((pPcatCurrentMemoryMode->MemoryModeSplit.AllowedVolatileMode & mask) == BIT1) {
      CurrentMemoryMode = CatSPrintClean(CurrentMemoryMode,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Allowed Volatile Memory Mode", L"1LM + 2LM");
    }

  }

  if (CurrentMemoryMode == NULL) {
    NVDIMM_DBG("DecodePcatCurrentMemoryMode failed. Not enough resources!");
  }
  return CurrentMemoryMode;
}

/**
  DecodePcatInterleaveFormatSupported - decodes the InterleaveFormatSupported field of PCAT structure type: MemoryInterleaveCapabilityTable

  @param[in] pPcatInterleaveFormatSupported pointer to InterleaveFormatSupported

  @retval NULL if no interleave information is present
  @retval string showing the interleave information
**/
CHAR16*
DecodePcatInterleaveFormatSupported(
  IN     VOID *pPcatInterleaveFormatSupportedField,
  IN     INTERLEAVE_SIZE *InterleaveSizeSupported
)
{
  UINT16 mask2 = BIT0;
  UINT8 skip = BIT3 | BIT4 | BIT5;
  UINT8 index = 0;
  UINT8 NumOfBitsSet = 0;
  CHAR16 *InterleaveFormatSupported = NULL;
  CHAR16 *InterleaveSize[] = {L"64B", L"128B", L"256B", L"Reserved", L"Reserved", L"Reserved", L"4KB", L"Reserved"};
  CHAR16 *NoOfChannelWays[] = {L"1-way", L"2-way", L"3-way", L"4-way", L"6-way", L"8-way", L"12-way", L"16-way", L"24-way"};
  CHAR16 *ChannelWaysSupported = NULL;
  UINT32 ChannelInterleaveSize = 0;
  UINT32 ImcInterleaveSize = 0;
  UINT16 NumberOfChannelWays = 0;
  UINT32 Recommended = 0;

  if (pPcatInterleaveFormatSupportedField == NULL) {
    return InterleaveFormatSupported;
  }

  if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
    INTERLEAVE_FORMAT *pPcatInterleaveFormatSupported = (INTERLEAVE_FORMAT *)pPcatInterleaveFormatSupportedField;
    ChannelInterleaveSize = pPcatInterleaveFormatSupported->InterleaveFormatSplit.ChannelInterleaveSize;
    ImcInterleaveSize = pPcatInterleaveFormatSupported->InterleaveFormatSplit.iMCInterleaveSize;
    NumberOfChannelWays = pPcatInterleaveFormatSupported->InterleaveFormatSplit.NumberOfChannelWays & MAX_UINT16;
    Recommended = pPcatInterleaveFormatSupported->InterleaveFormatSplit.Recommended;
  }
  else if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
    INTERLEAVE_FORMAT3 *pPcatInterleaveFormatSupported = (INTERLEAVE_FORMAT3 *)pPcatInterleaveFormatSupportedField;
    ChannelInterleaveSize = InterleaveSizeSupported->InterleaveSizeSplit.ChannelInterleaveSize;
    ImcInterleaveSize = InterleaveSizeSupported->InterleaveSizeSplit.iMCInterleaveSize;
    CountNumOfBitsSet(pPcatInterleaveFormatSupported->InterleaveFormatSplit.InterleaveMap, &NumOfBitsSet);
    GetBitFieldForNumOfChannelWays(NumOfBitsSet, &NumberOfChannelWays);
    Recommended = pPcatInterleaveFormatSupported->InterleaveFormatSplit.Recommended;
  }

  //Check if the BIOS supported interleave format is recommended
  InterleaveFormatSupported = CatSPrintClean(InterleaveFormatSupported,
    L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR,
    (Recommended ? L"-Recommended" : L"-Not recommended"));

  while (mask2 <= BIT6) {
    if (ChannelInterleaveSize & mask2) {
      InterleaveFormatSupported = CatSPrintClean(InterleaveFormatSupported,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-Channel interleave size", InterleaveSize[index]);
      break;
    }
    //Skip the reserved bits
    do {
      index++;
      mask2 <<= 1;
    } while ((mask2 & skip));
  }

  mask2 = BIT0;
  index = 0;
  while (mask2 <= BIT6) {
    if (ImcInterleaveSize &  mask2) {
      InterleaveFormatSupported = CatSPrintClean(InterleaveFormatSupported,
        L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
        L"-iMC interleave size", InterleaveSize[index]);
      break;
    }
    //Skip the reserved bits
    do {
      index++;
      mask2 <<= 1;
    } while ((mask2 & skip));
  }

  mask2 = BIT0;
  index = 0;
  while (mask2 <= BIT8) {
    if (NumberOfChannelWays & mask2) {
      ChannelWaysSupported = CatSPrintClean(ChannelWaysSupported,
        ((ChannelWaysSupported == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA),
        NoOfChannelWays[index]);
    }
    index++;
    mask2 <<= 1;
  }
  if (ChannelWaysSupported != NULL) {
    InterleaveFormatSupported = CatSPrintClean(InterleaveFormatSupported,
      L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_STR,
      L"-Channel ways", ChannelWaysSupported);
  }

  if (InterleaveFormatSupported == NULL) {
    NVDIMM_DBG("DecodePcatInterleaveFormatSupported failed. Not enough resources!");
  }

  FREE_POOL_SAFE(ChannelWaysSupported);
  return InterleaveFormatSupported;
}

/**
  PrintPcatHeader - prints the header of the parsed NFit table.

  @param[in] pPcat pointer to the parsed PCAT header.
  @param[in] pointer to command's printer context.
**/
VOID
PrintAcpiHeader(
  IN     TABLE_HEADER *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx
  )
{
  if (pHeader == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx,
    pPath,
    L"Signature",
    L"%c%c%c%c",
    ((UINT8 *)&pHeader->Signature)[0],
    ((UINT8 *)&pHeader->Signature)[1],
    ((UINT8 *)&pHeader->Signature)[2],
    ((UINT8 *)&pHeader->Signature)[3]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Length", FORMAT_INT32 L" bytes", pHeader->Length);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Revision", FORMAT_HEX_NOWIDTH, pHeader->Revision);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Checksum", FORMAT_HEX_NOWIDTH, pHeader->Checksum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx,
    pPath,
    L"OEMID",
    L"%c%c%c%c%c%c",
    pHeader->OemId[0],
    pHeader->OemId[1],
    pHeader->OemId[2],
    pHeader->OemId[3],
    pHeader->OemId[4],
    pHeader->OemId[5]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx,
    pPath,
    L"OEMTableID",
    L"%c%c%c%c%c%c%c%c",
    ((UINT8 *)&pHeader->OemTableId)[0],
    ((UINT8 *)&pHeader->OemTableId)[1],
    ((UINT8 *)&pHeader->OemTableId)[2],
    ((UINT8 *)&pHeader->OemTableId)[3],
    ((UINT8 *)&pHeader->OemTableId)[4],
    ((UINT8 *)&pHeader->OemTableId)[5],
    ((UINT8 *)&pHeader->OemTableId)[6],
    ((UINT8 *)&pHeader->OemTableId)[7]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"OEMRevision", FORMAT_HEX_NOWIDTH, pHeader->OemRevision);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx,
    pPath,
    L"CreatorID",
    L"%c%c%c%c",
    ((UINT8 *)&pHeader->CreatorId)[0],
    ((UINT8 *)&pHeader->CreatorId)[1],
    ((UINT8 *)&pHeader->CreatorId)[2],
    ((UINT8 *)&pHeader->CreatorId)[3]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"CreatorRevision", FORMAT_HEX_NOWIDTH L"\n", pHeader->CreatorRevision);
}

/**
  PrintPcatTable - prints the subtable of the parsed PCAT table.

  @param[in] pTable pointer to the PCAT subtable.
  @param[in] pointer to command's printer context.
**/
VOID
PrintPcatTable(
  IN     PCAT_TABLE_HEADER *pTable,
  IN     PRINT_CONTEXT *pPrinterCtx
  )
{
  UINT16 Index = 0;
  CHAR16 *InterleaveFormatSupportedIndex = NULL;
  CHAR16 *InterleaveFormatSupported = NULL;
  CHAR16 *DcpmmMgmtSWConfigInputSupport = NULL;
  CHAR16 *MemoryModeCapabilities = NULL;
  CHAR16 *CurrentMemoryMode = NULL;
  CHAR16 *PersistentMemoryRasCapability = NULL;
  CHAR16 *MemoryMode[] = {L"1LM", L"2LM", L"Reserved", L"AppDirect", L"Reserved"};
  CHAR16 *MaxPMInterleaveSets = NULL;

  if (pTable == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE *pReconfInputValidationInterfaceTable = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfoTable = NULL;
  CHAR16 * pGuidStr = NULL;

  PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
  TypeIndex++;
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, ACPI_TYPE_STR, FORMAT_HEX_NOWIDTH, pTable->Type);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32 L" bytes", pTable->Length);

  switch (pTable->Type) {
  case PCAT_TYPE_PLATFORM_CAPABILITY_INFO_TABLE:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"PlatformCapabilityInfoTable");
    if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
      PLATFORM_CAPABILITY_INFO *pPlatformCapabilityInfoTable = (PLATFORM_CAPABILITY_INFO *)pTable;
      if (pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport & BIOS_SUPPORTS_CHANGING_CONFIG) {
        DcpmmMgmtSWConfigInputSupport = CatSPrintClean(DcpmmMgmtSWConfigInputSupport, FORMAT_STR, L"Yes");
      } else {
        DcpmmMgmtSWConfigInputSupport = CatSPrintClean(DcpmmMgmtSWConfigInputSupport, FORMAT_STR, L"No");
      }
      if (pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport & BIOS_SUPPORTS_RUNTIME_INTERFACE) {
        DcpmmMgmtSWConfigInputSupport = CatSPrintClean(DcpmmMgmtSWConfigInputSupport, FORMAT_STR, L" & Runtime Interface for config validation");
      }
      if (DcpmmMgmtSWConfigInputSupport != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DcpmmMgmtSWConfigInputSupport", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport, DcpmmMgmtSWConfigInputSupport);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DcpmmMgmtSWConfigInputSupport", FORMAT_HEX_NOWIDTH,
          pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport);
      }
      FREE_POOL_SAFE(DcpmmMgmtSWConfigInputSupport);
      MemoryModeCapabilities = DecodePcatMemoryModeCapabilities((VOID *)&pPlatformCapabilityInfoTable->MemoryModeCapabilities);
      if (MemoryModeCapabilities != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryModeCapabilities", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pPlatformCapabilityInfoTable->MemoryModeCapabilities, MemoryModeCapabilities);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryModeCapabilities", FORMAT_HEX_NOWIDTH,
          pPlatformCapabilityInfoTable->MemoryModeCapabilities);
      }
      FREE_POOL_SAFE(MemoryModeCapabilities);
      CurrentMemoryMode = DecodePcatCurrentMemoryMode((VOID *)&pPlatformCapabilityInfoTable->CurrentMemoryMode, &pPlatformCapabilityInfoTable->MemoryModeCapabilities);
      if (CurrentMemoryMode != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CurrentMemoryMode", FORMAT_HEX_NOWIDTH FORMAT_STR,
          pPlatformCapabilityInfoTable->CurrentMemoryMode, CurrentMemoryMode);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CurrentMemoryMode", FORMAT_HEX_NOWIDTH,
          pPlatformCapabilityInfoTable->CurrentMemoryMode);
      }
      FREE_POOL_SAFE(CurrentMemoryMode);
      if (pPlatformCapabilityInfoTable->PersistentMemoryRasCapability & PERSISTENT_MEMORY_REGION_MIRRORING) {
        PersistentMemoryRasCapability = CatSPrintClean(PersistentMemoryRasCapability, ((PersistentMemoryRasCapability == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA), L"Mirroring");
      }
      if (pPlatformCapabilityInfoTable->PersistentMemoryRasCapability & PERSISTENT_MEMORY_REGION_SPARE) {
        PersistentMemoryRasCapability = CatSPrintClean(PersistentMemoryRasCapability, ((PersistentMemoryRasCapability == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA), L"Spare");
      }
      if (pPlatformCapabilityInfoTable->PersistentMemoryRasCapability & PERSISTENT_MEMORY_REGION_MIGRATION) {
        PersistentMemoryRasCapability = CatSPrintClean(PersistentMemoryRasCapability, ((PersistentMemoryRasCapability == NULL) ? FORMAT_STR : FORMAT_STR_WITH_COMMA), L"Migration");
      }
      if (PersistentMemoryRasCapability != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"PersistentMemoryRASCapability", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS L"\n",
          pPlatformCapabilityInfoTable->PersistentMemoryRasCapability, PersistentMemoryRasCapability);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"PersistentMemoryRASCapability", FORMAT_HEX_NOWIDTH L"\n",
          pPlatformCapabilityInfoTable->PersistentMemoryRasCapability);
      }
      FREE_POOL_SAFE(PersistentMemoryRasCapability);
    } else if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
      PLATFORM_CAPABILITY_INFO3 *pPlatformCapabilityInfoTable = (PLATFORM_CAPABILITY_INFO3 *)pTable;
      if (pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport & BIOS_SUPPORTS_CHANGING_CONFIG) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DcpmmMgmtSWConfigInputSupport", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport, L"Yes");
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DcpmmMgmtSWConfigInputSupport", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport, L"No");
      }
      MemoryModeCapabilities = DecodePcatMemoryModeCapabilities((VOID *)&pPlatformCapabilityInfoTable->MemoryModeCapabilities);
      if (MemoryModeCapabilities != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryModeCapabilities", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pPlatformCapabilityInfoTable->MemoryModeCapabilities, MemoryModeCapabilities);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryModeCapabilities", FORMAT_HEX_NOWIDTH,
          pPlatformCapabilityInfoTable->MemoryModeCapabilities);
      }
      FREE_POOL_SAFE(MemoryModeCapabilities);
      CurrentMemoryMode = DecodePcatCurrentMemoryMode((VOID *)&pPlatformCapabilityInfoTable->CurrentMemoryMode, NULL);
      if (CurrentMemoryMode != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CurrentMemoryMode", FORMAT_HEX_NOWIDTH FORMAT_STR,
          pPlatformCapabilityInfoTable->CurrentMemoryMode, CurrentMemoryMode);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CurrentMemoryMode", FORMAT_HEX_NOWIDTH,
          pPlatformCapabilityInfoTable->CurrentMemoryMode);
      }
      FREE_POOL_SAFE(CurrentMemoryMode);
      MaxPMInterleaveSets = CatSPrintClean(MaxPMInterleaveSets, L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_HEX,
        L"-Per Die", pPlatformCapabilityInfoTable->MaxPMInterleaveSets.MaxInterleaveSetsSplit.PerDie);
      MaxPMInterleaveSets = CatSPrintClean(MaxPMInterleaveSets, L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_HEX,
        L"-Per DCPMM", pPlatformCapabilityInfoTable->MaxPMInterleaveSets.MaxInterleaveSetsSplit.PerDcpmm);
      if (MaxPMInterleaveSets != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MaxPMInterleaveSets", FORMAT_HEX_NOWIDTH FORMAT_STR,
          pPlatformCapabilityInfoTable->MaxPMInterleaveSets.AsUint16, MaxPMInterleaveSets);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MaxPMInterleaveSets", FORMAT_HEX_NOWIDTH,
          pPlatformCapabilityInfoTable->MaxPMInterleaveSets.AsUint16);
      }
      FREE_POOL_SAFE(MaxPMInterleaveSets);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DDRCacheSize", FORMAT_UINT32 L" GiB per DDR DIMM" L"\n", pPlatformCapabilityInfoTable->DDRCacheSize);
    }
    break;
  case PCAT_TYPE_INTERLEAVE_CAPABILITY_INFO_TABLE:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"MemoryInterleaveCapabilityTable");
    if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
      MEMORY_INTERLEAVE_CAPABILITY_INFO *pMemoryInterleaveCapabilityInfoTable = (MEMORY_INTERLEAVE_CAPABILITY_INFO *)pTable;
      if (pMemoryInterleaveCapabilityInfoTable->MemoryMode <= 4) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryMode", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pMemoryInterleaveCapabilityInfoTable->MemoryMode, MemoryMode[pMemoryInterleaveCapabilityInfoTable->MemoryMode]);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryMode", FORMAT_HEX_NOWIDTH,
          pMemoryInterleaveCapabilityInfoTable->MemoryMode);
      }
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NumberOfInterleaveFormatsSupported", FORMAT_HEX_NOWIDTH, pMemoryInterleaveCapabilityInfoTable->NumOfFormatsSupported);
      for (Index = 0; Index < pMemoryInterleaveCapabilityInfoTable->NumOfFormatsSupported; Index++) {
        InterleaveFormatSupportedIndex = CatSPrint(NULL, L"InterleaveFormatSupported(%d)", Index);
        InterleaveFormatSupported = DecodePcatInterleaveFormatSupported((VOID *)&pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index], NULL);
        if (InterleaveFormatSupported != NULL) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, InterleaveFormatSupportedIndex, FORMAT_HEX_NOWIDTH FORMAT_STR,
            pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index], InterleaveFormatSupported);
        } else {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, InterleaveFormatSupportedIndex, FORMAT_HEX_NOWIDTH,
            pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index]);
        }
        FREE_POOL_SAFE(InterleaveFormatSupported);
      }
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"InterleaveAlignmentSize", FORMAT_HEX_NOWIDTH L"\n", pMemoryInterleaveCapabilityInfoTable->InterleaveAlignmentSize);
    } else if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
      MEMORY_INTERLEAVE_CAPABILITY_INFO3 *pMemoryInterleaveCapabilityInfoTable = (MEMORY_INTERLEAVE_CAPABILITY_INFO3 *)pTable;
      if (pMemoryInterleaveCapabilityInfoTable->MemoryMode <= 4) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryMode", FORMAT_HEX_NOWIDTH FORMAT_STR_WITH_PARANTHESIS,
          pMemoryInterleaveCapabilityInfoTable->MemoryMode, MemoryMode[pMemoryInterleaveCapabilityInfoTable->MemoryMode]);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryMode", FORMAT_HEX_NOWIDTH,
          pMemoryInterleaveCapabilityInfoTable->MemoryMode);
      }
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NumberOfInterleaveFormatsSupported", FORMAT_HEX_NOWIDTH, pMemoryInterleaveCapabilityInfoTable->NumOfFormatsSupported);
      for (Index = 0; Index < pMemoryInterleaveCapabilityInfoTable->NumOfFormatsSupported; Index++) {
        InterleaveFormatSupportedIndex = CatSPrint(NULL, L"InterleaveFormatSupported(%d)", Index);
        InterleaveFormatSupported = DecodePcatInterleaveFormatSupported((VOID *)&pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index], &pMemoryInterleaveCapabilityInfoTable->InterleaveSize);
        if (InterleaveFormatSupported != NULL) {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, InterleaveFormatSupportedIndex, FORMAT_HEX_NOWIDTH FORMAT_STR,
            pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index], InterleaveFormatSupported);
        } else {
          PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, InterleaveFormatSupportedIndex, FORMAT_HEX_NOWIDTH,
            pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index]);
        }
        FREE_POOL_SAFE(InterleaveFormatSupported);
      }
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"InterleaveAlignmentSize", FORMAT_HEX_NOWIDTH, pMemoryInterleaveCapabilityInfoTable->InterleaveAlignmentSize);
      MaxPMInterleaveSets = CatSPrintClean(MaxPMInterleaveSets, L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_HEX,
        L"-Per Die", pMemoryInterleaveCapabilityInfoTable->MaxInterleaveSetsPerMemType.MaxInterleaveSetsSplit.PerDie);
      MaxPMInterleaveSets = CatSPrintClean(MaxPMInterleaveSets, L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR_COLON_SPACE_HEX,
        L"-Per DCPMM", pMemoryInterleaveCapabilityInfoTable->MaxInterleaveSetsPerMemType.MaxInterleaveSetsSplit.PerDcpmm);
      if (MaxPMInterleaveSets != NULL) {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MaxPMInterleaveSetsPerMemType", FORMAT_HEX_NOWIDTH FORMAT_STR L"\n",
          pMemoryInterleaveCapabilityInfoTable->MaxInterleaveSetsPerMemType.AsUint16, MaxPMInterleaveSets);
      } else {
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MaxPMInterleaveSetsPerMemType", FORMAT_HEX_NOWIDTH L"\n",
          pMemoryInterleaveCapabilityInfoTable->MaxInterleaveSetsPerMemType.AsUint16);
      }
      FREE_POOL_SAFE(MaxPMInterleaveSets);
    }
    break;
  case PCAT_TYPE_RUNTIME_INTERFACE_TABLE:
    pReconfInputValidationInterfaceTable = (RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE *) pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"Re-configurationInputValidationInterfaceTable");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"AddressSpaceID", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->AddressSpaceId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"BitWidth", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->BitWidth);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"BitOffset", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->BitOffset);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"AccessSize", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->AccessSize);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Address", FORMAT_UINT64_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->Address);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"OperationType", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->TriggerOperationType);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Value", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->TriggerValue);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Mask", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->TriggerMask);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"GASStructure", FORMAT_STR, pReconfInputValidationInterfaceTable->GasStructure[0] == 0 ? L"System Memory" : L"Unknown");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"OperationType", FORMAT_HEX_NOWIDTH, pReconfInputValidationInterfaceTable->StatusOperationType);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Mask", FORMAT_HEX_NOWIDTH L"\n", pReconfInputValidationInterfaceTable->StatusMask);
    break;
  case PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE:
    pConfigManagementAttributesInfoTable = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pTable;
    pGuidStr = GuidToStr(&pConfigManagementAttributesInfoTable->Guid);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"ConfigurationManagementAttributesExtensionTable");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"VendorID", FORMAT_HEX_NOWIDTH, pConfigManagementAttributesInfoTable->VendorId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"GUID", FORMAT_STR, pGuidStr);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"GUIDDataPointer", FORMAT_POINTER L"\n", pConfigManagementAttributesInfoTable->pGuidData);
    FREE_POOL_SAFE(pGuidStr);
    break;
  case PCAT_TYPE_SOCKET_SKU_INFO_TABLE:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"SocketSkuInfoTable");
    if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
      SOCKET_SKU_INFO_TABLE *pSocketSkuInfoTable = (SOCKET_SKU_INFO_TABLE *)pTable;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SocketID", FORMAT_HEX_NOWIDTH, pSocketSkuInfoTable->SocketId);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MappedMemorySizeLimit", L"%ld", pSocketSkuInfoTable->MappedMemorySizeLimit);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"TotalMemorySizeMappedToSpa", L"%ld", pSocketSkuInfoTable->TotalMemorySizeMappedToSpa);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CachingMemorySize", L"%ld\n", pSocketSkuInfoTable->CachingMemorySize);
    } else if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
      DIE_SKU_INFO_TABLE *pDieSkuInfoTable = (DIE_SKU_INFO_TABLE *)pTable;
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SocketID", FORMAT_HEX_NOWIDTH, pDieSkuInfoTable->SocketId);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DieID", FORMAT_HEX_NOWIDTH, pDieSkuInfoTable->DieId);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MappedMemorySizeLimit", L"%ld", pDieSkuInfoTable->MappedMemorySizeLimit);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"TotalMemorySizeMappedToSpa", L"%ld", pDieSkuInfoTable->TotalMemorySizeMappedToSpa);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CachingMemorySize", L"%ld\n", pDieSkuInfoTable->CachingMemorySize);
    }
    break;
  default:
    break;
  }
}

/**
  PrintPcat - prints the header and all of the tables in the parsed PCAT table.

  @param[in] pPcat pointer to the parsed PCAT.
  @param[in] pointer to command's printer context.
**/
VOID
PrintPcat(
  IN     ParsedPcatHeader *pPcat,
  IN     PRINT_CONTEXT *pPrinterCtx
  )
{
  UINT32 Index = 0;

  if (pPcat == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  PRINTER_BUILD_KEY_PATH(pPath, DS_ACPI_INDEX_PATH, AcpiIndex);
  AcpiIndex++;
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SYSTEM_TARGET_STR, L"Platform Configurations Attributes Table");

  PcatRevision = pPcat->pPlatformConfigAttr->Header.Revision;
  PrintAcpiHeader(&pPcat->pPlatformConfigAttr->Header, pPrinterCtx);

  for (Index = 0; Index < pPcat->PlatformCapabilityInfoNum; Index++) {
    if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
      if (pPcat->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo[Index] == NULL) {
        return;
      }
      PrintPcatTable((PCAT_TABLE_HEADER *)pPcat->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo[Index], pPrinterCtx);
    }

    if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
      if (pPcat->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo[Index] == NULL) {
        return;
      }
      PrintPcatTable((PCAT_TABLE_HEADER *)pPcat->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo[Index], pPrinterCtx);
    }
  }

  for (Index = 0; Index < pPcat->MemoryInterleaveCapabilityInfoNum; Index++) {
    if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
      if (pPcat->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo[Index] == NULL) {
        return;
      }
      PrintPcatTable((PCAT_TABLE_HEADER *)pPcat->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo[Index], pPrinterCtx);
    }

    if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
      if (pPcat->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo[Index] == NULL) {
        return;
      }
      PrintPcatTable((PCAT_TABLE_HEADER *)pPcat->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo[Index], pPrinterCtx);
    }
  }

  for (Index = 0; Index < pPcat->RuntimeInterfaceValConfInputNum; Index++) {
    if (pPcat->ppRuntimeInterfaceValConfInput[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppRuntimeInterfaceValConfInput[Index], pPrinterCtx);
  }

  for (Index = 0; Index < pPcat->ConfigManagementAttributesInfoNum; Index++) {
    if (pPcat->ppConfigManagementAttributesInfo[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppConfigManagementAttributesInfo[Index], pPrinterCtx);
  }

  for (Index = 0; Index < pPcat->SocketSkuInfoNum; Index++) {
    if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(PcatRevision)) {
      if (pPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable[Index] == NULL) {
        return;
      }
      PrintPcatTable((PCAT_TABLE_HEADER *)pPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable[Index], pPrinterCtx);
    }

    if (IS_ACPI_REV_MAJ_1_MIN_1(PcatRevision)) {
      if (pPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable[Index] == NULL) {
        return;
      }
      PrintPcatTable((PCAT_TABLE_HEADER *)pPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable[Index], pPrinterCtx);
    }
  }
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pTypePath);
}

/**
  DecodeNfitNvDimmStateFlags - decodes the NvDimmStateFlags field of NFIT structure type: NvDimmRegion

  @param[in] NvDimmStateFlags field from NFIT table to be decoded

  @retval string showing information about events & operation status based on flags set
**/
CHAR16*
DecodeNfitNvDimmStateFlags(
  IN     UINT16 nfitNvDimmStateFlags
)
{
  UINT8 mask = BIT0;
  CHAR16 *NvDimmStateFlags = NULL;

  while (mask <= BIT6) {
    switch (mask) {
    case NVDIMM_STATE_FLAGS_SAVE:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-Save failed", NVDIMM_STATE_FLAGS_SAVE);
      }
      break;
    case NVDIMM_STATE_FLAGS_RESTORE:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-Restore failed", NVDIMM_STATE_FLAGS_RESTORE);
      }
      break;
    case NVDIMM_STATE_FLAGS_FLUSH:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-Flush failed", NVDIMM_STATE_FLAGS_FLUSH);
      }
      break;
    case NVDIMM_STATE_FLAGS_REGION_ARMED:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-PM writes disabled or Not armed or Previous ERASE failed", NVDIMM_STATE_FLAGS_REGION_ARMED);
      }
      break;
    case NVDIMM_STATE_FLAGS_EVENTS_OBSERVED:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-Smart & Health events prior to OSPM handoff", NVDIMM_STATE_FLAGS_EVENTS_OBSERVED);
      }
      break;
    case NVDIMM_STATE_FLAGS_EVENTS_NOTIFY:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-Notify OSPM of Smart & Health events", NVDIMM_STATE_FLAGS_EVENTS_NOTIFY);
      }
      break;
    case NVDIMM_STATE_FLAGS_NOT_MAPPED:
      if (nfitNvDimmStateFlags & mask) {
        NvDimmStateFlags = CatSPrintClean(NvDimmStateFlags,
          L"\n" SHOW_LIST_IDENT SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR SPACE_FORMAT_HEX,
          L"-NVDIMM region not mapped into SPA range", NVDIMM_STATE_FLAGS_NOT_MAPPED);
      }
      break;
    }
    mask <<= 1;
  }

  if (NvDimmStateFlags == NULL) {
    NVDIMM_DBG("DecodePcatMemoryModeCapabilities failed. Not enough resources!");
  }

  return NvDimmStateFlags;
}

/**
  PrintFitTable - prints the subtable of the parsed NFit table.

  @param[in] pTable pointer to the NFit subtable.
  @param[in] pointer to command's printer context.
**/
VOID
PrintFitTable(
  IN     SubTableHeader *pTable,
  IN     PRINT_CONTEXT *pPrinterCtx
  )
{
  SpaRangeTbl *pTableSpaRange = NULL;
  NvDimmRegionMappingStructure *pTableNvDimmRegion = NULL;
  InterleaveStruct *pTableInterleave = NULL;
  ControlRegionTbl *pTableControlRegion = NULL;
  BWRegionTbl *pTableBWRegion = NULL;
  FlushHintTbl *pTableFlushHint = NULL;
  PlatformCapabilitiesTbl *pTablePlatCap = NULL;
  UINT32 Index = 0;
  CHAR16 *pGuidStr = NULL;
  CHAR16 *LineOffset = NULL;
  CHAR16 *FlushHintAddress = NULL;
  CHAR16 *NvDimmStateFlags = NULL;

  if (pTable == NULL) {
    return;
  }

  PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
  TypeIndex++;
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, ACPI_TYPE_STR, FORMAT_HEX_NOWIDTH, pTable->Type);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32 L" bytes", pTable->Length);

  switch (pTable->Type) {
  case NVDIMM_SPA_RANGE_TYPE:
    pTableSpaRange = (SpaRangeTbl *)pTable;
    pGuidStr = GuidToStr(&pTableSpaRange->AddressRangeTypeGuid);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"SpaRange");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"AddressRangeType", FORMAT_STR, pGuidStr);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SpaRangeDescriptionTableIndex", FORMAT_HEX_NOWIDTH, pTableSpaRange->SpaRangeDescriptionTableIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_HEX_NOWIDTH, pTableSpaRange->Flags);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ProximityDomain", FORMAT_HEX_NOWIDTH, pTableSpaRange->ProximityDomain);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SystemPhysicalAddressRangeBase", FORMAT_UINT64_HEX_NOWIDTH, pTableSpaRange->SystemPhysicalAddressRangeBase);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SystemPhysicalAddressRangeLength", FORMAT_UINT64_HEX_NOWIDTH, pTableSpaRange->SystemPhysicalAddressRangeLength);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemoryMappingAttribute", FORMAT_UINT64_HEX_NOWIDTH L"\n", pTableSpaRange->AddressRangeMemoryMappingAttribute);
    FREE_POOL_SAFE(pGuidStr);
    break;
  case NVDIMM_NVDIMM_REGION_TYPE:
    pTableNvDimmRegion = (NvDimmRegionMappingStructure *)pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"NvDimmRegion");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle", FORMAT_HEX, pTableNvDimmRegion->DeviceHandle.AsUint32);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.DimmNumber", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.DimmNumber);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.MemChannel", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.MemChannel);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.MemControllerId", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.MemControllerId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.SocketId", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.SocketId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.NodeControllerId", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.NodeControllerId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvDimmPhysicalId", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->NvDimmPhysicalId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvDimmRegionalId", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->NvDimmRegionalId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SpaRangeDescriptionTableIndex", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->SpaRangeDescriptionTableIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvdimmControlRegionDescriptorTableIndex", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->NvdimmControlRegionDescriptorTableIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvDimmRegionSize", FORMAT_UINT64_HEX_NOWIDTH, pTableNvDimmRegion->NvDimmRegionSize);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"RegionOffset", FORMAT_UINT64_HEX_NOWIDTH, pTableNvDimmRegion->RegionOffset);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvDimmPhysicalAddressRegionBase", FORMAT_UINT64_HEX_NOWIDTH, pTableNvDimmRegion->NvDimmPhysicalAddressRegionBase);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"InterleaveStructureIndex", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->InterleaveStructureIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"InterleaveWays", FORMAT_HEX_NOWIDTH, pTableNvDimmRegion->InterleaveWays);
    NvDimmStateFlags = DecodeNfitNvDimmStateFlags(pTableNvDimmRegion->NvDimmStateFlags);
    if (NvDimmStateFlags != NULL) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvDimmStateFlags", FORMAT_HEX FORMAT_STR_NL,
        pTableNvDimmRegion->NvDimmStateFlags, NvDimmStateFlags);
    } else {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NvDimmStateFlags", FORMAT_HEX L"\n",
        pTableNvDimmRegion->NvDimmStateFlags);
    }
    FREE_POOL_SAFE(NvDimmStateFlags);
    break;
  case NVDIMM_INTERLEAVE_TYPE:
    pTableInterleave = (InterleaveStruct *)pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"Interleave");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"InterleaveStructureIndex", FORMAT_HEX_NOWIDTH, pTableInterleave->InterleaveStructureIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NumberOfLinesDescribed", FORMAT_HEX_NOWIDTH, pTableInterleave->NumberOfLinesDescribed);
    for (Index = 0; Index < pTableInterleave->NumberOfLinesDescribed; Index++) {
      LineOffset = CatSPrint(NULL, L"LineOffset %d", Index);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, LineOffset, FORMAT_HEX_NOWIDTH, pTableInterleave->LinesOffsets[Index]);
      FREE_POOL_SAFE(LineOffset);
    }
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"LineSize", FORMAT_HEX_NOWIDTH L"\n", pTableInterleave->LineSize);
    break;
  case NVDIMM_SMBIOS_MGMT_INFO_TYPE:
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"Smbios\n");
    break;
  case NVDIMM_CONTROL_REGION_TYPE:
    pTableControlRegion = (ControlRegionTbl *)pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"ControlRegion");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ControlRegionDescriptorTableIndex", FORMAT_HEX_NOWIDTH, pTableControlRegion->ControlRegionDescriptorTableIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"VendorId", FORMAT_HEX_NOWIDTH, pTableControlRegion->VendorId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DeviceId", FORMAT_HEX_NOWIDTH, pTableControlRegion->DeviceId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Rid", FORMAT_HEX_NOWIDTH, pTableControlRegion->Rid);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SubsystemVendorId", FORMAT_HEX_NOWIDTH, pTableControlRegion->SubsystemVendorId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SubsystemDeviceId", FORMAT_HEX_NOWIDTH, pTableControlRegion->SubsystemDeviceId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SubsystemRid", FORMAT_HEX_NOWIDTH, pTableControlRegion->SubsystemRid);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ValidFields", FORMAT_HEX_NOWIDTH, pTableControlRegion->ValidFields);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ManufacturingLocation", FORMAT_HEX_NOWIDTH, pTableControlRegion->ManufacturingLocation);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ManufacturingDate", FORMAT_HEX_NOWIDTH, pTableControlRegion->ManufacturingDate);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SerialNumber", FORMAT_HEX_NOWIDTH, pTableControlRegion->SerialNumber);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"RegionFormatInterfaceCode", FORMAT_HEX_NOWIDTH, pTableControlRegion->RegionFormatInterfaceCode);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NumberOfBlockControlWindows", FORMAT_HEX_NOWIDTH L"\n", pTableControlRegion->NumberOfBlockControlWindows);
    if (pTableControlRegion->NumberOfBlockControlWindows > 0) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SizeOfBlockControlWindow", FORMAT_UINT64_HEX_NOWIDTH, pTableControlRegion->SizeOfBlockControlWindow);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"CommandRegisterOffsetInBlockControlWindow", FORMAT_UINT64_HEX_NOWIDTH, pTableControlRegion->CommandRegisterOffsetInBlockControlWindow);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SizeOfCommandRegisterInBlockControlWindows", FORMAT_UINT64_HEX_NOWIDTH, pTableControlRegion->SizeOfCommandRegisterInBlockControlWindows);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"StatusRegisterOffsetInBlockControlWindow", FORMAT_UINT64_HEX_NOWIDTH, pTableControlRegion->StatusRegisterOffsetInBlockControlWindow);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SizeOfStatusRegisterInBlockControlWindows", FORMAT_UINT64_HEX_NOWIDTH, pTableControlRegion->SizeOfStatusRegisterInBlockControlWindows);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ControlRegionFlag", FORMAT_HEX_NOWIDTH L"\n", pTableControlRegion->ControlRegionFlag);
    }
    break;
  case NVDIMM_BW_DATA_WINDOW_REGION_TYPE:
    pTableBWRegion = (BWRegionTbl *)pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"BWRegion");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ControlRegionStructureIndex", FORMAT_HEX_NOWIDTH, pTableBWRegion->ControlRegionStructureIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NumberOfBlockDataWindows", FORMAT_HEX_NOWIDTH, pTableBWRegion->NumberOfBlockDataWindows);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"BlockDataWindowStartLogicalOffset", L"0x%lx", pTableBWRegion->BlockDataWindowStartLogicalOffset);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SizeOfBlockDataWindow", L"0x%lx", pTableBWRegion->SizeOfBlockDataWindow);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"AccessibleBlockCapacity", L"0x%lx", pTableBWRegion->AccessibleBlockCapacity);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"AccessibleBlockCapacityStartAddress", L"0x%lx\n", pTableBWRegion->AccessibleBlockCapacityStartAddress);
    break;
  case NVDIMM_FLUSH_HINT_TYPE:
    pTableFlushHint = (FlushHintTbl *)pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"FlushHint");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle", FORMAT_HEX, pTableFlushHint->DeviceHandle.AsUint32);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.DimmNumber", FORMAT_HEX_NOWIDTH, pTableFlushHint->DeviceHandle.NfitDeviceHandle.DimmNumber);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.MemChannel", FORMAT_HEX_NOWIDTH, pTableFlushHint->DeviceHandle.NfitDeviceHandle.MemChannel);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.MemControllerId", FORMAT_HEX_NOWIDTH, pTableFlushHint->DeviceHandle.NfitDeviceHandle.MemControllerId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.SocketId", FORMAT_HEX_NOWIDTH, pTableFlushHint->DeviceHandle.NfitDeviceHandle.SocketId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NfitDeviceHandle.NodeControllerId", FORMAT_HEX_NOWIDTH, pTableFlushHint->DeviceHandle.NfitDeviceHandle.NodeControllerId);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NumberOfFlushHintAddresses", FORMAT_HEX_NOWIDTH, pTableFlushHint->NumberOfFlushHintAddresses);
    for (Index = 0; Index < pTableFlushHint->NumberOfFlushHintAddresses; Index++) {
      FlushHintAddress = CatSPrint(NULL, L"FlushHintAddress %d", Index);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, FlushHintAddress, FORMAT_UINT64_HEX_NOWIDTH, pTableFlushHint->FlushHintAddress[Index]);
      FREE_POOL_SAFE(FlushHintAddress);
    }
    break;
  case NVDIMM_PLATFORM_CAPABILITIES_TYPE:
    pTablePlatCap = (PlatformCapabilitiesTbl *)pTable;
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, L"TypeEquals", L"PlatformCapabilities");
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"HighestValidCapability", L"0x%2.2x", pTablePlatCap->HighestValidCapability);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Capabilities", L"0x%8.8x", pTablePlatCap->Capabilities);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Capabilities.CPUCacheFlushToNVDIMM", FORMAT_INT32, (pTablePlatCap->Capabilities & CAPABILITY_CACHE_FLUSH) ? 1 : 0);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Capabilities.MemoryControllerFlushToNVDIMM", FORMAT_INT32, (pTablePlatCap->Capabilities & CAPABILITY_MEMORY_FLUSH) ? 1 : 0);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Capabilities.MemoryMirroring", FORMAT_INT32 L"\n", (pTablePlatCap->Capabilities & CAPABILITY_MEMORY_MIRROR) ? 1 : 0);
    break;
  default:
    break;
  }
}

/**
  PrintNFit - prints the header and all of the tables in the parsed NFit table.

  @param[in] pHeader pointer to the parsed NFit header.
  @param[in] pointer to command's printer context.
**/
VOID
PrintNFit(
  IN     ParsedFitHeader *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx
)
{
  UINT16 Index = 0;
  if (pHeader == NULL) {
    return;
  }

  PRINTER_BUILD_KEY_PATH(pPath, DS_ACPI_INDEX_PATH, AcpiIndex);
  AcpiIndex++;
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SYSTEM_TARGET_STR, L"NVDIMM Firmware Interface Table");
  PrintAcpiHeader(&pHeader->pFit->Header, pPrinterCtx);

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"BwRegionTablesNum", FORMAT_UINT32, pHeader->BWRegionTblesNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ControlRegionTablesNum", FORMAT_UINT32, pHeader->ControlRegionTblesNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"FlushHintTablesNum", FORMAT_UINT32, pHeader->FlushHintTblesNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveTablesNum", FORMAT_UINT32, pHeader->InterleaveTblesNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"NVDIMMRegionTablesNum", FORMAT_UINT32, pHeader->NvDimmRegionMappingStructuresNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"SmbiosTablesNum", FORMAT_UINT32, pHeader->SmbiosTblesNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"SpaRangeTablesNum", FORMAT_UINT32, pHeader->SpaRangeTblesNum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PlatformCapabilitiesTablesNum", FORMAT_UINT32 L"\n", pHeader->PlatformCapabilitiesTblesNum);

  for(Index = 0; Index < pHeader->BWRegionTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppBWRegionTbles[Index], pPrinterCtx);
  }

  for(Index = 0; Index < pHeader->ControlRegionTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppControlRegionTbles[Index], pPrinterCtx);
  }

  for(Index = 0; Index < pHeader->FlushHintTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppFlushHintTbles[Index], pPrinterCtx);
  }

  for(Index = 0; Index < pHeader->InterleaveTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppInterleaveTbles[Index], pPrinterCtx);
  }

  for(Index = 0; Index < pHeader->NvDimmRegionMappingStructuresNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppNvDimmRegionMappingStructures[Index], pPrinterCtx);
  }

  for(Index = 0; Index < pHeader->SmbiosTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppSmbiosTbles[Index], pPrinterCtx);
  }

  for(Index = 0; Index < pHeader->SpaRangeTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppSpaRangeTbles[Index], pPrinterCtx);
  }

  for (Index = 0; Index < pHeader->PlatformCapabilitiesTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppPlatformCapabilitiesTbles[Index], pPrinterCtx);
  }
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pTypePath);
}
/**
PrintPMTT - prints the header and all of the tables in the parsed PMTT table.

@param[in] pPcat pointer to the parsed PMTT.
@param[in] pointer to command's printer context.
**/
VOID
PrintPMTT(
  IN     TABLE_HEADER *pTable,
  IN     PRINT_CONTEXT *pPrinterCtx
)
{
  if (pTable == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  PRINTER_BUILD_KEY_PATH(pPath, DS_ACPI_INDEX_PATH, AcpiIndex);
  AcpiIndex++;
  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, SYSTEM_TARGET_STR, L"Platform Memory Topology Table");
  PrintAcpiHeader(pTable, pPrinterCtx);

  // Print PMTT 1.1 table if Rev is 1.1
  if (IS_ACPI_REV_MAJ_1_MIN_1(pTable->Revision)) {
    PrintPMTT2((VOID *)pTable, pPrinterCtx);
    return;
  }

  PMTT_TABLE *pPMTT = (PMTT_TABLE *)pTable;

  UINT64 PmttLen = pPMTT->Header.Length;
  UINT64 Offset = sizeof(pPMTT->Header) + sizeof(pPMTT->Reserved);
  while (Offset < PmttLen) {
    PMTT_COMMON_HEADER *pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
    if (pCommonHeader->Type == PMTT_TYPE_SOCKET) {
      PMTT_SOCKET *pSocket = (PMTT_SOCKET *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"Socket");
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SocketId", FORMAT_INT32, pSocket->SocketId);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved3", FORMAT_INT32, pSocket->Reserved3);
      Offset += sizeof(PMTT_SOCKET) + PMTT_COMMON_HDR_LEN;
    } else if (pCommonHeader->Type == PMTT_TYPE_iMC) {
      PMTT_iMC *piMC = (PMTT_iMC *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"iMC");
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ReadLatency", FORMAT_INT32, piMC->ReadLatency);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"WriteLatency", FORMAT_INT32, piMC->WriteLatency);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ReadBW", FORMAT_INT32, piMC->ReadBW);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"WriteBW", FORMAT_INT32, piMC->WriteBW);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"OptimalAccessUnit", FORMAT_INT32, piMC->OptimalAccessUnit);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"OptimalAccessAlignment", FORMAT_INT32, piMC->OptimalAccessAlignment);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved3", FORMAT_INT32, piMC->Reserved3);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"NoOfProximityDomains", FORMAT_INT32, piMC->NoOfProximityDomains);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ProximityDomainArray", FORMAT_INT32, piMC->ProximityDomainArray);
      Offset += sizeof(PMTT_iMC) + PMTT_COMMON_HDR_LEN;
    } else if (pCommonHeader->Type == PMTT_TYPE_MODULE) {
      PMTT_MODULE *pModule = (PMTT_MODULE *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"MODULE");
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"PhysicalComponentId", FORMAT_INT32, pModule->PhysicalComponentId);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved3", FORMAT_INT32, pModule->Reserved3);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SizeOfDimm", FORMAT_INT32, pModule->SizeOfDimm);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SmbiosHandle", FORMAT_INT32, pModule->SmbiosHandle);
      Offset += sizeof(PMTT_MODULE) + PMTT_COMMON_HDR_LEN;
    }
  }
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pTypePath);
}

/**
PrintPMTT2 - prints the header and all of the tables in the parsed PMTT 2.0 table.

@param[in] pPcat pointer to the parsed PMTT 2.0 table.
@param[in] pointer to command's printer context.
**/
VOID
PrintPMTT2(
  IN     VOID *pTable,
  IN     PRINT_CONTEXT *pPrinterCtx
)
{
  UINT8 ParentDeviceType = 0;
  CHAR16 *pGuidStr = NULL;
  PMTT_TABLE2 *pPMTT = (PMTT_TABLE2 *)pTable;

  if (pPMTT == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  UINT64 PmttLen = pPMTT->Header.Length;
  UINT64 Offset = sizeof(pPMTT->Header) + sizeof(pPMTT->NoOfMemoryDevices);
  while (Offset < PmttLen) {
    PMTT_COMMON_HEADER2 *pCommonHeader = (PMTT_COMMON_HEADER2 *)(((UINT8 *)pPMTT) + Offset);
    NVDIMM_DBG("Common table length: %d, mem devices: %d, Type: %d",
      pCommonHeader->Length, pCommonHeader->NoOfMemoryDevices, pCommonHeader->Type);
    if (pCommonHeader->Type == PMTT_TYPE_SOCKET) {
      ParentDeviceType = PMTT_TYPE_SOCKET;
      PMTT_SOCKET2 *pSocket = (PMTT_SOCKET2 *)(((UINT8 *)pPMTT) + Offset);
      NVDIMM_DBG("Socket ID: %d, no of sockets: %d", pSocket->SocketId);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"Socket");
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SocketId", FORMAT_INT32, pSocket->SocketId);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved3", FORMAT_INT32, pSocket->Reserved3);
      Offset += sizeof(PMTT_SOCKET2);
    }
    else if (pCommonHeader->Type == PMTT_TYPE_iMC) {
      ParentDeviceType = PMTT_TYPE_iMC;
      PMTT_iMC2 *piMC = (PMTT_iMC2 *)(((UINT8 *)pPMTT) + Offset);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"iMC");
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"MemControllerId", FORMAT_INT32, piMC->MemControllerID);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved3", FORMAT_INT32, piMC->Reserved3);
      Offset += sizeof(PMTT_iMC2);
    }
    else if (pCommonHeader->Type == PMTT_TYPE_VENDOR_SPECIFIC){
      PMTT_VENDOR_SPECIFIC2 *pVendorDevice = (PMTT_VENDOR_SPECIFIC2 *)(((UINT8 *)pPMTT) + Offset);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      if (ParentDeviceType == PMTT_TYPE_SOCKET) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"Die");
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"DieId", FORMAT_INT32, pVendorDevice->DeviceID);
      }
      else if (ParentDeviceType == PMTT_TYPE_iMC) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"Channel");
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"ChannelId", FORMAT_INT32, pVendorDevice->DeviceID);
        ParentDeviceType = PMTT_TYPE_VENDOR_SPECIFIC;
      }
      else if ((ParentDeviceType == PMTT_TYPE_VENDOR_SPECIFIC)) {
        PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"Slot");
        PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SlotId", FORMAT_INT32, pVendorDevice->DeviceID);
      }
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      pGuidStr = GuidToStr(&pVendorDevice->TypeUUID);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"TypeUUID", FORMAT_STR, pGuidStr);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved3", FORMAT_INT32, pVendorDevice->Reserved3);
      FREE_POOL_SAFE(pGuidStr);
      Offset += sizeof(PMTT_VENDOR_SPECIFIC2);
    }
    else if (pCommonHeader->Type == PMTT_TYPE_MODULE) {
      PMTT_MODULE2 *pModule = (PMTT_MODULE2 *)(((UINT8 *)pPMTT) + Offset);
      PRINTER_BUILD_KEY_PATH(pTypePath, DS_ACPITYPE_INDEX_PATH, AcpiIndex - 1, TypeIndex);
      TypeIndex++;
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pTypePath, ACPI_TYPE_STR, L"MODULE");
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Type", FORMAT_INT32, pCommonHeader->Type);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved1", FORMAT_INT32, pCommonHeader->Reserved1);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Length", FORMAT_INT32, pCommonHeader->Length);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Flags", FORMAT_INT32, pCommonHeader->Flags);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"Reserved2", FORMAT_INT32, pCommonHeader->Reserved2);
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pTypePath, L"SmbiosHandle", FORMAT_INT32, pModule->SmbiosHandle);
      Offset += sizeof(PMTT_MODULE2);
    }
  }
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pTypePath);
}
