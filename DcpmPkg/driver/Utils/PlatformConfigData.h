/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PLATFORM_CONFIG_DATA_H_
#define _PLATFORM_CONFIG_DATA_H_

#include <Types.h>
#include <PcdCommon.h>

struct _DIMM;

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4200 )
#endif

/**
  Generate PCD Configuration Input for the dimm based on its pools goal configuration

  The caller is responsible to free the allocated memory of PCD Config Input

  @param[in] pDimm the dimm that PCD Config Input is destined for
  @param[out] ppConfigInput new generated PCD Config Input

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
GeneratePcdConfInput(
  IN     struct _DIMM *pDimm,
     OUT NVDIMM_PLATFORM_CONFIG_INPUT **ppConfigInput
  );

/**
  Generate checksum for pData. Entire table and checksum must sum to 0.

  Formula:
  entire table [without checksum field] % 256 [max byte value + 1] + checksum = 0

  @param[in,out] pData Table that will generate the checksum for
  @param[in] Length Size of the pData
  @param[in] ChecksumOffset Offset that indicates the one-byte checksum field in the pData
**/
VOID
GenerateChecksum(
  IN OUT VOID *pData,
  IN     UINT32 Length,
  IN     UINT32 ChecksumOffset
  );

/**
  Verify the checksum. Entire table, including its checksum field, must sum to 0.

  @param[in] pData Table that will validate the checksum for
  @param[in] Length Size of the pData

  @retval TRUE The table and the checksum sum to 0
  @retval FALSE The table and the checksum not sum to 0
**/
BOOLEAN
IsChecksumValid(
  IN     VOID *pData,
  IN     UINT32 Length
  );

/**
  Get new Sequence Number based on Platform Config Data Config Output table

  We read the last SequenceNumber from Config Output table and increase it by 1. If it will be
  max UINT32 (2^32 - 1) value, then after increasing we will get 0, what is fine.

  @param[in] pDimm         Dimm that contains Config Output table
  @param[out] pSequenceNum Output variable for new Sequence Number
**/
EFI_STATUS
GetNewSequenceNumber(
  IN     struct _DIMM *pDimm,
     OUT UINT32 *pSequenceNum
  );

/**
  Get Platform Config Data table by given type from current config

  @param[in] pCurrentConfig Current Config table
  @param[in] TableType table type to retrieve
**/
VOID *
GetPcatTableFromCurrentConfig(
  IN     NVDIMM_CURRENT_CONFIG *pCurrentConfig,
  IN     UINT8 TableType
  );

/**
  Compare Dimm channel and iMC to determine Dimms order in interleave set

  This function supports 1-way, 2-way, 3-way and 4-way interleave sets

  @param[in] pFirst First item to compare
  @param[in] pSecond Second item to compare

  @retval -1 if first is less than second
  @retval  0 if first is equal to second
  @retval  1 if first is greater than second
**/
INT32
CompareDimmOrderInInterleaveSet(
  IN     VOID *pFirst,
  IN     VOID *pSecond
  );

/**
  Compare Dimm channel and iMC to determine Dimms order in interleave set

  This function supports 6-way interleave set

  @param[in] pFirst First item to compare
  @param[in] pSecond Second item to compare

  @retval -1 if first is less than second
  @retval  0 if first is equal to second
  @retval  1 if first is greater than second
**/
INT32
CompareDimmOrderInInterleaveSet6Way(
  IN     VOID *pFirst,
  IN     VOID *pSecond
  );

/**
  Validate the PCD CIN header

  @param[in] pPcdConfInput Pointer to the PCD CIN Header
  @param[in] pSecond Max allowed size of the PCD OEM Partition

  @retval TRUE if valid
  @retval FALSE if invalid.
**/
BOOLEAN 
IsPcdConfInputHeaderValid(
  IN  NVDIMM_PLATFORM_CONFIG_INPUT *pPcdConfInput, 
  IN  UINT32 PcdOemPartitionSize
);

/**
  Validate the PCD COUT header

  @param[in] pPcdConfOutput Pointer to the PCD COUT Header
  @param[in] pSecond Max allowed size of the PCD OEM Partition

  @retval TRUE if valid
  @retval FALSE if invalid.
**/
BOOLEAN 
IsPcdConfOutputHeaderValid(
  IN  NVDIMM_PLATFORM_CONFIG_OUTPUT *pPcdConfOutput, 
  IN  UINT32 PcdOemPartitionSize
);

/**
  Validate the PCD CCUR header

  @param[in] pPcdCurrentConf Pointer to the PCD CCUR Header
  @param[in] pSecond Max allowed size of the PCD OEM Partition

  @retval TRUE if valid
  @retval FALSE if invalid.
**/
BOOLEAN
IsPcdCurrentConfHeaderValid(
  IN  NVDIMM_CURRENT_CONFIG *pPcdCurrentConf,
  IN  UINT32 PcdOemPartitionSize
);

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif
