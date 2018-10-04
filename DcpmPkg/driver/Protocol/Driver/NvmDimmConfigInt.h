/*
 * Copyright (c) 2015-2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /**
  * @file NvmDimmConfigInt.h
  * @brief Internal header file for the NvmDimmConfig
  */

#ifndef _NVMDIMM_CONFIG_INT_H_
#define _NVMDIMM_CONFIG_INT_H_

/**
  Retrieve the User Driver Preferences from RunTime Services.

  @param[out] pDriverPreferences pointer to the current driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
ReadRunTimeDriverPreferences(
  OUT DRIVER_PREFERENCES *pDriverPreferences
);

/**
  Sorts the region list by Id

  @param[in out] pRegion1 A pointer to the Regions.
  @param[in out] pRegion2 A pointer to the copy of Regions.

  @retval int retruns 0,-1, 0
**/
INT32 SortRegionInfoById(VOID *pRegion1, VOID *pRegion2);

/**
  Sorts the DimmIds list by Id

  @param[in out] pDimmId1 A pointer to the pDimmId list.
  @param[in out] pDimmId2 A pointer to the copy of pDimmId list.

  @retval int retruns 0,-1, 0
**/
INT32 SortRegionDimmId(VOID *pDimmId1, VOID *pDimmId2);

/**
  Validate firmware Image version

  @param[in] pImage the FW Image header
  @param[in] Force is a BOOL which indicates whether to skip prompts
  @param[in] pDimm Pointer to the Dimm whose FW is validated

  @param[out] pNvmStatus NVM status code

  @retval EFI_ABORTED One of the checks failed
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_SUCCESS Update has completed successfully
**/
EFI_STATUS
ValidateImageVersion(
  IN       FW_IMAGE_HEADER *pImage,
  IN       BOOLEAN Force,
  IN       DIMM *pDimm,
  OUT  NVM_STATUS *pNvmStatus
);

/**
Parse EFI_ACPI_DESCRIPTION_HEADER (DSDT) and fetch NFIT & PCAT pointers to table
Also, parse PMTT table to check if MM can be configured
@param[in] pDsdt a pointer to EFI_ACPI_DESCRIPTION_HEADER instance for each of NFIT, PMTT and PCAT
@param[out] ppFitHead pointer to pointer to store NFIT table
@param[out] ppPcatHead pointer to pointer to store PCAT table
@param[out] pIsMemoryModeAllowed pointer to check if MM can be configured

@retval EFI_INVALID_PARAMETER passed NULL argument
@retval EFI_DEVICE_ERROR could not parse at least one of the tables
@retval EFI_SUCCESS Success
**/
EFI_STATUS
ParseAcpiTables(
  IN     CONST EFI_ACPI_DESCRIPTION_HEADER *pNfit,
  IN     CONST EFI_ACPI_DESCRIPTION_HEADER *pPcat,
  IN     CONST EFI_ACPI_DESCRIPTION_HEADER *pPMTT,
  OUT ParsedFitHeader **ppFitHead,
  OUT ParsedPcatHeader **ppPcatHead,
  OUT BOOLEAN *pIsMemoryModeAllowed
);

/**
  Fetch the NFIT and PCAT tables from EFI_SYSTEM_TABLE
  @param[in] pSystemTable is a pointer to the EFI_SYSTEM_TABLE instance
  @param[out] ppDsdt is a pointer to EFI_ACPI_DESCRIPTION_HEADER (NFIT)
  @param[out] ppPcat is a pointer to EFI_ACPI_DESCRIPTION_HEADER (PCAT)

  @retval EFI_SUCCESS   on success
  @retval EFI_INVALID_PARAMETER NULL argument
  @retval EFI_LOAD_ERROR if one or more of the tables could not be found
**/
EFI_STATUS
GetAcpiTables(
  IN     CONST EFI_SYSTEM_TABLE *pSystemTable,
  OUT EFI_ACPI_DESCRIPTION_HEADER **ppNfit,
  OUT EFI_ACPI_DESCRIPTION_HEADER **ppPcat,
  OUT EFI_ACPI_DESCRIPTION_HEADER **ppPMTT
);

/**
  Get the PCI base address from MCFG table from EFI_SYSTEM_TABLE

  @param[in] pSystemTable is a pointer to the EFI_SYSTEM_TABLE instance
  @param[out] pPciBaseAddress is a pointer to the Base Address

  @retval EFI_SUCCESS   on success
  @retval EFI_INVALID_PARAMETER NULL argument
  @retval EFI_LOAD_ERROR if one or more of the tables could not be found
**/
EFI_STATUS
GetPciBaseAddress(
  IN     CONST EFI_SYSTEM_TABLE *pSystemTable,
  OUT UINT64 *pPciBaseAddress
);

/**
  Check the memory map against the NFIT SPA memory for consistency

  @retval EFI_SUCCESS   on success
  @retval EFI_OUT_OF_RESOURCES for a failed allocation
  @retval EFI_BAD_BUFFER_SIZE if the nfit spa memory is more than the one in memmmap
**/
EFI_STATUS
CheckMemoryMap(
);

