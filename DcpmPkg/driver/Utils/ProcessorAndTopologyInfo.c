#include "ProcessorAndTopologyInfo.h"
#include <NvmSharedDefs.h>
#ifdef OS_BUILD
#include <os.h>
#endif

// 2 memory controllers, 3 channels
// where bit placement represents the
// PMem modules ordered as such:
// ---------
//          CH0        CH1        CH2
// IMC0 | 0b000001 | 0b000010 | 0b000100 |
// IMC1 | 0b001000 | 0b010000 | 0b100000 |
// ---------
UINT32 INTERLEAVE_SETS_2_3[] =
{
                0x3F,     //0b111111 x6

                0x1B,     //0b011011 x4
                0x2D,     //0b101101 x4
                0x36,     //0b110110 x4

                0x07,     //0b000111 x3
                0x38,     //0b111000 x3

                // favor across memory controller
                0x09,     //0b001001 x2
                0x12,     //0b010010 x2
                0x24,     //0b100100 x2

                // before across channel
                0x03,     //0b000011 x2
                0x05,     //0b000101 x2
                0x06,     //0b000110 x2
                0x18,     //0b011000 x2
                0x28,     //0b101000 x2
                0x30,     //0b110000 x2
};


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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 NumOfiMCsPerCPU = 0;
  UINT32 NumOfChannelsPeriMC = 0;
  UINT32 InterleaveMapListLength = 0;
  UINT32 Index = 0;
  BOOLEAN TopologyCanBeDetermined = FALSE;

  NVDIMM_ENTRY();

  ReturnCode = RetrievePlatformTopologyFromPmtt(&NumOfiMCsPerCPU, &NumOfChannelsPeriMC, &TopologyCanBeDetermined);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("RetrievePlatformTopologyFromPmtt failed.");
    goto Finish;
  }

  // Fallback to the default topology if it cannot be determined using PMTT
  if (!TopologyCanBeDetermined) {
    NumOfiMCsPerCPU = IMCS_PER_CPU_2_3;
    NumOfChannelsPeriMC = CHANNELS_PER_IMC_2_3;
  }

  if (ppInterleaveMap != NULL) {
    ReturnCode = RetrieveInterleaveSetMap(ppInterleaveMap, &InterleaveMapListLength);
    /**
      BIOS did not publish the interleave format list in PCAT or
      BIOS published it but the topology cannot be determined,
      so interleave bitmap cannot be interpreted correctly.
      In both cases, fallback to the default list for 2 iMcsPerCPU
      & 3 ChannelsPeriMC topology.
    **/
    if (ReturnCode == EFI_NOT_FOUND || !TopologyCanBeDetermined) {
      FREE_POOL_SAFE(*ppInterleaveMap);
      *ppInterleaveMap = AllocateZeroPool(sizeof(INTERLEAVE_SETS_2_3));
      if (*ppInterleaveMap == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        NVDIMM_WARN("Memory allocation error.");
        goto Finish;
      }
      CopyMem_S(*ppInterleaveMap, sizeof(INTERLEAVE_SETS_2_3), INTERLEAVE_SETS_2_3, sizeof(INTERLEAVE_SETS_2_3));
      InterleaveMapListLength = sizeof(INTERLEAVE_SETS_2_3)/sizeof(INTERLEAVE_SETS_2_3[0]);
      ReturnCode = EFI_SUCCESS;
    }
    else if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("RetrieveInterleaveSetMap failed.");
      goto Finish;
    }

    // Append the x1 bitmaps to the interleave fromat list received from BIOS
    *ppInterleaveMap = ReallocatePool(sizeof(**ppInterleaveMap) * InterleaveMapListLength,
      sizeof(**ppInterleaveMap) * (InterleaveMapListLength + (NumOfiMCsPerCPU * NumOfChannelsPeriMC) + 1), *ppInterleaveMap);
    if (*ppInterleaveMap == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_WARN("Memory allocation error.");
      goto Finish;
    }

    for (Index = 0; Index < (NumOfiMCsPerCPU * NumOfChannelsPeriMC); Index++) {
      (*ppInterleaveMap)[Index + InterleaveMapListLength] = INTERLEAVE_BYONE_BITMAP_IMC0_CH0 << Index;
    }
    (*ppInterleaveMap)[Index + InterleaveMapListLength] = END_OF_INTERLEAVE_SETS;
  }

  if (piMCNum != NULL) {
    *piMCNum = NumOfiMCsPerCPU & MAX_UINT8;
  }

  if (pChannelNum != NULL) {
    *pChannelNum = NumOfChannelsPeriMC & MAX_UINT8;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
