/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /**
 * @file NvmInterface.h
 * @brief Implementation of the EFI_DCPMM_CONFIG2_PROTOCOL, a custom protocol
 * to configure and manage Intel Optane persistent memory modules
 */

#ifndef _NVM_INTERFACE_H_
#define _NVM_INTERFACE_H_

#include <IndustryStandard/SmBios.h>
#include "NvmStatus.h"
#ifndef OS_BUILD
#include "Dcpmm.h"
#endif
#include "NvmTypes.h"
#include "NvmTables.h"
#include <FwUtility.h>
#include <PcdCommon.h>

// Auto = no restrictions
typedef enum _TRANSPORT_PROTOCOL {
  FisTransportSmbus = 0,
  FisTransportDdrt = 1,
  FisTransportAuto = 2
} TRANSPORT_PROTOCOL;

// Auto = no restrictions
typedef enum _TRANSPORT_PAYLOAD_SIZE {
  FisTransportSizeSmallMb = 0,
  FisTransportSizeLargeMb = 1,
  FisTransportSizeAuto = 2
} TRANSPORT_PAYLOAD_SIZE;

typedef struct _EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS {
  TRANSPORT_PROTOCOL Protocol;
  TRANSPORT_PAYLOAD_SIZE PayloadSize;
} EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS;

/**
  Resolves to TRUE if the "-smbus" flag was passed in via CLI or equivalent.
  Restricts all communications to smbus only. FALSE otherwise.
**/
#define IS_SMBUS_FLAG_ENABLED(TransportAttribs) (FisTransportSmbus == TransportAttribs.Protocol)

/**
  Resolves to TRUE if the "-ddrt" flag was passed in via CLI or equivalent.
  Restricts all communications to DDRT only. FALSE otherwise.
**/
#define IS_DDRT_FLAG_ENABLED(TransportAttribs) (FisTransportDdrt == TransportAttribs.Protocol)

/**
  Resolves to TRUE if the "-spmb" flag was passed in via CLI or equivalent.
  Restricts all communications to small payload mailbox only. FALSE otherwise.
**/
#define IS_SMALL_PAYLOAD_FLAG_ENABLED(TransportAttribs) (FisTransportSizeSmallMb == TransportAttribs.PayloadSize)

/**
  Resolves to TRUE if the "-lpmb" flag was passed in via CLI or equivalent.
  Restricts all communications to large payload mailbox only. FALSE otherwise.
**/
#define IS_LARGE_PAYLOAD_FLAG_ENABLED(TransportAttribs) (FisTransportSizeLargeMb == TransportAttribs.PayloadSize)

#define MAX_NO_OF_DIAGNOSTIC_SUBTESTS 5

#define EFI_DCPMM_CONFIG2_PROTOCOL_GUID \
  {0xd5a7cf05, 0x8ec1, 0x48cb, {0x95, 0x94, 0x20, 0x52, 0xf9, 0xbd, 0x11, 0x56}}

#define NVMDIMM_DRIVER_DEVICE_PATH_GUID \
  { 0xb976a9d2, 0x8772, 0x414f, {0x9f, 0xb0, 0x05, 0x99, 0x95, 0xf4, 0xbe, 0xac}}

#define EFI_DCPMM_PBR_PROTOCOL_GUID \
  {0x8761c5cc, 0x5dfc, 0x40cc, {0x89, 0xc9, 0xb6, 0x35, 0xea, 0x99, 0x23, 0x9f}}

typedef struct _EFI_DCPMM_CONFIG2_PROTOCOL EFI_DCPMM_CONFIG2_PROTOCOL;

typedef struct _EFI_DCPMM_PBR_PROTOCOL EFI_DCPMM_PBR_PROTOCOL;

