/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Uefi.h>
#include <Debug.h>
#include <NvmTables.h>

/**
  PrintPcatHeader - prints the header of the parsed NFit table.

  @param[in] pPcat pointer to the parsed PCAT header.
**/
VOID
PrintAcpiHeader(
  IN     TABLE_HEADER *pHeader
  )
{
  if (pHeader == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  Print(L"Signature: %c%c%c%c\n"
    L"Length: 0x%x\n"
    L"Revision: 0x%x\n"
    L"Checksum: 0x%x\n"
    L"OEMID: %c%c%c%c%c%c\n",
    ((UINT8 *)&pHeader->Signature)[0],
    ((UINT8 *)&pHeader->Signature)[1],
    ((UINT8 *)&pHeader->Signature)[2],
    ((UINT8 *)&pHeader->Signature)[3],
    pHeader->Length,
    pHeader->Revision,
    pHeader->Checksum,
    pHeader->OemId[0],
    pHeader->OemId[1],
    pHeader->OemId[2],
    pHeader->OemId[3],
    pHeader->OemId[4],
    pHeader->OemId[5]);
  Print(L"OEMTableID: %c%c%c%c%c%c%c%c\n"
    L"OEMRevision: 0x%x\n",
    ((UINT8 *)&pHeader->OemTableId)[0],
    ((UINT8 *)&pHeader->OemTableId)[1],
    ((UINT8 *)&pHeader->OemTableId)[2],
    ((UINT8 *)&pHeader->OemTableId)[3],
    ((UINT8 *)&pHeader->OemTableId)[4],
    ((UINT8 *)&pHeader->OemTableId)[5],
    ((UINT8 *)&pHeader->OemTableId)[6],
    ((UINT8 *)&pHeader->OemTableId)[7],
    pHeader->OemRevision);
  Print(L"CreatorID: %c%c%c%c\n",
    ((UINT8 *)&pHeader->CreatorId)[0],
    ((UINT8 *)&pHeader->CreatorId)[1],
    ((UINT8 *)&pHeader->CreatorId)[2],
    ((UINT8 *)&pHeader->CreatorId)[3]
    );
  Print(L"CreatorRevision: 0x%x\n", pHeader->CreatorRevision);
}

/**
  PrintPcatTable - prints the subtable of the parsed PCAT table.

  @param[in] pTable pointer to the PCAT subtable.
**/
VOID
PrintPcatTable(
  IN     PCAT_TABLE_HEADER *pTable
  )
{
  UINT16 Index = 0;

  if (pTable == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  PLATFORM_CAPABILITY_INFO *pPlatformCapabilityInfoTable = NULL;
  MEMORY_INTERLEAVE_CAPABILITY_INFO *pMemoryInterleaveCapabilityInfoTable = NULL;
  RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE *pReconfInputValidationInterfaceTable = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfoTable = NULL;
  SOCKET_SKU_INFO_TABLE *pSocketSkuInfoTable = NULL;

  Print(L"Type: 0x%x\n"
      L"Length: 0x%x\n",
      pTable->Type,
      pTable->Length);

  switch (pTable->Type) {
  case PCAT_TYPE_PLATFORM_CAPABILITY_INFO_TABLE:
    pPlatformCapabilityInfoTable = (PLATFORM_CAPABILITY_INFO *) pTable;
    Print(L"TypeEquals: PlatformCapabilityInfoTable\n"
        L"IntelNVDIMMManagementSWConfigInputSupport: 0x%x\n"
        L"MemoryModeCapabilities: 0x%x\n"
        L"CurrentMemoryMode: 0x%x\n"
        L"PersistentMemoryRASCapability: 0x%x\n",
        pPlatformCapabilityInfoTable->MgmtSwConfigInputSupport,
        pPlatformCapabilityInfoTable->MemoryModeCapabilities,
        pPlatformCapabilityInfoTable->CurrentMemoryMode,
        pPlatformCapabilityInfoTable->PersistentMemoryRasCapability
        );
    break;
  case PCAT_TYPE_INTERLEAVE_CAPABILITY_INFO_TABLE:
    pMemoryInterleaveCapabilityInfoTable = (MEMORY_INTERLEAVE_CAPABILITY_INFO *) pTable;
    Print(L"TypeEquals: MemoryInterleaveCapabilityTable\n"
        L"MemoryMode: 0x%x\n"
        L"InterleaveAlignmentSize: 0x%x\n"
        L"NumberOfInterleaveFormatsSupported: 0x%x\n",
        pMemoryInterleaveCapabilityInfoTable->MemoryMode,
        pMemoryInterleaveCapabilityInfoTable->InterleaveAlignmentSize,
        pMemoryInterleaveCapabilityInfoTable->NumOfFormatsSupported
        );
    for (Index = 0; Index < pMemoryInterleaveCapabilityInfoTable->NumOfFormatsSupported; Index++) {
      Print(L"InterleaveFormatSupported(%d): 0x%x\n", Index,
           pMemoryInterleaveCapabilityInfoTable->InterleaveFormatList[Index]);
    }
    break;
  case PCAT_TYPE_RUNTIME_INTERFACE_TABLE:
    pReconfInputValidationInterfaceTable = (RECONFIGURATION_INPUT_VALIDATION_INTERFACE_TABLE *) pTable;
    Print(L"TypeEquals: Re-configurationInputValidationInterfaceTable\n"
        L"AddressSpaceID: 0x%x\n"
        L"BitWidth: 0x%x\n"
        L"BitOffset: 0x%x\n"
        L"AccessSize: 0x%x\n"
        L"Address: 0x%llx\n"
        L"--VerifyTriggerOperation--\n"
        L"OperationType: 0x%x\n"
        L"Value: 0x%x\n"
        L"Mask: 0x%x\n"
        L"--VerifyStatusOperation--\n"
        L"GASStructure: " FORMAT_STR L"\n"
        L"OperationType: 0x%x\n"
        L"Mask: 0x%x\n",
        pReconfInputValidationInterfaceTable->AddressSpaceId,
        pReconfInputValidationInterfaceTable->BitWidth,
        pReconfInputValidationInterfaceTable->BitOffset,
        pReconfInputValidationInterfaceTable->AccessSize,
        pReconfInputValidationInterfaceTable->Address,
        pReconfInputValidationInterfaceTable->TriggerOperationType,
        pReconfInputValidationInterfaceTable->TriggerValue,
        pReconfInputValidationInterfaceTable->TriggerMask,
        pReconfInputValidationInterfaceTable->GasStructure[0] == 0 ? L"System Memory" : L"Unknown",
        pReconfInputValidationInterfaceTable->StatusOperationType,
        pReconfInputValidationInterfaceTable->StatusMask
        );
    break;
  case PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE:
    pConfigManagementAttributesInfoTable = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pTable;
    Print(L"ConfigurationManagementAttributesExtensionTable\n"
        L"VendorID: 0x%x\n"
        L"GUID: %g\n"
        L"GUIDDataPointer: %p\n",
        pConfigManagementAttributesInfoTable->VendorId,
        pConfigManagementAttributesInfoTable->Guid,
        pConfigManagementAttributesInfoTable->pGuidData
        );
    break;
  case PCAT_TYPE_SOCKET_SKU_INFO_TABLE:
    pSocketSkuInfoTable = (SOCKET_SKU_INFO_TABLE *) pTable;
    Print(L"SocketSkuInfoTable\n"
        L"SocketID: 0x%x\n"
        L"MappedMemorySizeLimit: %ld\n"
        L"TotalMemorySizeMappedToSpa: %ld\n"
        L"CachingMemorySize: %ld\n",
        pSocketSkuInfoTable->SocketId,
        pSocketSkuInfoTable->MappedMemorySizeLimit,
        pSocketSkuInfoTable->TotalMemorySizeMappedToSpa,
        pSocketSkuInfoTable->CachingMemorySize
        );
    break;
  default:
    break;
  }
}

/**
  PrintPcat - prints the header and all of the tables in the parsed PCAT table.

  @param[in] pPcat pointer to the parsed PCAT.
**/
VOID
PrintPcat(
  IN     ParsedPcatHeader *pPcat
  )
{
  UINT32 Index = 0;

  if (pPcat == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }

  PrintAcpiHeader(&pPcat->pPlatformConfigAttr->Header);
  Print(L"\n");

  for (Index = 0; Index < pPcat->PlatformCapabilityInfoNum; Index++) {
    if (pPcat->ppPlatformCapabilityInfo[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppPlatformCapabilityInfo[Index]);
    Print(L"\n");
  }

  for (Index = 0; Index < pPcat->MemoryInterleaveCapabilityInfoNum; Index++) {
    if (pPcat->ppMemoryInterleaveCapabilityInfo[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppMemoryInterleaveCapabilityInfo[Index]);
    Print(L"\n");
  }

  for (Index = 0; Index < pPcat->RuntimeInterfaceValConfInputNum; Index++) {
    if (pPcat->ppRuntimeInterfaceValConfInput[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppRuntimeInterfaceValConfInput[Index]);
    Print(L"\n");
  }

  for (Index = 0; Index < pPcat->ConfigManagementAttributesInfoNum; Index++) {
    if (pPcat->ppConfigManagementAttributesInfo[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppConfigManagementAttributesInfo[Index]);
    Print(L"\n");
  }

  for (Index = 0; Index < pPcat->SocketSkuInfoNum; Index++) {
    if (pPcat->ppSocketSkuInfoTable[Index] == NULL) {
      return;
    }
    PrintPcatTable((PCAT_TABLE_HEADER *) pPcat->ppSocketSkuInfoTable[Index]);
    Print(L"\n");
  }

}

/**
  PrintFitTable - prints the subtable of the parsed NFit table.

  @param[in] pTable pointer to the NFit subtable.
**/
VOID
PrintFitTable(
  IN     SubTableHeader *pTable
  )
{
  SpaRangeTbl *pTableSpaRange = NULL;
  NvDimmRegionTbl *pTableNvDimmRegion = NULL;
  InterleaveStruct *pTableInterleave = NULL;
  ControlRegionTbl *pTableControlRegion = NULL;
  BWRegionTbl *pTableBWRegion = NULL;
  FlushHintTbl *pTableFlushHint = NULL;
  UINT32 Index = 0;

  if (pTable == NULL) {
    return;
  }

  Print(L"Type: 0x%x\n"
      L"Length: 0x%x\n",
      pTable->Type,
      pTable->Length);

  switch (pTable->Type) {
  case NVDIMM_SPA_RANGE_TYPE:
    pTableSpaRange = (SpaRangeTbl *)pTable;
    Print(L"TypeEquals: SpaRange\n"
        L"AddressRangeType: %g\n"
        L"SpaRangeDescriptionTableIndex: 0x%x\n"
        L"Flags: 0x%x\n"
        L"ProximityDomain: 0x%x\n"
        L"SystemPhysicalAddressRangeBase: 0x%llx\n"
        L"SystemPhysicalAddressRangeLength: 0x%llx\n"
        L"MemoryMappingAttribute: 0x%llx\n",
        pTableSpaRange->AddressRangeTypeGuid,
        pTableSpaRange->SpaRangeDescriptionTableIndex,
        pTableSpaRange->Flags,
        pTableSpaRange->ProximityDomain,
        pTableSpaRange->SystemPhysicalAddressRangeBase,
        pTableSpaRange->SystemPhysicalAddressRangeLength,
        pTableSpaRange->AddressRangeMemoryMappingAttribute
        );
    break;
  case NVDIMM_NVDIMM_REGION_TYPE:
    pTableNvDimmRegion = (NvDimmRegionTbl *)pTable;
    Print(L"TypeEquals: NvDimmRegion\n"
        L"DeviceHandle: 0x%x\n"
        L"NfitDeviceHandle.DimmNumber: 0x%x\n"
        L"NfitDeviceHandle.MemChannel: 0x%x\n"
        L"NfitDeviceHandle.MemControllerId: 0x%x\n"
        L"NfitDeviceHandle.SocketId: 0x%x\n"
        L"NfitDeviceHandle.NodeControllerId: 0x%x\n"
        L"NvDimmPhysicalId: 0x%x\n"
        L"NvDimmRegionalId: 0x%x\n"
        L"SpaRangeDescriptionTableIndex: 0x%x\n"
        L"NvdimmControlRegionDescriptorTableIndex: 0x%x\n"
        L"NvDimmRegionSize: 0x%llx\n"
        L"RegionOffset: 0x%llx\n"
        L"NvDimmPhysicalAddressRegionBase: 0x%llx\n"
        L"InterleaveStructureIndex: 0x%x\n"
        L"InterleaveWays: 0x%x\n"
        L"NvDimmStateFlags: 0x%x\n",
        pTableNvDimmRegion->DeviceHandle.AsUint32,
        pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.DimmNumber,
        pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.MemChannel,
        pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.MemControllerId,
        pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.SocketId,
        pTableNvDimmRegion->DeviceHandle.NfitDeviceHandle.NodeControllerId,
        pTableNvDimmRegion->NvDimmPhysicalId,
        pTableNvDimmRegion->NvDimmRegionalId,
        pTableNvDimmRegion->SpaRangeDescriptionTableIndex,
        pTableNvDimmRegion->NvdimmControlRegionDescriptorTableIndex,
        pTableNvDimmRegion->NvDimmRegionSize,
        pTableNvDimmRegion->RegionOffset,
        pTableNvDimmRegion->NvDimmPhysicalAddressRegionBase,
        pTableNvDimmRegion->InterleaveStructureIndex,
        pTableNvDimmRegion->InterleaveWays,
        pTableNvDimmRegion->NvDimmStateFlags
        );
    break;
  case NVDIMM_INTERLEAVE_TYPE:
    pTableInterleave = (InterleaveStruct *)pTable;
    Print(L"TypeEquals: Interleave\n"
        L"InterleaveStructureIndex: 0x%x\n"
        L"NumberOfLinesDescribed: 0x%x\n"
        L"LineSize: 0x%x\n",
        pTableInterleave->InterleaveStructureIndex,
        pTableInterleave->NumberOfLinesDescribed,
        pTableInterleave->LineSize
        );
    for (Index = 0; Index < pTableInterleave->NumberOfLinesDescribed; Index++) {
      Print(L"LineOffset %d: 0x%x\n", Index, pTableInterleave->LinesOffsets[Index]);
    }
    break;
  case NVDIMM_SMBIOS_MGMT_INFO_TYPE:
    Print(L"TypeEquals: Smbios\n"
        );
    break;
  case NVDIMM_CONTROL_REGION_TYPE:
    pTableControlRegion = (ControlRegionTbl *)pTable;
    Print(L"TypeEquals: ControlRegion\n"
        L"ControlRegionDescriptorTableIndex: 0x%x\n"
        L"VendorId: 0x%x\n"
        L"DeviceId: 0x%x\n"
        L"Rid: 0x%x\n"
        L"SubsystemVendorId: 0x%x\n"
        L"SubsystemDeviceId: 0x%x\n"
        L"SubsystemRid: 0x%x\n"
        L"ValidFields: 0x%x\n"
        L"ManufacturingLocation: 0x%x\n"
        L"ManufacturingDate: 0x%x\n"
        L"SerialNumber: 0x%x\n"
        L"RegionFormatInterfaceCode: 0x%x\n"
        L"NumberOfBlockControlWindows: 0x%x\n",
        pTableControlRegion->ControlRegionDescriptorTableIndex,
        pTableControlRegion->VendorId,
        pTableControlRegion->DeviceId,
        pTableControlRegion->Rid,
        pTableControlRegion->SubsystemVendorId,
        pTableControlRegion->SubsystemDeviceId,
        pTableControlRegion->SubsystemRid,
        pTableControlRegion->ValidFields,
        pTableControlRegion->ManufacturingLocation,
        pTableControlRegion->ManufacturingDate,
        pTableControlRegion->SerialNumber,
        pTableControlRegion->RegionFormatInterfaceCode,
        pTableControlRegion->NumberOfBlockControlWindows
        );
    if (pTableControlRegion->NumberOfBlockControlWindows > 0) {
      Print(L"SizeOfBlockControlWindow: 0x%llx\n"
          L"CommandRegisterOffsetInBlockControlWindow: 0x%llx\n"
          L"SizeOfCommandRegisterInBlockControlWindows: 0x%llx\n"
          L"StatusRegisterOffsetInBlockControlWindow: 0x%llx\n"
          L"SizeOfStatusRegisterInBlockControlWindows: 0x%llx\n"
          L"ControlRegionFlag: 0x%x\n",
          pTableControlRegion->SizeOfBlockControlWindow,
          pTableControlRegion->CommandRegisterOffsetInBlockControlWindow,
          pTableControlRegion->SizeOfCommandRegisterInBlockControlWindows,
          pTableControlRegion->StatusRegisterOffsetInBlockControlWindow,
          pTableControlRegion->SizeOfStatusRegisterInBlockControlWindows,
          pTableControlRegion->ControlRegionFlag
          );
    }

    break;
  case NVDIMM_BW_DATA_WINDOW_REGION_TYPE:
    pTableBWRegion = (BWRegionTbl *)pTable;
    Print(L"TypeEquals: BWRegion\n"
        L"ControlRegionStructureIndex: 0x%x\n"
        L"NumberOfBlockDataWindows: 0x%x\n"
        L"BlockDataWindowStartLogicalOffset: 0x%lx\n"
        L"SizeOfBlockDataWindow: 0x%lx\n"
        L"AccessibleBlockCapacity: 0x%lx\n"
        L"AccessibleBlockCapacityStartAddress: 0x%lx\n",
        pTableBWRegion->ControlRegionStructureIndex,
        pTableBWRegion->NumberOfBlockDataWindows,
        pTableBWRegion->BlockDataWindowStartLogicalOffset,
        pTableBWRegion->SizeOfBlockDataWindow,
        pTableBWRegion->AccessibleBlockCapacity,
        pTableBWRegion->AccessibleBlockCapacityStartAddress
        );
    break;
  case NVDIMM_FLUSH_HINT_TYPE:
    pTableFlushHint = (FlushHintTbl *)pTable;
    Print(L"TypeEquals: FlushHint\n"
        L"NfitDeviceHandle.DimmNumber: 0x%x\n"
        L"NfitDeviceHandle.MemChannel: 0x%x\n"
        L"NfitDeviceHandle.MemControllerId: 0x%x\n"
        L"NfitDeviceHandle.SocketId: 0x%x\n"
        L"NfitDeviceHandle.NodeControllerId: 0x%x\n"
        L"NumberOfFlushHintAddressesInThisStructureM: 0x%x\n",
        pTableFlushHint->DeviceHandle.NfitDeviceHandle.DimmNumber,
        pTableFlushHint->DeviceHandle.NfitDeviceHandle.MemChannel,
        pTableFlushHint->DeviceHandle.NfitDeviceHandle.MemControllerId,
        pTableFlushHint->DeviceHandle.NfitDeviceHandle.SocketId,
        pTableFlushHint->DeviceHandle.NfitDeviceHandle.NodeControllerId,
        pTableFlushHint->NumberOfFlushHintAddressesInThisStructureM
        );
    for (Index = 0; Index < pTableFlushHint->NumberOfFlushHintAddressesInThisStructureM; Index++) {
      Print(L"FlushHintAddress %d: 0x%llx\n", Index, pTableFlushHint->FlushHintAddress[Index]);
    }
    break;
  default:
    break;
  }
}

/**
  PrintNFit - prints the header and all of the tables in the parsed NFit table.

  @param[in] pHeader pointer to the parsed NFit header.
**/
VOID
PrintNFit(
  IN     ParsedFitHeader *pHeader
  )
{
  UINT16 Index = 0;

  if (pHeader == NULL) {
    return;
  }

  PrintAcpiHeader(&pHeader->pFit->Header);

  Print(L"\n");

  Print(L"BwRegionTablesNum: %d\n"
      L"ControlRegionTablesNum: %d\n"
      L"FlushHintTablesNum: %d\n"
      L"InterleaveTablesNum: %d\n"
      L"NVDIMMRegionTablesNum: %d\n"
      L"SmbiosTablesNum: %d\n"
      L"SpaRangeTablesNum: %d\n",
      pHeader->BWRegionTblesNum,
      pHeader->ControlRegionTblesNum,
      pHeader->FlushHintTblesNum,
      pHeader->InterleaveTblesNum,
      pHeader->NvDimmRegionTblesNum,
      pHeader->SmbiosTblesNum,
      pHeader->SpaRangeTblesNum
      );

  Print(L"\n");

  for(Index = 0; Index < pHeader->BWRegionTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppBWRegionTbles[Index]);
    Print(L"\n");
  }

  for(Index = 0; Index < pHeader->ControlRegionTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppControlRegionTbles[Index]);
    Print(L"\n");
  }

  for(Index = 0; Index < pHeader->FlushHintTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppFlushHintTbles[Index]);
    Print(L"\n");
  }

  for(Index = 0; Index < pHeader->InterleaveTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppInterleaveTbles[Index]);
    Print(L"\n");
  }

  for(Index = 0; Index < pHeader->NvDimmRegionTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppNvDimmRegionTbles[Index]);
    Print(L"\n");
  }

  for(Index = 0; Index < pHeader->SmbiosTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppSmbiosTbles[Index]);
    Print(L"\n");
  }

  for(Index = 0; Index < pHeader->SpaRangeTblesNum; Index++) {
    PrintFitTable((SubTableHeader *)pHeader->ppSpaRangeTbles[Index]);
    Print(L"\n");
  }
}
/**
PrintPMTT - prints the header and all of the tables in the parsed PMTT table.

@param[in] pPcat pointer to the parsed PMTT.
**/
VOID
PrintPMTT(
  IN     PMTT_TABLE *pPMTT
)
{
  if (pPMTT == NULL) {
    NVDIMM_DBG("NULL Pointer provided");
    return;
  }
  PrintAcpiHeader(&pPMTT->Header);
  Print(L"\n");
  UINT64 PmttLen = pPMTT->Header.Length;
  UINT64 Offset = sizeof(pPMTT->Header) + sizeof(pPMTT->Reserved);
  while (Offset < PmttLen) {
    PMTT_COMMON_HEADER *pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
    if (pCommonHeader->Type == PMTT_TYPE_SOCKET) {
      PMTT_SOCKET *pSocket = (PMTT_SOCKET *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
      Print(L"--------------------------Socket--------------------------\n");
      Print(L"Type: %d\nReserved1: %d\nLength: %d\nFlags:%d\nReserved2:%d\n", pCommonHeader->Type,
        pCommonHeader->Reserved1, pCommonHeader->Length, pCommonHeader->Flags, pCommonHeader->Reserved2);
      Print(L"SocketId: %d\nReserved3: %d\n", pSocket->SocketId, pSocket->Reserved3);
      Offset += sizeof(PMTT_SOCKET) + PMTT_COMMON_HDR_LEN;
    } else if (pCommonHeader->Type == PMTT_TYPE_iMC) {
      PMTT_iMC *piMC = (PMTT_iMC *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
      Print(L"-------------------iMC-------------------\n");
      Print(L"Type: %d\nReserved1: %d\nLength: %d\nFlags:%d\nReserved2:%d\n", pCommonHeader->Type,
        pCommonHeader->Reserved1, pCommonHeader->Length, pCommonHeader->Flags, pCommonHeader->Reserved2);
      Print(L"ReadLatency: %d\nWriteLatency: %d\nReadBW: %d\nWriteBW:%d\nOptimalAccessUnit:%d\n"
        "OptimalAccessAlignment:%d\nReserved3:%d\nNoOfProximityDomains:%d\nProximityDomainArray:%d\n",
        piMC->ReadLatency, piMC->WriteLatency, piMC->ReadBW, piMC->WriteBW, piMC->OptimalAccessUnit,
        piMC->OptimalAccessAlignment, piMC->Reserved3, piMC->WriteBW, piMC->NoOfProximityDomains,
        piMC->ProximityDomainArray);
      Offset += sizeof(PMTT_iMC) + PMTT_COMMON_HDR_LEN;
    } else if (pCommonHeader->Type == PMTT_TYPE_MODULE) {
      Print(L"----MODULE----\n");
      PMTT_MODULE *pModule = (PMTT_MODULE *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
      Print(L"Type: %d\nReserved1: %d\nLength: %d\nFlags:%d\nReserved2:%d\n", pCommonHeader->Type,
        pCommonHeader->Reserved1, pCommonHeader->Length, pCommonHeader->Flags, pCommonHeader->Reserved2);
      Print(L"PhysicalComponentId: %d\nReserved3: %d\nSizeOfDimm: %d\nSmbiosHandle: %d\n",
        pModule->PhysicalComponentId, pModule->Reserved3, pModule->SizeOfDimm, pModule->SmbiosHandle);
      Offset += sizeof(PMTT_MODULE) + PMTT_COMMON_HDR_LEN;
    }
  }
}