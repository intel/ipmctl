/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Dimm.h"
#include "Region.h"
#include "Namespace.h"
#include <Library/PrintLib.h>
#include <Utility.h>
#include <Interleave.h>
#include <NvmWorkarounds.h>
#include <NvmSecurity.h>
#include <Convert.h>

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

STATIC
INT32
CompareRegionOffsetInDimmRegion(
  IN     VOID *pFirst,
  IN     VOID *pSecond
)
{
  DIMM_REGION *pDimmRegion = NULL;
  DIMM_REGION *pDimmRegion2 = NULL;

  if (pFirst == NULL || pSecond == NULL) {
    NVDIMM_DBG("NULL pointer found.");
    return 0;
  }

  pDimmRegion = DIMM_REGION_FROM_NODE(pFirst);
  pDimmRegion2 = DIMM_REGION_FROM_NODE(pSecond);

  if (pDimmRegion->SpaRegionOffset < pDimmRegion2->SpaRegionOffset) {
    return -1;
  }
  else if (pDimmRegion->SpaRegionOffset > pDimmRegion2->SpaRegionOffset) {
    return 1;
  }
  else {
    return 0;
  }
}

/**
  Allocate and initialize the Interleave Set by using NFIT table

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pNvDimmRegionMappingStructure The NVDIMM region that helps describe this region of memory
  @param[in] RegionId The next consecutive region id
  @param[out] ppIS Interleave Set parent for new dimm region

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeISFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionTbl,
  IN     UINT16 RegionId,
  OUT NVM_IS **ppIS
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  InterleaveStruct *pInterleaveTbl = NULL;
  PlatformCapabilitiesTbl *pPlatformCapabilitiesTbl = NULL;

  NVDIMM_ENTRY();

  if (pFitHead == NULL || pNvDimmRegionTbl == NULL || ppIS == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ppIS = AllocateZeroPool(sizeof(NVM_IS));

  if (*ppIS == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (pNvDimmRegionTbl->InterleaveStructureIndex != 0) {
    ReturnCode = GetInterleaveTable(pFitHead, pNvDimmRegionTbl->InterleaveStructureIndex, &pInterleaveTbl);
    if (pInterleaveTbl == NULL) {
      NVDIMM_DBG("InterleaveStructure table with index: %d not found.", pNvDimmRegionTbl->InterleaveStructureIndex);
      goto Finish;
    }
  }

  pPlatformCapabilitiesTbl = pFitHead->ppPlatformCapabilitiesTbles[0];

  InitializeListHead(&((*ppIS)->DimmRegionList));
  InitializeListHead(&((*ppIS)->AppDirectNamespaceList));

  (*ppIS)->Signature = IS_SIGNATURE;
  (*ppIS)->Size = 0;
  (*ppIS)->State = IS_STATE_HEALTHY;
  (*ppIS)->InterleaveSetIndex = pNvDimmRegionTbl->SpaRangeDescriptionTableIndex;
  (*ppIS)->RegionId = RegionId;
  (*ppIS)->SocketId = pNvDimmRegionTbl->DeviceHandle.NfitDeviceHandle.SocketId & MAX_UINT16;
  if (pInterleaveTbl != NULL) {
    (*ppIS)->InterleaveFormatChannel = pInterleaveTbl->LineSize & MAX_UINT16;
    (*ppIS)->InterleaveFormatImc = pInterleaveTbl->LineSize & MAX_UINT16;
  }
  (*ppIS)->InterleaveFormatWays = pNvDimmRegionTbl->InterleaveWays;
  (*ppIS)->MirrorEnable = (pPlatformCapabilitiesTbl->Capabilities & CAPABILITY_MEMORY_MIRROR) ? TRUE : FALSE;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Allocate and initialize the Interleave Set by using Interleave Information table from Platform Config Data

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeIS(
  IN     VOID *pInterleaveInfoTable,
  IN     UINT16 RegionId,
  IN     ACPI_REVISION PcdConfRevision,
     OUT NVM_IS **ppIS
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;

  if (pInterleaveInfoTable == NULL || ppIS == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *ppIS = AllocateZeroPool(sizeof(NVM_IS));

  if (*ppIS == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  InitializeListHead(&((*ppIS)->DimmRegionList));
  InitializeListHead(&((*ppIS)->AppDirectNamespaceList));

  if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdConfRevision)) {
    NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pInterleaveInfoTable;
    (*ppIS)->Signature = IS_SIGNATURE;
    (*ppIS)->Size = 0;
    (*ppIS)->State = IS_STATE_HEALTHY;
    (*ppIS)->InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
    (*ppIS)->RegionId = RegionId;
    (*ppIS)->InterleaveFormatChannel = pInterleaveInfo->InterleaveFormatChannel;
    (*ppIS)->InterleaveFormatImc = pInterleaveInfo->InterleaveFormatImc;
    (*ppIS)->InterleaveFormatWays = pInterleaveInfo->InterleaveFormatWays;
    (*ppIS)->MirrorEnable = pInterleaveInfo->MirrorEnable != 0 ? TRUE : FALSE;
  }
  else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdConfRevision)) {
    NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pInterleaveInfoTable;
    (*ppIS)->Signature = IS_SIGNATURE;
    (*ppIS)->Size = 0;
    (*ppIS)->State = IS_STATE_HEALTHY;
    (*ppIS)->InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
    (*ppIS)->RegionId = RegionId;
    (*ppIS)->InterleaveFormatChannel = pInterleaveInfo->InterleaveFormatChannel;
    (*ppIS)->InterleaveFormatImc = pInterleaveInfo->InterleaveFormatImc;
    GetBitFieldForNumOfChannelWays(pInterleaveInfo->NumOfDimmsInInterleaveSet, &(*ppIS)->InterleaveFormatWays);
    (*ppIS)->MirrorEnable = FALSE;
  }

  return Rc;
}

/**
Create and initialize all Interleave Sets

When something goes wrong with particular Interleave Set then no additional Interleave Set structs created or
error state on Interleave Set is set.

@param[in] pFitHead NVM Firmware Interface Table
@param[in] pDimmList Head of the list of all NVM DIMMs in the system
@param[out] pISList Head of the list for Interleave Sets

@retval EFI_SUCCESS
@retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeISs(
	IN     ParsedFitHeader *pFitHead,
	IN     LIST_ENTRY *pDimmList,
  IN     BOOLEAN UseNfit,
	OUT LIST_ENTRY *pISList
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (pFitHead == NULL || pDimmList == NULL || pISList == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!UseNfit) {
    ReturnCode = RetrieveISsFromPlatformConfigData(pFitHead, pDimmList, pISList);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Retrieving Interleave Sets from the Platform Config Data failed.");
      goto Finish;
    }
  }
  else {
    ReturnCode = RetrieveISsFromNfit(pFitHead, pDimmList, pISList);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Retrieving Interleave Sets from NFIT table failed.");
      goto Finish;
    }
  }

Finish:
  return ReturnCode;
}

/**
  Initialize interleave sets
  It initializes the interleave sets using NFIT or PCD

  @param[in] UseNfit Flag to indicate usage of NFIT or else default to PCD

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeInterleaveSets(
  IN     BOOLEAN UseNfit
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (UseNfit ? !gNvmDimmData->PMEMDev.RegionsNfitInitialized :
    !gNvmDimmData->PMEMDev.RegionsAndNsInitialized) {
    ReturnCode = InitializeISs(gNvmDimmData->PMEMDev.pFitHead, &gNvmDimmData->PMEMDev.Dimms,
      UseNfit, (UseNfit ? &gNvmDimmData->PMEMDev.ISsNfit : &gNvmDimmData->PMEMDev.ISs));
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve the REGION list, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    }
    else {
      if (!UseNfit) {
        gNvmDimmData->PMEMDev.RegionsAndNsInitialized = TRUE;
      }
      else {
        gNvmDimmData->PMEMDev.RegionsNfitInitialized = TRUE;
      }
    }
  }

  return ReturnCode;
}

/**
  Get Region by ID
  Scan the Region list for a Region identified by ID

  @param[in] pRegionList Head of the list for Regions
  @param[in] RegionId Region identifier

  @retval NVM_IS struct pointer if matching Region has been found
  @retval NULL pointer if not found
**/
NVM_IS *
GetRegionById(
  IN     LIST_ENTRY *pRegionList,
  IN     UINT16 RegionId
  )
{
  NVM_IS *pRegion = NULL;
  NVM_IS *pTargetRegion = NULL;
  LIST_ENTRY *pNode = NULL;

  NVDIMM_ENTRY();
  LIST_FOR_EACH(pNode, pRegionList) {
    pRegion = IS_FROM_NODE(pNode);
    if (pRegion->RegionId == RegionId) {
      pTargetRegion = pRegion;
      break;
    }
  }

  NVDIMM_EXIT();
  return pTargetRegion;
}

/**
  Get Region List
  Retruns the pointer to the region list.
  It is also initializing the region list if it is necessary.

  @param[in] pRegionList Head of the list for Regions
  @param[in] UseNfit Flag to indicate usage of NFIT

  @retval pointer to the region list
**/
EFI_STATUS
GetRegionList(
  IN     LIST_ENTRY **ppRegionList,
  IN     BOOLEAN UseNfit
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (UseNfit ? !gNvmDimmData->PMEMDev.RegionsNfitInitialized :
    !gNvmDimmData->PMEMDev.RegionsAndNsInitialized) {
    ReturnCode = InitializeISs(gNvmDimmData->PMEMDev.pFitHead, &gNvmDimmData->PMEMDev.Dimms,
      UseNfit, (UseNfit ? &gNvmDimmData->PMEMDev.ISsNfit : &gNvmDimmData->PMEMDev.ISs));
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve the REGION list, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    }
    else {
      if (!UseNfit) {
        gNvmDimmData->PMEMDev.RegionsAndNsInitialized = TRUE;
      }
      else {
        gNvmDimmData->PMEMDev.RegionsNfitInitialized = TRUE;
      }
    }
  }

  if (NULL != ppRegionList) {
    if (!UseNfit) {
      *ppRegionList = &gNvmDimmData->PMEMDev.ISs; //IS is region
    }
    else {
      *ppRegionList = &gNvmDimmData->PMEMDev.ISsNfit;
    }
  }
  return ReturnCode;
}

/**
  Clean the Interleave Set

  @param[in, out] pDimmList: the list of DCPMMs
  @param[in, out] pISList: the list of Interleave Sets to clean
**/
VOID
CleanISLists(
  IN OUT LIST_ENTRY *pDimmList,
  IN OUT LIST_ENTRY *pISList
  )
{
  NVM_IS *pIS = NULL;
  LIST_ENTRY *pISNode = NULL;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;

  NVDIMM_ENTRY();

  if (pDimmList == NULL || pISList == NULL) {
    goto Finish;
  }

  /** Free Interleave Sets and Dimm Regions. Remove them from the Interleave Set list. **/
  while (!IsListEmpty(pISList)) {
    pISNode = GetFirstNode(pISList);
    pIS = IS_FROM_NODE(pISNode);
    RemoveEntryList(pISNode);
    FreeISResources(pIS);
  }

  /** Clean pointers in Dimms **/
  LIST_FOR_EACH(pDimmNode, pDimmList) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    pDimm->ISsNum = 0;
    pDimm->IsRegionsNum = 0;
    pDimm->ISsNfitNum = 0;
    pDimm->IsRegionsNfitNum = 0;
  }

Finish:
  NVDIMM_EXIT();
}

/**
  Free a Interleave Set and all memory resources in use by the Interleave Set.

  @param[in, out] pIS the Interleave Set and its regions that will be released
**/
VOID
FreeISResources(
  IN OUT NVM_IS *pIS
  )
{
  DIMM_REGION *pDimmRegion = NULL;
  LIST_ENTRY *pDimmRegionNode = NULL;
  LIST_ENTRY *pDimmRegionNextNode = NULL;

  NVDIMM_ENTRY();

  if (pIS == NULL) {
    goto Finish;
  }

  /** Free regions which the Interleave Set is composed from **/
  LIST_FOR_EACH_SAFE(pDimmRegionNode, pDimmRegionNextNode, &pIS->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pDimmRegionNode);

    RemoveEntryList(pDimmRegionNode);
    FreePool(pDimmRegion);
  }

  FreePool(pIS);

Finish:
  NVDIMM_EXIT();
}

