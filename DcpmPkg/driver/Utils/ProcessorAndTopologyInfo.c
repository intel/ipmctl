#include "ProcessorAndTopologyInfo.h"

// 2 memory controllers, 3 channels
// where bit placement represents the
// DIMMs ordered as such:
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

                // lastly x1
                0x01,     //0b000001 x1
                0x02,     //0b000010 x1
                0x04,     //0b000100 x1
                0x08,     //0b001000 x1
                0x10,     //0b010000 x1
                0x20,     //0b100000 x1

                END_OF_INTERLEAVE_SETS
};

// Append this to interleave format list we
// receive from BIOS for convenience
UINT32 INTERLEAVE_SETS_2_3_x1[] =
{
                0x01,     //0b000001 x1
                0x02,     //0b000010 x1
                0x04,     //0b000100 x1
                0x08,     //0b001000 x1
                0x10,     //0b010000 x1
                0x20,     //0b100000 x1

                END_OF_INTERLEAVE_SETS
};

/* Get the topology and InterleaveSetMap Info based on the processor type
  @param[out] OPTIONAL - number of iMCs per CPU.
  @param[out] pChannelNum OPTIONAL - number of channles per iMC
  @param[out] pDimmsperIMC OPTIONAL - number of dimms per iMC
  @param[out] ppInterleaveMap OPTIONAL- return InterleaveSetMap based on the processor type

  @retval EFI_SUCCESS Ok
  @retval EFI_INVALID_PARAMETER invalid parameter
  @retval EFI_OUT_OF_RESOURCES memory allocation failed
*/
EFI_STATUS GetTopologyAndInterleaveSetMapInfoBasedOnProcessorType(UINT8 *piMCNum OPTIONAL, UINT8 *pChannelNum OPTIONAL,
            UINT8 *pDimmsperIMC OPTIONAL, UINT32 **ppInterleaveMap OPTIONAL) {
  EFI_STATUS EFIRc = EFI_SUCCESS;
  UINT32 InterleaveMapListLength = 0;
  UINT32 Index = 0;

  if (ppInterleaveMap != NULL) {
    EFIRc = RetrieveInterleaveSetMap(ppInterleaveMap, &InterleaveMapListLength);
    // If there is no MemoryInterleaveCapability table in PCAT
    if (EFIRc == EFI_NOT_FOUND) {
      *ppInterleaveMap = AllocateZeroPool(sizeof(INTERLEAVE_SETS_2_3));
      if (*ppInterleaveMap == NULL) {
        EFIRc = EFI_OUT_OF_RESOURCES;
        NVDIMM_WARN("Memory allocation error");
        goto Finish;
      }
      CopyMem_S(*ppInterleaveMap, sizeof(INTERLEAVE_SETS_2_3), INTERLEAVE_SETS_2_3, sizeof(INTERLEAVE_SETS_2_3));
      EFIRc = EFI_SUCCESS;
    }
    else if (EFI_ERROR(EFIRc)) {
      goto Finish;
    }
    else {
      *ppInterleaveMap = ReallocatePool(sizeof(**ppInterleaveMap) * InterleaveMapListLength,
        sizeof(**ppInterleaveMap) * (InterleaveMapListLength + (IMCS_PER_CPU_2_3 * CHANNELS_PER_IMC_2_3) + 1), *ppInterleaveMap);
      if (*ppInterleaveMap == NULL) {
        EFIRc = EFI_OUT_OF_RESOURCES;
        NVDIMM_WARN("Memory allocation error");
        goto Finish;
      }
      for (Index = 0; Index < (IMCS_PER_CPU_2_3 * CHANNELS_PER_IMC_2_3) + 1; Index++) {
        (*ppInterleaveMap)[Index + InterleaveMapListLength] = INTERLEAVE_SETS_2_3_x1[Index];
      }
    }
  }
  if (pDimmsperIMC != NULL) {
    *pDimmsperIMC = DIMMS_PER_MEM_CTRL_2_3;
  }
  if (piMCNum != NULL) {
    *piMCNum = IMCS_PER_CPU_2_3;
  }
  if (pChannelNum != NULL) {
    *pChannelNum = CHANNELS_PER_IMC_2_3;
  }

  Finish:
    return EFIRc;
}