/**
  Initialize ACPI tables (NFit and PCAT)

  @retval EFI_SUCCESS   on success
  @retval EFI_NOT_FOUND Nfit table not found
**/
EFI_STATUS
initAcpiTables(
);

#ifdef OS_BUILD
/**
  Uninitialize ACPI tables (NFit and PCAT)

  @retval EFI_SUCCESS   on success
  @retval EFI_NOT_FOUND Nfit table not found
**/
EFI_STATUS
uninitAcpiTables(
);
#endif // OS_BUILD

/**
  Initialize a simulated NFit table.

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES cannot allocate memory for NFit table
  @retval EFI_LOAD_ERROR problem appears when loading data to Nfit table
**/
EFI_STATUS
initSimulatedNFit(
);

/**
  Write simulated data to PCD tables on NVDIMMs

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES cannot allocate memory for NFit table
  @retval EFI_LOAD_ERROR problem appears when loading data to Nfit table
**/
EFI_STATUS
InitSimulatedPcd(
);

/**
  Parse ACPI tables and create DIMM list

  @retval EFI_SUCCESS  Success
  @retval EFI_...      Other errors from subroutines
**/
EFI_STATUS
FillDimmList(
);

/**
  Clean up the in memory DIMM inventory

  @retval EFI_SUCCESS  Success
  @retval EFI_...      Other errors from subroutines
**/
EFI_STATUS
FreeDimmList(
);

/**
  Retrieve Smbios tables dynamically, and populate Smbios table structures
  of type 17/20 for the specified Dimm Pid

  @param[in]  DimmPid The ID of the DIMM
  @param[out] pDmiPhysicalDev Pointer to smbios table strcture of type 17
  @param[out] pDmiDeviceMappedAddr Pointer to smbios table strcture of type 20

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_DEVICE_ERROR Failure to retrieve SMBIOS tables from gST
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetDmiMemdevInfo(
  IN     UINT16 DimmPid,
  OUT SMBIOS_STRUCTURE_POINTER *pDmiPhysicalDev,
  OUT SMBIOS_STRUCTURE_POINTER *pDmiDeviceMappedAddr,
  OUT SMBIOS_VERSION *pSmbiosVersion
);

/**
  Automatically provision capacity
  Decision logic for when to automatically provision capacity based on
  ProvisionCapacityStatus, PCD data and topology change.

  If automatic provisioning is triggered and succeeds, this function
  will reboot and never return.

  @param[in] pIntelDIMMConfig Pointer to struct containing EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
AutomaticProvisionCapacity(
  IN     INTEL_DIMM_CONFIG *pIntelDIMMConfig
);

/**
  Automatically provision namespaces
  Decision logic for when to automatically provision namespaces based on
  ProvisionNamespaceStatus.

  @param[in] pIntelDIMMConfig Pointer to struct containing EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
AutomaticProvisionNamespace(
  IN     INTEL_DIMM_CONFIG *pIntelDIMMConfig
);

/**
  Checks inputs and executes create goal
  Will remove all namespaces.

  @param[in] pIntelDIMMConfig Pointer to struct containing EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
AutomaticCreateGoal(
  IN     INTEL_DIMM_CONFIG *pIntelDIMMConfig
);

/**
  Checks inputs and executes create namespace on empty ISets

  @param[in] pIntelDIMMConfig Pointer to struct containing EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
AutomaticCreateNamespace(
  IN     INTEL_DIMM_CONFIG *pIntelDIMMConfig
);

/**
  Retrives Intel Dimm Config EFI vars

  User is responsible for freeing ppIntelDIMMConfig

  @param[out] pIntelDIMMConfig Pointer to struct to fill with EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
RetrieveIntelDIMMConfig(
  OUT INTEL_DIMM_CONFIG **ppIntelDIMMConfig
);

/**
  Updates IntelDIMMConfig EFI Vars

  @param[in] pIntelDIMMConfig pointer to new config to write
**/
VOID
UpdateIntelDIMMConfig(
  IN    INTEL_DIMM_CONFIG *pIntelDIMMConfig
);

/**
  Checks if PCD copy of vars matches EFI on all DIMMs
  If a DIMM is missing PCD data then it is considered not matching

  @param[in] pIntelDIMMConfig Pointer to struct with EFI vars
  @param[out] pVarsMatch True if PCD data on all DIMMs match

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
CheckPCDAutoConfVars(
  IN     INTEL_DIMM_CONFIG *pIntelDIMMConfigEfiVar,
  OUT BOOLEAN *pVarsMatch
);

/**
  Checks if the topology has changed based on CCUR config status

  @param[out] pTopologyChanged True if ConfigStatus indicates topology change

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
CheckTopologyChange(
  OUT BOOLEAN *pTopologyChanged
);

/**
  Checks if the previous goal was applied successfully

  @param[out] pGoalSuccess True if goal was applied successfully

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
CheckGoalStatus(
  OUT BOOLEAN *pGoalSuccess
);

#endif // _NVMDIMM_CONFIG_INT_H_