/**
  Allocate and initialize the dimm region by using NFIT table

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimm Target DIMM structure pointer
  @param[in] pISList List of interleaveset formed so far
  @param[in] pNvDimmRegionMappingStructure The NVDIMM region that helps describe this region of memory
  @param[out] pRegionId The next consecutive region id
  @param[out] ppNewIS Interleave Set parent for new dimm region
  @param[out] ppDimmRegion new allocated dimm region will be put here
  @param[out] pISDimmRegionAlreadyExists TRUE if Interleave Set DIMM region already exists

  @retval EFI_SUCCESS
  @retval EFI_NOT_FOUND the Dimm related with DimmRegion has not been found on the Dimm list
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeDimmRegionFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN     DIMM *pDimm,
  IN     LIST_ENTRY *pISList,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure,
  OUT    UINT16 *pRegionId,
  OUT    NVM_IS **ppCurrentIS,
  OUT    DIMM_REGION **ppDimmRegion,
  OUT    BOOLEAN *pISDimmRegionAlreadyExists
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pISNode = NULL;
  NVM_IS *pExistingIS = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  LIST_ENTRY *pDimmRegionNode = NULL;
  NVM_IS *pNewIS = NULL;
  BOOLEAN ISAlreadyExists = FALSE;

  NVDIMM_ENTRY();

  if (pDimm == NULL || pISList == NULL || pNvDimmRegionMappingStructure == NULL || pRegionId == NULL ||
    ppDimmRegion == NULL || ppCurrentIS == NULL || pISDimmRegionAlreadyExists == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pISDimmRegionAlreadyExists = FALSE;

  /**
  Check if Interleave Set already exists for this Interleave Set Index
  **/
  LIST_FOR_EACH(pISNode, pISList) {
    pExistingIS = IS_FROM_NODE(pISNode);
    if (pExistingIS->InterleaveSetIndex == pNvDimmRegionMappingStructure->SpaRangeDescriptionTableIndex) {
      ISAlreadyExists = TRUE;
      *ppCurrentIS = pExistingIS;
      LIST_FOR_EACH(pDimmRegionNode, &pExistingIS->DimmRegionList) {
        pDimmRegion = DIMM_REGION_FROM_NODE(pDimmRegionNode);
        if (pDimm->SerialNumber == pDimmRegion->pDimm->SerialNumber) {
          *pISDimmRegionAlreadyExists = TRUE;
          goto Finish;
        }
      }
    }
  }
  if (!(*pISDimmRegionAlreadyExists)) {
    if (!ISAlreadyExists) {
      ReturnCode = InitializeISFromNfit(pFitHead, pNvDimmRegionMappingStructure, *pRegionId, &pNewIS);
      if (EFI_ERROR(ReturnCode) || pNewIS == NULL) {
        FREE_POOL_SAFE(pNewIS);
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      (*pRegionId)++;
      InsertTailList(pISList, &(pNewIS->IsNode));
      *ppCurrentIS = pNewIS;
    }

    *ppDimmRegion = AllocateZeroPool(sizeof(DIMM_REGION));

    if (*ppDimmRegion == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    (*ppDimmRegion)->pDimm = pDimm;

    if (MAX_IS_PER_DIMM <= pDimm->ISsNfitNum) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    pDimm->pISsNfit[pDimm->ISsNfitNum] = *ppCurrentIS;
    pDimm->ISsNfitNum++;

    (*ppDimmRegion)->Signature = DIMM_REGION_SIGNATURE;
    (*ppDimmRegion)->PartitionOffset = pNvDimmRegionMappingStructure->NvDimmPhysicalAddressRegionBase - pDimm->PmStart;
    (*ppDimmRegion)->PartitionSize = pNvDimmRegionMappingStructure->NvDimmRegionSize;
  }
  else {
    *ppCurrentIS = NULL;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Allocate and initialize the dimm region by using Interleave Information table from Platform Config Data

  @param[in] pCurDimm the DIMM from which Interleave Information table was retrieved
  @param[in] pDimmList Head of the list of all Intel NVM Dimm in the system
  @param[in] pISList List of interleaveset formed so far
  @param[in] pIdentificationInfoTable Identification Information table
  @param[in] pInterleaveInfoTable Interleave information for the particular dimm
  @param[in] PcdConfRevision Revision of the PCD Config tables
  @param[out] pRegionId The next consecutive region id
  @param[out] ppNewIS Interleave Set parent for new dimm region
  @param[out] ppDimmRegion new allocated dimm region will be put here
  @param[out] pISAlreadyExists TRUE if Interleave Set already exists

  @retval EFI_SUCCESS
  @retval EFI_NOT_FOUND the Dimm related with DimmRegion has not been found on the Dimm list
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeDimmRegion(
  IN     DIMM *pCurDimm,
	IN     LIST_ENTRY *pDimmList,
	IN     LIST_ENTRY *pISList,
	IN     VOID *pIdentificationInfoTable,
	IN     VOID *pInterleaveInfoTable,
	IN     ACPI_REVISION PcdConfRevision,
	OUT    UINT16 *pRegionId,
	OUT    NVM_IS **ppNewIS,
	OUT    DIMM_REGION **ppDimmRegion,
	OUT    BOOLEAN *pISAlreadyExists
)
{
	EFI_STATUS Rc = EFI_SUCCESS;
	DIMM *pDimm = NULL;
	UINT16 ManufacturerInPcd = 0;
	UINT32 SerialNumberInPcd = 0;
	LIST_ENTRY *pISNode = NULL;
	NVM_IS *pExistingIS = NULL;
	DIMM_REGION *pDimmRegion = NULL;
	LIST_ENTRY *pDimmRegionNode = NULL;
	DIMM_UNIQUE_IDENTIFIER DimmUidInPcd;
	UINT16 InterleaveSetIndex = 0;
	UINT64 PartitionOffset = 0;
	UINT64 PartitionSize = 0;

	NVDIMM_ENTRY();

	ZeroMem(&DimmUidInPcd, sizeof(DimmUidInPcd));

	if (pCurDimm == NULL || pDimmList == NULL || pISList == NULL || pIdentificationInfoTable == NULL ||
    pInterleaveInfoTable == NULL || pRegionId == NULL || ppDimmRegion == NULL || ppNewIS == NULL || pISAlreadyExists == NULL) {
		Rc = EFI_INVALID_PARAMETER;
		goto Finish;
	}

	*pISAlreadyExists = FALSE;
	if (IS_ACPI_REV_INVALID(PcdConfRevision)) {
		Rc = EFI_INVALID_PARAMETER;
		NVDIMM_DBG("Error: Invalid revision value %d for PCD config table.", PcdConfRevision);
		goto Finish;
	}

	if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdConfRevision)) {
		NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pInterleaveInfoTable;
		NVDIMM_IDENTIFICATION_INFORMATION *pIdentificationInfo = (NVDIMM_IDENTIFICATION_INFORMATION *)pIdentificationInfoTable;
		if (IS_ACPI_REV_MAJ_0_MIN_1(PcdConfRevision)) {
			ManufacturerInPcd = pIdentificationInfo->DimmIdentification.Version1.DimmManufacturerId;
			SerialNumberInPcd = pIdentificationInfo->DimmIdentification.Version1.DimmSerialNumber;
		}
		else {
			CopyMem_S(&DimmUidInPcd, sizeof(DimmUidInPcd), &pIdentificationInfo->DimmIdentification.Version2.Uid, sizeof(DIMM_UNIQUE_IDENTIFIER));
			ManufacturerInPcd = DimmUidInPcd.ManufacturerId;
			SerialNumberInPcd = DimmUidInPcd.SerialNumber;
		}
		InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
		PartitionOffset = pIdentificationInfo->PartitionOffset;
		PartitionSize = pIdentificationInfo->PmPartitionSize;
	}
	else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdConfRevision)) {
		NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pInterleaveInfoTable;
		NVDIMM_IDENTIFICATION_INFORMATION3 *pIdentificationInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *)pIdentificationInfoTable;
    CopyMem_S(&DimmUidInPcd, sizeof(DimmUidInPcd), &pIdentificationInfo->DimmIdentification, sizeof(DIMM_UNIQUE_IDENTIFIER));
    ManufacturerInPcd = DimmUidInPcd.ManufacturerId;
    SerialNumberInPcd = DimmUidInPcd.SerialNumber;

		InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;
		PartitionOffset = pIdentificationInfo->PartitionOffset;
		PartitionSize = pIdentificationInfo->PmPartitionSize;
	}

  if (ManufacturerInPcd == 0 || SerialNumberInPcd == 0) {
    NVDIMM_DBG("Serial or manufacturer number in the Identification Information table is equal to 0.");
    Rc = EFI_DEVICE_ERROR;
    goto Finish;
  }

	if (IS_ACPI_REV_MAJ_0_MIN_1(PcdConfRevision)) {
		pDimm = GetDimmBySerialNumber(pDimmList, SerialNumberInPcd);
	}
	else {
		pDimm = GetDimmByUniqueIdentifier(pDimmList, DimmUidInPcd);
	}
	if (pDimm == NULL) {
		Rc = EFI_NOT_FOUND;
		NVDIMM_DBG("Dimm not found using the Identification Information table");
		goto Finish;
	}
	/**
	 Check if Interleave Set already exists for this Interleave Set Index
   **/
	LIST_FOR_EACH(pISNode, pISList) {
		pExistingIS = IS_FROM_NODE(pISNode);
		if (pExistingIS->InterleaveSetIndex == InterleaveSetIndex) {
			LIST_FOR_EACH(pDimmRegionNode, &pExistingIS->DimmRegionList) {
				pDimmRegion = DIMM_REGION_FROM_NODE(pDimmRegionNode);
				/**
				  Addressing the corner case where a dimm is moved from another system and has the same interleaveset index
				**/
				if (pDimm->SerialNumber == pDimmRegion->pDimm->SerialNumber) {
					*pISAlreadyExists = TRUE;
					goto Finish;
				}
			}
		}
	}
	if (!(*pISAlreadyExists)) {
		/* As this method is called inside the for loop for each dimm Interleave Information table,
		  it could be that ppNewIS is initialized the first time. Ignore it if it is already intialized and added to the list. Avoiding duplicates.*/
		if (*ppNewIS == NULL) {
			Rc = InitializeIS(pInterleaveInfoTable, *pRegionId, PcdConfRevision, ppNewIS);
			if (EFI_ERROR(Rc) || *ppNewIS == NULL) {
				Rc = EFI_OUT_OF_RESOURCES;
				goto Finish;
			}
			(*pRegionId)++;
			InsertTailList(pISList, &((*ppNewIS)->IsNode));
		}

    *ppDimmRegion = AllocateZeroPool(sizeof(DIMM_REGION));

    if (*ppDimmRegion == NULL) {
      Rc = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    (*ppDimmRegion)->pDimm = pDimm;

    /**
      Insert only mapped/healthy regions into pDimm->pISs array.
      PCD is not updated by BIOS on non-functional DIMMS. So
      non-functional DIMMs need to be excluded to avoid false
      indication of being in configured state.
    **/
    if (pCurDimm->Configured && !pCurDimm->NonFunctional) {
      if (MAX_IS_PER_DIMM <= pDimm->ISsNum) {
        Rc = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      pDimm->pISs[pDimm->ISsNum] = *ppNewIS;
      pDimm->ISsNum++;
    }

    (*ppDimmRegion)->Signature = DIMM_REGION_SIGNATURE;
    (*ppDimmRegion)->PartitionOffset = PartitionOffset;
    (*ppDimmRegion)->PartitionSize = PartitionSize;
	}
Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Retrieve Interleave Sets by using NFIT table

  Using the parsed NFIT table data to get information about Interleave Sets configuration.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pISList Head of the list for Interleave Sets

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveISsFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
     OUT LIST_ENTRY *pISList
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  DIMM_REGION *pNewDimmRegion = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  NVM_IS *pIS = NULL;
  NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure = NULL;
  LIST_ENTRY *pISNode = NULL;
  BOOLEAN UseLatestVersion = FALSE;
  BOOLEAN ISDimmRegionAlreadyExists = FALSE;
  UINT32 Index = 0;
  UINT16 RegionId = 1; // region id  used internally to distinguish different regions.
  UINT32 IsRegionIndex = 0;
  UINT32 NumOfDimmsInInterleaveSet = 0;

  NVDIMM_ENTRY();

  for (Index = 0; Index < pFitHead->NvDimmRegionMappingStructuresNum; Index++) {
    // Look for NVDIMM regions which have a SPA mapping for PM region type
    ReturnCode = GetNvDimmRegionMappingStructureForPid(pFitHead, pFitHead->ppNvDimmRegionMappingStructures[Index]->NvDimmPhysicalId, &gSpaRangePmRegionGuid, TRUE,
      pFitHead->ppNvDimmRegionMappingStructures[Index]->SpaRangeDescriptionTableIndex, &pNvDimmRegionMappingStructure);
    if (ReturnCode == EFI_NOT_FOUND) {
      NVDIMM_WARN("No NVDIMM region table found with SPA Range PM Region GUID");
      ReturnCode = EFI_SUCCESS;
      continue;
    }
    else if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    pDimm = GetDimmByPid(pFitHead->ppNvDimmRegionMappingStructures[Index]->NvDimmPhysicalId, pDimmList);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    ReturnCode = InitializeDimmRegionFromNfit(pFitHead, pDimm, pISList, pNvDimmRegionMappingStructure, &RegionId, &pIS, &pNewDimmRegion, &ISDimmRegionAlreadyExists);

    if (pIS == NULL) {
      FREE_POOL_SAFE(pNewDimmRegion);
      goto Finish;
    }
    if (ISDimmRegionAlreadyExists) {
      continue;
    }
    if (EFI_ERROR(ReturnCode) || pNewDimmRegion == NULL) {
      pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_INIT_FAILURE);
      NVDIMM_DBG("One of parameters was NULL or out of memory");
    }
    else {
      InsertTailList(&pIS->DimmRegionList, &pNewDimmRegion->DimmRegionNode);

      IsRegionIndex = pNewDimmRegion->pDimm->IsRegionsNfitNum;
      if (MAX_IS_PER_DIMM <= IsRegionIndex) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      pNewDimmRegion->pDimm->pIsRegionsNfit[IsRegionIndex] = pDimmRegion;
      pNewDimmRegion->pDimm->IsRegionsNfitNum = IsRegionIndex + 1;

      pIS->Size += pNewDimmRegion->PartitionSize;
    }
  }

  LIST_FOR_EACH(pISNode, pISList) {
    pIS = IS_FROM_NODE(pISNode);

    if (pIS == NULL) {
      continue;
    }

    // Check if any interleave set is broken
    ReturnCode = GetListSize(&pIS->DimmRegionList, &NumOfDimmsInInterleaveSet);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (NumOfDimmsInInterleaveSet < pIS->InterleaveFormatWays) {
      pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_DIMM_MISSING);
      NVDIMM_DBG("The Dimm related with the DimmRegion has not been found on the Dimm list");
    }

    // AppDirect Mapping already exists
    if (pIS->pSpaTbl != NULL) {
      continue;
    }

    ReturnCode = RetrieveAppDirectMappingFromNfit(pFitHead, pIS);
    if (EFI_ERROR(ReturnCode)) {
      pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_SPA_MISSING);
      NVDIMM_DBG("Couldn't retrieve AppDirect I/O structures from NFIT.");
    }

    ReturnCode = UseLatestNsLabelVersion(pIS, NULL, &UseLatestVersion);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (UseLatestVersion) {
      ReturnCode = CalculateISetCookie(pFitHead, pIS);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
    }
    else {
      ReturnCode = CalculateISetCookieVer1_1(pFitHead, pIS);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
    }

    ReturnCode = BubbleSortLinkedList(&pIS->DimmRegionList, CompareRegionOffsetInDimmRegion);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to sort DIMM regions in interleave set: 0x%x", pIS->InterleaveSetIndex);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve Interleave Sets by using Platform Config Data from Intel manageable NVM Dimms

  Using the Platform Config Data command to get information about Interleave Sets configuration.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pISList Head of the list for Interleave Sets
  @param[out] pRegionList Head of the list for Regions

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveISsFromPlatformConfigData(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
     OUT LIST_ENTRY *pISList
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS IReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  NVDIMM_CURRENT_CONFIG *pPcdCurrentConf = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  UINT16 RegionId = 1; // region id  used internally to distinguish different regions. Will be used when creating namespace.

  if (pDimmList == NULL || pISList == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  LIST_FOR_EACH(pDimmNode, pDimmList) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    // Set default values
    pDimm->ConfigStatus = DIMM_CONFIG_UNDEFINED;
    pDimm->IsNew = 0;
    pDimm->Configured = FALSE;

    if (!IsDimmManageable(pDimm) || DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask)) {
      continue;
    }

    // Free previous use of pcd header if needed
    FREE_POOL_SAFE(pPcdConfHeader);
    ReturnCode = GetPlatformConfigDataOemPartition(pDimm, FALSE, &pPcdConfHeader);
#ifdef MEMORY_CORRUPTION_WA
    if (ReturnCode == EFI_DEVICE_ERROR) {
      ReturnCode = GetPlatformConfigDataOemPartition(pDimm, FALSE, &pPcdConfHeader);
    }
#endif // MEMORY_CORRUPTIO_WA
    if (EFI_ERROR(ReturnCode)) {
      // Ignore all errors except for PMem module busy with sanitize operation
      if (EFI_NO_RESPONSE == ReturnCode) {
        /* Save the return code here and continue with the execution for rest of the dimms.
          This is done to make the UEFI initialization succeed. During UEFI init,
          return code will be ignored but we have to error out when the actual command is executed. */
        IReturnCode = ReturnCode;
      }
      ReturnCode = EFI_SUCCESS;
      continue;
    }

    if (pPcdConfHeader->CurrentConfStartOffset == 0 || pPcdConfHeader->CurrentConfDataSize == 0) {
      NVDIMM_DBG("There is no Current Config table");
      continue;
    }

    pPcdCurrentConf = GET_NVDIMM_CURRENT_CONFIG(pPcdConfHeader);

    if (!IsPcdCurrentConfHeaderValid(pPcdCurrentConf, pDimm->PcdOemPartitionSize)) {
      continue;
    }

    pDimm->ConfigStatus = (UINT8)pPcdCurrentConf->ConfigStatus;
    pDimm->IsNew = (pDimm->ConfigStatus == DIMM_CONFIG_NEW_DIMM) ? 1 : 0;

    switch (pPcdCurrentConf->ConfigStatus) {
      case DIMM_CONFIG_SUCCESS:
      case DIMM_CONFIG_OLD_CONFIG_USED:
      case DIMM_CONFIG_PM_MAPPED_VM_POPULATION_ISSUE:
        pDimm->Configured = TRUE;
        break;
      default:
        pDimm->Configured = FALSE;
        break;
    }

    pDimm->MappedVolatileCapacity = pPcdCurrentConf->VolatileMemSizeIntoSpa;
    pDimm->MappedPersistentCapacity = pPcdCurrentConf->PersistentMemSizeIntoSpa;

    pCurPcatTable = (PCAT_TABLE_HEADER *) &pPcdCurrentConf->pPcatTables;
    SizeOfPcatTables = pPcdConfHeader->CurrentConfDataSize - (UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)pPcdCurrentConf);

    /**
      Example of the use of the while loop condition
      Extension table #1   offset:  0   size: 10
      Extension table #2   offset: 10   size:  5
      Size of extension tables: 15 (10 + 5)

      Iteration #1:   offset: 0
      Iteration #2:   offset: 10
      Iteration #3:   offset: 15   stop the loop: offset isn't less than size
    **/
    while ((UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *) &pPcdCurrentConf->pPcatTables) < SizeOfPcatTables) {
      if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
        if (IS_ACPI_HEADER_REV_MAJ_0_MIN_VALID(pPcdCurrentConf)) {
          NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *) pCurPcatTable;
          RetrieveISFromInterleaveInformationTable(pFitHead, pDimmList, pInterleaveInfo,
            pPcdCurrentConf->Header.Revision, pDimm, &RegionId,
            pISList);

          pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
        } else if (IS_ACPI_HEADER_REV_MAJ_1_MIN_VALID(pPcdCurrentConf)) {
          NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pCurPcatTable;
          RetrieveISFromInterleaveInformationTable(pFitHead, pDimmList, pInterleaveInfo,
            pPcdCurrentConf->Header.Revision, pDimm, &RegionId,
            pISList);

          pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pInterleaveInfo->Header.Length);
        }
      } else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
        pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pCurPcatTable;

        pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
      } else {
        NVDIMM_DBG("This type (%d) of PCAT table shouldn't be contained in Current Configuration table",
          pCurPcatTable->Type);
        ReturnCode = EFI_DEVICE_ERROR;
        break;
      }
    }

    FREE_POOL_SAFE(pPcdConfHeader);
    pPcdConfHeader = NULL;
  }
  FREE_POOL_SAFE(pPcdConfHeader);

  return IReturnCode != EFI_SUCCESS ? IReturnCode : ReturnCode;
}

/**
  Parse Interleave Information table and create a Interleave Set if it doesn't exist yet.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all Intel NVM Dimms in the system
  @param[in] pInterleaveInfo Interleave Information table retrieve from DIMM
  @param[in] PcdCurrentConfRevision PCD Current Config table revision
  @param[in] pDimm the DIMM from which Interleave Information table was retrieved
  @param[in out] pRegionId Unique id for region
  @param[out] pISList Head of the list for Interleave Sets

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveISFromInterleaveInformationTable(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
  IN     VOID *pInterleaveInfoTable,
  IN     ACPI_REVISION PcdCurrentConfRevision,
  IN     DIMM *pDimm,
  IN OUT UINT16 *pRegionId,
     OUT LIST_ENTRY *pISList
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVM_IS *pIS = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  UINT32 Index = 0;
  UINT32 IsRegionIndex = 0;
  VOID *pCurrentIdentInfo = NULL;
  BOOLEAN UseLatestVersion = FALSE;
  BOOLEAN ISAlreadyExists = FALSE;
  UINT8 NumOfDimmsInInterleaveSet = 0;

  NVDIMM_ENTRY();

  if (pFitHead == NULL || pDimmList == NULL || pInterleaveInfoTable == NULL ||
    pDimm == NULL || pRegionId  == NULL || pISList == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /**
    Initialize Interleave Set and Dimm Regions
  **/
  if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdCurrentConfRevision)) {
    NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pInterleaveInfoTable;
    pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *) &pInterleaveInfo->pIdentificationInfoList;
    NumOfDimmsInInterleaveSet = pInterleaveInfo->NumOfDimmsInInterleaveSet;
  } else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdCurrentConfRevision)) {
    NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pInterleaveInfoTable;
    pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *) &pInterleaveInfo->pIdentificationInfoList;
    NumOfDimmsInInterleaveSet = pInterleaveInfo->NumOfDimmsInInterleaveSet;
  }

    for (Index = 0; Index < NumOfDimmsInInterleaveSet; Index++) {
      Rc = InitializeDimmRegion(pDimm, pDimmList, pISList, pCurrentIdentInfo, pInterleaveInfoTable, PcdCurrentConfRevision, pRegionId, &pIS, &pDimmRegion, &ISAlreadyExists);
      // pIS will be null when the IS already exist or when there is no memory to do malloc. In either case go to Finish.
      if (ISAlreadyExists || pIS == NULL) {
        goto Finish;
      }
      if ((EFI_ERROR(Rc) && Rc != EFI_NOT_FOUND) || pDimmRegion == NULL) {
        pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_INIT_FAILURE);
        NVDIMM_DBG("One of parameters was NULL or out of memory");
      } else {
        if (Rc == EFI_NOT_FOUND) {
          pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_DIMM_MISSING);
          NVDIMM_DBG("The Dimm related with the DimmRegion has not been found on the Dimm list");
        } else {
          InsertTailList(&pIS->DimmRegionList, &pDimmRegion->DimmRegionNode);

          IsRegionIndex = pDimmRegion->pDimm->IsRegionsNum;

          /**
            Insert only mapped/healthy DimmRegions into pDimm->pIsRegions array.
            PCD is not updated by BIOS on non-functional DIMMS. So
            non-functional DIMMs need to be excluded to avoid false
            indication of being in configured state.
          **/
          if (pDimm->Configured && !pDimm->NonFunctional) {
            if (MAX_IS_PER_DIMM <= IsRegionIndex) {
              Rc = EFI_OUT_OF_RESOURCES;
              goto Finish;
            }
            pDimmRegion->pDimm->pIsRegions[IsRegionIndex] = pDimmRegion;
            pDimmRegion->pDimm->IsRegionsNum = IsRegionIndex + 1;
          }

          pIS->Size += pDimmRegion->PartitionSize;
        }
      }
      if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdCurrentConfRevision)) {
        pCurrentIdentInfo = (UINT8 *)pCurrentIdentInfo + sizeof(NVDIMM_IDENTIFICATION_INFORMATION);
      } else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdCurrentConfRevision)) {
        pCurrentIdentInfo = (UINT8 *)pCurrentIdentInfo + sizeof(NVDIMM_IDENTIFICATION_INFORMATION3);
      }
    }

    if (pIS != NULL && !pDimm->Configured) {
      pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_CONFIG_INACTIVE);
    }

    Rc = RetrieveAppDirectMappingFromNfit(pFitHead, pIS);
    if (pIS != NULL) {
      if (EFI_ERROR(Rc)) {
        pIS->State = SetISStateWithPriority(pIS->State, IS_STATE_SPA_MISSING);
        NVDIMM_DBG("Couldn't retrieve AppDirect I/O structures from NFIT.");
      }

      pIS->SocketId = pDimm->SocketId;

      Rc = UseLatestNsLabelVersion(pIS, NULL, &UseLatestVersion);
      if (EFI_ERROR(Rc)) {
        goto Finish;
      }

      if (UseLatestVersion) {
        Rc = CalculateISetCookie(pFitHead, pIS);
        if (EFI_ERROR(Rc)) {
          goto Finish;
        }
      }
      else {
        Rc = CalculateISetCookieVer1_1(pFitHead, pIS);
        if (EFI_ERROR(Rc)) {
          goto Finish;
        }
      }

      Rc = BubbleSortLinkedList(&pIS->DimmRegionList, CompareRegionOffsetInDimmRegion);

      if (EFI_ERROR(Rc)) {
        goto Finish;
      }
    }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
Determine Region Type based on the Interleave sets

@param[in, out] pRegion    The region whose type needs to be determined

