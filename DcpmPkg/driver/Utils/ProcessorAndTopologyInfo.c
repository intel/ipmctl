#include "ProcessorAndTopologyInfo.h"

// 2 memory controllers, 3 channels
// where bit placement represents the
// DIMMs ordered as such:
// ---------
//         IMC0       IMC1
// CH0 | 0b000001 | 0b000010 |
// CH1 | 0b000100 | 0b001000 |
// CH2 | 0b010000 | 0b100000 |
// ---------
const UINT32 INTERLEAVE_SETS_2_3[] =
{
                0x3F,     //0b111111 x6

                0x0F,     //0b001111 x4
                0x3C,     //0b111100 x4
                0x33,     //0b110011 x4

                0x15,     //0b010101 x3
                0x2A,     //0b101010 x3

                // favor across memory controller
                0x03,     //0b000011 x2
                0x0C,     //0b001100 x2
                0x30,     //0b110000 x2

                // before across channel
                0x05,     //0b000101 x2
                0x0A,     //0b001010 x2
                0x14,     //0b010100 x2
                0x28,     //0b101000 x2
                0x11,     //0b010001 x2
                0x22,     //0b100010 x2

                // lastly x1
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
*/
EFI_STATUS GetTopologyAndInterleaveSetMapInfoBasedOnProcessorType(UINT8 *piMCNum OPTIONAL, UINT8 *pChannelNum OPTIONAL,
            UINT8 *pDimmsperIMC OPTIONAL, const UINT32 **ppInterleaveMap OPTIONAL) {
  EFI_STATUS EFIRc = EFI_SUCCESS;
  if (ppInterleaveMap != NULL) {
    *ppInterleaveMap = INTERLEAVE_SETS_2_3;
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
  return EFIRc;
}
