/*
 * Copyright (c) 2015-2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * @file NvmDimmConfig.h
 * @brief Implementation of the EFI_NVMDIMMS_CONFIG_PROTOCOL, a custom protocol
 * to configure and manage DCPMEM modules
 */

#ifndef _NVMDIMM_CONFIG_H_
#define _NVMDIMM_CONFIG_H_

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <IndustryStandard/SmBios.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <Debug.h>
#include <Core/Dimm.h>
#include <Utility.h>
#include <FwUtility.h>
#include <NvmDimmDriverData.h>
#include <NvmStatus.h>
#include <NvmInterface.h>
#include <NvmTables.h>
#include <PcdCommon.h>
#include <SmbiosUtility.h>
#include <Types.h>

extern EFI_GUID gNvmDimmConfigProtocolGuid;

#define DEVICE_LOCATOR_LEN 128 //!< DIMM Device Locator buffer length

#define FEATURE_NOT_SUPPORTED 0
#define FEATURE_SUPPORTED     1

#define EFI_PERSISTENT_MEMORY_REGION 14
#define MSR_RAPL_POWER_UNIT 0x618

#define UPDATE_FIRMWARE_DATA_PACKET_SIZE  64
#define FW_UPDATE_SP_INIT_TRANSFER        0x0
#define FW_UPDATE_SP_CONTINUE_TRANSFER    0x1
#define FW_UPDATE_SP_END_TRANSFER         0x2

#define FW_UPDATE_SP_SELECTOR             0x1

#define NFIT_PLATFORM_CAPABILITIES_BIT0     0x1
#define NFIT_MEMORY_CONTROLLER_FLUSH_BIT1   (NFIT_PLATFORM_CAPABILITIES_BIT0 << 0x1)

/**
  The update goes in 3 steps: initialization, data, end, where the data step can be done many times.
  Each of those steps must be done at least one, so the minimum number of packets will be 3.
**/
#define FW_UPDATE_SP_MINIMUM_PACKETS      3
/**
  The FW file size is around 192KB, this divided by the packet size will be 3072 packets of size 64 byte.
  Rounded up to 4k.
**/
#define FW_UPDATE_SP_MAXIMUM_PACKETS      4096

#pragma pack(push)
#pragma pack(1)
typedef struct {
    UINT16 TransactionType : 2;
    UINT16 PacketNumber : 14;
    UINT8 PayloadTypeSelector;
    UINT8 Reserved;
    UINT8 Data[UPDATE_FIRMWARE_DATA_PACKET_SIZE];
    UINT8 Reserved1[60];
} FW_SP_UPDATE_PACKET;
#pragma pack(pop)

/**
  SKU types & capabilities
**/
typedef enum {
  /** SKU Capabilities **/
  SkuMemoryModeOnly,
  SkuAppDirectModeOnly,
  SkuAppDirectStorageMode,
  SkuTriMode,
  SkuPackageSparingCapable,

  /** SKU Types **/
  SkuSoftProgrammableSku,
  SkuStandardSecuritySku,
  SkuControlledCountrySku
} DimmSkuType;

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
  Retrieve the number of DCPMEM modules in the system found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] pDimmCount The number of DCPMEM modules found in NFIT.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
EFIAPI
GetDimmCount(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
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
  Retrieve the number of uninitialized DCPMEM modules in the system found thru SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] pDimmCount The number of DCPMEM modules found thru SMBUS.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
EFIAPI
GetUninitializedDimmCount(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
  );

/**
  Retrieve the list of DCPMEM modules found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[out] pDimms The dimm list found in NFIT.

  @retval EFI_SUCCESS  The dimm list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL.
  @retval EFI_NOT_FOUND Dimm not found
**/
EFI_STATUS
EFIAPI
GetDimms(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO *pDimms
  );

/**
  Retrieve the list of uninitialized DCPMEM modules found thru SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[out] pDimms The dimm list found thru SMBUS.

  @retval EFI_SUCCESS  The dimm list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameter are NULL.
  @retval EFI_NOT_FOUND Dimm not found
**/
EFI_STATUS
EFIAPI
GetUninitializedDimms(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
     OUT DIMM_INFO *pDimms
  );

