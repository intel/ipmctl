/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _INTERLEAVE_H_
#define _INTERLEAVE_H_

#include <Library/BaseMemoryLib.h>
#include <Region.h>
#include <Debug.h>
#include <Types.h>
#include <Protocol/Driver/NvmDimmConfig.h>
#include <Dimm.h>
#include <NvmTypes.h>
#include "ProcessorAndTopologyInfo.h"
#ifdef OS_BUILD
#include <os_efi_api.h>
#endif // OS_BUILD


/**
  Perform interleaving across DIMMs and create goals

  @param[in] pPoolGoalTemplate The current pool goal template
  @param[in] pDimms The list of DIMMs
  @param[in] DimmsNum The number of DIMMs in the list
  @param[in] InterleaveSetSize The size of the interleave set
  @param[in] pDriverPreferences The driver preferences
  @param[in] SequenceIndex Sequence index per pool goal template
  @param[in out] pPoolGoal The list of pool goals
  @param[in out] pNewPoolsGoalNum Number of new pool goals
  @param[in out] pInterleaveSetIndex The interleave set index of the goal

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER If input parameters are null
  @retval EFI_OUT_OF_RESOURCES if allocation of new goal fails
**/
EFI_STATUS
PerformInterleavingAndCreateGoal(
  IN     REGION_GOAL_TEMPLATE *pPoolGoalTemplate,
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     UINT64 InterleaveSetSize,
  IN     DRIVER_PREFERENCES *pDriverPreferences OPTIONAL,
  IN     UINT16 SequenceIndex,
  IN OUT struct _REGION_GOAL *pRegionGoal[],
  IN OUT UINT32 *pNewPoolsGoalNum,
  IN OUT UINT16 *pInterleaveSetIndex
  );

#endif /** _INTERLEAVE_H_ **/
