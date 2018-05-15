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
  IN OUT ParsedPcatHeader *pParsedPcat
  )
{
  UINT32 Index = 0;

  if (pParsedPcat == NULL) {
    return;
  }

  FREE_POOL_SAFE(pParsedPcat->pPlatformConfigAttr);

  for (Index = 0; Index < pParsedPcat->PlatformCapabilityInfoNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppPlatformCapabilityInfo[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppPlatformCapabilityInfo);

  for (Index = 0; Index < pParsedPcat->MemoryInterleaveCapabilityInfoNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppMemoryInterleaveCapabilityInfo[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppMemoryInterleaveCapabilityInfo);

  for (Index = 0; Index < pParsedPcat->RuntimeInterfaceValConfInputNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppRuntimeInterfaceValConfInput[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppRuntimeInterfaceValConfInput);

  for (Index = 0; Index < pParsedPcat->ConfigManagementAttributesInfoNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppConfigManagementAttributesInfo[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppConfigManagementAttributesInfo);

  for (Index = 0; Index < pParsedPcat->SocketSkuInfoNum; Index++) {
    FREE_POOL_SAFE(pParsedPcat->ppSocketSkuInfoTable[Index]);
  }
  FREE_POOL_SAFE(pParsedPcat->ppSocketSkuInfoTable);

  FREE_POOL_SAFE(pParsedPcat);
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

  for(Index = 0; Index < ParsedNfit->NvDimmRegionTblesNum; Index++) {
    FREE_POOL_SAFE(ParsedNfit->ppNvDimmRegionTbles[Index]);
  }
  FREE_POOL_SAFE(ParsedNfit->ppNvDimmRegionTbles);
  ParsedNfit->NvDimmRegionTblesNum = 0;

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
  IN     ParsedFitHeader *pParsedNfit
  )
{
  if (pParsedNfit == NULL) {
    return;
  }

  FREE_POOL_SAFE(pParsedNfit->pFit);
  FreeNfitSubTables(pParsedNfit);

  FreePool(pParsedNfit);
}
