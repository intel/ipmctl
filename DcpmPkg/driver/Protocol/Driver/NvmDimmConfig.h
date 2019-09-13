/*
 * Copyright (c) 2015-2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
* @file NvmDimmConfig.h
* @brief Implementation of the EFI_DCPMM_CONFIG2_PROTOCOL, a custom protocol
* to configure and manage DCPMMs
*
* @mainpage Barlow Pass Software UEFI FW Protocols
*
* @section Introduction
* This document provides descriptions of protocols implemented by the
* Barlow Pass Driver for UEFI FW. Protocols implemented include:
* - EFI_DRIVER_BINDING_PROTOCOL
* - EFI_COMPONENT_NAME_PROTOCOL & EFI_COMPONENT_NAME2_PROTOCOL
* - EFI_DRIVER_DIAGNOSTICS_PROTOCOL & EFI_DRIVER_DIAGNOSTICS2_PROTOCOL
*  - Provides diagnostic tests for the specified DCPMM
* - EFI_DRIVER_HEALTH_PROTOCOL
*  - Provides standardized health status for the specified DCPMM
* - EFI_DCPMM_CONFIG2_PROTOCOL
*  - Provides configuration management for the specified DCPMM, including:
*   - Discovery
*   - Provisioning
*   - Health & Instrumentation
*   - Support and Maintenance
*   - Diagnostics & Debug
* - EFI_FIRMWARE_MANAGEMENT_PROTOCOL
*  - Provides standardized DCPMM firmware management
* - EFI_STORAGE_SECURITY_COMMAND_PROTOCOL
*  - Provides standardizd DCPMM security functionality
* - EFI_BLOCK_IO_PROTOCOL
*  - Provides BLOCK IO access to the specificed DPCMM Namespaces
* - EFI_NVDIMM_LABEL_PROTOCOL
*  - Provides standardized access to the specified DCPMM Labels
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
#include "NvmDimmConfigInt.h"

extern EFI_GUID gNvmDimmConfigProtocolGuid;
extern EFI_GUID gNvmDimmPbrProtocolGuid;

#define DEVICE_LOCATOR_LEN 128 //!< DIMM Device Locator buffer length

#define FEATURE_NOT_SUPPORTED 0
#define FEATURE_SUPPORTED     1

#define EFI_PERSISTENT_MEMORY_REGION 14
#define MSR_RAPL_POWER_UNIT 0x618

#define FW_UPDATE_INIT_TRANSFER        0x0
#define FW_UPDATE_CONTINUE_TRANSFER    0x1
#define FW_UPDATE_END_TRANSFER         0x2

#define FW_UPDATE_LARGE_PAYLOAD_SELECTOR             0x0
#define FW_UPDATE_SMALL_PAYLOAD_SELECTOR             0x1

#define NFIT_PLATFORM_CAPABILITIES_BIT0     0x1
#define NFIT_MEMORY_CONTROLLER_FLUSH_BIT1   (NFIT_PLATFORM_CAPABILITIES_BIT0 << 0x1)

/**
  The update goes in 3 steps: initialization, data, end, where the data step can be done many times.
  Each of those steps must be done at least one, so the minimum number of packets will be 3.
**/
#define FW_UPDATE_SP_MINIMUM_PACKETS      3
#define FW_UPDATE_SP_MAXIMUM_PACKETS      MAX_FIRMWARE_IMAGE_SIZE_B / UPDATE_FIRMWARE_DATA_PACKET_SIZE

#pragma pack(push)
#pragma pack(1)
typedef struct {
    UINT16 TransactionType : 2;
    UINT16 PacketNumber : 14;
    UINT8 PayloadTypeSelector;
    UINT8 Reserved;
    UINT8 Data[UPDATE_FIRMWARE_DATA_PACKET_SIZE];
    UINT8 Reserved1[60];
} FW_SMALL_PAYLOAD_UPDATE_PACKET;
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
  Retrieve the number of DCPMMs in the system found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDimmCount The number of DCPMMs found in NFIT.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDimmCount(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
  );