@retval EFI_SUCCESS
@retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
DetermineRegionType(
  IN NVM_IS *pRegion,
  OUT UINT8 *pRegionType
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 DimmCount = 0;

  NVDIMM_ENTRY();

  if (pRegion == NULL || NULL == pRegionType) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pRegionType = 0;


  ReturnCode = GetListSize(&pRegion->DimmRegionList, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Each interleave set will provide region type **/
  if (DimmCount == 1) {
    *pRegionType |= PM_TYPE_AD_NI;
  } else if (DimmCount > 1) {
    *pRegionType |= PM_TYPE_AD;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
Calculate free Region capacity

@param[in]  pRegion       Region that a free capacity will be calculated for
@param[out] pFreeCapacity Output parameter for result

@retval EFI_SUCCESS
@retval EFI_INVALID_PARAMETER one or more parameters are NULL
@retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
GetFreeRegionCapacity(
  IN  NVM_IS *pRegion,
  OUT UINT64 *pFreeCapacity
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pNode = NULL;
  UINT64 NamespaceCapacityUsed = 0;
  NAMESPACE *pNamespace = NULL;

  NVDIMM_ENTRY();

  if (pRegion == NULL || pFreeCapacity == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  LIST_FOR_EACH(pNode, &pRegion->AppDirectNamespaceList) {
    pNamespace = NAMESPACE_FROM_NODE(pNode, IsNode);
    NamespaceCapacityUsed += GetRawCapacity(pNamespace);
  }

  *pFreeCapacity = pRegion->Size - NamespaceCapacityUsed;

Finish:

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Determine Regions health based on health state of Interleave Sets

  @param[in] pRegion    The region whose health is to be determined

  @param[out] pHealthState The health state of the region
  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Could not allocate memory
**/
EFI_STATUS
DetermineRegionHealth(
  IN  NVM_IS *pRegion,
  OUT UINT16 *pHealthState
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BOOLEAN IsLocked = FALSE;
  BOOLEAN HasNewGoal = FALSE;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pNode = NULL;
  DIMM_REGION *pDimmRegion = NULL;

  NVDIMM_ENTRY();

  if ((pRegion == NULL) || (pHealthState == NULL)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pHealthState = RegionHealthStateNormal;

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishAdvance;
  }

  LIST_FOR_EACH(pNode, &pRegion->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pNode);
    pDimm = pDimmRegion->pDimm;

    if (!IsDimmManageable(pDimm) || DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask))
    {
      *pHealthState = RegionHealthStateError;
      break;
    }

    /** Check if any of the DIMMs are locked **/
    ReturnCode = IsDimmLocked(pDimm, &IsLocked);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishAdvance;
    }

    if (IsLocked) {
      *pHealthState = RegionHealthStateLocked;
      break;
    }

    /** Check if any of the DIMMs have a config goal created, but not yet applied **/
    ReturnCode = FindIfNewGoalOnDimm(pDimm, &HasNewGoal);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishAdvance;
    }

    if (HasNewGoal) {
      *pHealthState = RegionHealthStatePending;
      break;
    }
  }

  IsLocked = FALSE;
  HasNewGoal = FALSE;

  /** Check for the static health states **/
  if (*pHealthState == RegionHealthStateNormal) {
    if (pRegion->State != IS_STATE_HEALTHY) {
      switch (pRegion->State) {
        case IS_STATE_INIT_FAILURE:
        case IS_STATE_DIMM_MISSING:
        case IS_STATE_CONFIG_INACTIVE:
        case IS_STATE_SPA_MISSING:
          *pHealthState = RegionHealthStateError;
          break;
        default:
          *pHealthState = RegionHealthStateUnknown;
          break;
      }
    }
  }

FinishAdvance:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Determine if a set of dimms is configuring a given socket

  @param[in] DimmsNum Number of DIMMs to verify on socket
  @param[in] SocketId SocketId to verify that dimms are configuring
  @param[out] pWholeSocket True if dimms are fully configuring a socket

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one or more parameters are NULL
**/
STATIC
EFI_STATUS
IsConfigureWholeSocket(
  IN     UINT32 DimmsNum,
  IN     UINT32 SocketId,
  OUT BOOLEAN *pWholeSocket
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 DimmsOnSocket = 0;

  NVDIMM_ENTRY();

  if (pWholeSocket == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pWholeSocket = FALSE;

  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (SocketId == pDimm->SocketId && IsDimmManageable(pDimm)) {
      DimmsOnSocket++;
    }
  }

  if (DimmsNum == DimmsOnSocket) {
    *pWholeSocket = TRUE;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  For a given set of Region goal dimms reduce the capacity of the Region
  based on the requested reserved size

  @param[in out] pReservedSize Size to reduce the Region capacity
  @param[in out] RegionGoalDimms Array of Region goal dimms to reduce
  @param[in out] pRegionGoalDimmsNum number of elements in the array

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/

STATIC
EFI_STATUS
ReduceAppDirectCapacityPerReservedCapacity(
  IN OUT UINT64 *pReservedSize,
  IN OUT REGION_GOAL_DIMM RegionGoalDimms[MAX_DIMMS],
  IN OUT UINT32 *pRegionGoalDimmsNum
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 TotalRegionGoalCapacity = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 TempRegionGoalDimmIndex = 0;
  UINT64 ReduceBy = 0;
  UINT32 RemovedRegionGoalDimmsNum = 0;
  REGION_GOAL_DIMM TempRegionGoalDimms[MAX_DIMMS];
  UINT32 StartingRegionGoalDimmsNum = 0;
  BOOLEAN RemoveAllGoals = TRUE;

  NVDIMM_ENTRY();

  if (pReservedSize == NULL || RegionGoalDimms == NULL || pRegionGoalDimmsNum == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  StartingRegionGoalDimmsNum = *pRegionGoalDimmsNum;

  ZeroMem(TempRegionGoalDimms, sizeof(TempRegionGoalDimms[0]) * MAX_DIMMS);

  if (*pReservedSize == 0) {
    goto Finish;
  }

  // If capacity in the dimms is less than the amount requested
  // then take all capacity in dimms
  for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
    TotalRegionGoalCapacity += RegionGoalDimms[Index].RegionSize;
  }

  if (TotalRegionGoalCapacity <= *pReservedSize) {
    for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
      *pReservedSize -= RegionGoalDimms[Index].RegionSize;
      RegionGoalDimms[Index].RegionSize = 0;
      if (RegionGoalDimms[Index].VolatileSize > 0) {
        RemoveAllGoals = FALSE;
      }
    }

    if (RemoveAllGoals) {
      *pRegionGoalDimmsNum = 0;
    }
  } else {
    // When reducing capacity we don't want to stop unless we have consumed all the goals or
    // we have reduced the requested amount
    for (Index2 = 0; Index2 < StartingRegionGoalDimmsNum; Index2++) {
      // If reserved capacity does not consume the dimms then try to reduce each dimm evenly in allocations
      // of aligned persistent capacity. Region size should already be aligned to RegionAlignment
      ReduceBy = *pReservedSize / *pRegionGoalDimmsNum;
      ReduceBy = ROUNDUP(ReduceBy, gNvmDimmData->Alignments.RegionPersistentAlignment);
      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        // reduce each DIMM evenly if possible
        if (RegionGoalDimms[Index].RegionSize >= ReduceBy && *pReservedSize >= ReduceBy) {
          RegionGoalDimms[Index].RegionSize -= ReduceBy;
          *pReservedSize -= ReduceBy;
        }
        // reduce little more than needed (because of RegionAlignment)
        else if (RegionGoalDimms[Index].RegionSize >= ReduceBy && *pReservedSize < ReduceBy) {
          RegionGoalDimms[Index].RegionSize -= ReduceBy;
          *pReservedSize = 0;
        } else {
          *pReservedSize -= RegionGoalDimms[Index].RegionSize;
          RegionGoalDimms[Index].RegionSize = 0;
        }
      }

      // Reduce array and remove Dimms that have had all capacity RSVD
      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        if (RegionGoalDimms[Index].RegionSize == 0) {
          RemovedRegionGoalDimmsNum++;
        }
      }

      if (RemovedRegionGoalDimmsNum != 0) {
        for (Index = 0, TempRegionGoalDimmIndex = 0; Index < *pRegionGoalDimmsNum; Index++) {
          if (RegionGoalDimms[Index].RegionSize != 0 || RegionGoalDimms[Index].VolatileSize != 0) {
            CopyMem_S(&TempRegionGoalDimms[TempRegionGoalDimmIndex],
              sizeof(TempRegionGoalDimms[Index]),
              &RegionGoalDimms[Index], sizeof(TempRegionGoalDimms[Index]));
            TempRegionGoalDimmIndex++;
          }
        }

        ZeroMem(RegionGoalDimms, sizeof(RegionGoalDimms[0]) * MAX_DIMMS);

        CopyMem_S(RegionGoalDimms, sizeof(RegionGoalDimms[0]) * MAX_DIMMS, TempRegionGoalDimms, sizeof(RegionGoalDimms[0]) * MAX_DIMMS);

        *pRegionGoalDimmsNum = TempRegionGoalDimmIndex;
      }

      // Recalculate Total remaining capacity for next loop
      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        TotalRegionGoalCapacity += RegionGoalDimms[Index].RegionSize;
      }

      if (TotalRegionGoalCapacity == 0 || *pReservedSize == 0 || *pRegionGoalDimmsNum == 0) {
        break;
      }
    }

    if (TotalRegionGoalCapacity > 0 && *pReservedSize > 0) {
      ReturnCode = EFI_DEVICE_ERROR;
      NVDIMM_DBG("Unable to correctly map reserved capacity");
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Map specified request to actual Region Goal templates. Resolve special "remaining" values.

  @param[in] pDimms Array of pointers to manageable DIMMs only
  @param[in] pDimmsNum Number of pointers in pDimms
  @param[out] DimmsSymmetrical Array of Dimms for symmetrical region config
  @param[out] pDimmsSymmetricalNum Returned number of items in DimmsSymmetrical
  @param[out] DimmsAsymmetrical Array of Dimms for asymmetrical region config
  @param[out] pDimmsAsymmetricalNum Returned number of items in DimmsAsymmetrical
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatileSize Volatile region size
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percent
  @param[in] pMaxPMInterleaveSets Pointer to MaxPmInterleaveSets per Die & per Dcpmm
  @param[out] pVolatileSizeActual Actual Volatile region size
  @param[out] RegionGoalTemplates Array of region goal templates
  @param[out] pRegionGoalTemplatesNum Number of items in RegionGoalTemplates
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
MapRequestToActualRegionGoalTemplates(
  IN     DIMM *pDimms[MAX_DIMMS],
  IN     UINT32 DimmsNum,
     OUT REGION_GOAL_DIMM DimmsSymmetrical[MAX_DIMMS],
     OUT UINT32 *pDimmsSymmetricalNum,
     OUT REGION_GOAL_DIMM DimmsAsymmetrical[MAX_DIMMS],
     OUT UINT32 *pDimmsAsymmetricalNum,
  IN     UINT8 PersistentMemType,
  IN     UINT64 VolatileSize,
  IN     UINT32 ReservedPercent,
  IN     MAX_PMINTERLEAVE_SETS *pMaxPMInterleaveSets,
     OUT UINT64 *pVolatileSizeActual OPTIONAL,
     OUT REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM],
     OUT UINT32 *pRegionGoalTemplatesNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT64 AvailablePersistentSize = 0;
  UINT64 VolatileSizeActualOnDimm = 0;
  UINT64 SymmetricalSize = 0;
  UINT64 LeastDimmSize = MAX_UINT64_VALUE;
  UINT64 LeastPersistentSize = MAX_UINT64_VALUE;
  UINT64 PersistentSizeAsym = 0;
  UINT64 VolatileSizeActual = 0;
  UINT64 RegionPersistentAlignment = gNvmDimmData->Alignments.RegionPersistentAlignment;
  UINT64 TotalRawCapacity = 0;
  BOOLEAN AllReserved = FALSE;
  UINT64 ReservedSize = 0;
  UINT64 UnAllocatedAppDirect = 0;
  UINT32 AppDirectRegionGoalDimmsNum = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || DimmsNum == 0 ||
      DimmsSymmetrical == NULL || pDimmsSymmetricalNum == NULL ||
      DimmsAsymmetrical == NULL || pDimmsAsymmetricalNum == NULL ||
      RegionGoalTemplates == NULL || pRegionGoalTemplatesNum == NULL ||
      pCommandStatus == NULL) {
    goto Finish;
  }

  *pDimmsSymmetricalNum = 0;
  *pDimmsAsymmetricalNum = 0;

  /** there can be different sized dimms within the same socket **/
  for (Index = 0; Index < DimmsNum; Index++) {
    if (LeastDimmSize > pDimms[Index]->RawCapacity) {
      LeastDimmSize = pDimms[Index]->RawCapacity;
    }
    TotalRawCapacity += pDimms[Index]->RawCapacity;
  }

  if (TotalRawCapacity == ReservedSize) {
    AllReserved = TRUE;
  }

  if (TotalRawCapacity <= VolatileSize) {
    *pRegionGoalTemplatesNum = 0;

    VolatileSizeActual = 0;

    for (Index = 0; Index < DimmsNum; Index++) {
      VolatileSizeActual += pDimms[Index]->RawCapacity;

      DimmsSymmetrical[Index].pDimm = pDimms[Index];
      DimmsSymmetrical[Index].VolatileSize = pDimms[Index]->RawCapacity;
      (*pDimmsSymmetricalNum)++;
    }
  } else if (VolatileSize > 0 && PersistentMemType == PM_TYPE_RESERVED) {

    SymmetricalSize = ROUNDDOWN(LeastDimmSize, gNvmDimmData->Alignments.RegionVolatileAlignment) * DimmsNum;

    ReturnCode = CalculateActualVolatileSize(LeastDimmSize, VolatileSize / DimmsNum, &VolatileSizeActualOnDimm);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    for (Index = 0; Index < DimmsNum; Index++) {
      VolatileSizeActual += VolatileSizeActualOnDimm;

      DimmsSymmetrical[Index].pDimm = pDimms[Index];
      DimmsSymmetrical[Index].VolatileSize = VolatileSizeActualOnDimm;
      (*pDimmsSymmetricalNum)++;
    }
  } else {

    SymmetricalSize = ROUNDDOWN(LeastDimmSize, gNvmDimmData->Alignments.RegionVolatileAlignment) * DimmsNum;

    ReturnCode = CalculateActualVolatileSize(LeastDimmSize, VolatileSize / DimmsNum, &VolatileSizeActualOnDimm);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    // Asymetrical dimm configuration, find the partition alignment
    // point such that some amount of persistence exists on every dimm
    if (VolatileSizeActualOnDimm * DimmsNum >= SymmetricalSize) {

      if (LeastDimmSize < gNvmDimmData->Alignments.RegionPartitionAlignment) {
        ReturnCode = EFI_ABORTED;
        ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
        goto Finish;
      }

      ReturnCode = CalculateActualVolatileSize(LeastDimmSize,
        LeastDimmSize - gNvmDimmData->Alignments.RegionPartitionAlignment, &VolatileSizeActualOnDimm);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
    }

    if (PersistentMemType != PM_TYPE_RESERVED) {
      LeastPersistentSize = MAX_UINT64_VALUE;
      /** Calculate available persistent memory for Interleave Sets **/
      for (Index = 0; Index < DimmsNum; Index++) {
        // Dimms within the same socket can have different capacities
        if (LeastPersistentSize > (pDimms[Index]->RawCapacity - VolatileSizeActualOnDimm)) {
          LeastPersistentSize = pDimms[Index]->RawCapacity - VolatileSizeActualOnDimm;
          LeastPersistentSize = ROUNDDOWN(LeastPersistentSize, RegionPersistentAlignment);
        }
      }

      for (Index = 0; Index < DimmsNum; Index++) {
        AvailablePersistentSize = pDimms[Index]->RawCapacity - VolatileSizeActualOnDimm;
        AvailablePersistentSize = ROUNDDOWN(AvailablePersistentSize, RegionPersistentAlignment);

        if (AvailablePersistentSize == 0) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PERS_MEM_MUST_BE_APPLIED_TO_ALL_DIMMS);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }

        DimmsSymmetrical[Index].pDimm = pDimms[Index];

        if (PersistentMemType == PM_TYPE_AD_NI) {
          if (pMaxPMInterleaveSets != NULL) {
            if (*pDimmsSymmetricalNum >= pMaxPMInterleaveSets->MaxInterleaveSetsSplit.PerDie) {
              DimmsSymmetrical[Index].RegionSize = 0;
              DimmsSymmetrical[Index].VolatileSize = VolatileSizeActualOnDimm;
              SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_WARN_REGION_MAX_PM_INTERLEAVE_SETS_EXCEEDED);
            }
            else {
              DimmsSymmetrical[Index].RegionSize = AvailablePersistentSize;
              DimmsSymmetrical[Index].VolatileSize = VolatileSizeActualOnDimm;
              AppDirectRegionGoalDimmsNum++;
            }
          }
          else {
            DimmsSymmetrical[Index].RegionSize = AvailablePersistentSize;
            DimmsSymmetrical[Index].VolatileSize = VolatileSizeActualOnDimm;
          }
        }
        else if (PersistentMemType == PM_TYPE_AD) {
          DimmsSymmetrical[Index].RegionSize = LeastPersistentSize;
          DimmsSymmetrical[Index].VolatileSize = VolatileSizeActualOnDimm;

          PersistentSizeAsym = AvailablePersistentSize - LeastPersistentSize;

          if ((PersistentSizeAsym > 0) && (pMaxPMInterleaveSets != NULL)) {
            if (pMaxPMInterleaveSets->MaxInterleaveSetsSplit.PerDcpmm == MAX_IS_PER_DIMM) {
              if ((*pDimmsAsymmetricalNum + 1) < pMaxPMInterleaveSets->MaxInterleaveSetsSplit.PerDie) {
                DimmsAsymmetrical[*pDimmsAsymmetricalNum].pDimm = pDimms[Index];
                DimmsAsymmetrical[*pDimmsAsymmetricalNum].RegionSize = PersistentSizeAsym;
                (*pDimmsAsymmetricalNum)++;
              }
              else {
                SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_WARN_REGION_MAX_PM_INTERLEAVE_SETS_EXCEEDED);
              }
            }
          }
          else if (PersistentSizeAsym > 0) {
            DimmsAsymmetrical[*pDimmsAsymmetricalNum].pDimm = pDimms[Index];
            DimmsAsymmetrical[*pDimmsAsymmetricalNum].RegionSize = PersistentSizeAsym;
            (*pDimmsAsymmetricalNum)++;
          }
        }
        (*pDimmsSymmetricalNum)++;
      }

      /** Calculate Reserved size **/
      ReturnCode = CalculateDimmCapacityFromPercent(pDimms,
        (AppDirectRegionGoalDimmsNum != 0 ? AppDirectRegionGoalDimmsNum: *pDimmsSymmetricalNum),
        ReservedPercent, &ReservedSize);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      //If VolatileCapcity is zero there will be some unallocated PM capacity already due to AppDirect alignments
      //Remove this from ReservedSize as it is already reserved and does not need to be removed from AppDirect
      if (VolatileSize == 0) {
        for (Index = 0; Index < DimmsNum; Index++) {
          UnAllocatedAppDirect = (pDimms[Index]->RawCapacity - ROUNDDOWN(pDimms[Index]->RawCapacity,
              gNvmDimmData->Alignments.RegionPersistentAlignment));
          if (ReservedSize >= UnAllocatedAppDirect) {
            ReservedSize -= UnAllocatedAppDirect;
          } else {
            ReservedSize = 0;
          }
        }
      }

      //Reduce Asym Dimms of RSVD capacity first
      if (ReservedSize != 0 && *pDimmsAsymmetricalNum != 0) {
        ReturnCode = ReduceAppDirectCapacityPerReservedCapacity(&ReservedSize,
            DimmsAsymmetrical, pDimmsAsymmetricalNum);
        if (EFI_ERROR(ReturnCode)) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
          goto Finish;
        }
      }

      //If Rsvd capacity still exists reduce symmetrical dimms
      if (ReservedSize != 0 && *pDimmsSymmetricalNum != 0) {
        ReturnCode = ReduceAppDirectCapacityPerReservedCapacity(&ReservedSize, DimmsSymmetrical, pDimmsSymmetricalNum);
        if (EFI_ERROR(ReturnCode)) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
          goto Finish;
        }
      }

      //User requested all persistent memory but requested some amount not be reserved,
      // and we ended up reserving it all. Add some small amount back in
      if (VolatileSize == 0 && !AllReserved && *pDimmsSymmetricalNum == 0) {
        for (Index = 0; Index < DimmsNum; Index++) {
          DimmsSymmetrical[Index].pDimm = pDimms[Index];
          DimmsSymmetrical[Index].VolatileSize = 0;
          DimmsSymmetrical[Index].RegionSize = gNvmDimmData->Alignments.RegionPersistentAlignment;
          (*pDimmsSymmetricalNum)++;
        }
      }
    }

    // We have reserved all PM capacity and no volatile capacity exists
    if (PersistentMemType == PM_TYPE_RESERVED || *pDimmsSymmetricalNum == 0) {
      for (Index = 0; Index < DimmsNum; Index++) {
        DimmsSymmetrical[Index].pDimm = pDimms[Index];
        DimmsSymmetrical[Index].RegionSize = 0;
        DimmsSymmetrical[Index].VolatileSize = 0;
        (*pDimmsSymmetricalNum)++;
      }
      *pRegionGoalTemplatesNum = 0;
    } else if (PersistentMemType == PM_TYPE_AD_NI) {
      RegionGoalTemplates[0].InterleaveSetType = NON_INTERLEAVED;
      RegionGoalTemplates[0].Asymmetrical = FALSE;
      *pRegionGoalTemplatesNum = 1;
    } else if (PersistentMemType == PM_TYPE_AD) {
      /** Addressing the corner case where first socket has mixed
       * dimms and the 2nd doesn't - leave the goal template unchanged**/
      if (*pRegionGoalTemplatesNum != 2) {
        RegionGoalTemplates[0].InterleaveSetType = INTERLEAVED;
        RegionGoalTemplates[0].Asymmetrical = FALSE;
        *pRegionGoalTemplatesNum = 1;
        if (*pDimmsAsymmetricalNum > 0) {
          RegionGoalTemplates[1].InterleaveSetType = NON_INTERLEAVED;
          RegionGoalTemplates[1].Asymmetrical = TRUE;
          *pRegionGoalTemplatesNum = 2;
        }
      }
    } else {
      ReturnCode = EFI_ABORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    if (VolatileSize > 0) {
      VolatileSizeActual = VolatileSizeActualOnDimm * DimmsNum;
    }
  }

  /** Check if platform allows volatile mode  */
  if (VolatileSize > 0 && !gNvmDimmData->PMEMDev.IsMemModeAllowedByBios) {
    // set objectId to 0, there should be at leat one object in here
    SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_WARN_IMC_DDR_PMM_NOT_PAIRED);

  }

  if (pVolatileSizeActual != NULL) {
    *pVolatileSizeActual = VolatileSizeActual;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Verify that all the unconfigured DIMMs or all DIMMs on a given socket are configured at once to keep supported
  region configs.

  @param[in] pDimms List of DIMMs to configure
  @param[in] DimmsNum Number of DIMMs to configure
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatilePercent Volatile region size in percents
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one or more parameters are NULL
  @retval EFI_UNSUPPORTED A given config is unsupported
**/
EFI_STATUS
VerifyCreatingSupportedRegionConfigs(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     UINT8 PersistentMemType,
  IN     UINT32 VolatilePercent,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 ConfiguredDimmsNum = 0;
  UINT32 UnconfiguredDimmsNum = 0;
  UINT32 SpecifiedConfiguredDimmsNum = 0;
  UINT32 SpecifiedUnconfiguredDimmsNum = 0;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 Socket = 0;
  UINT32 Index = 0;
  UINT32 ExistingADNonInterleavedRegions = 0;
  UINT32 ExistingVolatileRegions = 0;
  UINT32 NumOfDimmsOnSocket = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pCommandStatus == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Socket = 0; Socket < MAX_SOCKETS; Socket++) {
    ConfiguredDimmsNum = 0;
    UnconfiguredDimmsNum = 0;
    NumOfDimmsOnSocket = 0;
    ExistingADNonInterleavedRegions = 0;
    ExistingVolatileRegions = 0;

    /** Get a number of all configured and unconfigured Dimms on a given socket **/
    LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);

      if (Socket == pDimm->SocketId) {
        // Unmanageable, non-functional, and media inaccessible DCPMMs are not included in goal requests
        if (!IsDimmManageable(pDimm) || pDimm->NonFunctional || DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask)) {
          continue;
        }
        // Population Violation DCPMMs are not included in goal requests except ADx1 100%
        if (IsDimmInPopulationViolation(pDimm) && !(PM_TYPE_AD_NI == PersistentMemType && 0 == VolatilePercent)) {
          continue;
        }

        if (pDimm->Configured) {
          ConfiguredDimmsNum += 1;
        } else {
          UnconfiguredDimmsNum += 1;
        }

        if (!IsPointerInArray((VOID **)pDimms, DimmsNum, pDimm)) {
          // Calculate the ADx1 regions on the unspecified DIMMs
          if (pDimm->VolatileCapacity == 0 &&
            pDimm->ISsNfitNum > 0 &&
            pDimm->pISsNfit[APPDIRECT_1_INDEX]->InterleaveFormatWays == INTERLEAVE_WAYS_X1) {
            ExistingADNonInterleavedRegions++;
          }

          // Calculate number of unspecified DIMMs with only volatile capacities
          if (pDimm->VolatileCapacity > 0 && pDimm->ISsNfitNum == 0) {
            ExistingVolatileRegions++;
          }
        }
        NumOfDimmsOnSocket++;
      }
    }

    SpecifiedConfiguredDimmsNum = 0;
    SpecifiedUnconfiguredDimmsNum = 0;

    /** Get a number of specified configured and unconfigured DIMMs on a given socket **/
    for (Index = 0; Index < DimmsNum; Index++) {
      if (Socket == pDimms[Index]->SocketId) {
        // Unmanageable and non-functional DCPMMs are not included in goal requests
        if (!IsDimmManageable(pDimms[Index]) || pDimms[Index]->NonFunctional) {
          continue;
        }
        // Population Violation DCPMMs are not included in goal requests except ADx1 100%
        if (IsDimmInPopulationViolation(pDimms[Index]) && !(PM_TYPE_AD_NI == PersistentMemType && 0 == VolatilePercent)) {
          continue;
        }

        if (pDimms[Index]->Configured) {
          SpecifiedConfiguredDimmsNum += 1;
        } else {
          SpecifiedUnconfiguredDimmsNum += 1;
        }
      }
    }

    /**
      If any DIMM is specified for a given socket then:
      - Target all unconfigured DCPMMs
      - Target all DCPMMs on a given socket
      - Target DCPMMs for AppDirect Not-Interleaved with all unspecified DCPMMs configured for AppDirect Not-Interleaved only
      - Target DCPMMs for 100% MemoryMode with all unspecified DCPMMs configured for MemoryMode only
    **/
    if (!(
        (SpecifiedConfiguredDimmsNum == 0 && SpecifiedUnconfiguredDimmsNum == 0) ||
        (SpecifiedConfiguredDimmsNum == 0 && SpecifiedUnconfiguredDimmsNum == UnconfiguredDimmsNum) ||
        (SpecifiedConfiguredDimmsNum == ConfiguredDimmsNum && SpecifiedUnconfiguredDimmsNum == UnconfiguredDimmsNum) ||
        ((PM_TYPE_AD_NI == PersistentMemType || (PM_TYPE_AD == PersistentMemType && (SpecifiedConfiguredDimmsNum + SpecifiedUnconfiguredDimmsNum) == 1)) &&
          VolatilePercent == 0 && ((SpecifiedConfiguredDimmsNum + SpecifiedUnconfiguredDimmsNum + ExistingADNonInterleavedRegions) == NumOfDimmsOnSocket)) ||
        (VolatilePercent == 100 && ((SpecifiedConfiguredDimmsNum + SpecifiedUnconfiguredDimmsNum + ExistingVolatileRegions) == NumOfDimmsOnSocket))
        )) {
      Rc = EFI_UNSUPPORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_CONF_UNSUPPORTED_CONFIG);
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Retrieve goal configurations by using Platform Config Data

  @param[in, out] pDimmList Head of the list of all NVM DIMMs in the system
  @param[in] Restore corrupt pcd

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveGoalConfigsFromPlatformConfigData(
  IN OUT LIST_ENTRY *pDimmList,
  IN     BOOLEAN RestoreCorrupt
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS IReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  NVDIMM_PLATFORM_CONFIG_INPUT *pPcdConfInput = NULL;
  NVDIMM_PLATFORM_CONFIG_OUTPUT *pPcdConfOutput = NULL;
  NVDIMM_PARTITION_SIZE_CHANGE *pPartitionSizeChange = NULL;
  PCAT_TABLE_HEADER *pPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  REGION_GOAL *pRegionGoals[MAX_IS_PER_DIMM * MAX_DIMMS];
  UINT32 RegionGoalsNum = 0;
  REGION_GOAL *pNewRegionGoal = NULL;
  BOOLEAN New = FALSE;
  UINT32 SequenceIndex = 0;
  ACPI_REVISION PcdCinRev;
  UINT8 InterleaveChangeStatus = 0;
  NVDIMM_ENTRY();

  SetMem(pRegionGoals, sizeof(pRegionGoals), 0x0);

  if (pDimmList == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = ClearInternalGoalConfigsInfo(pDimmList);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishError;
  }

  LIST_FOR_EACH(pDimmNode, pDimmList) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    // Set default values
    pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS;
    pDimm->RegionsGoalConfig = FALSE;
    pDimm->PcdSynced = TRUE;

    if (!IsDimmManageable(pDimm) || DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask)) {
      continue;
    }

    // Free previous use of pcd header if needed
    FREE_POOL_SAFE(pPcdConfHeader);
    ReturnCode = GetPlatformConfigDataOemPartition(pDimm, RestoreCorrupt, &pPcdConfHeader);
