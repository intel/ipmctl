/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PROCESSOR_AND_TOPOLOGY_INFO_H_
#define _PROCESSOR_AND_TOPOLOGY_INFO_H_

#include <Library/BaseLib.h>
#include <AcpiParsing.h>

#define END_OF_INTERLEAVE_SETS  0

#define DIMMS_PER_CHANNEL                 2
#define INTERLEAVE_BY_ONE_BITMAP_IMC0_CH0  1


/**
  Get the topology and InterleaveSetMap Info based on the processor type
  @param[out] piMCNum Number of iMCs per CPU.
  @param[out] pChannelNum Number of channels per iMC
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

#endif /* _PROCESSOR_AND_TOPOLOGY_INFO_H_ */