/**
  Retrieve the details about the DIMM specified with pid found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] dimmInfoCategories DIMM_INFO_CATEGORIES specifies which (if any)
  additional FW api calls is desired. If DIMM_INFO_CATEGORY_NONE, then only
  the properties from the pDimm struct will be populated.
  @param[out] pDimmInfo A pointer to the dimm found in NFIT

  @retval EFI_SUCCESS  The dimm information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the dimm with the pid provided does not exist.
**/
EFI_STATUS
EFIAPI
GetDimm(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO *pDimmInfo
  );

#ifdef OS_BUILD
/**
  Retrieve the PMON register values from the dimm

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] SmartDataMask This will specify whether or not to return the extra smart data along with the PMON
  Counter data
  @param[out] pPayloadPMONRegisters A pointer to the output payload PMON registers

  @retval EFI_SUCCESS  The dimm information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the dimm with the pid provided does not exist.
**/
EFI_STATUS
EFIAPI
GetPMONRegisters(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 SmartDataMask,
     OUT PT_PMON_REGISTERS *pPayloadPMONRegisters
  );

/**
  Set the PMON register values from the dimm

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] PMONGroupEnable Specifies which PMON Group to enable
  @param[out] pPayloadPMONRegisters A pointer to the output payload PMON registers

  @retval EFI_SUCCESS  The dimm information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the dimm with the pid provided does not exist.
**/
EFI_STATUS
EFIAPI
SetPMONRegisters(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 PMONGroupEnable
  );
#endif

/**
  Retrieve the list of sockets (physical processors) in the host server

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] pSocketCount The size of the list of sockets.
  @param[out] ppSockets Pointer to the list of sockets.

  @retval EFI_SUCCESS  The socket list was returned properly or,
                       the platform does not support socket sku limits
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL.
  @retval EFI_NOT_FOUND PCAT tables could not be retrieved successfully
  @retval EFI_DEVICE_ERROR Internal function error
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
EFIAPI
GetSockets(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT UINT32 *pSocketCount,
     OUT SOCKET_INFO **ppSockets
  );

/**
  Retrieve an SMBIOS table type 17 or type 20 for a specific DIMM

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] Type The Type of SMBIOS table to retrieve
  @param[out] pTable A pointer to the SMBIOS table

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER pTable is NULL
  @retval EFI_INVALID_PARAMETER DIMM pid is not valid, or Type is not valid
  @retval EFI_DEVICE_ERROR Failure to retrieve SMBIOS tables from gST
  @retval EFI_NOT_FOUND Smbios table of requested type was not found for specified device
**/
EFI_STATUS
EFIAPI
GetDimmSmbiosTable(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 Type,
     OUT SMBIOS_STRUCTURE_POINTER *pTable
  );

/**
  Check NVM device security state

  Function checks security state of a set of DIMMs. It sets security state
  to mixed when not all DIMMs have the same state.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] pSecurityState security state of a DIMM or all DIMMs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER when pSecurityState is NULL
  @retval EFI_NOT_FOUND it was not possible to get state of a DIMM
  @retval EFI_SUCCESS state correctly detected and stored in pSecurityState
**/
EFI_STATUS
EFIAPI
GetSecurityState(
    IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
    IN     UINT16 *pDimmIds,
    IN     UINT32 DimmIdsCount,
       OUT UINT8 *pSecurityState,
       OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set NVM device security state.

  Function sets security state on a set of DIMMs. If there is a failure on
  one of DIMMs function continues with setting state on following DIMMs
  but exits with error.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] SecurityOperation Security Operation code
  @param[in] pPassphrase a pointer to string with current passphrase
  @param[in] pNewPassphrase a pointer to string with new passphrase
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER when pLockState is NULL
  @retval EFI_OUT_OF_RESOURCES couldn't allocate memory for a structure
  @retval EFI_UNSUPPORTED LockState to be set is not recognized, or mixed sku of DCPMEM modules detected
  @retval EFI_DEVICE_ERROR setting state for a DIMM failed
  @retval EFI_NOT_FOUND a DIMM was not found
  @retval EFI_SUCCESS security state correctly set
**/
EFI_STATUS
EFIAPI
SetSecurityState(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 SecurityOperation,
  IN     CHAR16 *pPassphrase,
  IN     CHAR16 *pNewPassphrase,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Retrieve the NFIT ACPI table

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] ppNFit A pointer to the output NFIT table

  @retval EFI_SUCCESS Ok
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
  @retval EFI_NOT_FOUND PCAT tables not found
  @retval EFI_INVALID_PARAMETER pNFit is NULL
**/
EFI_STATUS
EFIAPI
GetAcpiNFit (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT ParsedFitHeader **ppNFit
  );

