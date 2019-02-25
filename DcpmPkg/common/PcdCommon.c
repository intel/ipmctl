/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <PcdCommon.h>
#include <Convert.h>

/**
  Print Platform Config Data table header

  @param[in] pHeader table header
**/
VOID
PrintPcdTableHeader(
  IN     TABLE_HEADER *pHeader
  )
{
  Print(L"Signature                    : %c%c%c%c\n",
    pHeader->Signature & 0xFF,
    (pHeader->Signature >> 8) & 0xFF,
    (pHeader->Signature >> 16) & 0xFF,
    (pHeader->Signature >> 24) & 0xFF);
  Print(L"Length                       : 0x%x\n", pHeader->Length);
  Print(L"Revision                     : 0x%x\n", pHeader->Revision);
  Print(L"Checksum                     : 0x%x\n", pHeader->Checksum);
  Print(L"OemId[0..5]                  : %c%c%c%c%c%c\n",
    pHeader->OemId[0],
    pHeader->OemId[1],
    pHeader->OemId[2],
    pHeader->OemId[3],
    pHeader->OemId[4],
    pHeader->OemId[5]);
  Print(L"OemTableId                   : %c%c%c%c%c%c\n",
    ((UINT8 *)&pHeader->OemTableId)[0],
    ((UINT8 *)&pHeader->OemTableId)[1],
    ((UINT8 *)&pHeader->OemTableId)[2],
    ((UINT8 *)&pHeader->OemTableId)[3],
    ((UINT8 *)&pHeader->OemTableId)[4],
    ((UINT8 *)&pHeader->OemTableId)[5]);
  Print(L"OemRevision                  : 0x%x\n", pHeader->OemRevision);
  Print(L"CreatorId                    : %c%c%c%c\n",
    ((UINT8 *)&pHeader->CreatorId)[0],
    ((UINT8 *)&pHeader->CreatorId)[1],
    ((UINT8 *)&pHeader->CreatorId)[2],
    ((UINT8 *)&pHeader->CreatorId)[3]);
  Print(L"CreatorRevision              : 0x%x\n", pHeader->CreatorRevision);
}

/**
  Print Platform Config Data PCAT table header

  @param[in] pHeader PCAT table header
**/
VOID
PrintPcdPcatTableHeader(
  IN     PCAT_TABLE_HEADER *pHeader
  )
{
  Print(L"Type                         : 0x%x\n", pHeader->Type);
  Print(L"Length                       : 0x%x\n", pHeader->Length);
}

/**
  Print Platform Config Data Partition Size Change table

  @param[in] pPartitionSizeChange Partition Size Change table
**/
VOID
PrintPcdPartitionSizeChange(
  IN     NVDIMM_PARTITION_SIZE_CHANGE *pPartitionSizeChange
  )
{
  Print(L"Platform Config Data Partition Size Change table\n");
  PrintPcdPcatTableHeader(&pPartitionSizeChange->Header);
  Print(L"PartitionSizeChangeStatus    : 0x%x\n", pPartitionSizeChange->PartitionSizeChangeStatus);
  Print(L"PartitionSize                : 0x%llx\n", pPartitionSizeChange->PmPartitionSize);
  Print(L"\n");
}

/**
  Print  Platform Config Data Identification Information table

  @param[in] pIdentificationInfo Identification Information table
  @param[in] PcdConfigTableRevision Revision of the PCD Config tables
**/
VOID
PrintPcdIdentificationInformation(
  IN     NVDIMM_IDENTIFICATION_INFORMATION *pIdentificationInfo,
  IN     UINT8 PcdConfigTableRevision
  )
{
  CHAR16 PartNumber[PART_NUMBER_SIZE + 1];
  CHAR16 *pTmpDimmUid = NULL;

  ZeroMem(PartNumber, sizeof(PartNumber));
  AsciiStrToUnicodeStrS(pIdentificationInfo->DimmIdentification.Version1.DimmPartNumber, PartNumber, PART_NUMBER_SIZE + 1);

  Print(L"Platform Config Data Identification Information table\n");

  if (PcdConfigTableRevision == NVDIMM_CONFIGURATION_TABLES_REVISION_1) {
    Print(L"DimmManufacturerId           : 0x%x\n", EndianSwapUint16(pIdentificationInfo->DimmIdentification.Version1.DimmManufacturerId));
    Print(L"DimmSerialNumber             : 0x%x\n", EndianSwapUint32(pIdentificationInfo->DimmIdentification.Version1.DimmSerialNumber));
    Print(L"DimmPartNumber               : " FORMAT_STR_NL, PartNumber);
  } else {
    pTmpDimmUid = CatSPrint(NULL, L"%04x-%02x-%04x-%08x", EndianSwapUint16(pIdentificationInfo->DimmIdentification.Version2.Uid.ManufacturerId),
      pIdentificationInfo->DimmIdentification.Version2.Uid.ManufacturingLocation,
      EndianSwapUint16(pIdentificationInfo->DimmIdentification.Version2.Uid.ManufacturingDate),
      EndianSwapUint32(pIdentificationInfo->DimmIdentification.Version2.Uid.SerialNumber));
    Print(L"DimmUniqueIdentifer          : " FORMAT_STR_NL, pTmpDimmUid);
  }
  Print(L"PartitionOffset              : 0x%llx\n", pIdentificationInfo->PartitionOffset);
  Print(L"PmPartitionSize              : 0x%llx\n", pIdentificationInfo->PmPartitionSize);

  if (pTmpDimmUid != NULL) {
    FREE_POOL_SAFE(pTmpDimmUid);
  }
}

