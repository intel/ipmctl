/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "PlatformConfigData.h"
#include <Library/PrintLib.h>
#include <Dimm.h>
#include <Region.h>
#include <Version.h>
#include <NvmDimmDriver.h>

#define NUM_OF_DIMMS_IN_SIX_WAY_INTERLEAVE_SET 6

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

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
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  VOID *pCurrentOffset = NULL;
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = NULL;
  NVDIMM_IDENTIFICATION_INFORMATION *pIdentInfo = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pAutoProvExtension = NULL;
  UINT32 ConfInputSize = 0;
  UINT64 LastPersistentMemoryOffset = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  NVDIMM_CONFIGURATION_HEADER *pConfHeader = NULL;
  NVDIMM_CURRENT_CONFIG *pPcdCurrentConf = NULL;
  UINT64 PmPartitionSize = 0;
  INTEL_DIMM_CONFIG *pIntelDIMMConfigEfiVar = NULL;
  INTEL_DIMM_CONFIG *pIntelDIMMConfigIn = NULL;

  NVDIMM_ENTRY();

  if (pDimm == NULL || ppConfigInput == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /**
    Allocate the block of memory for PCD Config Input
  **/
  ConfInputSize =
    sizeof(NVDIMM_PLATFORM_CONFIG_INPUT)
    + sizeof(NVDIMM_PARTITION_SIZE_CHANGE)
    + pDimm->RegionsGoalNum * (sizeof(NVDIMM_INTERLEAVE_INFORMATION));

  for (Index = 0; Index < pDimm->RegionsGoalNum; Index++) {
    ConfInputSize += pDimm->pRegionsGoal[Index]->DimmsNum * sizeof(NVDIMM_IDENTIFICATION_INFORMATION);
  }

  // Retrieve automatic provisioning EFI vars
  Rc = RetrieveIntelDIMMConfig(&pIntelDIMMConfigEfiVar);
  if (EFI_ERROR(Rc)) {
    // Not found or some error
    // Either case should not stop sending CIN
    Rc = EFI_SUCCESS;
  } else {
    if (pIntelDIMMConfigEfiVar->ProvisionCapacityMode == PROVISION_CAPACITY_MODE_AUTO) {
      // Add extension table to size
      ConfInputSize += sizeof(CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE) + sizeof(INTEL_DIMM_CONFIG);
    }
  }


  *ppConfigInput = AllocateZeroPool(ConfInputSize);
  if (*ppConfigInput == NULL) {
    Rc = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  /**
    PCD Configuration Input table
  **/
  (*ppConfigInput)->Header.Signature = NVDIMM_CONFIGURATION_INPUT_SIG;
  (*ppConfigInput)->Header.Length = ConfInputSize;

  /** Populate ther revision of the CIN table **/
  Rc = GetPlatformConfigDataOemPartition(pDimm, &pConfHeader);
#ifdef MEMORY_CORRUPTION_WA
  if (Rc == EFI_DEVICE_ERROR)
  {
	  Rc = GetPlatformConfigDataOemPartition(pDimm, &pConfHeader);
  }
#endif // MEMORY_CORRUPTIO_WA
  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Error in retrieving the Current Config table");
    goto Finish;
  }

  if (pConfHeader->CurrentConfStartOffset == 0 || pConfHeader->CurrentConfDataSize == 0) {
    NVDIMM_DBG("There is no Current Config table");

    if (gNvmDimmData->PMEMDev.pPcatHead == NULL) {
      NVDIMM_DBG("PCAT table not found");
      Rc = EFI_DEVICE_ERROR;
      goto Finish;
    } else if ((gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr->Header.Revision != PCAT_HEADER_REVISION_1) &&
               (gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr->Header.Revision != PCAT_HEADER_REVISION_2)) {
      NVDIMM_DBG("Incorrect PCAT table");
      Rc = EFI_DEVICE_ERROR;
      goto Finish;
     } else {
       (*ppConfigInput)->Header.Revision = gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr->Header.Revision;
     }
  } else {
    pPcdCurrentConf = GET_NVDIMM_CURRENT_CONFIG(pConfHeader);

    if (IsPcdCurrentConfHeaderValid(pPcdCurrentConf, pDimm->PcdOemPartitionSize)) {
      (*ppConfigInput)->Header.Revision = pPcdCurrentConf->Header.Revision;
    } else {
      NVDIMM_DBG("The data in Current Config table is invalid");
      Rc = EFI_DEVICE_ERROR;
      goto Finish;
    }
  }

  CopyMem_S((*ppConfigInput)->Header.OemId, sizeof((*ppConfigInput)->Header.OemId), pConfHeader->Header.OemId, sizeof((*ppConfigInput)->Header.OemId));
  (*ppConfigInput)->Header.OemTableId = pConfHeader->Header.OemTableId;
  (*ppConfigInput)->Header.OemRevision = pConfHeader->Header.OemRevision;
  (*ppConfigInput)->Header.CreatorId = pConfHeader->Header.CreatorId;
  (*ppConfigInput)->Header.CreatorRevision = pConfHeader->Header.CreatorRevision;

  /**
    We read the last SequenceNumber from Config Output table and increase it by 1. If it will be
    max UINT32 (2^32 - 1) value, then after increasing we will get 0, what is fine.
  **/
  Rc = GetNewSequenceNumber(pDimm, &(*ppConfigInput)->SequenceNumber);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }

  /**
    Partition Size Change table
  **/

  pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) &(*ppConfigInput)->pPcatTables;

  pPartSizeChange->Header.Type = PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE;
  pPartSizeChange->Header.Length = sizeof(NVDIMM_PARTITION_SIZE_CHANGE);
  /**
    Dimm Size = PM Size + Volatile Size

    User specifies Volatile Size, but Partition Size Change table needs PartitionSize defines
    size of persistent memory

    So here we specify the Persistent Size Aligned to 32GB. But if there is 0 Volatile,
    we will end up with Persistent Partition with a size under aligned by the reserved regions
    (cause the DIMM Raw capacity has them subtracted). The BIOS team has no problems with this value
    being under aligned right now, but they might change this in the future.
    If they do we will have to align this value properly.
  **/
  pPartSizeChange->PmPartitionSize = pDimm->RawCapacity - pDimm->VolatileSizeGoal;
  pPartSizeChange->PartitionSizeChangeStatus = NVDIMM_CONF_INPUT_PART_SIZE_CHANGE_STATUS;

  /**
    Interleave Information tables
  **/

  pCurrentOffset = (UINT8 *) pPartSizeChange + sizeof(NVDIMM_PARTITION_SIZE_CHANGE);

  for (Index = 0; Index < pDimm->RegionsGoalNum; Index++) {
    pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurrentOffset;

    pInterleaveInfo->Header.Type = PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE;
    pInterleaveInfo->Header.Length =
      (UINT16)sizeof(NVDIMM_INTERLEAVE_INFORMATION)
      + (UINT16)pDimm->pRegionsGoal[Index]->DimmsNum * (UINT16)sizeof(NVDIMM_IDENTIFICATION_INFORMATION);

    pInterleaveInfo->InterleaveSetIndex = pDimm->pRegionsGoal[Index]->InterleaveSetIndex;
    pInterleaveInfo->NumOfDimmsInInterleaveSet = (UINT8)pDimm->pRegionsGoal[Index]->DimmsNum;

    pInterleaveInfo->InterleaveMemoryType = NVDIMM_MEMORY_PERSISTENT_TYPE;

    pInterleaveInfo->InterleaveFormatChannel = pDimm->pRegionsGoal[Index]->ChannelInterleaving;
    pInterleaveInfo->InterleaveFormatImc = pDimm->pRegionsGoal[Index]->ImcInterleaving;

    pInterleaveInfo->InterleaveFormatWays = pDimm->pRegionsGoal[Index]->NumOfChannelWays;
    pInterleaveInfo->MirrorEnable = pDimm->pRegionsGoal[Index]->InterleaveSetType == MIRRORED ? 1 : 0;
    pInterleaveInfo->InterleaveChangeStatus = 0; // Used by Config Output, 0 for Config Input

    pCurrentOffset = (UINT8 *) pCurrentOffset + sizeof(NVDIMM_INTERLEAVE_INFORMATION);

    /**
      Identification Information tables
    **/
    PmPartitionSize = pDimm->pRegionsGoal[Index]->Size / pDimm->pRegionsGoal[Index]->DimmsNum;

    /** Sort Dimms list according to BIOS requirement for Dimms order in interleave set **/
    if (pDimm->pRegionsGoal[Index]->DimmsNum == NUM_OF_DIMMS_IN_SIX_WAY_INTERLEAVE_SET) {
      Rc = BubbleSort(pDimm->pRegionsGoal[Index]->pDimms, pDimm->pRegionsGoal[Index]->DimmsNum,
          sizeof(DIMM *), CompareDimmOrderInInterleaveSet6Way);
    } else {
      Rc = BubbleSort(pDimm->pRegionsGoal[Index]->pDimms, pDimm->pRegionsGoal[Index]->DimmsNum,
          sizeof(DIMM *), CompareDimmOrderInInterleaveSet);
    }
    if (EFI_ERROR(Rc)) {
      goto Finish;
    }

    for (Index2 = 0; Index2 < pDimm->pRegionsGoal[Index]->DimmsNum; Index2++) {
      pIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *) pCurrentOffset;
      if ((*ppConfigInput)->Header.Revision == NVDIMM_CONFIGURATION_TABLES_REVISION_1) {
	pIdentInfo->DimmIdentification.Version1.DimmManufacturerId = pDimm->pRegionsGoal[Index]->pDimms[Index2]->Manufacturer;
	pIdentInfo->DimmIdentification.Version1.DimmSerialNumber = pDimm->pRegionsGoal[Index]->pDimms[Index2]->SerialNumber;
	CopyMem_S(pIdentInfo->DimmIdentification.Version1.DimmPartNumber, sizeof(pIdentInfo->DimmIdentification.Version1.DimmPartNumber), pDimm->pRegionsGoal[Index]->pDimms[Index2]->PartNumber,
	    sizeof(pIdentInfo->DimmIdentification.Version1.DimmPartNumber));
      } else {
        pIdentInfo->DimmIdentification.Version2.Uid.ManufacturerId = pDimm->pRegionsGoal[Index]->pDimms[Index2]->VendorId;
        if (pDimm->pRegionsGoal[Index]->pDimms[Index2]->ManufacturingInfoValid) {
          pIdentInfo->DimmIdentification.Version2.Uid.ManufacturingLocation = pDimm->pRegionsGoal[Index]->pDimms[Index2]->ManufacturingLocation;
          pIdentInfo->DimmIdentification.Version2.Uid.ManufacturingDate = pDimm->pRegionsGoal[Index]->pDimms[Index2]->ManufacturingDate;
        }
        pIdentInfo->DimmIdentification.Version2.Uid.SerialNumber = pDimm->pRegionsGoal[Index]->pDimms[Index2]->SerialNumber;
      }
      pIdentInfo->PmPartitionSize = PmPartitionSize;
      pIdentInfo->PartitionOffset = LastPersistentMemoryOffset;

      pCurrentOffset = (UINT8 *) pCurrentOffset + sizeof(NVDIMM_IDENTIFICATION_INFORMATION);
    }

    LastPersistentMemoryOffset += PmPartitionSize;
  }

  /**
    Extension table for Intel automatic provisioning
   **/
  if (pIntelDIMMConfigEfiVar != NULL) {
    if (pIntelDIMMConfigEfiVar->ProvisionCapacityMode == PROVISION_CAPACITY_MODE_AUTO) {
      pAutoProvExtension = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pCurrentOffset;

      pAutoProvExtension->Header.Type = PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE;
      pAutoProvExtension->Header.Length = sizeof(CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE) +
        sizeof(INTEL_DIMM_CONFIG);
      pAutoProvExtension->VendorId = SPD_INTEL_VENDOR_ID;
      pAutoProvExtension->Guid = gIntelDimmConfigVariableGuid;

      pIntelDIMMConfigIn = (INTEL_DIMM_CONFIG *) pAutoProvExtension->pGuidData;

      pIntelDIMMConfigIn->Revision = INTEL_DIMM_CONFIG_REVISION;
      pIntelDIMMConfigIn->ProvisionCapacityMode = pIntelDIMMConfigEfiVar->ProvisionCapacityMode;
      pIntelDIMMConfigIn->MemorySize = pIntelDIMMConfigEfiVar->MemorySize;
      pIntelDIMMConfigIn->PMType = pIntelDIMMConfigEfiVar->PMType;
      pIntelDIMMConfigIn->ProvisionNamespaceMode = pIntelDIMMConfigEfiVar->ProvisionNamespaceMode;
      pIntelDIMMConfigIn->NamespaceFlags = pIntelDIMMConfigEfiVar->NamespaceFlags;
      pIntelDIMMConfigIn->NamespaceLabelVersion = pIntelDIMMConfigEfiVar->NamespaceLabelVersion;
    }
  }

  /** Generate checksum **/
  GenerateChecksum(*ppConfigInput, (*ppConfigInput)->Header.Length, PCAT_TABLE_HEADER_CHECKSUM_OFFSET);

