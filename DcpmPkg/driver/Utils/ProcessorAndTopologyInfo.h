/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PROCESSORANDTOPOLOGYINFO_H_
#define _PROCESSORANDTOPOLOGYINFO_H_

#include <Library/BaseLib.h>

#define END_OF_INTERLEAVE_SETS  0

#define DIMMS_PER_CHANNEL       2

 // 2 iMC and 3 channels each - purley
#define IMCS_PER_CPU_2_3                2
#define CHANNELS_PER_IMC_2_3            3
#define DIMMS_PER_MEM_CTRL_2_3          6


  /* Actual family number obtained by left shifting the extended family by 4 bits and adding the 8 - 11 family bits */
#define EXTENDED_FAMILY(value) ((value >> 20) & 0xF)
#define ACTUAL_FAMILY(value)  ((EXTENDED_FAMILY(value) << 4)  | ((value >> 8) & 0xF))

/* Actual model number obtained by left shifting the extended model by 4 bits and adding the 4 - 7 model bits */
#define EXTENDED_MODEL(value)  ((value >> 16) & 0xF)
#define ACTUAL_MODEL(value)    ((EXTENDED_MODEL(value) << 4)  | ((value >> 4) & 0xF))

/* Get the topology and InterleaveSetMap Info based on the processor type
  @param[out] OPTIONAL - number of iMCs per CPU.
  @param[out] pChannelNum OPTIONAL - number of channles per iMC
  @param[out] pDimmsperIMC OPTIONAL - number of dimms per iMC
  @param[out] ppInterleaveMap OPTIONAL- return InterleaveSetMap based on the processor type

  @retval EFI_SUCCESS Ok
  @retval EFI_INVALID_PARAMETER invalid parameter
*/
EFI_STATUS GetTopologyAndInterleaveSetMapInfoBasedOnProcessorType(UINT8 *piMCNum OPTIONAL, UINT8 *pChannelNum OPTIONAL,
  UINT8 *pDimmsperIMC OPTIONAL, const UINT32 **ppInterleaveMap OPTIONAL);

#endif /* _PROCESSORANDTOPOLOGYINFO_H_ */