#ifdef MEMORY_CORRUPTION_WA
    if (ReturnCode == EFI_DEVICE_ERROR) {
      ReturnCode = GetPlatformConfigDataOemPartition(pDimm, RestoreCorrupt, &pPcdConfHeader);
    }
#endif // MEMORY_CORRUPTIO_WA
    if (EFI_ERROR(ReturnCode)) {
      // Ignore all errors except for PMem module busy with sanitize operation
      if (EFI_NO_RESPONSE == ReturnCode) {
        /* Save the return code here and continue with the execution for rest of the dimms.
          This is done to make the UEFI initialization succeed. During UEFI init,
          return code will be ignored but we have to error out when the actual command is executed. */
        IReturnCode = ReturnCode;
      }
      ReturnCode = EFI_SUCCESS;
      continue;
    }

    if (NULL != pPcdConfHeader) {
      pPcdConfInput = GET_NVDIMM_PLATFORM_CONFIG_INPUT(pPcdConfHeader);
      pPcdConfOutput = GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(pPcdConfHeader);
    }

    // If no PCD Header, CIN record then no goal
    if ((NULL == pPcdConfHeader) || (pPcdConfHeader->ConfInputStartOffset == 0) || (pPcdConfHeader->ConfInputDataSize == 0)) {
      NVDIMM_DBG("There is no Config Input table");
      continue;
    }
    // CIN is corrupt
    else if (!IsPcdConfInputHeaderValid(pPcdConfInput, pDimm->PcdOemPartitionSize)) {
      pPcdConfHeader->ConfInputStartOffset = 0;
      pPcdConfHeader->ConfInputDataSize = 0;
      NVDIMM_DBG("The Config Input table is corrupted, Ignoring it");
      continue;
    }
    // If CIN and COUT sequence are the same, then goal attempted to be applied already
    else if ((pPcdConfHeader->ConfOutputStartOffset != 0) && (pPcdConfHeader->ConfOutputDataSize != 0) &&
      IsPcdConfOutputHeaderValid(pPcdConfOutput, pDimm->PcdOemPartitionSize) &&
      (pPcdConfInput->SequenceNumber == pPcdConfOutput->SequenceNumber)) {
      NVDIMM_DBG("The config goal is already applied");
      continue;
    }

    // We have a valid goal after this point
    PcdCinRev = pPcdConfInput->Header.Revision;

    pDimm->PcdSynced = FALSE;
    pDimm->RegionsGoalConfig = TRUE;

    pPcatTable = (PCAT_TABLE_HEADER *) &pPcdConfInput->pPcatTables;
    if (pPcatTable == NULL) {
      NVDIMM_ERR("pPcatTable is null");
      ReturnCode = EFI_ABORTED;
      goto FinishError;
    }
    SizeOfPcatTables = pPcdConfHeader->ConfInputDataSize - (UINT32)((UINT8 *)pPcatTable - (UINT8 *)pPcdConfInput);

    SequenceIndex = 0;

    /**
      Example of the use of the while loop condition
      Extension table #1   offset:  0   size: 10
      Extension table #2   offset: 10   size:  5
      Size of extension tables: 15 (10 + 5)

      Iteration #1:   offset: 0
      Iteration #2:   offset: 10
      Iteration #3:   offset: 15   stop the loop: offset isn't less than size
    **/
    while ((UINT32) ((UINT8 *)pPcatTable - (UINT8 *) &pPcdConfInput->pPcatTables) < SizeOfPcatTables) {
      switch (pPcatTable->Type) {
      case PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE:
        pPartitionSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pPcatTable;
        pDimm->VolatileSizeGoal = pDimm->RawCapacity - pPartitionSizeChange->PmPartitionSize;
        break;
      case PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE:
        if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdCinRev)) {
          NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pPcatTable;
          ReturnCode = RetrieveRegionGoalFromInterleaveInformationTable(pRegionGoals,
            RegionGoalsNum,
            pInterleaveInfo,
            PcdCinRev,
            &pNewRegionGoal,
            &New);
        } else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdCinRev)) {
          NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pPcatTable;
          ReturnCode = RetrieveRegionGoalFromInterleaveInformationTable(pRegionGoals,
            RegionGoalsNum,
            pInterleaveInfo,
            PcdCinRev,
            &pNewRegionGoal,
            &New);
        }

        if (ReturnCode == EFI_NOT_FOUND) {
          ReturnCode = EFI_SUCCESS;
          break;
        } else if (EFI_ERROR(ReturnCode)) {
          goto FinishError;
        }

        if (New && pNewRegionGoal != NULL) {
          pNewRegionGoal->SequenceIndex = SequenceIndex;
          ASSERT(RegionGoalsNum < MAX_IS_PER_DIMM * MAX_DIMMS);
          pRegionGoals[RegionGoalsNum] = pNewRegionGoal;
          RegionGoalsNum++;
        }

        SequenceIndex++;

        break;
      case PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE:
        break;
      default:
        NVDIMM_DBG("This type (%d) of PCAT table shouldn't be contained in Config Input table", pPcatTable->Type);
        ReturnCode = EFI_ABORTED;
        goto FinishError;
        break;
      }
      pPcatTable = (PCAT_TABLE_HEADER *)((UINT8 *)pPcatTable + pPcatTable->Length);
    }

    /** Unknown status as default **/
    pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_UNKNOWN;

    if (pPcdConfHeader->ConfOutputStartOffset == 0 || pPcdConfHeader->ConfOutputDataSize == 0 ||
        pPcdConfInput->SequenceNumber != pPcdConfOutput->SequenceNumber) {
      pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_NEW;
      pDimm->PcdSynced = TRUE;
      NVDIMM_DBG("There is no Config Output table");
      FREE_POOL_SAFE(pPcdConfHeader);
      continue;
    }

    if (!IsPcdConfOutputHeaderValid(pPcdConfOutput, pDimm->PcdOemPartitionSize)) {
      ReturnCode = EFI_ABORTED;
      goto FinishError;
    }

    if (pPcdConfOutput->ValidationStatus == CONFIG_OUTPUT_STATUS_SUCCESS) {
      pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS;
      pDimm->PcdSynced = TRUE;
      FREE_POOL_SAFE(pPcdConfHeader);
      continue;
    }
    else if (pPcdConfOutput->ValidationStatus == CONFIG_OUTPUT_STATUS_CPU_MAX_MEMORY_LIMIT_VIOLATION ||
             pPcdConfOutput->ValidationStatus == CONFIG_OUTPUT_STATUS_NM_FM_RATIO_UNSUPPORTED ||
             pPcdConfOutput->ValidationStatus == CONFIG_OUTPUT_STATUS_POPULATION_ISSUE) {
      pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_BAD_REQUEST;
    }

    pPcatTable = (PCAT_TABLE_HEADER *) &pPcdConfOutput->pPcatTables;
    if (pPcatTable == NULL) {
      NVDIMM_ERR("pPcatTable is null");
      ReturnCode = EFI_ABORTED;
      goto FinishError;
    }
    SizeOfPcatTables = pPcdConfHeader->ConfOutputDataSize - (UINT32)((UINT8 *)pPcatTable - (UINT8 *)pPcdConfOutput);

    /**
      Example of the use of the while loop condition
      Extension table #1   offset:  0   size: 10
      Extension table #2   offset: 10   size:  5
      Size of extension tables: 15 (10 + 5)

      Iteration #1:   offset: 0
      Iteration #2:   offset: 10
      Iteration #3:   offset: 15   stop the loop: offset isn't less than size
    **/
    while ((UINT32) ((UINT8 *)pPcatTable - (UINT8 *) &pPcdConfOutput->pPcatTables) < SizeOfPcatTables) {
      switch (pPcatTable->Type) {
      case PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE:
        pPartitionSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *) pPcatTable;

        switch (pPartitionSizeChange->PartitionSizeChangeStatus) {
        case PARTITION_SIZE_CHANGE_STATUS_SUCCESS:
          break;

        case PARTITION_SIZE_CHANGE_STATUS_DIMM_MISSING:
        case PARTITION_SIZE_CHANGE_STATUS_ISET_MISSING:
        case PARTITION_SIZE_CHANGE_STATUS_UNSUPPORTED_ALIGNMENT:
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_BAD_REQUEST;
          break;

        case PARTITION_SIZE_CHANGE_STATUS_EXCEED_DRAM_DECODERS:
        case PARTITION_SIZE_CHANGE_STATUS_EXCEED_SIZE:
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_NOT_ENOUGH_RESOURCES;
          break;

        case PARTITION_SIZE_CHANGE_STATUS_FW_ERROR:
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_FIRMWARE_ERROR;
          break;

        case PARTITION_SIZE_CHANGE_STATUS_RESERVED:
        case PARTITION_SIZE_CHANGE_STATUS_UNDEFINED:
        default:
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_FAILED_UNKNOWN;
          break;
        }

        break;
      case PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE:
        if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdCinRev)) {
          NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pPcatTable;
          InterleaveChangeStatus = pInterleaveInfo->InterleaveChangeStatus;
        }
        else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdCinRev)) {
          NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pPcatTable;
          InterleaveChangeStatus = pInterleaveInfo->InterleaveChangeStatus;
        }

        switch (InterleaveChangeStatus) {
        case INTERLEAVE_INFO_STATUS_SUCCESS:
          break;

        case INTERLEAVE_INFO_STATUS_DIMM_MISSING:
        case INTERLEAVE_INFO_STATUS_ISET_MISSING:
        case INTERLEAVE_INFO_STATUS_CHANNEL_NOT_MATCH:
        case INTERLEAVE_INFO_STATUS_UNSUPPORTED_ALIGNMENT:
        case INTERLEAVE_INFO_STATUS_REQUEST_UNSUPPORTED:
        case INTERLEAVE_INFO_STATUS_CIN_MISSING:
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_BAD_REQUEST;
          break;

        case INTERLEAVE_INFO_STATUS_EXCEED_DRAM_DECODERS:
        case INTERLEAVE_INFO_STATUS_EXCEED_MAX_SPA_SPACE:
        case INTERLEAVE_INFO_STATUS_MIRROR_FAILED:
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_NOT_ENOUGH_RESOURCES;
          break;

        case INTERLEAVE_INFO_STATUS_NOT_PROCESSED:
        case INTERLEAVE_INFO_STATUS_PARTITIONING_FAILED:
        case INTERLEAVE_INFO_STATUS_UNDEFINED:
        default :
          pDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_FAILED_UNKNOWN;
          break;
        }

        break;
      case PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE:
        break;
      default:
        NVDIMM_DBG("This type (%d) of PCAT table shouldn't be contained in Config Output table", pPcatTable->Type);
        ReturnCode = EFI_ABORTED;
        goto FinishError;
        break;
      }


      if (pDimm->GoalConfigStatus != GOAL_CONFIG_STATUS_UNKNOWN) {
        break;
      }

      pPcatTable = (PCAT_TABLE_HEADER *)((UINT8 *)pPcatTable + pPcatTable->Length);
    }

    FREE_POOL_SAFE(pPcdConfHeader);

    pDimm->PcdSynced = TRUE;
  }

  /** Success **/
  goto Finish;

FinishError:
  ClearInternalGoalConfigsInfo(pDimmList);
Finish:
  FREE_POOL_SAFE(pPcdConfHeader);

  ReturnCode = (IReturnCode != EFI_SUCCESS) ? IReturnCode : ReturnCode;
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Parse Interleave Information table and retrieve Region Goal (create a Region Goal if it doesn't exist yet)

  @param[in] pRegionGoals Array of all Region Goals in the system
  @param[in] RegionGoalsNum Number of pointers in pRegionGoals
  @param[in] pInterleaveInfo Interleave Information table retrieved from DIMM
  @param[in] PcdCinRev Revision of the PCD Config Input table
  @param[out] ppRegionGoal Output variable for Region Goal
  @param[out] pNew True if Region Goal new created, False if already exists

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveRegionGoalFromInterleaveInformationTable(
  IN     REGION_GOAL *pRegionGoals[],
  IN     UINT32 RegionGoalsNum,
  IN     VOID *pInterleaveInfoTable,
  IN     ACPI_REVISION PcdCinRev,
     OUT REGION_GOAL **ppRegionGoal,
     OUT BOOLEAN *pNew
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  BOOLEAN RegionGoalExists = FALSE;
  UINT32 Index = 0;
  REGION_GOAL *pRegionGoal = NULL;

  NVDIMM_ENTRY();

  if (pRegionGoals == NULL || pInterleaveInfoTable == NULL || ppRegionGoal == NULL || pNew == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  if (IS_ACPI_REV_INVALID(PcdCinRev)) {
    NVDIMM_DBG("Revision of PCD Config Input table is invalid");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto FinishError;
  }

  if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdCinRev)) {
    NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pInterleaveInfoTable;
    NVDIMM_IDENTIFICATION_INFORMATION *pCurrentIdentInfo = NULL;
    /**
    Check if Interleave Set already exists for this Interleave Set Index
  **/
    for (Index = 0; Index < RegionGoalsNum; Index++) {
      if (pRegionGoals[Index]->InterleaveSetIndex == pInterleaveInfo->InterleaveSetIndex) {
        RegionGoalExists = TRUE;
        pRegionGoal = pRegionGoals[Index];
        break;
      }
    }

    if (!RegionGoalExists) {
      /**
        Initialize Region Goal
      **/
      pRegionGoal = AllocateZeroPool(sizeof(*pRegionGoal));
      if (pRegionGoal == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto FinishError;
      }

      pRegionGoal->InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;

      pRegionGoal->ChannelInterleaving = pInterleaveInfo->InterleaveFormatChannel;
      pRegionGoal->ImcInterleaving = pInterleaveInfo->InterleaveFormatImc;
      pRegionGoal->NumOfChannelWays = pInterleaveInfo->InterleaveFormatWays;
      pRegionGoal->DimmsNum = pInterleaveInfo->NumOfDimmsInInterleaveSet;

      if (pInterleaveInfo->MirrorEnable) {
        pRegionGoal->InterleaveSetType = MIRRORED;
      }
      else if (pInterleaveInfo->NumOfDimmsInInterleaveSet == INTERLEAVE_WAYS_X1) {
        pRegionGoal->InterleaveSetType = NON_INTERLEAVED;
      }
      else {
        pRegionGoal->InterleaveSetType = INTERLEAVED;
      }

      pRegionGoal->Size = 0;

      pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *)&pInterleaveInfo->pIdentificationInfoList;

      /** First verify all dimm identifiers before assign Region Goal to Dimms **/
      for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
        if (IS_ACPI_REV_MAJ_0_MIN_1(PcdCinRev)) {
          pDimm = GetDimmBySerialNumber(&gNvmDimmData->PMEMDev.Dimms, pCurrentIdentInfo->DimmIdentification.Version1.DimmSerialNumber);
        }
        else {
          pDimm = GetDimmByUniqueIdentifier(&gNvmDimmData->PMEMDev.Dimms, pCurrentIdentInfo->DimmIdentification.Version2.Uid);
        }
        if (pDimm == NULL) {
          ReturnCode = EFI_NOT_FOUND;
          goto FinishError;
        }
        pCurrentIdentInfo++;
      }

      pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *)&pInterleaveInfo->pIdentificationInfoList;

      /** Now we are sure that all dimm identifiers are valid **/
      for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
        if (IS_ACPI_REV_MAJ_0_MIN_1(PcdCinRev)) {
          pDimm = GetDimmBySerialNumber(&gNvmDimmData->PMEMDev.Dimms, pCurrentIdentInfo->DimmIdentification.Version1.DimmSerialNumber);
        }
        else {
          pDimm = GetDimmByUniqueIdentifier(&gNvmDimmData->PMEMDev.Dimms, pCurrentIdentInfo->DimmIdentification.Version2.Uid);
        }
        if (pDimm == NULL) {
          continue;
        }

        pRegionGoal->pDimms[Index] = pDimm;
        pRegionGoal->Size += pCurrentIdentInfo->PmPartitionSize;

        ASSERT(pDimm->RegionsGoalNum < MAX_IS_PER_DIMM);
        pDimm->pRegionsGoal[pDimm->RegionsGoalNum] = pRegionGoal;
        pDimm->RegionsGoalNum++;

        pCurrentIdentInfo++;
      }

      *pNew = TRUE;
    }
    else {
      *pNew = FALSE;
    }
  }
  else if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcdCinRev)) {
    NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pInterleaveInfoTable;
    NVDIMM_IDENTIFICATION_INFORMATION3 *pCurrentIdentInfo = NULL;
    /**
    Check if Interleave Set already exists for this Interleave Set Index
  **/
    for (Index = 0; Index < RegionGoalsNum; Index++) {
      if (pRegionGoals[Index]->InterleaveSetIndex == pInterleaveInfo->InterleaveSetIndex) {
        RegionGoalExists = TRUE;
        pRegionGoal = pRegionGoals[Index];
        break;
      }
    }

    if (!RegionGoalExists) {
      /**
        Initialize Region Goal
      **/
      pRegionGoal = AllocateZeroPool(sizeof(*pRegionGoal));
      if (pRegionGoal == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto FinishError;
      }

      pRegionGoal->InterleaveSetIndex = pInterleaveInfo->InterleaveSetIndex;

      pRegionGoal->ChannelInterleaving = pInterleaveInfo->InterleaveFormatChannel;
      pRegionGoal->ImcInterleaving = pInterleaveInfo->InterleaveFormatImc;
      GetBitFieldForNumOfChannelWays(pInterleaveInfo->NumOfDimmsInInterleaveSet, &pRegionGoal->NumOfChannelWays);
      pRegionGoal->DimmsNum = pInterleaveInfo->NumOfDimmsInInterleaveSet;

      if (pInterleaveInfo->NumOfDimmsInInterleaveSet == INTERLEAVE_WAYS_X1) {
        pRegionGoal->InterleaveSetType = NON_INTERLEAVED;
      }
      else {
        pRegionGoal->InterleaveSetType = INTERLEAVED;
      }

      pRegionGoal->Size = 0;

      pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *)&pInterleaveInfo->pIdentificationInfoList;

      /** First verify all dimm identifiers before assign Region Goal to Dimms **/
      for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
        pDimm = GetDimmByUniqueIdentifier(&gNvmDimmData->PMEMDev.Dimms, pCurrentIdentInfo->DimmIdentification);

        if (pDimm == NULL) {
          ReturnCode = EFI_NOT_FOUND;
          goto FinishError;
        }
        pCurrentIdentInfo++;
      }

      pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *)&pInterleaveInfo->pIdentificationInfoList;

      /** Now we are sure that all dimm identifiers are valid **/
      for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
        pDimm = GetDimmByUniqueIdentifier(&gNvmDimmData->PMEMDev.Dimms, pCurrentIdentInfo->DimmIdentification);

        if (pDimm == NULL) {
          continue;
        }

        pRegionGoal->pDimms[Index] = pDimm;
        pRegionGoal->Size += pCurrentIdentInfo->PmPartitionSize;

        ASSERT(pDimm->RegionsGoalNum < MAX_IS_PER_DIMM);
        pDimm->pRegionsGoal[pDimm->RegionsGoalNum] = pRegionGoal;
        pDimm->RegionsGoalNum++;

        pCurrentIdentInfo++;
      }

      *pNew = TRUE;
    }
    else {
      *pNew = FALSE;
    }
  }

  /** Success **/
  *ppRegionGoal = pRegionGoal;
  goto Finish;