/**
  Print Platform Config Data Interleave Information table and its extension tables

  @param[in] pInterleaveInfo Interleave Information table
  @param[in] PcdConfigTableRevision Revision of the PCD Config tables
**/
VOID
PrintPcdInterleaveInformation(
  IN     NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo,
  IN     UINT8 PcdConfigTableRevision
  )
{
  UINT32 Index = 0;
  NVDIMM_IDENTIFICATION_INFORMATION *pCurrentIdentInfo = NULL;

  Print(L"Platform Config Data Interleave Information table\n");
  PrintPcdPcatTableHeader(&pInterleaveInfo->Header);
  Print(L"InterleaveSetIndex           : 0x%x\n", pInterleaveInfo->InterleaveSetIndex);
  Print(L"NumOfDimmsInInterleaveSet    : 0x%x\n", pInterleaveInfo->NumOfDimmsInInterleaveSet);
  Print(L"InterleaveMemoryType         : 0x%x\n", pInterleaveInfo->InterleaveMemoryType);
  Print(L"InterleaveFormatChannel      : 0x%x\n", pInterleaveInfo->InterleaveFormatChannel);
  Print(L"InterleaveFormatImc          : 0x%x\n", pInterleaveInfo->InterleaveFormatImc);
  Print(L"InterleaveFormatWays         : 0x%x\n", pInterleaveInfo->InterleaveFormatWays);
  Print(L"MirrorEnable                 : 0x%x\n", pInterleaveInfo->MirrorEnable);
  Print(L"InterleaveChangeStatus       : 0x%x\n", pInterleaveInfo->InterleaveChangeStatus);
  Print(L"\n");

  pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *) &pInterleaveInfo->pIdentificationInfoList;

  for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
    PrintPcdIdentificationInformation(pCurrentIdentInfo, PcdConfigTableRevision);
    Print(L"\n");

    pCurrentIdentInfo++;
  }
}

VOID
PrintPcdConfigManagementAttributesInformation(
  IN    CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo
  )
{
  INTEL_DIMM_CONFIG *pIntelDIMMConfig = NULL;
  EFI_GUID IntelDimmConfigVariableGuid = INTEL_DIMM_CONFIG_VARIABLE_GUID;
  CHAR16 *pGuidStr = NULL;

  pGuidStr = GuidToStr(&pConfigManagementAttributesInfo->Guid);

  Print(L"Platform Config Data Config Management Attributes Extension\n");
  PrintPcdPcatTableHeader(&pConfigManagementAttributesInfo->Header);
  Print(L"VendorID                     : 0x%x\n", pConfigManagementAttributesInfo->VendorId);
  Print(L"GUID                         : " FORMAT_STR_NL, pGuidStr);

  if (CompareGuid(&pConfigManagementAttributesInfo->Guid, &IntelDimmConfigVariableGuid)) {
    pIntelDIMMConfig = (INTEL_DIMM_CONFIG *) pConfigManagementAttributesInfo->pGuidData;
    Print(L"Revision                     : %d\n", pIntelDIMMConfig->Revision);
    Print(L"ProvisionCapacityMode        : %d\n", pIntelDIMMConfig->ProvisionCapacityMode);
    Print(L"MemorySize                   : %d\n", pIntelDIMMConfig->MemorySize);
    Print(L"PMType                       : %d\n", pIntelDIMMConfig->PMType);
    Print(L"ProvisionNamespaceMode       : %d\n", pIntelDIMMConfig->ProvisionNamespaceMode);
    Print(L"NamespaceFlags               : %d\n", pIntelDIMMConfig->NamespaceFlags);
    Print(L"NamespaceLabelVersion        : %d\n", pIntelDIMMConfig->NamespaceLabelVersion);
  }

  Print(L"\n");

  FREE_POOL_SAFE(pGuidStr);
}