/**
  Retrieve the PCAT ACPI table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] ppPcat output buffer with PCAT tables

  @retval EFI_SUCCESS Ok
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
  @retval EFI_NOT_FOUND PCAT tables not found
**/
EFI_STATUS
EFIAPI
GetAcpiPcat (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT ParsedPcatHeader **ppPcat
  );

/**
Retrieve the PMTT ACPI table

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[out] ppPMTTtbl output buffer with PMTT tables

@retval EFI_SUCCESS Ok
@retval EFI_OUT_OF_RESOURCES Problem with allocating memory
@retval EFI_NOT_FOUND PCAT tables not found
**/
EFI_STATUS
EFIAPI
GetAcpiPMTT(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  OUT PMTT_TABLE **ppPMTTtbl
);

/**
  Get Platform Config Data

  The caller is responsible for freeing ppDimmPcdInfo by using FreeDimmPcdInfoArray.

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param{in] PcdTarget Taget PCD partition: ALL=0, CONFIG=1, NAMESPACES=2
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] ppDimmPcdInfo Pointer to output array of PCDs
  @param[out] pDimmPcdInfoCount Number of items in Dimm PCD Info
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  **/
EFI_STATUS
EFIAPI
GetPcd(
	IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
	IN     UINT8 PcdTarget,
	IN     UINT16 *pDimmIds OPTIONAL,
	IN     UINT32 DimmIdsCount,
	OUT DIMM_PCD_INFO **ppDimmPcdInfo,
	OUT UINT32 *pDimmPcdInfoCount,
	OUT COMMAND_STATUS *pCommandStatus
);

/**
  Clear LSA Namespace partition

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
EFIAPI
DeletePcd(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Update firmware or training data of a specified NVDIMM

  @param[in] DimmPid Dimm ID of a NVDIMM on which update is to be performed
  @param[in] pImageBuffer is a pointer to FW image
  @param[in] ImageBufferSize is Image size in bytes
  @param[in] Force flag suppresses warning message in case of attempted downgrade

  @param[out] pNvmStatus NVM status code

  @retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
  @retval EFI_NOT_FOUND there is no NVDIMM with such Pid
  @retval EFI_DEVICE_ERROR Unable to communicate with PassThru protocol
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_SUCCESS Update has completed successfully
**/
EFI_STATUS
EFIAPI
UpdateDimmFw(
  IN     UINT16 DimmPid,
  IN     CONST VOID *pImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     BOOLEAN Force,
     OUT NVM_STATUS *pNvmStatus OPTIONAL
  );