/**
  Retrieve the number of PMem modules in the system found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDimmCount The number of PMem modules found in NFIT.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DIMM_COUNT) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
);

/**
  Retrieve the number of uninitialized PMem modules in the system found through SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDimmCount The number of PMem modules found through SMBUS.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_UNINITIALIZED_DIMM_COUNT) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
  );

/**
  Retrieve the list of PMem modules found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[in] dimmInfoCategories The categories of additional PMem module info parameters to retrieve
  @param[out] pDimms The PMem module list found in NFIT.

  @retval EFI_SUCCESS  The PMem module list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL.
  @retval EFI_NOT_FOUND PMem module not found
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DIMMS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO *pDimms
);

/**
  Retrieve the list of uninitialized PMem modules found through SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[out] pDimms The PMem module list found through SMBUS.

  @retval EFI_SUCCESS  The PMem module list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameter are NULL.
  @retval EFI_NOT_FOUND PMem module not found
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_UNINITIALIZED_DIMMS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
     OUT DIMM_INFO *pDimms
  );

/**
  Retrieve the details about the PMem module specified with pid found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the PMem module to retrieve
  @param[in] dimmInfoCategories The categories of additional PMem module info parameters to retrieve
  @param[out] pDimmInfo A pointer to the PMem module found in NFIT

  @retval EFI_SUCCESS  The PMem module information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the PMem module with the pid provided does not exist.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DIMM) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO *pDimmInfo
);
#ifdef OS_BUILD
/**
  Get the PMON registers

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the PMem module to retrieve
  @param[in] dimmInfoCategories The categories of additional PMem module info parameters to retrieve
  @param[out] pDimmInfo A pointer to the PMem module found in NFIT

  @retval EFI_SUCCESS  The PMem module information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the PMem module with the pid provided does not exist.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_PMON) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 SmartDataMask,
  OUT    PMON_REGISTERS *pPayloadPMONRegisters
);

/**
  Set the PMON Group

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the PMem module to retrieve
  @param[in] dimmInfoCategories The categories of additional PMem module info parameters to retrieve
  @param[out] pDimmInfo A pointer to the PMem module found in NFIT

  @retval EFI_SUCCESS  The PMem module information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the PMem module with the pid provided does not exist.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_SET_PMON) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 PMONGroupEnable
);
#endif

/**
  Retrieve the details about the uninitialized PMem module specified with pid found through SMBUS

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the PMem module to retrieve
  @param[out] pDimmInfo A pointer to the PMem module found through SMBUS

  @retval EFI_SUCCESS  The PMem module information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the PMem module with the pid provided does not exist.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_UNINITIALIZED_DIMM) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
     OUT DIMM_INFO *pDimmInfo
  );

/**
  Retrieve the list of sockets (physical processors) in the host server

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pSocketCount The size of the list of sockets.
  @param[out] ppSockets Pointer to the list of sockets.

  @retval EFI_SUCCESS  The socket list was returned properly or,
                       the platform does not support socket sku limits
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL.
  @retval EFI_NOT_FOUND PCAT tables could not be retrieved successfully
  @retval EFI_DEVICE_ERROR Internal function error
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_SOCKETS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pSocketCount,
     OUT SOCKET_INFO **ppSockets
);

/**
  Retrieve an SMBIOS table type 17 table for a specific PMem module

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the PMem module to retrieve
  @param[out] pTable A pointer to the SMBIOS table

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER pTable is NULL
  @retval EFI_INVALID_PARAMETER PMem module pid is not valid.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DIMM_SMBIOS_TABLE) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 Type,
     OUT SMBIOS_STRUCTURE_POINTER *pTable
);

/**
  Retrieve the NFIT ACPI table

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppNFit A pointer to the output NFIT table

  @retval EFI_SUCCESS Ok
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
  @retval EFI_NOT_FOUND PCAT tables not found
  @retval EFI_INVALID_PARAMETER pNFit is NULL
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ACPI_NFIT) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT ParsedFitHeader **ppNFit
);

/**
  Retrieve the PCAT ACPI table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppPcat output buffer with PCAT tables

  @retval EFI_SUCCESS Ok
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
  @retval EFI_NOT_FOUND PCAT tables not found
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ACPI_PCAT) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT ParsedPcatHeader **pPcat
);

/**
Retrieve the PMTT ACPI table

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
@param[out] ppPMTT output buffer with PMTT tables

@retval EFI_SUCCESS Ok
@retval EFI_OUT_OF_RESOURCES Problem with allocating memory
@retval EFI_NOT_FOUND PCAT tables not found
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ACPI_PMTT) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  OUT VOID **pPMTT
);

/**
  Get Platform Config Data

  The caller is responsible for freeing ppDimmPcdInfo by using FreeDimmPcdInfoArray.

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param{in] PcdTarget Target PCD partition: ALL=0, CONFIG=1, NAMESPACES=2
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[out] ppDimmPcdInfo Pointer to output array of PCDs
  @param[out] pDimmPcdInfoCount Number of items in PMem module PCD Info
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_PCD) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT8 PcdTarget,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
     OUT DIMM_PCD_INFO **ppDimmPcdInfo,
     OUT UINT32 *pDimmPcdInfoCount,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
Clear PCD configs

@param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
@param[in] pDimmIds Pointer to an array of PMem module IDs
@param[in] DimmIdsCount Number of items in array of PMem module IDs
@param[in] ConfigIdMask Bitmask that defines which config to delete
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_SUCCESS Success
@retval EFI_INVALID_PARAMETER One or more input parameters are NULL
@retval EFI_NO_RESPONSE FW busy for one or more PMem modules
@retval EFI_OUT_OF_RESOURCES Memory allocation failure
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_DELETE_PCD_CONFIG) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT32 ConfigIdMask,
  OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Check NVM device security state

  Function checks security state of a set of PMem modules. It sets security state
  to mixed when not all PMem modules have the same state.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[out] pSecurityState security state of a PMem module or all PMem modules
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER when pSecurityState is NULL
  @retval EFI_NOT_FOUND it was not possible to get state of a PMem module
  @retval EFI_SUCCESS state correctly detected and stored in pSecurityState
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_SECURITY_STATE) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
     OUT UINT8 *pSecurityState,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Set NVM device security state.

  Function sets security state on a set of PMem modules. If there is a failure on
  one of PMem modules function continues with setting state on following PMem modules
  but exits with error.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of PMem module IDs - if NULL, execute operation on all PMem modules
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] SecurityOperation Security Operation code
  @param[in] pPassphrase a pointer to string with current passphrase
  @param[in] pNewPassphrase a pointer to string with new passphrase
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER when pLockState is NULL
  @retval EFI_OUT_OF_RESOURCES couldn't allocate memory for a structure
  @retval EFI_UNSUPPORTED LockState to be set is not recognized, or mixed sku of PMem modules is detected
  @retval EFI_DEVICE_ERROR setting state for a PMem module failed
  @retval EFI_NOT_FOUND a PMem module was not found
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
  @retval EFI_SUCCESS security state correctly set
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_SET_SECURITY_STATE) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 SecurityOperation,
  IN     CHAR16 *pPassphrase,
  IN     CHAR16 *pNewPassphrase,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Gather info about total capacities on all PMem modules

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] UseNfit flag to indicate NFIT usage
  @param[out] pMemoryResourcesInfo structure filled with required information

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_ABORTED PCAT tables not found
  @retval Other errors failure of FW commands
  @retval EFI_SUCCESS Success
  @retval EFI_NO_RESPONSE FW busy on one or more PMem modules
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_REGION_COUNT) (
	IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     BOOLEAN UseNfit,
     OUT UINT32 *pCount
  );

/**
Retrieve the region list

@param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
@param[in] Count The number of regions.
@param[in] UseNfit flag to indicate NFIT usage
@param[out] pRegions The region list
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
@retval EFI_SUCCESS  The region list was returned properly
@retval EFI_INVALID_PARAMETER pRegions is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_REGIONS) (
	IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 Count,
  IN     BOOLEAN UseNfit,
	OUT struct _REGION_INFO *pRegions,
     OUT COMMAND_STATUS *pCommandStatus
	);

/**
  Retrieve the details about the region specified with region id

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] RegionId The region id of the region to retrieve
  @param[out] pRegion A pointer to the region
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_SUCCESS The region was returned properly
  @retval EFI_INVALID_PARAMETER pRegion is NULL
  @retval EFI_NO_RESPONSE FW busy on one or more PMem modules
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_REGION) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 RegionId,
  OUT struct _REGION_INFO *pRegion,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Gather info about total capacities on all PMem modules

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[out] pMemoryResourcesInfo structure filled with required information

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_ABORTED PCAT tables not found
  @retval Other errors failure of FW commands
  @retval EFI_SUCCESS Success
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_MEMORY_RESOURCES_INFO) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT MEMORY_RESOURCES_INFO *pMemoryResourcesInfo
  );

/**
Gather info about performance on all PMem modules

@param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
@param[out] pDimmCount pointer to the number of PMem modules on list
@param[out] pDimmsPerformanceData list of PMem modules' performance data

@retval EFI_INVALID_PARAMETER passed NULL argument
@retval EFI_ABORTED PCAT tables not found
@retval Other errors failure of FW commands
@retval EFI_SUCCESS Success
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DIMMS_PERFORMANCE) (
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

  @retval EFI_SUCCESS   on success
  @retval EFI_INVALID_PARAMETER NULL argument
  @retval EFI_NOT_STARTED Pcat tables not parsed
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_SYSTEM_CAPABILITIES_INFO) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT SYSTEM_CAPABILITIES_INFO *pSysCapInfo
  );

/**
  Update firmware or training data in one or all PMem modules of the system

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds is a pointer to an array of PMem module IDs - if NULL, execute operation on all PMem modules
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] pFileName Name is a pointer to a file containing FW image
  @param[in] pWorkingDirectory is a pointer to a path to FW image file
  @param[in] Examine flag enables image verification only
  @param[in] Force flag suppresses warning message in case of attempted downgrade
  @param[in] Recovery **Deprecated** Run the update on non-functional PMem modules only
  @param[in] Reserved Set to FALSE

  @param[out] pFwImageInfo is a pointer to a structure containing FW image information
    need to be provided if examine flag is set
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
  @retval EFI_NOT_FOUND there is no PMem module with such Pid
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_SUCCESS Update has completed successfully
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_UPDATE_FW) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CHAR16 *pFileName,
  IN     CHAR16 *pWorkingDirectory OPTIONAL,
  IN     BOOLEAN Examine,
  IN     BOOLEAN Force,
  IN     BOOLEAN Recovery,
  IN     BOOLEAN Reserved,
     OUT NVM_FW_IMAGE_INFO *pFwImageInfo OPTIONAL,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Get PMem module alarm thresholds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in]  DimmPid The ID of the PMem module
  @param[in]  SensorId Sensor id to retrieve information for
  @param[out] pNonCriticalThreshold Current non-critical threshold for sensor
  @param[out] pEnabledState Current enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER if no PMem module found for DimmPid or input parameter is NULL.
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_DEVICE_ERROR device error detected
  @retval EFI_SUCCESS Success
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ALARM_THR) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
  IN     UINT8 SensorId,
     OUT INT16 *pNonCriticalThreshold,
     OUT UINT8 *pEnabledState,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Set PMem module alarm thresholds

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in]  pDimmIds Pointer to an array of PMem module IDs
  @param[in]  DimmIdsCount Number of items in array of PMem module IDs
  @param[in]  SensorId Sensor id to set values for
  @param[in]  NonCriticalThreshold New non-critical threshold for sensor
  @param[in]  EnabledState New enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_SET_ALARM_THR) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 SensorId,
  IN     INT16 NonCriticalThreshold,
  IN     UINT8 EnabledState,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Get PMem module Health Info

  This FW command is used to retrieve current health of system, including SMART information:
  * Overall health status
  * Temperature
  * Spare blocks
  * Alarm Trips set (Temperature/Spare Blocks)
  * Device life span as a percentage
  * Last shutdown status
  * Dirty shutdowns
  * Last shutdown time.
  * Power Cycles (does not include warm resets or S3 resumes)
  * Power on time (life of PMem module has been powered on)
  * Uptime for current power cycle in seconds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in]  DimmPid The ID of the PMem module
  @param[out] pHealthInfo - pointer to structure containing all Health and Smart variables

  @retval EFI_INVALID_PARAMETER if no PMem module found for DimmPid.
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_DEVICE_ERROR device error detected
  @retval EFI_SUCCESS Success
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_SMART_AND_HEALTH) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
     OUT SMART_AND_HEALTH_INFO *pHealthInfo
  );

/**
  Get actual Region goal capacities that would be used based on input values.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] PersistentMemType Persistent memory type
  @param[in, out] pVolatilePercent Volatile region size in percents.
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percents
  @param[in] ReserveDimm Reserve one PMem module for use as a not interleaved AppDirect memory
  @param[out] pConfigGoals pointer to output array
  @param[out] pConfigGoalsCount number of elements written
  @param[out] pNumOfDimmsTargeted number of PMem modules targeted in a goal config request
  @param[out] pMaxPMInterleaveSetsPerDie pointer to Maximum PM Interleave Sets per Die
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ACTUAL_REGIONS_GOAL_CAPACITIES) (
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
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatilePercent Volatile region size in percents
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percents
  @param[in] ReserveDimm Reserve one PMem module for use as a not interleaved AppDirect memory
  @param[in] LabelVersionMajor Major version of label to init
  @param[in] LabelVersionMinor Minor version of label to init
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_CREATE_GOAL) (
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
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  EFI_DCPMM_CONFIG_DELETE_GOAL DeleteGoalConfig;
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_DELETE_GOAL) (
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
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pConfigGoals pointer to output array
  @param[out] pConfigGoalsCount number of elements written
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_GOALS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pFilePath Name is a pointer to a dump file path
  @param[in] pDevicePath is a pointer to a device where dump file will be stored
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_DUMP_GOAL)
(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Load region goal configuration from file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] pFileString Buffer for Region Goal configuration from file
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_LOAD_GOAL)
(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds,
  IN     UINT32 SocketIdsCount,
  IN     CHAR8 *pFileString,
     OUT COMMAND_STATUS *pCommandStatus
  );

typedef struct DIAGNOSTIC_INFO
{
  CHAR16 *TestName;
  CHAR16 *Message;
  CHAR16 *State;
  UINT8 StateVal;
  UINT32  ResultCode;
  CHAR16 *SubTestName[MAX_NO_OF_DIAGNOSTIC_SUBTESTS];
  UINT8  SubTestStateVal[MAX_NO_OF_DIAGNOSTIC_SUBTESTS];
  CHAR16 *SubTestState[MAX_NO_OF_DIAGNOSTIC_SUBTESTS];
  CHAR16 *SubTestMessage[MAX_NO_OF_DIAGNOSTIC_SUBTESTS];
  CHAR16 *SubTestEventCode[MAX_NO_OF_DIAGNOSTIC_SUBTESTS];
} DIAG_INFO;

/**
  Start Diagnostic Detail

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of PMem module IDs
  @param[in] DimmIdsCount Number of items in array of PMem module IDs
  @param[in] DiagnosticTestId ID of a diagnostic test to be started
  @param[in] DimmIdPreference Preference for the PMem module ID (handle or UID)
  @param[out] ppResult Pointer to structure that holds results of the tests

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NOT_STARTED Test was not executed
  @retval EFI_SUCCESS All Ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_START_DIAGNOSTIC) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CONST UINT8 DiagnosticTestId,
  IN     UINT8 DimmIdPreference,
  OUT DIAG_INFO **ppResultStr
  );

/**
  Create namespace
  Creates a AppDirect namespace on the provided region/PMem module.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] RegionId the ID of the region that the Namespace is supposed to be created.
  @param[in] Reserved
  @param[in] BlockSize the size of each of the block in the device.
    Valid block sizes are: 1 (for AppDirect Namespace), 512 (default), 514, 520, 528, 4096, 4112, 4160, 4224.
  @param[in] BlockCount the amount of block that this namespace should consist
  @param[in] pName - Namespace name.
  @param[in] Mode- boolean value to decide when the namespace
    should have the BTT arena included
               * 0 - Ignore
               * 1 - Yes
               * 2 - No
  @param[in] ForceAll Suppress all warnings
  @param[in] ForceAlignment Suppress alignment warnings
  @param[out] pActualNamespaceCapacity capacity needed to meet alignment requirements
  @param[out] pNamespaceId Pointer to the ID of the namespace that is created
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS if the operation was successful.
  @retval EFI_ALREADY_EXISTS if a namespace with the provided GUID already exists in the system.
  @retval EFI_DEVICE_ERROR if there was a problem with writing the configuration to the device.
  @retval EFI_OUT_OF_RESOURCES if there is not enough free space on the PMem module/region.
  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_CREATE_NAMESPACE) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 RegionId,
  IN     UINT16 Reserved,
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
  Delete Namespace

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] Force Force to perform deleting namespace configs on all affected PMem modules
  @param[in] NamespaceId the ID of the namespace to be removed.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS if the operation was successful.
  @retval EFI_NOT_FOUND if a namespace with the provided GUID does not exist in the system.
  @retval EFI_DEVICE_ERROR if there was a problem with writing the configuration to the device.
  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_DELETE_NAMESPACE) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     BOOLEAN Force,
  IN     UINT16 NamespaceId,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get Driver API Version

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pVersion output version

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_GET_DRIVER_API_VERSION) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT CHAR16 pVersion[FW_API_VERSION_LEN]
);

/**
  Get namespaces info

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pNamespaceListNode - pointer to namespace list node
  @param[out] pNamespacesCount - namespace count
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_NAMESPACES) (
    IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
    IN OUT LIST_ENTRY *pNamespaceListNode,
       OUT UINT32 *pNamespacesCount,
       OUT COMMAND_STATUS *pCommandStatus
);

/**
  Get Error log for given PMem module

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds - array of PMem module pids. Use all PMem modules if pDimms is NULL and DimmsCount is 0.
  @param[in] DimmsCount - number of PMem modules in array. Use all PMem modules if pDimms is NULL and DimmsCount is 0.
  @param[in] ThermalError - is thermal error (if not it is media error)
  @param[in] SequenceNumber - sequence number of error to fetch in queue
  @param[in] HighLevel - high level if true, low level otherwise
  @param[in, out] pCount - number of error entries in output array
  @param[out] pErrorLogs - output array of errors
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ERROR_LOG) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
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
  Get the debug log from a specified PMem module and fw debug log source

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID identifier of what PMem module to get log pages from
  @param[in] LogSource debug log source buffer to retrieve
  @param[in] Reserved for future use. Must be 0 for now.
  @param[out] ppDebugLogBuffer - an allocated buffer containing the raw debug log
  @param[out] pDebugLogBufferSize - the size of the raw debug log buffer
  @param[out] pCommandStatus structure containing detailed NVM error codes

  Note: The caller is responsible for freeing the returned buffer

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
  (EFIAPI *EFI_DCPMM_CONFIG_GET_FW_DEBUG_LOG) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
  IN     UINT8 LogSource,
  IN     UINT32 Reserved,
     OUT VOID **ppDebugLogBuffer,
     OUT UINTN *pDebugLogBufferSize,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set Optional Configuration Data Policy using FW command

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds - pointer to array of UINT16 PMem module ids to set
  @param[in] DimmIdsCount - number of elements in pDimmIds
  @param[in] Reserved
  @param[in] AveragePowerReportingTimeConstant - (FIS 2.1 and greater) AveragePowerReportingTimeConstant value to set
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_SET_OPTIONAL_DATA_POLICY) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 *Reserved,
  IN     UINT32 *pAveragePowerReportingTimeConstant,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get requested number of specific PMem module registers for given PMem module id

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmId - ID of a PMem module.
  @param[out] pBsr - Pointer to buffer for Boot Status register, contains
              high and low 4B register.
  @param[out] Reserved
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_RETRIEVE_DIMM_REGISTERS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmId,
     OUT UINT64 *pBsr,
     OUT UINT8 *Reserved,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get system topology from SMBIOS table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.

  @param[out] ppTopologyDimm Structure containing information about DDR entries from SMBIOS.
  @param[out] pTopologyDimmsNumber Number of DDR entries found in SMBIOS.

  @retval EFI_SUCCESS All ok.
  @retval EFI_DEVICE_ERROR Unable to find SMBIOS table in system configuration tables.
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_SYSTEM_TOPOLOGY) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT TOPOLOGY_DIMM_INFO **ppTopologyDimm,
     OUT UINT16 *pTopologyDimmsNumber
  );

/**
  Get the system-wide ARS status for the persistent memory capacity of the system.
  In this function, the system-wide ARS status is determined based on the ARS status
  values for the individual PMem modules.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.

  @param[out] pARSStatus pointer to the current system ARS status.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_ARS_STATUS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT8 *pARSStatus
  );

/**
  Get the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDriverPreferences pointer to the current driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DRIVER_PREFERENCES) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Set the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDriverPreferences pointer to the desired driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_SET_DRIVER_PREFERENCES) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Attempt to format a PMem module through a customer format command

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in]  pDimmIds Pointer to an array of PMem module IDs
  @param[in]  DimmIdsCount Number of items in array of PMem module IDs
  @param[in]  Recovery - Perform on non-functional PMem modules
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_OUT_OF_RESOURCES Memory Allocation failed
  @retval EFI_SUCCESS All Ok
**/

typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_DIMM_FORMAT) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     BOOLEAN Recovery,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Get DDRT IO init info

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve support data from
  @param[out] pDdrtTrainingStatus pointer to the PMem modules DDRT training status

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_DDRT_IO_INIT_INFO) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pDdrtTrainingStatus
  );

/**
  Get long operation status

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve status from
  @param[in] pOpcode pointer to opcode of long op command to check
  @param[in] pOpcode pointer to subopcode of long op command to check
  @param[out] pPercentComplete pointer to percentage current command has completed
  @param[out] pEstimatedTimeLeft pointer to time to completion BCD
  @param[out] pFwStatus pointer to completed mailbox status code

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_LONG_OP_STATUS) (
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
  @param[in] pDimmIds - pointer to array of UINT16 PMem module ids to get data for
  @param[in] DimmIdsCount - number of elements in pDimmIds

  @param[IN] ErrorInjType - Error Inject type
  @param[IN] ClearStatus - Is clear status set
  @param[IN] pInjectTemperatureValue - Pointer to inject temperature
  @param[IN] pInjectPoisonAddress - Pointer to inject poison address
  @param[IN] pPoisonType - Pointer to poison type
  @param[IN] pPercentRemaining - Pointer to percentage remaining
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

@retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
@retval EFI_INVALID_PARAMETER One or more parameters are invalid
@retval EFI_SUCCESS All ok
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_INJECT_ERROR) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT8  ErrorInjType,
  IN     UINT8  ClearStatus,
  IN     UINT64 *pInjectTemperatureValue,
  IN     UINT64 *pInjectPoisonAddress,
  IN     UINT8  *pPoisonType,
  IN     UINT8  *pPercentageremaining,
  OUT COMMAND_STATUS *pCommandStatus
 );

/**
  GetBsr value and return bsr or boot status bitmask depending on the requested options
  UEFI - Read directly from BSR register
  OS - Get BSR value from BIOS emulated command
  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID PMem module handle of the PMem module
  @param[out] pBsrValue pointer to BSR register value OPTIONAL
  @param[out] pBootStatusBitMask pointer to BootStatusBitmask OPTIONAL

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_NO_RESPONSE BSR value returned by FW is invalid
  @retval EFI_SUCCESS Success
  @retval Other errors failure of FW commands
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_BSR_AND_BITMASK) (
  IN      EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN      UINT16 DimmID,
  OUT     UINT64 *pBsrValue OPTIONAL,
  OUT     UINT16 *pBootStatusBitmask OPTIONAL
 );


/**
  Get Command Access Policy is used to retrieve a list of FW commands that may be restricted.
  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID Handle of the PMem module
  @param[in,out] pCount IN: Count is number of elements in the pCapInfo array. OUT: number of elements written to pCapInfo
  @param[out] pCapInfo Array of Command Access Policy Entries. If NULL, pCount will be updated with number of elements required. OPTIONAL

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_SUCCESS Success
  @retval Other errors failure of FW commands
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_COMMAND_ACCESS_POLICY) (
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  UINT16 DimmID,
  IN OUT UINT32 *pCount,
  IN OUT COMMAND_ACCESS_POLICY_ENTRY *pCapInfo OPTIONAL
);

/**
  Get Command Effect Log is used to retrieve a list PMem module FW commands and their effects on the PMem module subsystem.

  @param[in] pThis - A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID - Handle of the PMem module
  @param[in, out] pCelEntry - A pointer to the CEL entry table for a given PMem module
  @param[in, out] EntryCount - The number of CEL entries for a given table

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_COMMAND_EFFECT_LOG) (
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  UINT16 DimmID,
  IN OUT COMMAND_EFFECT_LOG_ENTRY **ppLogEntry,
  IN OUT UINT32 *pEntryCount
  );

/**
  Pass Through command to FW
  Sends a command to FW and waits for response from firmware

  @param[in,out] pCmd A firmware command structure
  @param[in] Timeout The timeout, in 100ns units, to use for the execution of the protocol command.
             A Timeout value of 0 means that this function will wait indefinitely for the protocol command to execute.
             If Timeout is greater than zero, then this function will return EFI_TIMEOUT if the time required to execute
             the receive data command is greater than Timeout.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
  @retval EFI_NOT_FOUND The driver could not find the encoded in pCmd PMem module in the system.
  @retval EFI_DEVICE_ERROR FW error received.
  @retval EFI_UNSUPPORTED if the command is ran not in the DEBUG version of the driver.
  @retval EFI_TIMEOUT A timeout occurred while waiting for the protocol command to execute.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_PASS_THRU) (
  IN OUT NVM_FW_CMD *pCmd,
  IN     UINT64 Timeout
);

/**
  Gets value of transport protocol and payload size settings from platform

  @param[in]     pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in,out] pAttribs A pointer to a variable used to store protocol and payload settings

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_GET_FIS_TRANSPORT_ATTRIBS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  OUT    EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS *pAttribs
  );

/**
  Sets value of transport protocol and payload size settings for platform

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in] Attribs The new value to assign to protocol and payload settings

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_CONFIG_SET_FIS_TRANSPORT_ATTRIBS) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS Attribs
  );

/**
  Sets the playback/recording mode

  @param[in] PbrMode: 0x0 - Normal, 0x1 - Recording, 0x2 - Playback

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_NOT_READY if PbrMode is set to Playback and session isn't loaded
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_SET_MODE) (
  IN     UINT32 PbrMode
);

/**
  Gets the current playback/recording mode

  @param[out] pPbrMode: 0x0 - Normal, 0x1 - Recording, 0x2 - Playback

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_NOT_READY if the pbr context is not available
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_MODE) (
  OUT     UINT32 *pPbrMode
);

/**
  Set the PBR Buffer to use

  @param[in] pBufferAddress: address of a buffer to use for playback or record mode
  @param[in] BufferSize: size in bytes of the buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_NOT_READY if the pbr context is not available
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_SET_SESSION) (
  IN     VOID *pBufferAddress,
  IN     UINT32 BufferSize
);

/**
  Get the PBR Buffer that is current being used

  @param[out] ppBufferAddress: address to the pbr buffer
  @param[out] pBufferSize: size in bytes of the buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_SESSION) (
  IN     VOID **ppBufferAddress,
  IN     UINT32 *pBufferSize
);

/**
  Clear the PBR Buffer that is current being used

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_FREE_SESSION) (
);

/**
  Reset all playback buffers to align with the specified TagId

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_RESET_SESSION) (
  IN     UINT32 TagId
);

/**
  Set a tag associated with the current recording buffer offset

  @param[in] Signature: signature associated with the tag
  @param[in] pName: name associated with the tag
  @param[in] pDescription: description of the tab
  @param[out] pId: logical ID given to the tag (will be appended to the name)

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_SET_TAG) (
  IN     UINT32 Signature,
  IN     CHAR16 *pName,
  IN     CHAR16 *pDescription,
  OUT    UINT32 *pId
);

/**
  Get the number of tags associated with the recording buffer

  @param[out] pCount: get the number of tags

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_TAG_COUNT) (
  OUT    UINT32 *pCount
);

/**
   Gets information pertaining to playback data associated with a specific
   data type (Signature).

   @param[in] Signature: Specifies data type interested in
   @param[out] pTotalDataItems: Number of logical data items of this Signature type that
      are available in the currently active playback session.
   @param[out] pTotalDataSize: Total size in bytes of all data associated with Signature type
   @param[out] pCurrentPlaybackDataOffset: Points to the next data item of Signature type
      that will be returned when PbrGetData is called with GET_NEXT_DATA_INDEX.
   @retval EFI_SUCCESS on success
 **/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_PB_INFO) (
  IN UINT32 Signature,
  OUT UINT32 *pTotalDataItems,
  OUT UINT32 *pTotalDataSize,
  OUT UINT32 *pCurrentPlaybackDataOffset
);

