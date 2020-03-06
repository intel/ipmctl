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

// 4 memory controllers, 2 channels
// where bit placement represents the
// PMem modules ordered as such:
// ---------
//           CH0          CH1
// IMC0 | 0b00000001 | 0b00000010 |
// IMC1 | 0b00000100 | 0b00001000 |
// IMC1 | 0b00010000 | 0b00100000 |
// IMC2 | 0b01000000 | 0b10000000 |
// ---------
UINT32 INTERLEAVE_SETS_4_2[] =
{
                0xFF,     //0b11111111 x8

                //across iMCs x4
                0x55,     //0b01010101 x4
                0xAA,     //0b10101010 x4
                0x33,     //0b00110011 x4
                0xCC,     //0b11001100 x4
                0xC3,     //0b11000011 x4
                0x3C,     //0b00111100 x4
                0x0F,     //0b00001111 x4
                0xF0,     //0b11110000 x4

                // across iMCs x2
                0x11,     //0b00010001 x2
                0x22,     //0b00100010 x2
                0x44,     //0b01000100 x2
                0x88,     //0b10001000 x2
                0x41,     //0b01000001 x2
                0x82,     //0b10000010 x2
                0x14,     //0b00010100 x2
                0x28,     //0b00101000 x2
                0x05,     //0b00000101 x2
                0x0A,     //0b00001010 x2
                0x50,     //0b01010000 x2
                0xA0,     //0b10100000 x2

                // single iMC fully populated
                0x03,     //0b00000011 x2
                0x0C,     //0b00001100 x2
                0x30,     //0b00110000 x2
                0xC0,     //0b11000000 x2

                // lastly x1
                0x01,     //0b00000001 x1
                0x02,     //0b00000010 x1
                0x04,     //0b00000100 x1
                0x08,     //0b00001000 x1
                0x10,     //0b00010000 x1
                0x20,     //0b00100000 x1
                0x40,     //0b01000000 x1
                0x80,     //0b10000000 x1
                END_OF_INTERLEAVE_SETS
};

// Append this to interleave format list we
// receive from BIOS for convenience
UINT32 INTERLEAVE_SETS_4_2_x1[] =
{
                0x01,     //0b00000001 x1
                0x02,     //0b00000010 x1
                0x04,     //0b00000100 x1
                0x08,     //0b00001000 x1
                0x10,     //0b00010000 x1
                0x20,     //0b00100000 x1
                0x40,     //0b01000000 x1
                0x80,     //0b10000000 x1
                END_OF_INTERLEAVE_SETS
};

// 4 iMC and 2 channels each
#define IMCS_PER_CPU_4_2                4
#define CHANNELS_PER_IMC_4_2            2
#define DIMMS_PER_MEM_CTRL_4_2          4

#define CURRENT_FAMILY 0x06
#define CURRENT_MODEL  0x6A

#define REGISTER_COUNT 4
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
  unsigned int RegisterList[REGISTER_COUNT]; /* Register list from eax to edx*/
  int InitialEaxRegisterVal = 1;
  UINT32 InterleaveMapListLength = 0;
  UINT32 Index = 0;
#ifdef OS_BUILD
  int NvmRc = NVM_SUCCESS;
  NvmRc = getCPUID(RegisterList, REGISTER_COUNT, InitialEaxRegisterVal);
  if (NvmRc != NVM_SUCCESS) {
    EFIRc = EFI_INVALID_PARAMETER;
    return EFIRc;
  }
#else
  AsmCpuid((UINT32)InitialEaxRegisterVal, (UINT32 *)&RegisterList[0], (UINT32 *)&RegisterList[1], (UINT32 *)&RegisterList[2], (UINT32 *)&RegisterList[3]);
#endif
  if (ACTUAL_FAMILY(RegisterList[0]) == CURRENT_FAMILY && ACTUAL_MODEL(RegisterList[0]) == CURRENT_MODEL) {
    if (ppInterleaveMap != NULL) {
      EFIRc = RetrieveInterleaveSetMap(ppInterleaveMap, &InterleaveMapListLength);
      // If there is no MemoryInterleaveCapability table in PCAT
      if (EFIRc == EFI_NOT_FOUND) {
        *ppInterleaveMap = AllocateZeroPool(sizeof(INTERLEAVE_SETS_4_2));
        if (*ppInterleaveMap == NULL) {
          EFIRc = EFI_OUT_OF_RESOURCES;
          NVDIMM_WARN("Memory allocation error");
          goto Finish;
        }
        CopyMem_S(*ppInterleaveMap, sizeof(INTERLEAVE_SETS_4_2), INTERLEAVE_SETS_4_2, sizeof(INTERLEAVE_SETS_4_2));
        EFIRc = EFI_SUCCESS;
      }
      else if (EFI_ERROR(EFIRc)) {
        goto Finish;
      }
      else {
        *ppInterleaveMap = ReallocatePool(sizeof(**ppInterleaveMap) * InterleaveMapListLength,
          sizeof(**ppInterleaveMap) * (InterleaveMapListLength + (IMCS_PER_CPU_4_2 * CHANNELS_PER_IMC_4_2) + 1), *ppInterleaveMap);
        if (*ppInterleaveMap == NULL) {
          EFIRc = EFI_OUT_OF_RESOURCES;
          NVDIMM_WARN("Memory allocation error");
          goto Finish;
        }
        for (Index = 0; Index < (IMCS_PER_CPU_4_2 * CHANNELS_PER_IMC_4_2) + 1; Index++) {
          (*ppInterleaveMap)[Index + InterleaveMapListLength] = INTERLEAVE_SETS_4_2_x1[Index];
        }
      }
    }
    if (pDimmsperIMC != NULL) {
      *pDimmsperIMC = DIMMS_PER_MEM_CTRL_4_2;
    }
    if (piMCNum != NULL) {
      *piMCNum = IMCS_PER_CPU_4_2;
    }
    if (pChannelNum != NULL) {
      *pChannelNum = CHANNELS_PER_IMC_4_2;
    }
  }
  else {
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
  }

Finish:
  return EFIRc;
}