FinishError:
  if (!RegionGoalExists) {
    FREE_POOL_SAFE(pRegionGoal);
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Map Regions Goal configs on specified DIMMs

  @param[in] DimmsSym Array of Dimms for symmetrical region config
  @param[in] DimmsSymNum Number of items in DimmsSym
  @param[in] DimmsAsym Array of Dimms for asymmetrical region config
  @param[in] DimmsAsymNum Number of items in DimmsAsym
  @param[in, out] pReserveDimm Dimm that its whole capacity will be set as persistent partition
  @param[in] ReserveDimmType Type of reserve dimm
  @param[in] VolatileSize Volatile region size in bytes
  @param[in] RegionGoalTemplates Array of template goal REGIONs
  @param[in] RegionGoalTemplatesNum Number of elements in RegionGoalTemplates
  @param[in] pDriverPreferences Driver preferences for interleave sets
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or input DIMMs are different than affected DIMMs
**/
EFI_STATUS
MapRegionsGoal(
  IN     REGION_GOAL_DIMM DimmsSym[MAX_DIMMS],
  IN     UINT32 DimmsSymNum,
  IN     REGION_GOAL_DIMM DimmsAsym[MAX_DIMMS],
  IN     UINT32 DimmsAsymNum,
  IN OUT DIMM *pReserveDimm OPTIONAL,
  IN     UINT8 ReserveDimmType OPTIONAL,
  IN     UINT64 VolatileSize,
  IN     REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM],
  IN     UINT32 RegionGoalTemplatesNum,
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pDimmNode = NULL;
  DIMM *pDimm = NULL;
  DIMM *pDimmsAndReserveDimm[MAX_DIMMS];
  UINT32 DimmsAndReserveDimmNum = 0;
  DIMM *pRelatedDimms[MAX_DIMMS];
  UINT32 RelatedDimmsNum = 0;
  DIMM *pRelatedDimmsOnSocket[MAX_DIMMS_PER_SOCKET];
  UINT32 RelatedDimmsOnSocketNum = 0;
  DIMM *pSpecifiedDimmsOnSocket[MAX_DIMMS_PER_SOCKET];
  UINT64 SpecifiedDimmsOnSocketRegionSize[MAX_DIMMS_PER_SOCKET];
  UINT32 SpecifiedDimmsOnSocketNum = 0;
  DIMM *pSpecifiedDimmsOnSocketAsym[MAX_DIMMS_PER_SOCKET];
  UINT64 SpecifiedDimmsOnSocketAsymRegionSize[MAX_DIMMS_PER_SOCKET];
  UINT32 SpecifiedDimmsOnSocketAsymNum = 0;
  REGION_GOAL *pExistingRegionsGoal[MAX_IS_CONFIGS];
  UINT32 ExistingRegionsGoalNum = 0;
  REGION_GOAL *pNewRegionsGoal[MAX_IS_PER_SOCKET];
  REGION_GOAL *pReserveDimmRegionsGoal = NULL;
  REGION_GOAL_TEMPLATE ResDimmRegionGoalTemplate;
  UINT32 NewRegionsGoalNum = 0;
  UINT32 Socket = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 RegionGoalSequenceIndex = 0;
  UINT64 InterleaveSetSize = 0;
  UINT16 AvailableISIndex = 0;
  REGION_GOAL_DIMM *pRegionGoalDimm = NULL;
  UINT32 ExistingRegionsNumOnSocket = 0;
  ACPI_REVISION PcatRevision;
  BOOLEAN WholeSocket = FALSE;
  MAX_PMINTERLEAVE_SETS MaxPMInterleaveSets;
  LIST_ENTRY *pRegionList = NULL;
  LIST_ENTRY *pRegionNode = NULL;
  NVM_IS *pRegion = NULL;

  NVDIMM_ENTRY();

  ZeroMem(pDimmsAndReserveDimm, sizeof(pDimmsAndReserveDimm));
  ZeroMem(pRelatedDimms, sizeof(pRelatedDimms));
  ZeroMem(pRelatedDimmsOnSocket, sizeof(pRelatedDimmsOnSocket));
  ZeroMem(pSpecifiedDimmsOnSocket, sizeof(pSpecifiedDimmsOnSocket));
  ZeroMem(pExistingRegionsGoal, sizeof(pExistingRegionsGoal));
  ZeroMem(pNewRegionsGoal, sizeof(pNewRegionsGoal));
  ZeroMem(&ResDimmRegionGoalTemplate, sizeof(ResDimmRegionGoalTemplate));
  ZeroMem(&PcatRevision, sizeof(PcatRevision));
  ZeroMem(&MaxPMInterleaveSets, sizeof(MaxPMInterleaveSets));

  if (DimmsSym == NULL || DimmsAsym == NULL || RegionGoalTemplates == NULL || pCommandStatus == NULL ||
      pDriverPreferences == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pReserveDimm != NULL && ReserveDimmType != RESERVE_DIMM_AD_NOT_INTERLEAVED) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead != NULL) {
    PcatRevision.AsUint8 = gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr->Header.Revision.AsUint8;
  }

  if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcatRevision)) {
    ReturnCode = RetrieveMaxPMInterleaveSets(&MaxPMInterleaveSets);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  /** Get the largest interleave set index in existing Config Input tables on DIMMs. **/
  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (!IsDimmManageable(pDimm)|| !IsDimmInSupportedConfig(pDimm)) {
      continue;
    }

    for (Index = 0; Index < pDimm->RegionsGoalNum; Index++) {
      if (pDimm->pRegionsGoal[Index]->InterleaveSetIndex > AvailableISIndex) {
        AvailableISIndex = pDimm->pRegionsGoal[Index]->InterleaveSetIndex;
      }
    }
  }
  /** Ensure index is unique across all dimms in the system, not just dimms targeted for goal creation.
      This can happen when adding new dimms to the system with previously configured regions.
  **/
  if (0x0 == AvailableISIndex) {

    ReturnCode = GetRegionList(&pRegionList, FALSE);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to retrieve the region list.");
      goto Finish;
    }

    /** Get the largest interleave set index in existing regions on DIMMs. **/
    LIST_FOR_EACH(pRegionNode, pRegionList) {
      pRegion = IS_FROM_NODE(pRegionNode);

      if (pRegion->InterleaveSetIndex > AvailableISIndex) {
        AvailableISIndex = pRegion->InterleaveSetIndex;
      }
    }
  }

  /** We have found the largest index. The next one is available. **/
  AvailableISIndex++;

  /** Put all specified Dimms into one array **/
  DimmsAndReserveDimmNum = 0;
  if (DimmsSymNum > 0) {
    for (Index = 0; Index < DimmsSymNum; Index++) {
      pDimmsAndReserveDimm[Index] = DimmsSym[Index].pDimm;
    }
    DimmsAndReserveDimmNum += DimmsSymNum;
  }
  if (pReserveDimm != NULL) {
    pDimmsAndReserveDimm[DimmsAndReserveDimmNum] = pReserveDimm;
    DimmsAndReserveDimmNum++;
  }

  if (DimmsAndReserveDimmNum == 0) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = ValidateRegionsCorrelations(pDimmsAndReserveDimm, DimmsAndReserveDimmNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }


  ReturnCode = FindRelatedDimmsByRegionGoalConfigs(pDimmsAndReserveDimm, DimmsAndReserveDimmNum, pRelatedDimms, &RelatedDimmsNum);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Get current list of Regions Goal that will be freed at the end. **/
  ReturnCode = FindUniqueRegionsGoal(pRelatedDimms, RelatedDimmsNum, pExistingRegionsGoal, &ExistingRegionsGoalNum);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Socket = 0; Socket < MAX_SOCKETS; Socket++) {
    RelatedDimmsOnSocketNum = 0;
    SpecifiedDimmsOnSocketNum = 0;
    SpecifiedDimmsOnSocketAsymNum = 0;
    for (Index = 0; Index < RelatedDimmsNum; Index++) {
      if (Socket == pRelatedDimms[Index]->SocketId && pRelatedDimms[Index] != pReserveDimm) {
        pRelatedDimmsOnSocket[RelatedDimmsOnSocketNum] = pRelatedDimms[Index];
        RelatedDimmsOnSocketNum++;

        /** Get array of specified DIMMs by user for a given socket **/
        pRegionGoalDimm = FindRegionGoalDimm(DimmsSym, DimmsSymNum, pRelatedDimms[Index]);
        if (pRegionGoalDimm != NULL) {
          pSpecifiedDimmsOnSocket[SpecifiedDimmsOnSocketNum] = pRegionGoalDimm->pDimm;
          SpecifiedDimmsOnSocketRegionSize[SpecifiedDimmsOnSocketNum] = pRegionGoalDimm->RegionSize;
          SpecifiedDimmsOnSocketNum++;
        }

        pRegionGoalDimm = FindRegionGoalDimm(DimmsAsym, DimmsAsymNum, pRelatedDimms[Index]);
        if (pRegionGoalDimm != NULL) {
          pSpecifiedDimmsOnSocketAsym[SpecifiedDimmsOnSocketAsymNum] = pRegionGoalDimm->pDimm;
          SpecifiedDimmsOnSocketAsymRegionSize[SpecifiedDimmsOnSocketAsymNum] = pRegionGoalDimm->RegionSize;
          SpecifiedDimmsOnSocketAsymNum++;
        }
      }
    }

    if (SpecifiedDimmsOnSocketNum == 0) {
      continue;
    }

    ExistingRegionsNumOnSocket = 0;

    /** Create goal interleave sets and validate them **/

    /** Interleaved and mirrored **/
    for (Index = 0, NewRegionsGoalNum = 0; Index < RegionGoalTemplatesNum; Index++) {
      if (RegionGoalTemplates[Index].InterleaveSetType == NON_INTERLEAVED) {
        continue;
      }

      RegionGoalSequenceIndex = Index;

      /** Need to split REGION into sockets. So we have to calculate a size of REGION on a given socket. **/
      if (RegionGoalTemplates[Index].Asymmetrical) {
        if (SpecifiedDimmsOnSocketAsymNum == 0) {
          continue;
        }

        InterleaveSetSize = 0;

        for (Index2 = 0; Index2 < SpecifiedDimmsOnSocketAsymNum; Index2++) {
          InterleaveSetSize += SpecifiedDimmsOnSocketAsymRegionSize[Index2];
        }

        ReturnCode = PerformInterleavingAndCreateGoal(&RegionGoalTemplates[Index], pSpecifiedDimmsOnSocketAsym,
                         SpecifiedDimmsOnSocketAsymNum, InterleaveSetSize, pDriverPreferences,
                         (UINT16)RegionGoalSequenceIndex, pNewRegionsGoal, &NewRegionsGoalNum, &AvailableISIndex);
      } else {
        InterleaveSetSize = 0;

        for (Index2 = 0; Index2 < SpecifiedDimmsOnSocketNum; Index2++) {
          InterleaveSetSize += SpecifiedDimmsOnSocketRegionSize[Index2];
        }

        ReturnCode = PerformInterleavingAndCreateGoal(&RegionGoalTemplates[Index], pSpecifiedDimmsOnSocket,
                         SpecifiedDimmsOnSocketNum, InterleaveSetSize, pDriverPreferences,
                         (UINT16)RegionGoalSequenceIndex, pNewRegionsGoal, &NewRegionsGoalNum, &AvailableISIndex);
      }
      if (EFI_ERROR(ReturnCode)) {
        goto FinishSimpleClean;
      }
    }

    /** Non-interleaved **/
    for (Index = 0; Index < RegionGoalTemplatesNum; Index++) {
      if (RegionGoalTemplates[Index].InterleaveSetType != NON_INTERLEAVED) {
        continue;
      }

      RegionGoalSequenceIndex = Index;

      if (RegionGoalTemplates[Index].Asymmetrical) {
        for (Index2 = 0; Index2 < SpecifiedDimmsOnSocketAsymNum; Index2++) {
          InterleaveSetSize = SpecifiedDimmsOnSocketAsymRegionSize[Index2];

          /* Corner case, Ex: On some platforms with 5 Dimms per socket
          AD Interleaved mode with all asymmetric DIMMs (different capacities)
          Instead of having one AD interleaved region we can have more */
          if (NewRegionsGoalNum == MaxPMInterleaveSets.MaxInterleaveSetsSplit.PerDie) {
            SetObjStatus(pCommandStatus, 0, NULL, 0, NVM_WARN_REGION_AD_NI_PM_INTERLEAVE_SETS_REDUCED);
            break;
          }

          pNewRegionsGoal[NewRegionsGoalNum] = CreateRegionGoal(&RegionGoalTemplates[Index],
              &pSpecifiedDimmsOnSocketAsym[Index2], 1, InterleaveSetSize, pDriverPreferences,
              (UINT16)RegionGoalSequenceIndex, &AvailableISIndex);
          if (pNewRegionsGoal[NewRegionsGoalNum] == NULL) {
            ReturnCode = EFI_OUT_OF_RESOURCES;
            goto FinishSimpleClean;
          }

          NewRegionsGoalNum++;
        }
      } else {
        for (Index2 = 0; Index2 < SpecifiedDimmsOnSocketNum; Index2++) {
          InterleaveSetSize = SpecifiedDimmsOnSocketRegionSize[Index2];

          /* For AD non-interleaved, only MaxPMInterleaveSetsPerDie DIMMs configured as AD X1, others are unmapped
          Goal cannot be created of size Zero
          Count the no of existing AD x1 regions on these DCPMMs, if any */
          if (InterleaveSetSize == 0) {
            if (IsDimmManageable(pSpecifiedDimmsOnSocket[Index2])) {
              for (Index = 0; Index < pSpecifiedDimmsOnSocket[Index2]->ISsNum; Index++) {
                if (pSpecifiedDimmsOnSocket[Index2]->pISs[Index]->InterleaveFormatWays == INTERLEAVE_SET_1_WAY) {
                  ExistingRegionsNumOnSocket++;
                }
              }
            }
            continue;
          }

          pNewRegionsGoal[NewRegionsGoalNum] = CreateRegionGoal(&RegionGoalTemplates[Index],
              &pSpecifiedDimmsOnSocket[Index2], 1, InterleaveSetSize, pDriverPreferences,
              (UINT16)RegionGoalSequenceIndex, &AvailableISIndex);
          if (pNewRegionsGoal[NewRegionsGoalNum] == NULL) {
            ReturnCode = EFI_OUT_OF_RESOURCES;
            goto FinishSimpleClean;
          }

          NewRegionsGoalNum++;
        }

        ReturnCode = IsConfigureWholeSocket(SpecifiedDimmsOnSocketNum, Socket, &WholeSocket);
        if (EFI_ERROR(ReturnCode)) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
          NVDIMM_DBG("Unable to determine if goal request is configuring entire socket or adding unconfigured dimms");
          goto Finish;
        }

        /* Corner case, Ex: On some platforms, with 6 AD X1s and 2 Dimms with unmapped capacities
        If we delete PCD on 2 Dimms with inaccessible memory and request goal creation with AD non-interleaved
        targeting these 2 Dimms, the total no of regions can exceed the MaxPMInterleaveSetsPerDie limit */
        if (!WholeSocket) {
          LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
            pDimm = DIMM_FROM_NODE(pDimmNode);

            if (!IsDimmManageable(pDimm)) {
              continue;
            }

            if (pDimm->SocketId == Socket && (!IsPointerInArray((VOID **)pSpecifiedDimmsOnSocket, SpecifiedDimmsOnSocketNum, pDimm))) {
              for (Index = 0; Index < pDimm->ISsNum; Index++) {
                if (pDimm->pISs[Index]->InterleaveFormatWays == INTERLEAVE_SET_1_WAY) {
                  ExistingRegionsNumOnSocket++;
                }
              }
            }
          }
        }

        if (IS_ACPI_REV_MAJ_1_MIN_VALID(PcatRevision) &&
          ((ExistingRegionsNumOnSocket + NewRegionsGoalNum) > MaxPMInterleaveSets.MaxInterleaveSetsSplit.PerDie)) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_MAX_PM_INTERLEAVE_SETS_EXCEEDED);
          ReturnCode = EFI_ABORTED;
          goto Finish;
        }
      }
    }

    ReturnCode = VerifyInterleaveSetsPlatformSupport(pNewRegionsGoal, NewRegionsGoalNum, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishSimpleClean;
    }

    /** If all goal interleave sets have been validated successfully, then we can assign them to DIMMs **/

    for (Index = 0; Index < RelatedDimmsOnSocketNum; Index++) {
      pRelatedDimmsOnSocket[Index]->RegionsGoalNum = 0;
    }

    for (Index = 0; Index < NewRegionsGoalNum; Index++) {
      for (Index2 = 0; Index2 < pNewRegionsGoal[Index]->DimmsNum; Index2++) {
        pDimm = pNewRegionsGoal[Index]->pDimms[Index2];
        RegionGoalSequenceIndex = pNewRegionsGoal[Index]->SequenceIndex;
        pDimm->pRegionsGoal[RegionGoalSequenceIndex] = pNewRegionsGoal[Index];
        /** Determine a number of Regions Goal for Dimm**/
        if (RegionGoalSequenceIndex + 1 > pDimm->RegionsGoalNum) {
          pDimm->RegionsGoalNum = RegionGoalSequenceIndex + 1;
        }
      }
    }

    for (Index = 0; Index < RelatedDimmsOnSocketNum; Index++) {
      pRegionGoalDimm = FindRegionGoalDimm(DimmsSym, DimmsSymNum, pRelatedDimmsOnSocket[Index]);
      if (pRegionGoalDimm != NULL) {
        pRelatedDimmsOnSocket[Index]->RegionsGoalConfig = TRUE;

        pRelatedDimmsOnSocket[Index]->VolatileSizeGoal = pRegionGoalDimm->VolatileSize;

      } else {
        pRelatedDimmsOnSocket[Index]->RegionsGoalConfig = FALSE;
        for (Index2 = 0; Index2 < MAX_IS_PER_DIMM; Index2++) {
          pRelatedDimmsOnSocket[Index]->pRegionsGoal[Index2] = NULL;
        }
        pRelatedDimmsOnSocket[Index]->RegionsGoalNum = 0;
        pRelatedDimmsOnSocket[Index]->VolatileSizeGoal = 0;
      }
      pRelatedDimmsOnSocket[Index]->GoalConfigStatus = GOAL_CONFIG_STATUS_NEW;
      pRelatedDimmsOnSocket[Index]->PcdSynced = FALSE;
    }
  }

  if (pReserveDimm != NULL) {
    if (ReserveDimmType == RESERVE_DIMM_AD_NOT_INTERLEAVED) {
      ResDimmRegionGoalTemplate.InterleaveSetType = NON_INTERLEAVED;
      ResDimmRegionGoalTemplate.Size = ROUNDDOWN(pReserveDimm->RawCapacity,
          gNvmDimmData->Alignments.RegionPersistentAlignment);

      pReserveDimmRegionsGoal = CreateRegionGoal(&ResDimmRegionGoalTemplate,
          &pReserveDimm, 1, ResDimmRegionGoalTemplate.Size, pDriverPreferences, 0, &AvailableISIndex);

      if (pReserveDimmRegionsGoal == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto FinishAdvanceClean;
      }

      ReturnCode = VerifyInterleaveSetsPlatformSupport(&pReserveDimmRegionsGoal, 1, pCommandStatus);
      if (EFI_ERROR(ReturnCode)) {
        FREE_POOL_SAFE(pReserveDimmRegionsGoal);
        goto FinishAdvanceClean;
      }

      pReserveDimm->RegionsGoalNum = 1;
      pReserveDimm->pRegionsGoal[0] = pReserveDimmRegionsGoal;

    } else {
      pReserveDimm->RegionsGoalNum = 0;
    }

    pReserveDimm->RegionsGoalConfig = TRUE;
    pReserveDimm->VolatileSizeGoal = 0;
    pReserveDimm->GoalConfigStatus = GOAL_CONFIG_STATUS_NEW;
    pReserveDimm->PcdSynced = FALSE;
  }

  for (Index = 0; Index < ExistingRegionsGoalNum; Index++) {
    FREE_POOL_SAFE(pExistingRegionsGoal[Index]);
  }

  goto Finish;

FinishSimpleClean:
  for (Index = 0; Index < NewRegionsGoalNum; Index++) {
    FREE_POOL_SAFE(pNewRegionsGoal[Index]);
  }