/**
   Adds data to the recording session

   @param[in] Signature: unique dword identifier that categorizes
      the data to be recorded
   @param[in] pData: Data to be recorded.  If NULL, a zeroed data buffer
      is allocated.  Useful, when used with ppData.
   @param[in] Size: Byte size of pData
   @param[in] Singleton: Only one data object associated with Signature.
      Data previously set will be overridden with this data object.
   @param[out] - ppData - May be NULL, otherwise will contain a pointer
      to the memory allocated in the recording buffer for this data object.
      Warning, this pointer is only guaranteed to be valid until the next
      call to this function.
   @retval EFI_SUCCESS on success
 **/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_SET_DATA) (
  IN UINT32 Signature,
  IN VOID *pData,
  IN UINT32 Size,
  IN BOOLEAN Singleton,
  OUT VOID **ppData,
  OUT UINT32 *pLogicalIndex
);

/**
   Gets data from the playback session

   @param[in] Signature: Specifies which data type to get
   @param[in] Index: GET_NEXT_DATA_INDEX gets the next data object within
      the playback session.  Otherwise, any positive value will result in
      getting the data object at position 'Index' (base 0).  If data associated
      with Signature is a Singleton, use Index '0'.
   @param[out] ppData: Newly allocated buffer that contains the data object.
      Caller is responsible for freeing it.
   @param[out] pSize: Size in bytes of ppData.
   @param[out] pLogicalIndex: May be NULL, otherwise will contain the
      logical index of the data object.
   @retval EFI_SUCCESS on success
 **/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_DATA) (
  IN UINT32 Signature,
  IN INT32 Index,
  OUT VOID **ppData,
  OUT UINT32 *pSize,
  OUT UINT32 *pLogicalIndex
);

