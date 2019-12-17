/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "NvmTables.h"
#include <Debug.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

/**
  Frees the memory of parsed Nfit subtables.

  @param[in] ParsedNfit pointer to the NFit header.
**/
STATIC
VOID
FreeNfitSubTables(
  IN     ParsedFitHeader *pParsedNfit
  );

/**
  Frees the memory associated in the parsed PCAT table.

  @param[in, out] pParsedPcat pointer to the PCAT header.
**/
VOID
FreeParsedPcat(
  IN OUT ParsedPcatHeader **ppParsedPcat
  )
{
  UINT32 Index = 0;
  ACPI_REVISION Revision;
  ParsedPcatHeader * pParsedPcat = NULL;
  if (ppParsedPcat == NULL || *ppParsedPcat == NULL) {
    return;
  }
  pParsedPcat = *ppParsedPcat;

  Revision = pParsedPcat->pPlatformConfigAttr->Header.Revision;
  FREE_POOL_SAFE(pParsedPcat->pPlatformConfigAttr);

  if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(Revision)) {
    for (Index = 0; Index < pParsedPcat->PlatformCapabilityInfoNum; Index++) {
      FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo[Index]);
    }
    FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo);
  }
  else if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(Revision)) {
    for (Index = 0; Index < pParsedPcat->PlatformCapabilityInfoNum; Index++) {
      FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo[Index]);
    }
    FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo);
  }

  if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(Revision)) {
    for (Index = 0; Index < pParsedPcat->MemoryInterleaveCapabilityInfoNum; Index++) {
      FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo[Index]);
    }
    FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo);
  }
  else if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(Revision)) {
    for (Index = 0; Index < pParsedPcat->PlatformCapabilityInfoNum; Index++) {
      FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo[Index]);
    }
    FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo);
  }

  for (Index = 0; Index < pParsedPcat->RuntimeInterfaceValConfInputNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppRuntimeInterfaceValConfInput[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppRuntimeInterfaceValConfInput);

  for (Index = 0; Index < pParsedPcat->ConfigManagementAttributesInfoNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppConfigManagementAttributesInfo[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppConfigManagementAttributesInfo);

  if (IS_ACPI_REV_MAJ_0_MIN_1_OR_MIN_2(Revision)) {
    for (Index = 0; Index < pParsedPcat->MemoryInterleaveCapabilityInfoNum; Index++) {
      FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable[Index]);
    }
    FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable);
  }
  else if (IS_ACPI_REV_MAJ_1_MIN_1_OR_MIN_2(Revision)) {
    for (Index = 0; Index < pParsedPcat->PlatformCapabilityInfoNum; Index++) {
      FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable[Index]);
    }
    FREE_POOL_SAFE(pParsedPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable);
  }

  FREE_POOL_SAFE(*ppParsedPcat);
}

/**
  Frees the memory associated in the parsed PMTT table.

  @param[in, out] pParsedPmtt pointer to the PMTT header.
**/
VOID
FreeParsedPmtt(
  IN OUT ParsedPmttHeader **ppParsedPmtt
  )
{
  UINT32 Index = 0;
  ParsedPmttHeader * pParsedPmtt = NULL;
  if (ppParsedPmtt == NULL || *ppParsedPmtt == NULL) {
    return;
  }

  pParsedPmtt = *ppParsedPmtt;
  FREE_POOL_SAFE(pParsedPmtt->pPmtt);

  for (Index = 0; Index < pParsedPmtt->SocketsNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppSockets[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppSockets);

  for (Index = 0; Index < pParsedPmtt->DiesNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppDies[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppDies);

  for (Index = 0; Index < pParsedPmtt->iMCsNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppiMCs[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppiMCs);

  for (Index = 0; Index < pParsedPmtt->ChannelsNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppChannels[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppChannels);

  for (Index = 0; Index < pParsedPmtt->SlotsNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppSlots[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppSlots);

  for (Index = 0; Index < pParsedPmtt->DDRModulesNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppDDRModules[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppDDRModules);

  for (Index = 0; Index < pParsedPmtt->DCPMModulesNum; Index++) {
    FREE_POOL_SAFE(pParsedPmtt->ppDCPMModules[Index]);
  }
  FREE_POOL_SAFE(pParsedPmtt->ppDCPMModules);

  FREE_POOL_SAFE(*ppParsedPmtt);
}

/**
  Frees the memory of parsed Nfit subtables.

  @param[in] ParsedNfit pointer to the NFit header.
**/
STATIC
VOID
FreeNfitSubTables(
  IN     ParsedFitHeader *ParsedNfit
  )
{
  UINT32 Index = 0;

  if (ParsedNfit == NULL) {
    return;
  }

  for(Index = 0; Index < ParsedNfit->BWRegionTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppBWRegionTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppBWRegionTbles);
  ParsedNfit->BWRegionTblesNum = 0;

  for(Index = 0; Index < ParsedNfit->ControlRegionTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppControlRegionTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppControlRegionTbles);
  ParsedNfit->ControlRegionTblesNum = 0;

  for(Index = 0; Index < ParsedNfit->FlushHintTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppFlushHintTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppFlushHintTbles);
  ParsedNfit->FlushHintTblesNum = 0;

  for(Index = 0; Index < ParsedNfit->InterleaveTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppInterleaveTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppInterleaveTbles);
  ParsedNfit->InterleaveTblesNum = 0;

  for(Index = 0; Index < ParsedNfit->NvDimmRegionMappingStructuresNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppNvDimmRegionMappingStructures[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppNvDimmRegionMappingStructures);
  ParsedNfit->NvDimmRegionMappingStructuresNum = 0;

  for(Index = 0; Index < ParsedNfit->SmbiosTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppSmbiosTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppSmbiosTbles);
  ParsedNfit->SmbiosTblesNum = 0;

  for(Index = 0; Index < ParsedNfit->SpaRangeTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppSpaRangeTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppSpaRangeTbles);
  ParsedNfit->SpaRangeTblesNum = 0;
}

/**
  Frees the memory associated in the parsed NFit table.

  @param[in] pParsedNfit pointer to the NFit header.
**/
VOID
FreeParsedNfit(
  IN     ParsedFitHeader **ppParsedNfit
  )
{
  ParsedFitHeader *pParsedNfit = NULL;
  if (ppParsedNfit == NULL || *ppParsedNfit == NULL) {
    return;
  }
  pParsedNfit = *ppParsedNfit;
  FREE_POOL_SAFE(pParsedNfit->pFit);
  FreeNfitSubTables(pParsedNfit);

  FREE_POOL_SAFE(*ppParsedNfit);
}