Finish:
  FREE_POOL_SAFE(pIntelDIMMConfigEfiVar);
  FREE_POOL_SAFE(pConfHeader);
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

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
  )
{
  UINT8 Checksum = 0;
  UINT8 *pByteData = (UINT8 *) pData;
  UINT32 Index = 0;

  if (pData == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    return;
  }

  pByteData[ChecksumOffset] = 0;

  for (Index = 0; Index < Length; Index++) {
    Checksum += pByteData[Index];
  }

  pByteData[ChecksumOffset] = MAX_UINT8_VALUE - Checksum + 1;
}

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
  )
{
  UINT8 Sum = 0;
  UINT8 *pByteData = (UINT8 *) pData;
  UINT32 Index = 0;

  if (pData == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    return FALSE;
  }

  for (Index = 0; Index < Length; Index++) {
    Sum += pByteData[Index];
  }

  if (Sum != 0) {
    NVDIMM_DBG("Checksum(%d) missed by %d", pByteData[PCAT_TABLE_HEADER_CHECKSUM_OFFSET], Sum);
  }

  return (Sum == 0) ? TRUE : FALSE;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  NVDIMM_PLATFORM_CONFIG_OUTPUT *pPcdConfOutput = NULL;

  NVDIMM_ENTRY();

  if (pDimm == NULL || pSequenceNum == NULL) {
    NVDIMM_DBG("NULL pointer provided.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetPlatformConfigDataOemPartition(pDimm, &pPcdConfHeader);
#ifdef MEMORY_CORRUPTION_WA
  if (ReturnCode == EFI_DEVICE_ERROR)
  {
	  ReturnCode = GetPlatformConfigDataOemPartition(pDimm, &pPcdConfHeader);
  }
#endif // MEMORY_CORRUPTIO_WA
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("GetPlatformConfigDataOemPartition returned EFIReturnCode:%d", ReturnCode);
    goto Finish;
  }

  if (pPcdConfHeader->ConfOutputStartOffset == 0 || pPcdConfHeader->ConfOutputDataSize == 0) {
    NVDIMM_DBG("There is no Current Config table");
    *pSequenceNum = 1;
  } else {
    pPcdConfOutput = GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(pPcdConfHeader);

    if (!IsPcdConfOutputHeaderValid(pPcdConfOutput, pDimm->PcdOemPartitionSize )) {
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    *pSequenceNum = pPcdConfOutput->SequenceNumber + 1;
  }

Finish:
  FREE_POOL_SAFE(pPcdConfHeader);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get Platform Config Data table by given type from current config

  @param[in] pCurrentConfig Current Config table
  @param[in] TableType table type to retrieve
**/
VOID *
GetPcatTableFromCurrentConfig(
  IN     NVDIMM_CURRENT_CONFIG *pCurrentConfig,
  IN     UINT8 TableType
  )
{
  VOID *pPartition = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;

  if (pCurrentConfig == NULL ||
    (TableType != PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE && TableType != PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE)) {
    goto Finish;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *) &pCurrentConfig->pPcatTables;
  SizeOfPcatTables = pCurrentConfig->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pCurrentConfig);

  while ((UINT32) ((UINT8 *) pCurPcatTable - (UINT8 *) &pCurrentConfig->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == TableType) {
      pPartition = pCurPcatTable;
      break;
    }
    pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, ((PCAT_TABLE_HEADER *) pCurPcatTable)->Length);
  }

Finish:
  return pPartition;
}

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
  )
{
  DIMM *pDimmFirst = NULL;
  DIMM *pDimmSecond = NULL;

  if (pFirst == NULL || pSecond == NULL) {
    NVDIMM_DBG("NULL pointer found.");
    return 0;
  }

  pDimmFirst = *((DIMM **) pFirst);
  pDimmSecond = *((DIMM **) pSecond);

  if (pDimmFirst->ChannelId < pDimmSecond->ChannelId) {
    return -1;
  } else if (pDimmFirst->ChannelId > pDimmSecond->ChannelId) {
    return 1;
  } else {
    if (pDimmFirst->ImcId < pDimmSecond->ImcId) {
      return -1;
    } else if (pDimmFirst->ImcId > pDimmSecond->ImcId) {
      return 1;
    } else {
      return 0;
    }
  }
}

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
  )
{
  DIMM *pDimmFirst = NULL;
  DIMM *pDimmSecond = NULL;
  UINT16 ParityResultFirst = 0;
  UINT16 ParityResultSecond = 0;

  if (pFirst == NULL || pSecond == NULL) {
    NVDIMM_DBG("NULL pointer found.");
    return 0;
  }

  pDimmFirst = *((DIMM **) pFirst);
  pDimmSecond = *((DIMM **) pSecond);

  /**
    6-way order:
      [CH 0, iMC 0]
      [CH 1, iMC 1]
      [CH 2, iMC 0]
      [CH 0, iMC 1]
      [CH 1, iMC 0]
      [CH 2, iMC 1]
  **/

  ParityResultFirst = (pDimmFirst->ChannelId + pDimmFirst->ImcId) % 2;
  ParityResultSecond = (pDimmSecond->ChannelId + pDimmSecond->ImcId) % 2;

  if (ParityResultFirst < ParityResultSecond) {
    return -1;
  } else if (ParityResultFirst > ParityResultSecond) {
    return 1;
  } else {
    if (pDimmFirst->ChannelId < pDimmSecond->ChannelId) {
      return -1;
    } else if (pDimmFirst->ChannelId > pDimmSecond->ChannelId) {
      return 1;
    } else {
      return 0;
    }
  }
}