/**
  Print Platform Config Data Current Config table and its PCAT tables

  @param[in] pCurrentConfig Current Config table
**/
VOID
PrintPcdCurrentConfig(
  IN     NVDIMM_CURRENT_CONFIG *pCurrentConfig
  )
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;

  if ((pCurrentConfig->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
      (pCurrentConfig->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    Print(L"Error: Invalid revision value %d for PCD current config table.", pCurrentConfig->Header.Revision);
    return;
  }

  Print(L"Platform Config Data Current Config table\n");
  PrintPcdTableHeader(&pCurrentConfig->Header);
  Print(L"ConfigStatus                 : 0x%x\n", pCurrentConfig->ConfigStatus);
  Print(L"VolatileMemSizeIntoSpa       : 0x%llx\n", pCurrentConfig->VolatileMemSizeIntoSpa);
  Print(L"PersistentMemSizeIntoSpa     : 0x%llx\n", pCurrentConfig->PersistentMemSizeIntoSpa);
  Print(L"\n");

  /**
    Check if there is at least one PCAT table
  **/
  if (pCurrentConfig->Header.Length <= sizeof(*pCurrentConfig)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *) &pCurrentConfig->pPcatTables;
  SizeOfPcatTables = pCurrentConfig->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pCurrentConfig);

  /**
    Example of the use of the while loop condition
    PCAT table #1   offset:  0   size: 10
    PCAT table #2   offset: 10   size:  5
    Size of PCAT tables: 15 (10 + 5)

    Iteration #1:   offset: 0
    Iteration #2:   offset: 10
    Iteration #3:   offset: 15   stop the loop: offset isn't less than size
  **/
  while ((UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)&pCurrentConfig->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pCurPcatTable;

      PrintPcdPartitionSizeChange(pPartSizeChange);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurPcatTable;

      PrintPcdInterleaveInformation(pInterleaveInfo, pCurrentConfig->Header.Revision);

      pCurPcatTable =  GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
      pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pCurPcatTable;

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
    } else {
      Print(L"Error: wrong PCAT table type\n");
      break;
    }
  }
}

/**
  Print Platform Config Data Config Input table and its PCAT tables

  @param[in] pConfigInput Config Input table
**/
VOID
PrintPcdConfInput(
  IN     NVDIMM_PLATFORM_CONFIG_INPUT *pConfigInput
  )
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;

  if ((pConfigInput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
      (pConfigInput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    Print(L"Invalid revision value %d for PCD Config Input table.", pConfigInput->Header.Revision);
    return;
  }

  Print(L"Platform Config Data Conf Input table\n");
  PrintPcdTableHeader(&pConfigInput->Header);
  Print(L"SequenceNumber               : 0x%x\n", pConfigInput->SequenceNumber);
  Print(L"\n");

  /**
    Check if there is at least one PCAT table
  **/
  if (pConfigInput->Header.Length <= sizeof(*pConfigInput)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *) &pConfigInput->pPcatTables;
  SizeOfPcatTables = pConfigInput->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pConfigInput);

  /**
    Example of the use of the while loop condition
    PCAT table #1   offset:  0   size: 10
    PCAT table #2   offset: 10   size:  5
    Size of PCAT tables: 15 (10 + 5)

    Iteration #1:   offset: 0
    Iteration #2:   offset: 10
    Iteration #3:   offset: 15   stop the loop: offset isn't less than size
  **/
  while ((UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)&pConfigInput->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pCurPcatTable;

      PrintPcdPartitionSizeChange(pPartSizeChange);

      pCurPcatTable =  GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurPcatTable;

      PrintPcdInterleaveInformation(pInterleaveInfo, pConfigInput->Header.Revision);

      pCurPcatTable =  GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
      pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pCurPcatTable;

      PrintPcdConfigManagementAttributesInformation(pConfigManagementAttributesInfo);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
    } else {
      Print(L"Error: wrong PCAT table type\n");
      break;
    }
  }
}

