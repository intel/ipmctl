/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Dimm.h"
#include "Interleave.h"
#include "Namespace.h"
#include "Region.h"
#include <Library/PrintLib.h>

#define DIMM_LOCATION(iMC, channel, iMCsPerCPU, channelsPeriMC)     (iMCsPerCPU * (channel % channelsPeriMC) + iMC)
#define DIMM_POPULATED(map, dimmIndex)  ((map >> dimmIndex) & 0x1)
#define CLEAR_DIMM(map, dimmIndex)      map &= ~(0x1 << dimmIndex)

 /**
   Remove the dimm from the list of dimms and update the other
   dimms in the list to shift left by 1.

   @param[in out] pDimmsList The current list of DIMMs
   @param[in out] pDimmsListNum The number of DIMMs
   @param[in out] pDimm The DIMM to be removed from pDimmsList

   @retval EFI_SUCCESS
   @retval EFI_INVALID_PARAMETER If input parameters are null
 **/
STATIC
EFI_STATUS
RemoveDimmFromList(
  IN OUT DIMM *pDimmsList[],
  IN OUT UINT32 *pDimmsListNum,
  IN     DIMM *pDimm
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  BOOLEAN Found = FALSE;

  NVDIMM_ENTRY();

  if (pDimmsList == NULL || pDimmsListNum == NULL || pDimm == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < *pDimmsListNum; Index++) {
    if (pDimm == pDimmsList[Index]) {
      Found = TRUE;
      break;
    }
  }

  if (Found) {
    for (; Index < (*pDimmsListNum - 1); Index++) {
      pDimmsList[Index] = pDimmsList[Index + 1];
    }
    (*pDimmsListNum)--;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
/**
  Remove the dimm array from the list of dimms and update the number
  of dimms in the list.

  @param[in out] pDimmsList The current list of DIMMs
  @param[in out] pDimmsListNum The number of DIMMs
  @param[in out] pDimmsArray The DIMMs to be removed from pDimmsList
  @param[in] DimmsArrayNum Number of DIMMs to be removed

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER If input parameters are null
**/
STATIC
EFI_STATUS
RemoveDimmArrayFromList(
  IN OUT DIMM *pDimmsList[],
  IN OUT UINT32 *pDimmsListNum,
  IN OUT DIMM *pDimmsArray[],
  IN     UINT32 DimmsArrayNum
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pDimmsList == NULL || pDimmsArray == NULL || pDimmsListNum == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < DimmsArrayNum; Index++) {
    // Remove 1 Dimm from the list at a time
    RemoveDimmFromList(pDimmsList, pDimmsListNum, pDimmsArray[Index]);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get the DIMMs matching the current interleave set map.

  @param[in out] pDimms The current list of DIMMs
  @param[in] DimmsNum The number of DIMMs
  @param[in] InterleaveMap The current interleave map
  @param[in out] pDimmsInterleaved The DIMMs to be interleaved together
  @param[in out] pDimmsUsed Number of interleaved DIMMs

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER If input parameters are null
**/
STATIC
EFI_STATUS
GetDimmsFromListMatchingInterleaveSet(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     const UINT32 InterleaveMap,
  IN     UINT8 iMCNum,
  IN     UINT8 ChannelNum,
  IN OUT DIMM *pDimmsInterleaved[],
  IN OUT UINT32 *pDimmsUsed
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 DimmsNotFound = InterleaveMap;
  UINT32 Index = 0;
  UINT32 DimmIndex = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsInterleaved == NULL || pDimmsUsed == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }


  for (Index = 0; Index < DimmsNum; Index++) {
    DimmIndex = DIMM_LOCATION(pDimms[Index]->ImcId, pDimms[Index]->ChannelId, iMCNum, ChannelNum);
    if (DIMM_POPULATED(InterleaveMap, DimmIndex)) {
      pDimmsInterleaved[(*pDimmsUsed)++] = pDimms[Index];
      CLEAR_DIMM(DimmsNotFound, DimmIndex);
    }
  }
  if (DimmsNotFound != 0) {
    *pDimmsUsed = 0;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Find the best possible set of DIMMs that can be interleaved.

  @param[in out] pDimms The current list of DIMMs
  @param[in] DimmsNum The number of DIMMs
  @param[in out] pDimmsInterleaved The DIMMs to be interleaved together
  @param[in out] pDimmsUsed Number of interleaved DIMMs

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER If input parameters are null
**/
STATIC
EFI_STATUS
FindBestInterleavingForDimms(
  IN OUT DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN OUT DIMM *pDimmsInterleaved[],
  IN OUT UINT32 *pDimmsUsed
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT8 NumOfiMCsPerCPU = 0;
  UINT8 NumOfChannelsPeriMC = 0;
  const UINT32 *pInterleaveSet = NULL;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsInterleaved == NULL || pDimmsUsed == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetTopologyAndInterleaveSetMapInfoBasedOnProcessorType(&NumOfiMCsPerCPU, &NumOfChannelsPeriMC, NULL, &pInterleaveSet);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; (*pDimmsUsed == 0) && (pInterleaveSet[Index] != END_OF_INTERLEAVE_SETS); Index++) {
    GetDimmsFromListMatchingInterleaveSet(pDimms, DimmsNum, pInterleaveSet[Index], NumOfiMCsPerCPU, NumOfChannelsPeriMC, pDimmsInterleaved, pDimmsUsed);
  }

  if (*pDimmsUsed == 0) {
    NVDIMM_WARN("Interleaving match not found");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Perform interleaving across DIMMs and create goals

  @param[in] pRegionGoalTemplate The current pool goal template
  @param[in] pDimms The list of DIMMs
  @param[in] DimmsNum The number of DIMMs in the list
  @param[in] InterleaveSetSize The size of the interleave set
  @param[in] pDriverPreferences The driver preferences
  @param[in] SequenceIndex Sequence index per pool goal template
  @param[in out] pRegionGoal The list of pool goals
  @param[in out] pNewRegionsGoalNum Number of new pool goals
  @param[in out] pInterleaveSetIndex The interleave set index of the goal

  @retval EFI_SUCCESS
  #retval EFI_INVALID_PARAMETER If IS is null
**/
EFI_STATUS
PerformInterleavingAndCreateGoal(
  IN     REGION_GOAL_TEMPLATE *pRegionGoalTemplate,
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     UINT64 InterleaveSetSize,
  IN     DRIVER_PREFERENCES *pDriverPreferences OPTIONAL,
  IN     UINT16 SequenceIndex,
  IN OUT struct _REGION_GOAL *pRegionGoal[],
  IN OUT UINT32 *pNewRegionsGoalNum,
  IN OUT UINT16 *pInterleaveSetIndex
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimmsCopy[MAX_DIMMS];
  DIMM *pDimmsInterleaved[MAX_DIMMS];
  UINT32 RemainingDimms = DimmsNum;
  UINT32 DimmsUsed = 0;
  UINT32 TotalDimmsUsed = 0;

  NVDIMM_ENTRY();

  ZeroMem(pDimmsCopy, sizeof(pDimmsCopy));
  ZeroMem(pDimmsInterleaved, sizeof(pDimmsInterleaved));

  if (pRegionGoalTemplate == NULL || pRegionGoal == NULL || pNewRegionsGoalNum == NULL ||
         pDimms == NULL || pInterleaveSetIndex == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Goal cannot be created of size Zero
  if (InterleaveSetSize == 0) {
    goto Finish;
  }

  if (DimmsNum > 0) {
    CopyMem_S(pDimmsCopy, sizeof(pDimms[0]) * DimmsNum, pDimms, sizeof(pDimms[0]) * DimmsNum);
  }

  while (RemainingDimms > 0) {
    ReturnCode = FindBestInterleavingForDimms(pDimmsCopy, RemainingDimms, pDimmsInterleaved, &DimmsUsed);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    RemoveDimmArrayFromList(pDimmsCopy, &RemainingDimms, pDimmsInterleaved, DimmsUsed);
    TotalDimmsUsed += DimmsUsed;
    if (TotalDimmsUsed > DimmsNum) {
      ReturnCode = EFI_BAD_BUFFER_SIZE;
      goto Finish;
    }
    pRegionGoal[(*pNewRegionsGoalNum)] = CreateRegionGoal(pRegionGoalTemplate, pDimmsInterleaved, DimmsUsed,
                                         InterleaveSetSize * DimmsUsed / DimmsNum, pDriverPreferences,
                                         (UINT16)SequenceIndex, pInterleaveSetIndex);
    if (pRegionGoal[(*pNewRegionsGoalNum)] == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    (*pNewRegionsGoalNum)++;
    DimmsUsed = 0;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