/**
  Validate the PCD CIN header

  @param[in] pPcdConfInput Pointer to the PCD CIN Header
  @param[in] pSecond Max allowed size of the PCD OEM Partition

  @retval TRUE if valid
  @retval FALSE if invalid.
**/
BOOLEAN IsPcdConfInputHeaderValid(NVDIMM_PLATFORM_CONFIG_INPUT *pPcdConfInput, UINT32 PcdOemPartitionSize)
{
  if (NULL == pPcdConfInput) {
    NVDIMM_DBG("DIMM Config Input table is NULL");
  }
  else if (pPcdConfInput->Header.Signature != NVDIMM_CONFIGURATION_INPUT_SIG) {
    NVDIMM_DBG("Incorrect signature of the DIMM Config Input table");
  }
  else if (pPcdConfInput->Header.Length > PcdOemPartitionSize) {
    NVDIMM_DBG("Length of PCD Config Input header is greater than max PCD OEM partition size");
  }
  else if (!IsChecksumValid(pPcdConfInput, pPcdConfInput->Header.Length)) {
    NVDIMM_DBG("The checksum of Config Input table is invalid.");
  }
  else if ((pPcdConfInput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
    (pPcdConfInput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    NVDIMM_DBG("Revision of PCD Config Input table is invalid");
  }
  else {
    NVDIMM_DBG("The data in Config Input table is valid.");
    return TRUE;
  }

  return FALSE;
}

/**
  Validate the PCD COUT header

  @param[in] pPcdConfOutput Pointer to the PCD COUT Header
  @param[in] pSecond Max allowed size of the PCD OEM Partition

  @retval TRUE if valid
  @retval FALSE if invalid.
**/
BOOLEAN IsPcdConfOutputHeaderValid(NVDIMM_PLATFORM_CONFIG_OUTPUT *pPcdConfOutput, UINT32 PcdOemPartitionSize)
{
  if (NULL == pPcdConfOutput) {
    NVDIMM_DBG("DIMM Config Output table is NULL");
  }
  else if (pPcdConfOutput->Header.Signature != NVDIMM_CONFIGURATION_OUTPUT_SIG) {
    NVDIMM_DBG("Icorrect signature of the DIMM Config Output table");
  }
  else if (pPcdConfOutput->Header.Length > PcdOemPartitionSize) {
    NVDIMM_DBG("Length of PCD Config Output header is greater than max PCD OEM partition size");
  }
  else if (!IsChecksumValid(pPcdConfOutput, pPcdConfOutput->Header.Length)) {
    NVDIMM_DBG("The checksum of Config Output table is invalid.");
  }
  else if ((pPcdConfOutput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
    (pPcdConfOutput->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    NVDIMM_DBG("Revision of PCD Config Output table is invalid");
  }
  else {
    NVDIMM_DBG("The data in Config Output table is valid.");
    return TRUE;
  }

  return FALSE;
}

/**
  Validate the PCD CCUR header

  @param[in] pPcdCurrentConf Pointer to the PCD CCUR Header
  @param[in] pSecond Max allowed size of the PCD OEM Partition

  @retval TRUE if valid
  @retval FALSE if invalid.
**/
BOOLEAN IsPcdCurrentConfHeaderValid(NVDIMM_CURRENT_CONFIG *pPcdCurrentConf, UINT32 PcdOemPartitionSize)
{
  if (NULL == pPcdCurrentConf) {
    NVDIMM_DBG("DIMM Config Output table is NULL");
  }
  else if (pPcdCurrentConf->Header.Signature != NVDIMM_CURRENT_CONFIG_SIG) {
    NVDIMM_DBG("Incorrect signature of the DIMM Current Config table");
  }
  else if (pPcdCurrentConf->Header.Length > PcdOemPartitionSize) {
    NVDIMM_DBG("Length of PCD Current Config header is greater than max PCD OEM partition size");
  }
  else if ((pPcdCurrentConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
    (pPcdCurrentConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    NVDIMM_DBG("Revision of PCD Current Config table is invalid");
  }
  else if (!IsChecksumValid(pPcdCurrentConf, pPcdCurrentConf->Header.Length)) {
    NVDIMM_DBG("The Current Config table checksum is invalid.");
  }
  else {
    NVDIMM_DBG("The data in Current Config table is valid.");
    return TRUE;
  }

  return FALSE;
}