/**
  Get tag info

  @param[in] pId: tag identification
  @param[out] pSignature: signature associated with the tag
  @param[out] pName: name associated with the tag
  @param[out] ppDescription: description of the tab
  @param[out] ppTagPartitionInfo: array of TagPartitionInfo structs
  @param[out] pTagPartitionCnt: number of items in pTagPartitionInfo

  All out pointers need to be freed by caller.

  @retval EFI_SUCCESS if the table was found and is properly returned.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_TAG) (
  IN     UINT32 Id,
  OUT    UINT32 *pSignature,
  OUT    CHAR16 **ppName,
  OUT    CHAR16 **ppDescription,
  OUT    VOID **ppTagPartitionInfo,
  OUT    UINT32 *pTagPartitionCnt
);

#ifndef OS_BUILD
/**
  Gets value of PcdDebugPrintErrorLevel for the pmem driver

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[out] ErrorLevel A pointer used to store the value of debug print error level

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid ErrorLevel Parameter.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_GET_DRIVER_DEBUG_PRINT_ERROR_LEVEL) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *ErrorLevel
  );

/**
  Sets value of PcdDebugPrintErrorLevel for the pmem driver

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in] ErrorLevel The new value to assign to debug print error level

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid ErrorLevel Parameter.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_PBR_SET_DRIVER_DEBUG_PRINT_ERROR_LEVEL) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 ErrorLevel
);
#endif //OS_BUILD

/**
  Get FIPS Mode retrieves the current FIPS Mode for the DimmID provided.

  @param[in] pThis - A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmID - Handle of the PMem module
  @param[out] pFIPSMode - A pointer to a FIPS_MODE struct to fill in
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
typedef
EFI_STATUS
(EFIAPI *EFI_DCPMM_GET_FIPS_MODE) (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT FIPS_MODE *pFIPSMode,
     OUT COMMAND_STATUS *pCommandStatus
);

/**
  Configuration and management of PMem modules Protocol Interface
**/
struct _EFI_DCPMM_CONFIG2_PROTOCOL {
  UINT32 Version;
  EFI_DCPMM_CONFIG_GET_DIMM_COUNT GetDimmCount;
  EFI_DCPMM_CONFIG_GET_DIMMS GetDimms;
  EFI_DCPMM_CONFIG_GET_DIMM GetDimm;
#ifdef OS_BUILD
  EFI_DCPMM_CONFIG_GET_PMON GetPMONRegisters;
  EFI_DCPMM_CONFIG_SET_PMON SetPMONRegisters;
#endif
  EFI_DCPMM_CONFIG_GET_UNINITIALIZED_DIMM_COUNT GetUninitializedDimmCount;
  EFI_DCPMM_CONFIG_GET_UNINITIALIZED_DIMMS GetUninitializedDimms;
  EFI_DCPMM_CONFIG_GET_SOCKETS GetSockets;
  EFI_DCPMM_CONFIG_GET_DIMM_SMBIOS_TABLE GetDimmSmbiosTable;
  EFI_DCPMM_CONFIG_GET_ACPI_NFIT GetAcpiNFit;
  EFI_DCPMM_CONFIG_GET_ACPI_PCAT GetAcpiPcat;
  EFI_DCPMM_CONFIG_GET_ACPI_PMTT GetAcpiPMTT;
  EFI_DCPMM_CONFIG_GET_PCD GetPcd;
  EFI_DCPMM_CONFIG_GET_SECURITY_STATE GetSecurityState;
  EFI_DCPMM_CONFIG_SET_SECURITY_STATE SetSecurityState;
  EFI_DCPMM_CONFIG_UPDATE_FW UpdateFw;
  EFI_DCPMM_CONFIG_GET_REGION_COUNT GetRegionCount;
  EFI_DCPMM_CONFIG_GET_REGIONS GetRegions;
  EFI_DCPMM_CONFIG_GET_REGION GetRegion;
  EFI_DCPMM_CONFIG_GET_MEMORY_RESOURCES_INFO GetMemoryResourcesInfo;
  EFI_DCPMM_CONFIG_GET_DIMMS_PERFORMANCE GetDimmsPerformanceData;
  EFI_DCPMM_CONFIG_GET_SYSTEM_CAPABILITIES_INFO GetSystemCapabilitiesInfo;
  EFI_DCPMM_GET_DRIVER_API_VERSION GetDriverApiVersion;
  EFI_DCPMM_CONFIG_GET_ALARM_THR GetAlarmThresholds;
  EFI_DCPMM_CONFIG_SET_ALARM_THR SetAlarmThresholds;
  EFI_DCPMM_CONFIG_GET_SMART_AND_HEALTH GetSmartAndHealth;
  EFI_DCPMM_CONFIG_GET_GOALS GetGoalConfigs;
  EFI_DCPMM_CONFIG_GET_ACTUAL_REGIONS_GOAL_CAPACITIES GetActualRegionsGoalCapacities;
  EFI_DCPMM_CONFIG_CREATE_GOAL CreateGoalConfig;
  EFI_DCPMM_CONFIG_DELETE_GOAL DeleteGoalConfig;
  EFI_DCPMM_CONFIG_DUMP_GOAL DumpGoalConfig;
  EFI_DCPMM_CONFIG_LOAD_GOAL LoadGoalConfig;
  EFI_DCPMM_CONFIG_START_DIAGNOSTIC StartDiagnostic;
  EFI_DCPMM_CONFIG_CREATE_NAMESPACE CreateNamespace;
  EFI_DCPMM_CONFIG_GET_NAMESPACES GetNamespaces;
  EFI_DCPMM_CONFIG_DELETE_NAMESPACE DeleteNamespace;
  EFI_DCPMM_CONFIG_GET_ERROR_LOG GetErrorLog;
  EFI_DCPMM_CONFIG_GET_FW_DEBUG_LOG GetFwDebugLog;
  EFI_DCPMM_CONFIG_SET_OPTIONAL_DATA_POLICY SetOptionalConfigurationDataPolicy;
  EFI_DCPMM_CONFIG_RETRIEVE_DIMM_REGISTERS RetrieveDimmRegisters;
  EFI_DCPMM_CONFIG_GET_SYSTEM_TOPOLOGY GetSystemTopology;
  EFI_DCPMM_CONFIG_GET_ARS_STATUS GetARSStatus;
  EFI_DCPMM_CONFIG_GET_DRIVER_PREFERENCES GetDriverPreferences;
  EFI_DCPMM_CONFIG_SET_DRIVER_PREFERENCES SetDriverPreferences;
  EFI_DCPMM_CONFIG_DIMM_FORMAT DimmFormat;
  EFI_DCPMM_CONFIG_GET_DDRT_IO_INIT_INFO GetDdrtIoInitInfo;
  EFI_DCPMM_CONFIG_GET_LONG_OP_STATUS GetLongOpStatus;
  EFI_DCPMM_CONFIG_INJECT_ERROR InjectError;
  EFI_DCPMM_CONFIG_GET_BSR_AND_BITMASK GetBSRAndBootStatusBitMask;
  EFI_DCPMM_CONFIG_PASS_THRU PassThru;
  EFI_DCPMM_CONFIG_DELETE_PCD_CONFIG ModifyPcdConfig;
  EFI_DCPMM_CONFIG_GET_FIS_TRANSPORT_ATTRIBS GetFisTransportAttributes;
  EFI_DCPMM_CONFIG_SET_FIS_TRANSPORT_ATTRIBS SetFisTransportAttributes;
  EFI_DCPMM_CONFIG_GET_COMMAND_ACCESS_POLICY GetCommandAccessPolicy;
  EFI_DCPMM_CONFIG_GET_COMMAND_EFFECT_LOG GetCommandEffectLog;
#ifndef OS_BUILD
  EFI_DCPMM_PBR_GET_DRIVER_DEBUG_PRINT_ERROR_LEVEL GetDriverDebugPrintErrorLevel;
  EFI_DCPMM_PBR_SET_DRIVER_DEBUG_PRINT_ERROR_LEVEL SetDriverDebugPrintErrorLevel;
#endif //OS_BUILD
  EFI_DCPMM_GET_FIPS_MODE GetFIPSMode;

};

/**
  Playback and Record APIs
**/
struct _EFI_DCPMM_PBR_PROTOCOL {
  EFI_DCPMM_PBR_SET_MODE PbrSetMode;
  EFI_DCPMM_PBR_GET_MODE PbrGetMode;
  EFI_DCPMM_PBR_SET_SESSION PbrSetSession;
  EFI_DCPMM_PBR_GET_SESSION PbrGetSession;
  EFI_DCPMM_PBR_FREE_SESSION PbrFreeSession;
  EFI_DCPMM_PBR_RESET_SESSION PbrResetSession;
  EFI_DCPMM_PBR_SET_TAG PbrSetTag;
  EFI_DCPMM_PBR_GET_TAG_COUNT PbrGetTagCount;
  EFI_DCPMM_PBR_GET_TAG PbrGetTag;
  EFI_DCPMM_PBR_GET_PB_INFO PbrGetDataPlaybackInfo;
  EFI_DCPMM_PBR_GET_DATA PbrGetData;
  EFI_DCPMM_PBR_SET_DATA PbrSetData;
};
#endif /** _NVM_INTERFACE_H_ **/
