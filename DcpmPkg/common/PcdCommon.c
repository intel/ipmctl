/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <PcdCommon.h>
#include <Convert.h>


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
    FreeLsaSafe(&pDimmPcdInfo[Index].pLabelStorageArea);
  }

  FREE_POOL_SAFE(pDimmPcdInfo);

Finish:
  NVDIMM_EXIT();
}