/**
  Print Platform Config Data Config Output table and its PCAT tables

  @param[in] pConfigOutput Config Output table
**/
VOID
PrintPcdConfOutput(
  IN     NVDIMM_PLATFORM_CONFIG_OUTPUT *pConfigOutput
  )
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;

  if ((pConfigOutput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
      (pConfigOutput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    Print(L"Error: Invalid revision value %d for PCD Config Output table.", pConfigOutput->Header.Revision);
    return;
  }

  Print(L"Platform Config Data Conf Output table\n");
  PrintPcdTableHeader(&pConfigOutput->Header);
  Print(L"SequenceNumber               : 0x%x\n", pConfigOutput->SequenceNumber);
  Print(L"ValidationStatus             : 0x%x\n", pConfigOutput->ValidationStatus);
  Print(L"\n");

  /** Check if there is at least one PCAT table **/
  if (pConfigOutput->Header.Length <= sizeof(*pConfigOutput)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *) &pConfigOutput->pPcatTables;
  SizeOfPcatTables = pConfigOutput->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pConfigOutput);

  /**
    Example of the use of the while loop condition
    PCAT table #1   offset:  0   size: 10
    PCAT table #2   offset: 10   size:  5
    Size of PCAT tables: 15 (10 + 5)

    Iteration #1:   offset: 0
    Iteration #2:   offset: 10
    Iteration #3:   offset: 15   stop the loop: offset isn't less than size
  **/
  while ((UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)&pConfigOutput->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pCurPcatTable;

      PrintPcdPartitionSizeChange(pPartSizeChange);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurPcatTable;

      PrintPcdInterleaveInformation(pInterleaveInfo, pConfigOutput->Header.Revision);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
    } else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
      pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pCurPcatTable;

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
    } else {
      Print(L"Error: wrong PCAT table type\n");
      break;
    }
  }
}

/**
   Print Platform Config Data Configuration Header table and all subtables

   @param[in] pConfHeader Configuration Header table
**/
VOID
PrintPcdConfigurationHeader(
  IN     NVDIMM_CONFIGURATION_HEADER *pConfHeader
  )
{
  Print(L"Platform Config Data Configuration Header table\n");
  PrintPcdTableHeader(&pConfHeader->Header);
  Print(L"CurrentConfDataSize          : 0x%x\n", pConfHeader->CurrentConfDataSize);
  Print(L"CurrentConfStartOffset       : 0x%x\n", pConfHeader->CurrentConfStartOffset);
  Print(L"ConfInputDataSize            : 0x%x\n", pConfHeader->ConfInputDataSize);
  Print(L"ConfInputDataOffset          : 0x%x\n", pConfHeader->ConfInputStartOffset);
  Print(L"ConfOutputDataSize           : 0x%x\n", pConfHeader->ConfOutputDataSize);
  Print(L"ConfOutputDataOffset         : 0x%x\n", pConfHeader->ConfOutputStartOffset);
  Print(L"\n");

  if (pConfHeader->CurrentConfStartOffset != 0 && pConfHeader->CurrentConfDataSize != 0) {
    PrintPcdCurrentConfig(GET_NVDIMM_CURRENT_CONFIG(pConfHeader));
  }
  if (pConfHeader->ConfInputStartOffset != 0 && pConfHeader->ConfInputDataSize != 0) {
    PrintPcdConfInput(GET_NVDIMM_PLATFORM_CONFIG_INPUT(pConfHeader));
  }
  if (pConfHeader->ConfOutputStartOffset != 0 && pConfHeader->ConfOutputDataSize != 0) {
    PrintPcdConfOutput(GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(pConfHeader));
  }
}

/**
  Free dimm PCD info array

  @param[in, out] pDimmPcdInfo Pointer to output array of PCDs
  @param[in, out] DimmPcdInfoCount Number of items in Dimm PCD Info
**/
VOID
FreeDimmPcdInfoArray(
  IN OUT DIMM_PCD_INFO *pDimmPcdInfo,
  IN OUT UINT32 DimmPcdInfoCount
  )
{
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pDimmPcdInfo == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < DimmPcdInfoCount; Index++) {
    FREE_POOL_SAFE(pDimmPcdInfo[Index].pConfHeader);
    FREE_POOL_SAFE(pDimmPcdInfo[Index].pLabelStorageArea);
  }

  FREE_POOL_SAFE(pDimmPcdInfo);

Finish:
  NVDIMM_EXIT();
}