FinishAdvanceClean:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Delete regions goal configs from input DIMMs and (if force is true) all DIMMs related by regions

  @param[in, out] pDimms Array of pointers to DIMMs
  @param[in] DimmsNum Number of pointers in pDimms
  @param[in] Force Force to perform deleting regions goal configs on all affected DIMMs
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or input DIMMs are different than affected DIMMs
**/
EFI_STATUS
DeleteRegionsGoalConfigs(
  IN OUT DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  DIMM *pRelatedDimms[MAX_DIMMS];
  UINT32 RelatedDimmsNum = 0;
  REGION_GOAL *pExistingRegionsGoal[MAX_IS_CONFIGS];
  UINT32 ExistingRegionsGoalNum = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pCommandStatus == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  SetMem(pRelatedDimms, sizeof(pRelatedDimms), 0x0);
  SetMem(pExistingRegionsGoal, sizeof(pExistingRegionsGoal), 0x0);

  Rc = FindRelatedDimmsByRegionGoalConfigs(pDimms, DimmsNum, pRelatedDimms, &RelatedDimmsNum);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }

  Rc = FindUniqueRegionsGoal(pRelatedDimms, RelatedDimmsNum, pExistingRegionsGoal, &ExistingRegionsGoalNum);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }

  /** The found list of DIMMs doesn't have the same elements as the input list **/
  if (DimmsNum != RelatedDimmsNum) {
    for (Index = 0; Index < RelatedDimmsNum; Index++) {
      if (!IsPointerInArray((VOID **) pDimms, DimmsNum, pRelatedDimms[Index])) {
        SetObjStatusForDimm(pCommandStatus, pRelatedDimms[Index],
            NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM);
      }
    }

    Rc = EFI_INVALID_PARAMETER;
    SetCmdStatus(pCommandStatus, NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM);
    goto Finish;
  }

  for (Index = 0; Index < RelatedDimmsNum; Index++) {
    if (pRelatedDimms[Index]->RegionsGoalConfig) {
      pRelatedDimms[Index]->RegionsGoalConfig = FALSE;
      for (Index2 = 0; Index2 < MAX_IS_PER_DIMM; Index2++) {
        pRelatedDimms[Index]->pRegionsGoal[Index2] = NULL;
      }
      pRelatedDimms[Index]->RegionsGoalNum = 0;
      pRelatedDimms[Index]->VolatileSizeGoal = 0;
      pRelatedDimms[Index]->GoalConfigStatus = GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS;
      pRelatedDimms[Index]->PcdSynced = FALSE;
    } else {
      pRelatedDimms[Index]->PcdSynced = TRUE;
      SetObjStatusForDimm(pCommandStatus, pRelatedDimms[Index], NVM_ERR_REGION_NO_GOAL_EXISTS_ON_DIMM);
    }
  }

  ClearRegionsGoal(pRelatedDimms, RelatedDimmsNum, pExistingRegionsGoal, ExistingRegionsGoalNum);

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}
/**
  Check if specified persistent memory type contain valid value

  @param[in] PersistentMemType Persistent memory type

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Specified value is invalid
**/
EFI_STATUS
PersistentMemoryTypeValidation(
  IN     UINT8 PersistentMemType
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  switch (PersistentMemType) {
  case PM_TYPE_AD:
  case PM_TYPE_AD_NI:
  case PM_TYPE_RESERVED:
    ReturnCode = EFI_SUCCESS;
    break;
  default:
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Select one reserve Dimm from specified list of Dimms and remove it from the list

  @param[in, out] pDimms Array of pointers to DIMMs
  @param[in, out] pDimmsNum Number of pointers in pDimms
  @param[out] ppReserveDimm Selected Dimm from the list

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
SelectReserveDimm(
  IN OUT DIMM *pDimms[MAX_DIMMS],
  IN OUT UINT32 *pDimmsNum,
     OUT DIMM **ppReserveDimm
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT32 SelectedIndex = 0;
  BOOLEAN Found = FALSE;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsNum == NULL || ppReserveDimm == NULL || *pDimmsNum < 1) {
    goto Finish;
  }

  /** Get the last manageable one from array as reserve Dimm **/
  SelectedIndex = *pDimmsNum;
  while (!Found && (SelectedIndex > 0)) {
    SelectedIndex--;

    *ppReserveDimm = pDimms[SelectedIndex];
    Found = TRUE;

    /** Remove reserve Dimm from array **/
    (*pDimmsNum)--;
    for (Index = SelectedIndex; Index < *pDimmsNum; Index++) {
      pDimms[Index] = pDimms[Index + 1];
    }

    ReturnCode = EFI_SUCCESS;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate system wide capacity for a given percent

  @param[in] pDimms Array of pointers to DIMMs
  @param[in] pDimmsNum Number of pointers in pDimms
  @param[in] Percent Percent to calculate
  @param[out] pDimmsCapacity Output dimms capacity in bytes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
CalculateDimmCapacityFromPercent(
  IN     DIMM *pDimms[MAX_DIMMS],
  IN     UINT32 DimmsNum,
  IN     UINT32 Percent,
     OUT UINT64 *pDimmsCapacity
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT64 DimmsCapacity = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsCapacity == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    DimmsCapacity += pDimms[Index]->RawCapacity;
  }
  *pDimmsCapacity = DimmsCapacity * Percent / 100;

  ReturnCode =  EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
/**
  Verify that all specified features in goal config are supported by platform

  @param[in] VolatileSize Volatile region size
  @param[in] PersistentMemType Persistent memory type
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER If one or more parameters are NULL
  @retval EFI_UNSUPPORTED A given config is unsupported
  @retval EFI_LOAD_ERROR There is no PCAT
**/
EFI_STATUS
VerifyPlatformSupport(
  IN     UINT64 VolatileSize,
  IN     UINT8 PersistentMemType,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  BOOLEAN AppDirect = FALSE;
  BOOLEAN ConfigChangeSupported = FALSE;
  MEMORY_MODE_CAPABILITIES MemModeCapabilities;

  NVDIMM_ENTRY();

  if (pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ZeroMem(&MemModeCapabilities, sizeof(MEMORY_MODE_CAPABILITIES));

  /** Check if BIOS supports changing configuration through management software **/
  CHECK_RESULT(CheckIfBiosSupportsConfigChange(&ConfigChangeSupported), Finish);
  if (!ConfigChangeSupported) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_MANAGEMENT_SOFT);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  CHECK_RESULT(CheckMemModeCapabilities(&MemModeCapabilities), Finish);
  /** Check if the platform supports 2LM Mode **/
  if (VolatileSize > 0 && !MemModeCapabilities.MemoryModesFlags.Memory) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  /** Check if the platform supports PM-Direct or PM-Cached Mode **/
  AppDirect = PersistentMemType == PM_TYPE_AD || PersistentMemType == PM_TYPE_AD_NI;
  if (AppDirect && !MemModeCapabilities.MemoryModesFlags.AppDirect) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_PM_MODE);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  /** Check if the platform supports Mixed Mode **/
  if (VolatileSize > 0 && AppDirect && !MemModeCapabilities.MemoryModesFlags.MixedMode) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_MIXED_MODE);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check if specified AppDirect Settings contain valid values

  @param[in] pDriverPreferences Driver preferences for AppDirect Provisioning

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER parameter is NULL or specified values are not valid
  @retval EFI_ABORTED Unable to find required system tables
**/
EFI_STATUS
AppDirectSettingsValidation(
  IN     DRIVER_PREFERENCES *pDriverPreferences
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  BOOLEAN Found = FALSE;
  UINT32 *pChannelInterleaveSize = NULL;
  UINT32 *piMCInterleaveSize = NULL;
  UINT32 InterleaveSizesSupportedLength = 0;

  NVDIMM_ENTRY();

  if (pDriverPreferences == NULL) {
    goto Finish;
  }

  if (pDriverPreferences->AppDirectGranularity > APPDIRECT_GRANULARITY_MAX) {
    goto Finish;
  }

  /** XOR - both properties has to be set as default or both unset **/
  if ((pDriverPreferences->ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE) !=
      (pDriverPreferences->ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE)) {
    goto Finish;
  }

  ReturnCode = RetrieveSupportediMcAndChannelInterleaveSizes(&pChannelInterleaveSize, &piMCInterleaveSize, NULL, NULL, &InterleaveSizesSupportedLength, NULL, NULL);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to retrieve supported iMc & Channel Interleave sizes from PCAT");
    goto Finish;
  }

  if (pDriverPreferences->ImcInterleaving != DEFAULT_IMC_INTERLEAVE_SIZE) {
    Found = FALSE;

    for (Index = 0; Index < InterleaveSizesSupportedLength; Index++) {
      if (pDriverPreferences->ImcInterleaving & piMCInterleaveSize[Index]) {
        Found = TRUE;
        break;
      }
    }
    if (!Found) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  if (pDriverPreferences->ChannelInterleaving != DEFAULT_CHANNEL_INTERLEAVE_SIZE) {
    Found = FALSE;

    for (Index = 0; Index < InterleaveSizesSupportedLength; Index++) {
      if (pDriverPreferences->ChannelInterleaving & pChannelInterleaveSize[Index]) {
        Found = TRUE;
        break;
      }
    }
    if (!Found) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pChannelInterleaveSize);
  FREE_POOL_SAFE(piMCInterleaveSize);
  return ReturnCode;
}


/**
  Check if specified AppDirect Settings will conflict with existing AppDirect Interleaves

  @param[in] pDriverPreferences Driver preferences for AppDirect Provisioning
  @param[out] pConflict True, conflict exists with existing AppDirect Memory
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER parameter is NULL
**/
EFI_STATUS
AppDirectSettingsConflict(
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT BOOLEAN *pConflict,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  struct _NVM_IS *pNvm_IS = NULL;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT8 RequestedImcInterleaving = 0;
  UINT8 RequestedChannelInterleaving = 0;
  UINT32 *pChannelInterleaveSize = NULL;
  UINT32 *piMCInterleaveSize = NULL;
  UINT32 *pRecommendedFormats = NULL;
  UINT32 InterleaveSizesSupportedLength = 0;

  NVDIMM_ENTRY();

  *pConflict = FALSE;

  if (pCommandStatus == NULL ||
    pDriverPreferences == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = RetrieveSupportediMcAndChannelInterleaveSizes(&pChannelInterleaveSize, &piMCInterleaveSize,
    &pRecommendedFormats, NULL, &InterleaveSizesSupportedLength, NULL, NULL);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to retrieve supported iMc & Channel Interleave sizes from PCAT");
    goto Finish;
  }

  /** If default we have to retrieve the default settings from PCAT **/
  if (pDriverPreferences->ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE &&
      pDriverPreferences->ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE) {
    if (pRecommendedFormats != NULL) {
      for (Index2 = 0; Index2 < InterleaveSizesSupportedLength; Index2++) {
        if (IS_BIT_SET_VAR(pRecommendedFormats[Index2], BIT0)) {
          RequestedImcInterleaving = (UINT8)piMCInterleaveSize[Index2];
          RequestedChannelInterleaving = (UINT8)pChannelInterleaveSize[Index2];
          break;
        }
      }
    }
    else {
      RequestedImcInterleaving = (UINT8)piMCInterleaveSize[Index2];
      RequestedChannelInterleaving = (UINT8)pChannelInterleaveSize[Index2];
    }
  } else {
    RequestedChannelInterleaving = pDriverPreferences->ChannelInterleaving;
    RequestedImcInterleaving = pDriverPreferences->ImcInterleaving;
  }

  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    if (pDimm->Configured) {
      for (Index = 0; Index < pDimm->ISsNum; Index++) {
        pNvm_IS = pDimm->pISs[Index];
        if (pNvm_IS->InterleaveFormatWays > 0 &&
          (pNvm_IS->InterleaveFormatImc != RequestedImcInterleaving ||
          pNvm_IS->InterleaveFormatChannel != RequestedChannelInterleaving)) {
          *pConflict = TRUE;
          break;
        }
      }
    }

    if (*pConflict) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_APPDIRECT_IN_SYSTEM);
      break;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pChannelInterleaveSize);
  FREE_POOL_SAFE(piMCInterleaveSize);
  FREE_POOL_SAFE(pRecommendedFormats);
  return ReturnCode;
}

/**
  For a given set of region goal dimms reduce the volatile capacity of the region
  based on the requested reserved size.

  It is assumed that no region capacity exists on the region goals.

  @param[in out] pReservedSize Size to reduce the region capacity
  @param[in out] RegionGoalDimms Array of region goal dimms to reduce
  @param[in out] pRegionGoalDimmsNum number of elements in the array

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_ABORTED persistent capacity discovered on one or more region goals.
**/

