/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PROCESSORANDTOPOLOGYINFO_H_
#define _PROCESSORANDTOPOLOGYINFO_H_

#include <Library/BaseLib.h>
#include <AcpiParsing.h>

#define END_OF_INTERLEAVE_SETS  0

#define DIMMS_PER_CHANNEL                 2
#define INTERLEAVE_BYONE_BITMAP_IMC0_CH0  1

// 2 iMC and 3 channels each - purley
#define IMCS_PER_CPU_2_3                2
#define CHANNELS_PER_IMC_2_3            3


/**
  Get the topology and InterleaveSetMap Info based on the processor type
  @param[out] piMCNum Number of iMCs per CPU.
  @param[out] pChannelNum Number of channles per iMC
  @param[out] ppInterleaveMap Pointer to InterleaveSetMap based on the processor type

  @retval EFI_SUCCESS Ok
  @retval EFI_INVALID_PARAMETER invalid parameter
  @retval EFI_OUT_OF_RESOURCES memory allocation failed
**/
EFI_STATUS
GetTopologyAndInterleaveSetMapInfo(
  OUT UINT8 *piMCNum OPTIONAL,
  OUT UINT8 *pChannelNum OPTIONAL,
  OUT UINT32 **ppInterleaveMap OPTIONAL
  );

#endif /* _PROCESSORANDTOPOLOGYINFO_H_ */