/**
  Recover firmware of a specified NVDIMM

  @param[in] DimmPid Dimm ID of a NVDIMM on which recovery is to be performed
  @param[in] pImageBuffer is a pointer to FW image
  @param[in] ImageBufferSize is Image size in bytes

  @param[out] pNvmStatus NVM error code

  @retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
  @retval EFI_NOT_FOUND there is no NVDIMM with such Pid
  @retval EFI_DEVICE_ERROR Unable to communicate with Dimm SPI
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_ACCESS_DENIED When SPI access is not unlocked
  @retval EFI_SUCCESS Update has completed successfully
**/
EFI_STATUS
EFIAPI
RecoverDimmFw(
  IN     UINT32 DimmHandle,
  IN     CONST VOID *pImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     CHAR16 *pWorkingDirectory OPTIONAL,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Update firmware or training data in one or all NVDIMMs of the system

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds is a pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pFileName Name is a pointer to a file containing FW image
  @param[in] pWorkingDirectory is a pointer to a path to FW image file
  @param[in] Examine flag enables image verification only
  @param[in] Force flag suppresses warning message in case of attempted downgrade
  @param[in] Recovery flag determine that recovery update should be performed
  @param[in] FlashSpi flag determine if the recovery update should be through the SPI

  @param[out] pFwImageInfo is a pointer to a structure containing FW image information
    need to be provided if examine flag is set
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
  @retval EFI_NOT_FOUND there is no NVDIMM with such Pid
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_SUCCESS Update has completed successfully
**/
EFI_STATUS
EFIAPI
UpdateFw(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CHAR16 *pFileName,
  IN     CHAR16 *pWorkingDirectory OPTIONAL,
  IN     BOOLEAN Examine,
  IN     BOOLEAN Force,
  IN     BOOLEAN Recovery,
  IN     BOOLEAN FlashSPI,
     OUT FW_IMAGE_INFO *pFwImageInfo OPTIONAL,
     OUT COMMAND_STATUS *pCommandStatus
);


/**
Retrieve the number of regions in the system

@param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[out] pCount The number of regions found.

@retval EFI_SUCCESS  The count was returned properly
@retval EFI_INVALID_PARAMETER pCount is NULL.
**/
EFI_STATUS
EFIAPI
GetRegionCount(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT UINT32 *pCount
);

/**
Retrieve the region list

@param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[in] Count The number of regions.
@param[out] pRegions The region list
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_SUCCESS  The region list was returned properly
@retval EFI_INVALID_PARAMETER pRegions is NULL.
@retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
**/
EFI_STATUS
EFIAPI
GetRegions(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT32 Count,
	OUT struct _REGION_INFO *pRegions,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
Retrieve the details about the region specified with region id

@param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
@param[in] RegionId The region id of the region to retrieve
@param[out] pRegion A pointer to the region
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_SUCCESS The region was returned properly
@retval EFI_INVALID_PARAMETER pRegion is NULL
@retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
**/
EFI_STATUS
EFIAPI
GetRegion(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
	IN     UINT16 RegionId,
	OUT struct _REGION_INFO *pRegionInfo,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Gather info about total capacities on all dimms

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[out] pMemoryResourcesInfo structure filled with required information

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_ABORTED PCAT tables not found
  @retval Other errors failure of FW commands
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetMemoryResourcesInfo(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT MEMORY_RESOURCES_INFO *pMemoryResourcesInfo
  );

/**
Gather info about performance on all dimms

@param[in] pThis a pointer to EFI_DCPMM_CONFIG_PROTOCOL instance
@param[out] pDimmCount pointer to the number of dimms on list
@param[out] pDimmsPerformanceData list of dimms' performance data

@retval EFI_INVALID_PARAMETER passed NULL argument
@retval EFI_ABORTED PCAT tables not found
@retval Other errors failure of FW commands
@retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetDimmsPerformanceData(
    IN  EFI_DCPMM_CONFIG_PROTOCOL *pThis,
    OUT UINT32 *pDimmCount,
    OUT DIMM_PERFORMANCE_DATA **pDimmsPerformanceData
);

/**
  Get System Capabilities information from PCAT tables
  Pointer to variable length pInterleaveFormatsSupported is allocated here and must be freed by
  caller.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[out] pSysCapInfo is a pointer to table with System Capabilities information

  @retval EFI_SUCCESS   on success
  @retval EFI_INVALID_PARAMETER NULL argument
  @retval EFI_NOT_STARTED Pcat tables not parsed
**/
EFI_STATUS
EFIAPI
GetSystemCapabilitiesInfo(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT SYSTEM_CAPABILITIES_INFO *pSysCapInfo
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
InitSimulatedPcd (
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
  Get DIMM alarm thresholds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in]  DimmPid The ID of the DIMM
  @param[in]  SensorId Sensor id to retrieve information for
  @param[out] pNonCriticalThreshold Current non-critical threshold for sensor
  @param[out] pEnabledState Current enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER if no DIMM found for DimmPid or input parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_DEVICE_ERROR device error detected
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetAlarmThresholds (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
  IN     UINT8 SensorId,
     OUT INT16 *pNonCriticalThreshold,
     OUT UINT8 *pEnabledState,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set DIMM alarm thresholds

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in]  pDimmIds Pointer to an array of DIMM IDs
  @param[in]  DimmIdsCount Number of items in array of DIMM IDs
  @param[in]  SensorId Sensor id to set values for
  @param[in]  NonCriticalThreshold New non-critical threshold for sensor
  @param[in]  EnabledState New enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
SetAlarmThresholds (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 SensorId,
  IN     INT16 NonCriticalThreshold,
  IN     UINT8 EnabledState,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get NVM DIMM Health Info

  This FW command is used to retrieve current health of system, including SMART information:
  * Overall health status
  * Temperature
  * Spare blocks
  * Alarm Trips set (Temperature/Spare Blocks)
  * Device life span as a percentage
  * Last shutdown status
  * Dirty shutdowns
  * Last shutdown time
  * AIT DRAM status
  * Power Cycles (does not include warm resets or S3 resumes)
  * Power on time (life of DIMM has been powered on)
  * Uptime for current power cycle in seconds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in]  DimmPid The ID of the DIMM
  @param[out] pSensorInfo - pointer to structure containing all Health and Smarth variables.
  @param[out] pLastShutdownStatus pointer to store last shutdown status
  @param[out] pLastShutdownTime pointer to store the time the system was last shutdown
  @param[out] pAitDramEnabled pointer to store the state of AIT DRAM (whether it is Enabled/ Disabled/ Unknown)

  @retval EFI_INVALID_PARAMETER if no DIMM found for DimmPid.
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_DEVICE_ERROR device error detected
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetSmartAndHealth (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
     OUT SENSOR_INFO *pSensorInfo,
     OUT UINT32 *pLastShutdownStatus OPTIONAL,
     OUT UINT64 *pLastShutdownTime OPTIONAL,
     OUT UINT8 *pAitDramEnabled OPTIONAL
  );

/**
  Get Driver API Version

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] pVersion output version

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetDriverApiVersion(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
    OUT CHAR16 pVersion[FW_API_VERSION_LEN]
  );

/**
  Get namespaces info

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] pNamespaceListNode - pointer to namespace list node
  @param[out] pNamespacesCount - namespace count
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI GetNamespaces (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN OUT LIST_ENTRY *pNamespaceListNode,
     OUT UINT32 *pNamespacesCount,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Get actual Region goal capacities that would be used based on input values.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] PersistentMemType Persistent memory type
  @param[in, out] pVolatilePercent Volatile region size in percents.
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percents
  @param[in] ReserveDimm Reserve one DIMM for use as a Storage or not interleaved AppDirect memory
  @param[out] pConfigGoals pointer to output array
  @param[out] pConfigGoalsCount number of elements written
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
GetActualRegionsGoalCapacities(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds    OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds  OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     UINT8 PersistentMemType,
  IN OUT UINT32 *pVolatilePercent,
  IN     UINT32 ReservedPercent,
  IN     UINT8 ReserveDimm,
     OUT REGION_GOAL_PER_DIMM_INFO *pConfigGoals,
     OUT UINT32 *pConfigGoalsCount,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Create region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] Examine Do a dry run if set
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatilePercent Volatile region size in percents
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percents
  @param[in] ReserveDimm Reserve one DIMM for use as a Storage or not interleaved AppDirect memory
  @param[in] LabelVersionMajor Major version of label to init
  @param[in] LabelVersionMinor Minor version of label to init
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
CreateGoalConfig (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     BOOLEAN Examine,
  IN     UINT16 *pDimmIds    OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds  OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     UINT8 PersistentMemType,
  IN     UINT32 VolatilePercent,
  IN     UINT32 ReservedPercent,
  IN     UINT8 ReserveDimm,
  IN     UINT16 LabelVersionMajor,
  IN     UINT16 LabelVersionMinor,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Delete region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
DeleteGoalConfig (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds    OPTIONAL,
  IN     UINT32 SocketIdsCount,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pConfigGoals pointer to output array
  @param[out] pConfigGoalsCount number of elements written
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
GetGoalConfigs(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds    OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     CONST UINT32 ConfigGoalTableSize,
     OUT REGION_GOAL_PER_DIMM_INFO *pConfigGoals,
     OUT UINT32 *pConfigGoalsCount,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Dump region goal configuration into the file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pFilePath Name is a pointer to a dump file path
  @param[in] pDevicePath is a pointer to a device where dump file will be stored
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
DumpGoalConfig(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Load region goal configuration from file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] pFileString Buffer for Region Goal configuration from file
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
LoadGoalConfig(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds,
  IN     UINT32 SocketIdsCount,
  IN     CHAR8 *pFileString,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Start Diagnostic

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] DiagnosticTests bitfield with selected diagnostic tests to be started
  @param[in] DimmIdPreference Preference for the Dimm ID (handle or UID)
  @param[out] ppResult Pointer to the combined result string

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NOT_STARTED Test was not executed
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
StartDiagnostic(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CONST UINT8 DiagnosticTests,
  IN     UINT8 DimmIdPreference,
     OUT CHAR16 **ppResultStr
  );

/**
  Create namespace
  Creates a Storage or AppDirect namespace on the provided region/dimm.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] RegionId the ID of the region that the Namespace is supposed to be created.
  @param[in] DimmId the PID of the Dimm that the Storage Namespace is supposed to be created.
  @param[in] BlockSize the size of each of the block in the device.
    Valid block sizes are: 1 (for AppDirect Namespace), 512 (default), 514, 520, 528, 4096, 4112, 4160, 4224.
  @param[in] BlockCount the amount of block that this namespace should consist
  @param[in] pName - Namespace name.
  @param[in] Enabled boolean value to decide when the driver should hide this
    namespace to the OS
  @param[in] Mode -  boolean value to decide when the namespace
    should have the BTT arena included
  @param[in] ForceAll Suppress all warnings
  @param[in] ForceAlignment Suppress alignment warnings
  @param[out] pActualNamespaceCapacity capacity needed to meet alignment requirements
  @param[out] pNamespaceId Pointer to the ID of the namespace that is created
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS if the operation was successful.
  @retval EFI_ALREADY_EXISTS if a namespace with the provided GUID already exists in the system.
  @retval EFI_DEVICE_ERROR if there was a problem with writing the configuration to the device.
  @retval EFI_OUT_OF_RESOURCES if there is not enough free space on the DIMM/Region.
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system.
**/
EFI_STATUS
EFIAPI
CreateNamespace(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 RegionId,
  IN     UINT16 DimmPid,
  IN     UINT32 BlockSize,
  IN     UINT64 BlockCount,
  IN     CHAR8 *pName,
  IN     BOOLEAN Mode,
  IN     BOOLEAN ForceAll,
  IN     BOOLEAN ForceAlignment,
      OUT UINT64 *pActualNamespaceCapacity,
      OUT UINT16 *pNamespaceId,
      OUT COMMAND_STATUS *pCommandStatus
  );


/**
  Modify namespace
  Modifies a block or persistent memory namespace on the provided region/dimm.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] NamespaceId the ID of the namespace to be modified
  @param[in] pName pointer to a ASCI NULL-terminated string with
    user defined name for the namespace
  @param[in] Force parameter needed to signalize that the caller is aware that this command
    may cause data corruption
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS if the operation was successful.
  @retval EFI_ALREADY_EXISTS if a namespace with the provided GUID does not exist in the system.
  @retval EFI_DEVICE_ERROR if there was a problem with writing the configuration to the device.
  @retval EFI_OUT_OF_RESOURCES if there is not enough free space on the DIMM/Region.
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system.

  Do not change property if NULL pointer provided
**/
EFI_STATUS
EFIAPI
ModifyNamespace(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 NamespaceId,
  IN     CHAR8 *pName,
  IN     BOOLEAN Force,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Delete namespace
  Deletes a block or persistent memory namespace.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] Force Force to perform deleting namespace configs on all affected DIMMs
  @param[in] NamespaceId the ID of the namespace to be removed.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS if the operation was successful.
  @retval EFI_NOT_FOUND if a namespace with the provided GUID does not exist in the system.
  @retval EFI_DEVICE_ERROR if there was a problem with writing the configuration to the device.
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system.
**/
EFI_STATUS
EFIAPI
DeleteNamespace(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     BOOLEAN Force,
  IN     UINT16 NamespaceId,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get Error log for given dimm

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds - array of dimm pids. Use all dimms if pDimms is NULL and DimmsCount is 0.
  @param[in] DimmsCount - number of dimms in array. Use all dimms if pDimms is NULL and DimmsCount is 0.
  @param[in] ThermalError - is thermal error (if not it is media error)
  @param[in] SequenceNumber - sequence number of error to fetch in queue
  @param[in] HighLevel - high level if true, low level otherwise
  @param[in, out] pCount - number of error entries in output array
  @param[out] pErrorLogs - output array of errors
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetErrorLog(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     CONST UINT32 DimmsCount,
  IN     CONST BOOLEAN ThermalError,
  IN     CONST UINT16 SequenceNumber,
  IN     CONST BOOLEAN HighLevel,
  IN OUT UINT32 *pMaxErrorsToFetch,
     OUT ERROR_LOG_INFO *pErrorLogs,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Dump FW logging level

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] LogPageOffset - log page offset
  @param[out] OutputDebugLogSize - size of output debug buffer
  @param[out] ppDebugLogs - pointer to allocated output buffer of debug messages, caller is responsible for freeing
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
DumpFwDebugLog(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 DimmDimmID,
     OUT VOID **ppDebugLogs,
     OUT UINT64 *pBytesWritten,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set Optional Configuration Data Policy using FW command

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds - pointer to array of UINT16 Dimm ids to set
  @param[in] DimmIdsCount - number of elements in pDimmIds
  @param[in] FirstFastRefresh - FirstFastRefresh value to set
  @param[in] ViralPolicy - ViralPolicy value to set

  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
SetOptionalConfigurationDataPolicy(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 FirstFastRefresh,
  IN     UINT8 ViralPolicy,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get requested number of specific DIMM registers for given DIMM id

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] DimmId - ID of a DIMM.
  @param[out] pBsr - Pointer to buffer for Boot Status register, contains
              high and low 4B register.
  @param[out] pFwMailboxStatus - Pointer to buffer for Host Fw Mailbox Status Register
  @param[in] SmallOutputRegisterCount - Number of small output registers to get, max 32.
  @param[out] pFwMailboxOutput - Pointer to buffer for Host Fw Mailbox small output Register.
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
RetrieveDimmRegisters(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 DimmId,
     OUT UINT64 *pBsr,
     OUT UINT64 *pFwMailboxStatus,
  IN     UINT32 SmallOutputRegisterCount,
     OUT UINT64 *pFwMailboxOutput,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Pass Thru command to FW
  Sends a command to FW and waits for response from firmware

  @param[in,out] pCmd A firmware command structure
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the protocol command.
             A Timeout value of 0 means that this function will wait indefinitely for the protocol command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
  @retval EFI_NOT_FOUND The driver could not find the encoded in pCmd DIMM in the system.
  @retval EFI_DEVICE_ERROR FW error received.
  @retval EFI_UNSUPPORTED if the command is ran not in the DEBUG version of the driver.
  @retval EFI_TIMEOUT A timeout occurred while waiting for the protocol command to execute.
**/
EFI_STATUS
EFIAPI
PassThruCommand(
  IN OUT FW_CMD *pCmd,
  IN     UINT64 Timeout
  );

/**
  Attempt to format a dimm through a customer format command

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in]  pDimmIds is a pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in]  DimmIdsCount Number of items in array of DIMM IDs
  @param[in]  Recovery - Perform on non-functional dimms
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_OUT_OF_RESOURCES Memory Allocation failed
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
DimmFormat(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     BOOLEAN Recovery,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
Gather capacities from dimm

@param[in]  DimmPid The ID of the DIMM
@param[out] pVolatileCapacity required volatile capacity
@param[out] pAppDirectCapacity required appdirect capacity
@param[out] pUnconfiguredCapacity required unconfigured capacity
@param[out] pReservedCapacity required reserved capacity
@param[out] pInaccessibleCapacity required inaccessible capacity

@retval EFI_INVALID_PARAMETER passed NULL argument
@retval Other errors failure of FW commands
@retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetCapacities(
  IN     UINT16 DimmPid,
  OUT UINT64 *pVolatileCapacity,
  OUT UINT64 *pAppDirectCapacity,
  OUT UINT64 *pUnconfiguredCapacity,
  OUT UINT64 *pReservedCapacity,
  OUT UINT64 *pInaccessibleCapacity
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
  Get system topology from SMBIOS table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.

  @param[out] ppTopologyDimm Structure containing information about DDR4 entries from SMBIOS.
  @param[out] pTopologyDimmsNumber Number of DDR4 entries found in SMBIOS.

  @retval EFI_SUCCESS All ok.
  @retval EFI_DEVICE_ERROR Unable to find SMBIOS table in system configuration tables.
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
**/
EFI_STATUS
EFIAPI
GetSystemTopology(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT TOPOLOGY_DIMM_INFO **ppTopologyDimm,
     OUT UINT16 *pTopologyDimmsNumber
  );

/**
  Get the system-wide ARS status for the persistent memory capacity of the system.
  In this function, the system-wide ARS status is determined based on the ARS status
  values for the individual DIMMs.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.

  @param[out] pARSStatus pointer to the current system ARS status.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetARSStatus(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT UINT8 *pARSStatus
  );

/**
  Get the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[out] pDriverPreferences pointer to the current driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetDriverPreferences(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
     OUT DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDriverPreferences pointer to the desired driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
SetDriverPreferences(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get DDRT IO init info

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve support data from
  @param[out] pDdrtTrainingStatus pointer to the dimms DDRT training status

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
EFIAPI
GetDdrtIoInitInfo(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pDdrtTrainingStatus
  );

/**
  Get long operation status

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve status from
  @param[in] pOpcode pointer to opcode of long op command to check
  @param[in] pOpcode pointer to subopcode of long op command to check
  @param[out] pPercentComplete pointer to percentage current command has completed
  @param[out] pEstimatedTimeLeft pointer to time to completion BCD
  @param[out] pFwStatus pointer to completed mailbox status code

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
EFIAPI
GetLongOpStatus(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pOpcode OPTIONAL,
     OUT UINT8 *pSubOpcode OPTIONAL,
     OUT UINT16 *pPercentComplete OPTIONAL,
     OUT UINT32 *pEstimatedTimeLeft OPTIONAL,
     OUT EFI_STATUS *pFwStatus
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

/**
  Run the time intensive initialization routines. This should be called by
  any module prior to using the driver protocols.

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.

  @retval EFI_SUCCESS  Initialization succeeded
  @retval EFI_XXX Any number of EFI error codes
**/
EFI_STATUS
EFIAPI
InitializeNvmDimmDriver(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis
  );


/**
InjectError

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[in] pDimmIds - pointer to array of UINT16 Dimm ids to get data for
@param[in] DimmIdsCount - number of elements in pDimmIds

@param[IN] ErrorInjType - Error Inject type
@param[IN] ClearStatus - Is clear status set
@param[IN] pInjectTemperatureValue - Pointer to inject temperature
@param[IN] pInjectPoisonAddress - Pointer to inject poison address
@param[IN] pPoisonType - Pointer to poison type
@param[IN] pPercentageRemaining - Pointer to percentage remaining
@param[out] pCommandStatus Structure containing detailed NVM error codes.

@retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
@retval EFI_INVALID_PARAMETER One or more parameters are invalid
@retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
InjectError(
	IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
	IN     UINT16 *pDimmIds,
	IN     UINT32 DimmIdsCount,
	IN     UINT8  ErrorInjType,
	IN     UINT8  ClearStatus,
	IN     UINT64 *pInjectTemperatureValue OPTIONAL,
	IN     UINT64 *pInjectPoisonAddress,
	IN     UINT8 *pPoisonType,
	IN     UINT8  *pPercentageRemaining,
	OUT COMMAND_STATUS *pCommandStatus
);


/**
Verify target DIMM IDs list. Fill output list of pointers to dimms.

If sockets were specified then get all DIMMs from these sockets.
If DIMM Ids were provided then check if those DIMMs exist.
If there are duplicate DIMM/socket Ids then report error.
If specified DIMMs count is 0 then take all Manageable DIMMs.
Update CommandStatus structure at the end.

@param[in] DimmIds An array of DIMM Ids
@param[in] DimmIdsCount Number of items in array of DIMM Ids
@param[in] SocketIds An array of Socket Ids
@param[in] SocketIdsCount Number of items in array of Socket Ids
@param[in] UninitializedDimms If true only uninitialized dimms are verified, if false only Initialized
@param[out] pDimms Output array of pointers to verified dimms
@param[out] pDimmsNum Number of items in array of pointers to dimms
@param[out] pCommandStatus Pointer to command status structure

@retval EFI_INVALID_PARAMETER Problem with getting specified DIMMs
@retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
VerifyTargetDimms(
	IN     UINT16 DimmIds[]      OPTIONAL,
	IN     UINT32 DimmIdsCount,
	IN     UINT16 SocketIds[]    OPTIONAL,
	IN     UINT32 SocketIdsCount,
	IN     BOOLEAN UninitializedDimms,
	OUT DIMM *pDimms[MAX_DIMMS],
	OUT UINT32 *pDimmsNum,
	OUT COMMAND_STATUS *pCommandStatus
);

#ifdef __MFG__
/**********************************************************Manufacturing related functions***********************************************************************/
/**
Update from mfg to prod firmware

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[in] pDimmIds is a pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
@param[in] DimmIdsCount Number of items in array of DIMM IDs
@param[in] pFileName Name is a pointer to a file containing FW image
@param[in] IsFWUpdate - set to 1 if doing FW update else it is 0
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
@retval EFI_NOT_FOUND there is no NVDIMM with such Pid
@retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
@retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
@retval EFI_SUCCESS Update has completed successfully
**/
EFI_STATUS
EFIAPI
InjectAndUpdateMfgToProdfw(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CHAR16 *pFileName,
  IN     BOOLEAN  IsFWUpdate,
  OUT COMMAND_STATUS *pCommandStatus
);

#endif /* __MFG__ */
#endif /* _NVMDIMM_CONFIG_H_ */