STATIC
EFI_STATUS
ReduceVolatileCapacityPerReservedCapacity(
  IN OUT UINT64 *pReservedSize,
  IN OUT REGION_GOAL_DIMM RegionGoalDimms[MAX_DIMMS],
  IN OUT UINT32 *pRegionGoalDimmsNum
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 TotalRegionGoalCapacity = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT64 ReduceBy = 0;
  UINT64 CurrentLargestDimmm = 0;
  UINT64 SecondLargestDimm = 0;
  UINT32 NumOfLargestDimms = 0;
  UINT64 MaxReducePerDIMM = 0;

  NVDIMM_ENTRY();

  if (pReservedSize == NULL || RegionGoalDimms == NULL || pRegionGoalDimmsNum == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (*pReservedSize == 0) {
    goto Finish;
  }

  // If capacity in the dimms is less than the amount requested
  // then take all capacity in dimms
  for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
    TotalRegionGoalCapacity += RegionGoalDimms[Index].VolatileSize;

    if (RegionGoalDimms[Index].RegionSize > 0) {
      NVDIMM_DBG("Cannot reduce volatile capacity while region capacity exists");
      ReturnCode = EFI_ABORTED;
    }
  }

  if (TotalRegionGoalCapacity <= *pReservedSize) {
    for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
      *pReservedSize -= RegionGoalDimms[Index].VolatileSize;
    }
    *pRegionGoalDimmsNum = 0;
  } else {
    // When reducing capacity we don't want to stop unless we have consumed all the goals or
    // we have reduced the requested amount. Reduce by consuming the asymmetrical size segments first.
    for (Index2 = 0; Index2 < *pRegionGoalDimmsNum; Index2++) {
      CurrentLargestDimmm = 0;
      SecondLargestDimm = 0;
      NumOfLargestDimms = 0;
      MaxReducePerDIMM = 0;

      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        CurrentLargestDimmm =
          (RegionGoalDimms[Index].VolatileSize > CurrentLargestDimmm) ?
            RegionGoalDimms[Index].VolatileSize : CurrentLargestDimmm;
      }

      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        SecondLargestDimm =
          (RegionGoalDimms[Index].VolatileSize != CurrentLargestDimmm
            && RegionGoalDimms[Index].VolatileSize > SecondLargestDimm) ?
            RegionGoalDimms[Index].VolatileSize : SecondLargestDimm;
      }

      MaxReducePerDIMM = CurrentLargestDimmm - SecondLargestDimm;

      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        if (RegionGoalDimms[Index].VolatileSize == CurrentLargestDimmm) {
          NumOfLargestDimms++;
        }
      }

      if (NumOfLargestDimms == 0) {
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }

      // If reserved capacity does not consume the dimms then try to reduce each dimm evenly in allocations
      // of aligned volatile capacity. volatile size should already be aligned to Volatile Alignment
      ReduceBy = *pReservedSize / NumOfLargestDimms;
      ReduceBy = ROUNDUP(ReduceBy, gNvmDimmData->Alignments.RegionVolatileAlignment);
      ReduceBy = ReduceBy > MaxReducePerDIMM ? MaxReducePerDIMM : ReduceBy;

      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {

        if (RegionGoalDimms[Index].VolatileSize != CurrentLargestDimmm) {
          continue;
        }

        // reduce each DIMM evenly if possible
        if (RegionGoalDimms[Index].VolatileSize >= ReduceBy && *pReservedSize >= ReduceBy) {
          RegionGoalDimms[Index].VolatileSize -= ReduceBy;
          *pReservedSize -= ReduceBy;
        } else if (RegionGoalDimms[Index].VolatileSize >= ReduceBy && *pReservedSize < ReduceBy) {
          RegionGoalDimms[Index].VolatileSize -= ReduceBy;
          *pReservedSize = 0;
        } else {
          *pReservedSize -= RegionGoalDimms[Index].VolatileSize;
          RegionGoalDimms[Index].VolatileSize = 0;
        }
      }

      // Recalculate Total remaining capacity for next loop
      for (Index = 0; Index < *pRegionGoalDimmsNum; Index++) {
        TotalRegionGoalCapacity += RegionGoalDimms[Index].VolatileSize;
      }

      if (TotalRegionGoalCapacity == 0 || *pReservedSize == 0) {
        break;
      }
    }

    if (TotalRegionGoalCapacity > 0 && *pReservedSize > 0) {
      ReturnCode = EFI_DEVICE_ERROR;
      NVDIMM_DBG("Unable to correctly map reserved capacity");
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Reduce system wide socket mapped memory to align with the system memory mapped SKU limits per Socket.
  @param[in] Socket  Socket Id for SKU limit calculations
  @param[in] pDimms Array of pointers to manageable DIMMs only
  @param[in] NumDimmsOnSocket Number of pointers in pDimms
  @param[in out] DimmsSymmetricalOnSocket Array of Dimms for symmetrical region config
  @param[in out] pDimmsSymmetricalNumOnSocket Returned number of items in DimmsSymmetrical
  @param[in out] DimmsAsymmetricalOnSocket Array of Dimms for asymmetrical region config
  @param[in out] pDimmsAsymmetricalNumOnSocket Returned number of items in DimmsAsymmetrical
  @param[in out] RegionGoalTemplates Array of region goal templates
  @param[in out] pRegionGoalTemplatesNum Number of items in RegionGoalTemplates
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_UNSUPPORTED Bad values retrieved from PCAT
**/
EFI_STATUS
ReduceCapacityForSocketSKU(
  IN     UINT32 Socket,
  IN     DIMM *pDimmsOnSocket[MAX_DIMMS],
  IN     UINT32 NumDimmsOnSocket,
  IN OUT REGION_GOAL_DIMM DimmsSymmetricalOnSocket[MAX_DIMMS],
  IN OUT UINT32 *pDimmsSymmetricalNumOnSocket,
  IN OUT REGION_GOAL_DIMM DimmsAsymmetricalOnSocket[MAX_DIMMS],
  IN OUT UINT32 *pDimmsAsymmetricalNumOnSocket,
  IN OUT REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM],
  IN OUT UINT32 *pRegionGoalTemplatesNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN WholeSocket = FALSE;
  UINT64 TotalRequestedMemoryOnSocket = 0;
  BOOLEAN NewConfigurationMemoryMode = FALSE;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 Index = 0;
  UINT64 ReduceCapacity = 0;
  UINT64 MappedMemorySizeLimit = 0;
  UINT64 DDRRawCapacity = 0;

  NVDIMM_ENTRY();

  if (pDimmsOnSocket == NULL || NumDimmsOnSocket == 0 ||
      DimmsSymmetricalOnSocket == NULL || pDimmsSymmetricalNumOnSocket == NULL ||
      DimmsAsymmetricalOnSocket == NULL || pDimmsAsymmetricalNumOnSocket == NULL ||
      RegionGoalTemplates == NULL || pRegionGoalTemplatesNum == NULL ||
      pCommandStatus == NULL) {
    goto Finish;
  }


  pCommandStatus->ObjectType = ObjectTypeSocket;

  TotalRequestedMemoryOnSocket = 0;

  ReturnCode = IsConfigureWholeSocket(NumDimmsOnSocket, Socket, &WholeSocket);

  if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    NVDIMM_DBG("Unable to determine if goal request is configuring entire socket or adding unconfigured dimms");
    goto Finish;
  }

  ReturnCode = RetrievePcatSocketSkuMappedMemoryLimit(Socket, &MappedMemorySizeLimit);
  // If no PCAT tables exist for a socket then that socket will not be reduced.
  if (ReturnCode == EFI_NOT_FOUND) {
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  } else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to retrieve socket sku info table for socket");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  // MemoryMode only exists on symmetrical dimms objects
  for (Index = 0; Index < *pDimmsSymmetricalNumOnSocket; Index++) {
    if (DimmsSymmetricalOnSocket[Index].VolatileSize > 0) {
      NewConfigurationMemoryMode = TRUE;
      break;
    }
  }

  // If we are adding just a single dimm then the system memory mode
  // can be defined by pre-existing dimms
  if (!NewConfigurationMemoryMode && !WholeSocket) {
    LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);

      if (Socket == pDimm->SocketId &&
        IsDimmManageable(pDimm) &&
        pDimm->MappedVolatileCapacity > 0 &&
        pDimm->Configured) {
        NewConfigurationMemoryMode = TRUE;
        break;
      }
    }
  }

  ReturnCode = GetDDRCapacities((UINT16)Socket, &DDRRawCapacity, NULL, NULL, NULL);
  if (EFI_ERROR(ReturnCode)) {
    // If not Purley, this is an error. On Purley we expect the PMTT table
    // to be missing on some platforms, workarounds are further below
    if (!IS_ACPI_HEADER_REV_MAJ_0_MIN_VALID(gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr)) {
      // Preserve return code and error out
      NVDIMM_DBG("Could not retrieve DDR raw capacity");
      goto Finish;
    }
    if (!NewConfigurationMemoryMode) {
      // None of the DDR is used as cache, it's fully volatile, so we need to
      // know the value to correctly calculate if we're violating the sku
      // limit. However, since we're on Purley and are probably missing the
      // PMTT table, for backwards compatibility we want to skip this ReduceCapacityForSocketSku
      // check and show a warning.
      ReturnCode = EFI_SUCCESS;
      SetCmdStatus(pCommandStatus, NVM_WARN_PMTT_TABLE_NOT_FOUND);
      goto Finish;
    }

    // All of the DDR is used as cache, so we don't need to know the current
    // DDR raw capacity, continue on without a warning
    DDRRawCapacity = 0;
    ReturnCode = EFI_SUCCESS;
  }

  for (Index = 0; Index < *pDimmsSymmetricalNumOnSocket; Index++) {
    TotalRequestedMemoryOnSocket += DimmsSymmetricalOnSocket[Index].RegionSize;
    TotalRequestedMemoryOnSocket += ROUNDDOWN(
        DimmsSymmetricalOnSocket[Index].VolatileSize,
        gNvmDimmData->Alignments.RegionVolatileAlignment);
  }

  for (Index = 0; Index < *pDimmsAsymmetricalNumOnSocket; Index++) {
    TotalRequestedMemoryOnSocket += DimmsAsymmetricalOnSocket[Index].RegionSize;
  }

  // Adding full DDR4 capacity only if new config will contain 1LM
  if (!NewConfigurationMemoryMode) {
    TotalRequestedMemoryOnSocket += DDRRawCapacity;
  }

  /**
    If adding a new dimm to a configured socket and the new configuration will be 1LM,
    then the total amount to be mapped will be old AD + new AD.
  **/
  if (!NewConfigurationMemoryMode && !WholeSocket) {

    LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);

      if (Socket == pDimm->SocketId && !IsPointerInArray((VOID **)pDimmsOnSocket, NumDimmsOnSocket, pDimm) && IsDimmManageable(pDimm)) {
        TotalRequestedMemoryOnSocket += pDimm->MappedPersistentCapacity;
      }
    }

  /**
    If adding a new dimm to a configured socket and the new configuration will be MemoryMode,
    then the total amount to be mapped will be All of the old AD + new AD + new MemoryMode + old MemoryMode.
  **/
  } else if (NewConfigurationMemoryMode && !WholeSocket) {
    LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);

      if (Socket == pDimm->SocketId && !IsPointerInArray((VOID **)pDimmsOnSocket, NumDimmsOnSocket, pDimm) && IsDimmManageable(pDimm)) {
        TotalRequestedMemoryOnSocket += pDimm->MappedPersistentCapacity;
        TotalRequestedMemoryOnSocket += pDimm->MappedVolatileCapacity;
      }
    }
  }

  // Reduce capacity on socket if larger than the amount we can map.
  if (TotalRequestedMemoryOnSocket > MappedMemorySizeLimit) {

    SetObjStatus(pCommandStatus, Socket, NULL, 0, NVM_WARN_MAPPED_MEM_REDUCED_DUE_TO_CPU_SKU);

    ReduceCapacity = TotalRequestedMemoryOnSocket - MappedMemorySizeLimit;

    // Reduce AppDirect first and then Volatile
    ReturnCode = ReduceAppDirectCapacityPerReservedCapacity
        (&ReduceCapacity, DimmsAsymmetricalOnSocket, pDimmsAsymmetricalNumOnSocket);
    if (EFI_ERROR(ReturnCode)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    ReturnCode = ReduceAppDirectCapacityPerReservedCapacity
        (&ReduceCapacity, DimmsSymmetricalOnSocket, pDimmsSymmetricalNumOnSocket);
    if (EFI_ERROR(ReturnCode)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    ReturnCode = ReduceVolatileCapacityPerReservedCapacity
        (&ReduceCapacity, DimmsSymmetricalOnSocket, pDimmsSymmetricalNumOnSocket);
    if (EFI_ERROR(ReturnCode)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    // If we consumed all AD and no volatile exists configure for PM_STORAGE
    if (*pDimmsSymmetricalNumOnSocket == 0) {
      for (Index = 0; Index < NumDimmsOnSocket; Index++) {
        DimmsSymmetricalOnSocket[Index].pDimm = pDimmsOnSocket[Index];
        DimmsSymmetricalOnSocket[Index].RegionSize = 0;
        DimmsSymmetricalOnSocket[Index].VolatileSize = 0;
        (*pDimmsSymmetricalNumOnSocket)++;
      }
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Verify DIMMs SKU support

  @param[in] ppDimms Array of dimms
  @param[in] DimmsNum Number of dimms
  @param[in, out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS No SKU violation
  @retval EFI_ABORTED SKU violation
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
VerifySKUSupportForCreateGoal(
  IN     DIMM **ppDimms,
  IN     UINT32 DimmsNum,
  IN OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  DIMM *pDimm = NULL;
  BOOLEAN VolatileSkuViolated = FALSE;
  BOOLEAN AppDirectSkuViolated = FALSE;

  NVDIMM_ENTRY();

  if (ppDimms == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    pDimm = ppDimms[Index];

    VolatileSkuViolated = pDimm->VolatileSizeGoal > 0 && pDimm->SkuInformation.MemoryModeEnabled == MODE_DISABLED;
    AppDirectSkuViolated = pDimm->RegionsGoalNum > 0 && pDimm->SkuInformation.AppDirectModeEnabled == MODE_DISABLED;


    if (VolatileSkuViolated || AppDirectSkuViolated) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_CONFIG_NOT_SUPPORTED_BY_CURRENT_SKU);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Clear previous regions goal configs and - if regions goal configs is specified - replace them with new one.

  1. Clear previous regions goal configs on all affected dimms
  2. [OPTIONAL] Send new regions goal configs to dimms
  3. Set information about synchronization with dimms

  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pDimmList is NULL
  @retval return codes from SendConfigInputToDimm
**/
EFI_STATUS
ApplyGoalConfigsToDimms(
  IN     DIMM **ppDimms,
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  NVDIMM_PLATFORM_CONFIG_INPUT *pNewConfigInput = NULL;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  CHECK_NULL_ARG(ppDimms, Finish);

  /**
    Clear previous regions goal configs
  **/
  for (Index = 0; Index < DimmsNum; Index++) {
    pDimm = ppDimms[Index];

    if (!IsDimmManageable(pDimm)) {
      continue;
    }
    if (pDimm->PcdSynced) {
      continue;
    }

    /** Remove Configuration Input table from Platform Config Data **/
    ReturnCode = SendConfigInputToDimm(pDimm, NULL);
    if (EFI_ERROR(ReturnCode)) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_REGION_CONF_APPLYING_FAILED);
      goto Finish;
    }
  }

  /**
    Send new regions goal configs to dimms
  **/
  for (Index = 0; Index < DimmsNum; Index++) {
    pDimm = ppDimms[Index];

    if (!IsDimmManageable(pDimm) || !pDimm->RegionsGoalConfig) {
      continue;
    }
    if (pDimm->PcdSynced) {
      continue;
    }

    ReturnCode = GeneratePcdConfInput(pDimm, &pNewConfigInput);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Generating Platform Config Data Configuration Input failed.");
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_REGION_CONF_APPLYING_FAILED);
      goto Finish;
    }

    ReturnCode = SendConfigInputToDimm(pDimm, pNewConfigInput);
    if (EFI_ERROR(ReturnCode)) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_REGION_CONF_APPLYING_FAILED);
      goto Finish;
    }
  }

  /**
    If all data has been sent to dimms successfully, then we are synchronized
  **/
  for (Index = 0; Index < DimmsNum; Index++) {
    pDimm = ppDimms[Index];

    if (!IsDimmManageable(pDimm)) {
      continue;
    }
    if (pDimm->PcdSynced) {
      continue;
    }
    SetObjStatusForDimm(pCommandStatus, pDimm, NVM_SUCCESS);
    pDimm->PcdSynced = TRUE;
  }
  SetCmdStatus(pCommandStatus, NVM_SUCCESS);

Finish:
  if (EFI_ERROR(ReturnCode) && (EFI_INVALID_PARAMETER != ReturnCode)) {
    // Create Goal ERROR! Try to remove Configuration Input table from Platform Config Data
    for (Index = 0; Index < DimmsNum; Index++) {
      pDimm = ppDimms[Index];

      if (!IsDimmManageable(pDimm)) {
        continue;
      }
      if (pDimm->PcdSynced) {
        continue;
      }
      SendConfigInputToDimm(pDimm, NULL);
    }
  }
  FREE_POOL_SAFE(pNewConfigInput);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Send new Configuration Input to dimm.

  Get Platform Config Data from dimm, replace Configuration Input with new one, and send it back
  to dimm. If pNewConfigInput is NULL, then the function will send Platform Config Data without
  Configuration Input (old one will be removed).

  @param[in] pDimm dimm that we replace Platform Config Data for
  @param[in] pNewConfigInput new config input for dimm

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pDimm is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval other error codes from called functions:
          GetPlatformConfigDataOemPartition, SetPlatformConfigDataOemPartition
**/
EFI_STATUS
SendConfigInputToDimm(
  IN     DIMM *pDimm,
  IN     NVDIMM_PLATFORM_CONFIG_INPUT *pNewConfigInput OPTIONAL
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVDIMM_CONFIGURATION_HEADER *pConfHeader = NULL;
  NVDIMM_CONFIGURATION_HEADER *pNewConfHeader = NULL;

  UINT32 PcdLength = 0;
  UINT32 CurrentOffset = 0;

  NVDIMM_ENTRY();

  if (pDimm == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Get current Platform Config Data from dimm **/
  Rc = GetPlatformConfigDataOemPartition(pDimm, TRUE, &pConfHeader);
#ifdef MEMORY_CORRUPTION_WA
  if (Rc == EFI_DEVICE_ERROR) {
	  Rc = GetPlatformConfigDataOemPartition(pDimm, TRUE, &pConfHeader);
  }
#endif // MEMORY_CORRUPTIO_WA
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }

  /**
    Create new Platform Config Data
  **/
  PcdLength = sizeof(NVDIMM_CONFIGURATION_HEADER) + pConfHeader->CurrentConfDataSize +
    (pNewConfigInput != NULL ? pNewConfigInput->Header.Length : 0) + pConfHeader->ConfOutputDataSize;

  pNewConfHeader = AllocateZeroPool(PcdLength);
  if (pNewConfHeader == NULL) {
    Rc = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  /** Copy Configuration Header table **/
  CopyMem_S(pNewConfHeader, PcdLength, pConfHeader, sizeof(*pConfHeader));

  CurrentOffset += sizeof(*pConfHeader);

  /** Copy Current Config table **/

  if (pConfHeader->CurrentConfStartOffset != 0 && pConfHeader->CurrentConfDataSize != 0) {
    CopyMem_S(
      (UINT8 *) pNewConfHeader + CurrentOffset,
      PcdLength - CurrentOffset,
      (UINT8 *) pConfHeader + pConfHeader->CurrentConfStartOffset,
      pConfHeader->CurrentConfDataSize);

    pNewConfHeader->CurrentConfStartOffset = CurrentOffset;
    pNewConfHeader->CurrentConfDataSize = pConfHeader->CurrentConfDataSize;
    CurrentOffset += pNewConfHeader->CurrentConfDataSize;
  } else {
    pNewConfHeader->CurrentConfStartOffset = 0;
    pNewConfHeader->CurrentConfDataSize = 0;
  }

  /** Copy new Configuration Input table **/
  if (pNewConfigInput != NULL) {
    CopyMem_S(
      (UINT8 *) pNewConfHeader + CurrentOffset,
      PcdLength - CurrentOffset,
      pNewConfigInput,
      pNewConfigInput->Header.Length);

    pNewConfHeader->ConfInputStartOffset = CurrentOffset;
    pNewConfHeader->ConfInputDataSize = pNewConfigInput->Header.Length;
    CurrentOffset += pNewConfHeader->ConfInputDataSize;

    /**
      Update Configuration Header Revision
      Not needed for Purley platforms, as only one
      revision (0x1) is supported.
    **/
    if (!IS_ACPI_HEADER_REV_MAJ_0_MIN_VALID(pNewConfHeader)) {
      CopyMem_S(&pNewConfHeader->Header.Revision, sizeof(ACPI_REVISION),
        &pNewConfigInput->Header.Revision, sizeof(ACPI_REVISION));
    }
  } else {
    pNewConfHeader->ConfInputStartOffset = 0;
    pNewConfHeader->ConfInputDataSize = 0;
  }

  /** Make COUT zero while applying a new CIN **/
  pNewConfHeader->ConfOutputStartOffset = 0;
  pNewConfHeader->ConfOutputDataSize = 0;

  GenerateChecksum(pNewConfHeader, pNewConfHeader->Header.Length, PCAT_TABLE_HEADER_CHECKSUM_OFFSET);

  /** Send new Platform Config Data back to dimm **/
  Rc = SetPlatformConfigDataOemPartition(pDimm, pNewConfHeader, PcdLength);
  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Failed to set Platform Config Data");
    goto Finish;
  }

Finish:
  if (pNewConfHeader != NULL) {
    FreePool(pNewConfHeader);
  }
  if (pConfHeader != NULL) {
    FreePool(pConfHeader);
  }
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Verify that specified Dimms don't affect other Dimms by current Regions or Regions goal configs

  @param[in, out] pDimms Array of pointers to DIMMs
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or specified Dimms affect other Dimms
**/
EFI_STATUS
ValidateRegionsCorrelations(
  IN OUT DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS Rc = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  DIMM *pRelatedDimms[MAX_DIMMS];
  UINT32 RelatedDimmsNum = 0;
  OBJECT_STATUS *pObjectStatus = NULL;
  NvmStatusCode LastNvmStatus = NVM_LAST_STATUS_VALUE;

  NVDIMM_ENTRY();

  SetMem(pRelatedDimms, sizeof(pRelatedDimms), 0x0);

  if (pDimms == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  Rc = FindRelatedDimmsByRegions(pDimms, DimmsNum, pRelatedDimms, &RelatedDimmsNum);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }
  if (DimmsNum != RelatedDimmsNum) {
    for (Index = 0; Index < RelatedDimmsNum; Index++) {
      if (!IsPointerInArray((VOID **) pDimms, DimmsNum, pRelatedDimms[Index])) {
        SetObjStatusForDimm(pCommandStatus, pRelatedDimms[Index],
            NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM);
      }
    }
    LastNvmStatus = NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM;
  }

  Rc = FindRelatedDimmsByRegionGoalConfigs(pDimms, DimmsNum, pRelatedDimms, &RelatedDimmsNum);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }

  if (DimmsNum != RelatedDimmsNum) {
    for (Index = 0; Index < RelatedDimmsNum; Index++) {
      if (!IsPointerInArray((VOID **) pDimms, DimmsNum, pRelatedDimms[Index])) {
        pObjectStatus = GetObjectStatus(pCommandStatus, pRelatedDimms[Index]->DeviceHandle.AsUint32);
        if (pObjectStatus != NULL) {
          if (LastNvmStatus == NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM) {
            SetObjStatusForDimm(pCommandStatus, pRelatedDimms[Index],
              NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM);
            LastNvmStatus = NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM;
          } else {
            SetObjStatusForDimm(pCommandStatus, pRelatedDimms[Index],
              NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM);
            LastNvmStatus = NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM;
          }
        } else {
          SetObjStatusForDimm(pCommandStatus, pRelatedDimms[Index],
              NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM);
          LastNvmStatus = NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM;
        }
      }
    }
  }

  if (LastNvmStatus == NVM_ERR_REGION_CURR_CONF_AFFECTS_UNSPEC_DIMM ||
      LastNvmStatus == NVM_ERR_REGION_GOAL_CONF_AFFECTS_UNSPEC_DIMM ||
      LastNvmStatus == NVM_ERR_REGION_GOAL_CURR_CONF_AFFECTS_UNSPEC_DIMM) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Rc = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Find related dimms based on region configs relations

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pRelatedDimms Output array of pointers to dimms
  @param[out] pRelatedDimmsNum Output number of found dimms

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
FindRelatedDimmsByRegions(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT DIMM *pRelatedDimms[MAX_DIMMS],
     OUT UINT32 *pRelatedDimmsNum
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  VOID *pDimmPointer = NULL;
  LIST_ENTRY *pDimmRegionNode = NULL;
  DIMM_REGION *pDimmRegion = NULL;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pRelatedDimms == NULL || pRelatedDimmsNum == NULL ||
    DimmsNum > MAX_DIMMS) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pRelatedDimmsNum = 0;

  for (Index = 0; Index < DimmsNum; Index++) {
    pRelatedDimms[Index] = pDimms[Index];
    (*pRelatedDimmsNum) += 1;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    //only check if dimm has been configured by bios, otherwise
    //could be a dimm from a broken interleave set (moved from a different platform)
    if (DIMM_CONFIG_SUCCESS == pDimms[Index]->ConfigStatus)
    {
      for (Index2 = 0; Index2 < pDimms[Index]->ISsNum; Index2++) {
        LIST_FOR_EACH(pDimmRegionNode, &pDimms[Index]->pISs[Index2]->DimmRegionList) {
          pDimmRegion = DIMM_REGION_FROM_NODE(pDimmRegionNode);
          pDimmPointer = pDimmRegion->pDimm;

          if (*pRelatedDimmsNum >= MAX_DIMMS) {
            NVDIMM_ERR("Found more Dimms than %d. Not possible in theory.", MAX_DIMMS);
            Rc = EFI_ABORTED;
            goto Finish;
          }
          else if (!IsPointerInArray((VOID **)pRelatedDimms, *pRelatedDimmsNum, pDimmPointer)) {
            ASSERT(*pRelatedDimmsNum < MAX_DIMMS);
            if (IsDimmManageable(pDimmPointer)) {
              pRelatedDimms[(*pRelatedDimmsNum)] = pDimmPointer;
              (*pRelatedDimmsNum) += 1;
            }
          }
        }
      }
    }
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Find related dimms based on region goal configs relations

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pRelatedDimms Output array of pointers to dimms
  @param[out] pRelatedDimmsNum Output number of found dimms

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
FindRelatedDimmsByRegionGoalConfigs(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT DIMM *pRelatedDimms[MAX_DIMMS],
     OUT UINT32 *pRelatedDimmsNum
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 Index3 = 0;
  VOID *pDimmPointer = NULL;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pRelatedDimms == NULL || pRelatedDimmsNum == NULL ||
    DimmsNum > MAX_DIMMS) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pRelatedDimmsNum = 0;

  for (Index = 0; Index < DimmsNum; Index++) {
    pRelatedDimms[(*pRelatedDimmsNum)] = pDimms[Index];
    (*pRelatedDimmsNum) += 1;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    for (Index2 = 0; Index2 < pDimms[Index]->RegionsGoalNum; Index2++) {
      for (Index3 = 0; Index3 < pDimms[Index]->pRegionsGoal[Index2]->DimmsNum; Index3++) {
        pDimmPointer = (VOID *) pDimms[Index]->pRegionsGoal[Index2]->pDimms[Index3];

        if (*pRelatedDimmsNum >= MAX_DIMMS) {
          NVDIMM_ERR("Found more Dimms than %d. Not possible in theory.", MAX_DIMMS);
          Rc = EFI_ABORTED;
          goto Finish;
        }
        else if (!IsPointerInArray((VOID **) pRelatedDimms, *pRelatedDimmsNum, pDimmPointer)) {
          ASSERT(*pRelatedDimmsNum < MAX_DIMMS);

          if (IsDimmManageable(pDimmPointer) && IsDimmInSupportedConfig(pDimmPointer)) {
             pRelatedDimms[(*pRelatedDimmsNum)] = pDimmPointer;
             (*pRelatedDimmsNum) += 1;
          }
        }
      }
    }
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Find an unique list of goal regions based on input list of dimms

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pRegionsGoal Output array of pointers to REGION_GOAL
  @param[out] pRegionsGoalNum Output number of unique goal regions

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_ABORTED More config goals found than can exist
**/
EFI_STATUS
FindUniqueRegionsGoal(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT REGION_GOAL *pRegionsGoal[MAX_IS_CONFIGS],
     OUT UINT32 *pRegionsGoalNum
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  VOID *pRegionGoalPointer = NULL;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pRegionsGoal == NULL || pRegionsGoalNum == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pRegionsGoalNum = 0;

  for (Index = 0; Index < DimmsNum; Index++) {
    for (Index2 = 0; Index2 < pDimms[Index]->RegionsGoalNum; Index2++) {
      pRegionGoalPointer = (VOID *) pDimms[Index]->pRegionsGoal[Index2];

      if (*pRegionsGoalNum >= MAX_IS_CONFIGS) {
        NVDIMM_ERR("Found more regions than %d. Not possible in theory.", MAX_IS_CONFIGS);
        Rc = EFI_ABORTED;
        goto Finish;
      }
      else if (!IsPointerInArray((VOID **) pRegionsGoal, *pRegionsGoalNum, pRegionGoalPointer)) {
        pRegionsGoal[(*pRegionsGoalNum)] = pRegionGoalPointer;
        (*pRegionsGoalNum)++;
      }
    }
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}


EFI_STATUS
ClearRegionsGoal(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     REGION_GOAL **pRegionsGoal,
  IN     UINT32 ExistingRegionsGoalNum
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 Index3 = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pRegionsGoal == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    for (Index2 = 0; Index2 < pDimms[Index]->RegionsGoalNum; Index2++) {
      if (pDimms[Index]->pRegionsGoal[Index2] == pRegionsGoal[Index3]) {
        pDimms[Index]->pRegionsGoal[Index2] = NULL;
      }
    }
  }

  for (Index = 0; Index < ExistingRegionsGoalNum; Index++) {
    FreePool(pRegionsGoal[Index]);
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Check if the platform supports specified interleave sets. Also set default iMC and Channel interleave sizes if
  they are not specified.

  @param[in, out]  pRegionGoal Array of pointers to regions goal
  @param[in]       pRegionGoal Number of pointers in pRegionGoal
  @param[out]      pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_LOAD_ERROR  PCAT or its subtable not found
  @retval EFI_ABORTED     Invalid region configuration, not supported by platform
**/
EFI_STATUS
VerifyInterleaveSetsPlatformSupport(
  IN OUT REGION_GOAL *pRegionGoal[],
  IN     UINT32 RegionGoalNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT64 Alignment = 0;
  BOOLEAN Found = FALSE;
  UINT8 iMCIntSize = 0;
  UINT8 ChannelIntSize = 0;
  UINT32 *pChannelInterleaveSize = NULL;
  UINT32 *piMCInterleaveSize = NULL;
  UINT32 *pRecommendedFormats = NULL;
  UINT32 *pChannelWays = NULL;
  UINT32 InterleaveAlignmentSize = 0;
  UINT32 InterleaveMapListLength = 0;
  UINT32 Length = 0;
  ACPI_REVISION Revision;

  NVDIMM_ENTRY();

  if (pRegionGoal == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = RetrieveSupportediMcAndChannelInterleaveSizes(&pChannelInterleaveSize, &piMCInterleaveSize,
    &pRecommendedFormats, &pChannelWays, &Length, &InterleaveAlignmentSize, &Revision);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to retrieve supported iMc & Channel Interleave sizes from PCAT");
    goto Finish;
  }

  if (IS_ACPI_REV_MAJ_1_MIN_VALID(Revision)) {
    ReturnCode = RetrieveChannelWaysFromInterleaveSetMap(&pChannelWays, &InterleaveMapListLength);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to retrieve number of channel ways supported from Interleave Map");
      goto Finish;
    }
    Length = InterleaveMapListLength;
  }

  if (pChannelWays == NULL) {
    NVDIMM_DBG("Interleave channel ways unknown!");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  for (Index = 0; Index < RegionGoalNum; Index++) {
    Alignment = Pow(2, InterleaveAlignmentSize);

    /** Interleave set has to have a proper alignment **/
    if (pRegionGoal[Index]->Size % (Alignment * pRegionGoal[Index]->DimmsNum) != 0) {
      NVDIMM_DBG("Interleave set lacking proper alignment defined by BIOS in PCAT tables.");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    if (pRegionGoal[Index]->Size == 0) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_SIZE_TOO_SMALL_FOR_INT_SET_ALIGNMENT);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    Found = FALSE;

    for (Index2 = 0; Index2 < Length; Index2++) {
      if (!IS_BIT_SET_VAR(pRegionGoal[Index]->NumOfChannelWays, (UINT16)pChannelWays[Index2])) {
        continue;
      }

      if (IS_ACPI_REV_MAJ_1_MIN_VALID(Revision)) {
        iMCIntSize = (UINT8)piMCInterleaveSize[0];
        ChannelIntSize = (UINT8)pChannelInterleaveSize[0];
      } else {
        iMCIntSize = (UINT8)piMCInterleaveSize[Index2];
        ChannelIntSize = (UINT8)pChannelInterleaveSize[Index2];
      }

      if (pRegionGoal[Index]->InterleaveSetType == DEFAULT_INTERLEAVE_SET_TYPE) {
        pRegionGoal[Index]->InterleaveSetType = INTERLEAVED;
      }

      if (pRegionGoal[Index]->ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE &&
          pRegionGoal[Index]->ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE &&
          ((IS_ACPI_REV_MAJ_0_MIN_VALID(Revision) && pRecommendedFormats != NULL)
            ? IS_BIT_SET_VAR(pRecommendedFormats[Index2], BIT0) : TRUE)) {
        pRegionGoal[Index]->ImcInterleaving = iMCIntSize;
        pRegionGoal[Index]->ChannelInterleaving = ChannelIntSize;
        Found = TRUE;
        break;
      } else if (pRegionGoal[Index]->ImcInterleaving == iMCIntSize &&
          pRegionGoal[Index]->ChannelInterleaving == ChannelIntSize) {
        Found = TRUE;
        break;
      }
    }

    if (!Found) {
      if (pRegionGoal[Index]->ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE &&
          pRegionGoal[Index]->ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_DEFAULT_INT_SIZES);
      } else {
        ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_SPECIFIED_INT_SIZES);
      }
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pChannelInterleaveSize);
  FREE_POOL_SAFE(piMCInterleaveSize);
  FREE_POOL_SAFE(pRecommendedFormats);
  FREE_POOL_SAFE(pChannelWays);
  return ReturnCode;
}

/**
  Create REGION goal

  @param[in] pRegionGoalTemplate Pointer to REGION goal template
  @param[in] pDimms Array of pointers to DIMMs
  @param[in] DimmsNum Number of pointers in pDimms
  @param[in] InterleaveSetSize Interleave set size
  @param[in] pDriverPreferences Driver preferences for interleave sets Optional
  @param[in] SequenceIndex Variable to keep an order of REGIONs on DIMMs
  @param[in, out] pInterleaveSetIndex Unique index for interleave set

  @retval REGION_GOAL success
  @retval NULL one or more parameters are NULL or memory allocation failure
**/
REGION_GOAL *
CreateRegionGoal(
  IN     REGION_GOAL_TEMPLATE *pRegionGoalTemplate,
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     UINT64 InterleaveSetSize,
  IN     DRIVER_PREFERENCES *pDriverPreferences OPTIONAL,
  IN     UINT16 SequenceIndex,
  IN OUT UINT16 *pInterleaveSetIndex
  )
{
  REGION_GOAL *pRegionGoal = NULL;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pRegionGoalTemplate == NULL ||
    pDimms == NULL ||
    pInterleaveSetIndex == NULL) {
    goto Finish;
  }

  pRegionGoal = AllocateZeroPool(sizeof(*pRegionGoal));
  if (pRegionGoal == NULL) {
    goto Finish;
  }

  pRegionGoal->SequenceIndex = SequenceIndex;
  pRegionGoal->InterleaveSetType = pRegionGoalTemplate->InterleaveSetType;

  if (pRegionGoal->InterleaveSetType == NON_INTERLEAVED || pDriverPreferences == NULL) {
    pRegionGoal->ImcInterleaving = DEFAULT_IMC_INTERLEAVE_SIZE;
    pRegionGoal->ChannelInterleaving = DEFAULT_CHANNEL_INTERLEAVE_SIZE;
  } else {
    pRegionGoal->ImcInterleaving = pDriverPreferences->ImcInterleaving;
    pRegionGoal->ChannelInterleaving = pDriverPreferences->ChannelInterleaving;
  }

  /** @todo (WW21):
    There are additional restrictions on Interleave Set configurations. We decided with Software Management team that
    we should create two Interleave Sets, one on each iMC, if it is not possible to create one Interleave Set on
    socket (2 iMCs).

    Need to implement algorithm.
  **/

  switch (DimmsNum) {
  case 1:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_1_WAY;
    break;
  case 2:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_2_WAY;
    break;
  case 3:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_3_WAY;
    break;
  case 4:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_4_WAY;
    break;
  case 6:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_6_WAY;
    break;
  case 8:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_8_WAY;
    break;
  case 12:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_12_WAY;
    break;
  case 16:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_16_WAY;
    break;
  case 24:
    pRegionGoal->NumOfChannelWays = INTERLEAVE_SET_24_WAY;
    break;
  default:
    NVDIMM_WARN("Unsupported number of DIMMs in interleave set: %d", DimmsNum);
    pRegionGoal->NumOfChannelWays = 0;
    break;
  }

  pRegionGoal->InterleaveSetIndex = *pInterleaveSetIndex;
  (*pInterleaveSetIndex)++;

  for (Index = 0; Index < DimmsNum; Index++) {
    pRegionGoal->pDimms[Index] = pDimms[Index];
  }
  pRegionGoal->DimmsNum = DimmsNum;
  pRegionGoal->Size = InterleaveSetSize;

Finish:
  NVDIMM_EXIT();
  return pRegionGoal;
}

/**
  Get minimum and maximum sizes of AppDirect Namespace that can be created on the Interleave Set

  @param[in]  pIS      Interleave Set that sizes of AppDirect Namespaces will be determined for
  @param[out] pMinSize Output parameter for minimum size
  @param[out] pMaxSize Output parameter for maximum size

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
ADNamespaceMinAndMaxAvailableSizeOnIS(
  IN     NVM_IS *pIS,
     OUT UINT64 *pMinSize,
     OUT UINT64 *pMaxSize
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  MEMMAP_RANGE AppDirectRange;
  UINT32 RegionCount = 0;

  ZeroMem(&AppDirectRange, sizeof(AppDirectRange));

  NVDIMM_ENTRY();

  if (pIS == NULL || pMinSize == NULL || pMaxSize == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pMinSize = 0;
  *pMaxSize = 0;


  ReturnCode = GetListSize(&pIS->DimmRegionList, &RegionCount);
  if (EFI_ERROR(ReturnCode) || RegionCount == 0) {
    goto Finish;
  }

  ReturnCode = FindADMemmapRangeInIS(pIS, MAX_UINT64_VALUE, &AppDirectRange);

  if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_NOT_FOUND) {
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

  *pMinSize = gNvmDimmData->Alignments.PmNamespaceMinSize * RegionCount;
  *pMaxSize = AppDirectRange.RangeLength * RegionCount;

  if (*pMinSize > *pMaxSize) {
    *pMinSize = 0;
    *pMaxSize = 0;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Verify that all DIMMs with goal config are specified on a given socket at once to keep supported region configs.

  @param[in] pDimms List of DIMMs to configure
  @param[in] DimmsNum Number of DIMMs to configure
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one or more parameters are NULL
  @retval EFI_UNSUPPORTED A given config is unsupported
**/
EFI_STATUS
VerifyDeletingSupportedRegionConfigs(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 DimmsWithGoalConfigNum = 0;
  UINT32 SpecifiedDimmsWithGoalConfigNum = 0;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 Socket = 0;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pCommandStatus == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Socket = 0; Socket < MAX_SOCKETS; Socket++) {
    DimmsWithGoalConfigNum = 0;

    /** Get a number of all Dimms with goal config on a given socket **/
    LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);

      if (!IsDimmManageable(pDimm)){
        continue;
      }

      if (Socket == pDimm->SocketId) {
        if (pDimm->RegionsGoalConfig) {
          DimmsWithGoalConfigNum++;
        }
      }
    }

    SpecifiedDimmsWithGoalConfigNum = 0;

    /** Get a number of specified DIMMs with goal config on a given socket **/
    for (Index = 0; Index < DimmsNum; Index++) {
      if (Socket == pDimms[Index]->SocketId) {
        if (pDimms[Index]->RegionsGoalConfig) {
          SpecifiedDimmsWithGoalConfigNum++;
        }
      }
    }

    /**
      If any DIMM is specified for a given socket then all DIMMs with goal config have to be specified
    **/
    if (!(SpecifiedDimmsWithGoalConfigNum == 0 || SpecifiedDimmsWithGoalConfigNum == DimmsWithGoalConfigNum)) {
      Rc = EFI_UNSUPPORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_CONF_UNSUPPORTED_CONFIG);
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Find Region Goal Dimm in an array

  @param[in] pDimms Array that will be searched
  @param[in] DimmsNum Number of items in pDimms
  @param[in] pDimmToFind Dimm to find

  @retval Region Goal Dimm pointer - if found
  @retval NULL - if not found
**/
REGION_GOAL_DIMM *
FindRegionGoalDimm(
  IN     REGION_GOAL_DIMM *pDimms,
  IN     UINT32 DimmsNum,
  IN     DIMM *pDimmToFind
  )
{
  REGION_GOAL_DIMM *pRegionGoalDimm = NULL;
  UINT32 Index = 0;

  if (pDimms == NULL || pDimmToFind == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (pDimms[Index].pDimm == pDimmToFind) {
      pRegionGoalDimm = &pDimms[Index];
      break;
    }
  }

Finish:
  return pRegionGoalDimm;
}


/**
  Clear all internal goal configurations structures

  @param[in, out] pDimmList Head of the list of all NVM DIMMs in the system

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
ClearInternalGoalConfigsInfo(
  IN OUT LIST_ENTRY *pDimmList
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pDimmNode = NULL;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  REGION_GOAL *pRegionsGoal[MAX_IS_CONFIGS];
  UINT32 RegionsGoalNum = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;

  NVDIMM_ENTRY();

  SetMem(pDimms, sizeof(pDimms), 0x0);
  SetMem(pRegionsGoal, sizeof(pRegionsGoal), 0x0);

  if (pDimmList == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Copy list to array **/
  DimmsNum = 0;
  LIST_FOR_EACH(pDimmNode, pDimmList) {
    ASSERT(DimmsNum < MAX_DIMMS);
    pDimms[DimmsNum] = DIMM_FROM_NODE(pDimmNode);
    DimmsNum++;
  }

  ReturnCode = FindUniqueRegionsGoal(pDimms, DimmsNum, pRegionsGoal, &RegionsGoalNum);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Error in FindUniqueRegionsGoal");
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    pDimms[Index]->RegionsGoalConfig = FALSE;
    for (Index2 = 0; Index2 < MAX_IS_PER_DIMM; Index2++) {
      pDimms[Index]->pRegionsGoal[Index2] = NULL;
    }
    pDimms[Index]->RegionsGoalNum = 0;
    pDimms[Index]->VolatileSizeGoal = 0;
    pDimms[Index]->GoalConfigStatus = GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS;
    pDimms[Index]->PcdSynced = FALSE;
  }

  for (Index = 0; Index < RegionsGoalNum; Index++) {
    FREE_POOL_SAFE(pRegionsGoal[Index]);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Calculate actual volatile size

  @param[in] RawDimmCapacity Raw capacity to calculate actual volatile size for
  @param[in] VolatileSizeRounded
  @param[out] pVolatileSizeActual Actual Volatile region size - actual volatile size with metadata

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
CalculateActualVolatileSize(
  IN     UINT64 RawDimmCapacity,
  IN     UINT64 VolatileSizeRequested,
     OUT UINT64 *pVolatileSizeActual
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 TempPersistentSize = 0;
  UINT64 UnalignedPersistentSize = 0;

  NVDIMM_ENTRY();

  if (pVolatileSizeActual == NULL) {
    goto Finish;
  }

  // If user requests more than 0 but less than the minimum amount, round up to the minimum for them
  if (VolatileSizeRequested != 0 && VolatileSizeRequested < gNvmDimmData->Alignments.RegionVolatileAlignment) {
    VolatileSizeRequested = ROUNDUP(VolatileSizeRequested, gNvmDimmData->Alignments.RegionVolatileAlignment);
  }

  if (RawDimmCapacity <= VolatileSizeRequested) {
    *pVolatileSizeActual = RawDimmCapacity;
  } else if (VolatileSizeRequested == 0) {
    *pVolatileSizeActual = 0;
  } else {
    TempPersistentSize = RawDimmCapacity - VolatileSizeRequested;
    // alignment calculation here is done at socket level
    UnalignedPersistentSize = TempPersistentSize % gNvmDimmData->Alignments.RegionPartitionAlignment;
    if (UnalignedPersistentSize > (gNvmDimmData->Alignments.RegionPartitionAlignment / 2)) {
      TempPersistentSize = ROUNDUP(TempPersistentSize, gNvmDimmData->Alignments.RegionPartitionAlignment);
    } else {
      TempPersistentSize = ROUNDDOWN(TempPersistentSize, gNvmDimmData->Alignments.RegionPartitionAlignment);
      // If we round down to 0 the user would receive 100% volatile and they had requested at least a little persistent
      if (TempPersistentSize == 0) {
        TempPersistentSize = gNvmDimmData->Alignments.RegionPartitionAlignment;
      }
    }

    // Always give the user some amount of volatile if they have requested it
    if (TempPersistentSize >= RawDimmCapacity && VolatileSizeRequested != 0) {
      *pVolatileSizeActual = RawDimmCapacity  - (TempPersistentSize - gNvmDimmData->Alignments.RegionPartitionAlignment);
    } else if (TempPersistentSize >= RawDimmCapacity) {
      *pVolatileSizeActual = 0;
    } else {
      *pVolatileSizeActual = RawDimmCapacity - TempPersistentSize;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check for new goal configs for the DIMM

  @param[in] pDIMM The current DIMM
  @param[out] pHasNewGoal TRUE if any of the dimms have a new goal, else FALSE

  @retval EFI_SUCCESS
  #retval EFI_INVALID_PARAMETER If IS is null
**/
EFI_STATUS
FindIfNewGoalOnDimm(
  IN     DIMM *pDimm,
     OUT BOOLEAN *pHasNewGoal
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if ((pDimm == NULL) || (pHasNewGoal == NULL)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pHasNewGoal = FALSE;

  if (pDimm->GoalConfigStatus == GOAL_CONFIG_STATUS_NEW) {
     *pHasNewGoal = TRUE;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check if security state of specified DIMM is locked

  @param[in] pDimm The current DIMM
  @param[out] pIsLocked TRUE if security state of specified dimm is locked

  @retval EFI_SUCCESS
  #retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Allocation failed
**/
EFI_STATUS
IsDimmLocked(
  IN     DIMM *pDimm,
     OUT BOOLEAN *pIsLocked
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PT_GET_SECURITY_PAYLOAD *pSecurityPayload = NULL;
  UINT8 SecurityState = SECURITY_UNKNOWN;

  NVDIMM_ENTRY();

  if ((pDimm == NULL) || (pIsLocked == NULL)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pIsLocked = FALSE;

  pSecurityPayload = AllocateZeroPool(sizeof(*pSecurityPayload));
  if (pSecurityPayload == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = FwCmdGetSecurityInfo(pDimm, pSecurityPayload);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("FW CMD Error: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }
  ConvertSecurityBitmask(pSecurityPayload->SecurityStatus.AsUint32, &SecurityState);
  /** If any of the DIMM on the IS is locked, then break **/
  if (SecurityState == SECURITY_LOCKED) {
    *pIsLocked = TRUE;
  }

Finish:
  FREE_POOL_SAFE(pSecurityPayload);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set Interleave Set state taking states' priority into account

  @param[in] CurrentState Current IS state
  @param[in] NewState IS state to be set

  @retval UINT8 New IS state
**/
UINT8
SetISStateWithPriority(
  IN    UINT8 CurrentState,
  IN    UINT8 NewState
)
{
  return CurrentState > NewState ? CurrentState : NewState;
}

/**
  Check for existing goal configs on a socket for which a new goal config has been requested

  @param[in] pDimms Array of pointers to DIMMs based on the goal config requested
  @param[in] pDimmsNum Number of pointers in pDimms

  @retval EFI_ABORTED one or more DIMMs on a socket already have goal configs
  @retval EFI_INVALID_PARAMETER pDimms or pDimmsNum is NULL
**/
EFI_STATUS
CheckForExistingGoalConfigPerSocket(
  IN    DIMM *pDimms[MAX_DIMMS],
  IN    UINT32 *pDimmsNum
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pDimmNode = NULL;
  DIMM *pDimm = NULL;
  UINT32 Socket = MAX_UINT32_VALUE;
  UINT32 PrevSocket = MAX_UINT32_VALUE;
  UINT32 Index = 0;

  if (pDimms == NULL || pDimmsNum == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for (Index = 0; Index < *pDimmsNum; Index++) {
    Socket = pDimms[Index]->SocketId;
    if (PrevSocket == Socket) {
      continue;
    }
    LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);
      if (!IsDimmManageable(pDimm) || pDimm->NonFunctional || (Socket != pDimm->SocketId)) {
        continue;
      }

      if (pDimm->GoalConfigStatus != GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS) {
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }
    PrevSocket = Socket;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Examines the system topology for the system DDR capacity and compares
  it to the 2LM capacity to check for ratio violations

  @param[in] SocketId Socket Id, value 0xFFFF indicates include all socket values
  @param[in] pDimmsSym Array of Dimms for symmetrical region config
  @param[in] DimmsSymNum Number of items in DimmsSym
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER input parameter null
**/
EFI_STATUS
CheckNmFmLimits(
  IN     UINT16 SocketId,
  IN     REGION_GOAL_DIMM *pDimmsSym,
  IN     UINT32  DimmsSymNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 TwoLM_FmMinRecommended = 0;
  UINT64 TwoLM_FmMaxRecommended = 0;
  UINT64 TwoLM_NMTotal = 0;
  UINT64 TwoLM_FMTotal = 0;

  NVDIMM_ENTRY();

  if (pDimmsSym == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Get total PMem module volatile capacity (Far Memory)
  ReturnCode = CalculateFarMemorySizeForNewGoalConfigs(SocketId, pDimmsSym, DimmsSymNum,
    &TwoLM_FMTotal, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not calculate far memory capacity.");
    goto Finish;
  }

  if (TwoLM_FMTotal == 0) {
    //no limit check necessary - no 2LM goal in play
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  // Get total DDR capacity (Near Memory)
  ReturnCode = GetDDRCapacities(SocketId, &TwoLM_NMTotal, NULL, NULL, NULL);
  if (EFI_ERROR(ReturnCode)) {
    // If not Purley, this is an error. On Purley we expect the PMTT table
    // to be missing on some platforms, workarounds are further below
    if (!IS_ACPI_HEADER_REV_MAJ_0_MIN_VALID(gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr)) {
      // Preserve return code and error out
      NVDIMM_DBG("Could not retrieve usable DDR cache size");
      goto Finish;
    }
    // We can't determine the cache size (and it's non-zero), likely because the PMTT
    // table is missing. Since this is an allowed condition at this point, show
    // a warning to the user and return success.
    ReturnCode = EFI_SUCCESS;
    SetCmdStatus(pCommandStatus, NVM_WARN_PMTT_TABLE_NOT_FOUND);
    goto Finish;
  }

  if (TwoLM_NMTotal > TwoLM_FMTotal) {
    NVDIMM_ERR("NM:FM ratio violated and is greater than 1.");
    ResetCmdStatus(pCommandStatus, NVM_ERR_NMFM_RATIO_GREATER_THAN_ONE);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  TwoLM_FmMinRecommended = (UINT64)(TwoLM_NMTotal * TWOLM_NMFM_RATIO_LOWER);
  TwoLM_FmMaxRecommended = TwoLM_NMTotal * TWOLM_NMFM_RATIO_UPPER;

  if (TwoLM_FMTotal > TwoLM_FmMaxRecommended) {
    SetCmdStatus(pCommandStatus, NVM_WARN_NMFM_RATIO_UPPER_VIOLATION);
  }
  else if (TwoLM_FMTotal < TwoLM_FmMinRecommended) {
    SetCmdStatus(pCommandStatus, NVM_WARN_NMFM_RATIO_LOWER_VIOLATION);
  }

Finish:
  if ((pCommandStatus != NULL) && EFI_ERROR(ReturnCode) &&
    (pCommandStatus->GeneralStatus == NVM_ERR_OPERATION_NOT_STARTED)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if all DIMMs in the list are in configured state

  @param[IN] pDimmList Head of the Dimm list
  @param[IN] pDimmsUnConfigured Boolean flag to indicate if any PMem module is unconfigured
  @param[OUT] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if input parameter null
**/
EFI_STATUS
CheckIfAllDimmsConfigured(
  IN     LIST_ENTRY *pDimmList,
     OUT BOOLEAN *pDimmsUnConfigured,
     OUT COMMAND_STATUS *pCommandStatus OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;

  NVDIMM_ENTRY();

  if (pDimmList == NULL || pDimmsUnConfigured == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  LIST_FOR_EACH(pDimmNode, pDimmList) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (!IsDimmManageable(pDimm) || DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask)) {
      continue;
    }

    if (pDimm->ConfigStatus == DIMM_CONFIG_UNDEFINED) {
      if (pCommandStatus != NULL) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_PCD_CURR_CONF_MISSING);
      }
      *pDimmsUnConfigured = TRUE;
      goto Finish;
    }
  }

  *pDimmsUnConfigured = FALSE;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate total far memory on PMem modules for existing goal configs

  @param[in] SocketId Socket Id, value 0xFFFF indicates include all socket values
  @param[in] pDimmsSym Array of Dimms for symmetrical region config
  @param[in] DimmsSymNum Number of items in DimmsSym
  @param[out] pTotalFarMemorySize Pointer to total far memory capacity
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER if input parameter null
**/
EFI_STATUS
CalculateFarMemorySizeForNewGoalConfigs(
  IN     UINT16 SocketId,
  IN     REGION_GOAL_DIMM *pDimmsSym,
  IN     UINT32  DimmsSymNum,
     OUT UINT64 *pTotalFarMemorySize,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  UINT64 VolatileCapacityUnspecifiedDimms = 0;
  UINT32 Index1 = 0;
  UINT32 Index2 = 0;
  UINT32 NumOfUnspecifiedDimms = 0;
  UINT16 *pSocketIds = NULL;
  UINT32 SocketsNum = 0;
  BOOLEAN UnSpecifiedDimm = TRUE;
  REQUIRE_DCPMMS RequireDcpmmsBitfield = REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL |
                                         REQUIRE_DCPMMS_NO_POPULATION_VIOLATION;

  NVDIMM_ENTRY();

  if (pDimmsSym == NULL || pTotalFarMemorySize == NULL ||
    pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Set the default value to 0
  *pTotalFarMemorySize = 0;

  for (Index1 = 0; Index1 < DimmsSymNum; Index1++)
  {
    if (SocketId == SOCKET_ID_ALL || pDimmsSym[Index1].pDimm->SocketId == SocketId) {
      *pTotalFarMemorySize += ROUNDDOWN(pDimmsSym[Index1].VolatileSize,
        gNvmDimmData->Alignments.RegionVolatileAlignment);
    }
  }

  if (*pTotalFarMemorySize == 0) {
    // No 2LM goal in play
    goto Finish;
  }

  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * MAX_DIMMS);
  if (ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  /**
    Initialize socket ID array and array length input parameters for
    VerifyTargetDimms function when a single socket ID is specified.
  **/
  if (SocketId != SOCKET_ID_ALL) {
    pSocketIds = &SocketId;
    SocketsNum = 1;
  }

  ReturnCode = VerifyTargetDimms(NULL, 0, pSocketIds, SocketsNum, RequireDcpmmsBitfield,
    ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // If all usable PMem modules targeted, then skip further calculations
  if (DimmsSymNum == DimmsNum) {
    goto Finish;
  }

  /**
    Targeting a subset of PMem modules for 100% MemoryMode is allowed if
    all unspecified PMem modules are configured for 100% MemoryMode only.
    Account for volatile partitions on unspecified PMem modules in this case.
  **/
  for (Index1 = 0; Index1 < DimmsNum; Index1++) {
    for (Index2 = 0; Index2 < DimmsSymNum; Index2++) {
      if ((SocketId == SOCKET_ID_ALL || pDimmsSym[Index2].pDimm->SocketId == SocketId) &&
          (pDimmsSym[Index2].pDimm->DeviceHandle.AsUint32 == ppDimms[Index1]->DeviceHandle.AsUint32)) {
        UnSpecifiedDimm = FALSE;
        break;
      }
    }

    if (UnSpecifiedDimm && ppDimms[Index1]->VolatileCapacity > 0 && ppDimms[Index1]->ISsNfitNum == 0) {
      NumOfUnspecifiedDimms++;
      VolatileCapacityUnspecifiedDimms += ROUNDDOWN(ppDimms[Index1]->VolatileCapacity,
        gNvmDimmData->Alignments.RegionVolatileAlignment);
    }

    UnSpecifiedDimm = TRUE;
  }

  if ((DimmsSymNum + NumOfUnspecifiedDimms) == DimmsNum) {
    *pTotalFarMemorySize += VolatileCapacityUnspecifiedDimms;
  }

Finish:
  FREE_POOL_SAFE(ppDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