/**
  Retrieve the number of uninitialized DCPMMs in the system found thru SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDimmCount The number of DCPMMs found thru SMBUS.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetUninitializedDimmCount(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
  );

/**
  Retrieve the list of DCPMMs found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[in] DimmInfoCategories See @ref DIMM_INFO_CATEGORY_TYPES specifies which (if any)
  additional FW api calls is desired. If ::DIMM_INFO_CATEGORY_NONE, then only
  the properties from the pDimms struct(s) will be populated.
  @param[out] pDimms The dimm list found in NFIT.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDimms(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
  IN     DIMM_INFO_CATEGORIES DimmInfoCategories,
     OUT DIMM_INFO *pDimms
  );

/**
  Retrieve the list of uninitialized DCPMMs found thru SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[out] pDimms The dimm list found thru SMBUS.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetUninitializedDimms(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
     OUT DIMM_INFO *pDimms
  );

/**
  Retrieve the details about the DIMM specified with pid found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] DimmInfoCategories  @ref DIMM_INFO_CATEGORY_TYPES specifies which (if any)
  additional FW api calls is desired. If ::DIMM_INFO_CATEGORY_NONE, then only
  the properties from the pDimm struct will be populated.
  @param[out] pDimmInfo A pointer to the dimm found in NFIT

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDimm(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     DIMM_INFO_CATEGORIES DimmInfoCategories,
     OUT DIMM_INFO *pDimmInfo
  );

#ifdef OS_BUILD
/**
  Retrieve the PMON register values from the dimm

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] SmartDataMask This will specify whether or not to return the extra smart data along with the PMON
  Counter data
  @param[out] pPayloadPMONRegisters A pointer to the output payload PMON registers

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetPMONRegisters(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 SmartDataMask,
  OUT    PMON_REGISTERS *pPayloadPMONRegisters
  );

/**
  Set the PMON register values from the dimm

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] PMONGroupEnable Specifies which PMON Group to enable
  @param[out] pPayloadPMONRegisters A pointer to the output payload PMON registers

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
SetPMONRegisters(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 PMONGroupEnable
  );
#endif

/**
  Retrieve the list of sockets (physical processors) in the host server

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pSocketCount The size of the list of sockets.
  @param[out] ppSockets Pointer to the list of sockets.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetSockets(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pSocketCount,
     OUT SOCKET_INFO **ppSockets
  );

/*
  Retrieve an SMBIOS table type 17 or type 20 for a specific DIMM

  Function available in the DEBUG build only!

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the DIMM to retrieve
  @param[in] Type The Type of SMBIOS table to retrieve. Valid values: 17, 20.
  @param[out] pTable A pointer to the SMBIOS table

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDimmSmbiosTable(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 Type,
     OUT SMBIOS_STRUCTURE_POINTER *pTable
  );

/**
  Check NVM device security state

  Function checks security state of a set of DIMMs. It sets security state
  to mixed when not all DIMMs have the same state.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] pSecurityState security state of a DIMM or all DIMMs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetSecurityState(
    IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] SecurityOperation Security Operation code
  @param[in] pPassphrase a pointer to string with current passphrase
  @param[in] pNewPassphrase a pointer to string with new passphrase
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
SetSecurityState(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 SecurityOperation,
  IN     CHAR16 *pPassphrase,
  IN     CHAR16 *pNewPassphrase,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Retrieve the NFIT ACPI table

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppNFit A pointer to the output NFIT table

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetAcpiNFit (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT ParsedFitHeader **ppNFit
  );

/**
  Retrieve the PCAT ACPI table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppPcat output buffer with PCAT tables

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetAcpiPcat (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT ParsedPcatHeader **ppPcat
  );

/**
  Retrieve the PMTT ACPI table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppPMTTtbl output buffer with PMTT tables. This buffer must be freed by caller.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetAcpiPMTT(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  OUT VOID **ppPMTTtbl
);

/**
  Get Platform Config Data

  The caller is responsible for freeing ppDimmPcdInfo by using FreeDimmPcdInfoArray.

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] PcdTarget Taget PCD partition: ALL=0, CONFIG=1, NAMESPACES=2
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] ppDimmPcdInfo Pointer to output array of PCDs
  @param[out] pDimmPcdInfoCount Number of items in Dimm PCD Info
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
  **/
EFI_STATUS
EFIAPI
GetPcd(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT8 PcdTarget,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  OUT DIMM_PCD_INFO **ppDimmPcdInfo,
  OUT UINT32 *pDimmPcdInfoCount,
  OUT COMMAND_STATUS *pCommandStatus
);

/**
  Clear LSA Namespace partition

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)

  Note: This function is deprecated. Please use the new function DeletePcdConfig.
**/
EFI_STATUS
EFIAPI
DeletePcd(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
Modifies select partition data from the PCD

@param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
@param[in] pDimmIds Pointer to an array of DIMM IDs
@param[in] DimmIdsCount Number of items in array of DIMM IDs
@param[in] ConfigIdMask Bitmask that defines which config to delete. See @ref DELETE_PCD_CONFIG_ALL_MASK
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_SUCCESS Success
@retval EFI_INVALID_PARAMETER One or more input parameters are NULL
@retval EFI_NO_RESPONSE FW busy for one or more dimms
@retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
EFI_STATUS
EFIAPI
ModifyPcdConfig(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT32 ConfigIdMask,
    OUT COMMAND_STATUS *pCommandStatus
);

/**
  Update firmware or training data of a specified NVDIMM

  @param[in] DimmPid Dimm ID of a NVDIMM on which update is to be performed
  @param[in] pImageBuffer is a pointer to FW image
  @param[in] ImageBufferSize is Image size in bytes
  @param[in] Force flag suppresses warning message in case of attempted downgrade

  @param[out] pNvmStatus NVM status code

  @remarks If Address Range Scrub (ARS) is in progress on any target DIMM,
  an attempt will be made to abort ARS and the proceed with the firmware update.

  @remarks A reboot is required to activate the updated firmware image and is
  recommended to ensure ARS runs to completion.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
UpdateDimmFw(
  IN     UINT16 DimmPid,
  IN     CONST VOID *pImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     BOOLEAN Force,
     OUT NVM_STATUS *pNvmStatus
  );

/**
  Recover firmware of a specified NVDIMM

  @param[in] DimmHandle Dimm ID of a NVDIMM on which recovery is to be performed
  @param[in] pImageBuffer is a pointer to FW image
  @param[in] ImageBufferSize is Image size in bytes

  @param[out] pNvmStatus NVM error code
  @param[out] pCommandStatus  command status list

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
RecoverDimmFw(
  IN     UINT32 DimmHandle,
  IN     CONST VOID *pImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     CHAR16 *pWorkingDirectory OPTIONAL,
     OUT NVM_STATUS *pNvmStatus,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Update firmware or training data in one or all NVDIMMs of the system

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds is a pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pFileName Name is a pointer to a file containing FW image
  @param[in] pWorkingDirectory is a pointer to a path to FW image file
  @param[in] Examine flag enables image verification only
  @param[in] Force flag suppresses warning message in case of attempted downgrade
  @param[in] Recovery flag determine that recovery update should be performed
  @param[in] FlashSPI flag determine if the recovery update should be through the SPI

  @param[out] pFwImageInfo is a pointer to a structure containing FW image information
    need to be provided if examine flag is set
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @remarks If Address Range Scrub (ARS) is in progress on any target DIMM,
  an attempt will be made to abort ARS and the proceed with the firmware update.

  @remarks A reboot is required to activate the updated firmware image and is
  recommended to ensure ARS runs to completion.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
UpdateFw(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] UseNfit Flag to indicate NFIT usage
  @param[out] pCount The number of regions found.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetRegionCount(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     BOOLEAN UseNfit,
  OUT UINT32 *pCount
);

/**
  Retrieve the region list

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Count The number of regions.
  @param[in] UseNfit Flag to indicate NFIT usage
  @param[out] pRegions The region info list
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetRegions(
  IN    EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN    UINT32 Count,
  IN    BOOLEAN UseNfit,
  OUT   REGION_INFO *pRegions,
  OUT   COMMAND_STATUS *pCommandStatus
);

/**
  Retrieve the details about the region specified with region id

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] RegionId The region id of the region to retrieve
  @param[out] pRegionInfo A pointer to the region info
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetRegion(
  IN    EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN    UINT16 RegionId,
  OUT   REGION_INFO *pRegionInfo,
  OUT   COMMAND_STATUS *pCommandStatus
);

/**
  Gather info about total capacities on all dimms

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[out] pMemoryResourcesInfo structure filled with required information

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetMemoryResourcesInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT MEMORY_RESOURCES_INFO *pMemoryResourcesInfo
  );

/**
  Gather info about performance on all dimms

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[out] pDimmCount pointer to the number of dimms on list
  @param[out] pDimmsPerformanceData list of dimms' performance data

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDimmsPerformanceData(
    IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
    OUT UINT32 *pDimmCount,
    OUT DIMM_PERFORMANCE_DATA **pDimmsPerformanceData
);

/**
  Get System Capabilities information from PCAT tables
  Pointer to variable length pInterleaveFormatsSupported is allocated here and must be freed by
  caller.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[out] pSysCapInfo is a pointer to table with System Capabilities information

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetSystemCapabilitiesInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT SYSTEM_CAPABILITIES_INFO *pSysCapInfo
  );

/**
  Get DIMM alarm thresholds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in]  DimmPid The ID of the DIMM
  @param[in]  SensorId Sensor ID to retrieve information for. See @ref SENSOR_TYPES
  @param[out] pNonCriticalThreshold Current non-critical threshold for sensor
  @param[out] pEnabledState Current enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetAlarmThresholds (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
  IN     UINT8 SensorId,
     OUT INT16 *pNonCriticalThreshold,
     OUT UINT8 *pEnabledState,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set DIMM alarm thresholds

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in]  pDimmIds Pointer to an array of DIMM IDs
  @param[in]  DimmIdsCount Number of items in array of DIMM IDs
  @param[in]  SensorId Sensor id to set values for
  @param[in]  NonCriticalThreshold New non-critical threshold for sensor
  @param[in]  EnabledState New enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
SetAlarmThresholds (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 SensorId,
  IN     INT16 NonCriticalThreshold,
  IN     UINT8 EnabledState,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get DIMM Health Info

  This FW command is used to retrieve current health of system, including SMART information:
  * Overall health status
  * Temperature
  * Alarm Trips set (Temperature/Spare Blocks)
  * Device life span as a percentage
  * Latched Last shutdown status
  * Unlatched Last shutdown status
  * Dirty shutdowns
  * Last shutdown time
  * AIT DRAM status
  * Power Cycles (does not include warm resets or S3 resumes)
  * Power on time (life of DIMM has been powered on)
  * Uptime for current power cycle in seconds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in]  DimmPid The ID of the DIMM
  @param[out] pHealthInfo pointer to structure containing all Health and Smarth variables

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetSmartAndHealth (
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  UINT16 DimmPid,
  OUT SMART_AND_HEALTH_INFO *pHealthInfo
  );

/**
  Get Driver API Version

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pVersion output version in string format MM.mm. M = Major, m = minor.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDriverApiVersion(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
    OUT CHAR16 pVersion[FW_API_VERSION_LEN]
  );

/**
  Get namespaces info

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pNamespaceListNode Pointer to namespace list node of @ref NAMESPACE_INFO structs.
  @param[out] pNamespacesCount Count of namespaces on the list
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI GetNamespaces (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN OUT LIST_ENTRY *pNamespaceListNode,
     OUT UINT32 *pNamespacesCount,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Get actual Region goal capacities that would be used based on input values.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  @param[out] pNumOfDimmsTargeted number of DIMMs targeted in a goal config request
  @param[out] pMaxPMInterleaveSetsPerDie pointer to Maximum PM Interleave Sets per Die
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
GetActualRegionsGoalCapacities(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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
     OUT UINT32 *pNumOfDimmsTargeted         OPTIONAL,
     OUT UINT32 *pMaxPMInterleaveSetsPerDie  OPTIONAL,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Create region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  @param[out] pMaxPMInterleaveSetsPerDie pointer to Maximum PM Interleave Sets per Die
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
CreateGoalConfig (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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
  OUT UINT32 *pMaxPMInterleaveSetsPerDie  OPTIONAL,
  OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Delete region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
DeleteGoalConfig (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds    OPTIONAL,
  IN     UINT32 SocketIdsCount,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] ConfigGoalTableSize Number of elements in the pConfigGoals array passed in
  @param[out] pConfigGoals pointer to output array
  @param[out] pConfigGoalsCount number of elements written
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetGoalConfigs(
  IN    EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN    UINT16 *pDimmIds      OPTIONAL,
  IN    UINT32 DimmIdsCount,
  IN    UINT16 *pSocketIds    OPTIONAL,
  IN    UINT32 SocketIdsCount,
  IN    CONST UINT32 ConfigGoalTableSize,
  OUT   REGION_GOAL_PER_DIMM_INFO *pConfigGoals,
  OUT   UINT32 *pConfigGoalsCount,
  OUT   COMMAND_STATUS *pCommandStatus
);

/**
  Dump region goal configuration into the file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pFilePath is a pointer to a dump file path
  @param[in] pDevicePath is a pointer to a device where dump file will be stored
  @param[out] pCommandStatus structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
DumpGoalConfig(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Load region goal configuration from file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] pFileString Buffer for Region Goal configuration from file
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
LoadGoalConfig(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds,
  IN     UINT32 SocketIdsCount,
  IN     CHAR8 *pFileString,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Start Diagnostic Tests with Detail parameter

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] DiagnosticTests bitfield with selected diagnostic tests to be started
  @param[in] DimmIdPreference Preference for the Dimm ID (handle or UID)
  @param[out] ppResult Pointer to the structure with information about test

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
StartDiagnostic(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CONST UINT8 DiagnosticTests,
  IN     UINT8 DimmIdPreference,
  OUT DIAG_INFO **ppResultStr

);

/**
  Create namespace
  Creates a Storage or AppDirect namespace on the provided region/dimm.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] RegionId the ID of the region that the Namespace is supposed to be created.
  @param[in] DimmPid the PID of the Dimm that the Storage Namespace is supposed to be created.
  @param[in] BlockSize the size of each of the block in the device.
    Valid block sizes are: 1 (for AppDirect Namespace), 512 (default), 514, 520, 528, 4096, 4112, 4160, 4224.
  @param[in] BlockCount the amount of block that this namespace should consist
  @param[in] pName - Namespace name.
  @param[in] Mode -  boolean value to decide when the namespace
    should have the BTT arena included
  @param[in] ForceAll Suppress all warnings
  @param[in] ForceAlignment Suppress alignment warnings
  @param[out] pActualNamespaceCapacity capacity needed to meet alignment requirements
  @param[out] pNamespaceId Pointer to the ID of the namespace that is created
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
CreateNamespace(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] NamespaceId the ID of the namespace to be modified
  @param[in] pName pointer to a ASCI NULL-terminated string with
    user defined name for the namespace
  @param[in] Force parameter needed to signalize that the caller is aware that this command
    may cause data corruption
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)

  Do not change property if NULL pointer provided
**/
EFI_STATUS
EFIAPI
ModifyNamespace(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 NamespaceId,
  IN     CHAR8 *pName,
  IN     BOOLEAN Force,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Delete namespace
  Deletes a block or persistent memory namespace.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] Force Force to perform deleting namespace configs on all affected DIMMs
  @param[in] NamespaceId the ID of the namespace to be removed.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
DeleteNamespace(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     BOOLEAN Force,
  IN     UINT16 NamespaceId,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get Error log for given dimm

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds - array of dimm pids. Use all dimms if pDimms is NULL and DimmsCount is 0.
  @param[in] DimmsCount - number of dimms in array. Use all dimms if pDimms is NULL and DimmsCount is 0.
  @param[in] ThermalError - TRUE = Thermal error, FALSE = media error
  @param[in] SequenceNumber - sequence number of error to fetch in queue
  @param[in] HighLevel - high level if true, low level otherwise
  @param[in, out] pErrorLogCount - IN: element count of pErrorLogs. OUT: Count of error entries in pErrorLogs
  @param[out] pErrorLogs - output array of errors. Allocated to elmeent count indicated by pErrorLogCount
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetErrorLog(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     CONST UINT32 DimmsCount,
  IN     CONST BOOLEAN ThermalError,
  IN     CONST UINT16 SequenceNumber,
  IN     CONST BOOLEAN HighLevel,
  IN OUT UINT32 *pErrorLogCount,
     OUT ERROR_LOG_INFO *pErrorLogs,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get the debug log from a specified dimm and fw debug log source

  Note: The caller is responsible for freeing the returned buffer

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID identifier of what dimm to get log pages from
  @param[in] LogSource debug log source buffer to retrieve
  @param[in] Reserved for future use. Must be 0 for now.
  @param[out] ppDebugLogBuffer an allocated buffer containing the raw debug log
  @param[out] pDebugLogBufferSize the size of the raw debug log buffer
  @param[out] pCommandStatus structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetFwDebugLog(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
  IN     UINT8 LogSource,
  IN     UINT32 Reserved,
     OUT VOID **ppDebugLogBuffer,
     OUT UINTN *pDebugLogBufferSize,
     OUT COMMAND_STATUS *pCommandStatus
  );


/**
  Dump FW debug logs

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID identifier of what dimm to get log pages from
  @param[out] ppDebugLogs pointer to allocated output buffer of debug messages, caller is responsible for freeing
  @param[out] pBytesWritten size of output buffer
  @param[out] pCommandStatus structure containing detailed NVM error codes

  Note: This function is deprecated. Please use the new function GetFwDebugLog.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
DumpFwDebugLog(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT VOID **ppDebugLogs,
     OUT UINT64 *pBytesWritten,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set Optional Configuration Data Policy using FW command

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds - pointer to array of UINT16 Dimm ids to set
  @param[in] DimmIdsCount - number of elements in pDimmIds
  @param[in] AveragePowerReportingTimeConstantMultiplier - AveragePowerReportingTimeConstantMultiplier value to set
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
EFI_STATUS
EFIAPI
SetOptionalConfigurationDataPolicy(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 *AveragePowerReportingTimeConstantMultiplier,
  IN     UINT32 *AveragePowerReportingTimeConstant,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get requested number of specific DIMM registers for given DIMM id

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmId ID of a DIMM.
  @param[out] pBsr Pointer to buffer for Boot Status register, contains
              high and low 4B register.
  @param[out] pFwMailboxStatus Pointer to buffer for Host Fw Mailbox Status
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
RetrieveDimmRegisters(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmId,
     OUT UINT64 *pBsr,
     OUT UINT8 *pFwMailboxStatus,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Pass Through Command to FW
  Sends a command to FW and waits for response from firmware
  NOTE: Available only in debug driver.

  @param[in,out] pCmd A firmware command structure
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the protocol command.
             A Timeout value of 0 means that this function will wait indefinitely for the protocol command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
PassThruCommand(
  IN OUT FW_CMD *pCmd,
  IN     UINT64 Timeout
  );

/**
  Attempt to format a dimm through a customer format command

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in]  pDimmIds is a pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in]  DimmIdsCount Number of items in array of DIMM IDs
  @param[in]  Recovery - Perform on non-functional dimms
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
DimmFormat(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
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
  Get system topology from SMBIOS table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.

  @param[out] ppTopologyDimm Structure containing information about DDR4 entries from SMBIOS.
  @param[out] pTopologyDimmsNumber Number of DDR4 entries found in SMBIOS.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetSystemTopology(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT TOPOLOGY_DIMM_INFO **ppTopologyDimm,
     OUT UINT16 *pTopologyDimmsNumber
  );

/**
  Get the system-wide ARS status for the persistent memory capacity of the system.
  In this function, the system-wide ARS status is determined based on the ARS status
  values for the individual DIMMs.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.

  @param[out] pARSStatus pointer to the current system ARS status.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetARSStatus(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT8 *pARSStatus
  );

/**
  Get the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDriverPreferences pointer to the current driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)k
**/
EFI_STATUS
EFIAPI
GetDriverPreferences(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDriverPreferences pointer to the desired driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
SetDriverPreferences(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get DDRT IO init info

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve support data from
  @param[out] pDdrtTrainingStatus pointer to the dimms DDRT training status

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetDdrtIoInitInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pDdrtTrainingStatus
  );

/**
  Get long operation status

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve status from
  @param[in] pOpcode pointer to opcode of long op command to check
  @param[in] pSubOpcode pointer to subopcode of long op command to check
  @param[out] pPercentComplete pointer to percentage current command has completed
  @param[out] pEstimatedTimeLeft pointer to time to completion BCD
  @param[out] pFwStatus pointer to completed mailbox status code

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetLongOpStatus(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pOpcode OPTIONAL,
     OUT UINT8 *pSubOpcode OPTIONAL,
     OUT UINT16 *pPercentComplete OPTIONAL,
     OUT UINT32 *pEstimatedTimeLeft OPTIONAL,
     OUT EFI_STATUS *pFwStatus
  );

/**
  InjectError

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds - pointer to array of UINT16 Dimm ids to get data for
  @param[in] DimmIdsCount - number of elements in pDimmIds
  @param[in] ErrorInjType - Error Inject type
  @param[in] ClearStatus - Is clear status set
  @param[in] pInjectTemperatureValue - Pointer to inject temperature
  @param[in] pInjectPoisonAddress - Pointer to inject poison address
  @param[in] pPoisonType - Pointer to poison type
  @param[in] pPercentageRemaining - Pointer to percentage remaining
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
InjectError(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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
  GetBsr value and return bsr or bootstatusbitmask depending on the requested options
  UEFI - Read directly from BSR register
  OS - Get BSR value from BIOS emulated command

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID -  dimm handle of the DIMM
  @param[out] pBsrValue - pointer to  BSR register value OPTIONAL
  @param[out] pBootStatusBitMask  - pointer to bootstatusbitmask OPTIONAL

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetBSRAndBootStatusBitMask(
  IN      EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN      UINT16 DimmID,
  OUT     UINT64 *pBsrValue OPTIONAL,
  OUT     UINT16 *pBootStatusBitmask OPTIONAL
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
  @param[in] CheckSupportedConfigDimms If true, include dimms in unmapped set of dimms (non-POR) in
                                       returned dimm list. If false, skip these dimms from returned list
  @param[out] pDimms Output array of pointers to verified dimms
  @param[out] pDimmsNum Number of items in array of pointers to dimms
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
VerifyTargetDimms(
  IN     UINT16 DimmIds[]      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 SocketIds[]    OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     BOOLEAN UninitializedDimms,
  IN     BOOLEAN CheckSupportedConfigDimms,
  OUT DIMM *pDimms[MAX_DIMMS],
  OUT UINT32 *pDimmsNum,
  OUT COMMAND_STATUS *pCommandStatus
);

/**
  Examine a given DIMM to see if a long op is in progress and report it back

  @param[in] pDimm The dimm to check the status of
  @param[out] pNvmStatus The status of the dimm's long op status. NVM_SUCCESS = No long op status is under way.

  @retval EFI_SUCCESS if the request for long op status was successful (whether a long op status is under way or not)
  @retval EFI_... the error preventing the check for the long op status
**/
EFI_STATUS
CheckForLongOpStatusInProgress(
  IN     DIMM *pDimm,
  OUT    NVM_STATUS *pNvmStatus
);

/**
  Get Command Access Policy is used to retrieve a list of FW commands that may be restricted.
  NOTE: Available only in debug driver.

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID Handle of the DIMM
  @param[in,out] pCount IN: Count is number of elements in the pCapInfo array. OUT: number of elements written to pCapInfo
  @param[out] pCapInfo Array of Command Access Policy Entries. If NULL, pCount will be updated with number of elements required. OPTIONAL

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetCommandAccessPolicy(
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  UINT16 DimmID,
  IN OUT UINT32 *pCount,
  IN OUT COMMAND_ACCESS_POLICY_ENTRY *pCapInfo OPTIONAL
);

/**
  Get Command Effect Log is used to retrieve a list DIMM FW commands and their effects on the DIMM subsystem.

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID Handle of the DIMM

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetCommandEffectLog(
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  UINT16 DimmID,
  IN OUT COMMAND_EFFECT_LOG_ENTRY **ppLogEntry,
  IN OUT UINT32 *pEntryCount
);

#ifndef OS_BUILD
/**
  This function makes calls to the dimms required to initialize the driver.

  @retval EFI_SUCCESS if no errors.
  @retval EFI_xxxx depending on error encountered.
**/
EFI_STATUS
LoadArsList();
#endif

/**
  Gets value of transport protocol and payload size settings from platform

  @param[in]     pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in,out] pAttribs A pointer to a variable used to store protocol and payload settings

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
**/
EFI_STATUS
EFIAPI
GetFisTransportAttributes(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN OUT EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS *pAttribs
);

/**
  Sets value of transport protocol and payload size settings for platform

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in] Attribs The new value to assign to protocol and payload settings

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
**/
EFI_STATUS
EFIAPI
SetFisTransportAttributes(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS Attribs
);

#endif /* _NVMDIMM_CONFIG_H_ */
