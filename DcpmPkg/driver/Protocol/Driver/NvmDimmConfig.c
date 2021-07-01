/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Guid/Acpi.h>
#include "NvmDimmConfig.h"
#ifndef OS_BUILD
#include <Dcpmm.h>
#endif
#include "NvmTypes.h"
#include <AcpiParsing.h>
#include <Dimm.h>
#include <Region.h>
#include <Namespace.h>
#include <NvmDimmPassThru.h>
#include <NvmDimmDriver.h>
#include <FwUtility.h>
#include <SmbiosUtility.h>
#include <PlatformConfigData.h>
#include "DriverHealth.h"
#include <DumpLoadRegions.h>
#include <NvmInterface.h>
#include "NvmSecurity.h"
#include <NvmWorkarounds.h>
#include <NvmTables.h>
#include <Convert.h>
#include <ShowAcpi.h>
#include <CoreDiagnostics.h>
#include <NvmHealth.h>
#include <Utility.h>
#include <PbrDcpmm.h>
#ifndef OS_BUILD
#include <SpiRecovery.h>
#include <FConfig.h>
#include <Spi.h>
#include <Smbus.h>
#endif

#ifdef OS_BUILD
#include <os_efi_api.h>
#include <os_efi_preferences.h>
#endif // OS_BUILD

#include <FwVersion.h>

#define INVALID_SOCKET_ID 0xFFFF
#define ARS_LIST_NOT_INITIALIZED -1
#define PMTT_INFO_SIGNATURE     SIGNATURE_64('P', 'M', 'T', 'T', 'I', 'N', 'F', 'O')
#define PMTT_INFO_FROM_NODE(a)  CR(a, PMTT_INFO, PmttNode, PMTT_INFO_SIGNATURE)
#define IS_DIMM_SECURITY_ENABLED(SecurityStateBitmask) (SecurityStateBitmask & SECURITY_MASK_ENABLED)

 /** PMTT 0.2 vendor specific modules: Die, Channel, Slot added **/
 typedef struct _PMTT_VERSION {
   ACPI_REVISION  Revision;          //!< PMTT Version
   struct {
     UINT16 DieID;            //!< die identifier
     UINT16 ChannelID;        //!< Channel identifier
     UINT16 SlotID;           //!< Slot identifier
   } VendorData;
 } PMTT_VERSION;

typedef struct _PMTT_INFO {
  LIST_ENTRY PmttNode;
  UINT64 Signature;            //!< PMTT_INFO_SIGNATURE
  PMTT_VERSION PmttVersion;    //!< PMTT Version
  UINT16 DimmID;               //!< SMBIOS Type 17 handle corresponding to this memory device
  UINT16 NodeControllerID;     //!< die identifier
  UINT16 SocketID;             //!< zero based socket identifier
  UINT16 MemControllerID;      //!< Memory Controller identifier
} PMTT_INFO;

typedef struct _REGION_GOAL_APPDIRECT_INDEX_TABLE {
  REGION_GOAL *pRegionGoal;
  UINT32 AppDirectIndex;
} REGION_GOAL_APPDIRECT_INDEX_TABLE;

/** Memory Device SMBIOS Table **/
#define SMBIOS_TYPE_MEM_DEV             17
/** Memory Device Mapped Address SMBIOS Table **/
#define SMBIOS_TYPE_MEM_DEV_MAPPED_ADDR 20

#define TEST_NAMESPACE_NAME_LEN         14

EFI_GUID gNvmDimmConfigProtocolGuid = EFI_DCPMM_CONFIG2_PROTOCOL_GUID;
EFI_GUID gNvmDimmPbrProtocolGuid = EFI_DCPMM_PBR_PROTOCOL_GUID;

extern NVMDIMMDRIVER_DATA *gNvmDimmData;
extern CONST UINT64 gSupportedBlockSizes[SUPPORTED_BLOCK_SIZES_COUNT];
// These will be overwritten by SetDefaultProtocolAndPayloadSizeOptions()
EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS gTransportAttribs = { FisTransportAuto, FisTransportSizeAuto };

#ifndef OS_BUILD
DCPMM_ARS_ERROR_RECORD * gArsBadRecords = NULL;
INT32 gArsBadRecordsCount = ARS_LIST_NOT_INITIALIZED;
extern volatile   UINT32  _gPcd_BinaryPatch_PcdDebugPrintErrorLevel;
#endif

/**
  Instance of NvmDimmConfigProtocol
**/
EFI_DCPMM_CONFIG2_PROTOCOL gNvmDimmDriverNvmDimmConfig =
{
  NVMD_CONFIG_PROTOCOL_VERSION,
  GetDimmCount,
  GetDimms,
  GetDimm,
#ifdef OS_BUILD
  GetPMONRegisters,
  SetPMONRegisters,
#endif
  GetUninitializedDimmCount,
  GetUninitializedDimms,
  GetSockets,
  GetDimmSmbiosTable,
  GetAcpiNFit,
  GetAcpiPcat,
  GetAcpiPMTT,
  GetPcd,
  GetSecurityState,
  SetSecurityState,
  UpdateFw,
  GetRegionCount,
  GetRegions,
  GetRegion,
  GetMemoryResourcesInfo,
  GetDimmsPerformanceData,
  GetSystemCapabilitiesInfo,
  GetDriverApiVersion,
  GetAlarmThresholds,
  SetAlarmThresholds,
  GetSmartAndHealth,
  GetGoalConfigs,
  GetActualRegionsGoalCapacities,
  CreateGoalConfig,
  DeleteGoalConfig,
  DumpGoalConfig,
  LoadGoalConfig,
  StartDiagnostic,
  CreateNamespace,
  GetNamespaces,
  DeleteNamespace,
  GetErrorLog,
  GetFwDebugLog,
  SetOptionalConfigurationDataPolicy,
  RetrieveDimmRegisters,
  GetSystemTopology,
  GetARSStatus,
  GetDriverPreferences,
  SetDriverPreferences,
  DimmFormat,
  GetDdrtIoInitInfo,
  GetLongOpStatus,
  InjectError,
  GetBSRAndBootStatusBitMask,
  PassThruCommand,
  ModifyPcdConfig,
  GetFisTransportAttributes,
  SetFisTransportAttributes,
  GetCommandAccessPolicy,
  GetCommandEffectLog,
#ifndef OS_BUILD
  GetDriverDebugPrintErrorLevel,
  SetDriverDebugPrintErrorLevel,
#endif //OS_BUILD
};


EFI_DCPMM_PBR_PROTOCOL gNvmDimmDriverNvmDimmPbr =
{
  PbrSetMode,
  PbrGetMode,
  PbrSetSession,
  PbrGetSession,
  PbrFreeSession,
  PbrResetSession,
  PbrSetTag,
  PbrGetTagCount,
  PbrGetTag,
  PbrGetDataPlaybackInfo,
  PbrGetData,
  PbrSetData,
};

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  UINTN VariableSize = 0;
  NVDIMM_ENTRY();

  /* check input parameters */
  if (pDriverPreferences == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ZeroMem(pDriverPreferences, sizeof(*pDriverPreferences));

  VariableSize = sizeof(pDriverPreferences->ChannelInterleaving);
  ReturnCode = GET_VARIABLE(
    CHANNEL_INTERLEAVE_SIZE_VARIABLE_NAME,
    gNvmDimmNgnvmVariableGuid,
    &VariableSize,
    &pDriverPreferences->ChannelInterleaving);

  if(ReturnCode == EFI_NOT_FOUND) {
    pDriverPreferences->ChannelInterleaving = DEFAULT_CHANNEL_INTERLEAVE_SIZE;
    ReturnCode = EFI_SUCCESS;
  } else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve Channel Interleave Variable");
    goto Finish;
  }

  VariableSize = sizeof(pDriverPreferences->ImcInterleaving);
  ReturnCode = GET_VARIABLE(
    IMC_INTERLEAVE_SIZE_VARIABLE_NAME,
    gNvmDimmNgnvmVariableGuid,
    &VariableSize,
    &pDriverPreferences->ImcInterleaving);

  if(ReturnCode == EFI_NOT_FOUND) {
    pDriverPreferences->ImcInterleaving = DEFAULT_IMC_INTERLEAVE_SIZE;
    ReturnCode = EFI_SUCCESS;
  } else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve iMC Interleave Variable");
    goto Finish;
  }

  if ((pDriverPreferences->ImcInterleaving == DEFAULT_IMC_INTERLEAVE_SIZE) !=
      (pDriverPreferences->ChannelInterleaving == DEFAULT_CHANNEL_INTERLEAVE_SIZE)) {
    NVDIMM_DBG("Only one interleave preference set to default, invalid configuration");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

    pDriverPreferences->AppDirectGranularity = APPDIRECT_GRANULARITY_DEFAULT;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get information about support of SKU features

  @param[in] pDimm - pointer to DIMM with SKU informations in bit field form received from FW
  @param[in] SkuType - feature type

  @retval TRUE - if feature is supported by current SKU
  @retval FALSE - if feature is not supported by current SKU
**/
STATIC
BOOLEAN
IsDimmSkuSupported(
  IN     DIMM *pDimm,
  IN     UINT8 SkuType
  )
{
  BOOLEAN ReturnValue = FALSE;

  if (pDimm == NULL) {
    NVDIMM_DBG("NULL pointer provided.");
    goto Finish;
  }

  switch (SkuType) {
  case SkuMemoryModeOnly:
    if (pDimm->SkuInformation.MemoryModeEnabled == MODE_ENABLED &&
        pDimm->SkuInformation.AppDirectModeEnabled == MODE_DISABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuAppDirectModeOnly:
    if (pDimm->SkuInformation.MemoryModeEnabled == MODE_DISABLED &&
        pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuTriMode:
    if (pDimm->SkuInformation.MemoryModeEnabled == MODE_ENABLED &&
        pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuPackageSparingCapable:
    if (pDimm->SkuInformation.PackageSparingCapable == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuSoftProgrammableSku:
    if (pDimm->SkuInformation.SoftProgrammableSku == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuStandardSecuritySku:
    if (pDimm->SkuInformation.EncryptionEnabled == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuControlledCountrySku:
    if (pDimm->SkuInformation.EncryptionEnabled == MODE_DISABLED) {
      ReturnValue = TRUE;
    }
    break;
  }

Finish:
  return ReturnValue;

}

/**
  Retrieve the number of functional DCPMMs in the system found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDimmCount The number of DCPMMs found in NFIT.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
EFIAPI
GetDimmCount(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pCurrentDimm = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDimmCount == NULL) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  *pDimmCount = 0;
  LIST_FOR_EACH(pCurrentDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (TRUE == pCurrentDimm->NonFunctional) {
      continue;
    }
    (*pDimmCount)++;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve the number of non-functional DCPMMs in the system

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDimmCount The number of DCPMMs.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
EFIAPI
GetUninitializedDimmCount(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pDimmCount
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pCurrentDimm = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  NVDIMM_ENTRY();

  if (pThis == NULL || pDimmCount == NULL) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  *pDimmCount = 0;
  LIST_FOR_EACH(pCurrentDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (FALSE == pCurrentDimm->NonFunctional) {
      continue;
    }
    (*pDimmCount)++;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#ifdef OS_BUILD
/**
Read the DIMM's PCD and get the mapped memory sizes

@param[in]  DimmPid The ID of the DIMM

@retval EFI_INVALID_PARAMETER passed NULL argument
@retval Other errors failure of FW commands
@retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetDimmMappedMemSize(
  IN DIMM *pDimm)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  NVDIMM_CURRENT_CONFIG *pPcdCurrentConf = NULL;

  if (NULL == pDimm) {
    return EFI_INVALID_PARAMETER;
  }

  if (!IsDimmManageable(pDimm)) {
    return EFI_NOT_READY;
  }

  // DIMM PCD already read
  if (pDimm->PcdMappedMemInfoRead) {
    NVDIMM_DBG("DIMM: 0x%04x PCD already read!", pDimm->DeviceHandle.AsUint32);
    return EFI_SUCCESS;
  }

  ReturnCode = GetPlatformConfigDataOemPartition(pDimm, FALSE, &pPcdConfHeader);
  if (EFI_ERROR(ReturnCode)) {
    return EFI_DEVICE_ERROR;
  }

  if (pPcdConfHeader->CurrentConfStartOffset == 0 || pPcdConfHeader->CurrentConfDataSize == 0) {
    NVDIMM_DBG("There is no Current Config table");
    FreePool(pPcdConfHeader);
    return EFI_LOAD_ERROR;
  }

  pPcdCurrentConf = GET_NVDIMM_CURRENT_CONFIG(pPcdConfHeader);

  if (!IsPcdCurrentConfHeaderValid(pPcdCurrentConf, pDimm->PcdOemPartitionSize)) {
    FreePool(pPcdConfHeader);
    return EFI_VOLUME_CORRUPTED;
  }

  pDimm->ConfigStatus = (UINT8)pPcdCurrentConf->ConfigStatus;
  pDimm->IsNew = (pDimm->ConfigStatus == DIMM_CONFIG_NEW_DIMM) ? 1 : 0;

  switch (pPcdCurrentConf->ConfigStatus) {
    case DIMM_CONFIG_SUCCESS:
    case DIMM_CONFIG_OLD_CONFIG_USED:
    // 2LM is not mapped because of NM:FM violation, but 1LM is mapped/healthy
    case DIMM_CONFIG_DCPMM_NM_FM_RATIO_UNSUPPORTED:
    case DIMM_CONFIG_PM_MAPPED_VM_POPULATION_ISSUE:
      pDimm->Configured = TRUE;
      break;
    default:
      pDimm->Configured = FALSE;
      break;
  }

  pDimm->MappedVolatileCapacity = pPcdCurrentConf->VolatileMemSizeIntoSpa;
  pDimm->MappedPersistentCapacity = pPcdCurrentConf->PersistentMemSizeIntoSpa;

  pDimm->PcdMappedMemInfoRead = TRUE;

  FreePool(pPcdConfHeader);
  return EFI_SUCCESS;
}

#endif // OS_BUILD

/*
 * Helper function for initializing information from the NFIT for non-functional
 * dimms only. This should eventually include functional dimms as well
 * (GetDimmInfo), but currently avoiding as it is hard to extract the NFIT-only
 * calls from GetDimmInfo.
 */
VOID
InitializeNfitDimmInfoFieldsFromDimm(
  IN     DIMM *pDimm,
     OUT DIMM_INFO *pDimmInfo
  )
{
  //pDimm->Signature = DIMM_SIGNATURE;
  pDimmInfo->Configured = pDimm->Configured;
  //pDimm->ISsNum = 0;
  pDimmInfo->DimmID = pDimm->DimmID;
  pDimmInfo->DimmHandle = pDimm->DeviceHandle.AsUint32;
  pDimmInfo->SocketId = pDimm->SocketId;
  pDimmInfo->ImcId = pDimm->ImcId;
  pDimmInfo->NodeControllerID = pDimm->NodeControllerID;
  pDimmInfo->ChannelId = pDimm->ChannelId;
  pDimmInfo->ChannelPos = pDimm->ChannelPos;
  //pDimm->NvDimmStateFlags
  pDimmInfo->VendorId = pDimm->VendorId;
  pDimmInfo->DeviceId = pDimm->DeviceId;
  pDimmInfo->Rid = pDimm->Rid;
  pDimmInfo->SubsystemVendorId = pDimm->SubsystemVendorId;
  pDimmInfo->SubsystemDeviceId = pDimm->SubsystemDeviceId;
  pDimmInfo->SubsystemRid = pDimm->SubsystemRid;
  pDimmInfo->ManufacturingInfoValid = pDimm->ManufacturingInfoValid;
  pDimmInfo->ManufacturingLocation = pDimm->ManufacturingLocation;
  pDimmInfo->ManufacturingDate = pDimm->ManufacturingDate;
  pDimmInfo->SerialNumber = pDimm->SerialNumber;
  pDimmInfo->Capacity = pDimm->RawCapacity;
  pDimmInfo->ManufacturerId = pDimm->Manufacturer;

  pDimmInfo->SmbusAddress = pDimm->SmbusAddress;

  CHECK_RESULT_CONTINUE(GetDimmUid(pDimm, pDimmInfo->DimmUid, MAX_DIMM_UID_LENGTH));
  if (StrLen(pDimmInfo->DimmUid) == 0) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_UID;
  }
}

/*
 * Helper function for initializing information from the SMBIOS
 */
EFI_STATUS
FillSmbiosInfo(
  IN OUT DIMM_INFO *pDimmInfo
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  SMBIOS_STRUCTURE_POINTER DmiPhysicalDev;
  SMBIOS_STRUCTURE_POINTER DmiDeviceMappedAddr;
  SMBIOS_VERSION SmbiosVersion;
  UINT64 CapacityFromSmbios = 0;

  ReturnCode = GetDmiMemdevInfo(pDimmInfo->DimmID, &DmiPhysicalDev, &DmiDeviceMappedAddr, &SmbiosVersion);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failure to retrieve SMBIOS tables");
    return ReturnCode;
  }

  /* SMBIOS type 17 table info */
  if (DmiPhysicalDev.Type17 != NULL) {
    if (DmiPhysicalDev.Type17->MemoryType == MemoryTypeDdr4) {
      //Prior to SMBIOS MemoryType 0x1F (Logical non-volatile), DCPM's were identified by
      //MemoryType 0x1A (DDR4) with TypeDetail[Nonvolatile] set.
      //Leaving here for backwards compatibility
      if (DmiPhysicalDev.Type17->TypeDetail.Nonvolatile) {
        pDimmInfo->MemoryType = MEMORYTYPE_DCPM;
      }
      else {
        pDimmInfo->MemoryType = MEMORYTYPE_DDR4;
      }
    }
    else if (DmiPhysicalDev.Type17->MemoryType == MemoryTypeDdr5) {
      pDimmInfo->MemoryType = MEMORYTYPE_DDR5;
    }
    else if (DmiPhysicalDev.Type17->MemoryType == MemoryTypeLogicalNonVolatileDevice) {
      pDimmInfo->MemoryType = MEMORYTYPE_DCPM;
    }
    else {
      pDimmInfo->MemoryType = MEMORYTYPE_UNKNOWN;
    }

    pDimmInfo->FormFactor = DmiPhysicalDev.Type17->FormFactor;
    pDimmInfo->DataWidth = DmiPhysicalDev.Type17->DataWidth;
    pDimmInfo->TotalWidth = DmiPhysicalDev.Type17->TotalWidth;
    pDimmInfo->Speed = DmiPhysicalDev.Type17->Speed;

    ReturnCode = GetSmbiosCapacity(DmiPhysicalDev.Type17->Size, DmiPhysicalDev.Type17->ExtendedSize,
      SmbiosVersion, &CapacityFromSmbios);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve capacity from SMBIOS table (" FORMAT_EFI_STATUS ")", ReturnCode);
    }

    pDimmInfo->CapacityFromSmbios = CapacityFromSmbios;

    TempReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &(DmiPhysicalDev.Type17),
      DmiPhysicalDev.Type17->DeviceLocator,
      pDimmInfo->DeviceLocator, sizeof(pDimmInfo->DeviceLocator));
    if (EFI_ERROR(TempReturnCode)) {
      StrnCpyS(pDimmInfo->DeviceLocator, DEVICE_LOCATOR_LEN, SMBIOS_STR_UNKNOWN, StrLen(SMBIOS_STR_UNKNOWN));
      NVDIMM_WARN("Failed to retrieve the device locator from SMBIOS table (" FORMAT_EFI_STATUS ")", ReturnCode);
    }
    TempReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &(DmiPhysicalDev.Type17),
      DmiPhysicalDev.Type17->BankLocator,
      pDimmInfo->BankLabel, sizeof(pDimmInfo->BankLabel));
    if (EFI_ERROR(TempReturnCode)) {
      StrnCpyS(pDimmInfo->BankLabel, BANKLABEL_LEN, SMBIOS_STR_UNKNOWN, StrLen(SMBIOS_STR_UNKNOWN));
      NVDIMM_WARN("Failed to retrieve the bank locator from SMBIOS table (" FORMAT_EFI_STATUS ")", ReturnCode);
    }
    TempReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &(DmiPhysicalDev.Type17),
      DmiPhysicalDev.Type17->Manufacturer,
      pDimmInfo->ManufacturerStr, sizeof(pDimmInfo->ManufacturerStr));
    if (EFI_ERROR(TempReturnCode)) {
      StrnCpyS(pDimmInfo->ManufacturerStr, MANUFACTURER_LEN, SMBIOS_STR_UNKNOWN, StrLen(SMBIOS_STR_UNKNOWN));
      NVDIMM_WARN("Failed to retrieve the manufacturer string from SMBIOS table (" FORMAT_EFI_STATUS ")", ReturnCode);
    }
  }
  else {
    NVDIMM_ERR("SMBIOS table of type 17 for DIMM 0x%x was not found.", pDimmInfo->DimmID);
  }
  return ReturnCode;
}

/**
  Init DIMM_INFO structure for given Initialized DIMM

  @param[in] pDimm DIMM that will be used to create DIMM_INFO
  @param[in] dimmInfoCategories DIMM_INFO_CATEGORIES specifies which (if any)
  additional FW api calls is desired. If DIMM_INFO_CATEGORY_NONE, then only
  the properties from the pDimm struct will be populated.
  @param[in,out] pDimmInfo DIMM_INFO instance to fill in

  @retval EFI_SUCCESS Creation performed without errors
  @retval EFI_INVALID_PARAMETER If pDimm or pDimmMinInfo is NULL
  @retval EFI_DEVICE_ERROR If communication with DIMM fails
**/
EFI_STATUS
GetDimmInfo (
  IN     DIMM *pDimm,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
  IN OUT DIMM_INFO *pDimmInfo
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PT_GET_SECURITY_PAYLOAD *pSecurityPayload = NULL;
  PT_OUTPUT_PAYLOAD_GET_SECURITY_OPT_IN *pSecurityOptInPayload = NULL;
  PT_PAYLOAD_GET_PACKAGE_SPARING_POLICY *pGetPackageSparingPayload = NULL;
  LIST_ENTRY *pNodeNamespace = NULL;
  NAMESPACE *pCurNamespace = NULL;
  SMART_AND_HEALTH_INFO HealthInfo;
  PT_OPTIONAL_DATA_POLICY_PAYLOAD OptionalDataPolicyPayload;
  PT_VIRAL_POLICY_PAYLOAD ViralPolicyPayload;
  PT_POWER_MANAGEMENT_POLICY_OUT *pPowerManagementPolicyPayload = NULL;
  PT_DEVICE_CHARACTERISTICS_OUT *pDevCharacteristics = NULL;
  PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE3 *pPayloadMemInfoPage3 = NULL;
  PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE4 *pPayloadMemInfoPage4 = NULL;
  PT_PAYLOAD_FW_IMAGE_INFO *pPayloadFwImage = NULL;
  PT_OUTPUT_PAYLOAD_GET_EADR PayloadExtendedAdr;
  PT_OUTPUT_PAYLOAD_GET_LATCH_SYSTEM_SHUTDOWN_STATE PayloadLatchSystemShutdownState;
  SMBIOS_STRUCTURE_POINTER DmiPhysicalDev;
  SMBIOS_STRUCTURE_POINTER DmiDeviceMappedAddr;
  SMBIOS_VERSION SmbiosVersion;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  ZeroMem(&HealthInfo, sizeof(HealthInfo));
  ZeroMem(&OptionalDataPolicyPayload, sizeof(OptionalDataPolicyPayload));
  ZeroMem(&ViralPolicyPayload, sizeof(ViralPolicyPayload));
  ZeroMem(&DmiPhysicalDev, sizeof(DmiPhysicalDev));
  ZeroMem(&DmiDeviceMappedAddr, sizeof(DmiDeviceMappedAddr));
  ZeroMem(&SmbiosVersion, sizeof(SmbiosVersion));

  if (pDimm == NULL || pDimmInfo == NULL) {
    NVDIMM_DBG("Convert operation failed. Invalid pointer");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

#ifdef OS_BUILD
  GetDimmMappedMemSize(pDimm);
#endif // OS_BUILD

  pDimmInfo->DimmID = pDimm->DimmID;
  pDimmInfo->SocketId = pDimm->SocketId;
  pDimmInfo->ChannelId = pDimm->ChannelId;
  pDimmInfo->ChannelPos = pDimm->ChannelPos;
  for (Index = 0; Index < pDimm->FmtInterfaceCodeNum; Index++) {
    pDimmInfo->InterfaceFormatCode[Index] = pDimm->FmtInterfaceCode[Index];
  }
  pDimmInfo->InterfaceFormatCodeNum = pDimm->FmtInterfaceCodeNum;

  pDimmInfo->DimmHandle = pDimm->DeviceHandle.AsUint32;
  pDimmInfo->FwVer = pDimm->FwVer;
  pDimmInfo->FwActiveApiVersionMajor = pDimm->FwActiveApiVersionMajor;
  pDimmInfo->FwActiveApiVersionMinor = pDimm->FwActiveApiVersionMinor;

  /* Package Sparing Capable */
  if (pDimm->SkuInformation.PackageSparingCapable) {
    pDimmInfo->PackageSparingCapable = TRUE;
  } else {
    pDimmInfo->PackageSparingCapable = FALSE;
  }

  /* Memory Modes Supported */
  pDimmInfo->ModesSupported = 0;
  if (pDimm->SkuInformation.MemoryModeEnabled == MODE_ENABLED) {
    pDimmInfo->ModesSupported |= BIT0;
  }

  if (pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
    pDimmInfo->ModesSupported |= BIT2;
  }

  /* Security Capabilities */
  if (pDimm->SkuInformation.EncryptionEnabled == MODE_ENABLED) {
    pDimmInfo->SecurityCapabilities |= (SECURITY_CAPABILITIES_ERASE_CAPABLE | SECURITY_CAPABILITIES_ENCRYPTION_SUPPORTED);
  }

  pDimmInfo->VendorId = pDimm->VendorId;
  pDimmInfo->DeviceId = pDimm->DeviceId;

  pDimmInfo->Rid = pDimm->Rid;
  pDimmInfo->ImcId = pDimm->ImcId;
  pDimmInfo->NodeControllerID = pDimm->NodeControllerID;

  pDimmInfo->SubsystemVendorId = pDimm->SubsystemVendorId;
  pDimmInfo->SubsystemDeviceId = pDimm->SubsystemDeviceId;
  pDimmInfo->SubsystemRid = pDimm->SubsystemRid;
  pDimmInfo->ControllerRid = pDimm->ControllerRid;

  pDimmInfo->ManufacturingInfoValid = pDimm->ManufacturingInfoValid;
  pDimmInfo->ManufacturingLocation = pDimm->ManufacturingLocation;
  pDimmInfo->ManufacturingDate = pDimm->ManufacturingDate;

  pDimmInfo->ManufacturerId = pDimm->Manufacturer;
  pDimmInfo->SerialNumber = pDimm->SerialNumber;

  ReturnCode = FillSmbiosInfo(pDimmInfo);
  CHECK_RESULT_CONTINUE(GetDimmUid(pDimm, pDimmInfo->DimmUid, MAX_DIMM_UID_LENGTH));
  if (StrLen(pDimmInfo->DimmUid) == 0) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_UID;
  }

  ReturnCode = GetDimmUid(pDimm, pDimmInfo->DimmUid, MAX_DIMM_UID_LENGTH);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  if (StrLen(pDimmInfo->DimmUid) == 0) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_UID;
  }

  // Set defaults first
  pDimmInfo->ManageabilityState = MANAGEMENT_VALID_CONFIG;
  pDimmInfo->HealthState = HEALTH_UNKNOWN;
  // Then change as needed.
  if (!IsDimmManageable(pDimm)) {
    pDimmInfo->ManageabilityState = MANAGEMENT_INVALID_CONFIG;
    pDimmInfo->HealthState = HEALTH_UNMANAGEABLE;
  } else if (TRUE == pDimm->NonFunctional) {
    pDimmInfo->HealthState = HEALTH_NON_FUNCTIONAL;
  }

  pDimmInfo->IsNew = pDimm->IsNew;
  pDimmInfo->RebootNeeded = pDimm->RebootNeeded;
  pDimmInfo->PmCapacity = pDimm->PmCapacity;

  /* Configuration Status */
  switch (pDimm->ConfigStatus) {
    case DIMM_CONFIG_SUCCESS:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_VALID;
      break;
    case DIMM_CONFIG_OLD_CONFIG_USED:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_REVERTED;
      break;
    case DIMM_CONFIG_REVISION_NOT_SUPPORTED:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_UNSUPPORTED;
      break;
    case DIMM_CONFIG_UNDEFINED:
    case DIMM_CONFIG_RESERVED:
    case DIMM_CONFIG_NEW_DIMM:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_NOT_CONFIG;
      break;
    case DIMM_CONFIG_IS_INCOMPLETE:
    case DIMM_CONFIG_NO_MATCHING_IS:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_BROKEN_INTERLEAVE;
      break;
    case DIMM_CONFIG_DCPMM_POPULATION_ISSUE:
      if ((pDimm->ISsNum > 0) && (pDimm->NvDimmStateFlags & NVDIMM_STATE_FLAGS_NOT_MAPPED)) {
        pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_BROKEN_INTERLEAVE;
      } else {
        pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_UNSUPPORTED;
      }
      break;
    // 2LM is not mapped because of NM:FM violation, but 1LM is mapped/healthy
    case DIMM_CONFIG_DCPMM_NM_FM_RATIO_UNSUPPORTED:
    case DIMM_CONFIG_PM_MAPPED_VM_POPULATION_ISSUE:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_PARTIALLY_SUPPORTED;
      break;
    case DIMM_CONFIG_BAD_CONFIG:
    case DIMM_CONFIG_IN_CHECKSUM_NOT_VALID:
    case DIMM_CONFIG_CURR_CHECKSUM_NOT_VALID:
    case DIMM_CONFIG_PM_NOT_MAPPED:
    case DIMM_CONFIG_CPU_MAX_MEMORY_LIMIT_VIOLATION:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_BAD_CONFIG;
      break;
    default:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_NOT_CONFIG;
      break;
  }

  pDimmInfo->IsInPopulationViolation = IsDimmInPopulationViolation(pDimm);
  pDimmInfo->SkuInformation = *((UINT32 *) &pDimm->SkuInformation);

  /* SKU Violation */
  pDimmInfo->SKUViolation = FALSE;
  if (pDimm->MappedVolatileCapacity > 0 && pDimm->SkuInformation.MemoryModeEnabled == MODE_DISABLED) {
    pDimmInfo->SKUViolation = TRUE;
  } else if (pDimm->MappedPersistentCapacity > 0 && pDimm->SkuInformation.AppDirectModeEnabled == MODE_DISABLED) {
    pDimmInfo->SKUViolation = TRUE;
  } else {
    for (Index = 0; Index < pDimm->ISsNum; Index++) {
      LIST_FOR_EACH(pNodeNamespace, &pDimm->pISs[Index]->AppDirectNamespaceList) {
        pCurNamespace = NAMESPACE_FROM_NODE(pNodeNamespace, IsNode);
        if (pCurNamespace->NamespaceType == APPDIRECT_NAMESPACE &&
          pDimm->SkuInformation.AppDirectModeEnabled == MODE_DISABLED) {
          pDimmInfo->SKUViolation = TRUE;
          break;
        }
      }
    }
  }

  AsciiStrToUnicodeStrS(pDimm->PartNumber, pDimmInfo->PartNumber, PART_NUMBER_LEN + 1);

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_SECURITY)
  {
    /* Security opt-in */
    pSecurityOptInPayload = AllocateZeroPool(sizeof(*pSecurityOptInPayload));
    if (pSecurityOptInPayload == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    /* Get Security Opt-In SVN Downgrade*/
    pDimmInfo->SVNDowngradeOptIn = OPT_IN_VALUE_INVALID;
    ReturnCode = FwCmdGetSecurityOptIn(pDimm, NVM_SVN_DOWNGRADE, pSecurityOptInPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FW CMD Error (OPT_IN_SVN_DOWNGRADE): " FORMAT_EFI_STATUS "", ReturnCode);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_SVN_DOWNGRADE;
    }

    if (pSecurityOptInPayload->OptInCode == NVM_SVN_DOWNGRADE) {
      pDimmInfo->SVNDowngradeOptIn = pSecurityOptInPayload->OptInValue;
    }

    pSecurityOptInPayload = ZeroMem(pSecurityOptInPayload, sizeof(*pSecurityOptInPayload));

    /* Get Security Opt-In Secure Erase Policy*/
    pDimmInfo->SecureErasePolicyOptIn = OPT_IN_VALUE_INVALID;
    ReturnCode = FwCmdGetSecurityOptIn(pDimm, NVM_SECURE_ERASE_POLICY, pSecurityOptInPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FW CMD Error (OPT_IN_SECURE_ERASE_POLICY): " FORMAT_EFI_STATUS "", ReturnCode);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_SECURE_ERASE_POLICY;
    }

    if (pSecurityOptInPayload->OptInCode == NVM_SECURE_ERASE_POLICY) {
      pDimmInfo->SecureErasePolicyOptIn = pSecurityOptInPayload->OptInValue;
    }

    pSecurityOptInPayload = ZeroMem(pSecurityOptInPayload, sizeof(*pSecurityOptInPayload));

    /* Get Security Opt-In S3 Resume */
    pDimmInfo->S3ResumeOptIn = OPT_IN_VALUE_INVALID;
    ReturnCode = FwCmdGetSecurityOptIn(pDimm, NVM_S3_RESUME, pSecurityOptInPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FW CMD Error (OPT_IN_S3_RESUME): " FORMAT_EFI_STATUS "", ReturnCode);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_S3RESUME;
    }

    if (pSecurityOptInPayload->OptInCode == NVM_S3_RESUME) {
      pDimmInfo->S3ResumeOptIn = pSecurityOptInPayload->OptInValue;
    }

    pSecurityOptInPayload = ZeroMem(pSecurityOptInPayload, sizeof(*pSecurityOptInPayload));

    /* Get FW Activate Opt-In Secure Erase Policy*/
    pDimmInfo->FwActivateOptIn = OPT_IN_VALUE_INVALID;
    ReturnCode = FwCmdGetSecurityOptIn(pDimm, NVM_FW_ACTIVATE, pSecurityOptInPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FW CMD Error (OPT_IN_FW_ACTIVATE): " FORMAT_EFI_STATUS "", ReturnCode);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_FW_ACTIVATE;
    }

    if (pSecurityOptInPayload->OptInCode == NVM_FW_ACTIVATE) {
      pDimmInfo->FwActivateOptIn = pSecurityOptInPayload->OptInValue;
    }

    /* security state */
    pSecurityPayload = AllocateZeroPool(sizeof(*pSecurityPayload));
    if (pSecurityPayload == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    ReturnCode = FwCmdGetSecurityInfo(pDimm, pSecurityPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FW CMD Error (SECURITY_INFO): " FORMAT_EFI_STATUS "", ReturnCode);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_SECURITY_INFO;
    }
    pDimmInfo->SecurityStateBitmask = pSecurityPayload->SecurityStatus.AsUint32;
    ConvertSecurityBitmask(pSecurityPayload->SecurityStatus.AsUint32, &pDimmInfo->SecurityState);

    pDimmInfo->MasterPassphraseEnabled = (BOOLEAN) pSecurityPayload->SecurityStatus.Separated.MasterPassphraseEnabled;
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_PACKAGE_SPARING)
  {
    ReturnCode = FwCmdGetPackageSparingPolicy(pDimm, &pGetPackageSparingPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Get package sparing policy failed with error " FORMAT_EFI_STATUS " for DIMM 0x%x", ReturnCode, pDimm->DeviceHandle.AsUint32);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_PACKAGE_SPARING;
    }
    else {
      pDimmInfo->PackageSparingEnabled = pGetPackageSparingPayload->Enable;
      // This maintains backwards compatibility with FIS 1.3
      pDimmInfo->PackageSparesAvailable = (pGetPackageSparingPayload->Supported > PACKAGE_SPARING_NOT_SUPPORTED) ?
        PACKAGE_SPARES_AVAILABLE : PACKAGE_SPARES_NOT_AVAILABLE;
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_ARS_STATUS)
  {
    /* address range scrub */
    ReturnCode = FwCmdGetARS(pDimm, &pDimmInfo->ARSStatus);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FwCmdGetARS failed with error " FORMAT_EFI_STATUS " for DIMM %d", ReturnCode, pDimm->DeviceHandle.AsUint32);
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_SMART_AND_HEALTH)
  {
    /* Get current health state */
    ReturnCode = GetSmartAndHealth(&gNvmDimmDriverNvmDimmConfig,pDimm->DimmID, &HealthInfo);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_SMART_AND_HEALTH;
    }
    // Fill in the DCPMM's understanding of its own HealthState if we didn't have any
    // other opinions earlier
    if (HEALTH_UNKNOWN == pDimmInfo->HealthState) {
       ConvertHealthBitmask(HealthInfo.HealthStatus, &pDimmInfo->HealthState);
    }
    pDimmInfo->HealthStatusReason = HealthInfo.HealthStatusReason;
    pDimmInfo->LatchedLastShutdownStatusDetails = HealthInfo.LatchedLastShutdownStatusDetails;
    pDimmInfo->UnlatchedLastShutdownStatusDetails = HealthInfo.UnlatchedLastShutdownStatusDetails;
    pDimmInfo->ThermalThrottlePerformanceLossPrct = HealthInfo.ThermalThrottlePerformanceLossPrct;
    pDimmInfo->LastShutdownTime = HealthInfo.LastShutdownTime;
    pDimmInfo->AitDramEnabled = HealthInfo.AitDramEnabled;
    pDimmInfo->MaxMediaTemperature = HealthInfo.MaxMediaTemperature;
    pDimmInfo->MaxControllerTemperature = HealthInfo.MaxControllerTemperature;
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_POWER_MGMT_POLICY)
  {
    /* Get current Power Management Policy info */
    ReturnCode = FwCmdGetPowerManagementPolicy(pDimm, &pPowerManagementPolicyPayload);
    if (ReturnCode == EFI_OUT_OF_RESOURCES) {
      goto Finish;
    }
    else if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FwCmdGetPowerManagementPolicy failed with error " FORMAT_EFI_STATUS " for DIMM 0x%x", ReturnCode, pDimm->DeviceHandle.AsUint32);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_POWER_MGMT;
    }
    else {
      if (2 > pPowerManagementPolicyPayload->FisMajor) {
        pDimmInfo->PeakPowerBudget.Header.Status.Code = ReturnCode;
        pDimmInfo->PeakPowerBudget.Header.Type = DIMM_INFO_TYPE_UINT16;
        pDimmInfo->PeakPowerBudget.Data = pPowerManagementPolicyPayload->Payload.Fis_1_15.PeakPowerBudget;
      }
      else {
        pDimmInfo->PeakPowerBudget.Header.Status.Code = EFI_UNSUPPORTED;
      }

      pDimmInfo->AvgPowerLimit.Header.Status.Code = ReturnCode;
      pDimmInfo->AvgPowerLimit.Header.Type = DIMM_INFO_TYPE_UINT16;
      pDimmInfo->AvgPowerLimit.Data = pPowerManagementPolicyPayload->Payload.Fis_2_01.AveragePowerLimit;

      if (2 <= pPowerManagementPolicyPayload->FisMajor) {
        /* 2.1+: MemoryBandwidthBoostFeature */
        pDimmInfo->MemoryBandwidthBoostFeature.Header.Status.Code = ReturnCode;
        pDimmInfo->MemoryBandwidthBoostFeature.Header.Type = DIMM_INFO_TYPE_UINT16;
        pDimmInfo->MemoryBandwidthBoostFeature.Data = pPowerManagementPolicyPayload->Payload.Fis_2_01.MemoryBandwidthBoostFeature;

        /* 2.1+: MemoryBandwidthBoostMaxPowerLimit */
        pDimmInfo->MemoryBandwidthBoostMaxPowerLimit.Header.Status.Code = ReturnCode;
        pDimmInfo->MemoryBandwidthBoostMaxPowerLimit.Header.Type = DIMM_INFO_TYPE_UINT16;
        pDimmInfo->MemoryBandwidthBoostMaxPowerLimit.Data = pPowerManagementPolicyPayload->Payload.Fis_2_01.MemoryBandwidthBoostMaxPowerLimit;
      }
      else {
        pDimmInfo->MemoryBandwidthBoostFeature.Header.Status.Code = EFI_UNSUPPORTED;
        pDimmInfo->MemoryBandwidthBoostMaxPowerLimit.Header.Status.Code = EFI_UNSUPPORTED;
      }

      if ((2 <= pPowerManagementPolicyPayload->FisMajor && 1 <= pPowerManagementPolicyPayload->FisMinor)
        || 3 <= pPowerManagementPolicyPayload->FisMajor) {
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstant.Header.Status.Code = ReturnCode;
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstant.Header.Type = DIMM_INFO_TYPE_UINT32;
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstant.Data = pPowerManagementPolicyPayload->Payload.Fis_2_01.MemoryBandwidthBoostAveragePowerTimeConstant;
      }
      else {
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstant.Header.Status.Code = EFI_UNSUPPORTED;
      }
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_DEVICE_CHARACTERISTICS)
  {
    ReturnCode = FwCmdDeviceCharacteristics(pDimm, &pDevCharacteristics);
    if (ReturnCode == EFI_OUT_OF_RESOURCES) {
      goto Finish;
    }
    else if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FwCmdDeviceCharacteristics failed with error " FORMAT_EFI_STATUS " for DIMM 0x%x", ReturnCode, pDimm->DeviceHandle.AsUint32);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_DEVICE_CHARACTERISTICS;
    }
    else {
      /* MaxAveragePowerLimit */
      if ((1 <= pDevCharacteristics->FisMajor && 13 <= pDevCharacteristics->FisMinor) || 2 <= pDevCharacteristics->FisMajor) {
        pDimmInfo->MaxAveragePowerLimit.Header.Status.Code = ReturnCode;
        pDimmInfo->MaxAveragePowerLimit.Header.Type = DIMM_INFO_TYPE_UINT16;
        pDimmInfo->MaxAveragePowerLimit.Data = pDevCharacteristics->Payload.Fis_2_01.MaxAveragePowerLimit;
      }
      else {
        pDimmInfo->MaxAveragePowerLimit.Header.Status.Code = EFI_UNSUPPORTED;
      }

      /* 2.1+ MaxMemoryBandwidthBoostMaxPowerLimit */
      if (2 <= pDevCharacteristics->FisMajor) {
        pDimmInfo->MaxMemoryBandwidthBoostMaxPowerLimit.Header.Status.Code = ReturnCode;
        pDimmInfo->MaxMemoryBandwidthBoostMaxPowerLimit.Header.Type = DIMM_INFO_TYPE_UINT16;
        pDimmInfo->MaxMemoryBandwidthBoostMaxPowerLimit.Data = pDevCharacteristics->Payload.Fis_2_01.MaxMemoryBandwidthBoostMaxPowerLimit;
      }
      else {
        pDimmInfo->MaxMemoryBandwidthBoostMaxPowerLimit.Header.Status.Code = EFI_UNSUPPORTED;
      }

      /* 2.1+ MaxMemoryBandwidthBoostAveragePowerTimeConstant
              MemoryBandwidthBoostAveragePowerTimeConstantStep
              MaxAveragePowerReportingTimeConstant
              AveragePowerReportingTimeConstantStep
      */
      if ((2 <= pDevCharacteristics->FisMajor && 1 <= pDevCharacteristics->FisMinor)
        || 3 <= pDevCharacteristics->FisMajor) {
        pDimmInfo->MaxMemoryBandwidthBoostAveragePowerTimeConstant.Header.Status.Code = ReturnCode;
        pDimmInfo->MaxMemoryBandwidthBoostAveragePowerTimeConstant.Header.Type = DIMM_INFO_TYPE_UINT32;
        pDimmInfo->MaxMemoryBandwidthBoostAveragePowerTimeConstant.Data = pDevCharacteristics->Payload.Fis_2_01.MaxMemoryBandwidthBoostAveragePowerTimeConstant;

        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstantStep.Header.Status.Code = ReturnCode;
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstantStep.Header.Type = DIMM_INFO_TYPE_UINT32;
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstantStep.Data = pDevCharacteristics->Payload.Fis_2_01.MemoryBandwidthBoostAveragePowerTimeConstantStep;

        pDimmInfo->MaxAveragePowerReportingTimeConstant.Header.Status.Code = ReturnCode;
        pDimmInfo->MaxAveragePowerReportingTimeConstant.Header.Type = DIMM_INFO_TYPE_UINT32;
        pDimmInfo->MaxAveragePowerReportingTimeConstant.Data = pDevCharacteristics->Payload.Fis_2_01.MaxAveragePowerReportingTimeConstant;

        pDimmInfo->AveragePowerReportingTimeConstantStep.Header.Status.Code = ReturnCode;
        pDimmInfo->AveragePowerReportingTimeConstantStep.Header.Type = DIMM_INFO_TYPE_UINT32;
        pDimmInfo->AveragePowerReportingTimeConstantStep.Data = pDevCharacteristics->Payload.Fis_2_01.AveragePowerReportingTimeConstantStep;
      }
      else {
        pDimmInfo->MaxMemoryBandwidthBoostAveragePowerTimeConstant.Header.Status.Code = EFI_UNSUPPORTED;
        pDimmInfo->MemoryBandwidthBoostAveragePowerTimeConstantStep.Header.Status.Code = EFI_UNSUPPORTED;
        pDimmInfo->MaxAveragePowerReportingTimeConstant.Header.Status.Code = EFI_UNSUPPORTED;
        pDimmInfo->AveragePowerReportingTimeConstantStep.Header.Status.Code = EFI_UNSUPPORTED;
      }
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_OPTIONAL_CONFIG_DATA_POLICY) {
    /* Get current AveragePowerReportingTimeConstant */
    ReturnCode = FwCmdGetOptionalConfigurationDataPolicy(pDimm, &OptionalDataPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_OPTIONAL_CONFIG_DATA;
    }

    /* AvgPowerReportingTimeConstant */
    if ((2 == OptionalDataPolicyPayload.FisMajor && 1 <= OptionalDataPolicyPayload.FisMinor)
      || 3 <= OptionalDataPolicyPayload.FisMajor) {
      pDimmInfo->AvgPowerReportingTimeConstant.Header.Status.Code = ReturnCode;
      pDimmInfo->AvgPowerReportingTimeConstant.Header.Type = DIMM_INFO_TYPE_UINT32;
      pDimmInfo->AvgPowerReportingTimeConstant.Data = OptionalDataPolicyPayload.Payload.Fis_2_01.AveragePowerReportingTimeConstant;
    }
    else {
      pDimmInfo->AvgPowerReportingTimeConstant.Header.Status.Code = EFI_UNSUPPORTED;
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_VIRAL_POLICY)
  {
    /* Get current ViralPolicy state */
    ReturnCode = FwCmdGetViralPolicy(pDimm, &ViralPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_VIRAL_POLICY;
    }

    pDimmInfo->ViralPolicyEnable = ViralPolicyPayload.ViralPolicyEnable;
    pDimmInfo->ViralStatus = ViralPolicyPayload.ViralStatus;
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_OVERWRITE_DIMM_STATUS)
  {
    ReturnCode = GetOverwriteDimmStatus(pDimm, &pDimmInfo->OverwriteDimmStatus);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_OVERWRITE_STATUS;
    }
  }

  // Data already in pDimm
  pDimmInfo->Configured = pDimm->Configured;
  ReturnCode = GetDcpmmCapacities(pDimm->DimmID, &pDimmInfo->Capacity, &pDimmInfo->VolatileCapacity,
    &pDimmInfo->AppDirectCapacity, &pDimmInfo->UnconfiguredCapacity, &pDimmInfo->ReservedCapacity,
    &pDimmInfo->InaccessibleCapacity);
  if (EFI_ERROR(ReturnCode)) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_CAPACITY;
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_FW_IMAGE_INFO)
  {
    ReturnCode = FwCmdGetFirmwareImageInfo(pDimm, &pPayloadFwImage);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_FW_IMAGE_INFO; // maybe used in other caller APIs excluding ShowDimms()
    }
    else {
      pDimmInfo->LastFwUpdateStatus = pPayloadFwImage->LastFwUpdateStatus;
      pDimmInfo->StagedFwVersion = ParseFwVersion(pPayloadFwImage->StagedFwRevision);
      pDimmInfo->StagedFwActivatable = pPayloadFwImage->StagedFwActivatable;
      pDimmInfo->FWImageMaxSize = pPayloadFwImage->FWImageMaxSize * BLOCKSIZE_4K; // the value FWImageMaxSize is actually the multiply of 4 KiB blocks
      pDimmInfo->QuiesceRequired = pPayloadFwImage->QuiesceRequired;
      pDimmInfo->ActivationTime = pPayloadFwImage->ActivationTime;
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_MEM_INFO_PAGE_3)
  {
      ReturnCode = FwCmdGetMemoryInfoPage(pDimm, MEMORY_INFO_PAGE_3, sizeof(PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE3), (VOID **)&pPayloadMemInfoPage3);
      if (EFI_ERROR(ReturnCode)) {
        pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_MEM_INFO_PAGE;
      }
      else {
        pDimmInfo->ErrorInjectionEnabled = (pPayloadMemInfoPage3->ErrorInjectStatus >> ERR_INJECTION_ENABLED_BIT) & ERR_INJECTION_ENABLED_BIT_MASK;
        pDimmInfo->MediaTemperatureInjectionEnabled = (pPayloadMemInfoPage3->ErrorInjectStatus >> ERR_INJECTION_MEDIA_TEMP_ENABLED_BIT) & ERR_INJECTION_MEDIA_TEMP_ENABLED_BIT_MASK;
        pDimmInfo->SoftwareTriggersEnabled = (pPayloadMemInfoPage3->ErrorInjectStatus >> ERR_INJECTION_SW_TRIGGER_ENABLED_BIT) & ERR_INJECTION_SW_TRIGGER_ENABLED_BIT_MASK;
        pDimmInfo->PoisonErrorInjectionsCounter = pPayloadMemInfoPage3->PoisonErrorInjectionsCounter;
        pDimmInfo->PoisonErrorClearCounter = pPayloadMemInfoPage3->PoisonErrorClearCounter;
        pDimmInfo->MediaTemperatureInjectionsCounter = pPayloadMemInfoPage3->MediaTemperatureInjectionsCounter;
        pDimmInfo->SoftwareTriggersCounter = pPayloadMemInfoPage3->SoftwareTriggersCounter;
        pDimmInfo->SoftwareTriggersEnabledDetails = pPayloadMemInfoPage3->SoftwareTriggersEnabledDetails;
      }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_MEM_INFO_PAGE_4)
  {
    ReturnCode = FwCmdGetMemoryInfoPage(pDimm, MEMORY_INFO_PAGE_4, sizeof(PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE4), (VOID **)&pPayloadMemInfoPage4);
    pDimmInfo->DcpmmAveragePower.Header.Status.Code = ReturnCode;
    if (pDimm->FwVer.FwApiMajor < 3) {
      pDimmInfo->AveragePower12V.Header.Status.Code = ReturnCode;
      pDimmInfo->AveragePower1_2V.Header.Status.Code = ReturnCode;
    } else {
      pDimmInfo->AveragePower12V.Header.Status.Code = EFI_UNSUPPORTED;
      pDimmInfo->AveragePower1_2V.Header.Status.Code = EFI_UNSUPPORTED;
    }

    if (EFI_SUCCESS == ReturnCode) {
      pDimmInfo->DcpmmAveragePower.Data = pPayloadMemInfoPage4->DcpmmAveragePower;
      pDimmInfo->DcpmmAveragePower.Header.Type = DIMM_INFO_TYPE_UINT16;
      pDimmInfo->AveragePower12V.Data = pPayloadMemInfoPage4->AveragePower12V;
      pDimmInfo->AveragePower12V.Header.Type = DIMM_INFO_TYPE_UINT16;
      pDimmInfo->AveragePower1_2V.Data = pPayloadMemInfoPage4->AveragePower1_2V;
      pDimmInfo->AveragePower1_2V.Header.Type = DIMM_INFO_TYPE_UINT16;
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_EXTENDED_ADR)
  {
    ReturnCode = FwCmdGetExtendedAdrInfo(pDimm, &PayloadExtendedAdr);
    pDimmInfo->ExtendedAdrEnabled.Header.Status.Code = ReturnCode;
    pDimmInfo->PrevPwrCycleExtendedAdrEnabled.Header.Status.Code = ReturnCode;
    if (EFI_SUCCESS == ReturnCode) {
      pDimmInfo->ExtendedAdrEnabled.Data = PayloadExtendedAdr.ExtendedAdrStatus;
      pDimmInfo->ExtendedAdrEnabled.Header.Type = DIMM_INFO_TYPE_BOOLEAN;
      pDimmInfo->PrevPwrCycleExtendedAdrEnabled.Data = PayloadExtendedAdr.PreviousExtendedAdrStatus;
      pDimmInfo->PrevPwrCycleExtendedAdrEnabled.Header.Type = DIMM_INFO_TYPE_BOOLEAN;
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_LATCH_SYSTEM_SHUTDOWN_STATE) {
    ReturnCode = FwCmdGetLatchSystemShutdownStateInfo(pDimm, &PayloadLatchSystemShutdownState);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_LATCH_SYSTEM_SHUTDOWN_STATE;
    }

    pDimmInfo->LatchSystemShutdownState = PayloadLatchSystemShutdownState.LatchSystemShutdownState;
    pDimmInfo->PrevPwrCycleLatchSystemShutdownState = PayloadLatchSystemShutdownState.PreviousPowerCycleLatchSystemShutdownState;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPowerManagementPolicyPayload);
  FREE_POOL_SAFE(pDevCharacteristics);
  FREE_POOL_SAFE(pPayloadMemInfoPage3);
  FREE_POOL_SAFE(pPayloadMemInfoPage4);
  FREE_POOL_SAFE(pPayloadFwImage);
  FREE_POOL_SAFE(pGetPackageSparingPayload);
  FREE_POOL_SAFE(pSecurityOptInPayload);
  FREE_POOL_SAFE(pSecurityPayload);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check if there is at least one DIMM on specified socket

  @param[in] SocketId

  @retval FALSE SocketId is not valid, it is out of allowed range or
    there was a problem with getting DIMM list
**/
STATIC
BOOLEAN
IsSocketIdValid(
  IN     UINT16 SocketId
  )
{
  NVDIMM_ENTRY();
  BOOLEAN SocketIdValid = FALSE;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;


  // socket id must be within allowed range
  if (SocketId > MAX_SOCKETS) {
    goto Finish;
  }

  // at least one dimm must be plugged to specified socket
  LIST_FOR_EACH(pCurrentDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (pDimm != NULL && pDimm->SocketId == SocketId) {
      SocketIdValid = TRUE;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT();
  return SocketIdValid;
}

/**
  Check if security option is supported

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] SecurityOperation

  @retval TRUE Security operation is supported
  @retval FALSE Security operation is not supported
**/
STATIC
BOOLEAN
IsSecurityOpSupported(
  IN     UINT16 SecurityOperation
  )
{
  BOOLEAN Result = FALSE;
  SYSTEM_CAPABILITIES_INFO SystemCapabilitiesInfo;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  SystemCapabilitiesInfo.PtrInterleaveFormatsSupported = 0;

  ReturnCode = GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig, &SystemCapabilitiesInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on GetSystemCapabilitiesInfo");
    goto Finish;
  }

  switch (SecurityOperation) {
  case SECURITY_OPERATION_SET_PASSPHRASE:
    if (SystemCapabilitiesInfo.EnableDeviceSecuritySupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  case SECURITY_OPERATION_CHANGE_PASSPHRASE:
    if (SystemCapabilitiesInfo.ChangeDevicePassphraseSupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  case SECURITY_OPERATION_DISABLE_PASSPHRASE:
    if (SystemCapabilitiesInfo.DisableDeviceSecuritySupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  case SECURITY_OPERATION_UNLOCK_DEVICE:
    if (SystemCapabilitiesInfo.UnlockDeviceSecuritySupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  case SECURITY_OPERATION_FREEZE_DEVICE:
    if (SystemCapabilitiesInfo.FreezeDeviceSecuritySupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  case SECURITY_OPERATION_ERASE_DEVICE:
    if (SystemCapabilitiesInfo.EraseDeviceDataSupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  case SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE:
    if (SystemCapabilitiesInfo.ChangeMasterPassphraseSupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;


  case SECURITY_OPERATION_MASTER_ERASE_DEVICE:
    if (SystemCapabilitiesInfo.MasterEraseDeviceDataSupported == FEATURE_SUPPORTED) {
      Result = TRUE;
    }
    break;

  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveSize);
  NVDIMM_EXIT();
  return Result;
}

/**
  Helper function for VerifyTargetDimms().
  Contains logic for checking if a DCPMM is allowed based on the
  RequireDcpmmsBitfield provided to VerifyTargetDimms.

  @param[in] pCurrentDimm Pointer to DCPMM to test
  @param[in] RequireDcpmmsBitfield Indicate what requirements should be validated
  on the list of DCPMMs discovered.
**/
BOOLEAN IsDimmAllowed(
  IN DIMM *pDimm,
  IN REQUIRE_DCPMMS RequireDcpmmsBitfield
  )
{
  BOOLEAN Allowed = TRUE;

  // Verify the mutually exclusive REQUIRE_DCPMMS_MANAGEABLE and REQUIRE_DCPMMS_UNMANAGEABLE flags are not both set
  ASSERT(!((REQUIRE_DCPMMS_MANAGEABLE & RequireDcpmmsBitfield) && (REQUIRE_DCPMMS_UNMANAGEABLE & RequireDcpmmsBitfield)));
  // Generally we only work with manageable NVDIMMs, which are
  // an Intel DCPMM with a reasonable FIS API version.
  if ((REQUIRE_DCPMMS_MANAGEABLE & RequireDcpmmsBitfield) && !IsDimmManageable(pDimm)) {
    Allowed = FALSE;
  }
  if ((REQUIRE_DCPMMS_UNMANAGEABLE & RequireDcpmmsBitfield) && IsDimmManageable(pDimm)) {
    Allowed = FALSE;
  }

  // Verify the mutually exclusive REQUIRE_DCPMMS_FUNCTIONAL and REQUIRE_DCPMMS_NON_FUNCTIONAL flags are not both set
  ASSERT(!((REQUIRE_DCPMMS_FUNCTIONAL & RequireDcpmmsBitfield) && (REQUIRE_DCPMMS_NON_FUNCTIONAL & RequireDcpmmsBitfield)));
  // Non-functional DCPMMs generally means just that DDRT is untrained, but there
  // can be other causes. Keeping previous behavior for now until we split up the
  // meaning of non-functional more.
  if ((REQUIRE_DCPMMS_FUNCTIONAL & RequireDcpmmsBitfield) && pDimm->NonFunctional) {
    Allowed = FALSE;
  }
  if ((REQUIRE_DCPMMS_NON_FUNCTIONAL & RequireDcpmmsBitfield) && !pDimm->NonFunctional) {
    Allowed = FALSE;
  }

  // Verify the mutually exclusive REQUIRE_DCPMMS_POPULATION_VIOLATION and REQUIRE_DCPMMS_NO_POPULATION_VIOLATION flags are not both set
  ASSERT(!((REQUIRE_DCPMMS_POPULATION_VIOLATION & RequireDcpmmsBitfield) && (REQUIRE_DCPMMS_NO_POPULATION_VIOLATION & RequireDcpmmsBitfield)));
  // Population violation DCPMMs means DCPMMs that are in locations that are not part of a POR configuration
  if ((REQUIRE_DCPMMS_POPULATION_VIOLATION & RequireDcpmmsBitfield) && !IsDimmInPopulationViolation(pDimm)) {
    Allowed = FALSE;
  }
  if ((REQUIRE_DCPMMS_NO_POPULATION_VIOLATION & RequireDcpmmsBitfield) && IsDimmInPopulationViolation(pDimm)) {
    Allowed = FALSE;
  }

  if ((REQUIRE_DCPMMS_NO_UNMAPPED_POPULATION_VIOLATION & RequireDcpmmsBitfield) && IsDimmInUnmappedPopulationViolation(pDimm)) {
    Allowed = FALSE;
  }

  return Allowed;
}

/**
  Verify target DIMM IDs list. Fill output list of pointers to dimms.

  If sockets were specified then get all DIMMs from these sockets.
  If DIMM Ids were provided then check if those DIMMs exist.
  If there are duplicate DIMM/socket Ids then report error.
  If specified DIMMs count is 0 then take all Manageable DIMMs.
  Update CommandStatus structure with any warnings/errors found.

  @param[in] DimmIds An array of DIMM Ids
  @param[in] DimmIdsCount Number of items in array of DIMM Ids
  @param[in] SocketIds An array of Socket Ids
  @param[in] SocketIdsCount Number of items in array of Socket Ids
  @param[in] RequireDcpmmsBitfield Indicate what requirements should be validated on
  the list of DCPMMs discovered.
  @param[out] pDimms Output array of pointers to verified dimms
  @param[out] pDimmsNum Number of items in array of pointers to dimms
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_INVALID_PARAMETER Problem with getting specified DIMMs
  @retval EFI_SUCCESS All Ok
 **/
EFI_STATUS
EFIAPI
VerifyTargetDimms (
  IN     UINT16 DimmIds[]      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 SocketIds[]    OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     REQUIRE_DCPMMS RequireDcpmmsBitfield,
     OUT DIMM *pDimms[MAX_DIMMS],
     OUT UINT32 *pDimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  LIST_ENTRY *pDimmList = NULL;
  DIMM *pCurrentDimm = NULL;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  BOOLEAN FoundMatchDimmId = FALSE;
  BOOLEAN FoundMatchSocketId = FALSE;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsNum == NULL || pCommandStatus == NULL ||
      DimmIdsCount > MAX_DIMMS || SocketIdsCount > MAX_SOCKETS) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pDimmsNum = 0;

  pDimmList = &gNvmDimmData->PMEMDev.Dimms;

  // Input sanity checking
  if ((SocketIdsCount > 0 && SocketIds == NULL) ||
      (DimmIdsCount > 0 && DimmIds == NULL)) {
    NVDIMM_ERR("Invalid input parameters");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Verify system dimm list
  LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (pCurrentDimm == NULL) {
      ReturnCode = EFI_LOAD_ERROR;
      goto Finish;
    }
  }

  // Process through all dimm and socket ids to give maximal feedback, then
  // go to Finish

  // Verify provided dimms ids
  for (Index = 0; Index < DimmIdsCount; Index++) {
    pCurrentDimm = GetDimmByPid(DimmIds[Index], &gNvmDimmData->PMEMDev.Dimms);
    if (pCurrentDimm == NULL) {
      SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_DIMM_NOT_FOUND);
      ReturnCode = EFI_INVALID_PARAMETER;
      continue;
    }

    if (!IsDimmAllowed(pCurrentDimm, RequireDcpmmsBitfield)) {
      SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_DIMM_EXCLUDED);
      ReturnCode = EFI_INVALID_PARAMETER;
      continue;
    }
    for (Index2 = Index+1; Index2 < DimmIdsCount; Index2++) {
      if (DimmIds[Index] == DimmIds[Index2]) {
        NVDIMM_ERR("Duplicate dimm id");
        SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_DIMM_ID_DUPLICATED);
        ReturnCode = EFI_INVALID_PARAMETER;
        break;
      }
    }
  }

  // Verify provided socket ids
  for (Index = 0; Index < SocketIdsCount; Index++) {
    if (!IsSocketIdValid(SocketIds[Index])) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_SOCKET_ID_NOT_VALID);
      ReturnCode = EFI_INVALID_PARAMETER;
      continue;
    }
    // check for duplicate entries
    for (Index2 = Index+1; Index2 < SocketIdsCount; Index2++) {
      if (SocketIds[Index] == SocketIds[Index2]) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_SOCKET_ID_DUPLICATED);
        ReturnCode = EFI_INVALID_PARAMETER;
        break;
      }
    }
  }

  // Go to Finish with any error from previous sections
  CHECK_RETURN_CODE(ReturnCode, Finish);

  // Main loop, go through each DCPMM in platform
  LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    // If it is not an allowed DCPMM, skip it
    // Error was already thrown if it is a specified DCPMM
    if (!IsDimmAllowed(pCurrentDimm, RequireDcpmmsBitfield)) {
      continue;
    }

    FoundMatchDimmId = FALSE;
    FoundMatchSocketId = FALSE;
    for (Index = 0; Index < DimmIdsCount; Index++) {
      if (pCurrentDimm->DimmID == DimmIds[Index]) {
        FoundMatchDimmId = TRUE;
      }
    }
    for (Index = 0; Index < SocketIdsCount; Index++) {
      if (pCurrentDimm->SocketId == SocketIds[Index]) {
        FoundMatchSocketId = TRUE;
      }
    }

    // Check if specified socket and dimm ids conflict
    if (DimmIdsCount > 0 && SocketIdsCount > 0 &&
        (FoundMatchDimmId != FoundMatchSocketId)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_SOCKET_ID_INCOMPATIBLE_W_DIMM_ID);
      goto Finish;
    }

    // If unspecified or we found a match
    if ((DimmIdsCount == 0 && SocketIdsCount == 0) ||
        FoundMatchSocketId == TRUE || FoundMatchDimmId == TRUE) {

      // Check for buffer overflow
      if (*pDimmsNum >= MAX_DIMMS) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
        ReturnCode = EFI_BUFFER_TOO_SMALL;
        goto Finish;
      }

      // Add to output dimms list
      pDimms[(*pDimmsNum)] = pCurrentDimm;
      (*pDimmsNum)++;
    }
  }

  // sanity checks
  if (*pDimmsNum == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_NO_USABLE_DIMMS);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Verify target DIMM IDs in list are available for SPI Flash.

  If DIMM Ids were provided then check if those DIMMs exist in a SPI flashable
  state and return list of verified dimms.
  If specified DIMMs count is 0 then return all DIMMS that are in SPI
  Flashable state.
  Update CommandStatus structure at the end.

  @param[in] DimmIds An array of DIMM Ids
  @param[in] DimmIdsCount Number of items in array of DIMM Ids
  @param[out] pDimms Output array of pointers to verified dimms
  @param[out] pDimmsNum Number of items in array of pointers to dimms
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS Success
  @retval EFI_NOT_FOUND a dimm in DimmIds is not in a flashable state or no dimms found
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
VerifyNonfunctionalTargetDimms(
  IN     UINT16 DimmIds[]      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  OUT DIMM *pDimms[MAX_DIMMS],
  OUT UINT32 *pDimmsNum,
  OUT COMMAND_STATUS *pCommandStatus
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pDimmList = NULL;
  DIMM *pCurrentDimm = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  UINT32 Index = 0;
  BOOLEAN AllRequestedDimmsVerified = TRUE;

  *pDimmsNum = 0;

  pDimmList = &gNvmDimmData->PMEMDev.Dimms;

  // Input sanity checking
  if (DimmIdsCount > 0 && DimmIds == NULL) {
    NVDIMM_ERR("Invalid input parameters");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmIdsCount > 0) {
    // check if specified DIMMs exist in desired state
    for (Index = 0; Index < DimmIdsCount; Index++) {
      pCurrentDimm = GetDimmByPid(DimmIds[Index], pDimmList);
      if (pCurrentDimm == NULL) {
        NVDIMM_DBG("Failed on GetDimmByPid. Does DIMM 0x%04x exist?", DimmIds[Index]);
        AllRequestedDimmsVerified = FALSE;
        ReturnCode = EFI_NOT_FOUND;
        goto Finish;
      }

      if (!pCurrentDimm->NonFunctional ||
        (pCurrentDimm->SubsystemVendorId != SPD_INTEL_VENDOR_ID) ||
        (pCurrentDimm->SubsystemDeviceId != SPD_DEVICE_ID_15))
      {
        AllRequestedDimmsVerified = FALSE;
      }
      else {
        pCurrentDimm->SmbusAddress.Cpu = (UINT8)(pCurrentDimm->DeviceHandle.NfitDeviceHandle.SocketId);
        pCurrentDimm->SmbusAddress.Imc = (UINT8)(pCurrentDimm->DeviceHandle.NfitDeviceHandle.MemControllerId);
        pCurrentDimm->SmbusAddress.Slot =
          (UINT8)(pCurrentDimm->DeviceHandle.NfitDeviceHandle.MemChannel * MAX_DIMMS_PER_CHANNEL +
            pCurrentDimm->DeviceHandle.NfitDeviceHandle.DimmNumber);

        // add dimm to list of verified dimms
        pCurrentDimm->Signature = DIMM_SIGNATURE;
        pDimms[(*pDimmsNum)] = pCurrentDimm;
        (*pDimmsNum)++;
      }
    }
    if (AllRequestedDimmsVerified) {
      ReturnCode = EFI_SUCCESS;
    }
    else {
      ReturnCode = EFI_NOT_FOUND;
    }
  }
  else {
    // get all dimms in system in desired state
    LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
      pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
      if (pCurrentDimm == NULL) {
        ReturnCode = EFI_LOAD_ERROR;
        goto Finish;
      }
      if ((pCurrentDimm->NonFunctional) &&
        (pCurrentDimm->SubsystemVendorId == SPD_INTEL_VENDOR_ID) &&
        (pCurrentDimm->SubsystemDeviceId == SPD_DEVICE_ID_15))
      {
        pCurrentDimm->SmbusAddress.Cpu = (UINT8)(pCurrentDimm->DeviceHandle.NfitDeviceHandle.SocketId);
        pCurrentDimm->SmbusAddress.Imc = (UINT8)(pCurrentDimm->DeviceHandle.NfitDeviceHandle.MemControllerId);
        pCurrentDimm->SmbusAddress.Slot =
          (UINT8)(pCurrentDimm->DeviceHandle.NfitDeviceHandle.MemChannel * MAX_DIMMS_PER_CHANNEL +
            pCurrentDimm->DeviceHandle.NfitDeviceHandle.DimmNumber);

        pCurrentDimm->Signature = DIMM_SIGNATURE;

        // add dimm to list of verified dimms
        pDimms[(*pDimmsNum)] = pCurrentDimm;
        (*pDimmsNum)++;
      }
    }
    ReturnCode = EFI_SUCCESS;
  }

  // sanity checks
  if (*pDimmsNum == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
  else if (*pDimmsNum > MAX_DIMMS) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

Finish:

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set object status for PMem modules not paired with DDR in case of 2LM

  @param[in] pDimms Array of pointers to targeted PMem modules only
  @param[in] pDimmsNum Number of pointers in pDimms
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_LOAD_ERROR Error in retrieving information from ACPI tables
  @retval EFI_INVALID_PARAMETER pCommandStatus is NULL
  @retval EFI_SUCCESS All Ok
 **/
EFI_STATUS
SetObjStatusForPMemNotPairedWithDdr(
  IN     DIMM *pDimms[MAX_DIMMS],
  IN     UINT32 DimmsNum,
  OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_LOAD_ERROR;
  DIMM *pDimm = NULL;
  UINT32 Index1 = 0, Index2 = 0;
  BOOLEAN IsDcpmmPairedWithDdr = FALSE;
  BOOLEAN NonPorCrossTileSupportedConfig = FALSE;
  ParsedPmttHeader *pPmttHead = NULL;

  NVDIMM_ENTRY();

  if (NULL == pDimms || NULL == pCommandStatus ||
    DimmsNum == 0 || DimmsNum > MAX_DIMMS) {
    NVDIMM_DBG("Invalid parameter passed.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = CheckIsNonPorCrossTileSupportedConfig(&NonPorCrossTileSupportedConfig);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckIsNonPorCrossTileSupportedConfig failed.");
    goto Finish;
  }

  /**
    When in a non-POR cross-tile supported configuration,
    there are no PMem modules that are unusable for 2LM.
  **/
  if (NonPorCrossTileSupportedConfig) {
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  pPmttHead = gNvmDimmData->PMEMDev.pPmttHead;
  if (NULL == pPmttHead) {
    NVDIMM_DBG("Pmtt head not found.");
    /**
    This warning message is disabled on Purley platforms (PMTT Rev: 0x1)
    pPmttHead is NULL for older PMTT revisions
    **/
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  for (Index1 = 0; Index1 < pPmttHead->DCPMModulesNum; Index1++) {
    pDimm = GetDimmByPid(pPmttHead->ppDCPMModules[Index1]->SmbiosHandle, &gNvmDimmData->PMEMDev.Dimms);
    if (pDimm == NULL) {
      NVDIMM_DBG("Failed to retrieve the DCPMM pid %x", pPmttHead->ppDCPMModules[Index1]->SmbiosHandle);
      goto Finish;
    }

    if (!IsPointerInArray((VOID **)pDimms, DimmsNum, pDimm)) {
      continue;
    }

    // Unmanageable, non-functional and population violation PMem modules excluded for Memory Mode
    if (!IsDimmManageable(pDimm) || pDimm->NonFunctional || IsDimmInPopulationViolation(pDimm)) {
      continue;
    }

    // Check to see if this PMem module is paired with a DDR on the iMc
    for (Index2 = 0; Index2 < pPmttHead->DDRModulesNum; Index2++) {
      if ((pPmttHead->ppDDRModules[Index2]->SocketId == pPmttHead->ppDCPMModules[Index1]->SocketId) &&
        (pPmttHead->ppDDRModules[Index2]->DieId == pPmttHead->ppDCPMModules[Index1]->DieId) &&
        (pPmttHead->ppDDRModules[Index2]->MemControllerId == pPmttHead->ppDCPMModules[Index1]->MemControllerId)) {
        IsDcpmmPairedWithDdr = TRUE;
        break;
      }
    }

    if (!IsDcpmmPairedWithDdr) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_WARN_PMEM_MODULE_NOT_PAIRED_FOR_2LM);
    }

    IsDcpmmPairedWithDdr = FALSE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve the list of functional DCPMMs found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[in] dimmInfoCategories DIMM_INFO_CATEGORIES specifies which (if any)
  additional FW api calls is desired. If DIMM_INFO_CATEGORY_NONE, then only
  the properties from the pDimm struct will be populated.
  @param[out] pDimms The dimm list found in NFIT.

  @retval EFI_SUCCESS  The dimm list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or invalid.
  @retval EFI_NOT_FOUND Dimm not found
**/
EFI_STATUS
EFIAPI
GetDimms(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO *pDimms
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  LIST_ENTRY *pNode = NULL;
  DIMM *pCurDimm = NULL;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDimms == NULL) {
    NVDIMM_DBG("pDimms is NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  LIST_COUNT(pNode, &gNvmDimmData->PMEMDev.Dimms, Index);

  if (DimmCount > Index)
  {
    NVDIMM_DBG("DimmCount is more than DIMM list count");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  SetMem(pDimms, sizeof(*pDimms) * DimmCount, 0); // this clears error mask as well

  Index = 0;
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurDimm = DIMM_FROM_NODE(pNode);
    if (pCurDimm->NonFunctional == TRUE) {
      continue;
    }

    if (DimmCount <= Index) {
      NVDIMM_DBG("Array is too small to hold entire DIMM list");
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    GetDimmInfo(pCurDimm, dimmInfoCategories, &pDimms[Index]);
    Index++;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Retrieve the list of non-functional PMem modules found in NFIT

  Note: To properly fill in these fields, it is necessary to call GetDimm()
  after this with your desired DIMM_INFO_CATEGORIES.

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[out] pDimms The PMem module list

  @retval EFI_SUCCESS  The module list was returned properly
  @retval EFI_INVALID_PARAMETER one or more parameter are NULL or invalid.
  @retval EFI_NOT_FOUND PMem module not found
**/
EFI_STATUS
EFIAPI
GetUninitializedDimms(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 DimmCount,
     OUT DIMM_INFO *pDimms
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  LIST_ENTRY *pNode = NULL;
  DIMM *pCurDimm = NULL;

  NVDIMM_ENTRY();

  if (pThis == NULL || pDimms == NULL) {
    NVDIMM_DBG("Parameter is NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (DimmCount > MAX_DIMMS) {
    NVDIMM_DBG("DimmCount is larger than MAX_DIMMS");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  SetMem(pDimms, sizeof(*pDimms) * DimmCount, 0); // this clears error mask as well

  Index = 0;
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurDimm = DIMM_FROM_NODE(pNode);
    if (pCurDimm->NonFunctional == FALSE) {
      continue;
    }
    if (DimmCount <= Index) {
      NVDIMM_DBG("Array is too small to hold entire DIMM list");
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    InitializeNfitDimmInfoFieldsFromDimm(pCurDimm, &(pDimms[Index]));
    FillSmbiosInfo(&(pDimms[Index]));
    pDimms[Index].MemoryType = MEMORYTYPE_DCPM;
    AsciiStrToUnicodeStrS(pCurDimm->PartNumber, pDimms[Index].PartNumber, PART_NUMBER_STR_LEN);

    pDimms[Index].FwVer = pCurDimm->FwVer;
    pDimms[Index].HealthState = HEALTH_NON_FUNCTIONAL;

    Index++;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve the details about the DIMM specified with pid found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     DIMM_INFO_CATEGORIES dimmInfoCategories,
     OUT DIMM_INFO *pDimmInfo
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDimmInfo == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  SetMem(pDimmInfo, sizeof(*pDimmInfo), 0); // this clears error mask as well
  pDimm = GetDimmByPid(Pid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    NVDIMM_DBG("Failed to retrieve the DCPMM pid %x", Pid);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = GetDimmInfo(pDimm, dimmInfoCategories, pDimmInfo);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to convert Dimm to Discovery");
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#ifdef OS_BUILD


/**
  Retrieve the PMON register values from the dimm

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 SmartDataMask,
  OUT    PMON_REGISTERS *pPayloadPMONRegisters
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  SetMem(pPayloadPMONRegisters, sizeof(*pPayloadPMONRegisters), 0); // this clears error mask as well
  pDimm = GetDimmByPid(Pid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    NVDIMM_DBG("Failed to retrieve the DCPMM pid %x", Pid);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = FwCmdGetPMONRegisters(pDimm, SmartDataMask, pPayloadPMONRegisters);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to Get PMON Registers");
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set the PMON register values from the dimm

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Pid The ID of the dimm to retrieve
  @param[in] PMONGroupEnable Specifies which PMON Group to enable
  @param[out] pPayloadPMONRegisters A pointer to the output payload PMON registers

  @retval EFI_SUCCESS  The dimm information was returned properly
  @retval EFI_INVALID_PARAMETER pDimm is NULL or the dimm with the pid provided does not exist.
**/
EFI_STATUS
EFIAPI
SetPMONRegisters(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 PMONGroupEnable
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pDimm = GetDimmByPid(Pid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    NVDIMM_DBG("Failed to retrieve the DCPMM pid %x", Pid);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = FwCmdSetPMONRegisters(pDimm, PMONGroupEnable);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to Get PMON Registers");
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif

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
EFI_STATUS
EFIAPI
GetSockets(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pSocketCount,
     OUT SOCKET_INFO **ppSockets
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  ParsedPcatHeader *pPcat = NULL;
  UINT32 Index = 0;
  UINT32 SocketCount = 0;
  UINT32 LogicalSocketID = 0;

  NVDIMM_ENTRY();

  if (pThis == NULL || pSocketCount == NULL || ppSockets == NULL) {
    NVDIMM_DBG("One or more parameters are  NULL");
    goto Finish;
  }

  ReturnCode = GetAcpiPcat(pThis, &pPcat);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve the PCAT tables.");
    goto Finish;
  }

  if (pPcat != NULL) {
    SocketCount = pPcat->SocketSkuInfoNum;
  } else {
    ReturnCode = EFI_DEVICE_ERROR;
    goto Finish;
  }

  if (IS_ACPI_HEADER_REV_MAJ_0_MIN_VALID(pPcat->pPlatformConfigAttr)) {
    SOCKET_SKU_INFO_TABLE *pSocketSkuInfo = NULL;
    if (SocketCount == 0 || pPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable == NULL) {
      NVDIMM_DBG("Platform does not support socket SKU limits.");
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }

    *ppSockets = AllocateZeroPool(sizeof(**ppSockets) * SocketCount);
    if (*ppSockets == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    for (Index = 0; Index < SocketCount; Index++) {
      if (pPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable[Index] == NULL) {
        ReturnCode = EFI_DEVICE_ERROR;
        goto FinishError;
      }
      pSocketSkuInfo = pPcat->pPcatVersion.Pcat2Tables.ppSocketSkuInfoTable[Index];
      (*ppSockets)[Index].SocketId = pSocketSkuInfo->SocketId;
      (*ppSockets)[Index].MappedMemoryLimit = pSocketSkuInfo->MappedMemorySizeLimit;
      (*ppSockets)[Index].TotalMappedMemory = pSocketSkuInfo->TotalMemorySizeMappedToSpa;
    }
  }
  else if (IS_ACPI_HEADER_REV_MAJ_1_OR_MAJ_3(pPcat->pPlatformConfigAttr)) {
    DIE_SKU_INFO_TABLE *pDieSkuInfo = NULL;
    if (SocketCount == 0 || pPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable == NULL) {
      NVDIMM_DBG("Platform does not support socket SKU limits.");
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }

    *ppSockets = AllocateZeroPool(sizeof(**ppSockets) * SocketCount);
    if (*ppSockets == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    for (Index = 0; Index < SocketCount; Index++) {
      if (pPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable[Index] == NULL) {
        ReturnCode = EFI_DEVICE_ERROR;
        goto FinishError;
      }
      pDieSkuInfo = pPcat->pPcatVersion.Pcat3Tables.ppDieSkuInfoTable[Index];

      ReturnCode = GetLogicalSocketIdFromPmtt(pDieSkuInfo->SocketId, pDieSkuInfo->DieId, &LogicalSocketID);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to retrieve logical socket ID");
        goto Finish;
      }

      (*ppSockets)[Index].SocketId = LogicalSocketID & 0XFFFF;
      (*ppSockets)[Index].MappedMemoryLimit = pDieSkuInfo->MappedMemorySizeLimit;
      (*ppSockets)[Index].TotalMappedMemory = pDieSkuInfo->TotalMemorySizeMappedToSpa;
    }
  }
  else {
    NVDIMM_DBG("Unknown PCAT table revision");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  *pSocketCount = SocketCount;
  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishError:
  if (*ppSockets != NULL) {
    FREE_POOL_SAFE(*ppSockets);
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check NVM device security state

  Function checks security state of a set of DIMMs. It sets security state
  to mixed when not all DIMMs have the same state.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
     OUT UINT8 *pSecurityState,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  UINT32 SystemSecurityState = 0;
  UINT32 DimmSecurityState = 0;
  BOOLEAN IsMixed = FALSE;

  NVDIMM_ENTRY();

  SetMem(pDimms, sizeof(pDimms), 0x0);

  if (pThis == NULL || pSecurityState == NULL || pDimmIds == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0,
    REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL,
    pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(
        pDimms[Index],
        PT_TIMEOUT_INTERVAL,
        &DimmSecurityState
        );
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
      }
      goto Finish;
    }
    if (Index == 0) {
      SystemSecurityState = DimmSecurityState;
    }
    if (DimmSecurityState != SystemSecurityState) {
      IsMixed = TRUE;
    }
  }
  if (IsMixed) {
    *pSecurityState = SECURITY_MIXED_STATE;
  } else {
    ConvertSecurityBitmask(SystemSecurityState, pSecurityState);
  }
  ReturnCode = EFI_SUCCESS;
  SetCmdStatus(pCommandStatus, NVM_SUCCESS);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
};
/**
  PopulateAppDirectIndex

  Function tries to populate AppDirectIndex for a Regional goal for all regional goals.

  @param[out] pNumberedGoals This a pointer to array of regional goal appdirect index structures
  @param[out] pNumberedGoalsNum Number of items in the pNumberedGoals
  @param[out] pAppDirectIndex The next appdirect index
**/
static void PopulateAppDirectIndex(
  OUT REGION_GOAL_APPDIRECT_INDEX_TABLE *pNumberedGoals,
  OUT UINT32 *pNumberedGoalsNum,
  OUT UINT32 *pAppDirectIndex
)
{
  DIMM *pCurrentDimm = NULL;
  LIST_ENTRY *pDimmList = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  BOOLEAN AppDirectIndexFound = FALSE;
  UINT32 Index1 = 0;
  UINT32 Index2 = 0;
  pDimmList = &gNvmDimmData->PMEMDev.Dimms;
  if (pNumberedGoals == NULL || pNumberedGoalsNum == NULL || pAppDirectIndex == NULL){
    return;
  }
  LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (pCurrentDimm == NULL) {
      return;
    }
    if (IsDimmManageable(pCurrentDimm)) {
      //Iterate over RegionsGoal for current dimm
      for (Index1 = 0; Index1 < pCurrentDimm->RegionsGoalNum; ++Index1) {
        AppDirectIndexFound = FALSE;
        //Iterate over numbered goals to see if it already exists
        for (Index2 = 0; Index2 < *pNumberedGoalsNum; Index2++) {
          if (pNumberedGoals[Index2].pRegionGoal == pCurrentDimm->pRegionsGoal[Index1]) {
            AppDirectIndexFound = TRUE;
            break;
          }
        }
        if (!AppDirectIndexFound) {
          pNumberedGoals[*pNumberedGoalsNum].pRegionGoal = pCurrentDimm->pRegionsGoal[Index1];
          pNumberedGoals[*pNumberedGoalsNum].AppDirectIndex = *pAppDirectIndex;
          (*pNumberedGoalsNum)++;
          (*pAppDirectIndex)++;
        }
      }
    }
  }
}

/**
  Get region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pConfigGoals pointer to output array
  @param[out] pConfigGoalsCount number of elements written
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_NOT_FOUND PMem module could not be found
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
GetGoalConfigs(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 *pSocketIds    OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     CONST UINT32 ConfigGoalTableSize,
     OUT REGION_GOAL_PER_DIMM_INFO *pConfigGoals,
     OUT UINT32 *pConfigGoalsCount,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  DIMM *pCurrentDimm = NULL;
  REGION_GOAL_PER_DIMM_INFO *pCurrentGoal = NULL;
  UINT32 DimmsCount = 0;
  UINT32 ConfigRegionCount = 0;
  UINT32 Index1 = 0;
  UINT32 Index2 = 0;
  UINT32 Index3 = 0;
  UINT32 NumberedGoalsNum = 0;
  UINT32 AppDirectIndex = 1;
  UINT32 SequenceIndex = 0;
  REGION_GOAL_APPDIRECT_INDEX_TABLE NumberedGoals[MAX_IS_PER_DIMM * MAX_DIMMS];
  MEMORY_MODE AllowedMode = MEMORY_MODE_1LM;

  SetMem(pDimms, sizeof(pDimms), 0x0);
  SetMem(&NumberedGoals, sizeof(NumberedGoals), 0x0);

  if (pConfigGoalsCount == NULL || pConfigGoals == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_ERR("Some of required pointers are null");
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_VOLUME_CORRUPTED == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_PCD_BAD_DEVICE_CONFIG);
    } else if (EFI_NO_RESPONSE == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    NVDIMM_ERR("ERROR: RetrieveGoalConfigsFromPlatformConfigData");
    goto Finish;
  }

  //Try to calculate appdirect index for all regional goals for all dimms in advance
  PopulateAppDirectIndex(NumberedGoals, &NumberedGoalsNum, &AppDirectIndex);
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount,
      REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL,
      pDimms, &DimmsCount, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    if (ReturnCode == EFI_NOT_FOUND && pCommandStatus->GeneralStatus == NVM_ERR_NO_USABLE_DIMMS) {
      NVDIMM_DBG("No usable dimms found in GetGoalConfigs");
      ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
      *pConfigGoalsCount = 0;
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }
    else
    {
      NVDIMM_ERR("ERROR: VerifyTargetDimms");
      goto Finish;
    }
  }

  /** Fetch region goals **/
  for (Index1 = 0; Index1 < DimmsCount; ++Index1) {
    pCurrentDimm = pDimms[Index1];
  NVDIMM_DBG("Fetch region dimm idx = %d", Index1);
  NVDIMM_DBG("Fetch region cfg status = 0x%x", pCurrentDimm->GoalConfigStatus);
    if (!pCurrentDimm->RegionsGoalConfig || pCurrentDimm->GoalConfigStatus == GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS) {
    NVDIMM_DBG("Fetch region continue...");
      continue; // No goal config or success status, so omit it.
    }

    if (ConfigRegionCount == ConfigGoalTableSize - 1) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      NVDIMM_ERR("Output table for config goals is to small");
      goto Finish;
    }
    pCurrentGoal = &pConfigGoals[ConfigRegionCount];
    pCurrentGoal->DimmID = pCurrentDimm->DeviceHandle.AsUint32;

    ReturnCode = GetDimmUid(pCurrentDimm, pCurrentGoal->DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_ERR("ERROR: GetDimmUid");
      goto Finish;
    }

    pCurrentGoal->SocketId = pCurrentDimm->SocketId;
    pCurrentGoal->VolatileSize = pCurrentDimm->VolatileSizeGoal;

    pCurrentGoal->PersistentRegions = pCurrentDimm->RegionsGoalNum;

    NVDIMM_DBG("dimm index %d", Index1);
    NVDIMM_DBG("dimm socket id %x", pCurrentDimm->SocketId);
    NVDIMM_DBG("dimm volatile size %x", pCurrentDimm->VolatileSizeGoal);
    NVDIMM_DBG("dimm goal %x", pCurrentDimm->RegionsGoalNum);

    for (Index2 = 0; Index2 < pCurrentDimm->RegionsGoalNum; ++Index2) {
      SequenceIndex = pCurrentDimm->pRegionsGoal[Index2]->SequenceIndex;
    NVDIMM_DBG("region loop %d, region goal size %x, dimms num %x", Index2, pCurrentDimm->pRegionsGoal[Index2]->Size, pCurrentDimm->pRegionsGoal[Index2]->DimmsNum);
      pCurrentGoal->NumberOfInterleavedDimms[SequenceIndex] = (UINT8)pCurrentDimm->pRegionsGoal[Index2]->DimmsNum;
      pCurrentGoal->AppDirectSize[SequenceIndex] =
          pCurrentDimm->pRegionsGoal[Index2]->Size / pCurrentDimm->pRegionsGoal[Index2]->DimmsNum;
      pCurrentGoal->InterleaveSetType[SequenceIndex] = pCurrentDimm->pRegionsGoal[Index2]->InterleaveSetType;
      pCurrentGoal->ImcInterleaving[SequenceIndex] = pCurrentDimm->pRegionsGoal[Index2]->ImcInterleaving;
      pCurrentGoal->ChannelInterleaving[SequenceIndex] = pCurrentDimm->pRegionsGoal[Index2]->ChannelInterleaving;

      /** Retrieve previously calculated AppDirectIndex **/
      for (Index3 = 0; Index3 < NumberedGoalsNum; Index3++) {
        NVDIMM_DBG("appdir loop %d", Index3);
        if (NumberedGoals[Index3].pRegionGoal == pCurrentDimm->pRegionsGoal[Index2]) {
          NVDIMM_DBG("appdir found!");
          pCurrentGoal->AppDirectIndex[SequenceIndex] = (UINT8)NumberedGoals[Index3].AppDirectIndex;
          break;
        }
      }
    }

    pCurrentGoal->Status = pCurrentDimm->GoalConfigStatus;

    ConfigRegionCount++;
  }

  /** Warning when 2LM mode is off (1LM is selected in the BIOS setup) **/
  ReturnCode = AllowedMemoryMode(&AllowedMode);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  if (!IS_BIOS_VOLATILE_MEMORY_MODE_2LM(AllowedMode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE);
  }

  *pConfigGoalsCount = ConfigRegionCount;
Finish:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
  return ReturnCode;
}

/**
  Get DIMM alarm thresholds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
  IN     UINT8 SensorId,
     OUT INT16 *pNonCriticalThreshold,
     OUT UINT8 *pEnabledState,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  PT_PAYLOAD_ALARM_THRESHOLDS *pPayloadAlarmThresholds = NULL;
  INT16 NonCriticalThreshold = THRESHOLD_UNDEFINED;
  UINT8 EnabledState = ENABLED_STATE_UNDEFINED;

  NVDIMM_ENTRY();

  if (pNonCriticalThreshold == NULL && pEnabledState == NULL) {
    goto Finish;
  }

  if ((SensorId != SENSOR_TYPE_MEDIA_TEMPERATURE) &&
    (SensorId != SENSOR_TYPE_CONTROLLER_TEMPERATURE) &&
    (SensorId != SENSOR_TYPE_PERCENTAGE_REMAINING)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    SetObjStatus(pCommandStatus, DimmPid, NULL, 0, NVM_ERR_DIMM_NOT_FOUND, ObjectTypeDimm);
    goto Finish;
  }

  ReturnCode = FwCmdGetAlarmThresholds(pDimm, &pPayloadAlarmThresholds);
  if (pPayloadAlarmThresholds == NULL) {
    ReturnCode = EFI_DEVICE_ERROR;
  }
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_SECURITY_VIOLATION) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_INVALID_SECURITY_STATE);
    } else {
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    }
    goto Finish;
  }

  switch (SensorId) {
  case SENSOR_TYPE_MEDIA_TEMPERATURE:
    NonCriticalThreshold = TransformFwTempToRealValue(pPayloadAlarmThresholds->MediaTemperatureThreshold);
    EnabledState = (UINT8) pPayloadAlarmThresholds->Enable.Separated.MediaTemperature;
    break;
  case SENSOR_TYPE_CONTROLLER_TEMPERATURE:
    NonCriticalThreshold = TransformFwTempToRealValue(pPayloadAlarmThresholds->ControllerTemperatureThreshold);
    EnabledState = (UINT8) pPayloadAlarmThresholds->Enable.Separated.ControllerTemperature;
    break;
  case SENSOR_TYPE_PERCENTAGE_REMAINING:
    NonCriticalThreshold = (INT16) pPayloadAlarmThresholds->PercentageRemainingThreshold;
    EnabledState = (UINT8) pPayloadAlarmThresholds->Enable.Separated.PercentageRemaining;
    break;
  }

  if (pNonCriticalThreshold != NULL && NonCriticalThreshold != THRESHOLD_UNDEFINED) {
    *pNonCriticalThreshold = NonCriticalThreshold;
  }
  if (pEnabledState != NULL && EnabledState != ENABLED_STATE_UNDEFINED) {
    *pEnabledState = EnabledState;
  }

Finish:
  FREE_POOL_SAFE(pPayloadAlarmThresholds);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set DIMM alarm thresholds

  @param[in]  pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in]  pDimmIds Pointer to an array of DIMM IDs
  @param[in]  DimmIdsCount Number of items in array of DIMM IDs
  @param[in]  SensorId Sensor id to set values for
  @param[in]  NonCriticalThreshold New non-critical threshold for sensor
  @param[in]  EnabledState New enable state for sensor
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  PT_PAYLOAD_ALARM_THRESHOLDS *pPayloadAlarmThresholds = NULL;
  UINT32 Index = 0;
  PT_DEVICE_CHARACTERISTICS_OUT *pDevCharacteristics = NULL;
  INT16 ShutdownTemperature = 0;

  NVDIMM_ENTRY();

  SetMem(pDimms, sizeof(pDimms), 0x0);

  if (pCommandStatus == NULL) {
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  if (SensorId != SENSOR_TYPE_MEDIA_TEMPERATURE &&
     SensorId != SENSOR_TYPE_CONTROLLER_TEMPERATURE &&
     SensorId != SENSOR_TYPE_PERCENTAGE_REMAINING) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_SENSOR_NOT_VALID);
    goto Finish;
  }

  if (EnabledState != ENABLED_STATE_UNDEFINED &&
     EnabledState != ENABLED_STATE_ENABLE && EnabledState != ENABLED_STATE_DISABLE) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_SENSOR_ENABLED_STATE_INVALID_VALUE);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0,
      REQUIRE_DCPMMS_MANAGEABLE |
      REQUIRE_DCPMMS_FUNCTIONAL,
       pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (NonCriticalThreshold == THRESHOLD_UNDEFINED && EnabledState == ENABLED_STATE_UNDEFINED) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  for ((Index = 0); Index < DimmsNum; (Index++)) {
    // let's read current values so we'll not overwrite them during setting
    ReturnCode = FwCmdGetAlarmThresholds(pDimms[Index], &pPayloadAlarmThresholds);
    if (pPayloadAlarmThresholds == NULL) {
      ReturnCode = EFI_DEVICE_ERROR;
    }
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      } else {
        ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      }
      goto Finish;
    }

    if (SensorId == SENSOR_TYPE_CONTROLLER_TEMPERATURE || SensorId == SENSOR_TYPE_MEDIA_TEMPERATURE) {
      // Get Shutdown threshold for controller
      ReturnCode = FwCmdDeviceCharacteristics(pDimms[Index], &pDevCharacteristics);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
    }

    if (SensorId == SENSOR_TYPE_CONTROLLER_TEMPERATURE) {
      if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
        ShutdownTemperature = TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.ControllerShutdownThreshold);

        if (!IS_IN_RANGE(NonCriticalThreshold, TEMPERATURE_THRESHOLD_MIN, ShutdownTemperature)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        pPayloadAlarmThresholds->ControllerTemperatureThreshold = TransformRealValueToFwTemp(NonCriticalThreshold);
        pPayloadAlarmThresholds->Enable.Separated.ControllerTemperature = TRUE;
      }

      if (EnabledState != ENABLED_STATE_UNDEFINED) {
        pPayloadAlarmThresholds->Enable.Separated.ControllerTemperature = EnabledState;
      }
    }
    if (SensorId == SENSOR_TYPE_MEDIA_TEMPERATURE) {
      if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
        ShutdownTemperature = TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.MediaShutdownThreshold);

        if (!IS_IN_RANGE(NonCriticalThreshold, TEMPERATURE_THRESHOLD_MIN, ShutdownTemperature)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        pPayloadAlarmThresholds->MediaTemperatureThreshold = TransformRealValueToFwTemp(NonCriticalThreshold);
        pPayloadAlarmThresholds->Enable.Separated.MediaTemperature = TRUE;
      }

      if (EnabledState != ENABLED_STATE_UNDEFINED) {
        pPayloadAlarmThresholds->Enable.Separated.MediaTemperature = EnabledState;
      }
    }
    if (SensorId == SENSOR_TYPE_PERCENTAGE_REMAINING) {
      if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
        if (!IS_IN_RANGE(NonCriticalThreshold, CAPACITY_THRESHOLD_MIN, CAPACITY_THRESHOLD_MAX)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        pPayloadAlarmThresholds->PercentageRemainingThreshold = (UINT8) NonCriticalThreshold;
        pPayloadAlarmThresholds->Enable.Separated.PercentageRemaining = TRUE;
      }
      if (EnabledState != ENABLED_STATE_UNDEFINED) {
        pPayloadAlarmThresholds->Enable.Separated.PercentageRemaining = EnabledState;
      }
    }

    ReturnCode = FwCmdSetAlarmThresholds(pDimms[Index], pPayloadAlarmThresholds);
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
      }
    } else {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
    }
  }

Finish:
  FREE_POOL_SAFE(pDevCharacteristics);
  FREE_POOL_SAFE(pPayloadAlarmThresholds);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get NVM DIMM Health Info

  This FW command is used to retrieve current health of system, including SMART information:
  * Overall health status
  * Temperature
  * Spare blocks
  * Alarm Trips set (Temperature/Spare Blocks)
  * Device life span as a percentage
  * Latched Last shutdown status
  * Dirty shutdowns
  * Last shutdown time.
  * AIT DRAM status
  * Power Cycles (does not include warm resets or S3 resumes)
  * Power on time (life of DIMM has been powered on)
  * Uptime for current power cycle in seconds

  @param[in]  pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in]  DimmPid The ID of the DIMM
  @param[out] pHealthInfo - pointer to structure containing all Health and Smart variables

  @retval EFI_INVALID_PARAMETER if no DIMM found for DimmPid.
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_DEVICE_ERROR device error detected
  @retval EFI_NOT_READY the specified DIMM is unmanageable
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetSmartAndHealth (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmPid,
     OUT SMART_AND_HEALTH_INFO *pHealthInfo
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  DIMM *pDimm = NULL;
  PT_PAYLOAD_SMART_AND_HEALTH *pPayloadSmartAndHealth = NULL;
  PT_DEVICE_CHARACTERISTICS_OUT *pDevCharacteristics = NULL;

  NVDIMM_ENTRY();

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL || pHealthInfo == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    ReturnCode = EFI_NOT_READY;
    goto Finish;
  }

  ReturnCode = FwCmdGetSmartAndHealth(pDimm, &pPayloadSmartAndHealth);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = FwCmdDeviceCharacteristics(pDimm, &pDevCharacteristics);
  if (EFI_ERROR(ReturnCode) || pDevCharacteristics == NULL) {
    goto Finish;
  }

  /** Get common data **/
  pHealthInfo->PercentageRemainingValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.PercentageRemaining;
  pHealthInfo->MediaTemperatureValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.MediaTemperature;
  pHealthInfo->ControllerTemperatureValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.ControllerTemperature;
  pHealthInfo->MediaTemperature = TransformFwTempToRealValue(pPayloadSmartAndHealth->MediaTemperature);
  pHealthInfo->HealthStatus = pPayloadSmartAndHealth->HealthStatus;
  pHealthInfo->HealthStatusReason = (pPayloadSmartAndHealth->ValidationFlags.Separated.HealthStatusReason) ?
         pPayloadSmartAndHealth->HealthStatusReason : (UINT16)HEALTH_STATUS_REASON_NONE;
  pHealthInfo->PercentageRemaining = pPayloadSmartAndHealth->PercentageRemaining;
  pHealthInfo->LatchedLastShutdownStatus = pPayloadSmartAndHealth->LatchedLastShutdownStatus;
  /** Get Vendor specific data **/
  pHealthInfo->ControllerTemperature = TransformFwTempToRealValue(pPayloadSmartAndHealth->ControllerTemperature);
  pHealthInfo->UpTime = (UINT32)pPayloadSmartAndHealth->VendorSpecificData.UpTime;
  pHealthInfo->PowerCycles = pPayloadSmartAndHealth->VendorSpecificData.PowerCycles;
  pHealthInfo->PowerOnTime = (UINT32)pPayloadSmartAndHealth->VendorSpecificData.PowerOnTime;
  pHealthInfo->LatchedDirtyShutdownCount = pPayloadSmartAndHealth->LatchedDirtyShutdownCount;
  pHealthInfo->UnlatchedDirtyShutdownCount = pPayloadSmartAndHealth->VendorSpecificData.UnlatchedDirtyShutdownCount;
  pHealthInfo->MaxMediaTemperature = TransformFwTempToRealValue(pPayloadSmartAndHealth->VendorSpecificData.MaxMediaTemperature);
  pHealthInfo->MaxControllerTemperature = TransformFwTempToRealValue(pPayloadSmartAndHealth->VendorSpecificData.MaxControllerTemperature);

  /** Get Device Characteristics data **/
  pHealthInfo->ContrTempShutdownThresh =
      TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.ControllerShutdownThreshold);
  pHealthInfo->ControllerThrottlingStartThresh =
      TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.ControllerThrottlingStartThreshold);
  pHealthInfo->ControllerThrottlingStopThresh =
      TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.ControllerThrottlingStopThreshold);
  pHealthInfo->MediaTempShutdownThresh =
      TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.MediaShutdownThreshold);
  pHealthInfo->MediaThrottlingStartThresh =
      TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.MediaThrottlingStartThreshold);
  pHealthInfo->MediaThrottlingStopThresh =
      TransformFwTempToRealValue(pDevCharacteristics->Payload.Fis_2_01.MediaThrottlingStopThreshold);

  /** Check triggered alarms **/
  pHealthInfo->MediaTemperatureTrip = (pPayloadSmartAndHealth->AlarmTrips.Separated.MediaTemperature != 0);
  pHealthInfo->ControllerTemperatureTrip = (pPayloadSmartAndHealth->AlarmTrips.Separated.ControllerTemperature != 0);
  pHealthInfo->PercentageRemainingTrip = (pPayloadSmartAndHealth->AlarmTrips.Separated.PercentageRemaining != 0);

  /** Copy extended detail bits **/
  CopyMem_S(&pHealthInfo->LatchedLastShutdownStatusDetails, sizeof(LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED), pPayloadSmartAndHealth->VendorSpecificData.LatchedLastShutdownExtendedDetails.Raw, sizeof(LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED));
  /** Shift extended over, add the original 8 bits **/
  pHealthInfo->LatchedLastShutdownStatusDetails = (pHealthInfo->LatchedLastShutdownStatusDetails << sizeof(LAST_SHUTDOWN_STATUS_DETAILS) * 8)
    + pPayloadSmartAndHealth->VendorSpecificData.LatchedLastShutdownDetails.AllFlags;

  /** Copy extended detail bits **/
  CopyMem_S(&pHealthInfo->UnlatchedLastShutdownStatusDetails, sizeof(LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED), pPayloadSmartAndHealth->VendorSpecificData.UnlatchedLastShutdownExtendedDetails.Raw, sizeof(LAST_SHUTDOWN_STATUS_DETAILS_EXTENDED));
  /** Shift extended over, add the original 8 bits **/
  pHealthInfo->UnlatchedLastShutdownStatusDetails = (pHealthInfo->UnlatchedLastShutdownStatusDetails << sizeof(LAST_SHUTDOWN_STATUS_DETAILS) * 8)
    + pPayloadSmartAndHealth->VendorSpecificData.UnlatchedLastShutdownDetails.AllFlags;

  pHealthInfo->LastShutdownTime = pPayloadSmartAndHealth->VendorSpecificData.LastShutdownTime;

  pHealthInfo->AitDramEnabled = pPayloadSmartAndHealth->AITDRAMStatus;

  if ((pPayloadSmartAndHealth->ValidationFlags.Separated.AITDRAMStatus == 0) &&
    (pPayloadSmartAndHealth->HealthStatus < HealthStatusCritical)) {
    pHealthInfo->AitDramEnabled = AIT_DRAM_ENABLED;
  }

  pHealthInfo->ThermalThrottlePerformanceLossPrct = pPayloadSmartAndHealth->VendorSpecificData.ThermalThrottlePerformanceLossPercent;

  ReturnCode = FwCmdGetErrorCount(pDimm, &pHealthInfo->MediaErrorCount, &pHealthInfo->ThermalErrorCount);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  FREE_POOL_SAFE(pDevCharacteristics);
  FREE_POOL_SAFE(pPayloadSmartAndHealth);
  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

/**
  Set security state on multiple PMem modules.

  If there is a failure on one of the PMem modules, the function does not
  continue onto the remaining modules but exits with an error.

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] pDimmIds Pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] SecurityOperation Security Operation code
  @param[in] pPassphrase a pointer to string with current passphrase. For default Master Passphrase (0's) use a zero length, null terminated string.
  @param[in] pNewPassphrase a pointer to string with new passphrase
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER when pLockState is NULL
  @retval EFI_OUT_OF_RESOURCES couldn't allocate memory for a structure
  @retval EFI_UNSUPPORTED LockState to be set is not recognized, or mixed sku of DCPMMs detected
  @retval EFI_DEVICE_ERROR setting state for a DIMM failed
  @retval EFI_NOT_FOUND a DIMM was not found
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_SUCCESS security state correctly set
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
)
{
  EFI_STATUS ReturnCode = EFI_UNSUPPORTED;
  EFI_STATUS TempReturnCode = EFI_UNSUPPORTED;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT16 PayloadBufferSize = 0;
  UINT8 SubOpcode = 0;
  CHAR8 AsciiPassword[PASSPHRASE_BUFFER_SIZE + 1];
  UINT32 Index = 0;
  UINT32 DimmSecurityState = 0;
  PT_SET_SECURITY_PAYLOAD *pSecurityPayload = NULL;
  BOOLEAN NamespaceFound = FALSE;
  BOOLEAN AreNotPartOfPendingGoal = TRUE;
  BOOLEAN IsSupported = FALSE;
  BOOLEAN CheckSupportedConfigDimms = TRUE;
  REQUIRE_DCPMMS RequireDcpmmsBitfield = REQUIRE_DCPMMS_MANAGEABLE;
  DIMM *pCurrentDimm = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  LIST_ENTRY *pDimmList = NULL;
  UINT8 DimmARSStatus = 0;

  NVDIMM_ENTRY();

  SetMem(pDimms, sizeof(pDimms), 0x0);
  SetMem(AsciiPassword, sizeof(AsciiPassword), 0x0);

  IsSupported = IsSecurityOpSupported(SecurityOperation);
  if (!IsSupported) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED);
    goto Finish;
  }

  if (pCommandStatus == NULL) {
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  if (SecurityOperation != SECURITY_OPERATION_SET_PASSPHRASE &&
    SecurityOperation != SECURITY_OPERATION_CHANGE_PASSPHRASE &&
    SecurityOperation != SECURITY_OPERATION_DISABLE_PASSPHRASE &&
    SecurityOperation != SECURITY_OPERATION_UNLOCK_DEVICE &&
    SecurityOperation != SECURITY_OPERATION_ERASE_DEVICE &&
    SecurityOperation != SECURITY_OPERATION_FREEZE_DEVICE &&
    SecurityOperation != SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE &&
    SecurityOperation != SECURITY_OPERATION_MASTER_ERASE_DEVICE) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_SECURITY_OPERATION);
  }

  // Erase Device Data operation is supported for Dimms
  // excluded from POR config. For any other commands, the DCPMM needs
  // to be in a POR config.
  if (!(SecurityOperation == SECURITY_OPERATION_MASTER_ERASE_DEVICE
    || SecurityOperation == SECURITY_OPERATION_ERASE_DEVICE))
  {
    RequireDcpmmsBitfield |= REQUIRE_DCPMMS_NO_UNMAPPED_POPULATION_VIOLATION;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, RequireDcpmmsBitfield,
      pDimms, &DimmsNum, pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // Prevent user from enabling security when goal is pending due to BIOS restrictions
  if (SecurityOperation == SECURITY_OPERATION_SET_PASSPHRASE) {
    // Check if input DIMMs are not part of a goal
    ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE, CheckSupportedConfigDimms);
    if (EFI_ERROR(ReturnCode)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
      goto Finish;
    }
    pDimmList = &gNvmDimmData->PMEMDev.Dimms;
    // Loop through all specified dimms until a match is found
    for (Index = 0; (Index < DimmsNum) && (AreNotPartOfPendingGoal == TRUE); Index++) {
      UINT32 NodeCount = 0;
      LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
        pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
        if (pCurrentDimm == NULL) {
          NVDIMM_DBG("Failed on Get Dimm from node %d", NodeCount);
          ReturnCode = EFI_NOT_FOUND;
          AreNotPartOfPendingGoal = FALSE;
          goto Finish;
        }
        NodeCount++;
        // Valid match found and GoalConfigStatus is valid
        if ((pCurrentDimm->DimmID == pDimmIds[Index]) &&
          (pCurrentDimm->GoalConfigStatus != GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS)) {
            AreNotPartOfPendingGoal = FALSE;
            break;
        }
      }
    }
  }

  if (!AreNotPartOfPendingGoal) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_ENABLE_SECURITY_NOT_ALLOWED);
      ReturnCode = EFI_ACCESS_DENIED;
      goto Finish;
  }

  if ((pPassphrase != NULL && StrLen(pPassphrase) > PASSPHRASE_BUFFER_SIZE) ||
      (pNewPassphrase != NULL && StrLen(pNewPassphrase) > PASSPHRASE_BUFFER_SIZE)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_TOO_LONG);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  PayloadBufferSize = IN_PAYLOAD_SIZE;
  pSecurityPayload = AllocateZeroPool(PayloadBufferSize);
  if (pSecurityPayload == NULL) {
    NVDIMM_ERR("Out of memory");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (pPassphrase != NULL) {
    UnicodeStrToAsciiStrS(pPassphrase, AsciiPassword, PASSPHRASE_BUFFER_SIZE + 1);
    CopyMem_S(&pSecurityPayload->PassphraseCurrent, sizeof(pSecurityPayload->PassphraseCurrent), AsciiPassword, AsciiStrLen(AsciiPassword));
  }
  if (pNewPassphrase != NULL) {
    UnicodeStrToAsciiStrS(pNewPassphrase, AsciiPassword, PASSPHRASE_BUFFER_SIZE + 1);
    CopyMem_S(&pSecurityPayload->PassphraseNew, sizeof(pSecurityPayload->PassphraseNew), AsciiPassword, AsciiStrLen(AsciiPassword));
  }

  /**
    Iterate over DIMMs list, perform all checks and execute the command on each of them. If any check fails stop
    iteration and skip remaining DIMMs.
  **/
  for (Index = 0; Index < DimmsNum; Index++) {
    if (!IsDimmSkuSupported(pDimms[Index], SkuStandardSecuritySku)) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }
    ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
      }
      goto Finish;
    }

    if ((SecurityOperation == SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE) || (SecurityOperation == SECURITY_OPERATION_MASTER_ERASE_DEVICE)) {
      if (DimmSecurityState & SECURITY_MASK_MASTER_COUNTEXPIRED) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_MASTER_PP_COUNT_EXPIRED);
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }
    else {
      if (DimmSecurityState & SECURITY_MASK_COUNTEXPIRED) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_USER_PP_COUNT_EXPIRED);
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }

    /**
      Verify if operation is possible to perform in current device state
      and if required passphrases are provided
    **/
    if (SecurityOperation == SECURITY_OPERATION_SET_PASSPHRASE) {
      /**
        Set Passphrase (B->H state transition according to FW Interface Spec)
        SecurityState=NotEnabled,NotLocked,NotFrozen
      **/
      if (!(DimmSecurityState & SECURITY_MASK_ENABLED) &&
          !(DimmSecurityState & SECURITY_MASK_LOCKED) &&
          !(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        NVDIMM_DBG("Set initial passphrase");
        if (pNewPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        SubOpcode = SubopSetPass;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_CHANGE_PASSPHRASE) {
      /**
        Change Passphrase (H->H state transition according to FW Interface Spec)
        SecurityState=Enabled,NotLocked,NotFrozen -> LockState=Enabled
      **/
      if ((DimmSecurityState & SECURITY_MASK_ENABLED) &&
          !(DimmSecurityState & SECURITY_MASK_LOCKED) &&
          !(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        NVDIMM_DBG("Change passphrase");
        if (pPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        if (pNewPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        SubOpcode = SubopSetPass;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_DISABLE_PASSPHRASE) {
      /**
        Disable Passphrase (H->B state transition according to FW Interface Spec)
        SecurityState=Enabled,NotLocked,NotFrozen -> LockState=Disabled
      **/
      if ((DimmSecurityState & SECURITY_MASK_ENABLED) &&
          !(DimmSecurityState & SECURITY_MASK_LOCKED) &&
          !(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        NVDIMM_DBG("Disable passphrase");
        if (pPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        SubOpcode = SubopDisablePass;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_UNLOCK_DEVICE) {
      /**
        Unlock device (D->H state transition according to FW Interface Spec)
        SecurityState=Enabled,Locked,NotFrozen -> LockState=Unlocked
      **/
      if ((DimmSecurityState & SECURITY_MASK_ENABLED) &&
          (DimmSecurityState & SECURITY_MASK_LOCKED) &&
         !(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        if (pPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        SubOpcode = SubopUnlockUnit;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_ERASE_DEVICE) {
      /**
        Security Erase (H->E state transition according to FW Interface Spec)
        SecurityState=Enabled,NotLocked,NotFrozen -> LockState=Disabled
      **/
      if (DimmSecurityState & SECURITY_MASK_MASTER_ENABLED) {
        if (pPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
      }

      ReturnCode = FwCmdGetARS(pDimms[Index], &DimmARSStatus);
      if (LONG_OP_STATUS_IN_PROGRESS == DimmARSStatus) {
        NVDIMM_ERR("ARS in progress.\n");
        ResetCmdStatus(pCommandStatus, NVM_ERR_ARS_IN_PROGRESS);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

      ReturnCode = IsNamespaceOnDimms(&pDimms[Index], 1, &NamespaceFound);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      if (NamespaceFound) {
        ReturnCode = EFI_ABORTED;
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURE_ERASE_NAMESPACE_EXISTS);
        goto Finish;
      }

      if (!(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        SubOpcode = SubopSecEraseUnit;
        pSecurityPayload->PassphraseType = SECURITY_USER_PASSPHRASE;
#ifndef OS_BUILD
        /** Need to call WBINVD before secure erase **/
        AsmWbinvd();
#endif
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_FREEZE_DEVICE) {
      /**
        Freeze Lock (H->F1 or B->F2 state transition according to FW Interface Spec)
        SecurityState=Enabled,NotLocked,NotFrozen,Disabled & LockState=Frozen
       **/
      if (!(DimmSecurityState & SECURITY_MASK_FROZEN) &&
          !(DimmSecurityState & SECURITY_MASK_LOCKED)) {
        SubOpcode = SubopSecFreezeLock;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE) {
      /**
        SecurityState=Disabled,MasterPassphraseEnabled
        Master Passphrase count expired via NVM_ERR
      **/
      if (!(DimmSecurityState & SECURITY_MASK_MASTER_ENABLED)) {
        // starting with FIS 3.2 the enabled bit for master passphrase is not enabled until
        // the master passphrase is changed from default so skip this check because this might
        // be initial setting of the passphrase
        if ( ! (((3 == pDimms[Index]->FwVer.FwApiMajor) && (2 <= pDimms[Index]->FwVer.FwApiMinor)) ||
          (3 < pDimms[Index]->FwVer.FwApiMajor))) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_NOT_SUPPORTED);
          ReturnCode = EFI_UNSUPPORTED;
          goto Finish;
        }
      }

      if ((DimmSecurityState & SECURITY_MASK_MASTER_COUNTEXPIRED)) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_MASTER_PP_COUNT_EXPIRED);
        ReturnCode = EFI_SECURITY_VIOLATION;
        goto Finish;
      }

      if (!(DimmSecurityState & SECURITY_MASK_ENABLED)) {
        if (pPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        if (pNewPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_NEW_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        SubOpcode = SubopSetMasterPass;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else if (SecurityOperation == SECURITY_OPERATION_MASTER_ERASE_DEVICE) {
      /**
        Security Erase (H->E state transition according to FW Interface Spec)
        SecurityState=MasterPassphraseEnabled,Enabled,NotLocked,NotFrozen -> LockState=Disabled
        NotSecurityCountExpired
      **/
      if (!(DimmSecurityState & SECURITY_MASK_MASTER_ENABLED)) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_NOT_SUPPORTED);
        ReturnCode = EFI_UNSUPPORTED;
        goto Finish;
      }

      if ((DimmSecurityState & SECURITY_MASK_MASTER_COUNTEXPIRED)) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_MASTER_PP_COUNT_EXPIRED);
        ReturnCode = EFI_SECURITY_VIOLATION;
        goto Finish;
      }

      if (!(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        if (pPassphrase == NULL) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_PASSPHRASE_NOT_PROVIDED);
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }

        SubOpcode = SubopSecEraseUnit;
        pSecurityPayload->PassphraseType = SECURITY_MASTER_PASSPHRASE;
#ifndef OS_BUILD
        /** Need to call WBINVD before secure erase **/
        AsmWbinvd();
#endif
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

    } else {
      ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_SECURITY_OPERATION);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }

    ReturnCode = SetDimmSecurityState(pDimms[Index], PtSetSecInfo, SubOpcode, PayloadBufferSize,
        pSecurityPayload, PT_TIMEOUT_INTERVAL);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on SetDimmSecurityState, ReturnCode=" FORMAT_EFI_STATUS "", ReturnCode);
      switch (ReturnCode) {
      case EFI_ACCESS_DENIED:
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_PASSPHRASE);
        break;
      case EFI_NO_RESPONSE:
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_BUSY_DEVICE);
        break;
      case EFI_UNSUPPORTED:
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_NOT_SUPPORTED);
        break;
      case EFI_NOT_STARTED:
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_MASTER_PASSPHRASE_NOT_SET);
        break;
      default:
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
        break;
      }
      goto Finish;
    }
#ifndef OS_BUILD
    /** Need to call WBINVD after unlock or secure erase **/
    if (SecurityOperation == SECURITY_OPERATION_ERASE_DEVICE ||
        SecurityOperation == SECURITY_OPERATION_UNLOCK_DEVICE) {
      AsmWbinvd();
    }
#endif
    /** @todo(check on real HW)
      WARNING: SetDimmSecurityState will not return EFI_ERROR on SECURITY_MASK_COUNTEXPIRED
      so we have to check it additionally with GetDimmSecurityState.
      It could be Simics/FW bug. Check how it is done on real HW.
    **/
    ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmSecurityState, ReturnCode=" FORMAT_EFI_STATUS "", ReturnCode);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
    else if ((SecurityOperation == SECURITY_OPERATION_CHANGE_MASTER_PASSPHRASE) || (SecurityOperation == SECURITY_OPERATION_MASTER_ERASE_DEVICE)) {
      if (DimmSecurityState & SECURITY_MASK_MASTER_COUNTEXPIRED) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_MASTER_PP_COUNT_EXPIRED);
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }
    else {
      if (DimmSecurityState & SECURITY_MASK_COUNTEXPIRED) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_USER_PP_COUNT_EXPIRED);
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }

    SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);

    pDimms[Index]->EncryptionEnabled = ((DimmSecurityState & SECURITY_MASK_ENABLED) != 0);
  }

  if (!EFI_ERROR(ReturnCode)) {
    SetCmdStatus(pCommandStatus, NVM_SUCCESS);
  }

Finish:
  if (SecurityOperation == SECURITY_OPERATION_UNLOCK_DEVICE || SecurityOperation == SECURITY_OPERATION_ERASE_DEVICE ||
    SecurityOperation == SECURITY_OPERATION_MASTER_ERASE_DEVICE) {
    TempReturnCode = ReenumerateNamespacesAndISs(TRUE);
    if (EFI_ERROR(TempReturnCode)) {
      NVDIMM_DBG("Unable to re-enumerate namespace on unlocked DIMMs. ReturnCode=" FORMAT_EFI_STATUS "", TempReturnCode);
    }
  }

  CleanStringMemory(AsciiPassword);
  FREE_POOL_SAFE(pSecurityPayload);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve an SMBIOS table type 17 or type 20 for a specific DIMM

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 Pid,
  IN     UINT8 Type,
     OUT SMBIOS_STRUCTURE_POINTER *pTable
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifndef OS_BUILD

  SMBIOS_STRUCTURE_POINTER DmiPhysicalDev;
  SMBIOS_STRUCTURE_POINTER DmiDeviceMappedAddr;
  SMBIOS_VERSION SmbiosVersion;
  DIMM *pDimm = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&DmiPhysicalDev, sizeof(DmiPhysicalDev));
  ZeroMem(&DmiDeviceMappedAddr, sizeof(DmiDeviceMappedAddr));
  ZeroMem(&SmbiosVersion, sizeof(SmbiosVersion));

  /* check table pointer */
  if (pTable == NULL) {
    NVDIMM_DBG("pTable is NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /* Ensure that the Dimm Pid is valid */
  pDimm = GetDimmByPid(Pid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    NVDIMM_DBG("Failed to retrieve the DIMM with pid %x", Pid);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Dynamically retrieve SMBIOS tables from gST **/
  ReturnCode = GetDmiMemdevInfo(Pid, &DmiPhysicalDev, &DmiDeviceMappedAddr, &SmbiosVersion);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failure to retrieve SMBIOS tables");
    goto Finish;
  }

  /* populate the table */
  if (Type == SMBIOS_TYPE_MEM_DEV) {
    if (DmiPhysicalDev.Raw != NULL) {
      pTable->Raw = DmiPhysicalDev.Raw;
    } else {
      NVDIMM_ERR("SMBIOS type 17 table doesn't exist for Dimm 0x%04x", Pid);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
  } else if (Type == SMBIOS_TYPE_MEM_DEV_MAPPED_ADDR) {
    if (DmiDeviceMappedAddr.Raw != NULL) {
      pTable->Raw = DmiDeviceMappedAddr.Raw;
    } else {
      NVDIMM_ERR("SMBIOS type 20 table doesn't exist for Dimm 0x%04x", Pid);
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }
  } else {
    NVDIMM_DBG("Type %d is not valid", Type);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  ReturnCode = EFI_SUCCESS;
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
#endif
  return ReturnCode;
}

/**
  Retrieve the NFIT ACPI table

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppNFit A pointer to the output NFIT table

  @retval EFI_SUCCESS Ok
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
  @retval EFI_NOT_FOUND PCAT tables not found
  @retval EFI_INVALID_PARAMETER pNFit is NULL
**/
EFI_STATUS
EFIAPI
GetAcpiNFit (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT ParsedFitHeader **ppNFit
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (ppNFit == NULL) {
    goto Finish;
  }
  if (gNvmDimmData->PMEMDev.pFitHead != NULL) {
    *ppNFit = gNvmDimmData->PMEMDev.pFitHead;
    ReturnCode = EFI_SUCCESS;
  } else {
    NVDIMM_ERR("NFIT does not exist");
    ReturnCode = EFI_NOT_FOUND;
  }

Finish:
  return ReturnCode;
}

/**
  Retrieve the PCAT ACPI table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] ppPcat output buffer with PCAT tables

  @retval EFI_SUCCESS Ok
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
  @retval EFI_NOT_FOUND PCAT tables not found
**/
EFI_STATUS
EFIAPI
GetAcpiPcat (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT ParsedPcatHeader **ppPcat
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (ppPcat == NULL) {
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead != NULL) {
    *ppPcat = gNvmDimmData->PMEMDev.pPcatHead;
    ReturnCode = EFI_SUCCESS;
  } else {
    NVDIMM_DBG("PCAT does not exist");
    ReturnCode = EFI_NOT_FOUND;
  }

Finish:
  return ReturnCode;
}

/**
Retrieve the PMTT ACPI table

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
@param[out] ppPMTT output buffer with PMTT tables. This buffer must be freed by caller.

@retval EFI_SUCCESS Ok
@retval EFI_OUT_OF_RESOURCES Problem with allocating memory
@retval EFI_NOT_FOUND PCAT tables not found
**/
EFI_STATUS
EFIAPI
GetAcpiPMTT(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  OUT VOID **ppPMTT
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifdef OS_BUILD
  UINT32 Size = 0;
  PbrContext *pContext = PBR_CTX();
#endif
  if (ppPMTT == NULL) {
    goto Finish;
  }

#ifdef OS_BUILD
  if (PBR_PLAYBACK_MODE == PBR_GET_MODE(pContext)) {
    ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_PMTT, ppPMTT, &Size);
  }
  else {
    ReturnCode = get_pmtt_table((EFI_ACPI_DESCRIPTION_HEADER**)ppPMTT, &Size);
  }
  if (EFI_ERROR(ReturnCode) || NULL == *ppPMTT) {
    NVDIMM_DBG("Failed to retrieve PMTT table.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
#else
  EFI_ACPI_DESCRIPTION_HEADER *pTempPMTT = NULL;
  UINT32 PmttTableLength = 0;
  CHECK_RESULT(GetAcpiTables(gST, NULL, NULL, &pTempPMTT), Finish);

  // Allocate and copy the buffer to be consistent with OS call
  PmttTableLength = pTempPMTT->Length;
  *ppPMTT = AllocatePool(PmttTableLength);
  if (NULL == *ppPMTT) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  CopyMem_S(*ppPMTT, PmttTableLength, pTempPMTT, PmttTableLength);

#endif

Finish:
  return ReturnCode;
}

/**
  Get Platform Config Data

  The caller is responsible for freeing ppDimmPcdInfo by using FreeDimmPcdInfoArray.

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] PcdTarget Target PCD partition: ALL=0, CONFIG=1, NAMESPACES=2
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[out] ppDimmPcdInfo Pointer to output array of PCDs
  @param[out] pDimmPcdInfoCount Number of items in Dimm PCD Info
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more input parameters are NULL
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsCount = 0;
  UINT32 Index = 0;
  UINT32 IndexNew = 0;
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  LABEL_STORAGE_AREA *pLabelStorageArea = NULL;

  NVDIMM_ENTRY();

  ZeroMem(pDimms, sizeof(pDimms));

  if (pThis == NULL || ppDimmPcdInfo == NULL || pDimmPcdInfoCount == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  //Set initial value of *ppDimmPcdInfo
  *ppDimmPcdInfo = NULL;

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0,
    REQUIRE_DCPMMS_MANAGEABLE, pDimms, &DimmsCount,
      pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  // Redo the VerifyTargetDimms output to not include media inacessible modules
  // and set the object status accordingly for these modules.
  // This will be re-done in VerifyTargetDimms in the near future
  IndexNew = 0;
  for (Index = 0; Index < DimmsCount; Index++) {
    if (DIMM_MEDIA_NOT_ACCESSIBLE(pDimms[Index]->BootStatusBitmask)) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_MEDIA_NOT_ACCESSIBLE);
      continue;
    }
    pDimms[IndexNew] = pDimms[Index];
    IndexNew++;
  }
  DimmsCount = IndexNew;

  *ppDimmPcdInfo = AllocateZeroPool(sizeof(**ppDimmPcdInfo) * DimmsCount);
  if (*ppDimmPcdInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  for (Index = 0; Index < DimmsCount; Index++) {
    pPcdConfHeader = NULL;
    pLabelStorageArea = NULL;
    (*ppDimmPcdInfo)[Index].DimmId = pDimms[Index]->DeviceHandle.AsUint32;

    ReturnCode = GetDimmUid(pDimms[Index], (*ppDimmPcdInfo)[Index].DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_CONFIG) {
      ReturnCode = GetPlatformConfigDataOemPartition(pDimms[Index], FALSE, &pPcdConfHeader);
      if (ReturnCode == EFI_NO_MEDIA) {
        continue;
      }
  #ifdef MEMORY_CORRUPTION_WA
      if (ReturnCode == EFI_DEVICE_ERROR)
      {
        ReturnCode = GetPlatformConfigDataOemPartition(pDimms[Index], FALSE, &pPcdConfHeader);
      }
  #endif // MEMORY_CORRUPTIO_WA
      if (EFI_ERROR(ReturnCode)) {
        if (ReturnCode == EFI_NO_RESPONSE) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
          goto Finish;
        }
        NVDIMM_DBG("GetPlatformConfigDataOemPartition returned: " FORMAT_EFI_STATUS "", ReturnCode);
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_GET_PCD_FAILED);
        goto Finish;
      }
      (*ppDimmPcdInfo)[Index].pConfHeader = pPcdConfHeader;
    }

    if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_NAMESPACES)
    {
  #ifndef OS_BUILD
      if (pDimms[Index]->LsaStatus == LSA_NOT_INIT || pDimms[Index]->LsaStatus == LSA_COULD_NOT_READ_NAMESPACES) {
        continue;
      }
  #endif // OS_BUILD
      ReturnCode = ReadLabelStorageArea(pDimms[Index]->DimmID, &pLabelStorageArea);
  #ifdef OS_BUILD
      if (ReturnCode == EFI_NOT_FOUND) {
        NVDIMM_DBG("LSA not found on DIMM 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
        pDimms[Index]->LsaStatus = LSA_NOT_INIT;
        continue;
      }
      else if (ReturnCode == EFI_ACCESS_DENIED) {
        NVDIMM_DBG("LSA not found on DIMM 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
        pDimms[Index]->LsaStatus = LSA_COULD_NOT_READ_NAMESPACES;
        continue;
      }
      else if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_ACCESS_DENIED) {
        pDimms[Index]->LsaStatus = LSA_CORRUPTED;
        /**
        If the LSA is corrupted, we do nothing - it may be a driver mismach between UEFI and the OS,
        so we don't want to "kill" a valid configuration
        **/
        NVDIMM_DBG("LSA corrupted on DIMM 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
        NVDIMM_DBG("Error in retrieving the LSA: " FORMAT_EFI_STATUS "", ReturnCode);
        goto Finish;
      }
      pDimms[Index]->LsaStatus = LSA_OK;
  #else // OS_BUILD
      if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_ACCESS_DENIED) {
        NVDIMM_DBG("Error in retrieving the LSA: " FORMAT_EFI_STATUS "", ReturnCode);
        goto Finish;
      }
  #endif // OS_BUILD
      (*ppDimmPcdInfo)[Index].pLabelStorageArea = pLabelStorageArea;
    }
  }

  *pDimmPcdInfoCount = DimmsCount;

  ReturnCode = EFI_SUCCESS;
  pCommandStatus->GeneralStatus = NVM_SUCCESS;

Finish:
  if (EFI_ERROR(ReturnCode) && ppDimmPcdInfo != NULL && *ppDimmPcdInfo != NULL && pDimmPcdInfoCount != NULL) {
    FreeDimmPcdInfoArray(*ppDimmPcdInfo, *pDimmPcdInfoCount);
    *ppDimmPcdInfo = NULL;
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Modifies select partition data from the PCD

@param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
@param[in] pDimmIds Pointer to an array of DIMM IDs
@param[in] DimmIdsCount Number of items in array of DIMM IDs
@param[in] ConfigIdMask Bitmask that defines which config to delete
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
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS TmpReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsCount = 0;
  UINT32 Index = 0;
  UINT32 SecurityState = 0;
  NVDIMM_CONFIGURATION_HEADER *pConfigHeader = NULL;
  UINT32 ConfigSize = 0;

  NVDIMM_ENTRY();

  ZeroMem(pDimms, sizeof(pDimms));

  if (pThis == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  //only allow valid config id options
  if (0x0 == ConfigIdMask || 0x0 != (ConfigIdMask & ~DELETE_PCD_CONFIG_ALL_MASK)) {
    goto Finish;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0,
    REQUIRE_DCPMMS_MANAGEABLE, pDimms, &DimmsCount,
    pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  //iterate through all dimms
  for (Index = 0; Index < DimmsCount; Index++) {

    // Reject dimms that are media inaccessible
    // TODO: Move into VerifyTargetDimms
    if (DIMM_MEDIA_NOT_ACCESSIBLE(pDimms[Index]->BootStatusBitmask)) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_MEDIA_NOT_ACCESSIBLE_CANNOT_CONTINUE);
      ReturnCode = EFI_UNSUPPORTED;
      continue;
    }

    TmpReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &SecurityState);
    if (EFI_ERROR(TmpReturnCode)) {
      KEEP_ERROR(ReturnCode, TmpReturnCode);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
      NVDIMM_DBG("Failed to get DIMM security state");
      goto Finish;
    }

    //don't allow deleting anything from PCD if device is locked
    if (!IsConfiguringAllowed(SecurityState)) {
      KEEP_ERROR(ReturnCode, EFI_ACCESS_DENIED);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      NVDIMM_DBG("Locked DIMM discovered : 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    //zero LSA
    if (ConfigIdMask & DELETE_PCD_CONFIG_LSA_MASK) {
      TmpReturnCode = ZeroLabelStorageArea(pDimms[Index]->DimmID);
      if (EFI_ERROR(TmpReturnCode)) {
        KEEP_ERROR(ReturnCode, TmpReturnCode);
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
        NVDIMM_DBG("Error in zero-ing the LSA: " FORMAT_EFI_STATUS "", TmpReturnCode);
        continue;
      }
      else {
        NVDIMM_DBG("Zero'ed the LSA on DIMM : 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
      }
    }

    //if any of the PCD configuration bits were set
    if (ConfigIdMask & (DELETE_PCD_CONFIG_CIN_MASK | DELETE_PCD_CONFIG_COUT_MASK | DELETE_PCD_CONFIG_CCUR_MASK)) {

      FREE_POOL_SAFE(pConfigHeader);
      //read partition 1 where CCUR/CIN/COUT resides
      //we are only going to modify the DATA SIZE and START OFFSET values in the header before writing it back out
      TmpReturnCode = GetPlatformConfigDataOemPartition(pDimms[Index], TRUE, &pConfigHeader);
      if (EFI_ERROR(TmpReturnCode)) {
        KEEP_ERROR(ReturnCode, TmpReturnCode);
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_GET_PCD_FAILED);
        NVDIMM_DBG("Failed to get PCD");
        continue;
      }

      //determine the size of the PCD partition, which will be used at the end to write the partition back to PCD
      TmpReturnCode = GetPcdOemDataSize(pConfigHeader, &ConfigSize);
      if (EFI_ERROR(TmpReturnCode)) {
        KEEP_ERROR(ReturnCode, TmpReturnCode);
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
        NVDIMM_DBG("Failed to get PCD size");
        continue;
      }

      //clear CIN
      if (ConfigIdMask & DELETE_PCD_CONFIG_CIN_MASK) {
        pConfigHeader->ConfInputDataSize = 0x0;
        pConfigHeader->ConfInputStartOffset = 0x0;
      }

      //clear COUT
      if (ConfigIdMask & DELETE_PCD_CONFIG_COUT_MASK) {
        pConfigHeader->ConfOutputDataSize = 0x0;
        pConfigHeader->ConfOutputStartOffset = 0x0;
      }

      //clear CCUR
      if (ConfigIdMask & DELETE_PCD_CONFIG_CCUR_MASK) {
        pConfigHeader->CurrentConfDataSize = 0x0;
        pConfigHeader->CurrentConfStartOffset = 0x0;
      }

      //values in partition have changed so we need to recalculate the checksum before writing back to PCD
      GenerateChecksum(pConfigHeader, pConfigHeader->Header.Length, PCAT_TABLE_HEADER_CHECKSUM_OFFSET);

      //write full partition 1 back to PCD with updated values
      TmpReturnCode = SetPlatformConfigDataOemPartition(pDimms[Index], pConfigHeader, ConfigSize);
      if (EFI_ERROR(TmpReturnCode)) {
        KEEP_ERROR(ReturnCode, TmpReturnCode);
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
        NVDIMM_DBG("Failed to set PCD");
        continue;
      }
    }
    SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
  }

  ReenumerateNamespacesAndISs(TRUE);

Finish:
  FREE_POOL_SAFE(pConfigHeader);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Extracts from the region structure the minimal data that
is needed by the CLI.

@param[in] pRegion pointer to the Region structure
@param[out] pRegionMin pointer to the minimal region data structure

@retval EFI_SUCCESS when everything succeeds.
@retval EFI_INVALID_ARGUMENT if at least one of the input arguments
equals NULL.
**/
STATIC
EFI_STATUS
GetRegionMinimalInfo(
  IN  NVM_IS *pRegion,
  OUT REGION_INFO *pRegionMin
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  DIMM *pDimm = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  LIST_ENTRY *pDimmRegionNode = NULL;

  NVDIMM_ENTRY();

  if (pRegion == NULL || pRegionMin == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pRegionMin->RegionId = pRegion->RegionId;
  pRegionMin->SocketId = pRegion->SocketId;
  DetermineRegionType(pRegion, &pRegionMin->RegionType);
  pRegionMin->Capacity = pRegion->Size;
  if (pRegion->InterleaveSetCookie != 0) {
    pRegionMin->CookieId = pRegion->InterleaveSetCookie;
  } else {
    pRegionMin->CookieId = pRegion->InterleaveSetCookie_1_1;
  }

  ReturnCode = DetermineRegionHealth(pRegion, &pRegionMin->Health);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = GetFreeRegionCapacity(pRegion, &pRegionMin->FreeCapacity);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  ReturnCode = ADNamespaceMinAndMaxAvailableSizeOnIS(pRegion, &pRegionMin->AppDirNamespaceMinSize,
    &pRegionMin->AppDirNamespaceMaxSize);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pRegionMin->DimmIdCount = 0;

  LIST_FOR_EACH(pDimmRegionNode, &pRegion->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pDimmRegionNode);
    pDimm = pDimmRegion->pDimm;
    pRegionMin->DimmId[pRegionMin->DimmIdCount] = (UINT16)pDimm->DeviceHandle.AsUint32;
    pRegionMin->DimmIdCount++;
  }
  BubbleSort(pRegionMin->DimmId, pRegionMin->DimmIdCount, sizeof(pRegionMin->DimmId[0]), SortRegionDimmId);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Retrieve the number of regions in the system

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] UseNfit flag to indicate NFIT usage
  @param[out] pCount The number of regions found.

  @retval EFI_SUCCESS  The count was returned properly
  @retval EFI_INVALID_PARAMETER pCount is NULL.
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
EFI_STATUS
EFIAPI
GetRegionCount(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     BOOLEAN UseNfit,
     OUT UINT32 *pCount
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  LIST_ENTRY *pRegionNode = NULL;
  LIST_ENTRY *pRegionList = NULL;


  NVDIMM_ENTRY();
  /**
  check input parameters
  **/
  if (pCount == NULL) {
    NVDIMM_DBG("pCount is NULL");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pCount = 0;
  Rc = ReenumerateNamespacesAndISs(FALSE);
  if (EFI_ERROR(Rc)) {
    if (EFI_NO_RESPONSE == Rc) {
      goto Finish;
    }
  }

  Rc = GetRegionList(&pRegionList, UseNfit);
  if (EFI_NO_RESPONSE == Rc) {
    goto Finish;
  }

  LIST_FOR_EACH(pRegionNode, pRegionList) {
    (*pCount)++;
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Sorts the region list by Id

  @param[in out] pRegion1 A pointer to the Regions.
  @param[in out] pRegion2 A pointer to the copy of Regions.

  @retval int returns 0,-1, 0
**/
INT32 SortRegionInfoById(VOID *pRegion1, VOID *pRegion2)
{
  REGION_INFO *pRegionA = (REGION_INFO *)pRegion1;
  REGION_INFO *pRegionB = (REGION_INFO *)pRegion2;

  if (pRegionA->RegionId == pRegionB->RegionId) {
    return 0;
  } else if (pRegionA->RegionId < pRegionB->RegionId) {
    return -1;
  } else {
    return 1;
  }
}

/**
  Sorts the DimmIds list by Id

  @param[in out] pDimmId1 A pointer to the pDimmId list.
  @param[in out] pDimmId2 A pointer to the copy of pDimmId list.

  @retval int returns 0,-1, 0
**/
INT32 SortRegionDimmId(VOID *pDimmId1, VOID *pDimmId2)
{
  if (*(UINT16 *)pDimmId1 == *(UINT16 *)pDimmId2) {
    return 0;
  } else if (*(UINT16 *)pDimmId1 < *(UINT16 *)pDimmId2) {
    return -1;
  } else {
    return 1;
  }
}

/**
  Retrieve the region list

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] Count The number of regions.
  @param[in] UseNfit flag to indicate NFIT usage
  @param[out] pRegions The region list
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS  The region list was returned properly
  @retval EFI_INVALID_PARAMETER pRegions is NULL.
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_NO_RESPONSE FW busy on one or more dimms
**/
EFI_STATUS
EFIAPI
GetRegions(
  IN    EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN    UINT32 Count,
  IN    BOOLEAN UseNfit,
  OUT   REGION_INFO *pRegions,
  OUT   COMMAND_STATUS *pCommandStatus
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  NVM_IS *pCurRegion = NULL;
  LIST_ENTRY *pCurRegionNode = NULL;
  LIST_ENTRY *pRegionList = NULL;

  NVDIMM_ENTRY();

  //This would be just MAX_REGIONS, except there is some HII constraint
  //making that figure artificially low. Will update when resolved.
  if (Count > MAX_SOCKETS * MAX_REGIONS_PER_SOCKET) {
    Rc = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    NVDIMM_WARN("Count passed exceeds the platform maximum region count.");
    goto Finish;
  }

  Rc = GetRegionList(&pRegionList, UseNfit);
  if (EFI_ERROR(Rc)) {
    if (EFI_NO_RESPONSE == Rc) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    goto Finish;
  }
  /**
    check input parameters
  **/
  if (pRegions == NULL || pCommandStatus == NULL) {
    NVDIMM_DBG("Invalid parameter");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

#ifdef OS_BUILD
  Rc = InitializeNamespaces();
  if (EFI_ERROR(Rc)) {
    NVDIMM_WARN("Failed to initialize Namespaces, error = " FORMAT_EFI_STATUS ".", Rc);
    Rc = EFI_SUCCESS; // we don't want to fail the GetRegions function in case the Namespace Initialization error
  }
#endif

  ZeroMem(pRegions, sizeof(*pRegions) * Count);

  LIST_FOR_EACH(pCurRegionNode, pRegionList) {
    pCurRegion = IS_FROM_NODE(pCurRegionNode);

    if (Count <= Index) {
      NVDIMM_DBG("Array is too small to hold entire region list");
      Rc = EFI_INVALID_PARAMETER;
      break;
    }

    Rc = GetRegionMinimalInfo(pCurRegion, &pRegions[Index]);
    if (EFI_ERROR(Rc)) {
      NVDIMM_WARN("Failed to get region minimal info, error = " FORMAT_EFI_STATUS ".", Rc);
      goto Finish;
    }
    Index++;
  }

  BubbleSort(pRegions, Count, sizeof(*pRegions), SortRegionInfoById);

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Retrieve the details about the region specified with region id

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] RegionId The region id of the region to retrieve
  @param[out] pRegion A pointer to the region
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS The region was returned properly
  @retval EFI_INVALID_PARAMETER pRegion is NULL
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
**/
EFI_STATUS
EFIAPI
GetRegion(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 RegionId,
     OUT REGION_INFO *pRegionInfo,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVM_IS *pRegion = NULL;
  LIST_ENTRY *pRegionList = NULL;
  NVDIMM_ENTRY();
  Rc = ReenumerateNamespacesAndISs(FALSE);
  if (EFI_ERROR(Rc)) {
    if ((Rc == EFI_NOT_FOUND && IsLsaNotInitializedOnADimm()))
    {
      NVDIMM_WARN("Failure to refresh Namespaces is because LSA not initialized");
    }
    else
    {
      goto Finish;
    }
  }
  Rc = GetRegionList(&pRegionList, FALSE);
  if (pRegionList == NULL) {
    goto Finish;
  }
  Rc = EFI_SUCCESS;

  if (pRegionInfo == NULL || pCommandStatus == NULL) {
    NVDIMM_DBG("Invalid parameter");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ZeroMem(pRegionInfo, sizeof(*pRegionInfo));
  pRegion = GetRegionById(pRegionList, RegionId);
  if (pRegion == NULL) {
    NVDIMM_DBG("There is no %d region id on the list of regions", RegionId);
    Rc = EFI_INVALID_PARAMETER;
  } else {
   GetRegionMinimalInfo(pRegion, pRegionInfo);
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Gather info about total capacities on all dimms

  @param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[out] pMemoryResourcesInfo structure filled with required information

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_ABORTED PCAT tables not found
  @retval Other errors failure of FW commands
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetMemoryResourcesInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT MEMORY_RESOURCES_INFO *pMemoryResourcesInfo
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS PreservedReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();
  CHECK_NULL_ARG(pThis, Finish);
  CHECK_NULL_ARG(pMemoryResourcesInfo, Finish);

  // Make sure we start with 00 values
  ZeroMem(pMemoryResourcesInfo, sizeof(*pMemoryResourcesInfo));

  ReturnCode = ReenumerateNamespacesAndISs(TRUE);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to refresh Namespaces and Interleave Sets information");
#ifdef OS_BUILD
    goto Finish;
#else
    if ((ReturnCode == EFI_NOT_FOUND && IsLsaNotInitializedOnADimm()))
    {
      NVDIMM_WARN("Failure to refresh Namespaces is because LSA not initialized");
    }
    else
    {
      goto Finish;
    }
#endif
  }
  // Don't fail out *immediately* if one of them fails. Fail out at the end

  // Get DCPMM Sizes
  CHECK_RESULT_CONTINUE_PRESERVE_ERROR(GetTotalDcpmmCapacities(&gNvmDimmData->PMEMDev.Dimms, &pMemoryResourcesInfo->RawCapacity, &pMemoryResourcesInfo->VolatileCapacity,
    &pMemoryResourcesInfo->AppDirectCapacity, &pMemoryResourcesInfo->UnconfiguredCapacity, &pMemoryResourcesInfo->ReservedCapacity,
    &pMemoryResourcesInfo->InaccessibleCapacity));
  if (EFI_ERROR(ReturnCode)) {
    CHECK_RESULT_CONTINUE_PRESERVE_ERROR(CheckIfAllDimmsConfigured(&gNvmDimmData->PMEMDev.Dimms, &pMemoryResourcesInfo->PcdInvalid, NULL));
  }

  // Get DDR Sizes
  CHECK_RESULT_CONTINUE_PRESERVE_ERROR(GetDDRCapacities(SOCKET_ID_ALL, &pMemoryResourcesInfo->DDRRawCapacity, &pMemoryResourcesInfo->DDRCacheCapacity,
    &pMemoryResourcesInfo->DDRVolatileCapacity, &pMemoryResourcesInfo->DDRInaccessibleCapacity));

Finish:
  if (EFI_ERROR(PreservedReturnCode) && !EFI_ERROR(ReturnCode)) {
    ReturnCode = PreservedReturnCode;
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Gather info about performance on all dimms

@param[in] pThis a pointer to EFI_DCPMM_CONFIG2_PROTOCOL instance
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
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
    OUT UINT32 *pDimmCount,
  OUT DIMM_PERFORMANCE_DATA **pDimmsPerformanceData
)
{
    EFI_STATUS ReturnCode = EFI_SUCCESS;
    DIMM *pDimm = NULL;
    LIST_ENTRY *pDimmNode = NULL;
    UINT32 Index = 0;
    PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE0 *pPayloadMemInfoPage0 = NULL;
    PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE1 *pPayloadMemInfoPage1 = NULL;

    NVDIMM_ENTRY();

    if ((NULL == pThis) || (NULL == pDimmCount) || (NULL == pDimmsPerformanceData)) {
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
    }

    // Get total DIMM count and allocate memory for the table
    ReturnCode = GetListSize(&gNvmDimmData->PMEMDev.Dimms, pDimmCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_ERR("Failed to get DIMM count; Return code 0x%08x", ReturnCode);
      ReturnCode = EFI_DEVICE_ERROR;
      goto Finish;
    }

    if(NULL == (*pDimmsPerformanceData = AllocateZeroPool(sizeof(DIMM_PERFORMANCE_DATA) * (*pDimmCount)))) {
        NVDIMM_ERR("Memory allocation failure");
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
    }

    LIST_FOR_UNTIL_INDEX(pDimmNode, &gNvmDimmData->PMEMDev.Dimms, *pDimmCount, Index) {
        pDimm = DIMM_FROM_NODE(pDimmNode);

        if (!IsDimmManageable(pDimm)) {
            NVDIMM_WARN("Dimm 0x%x is not manageable", pDimm->DeviceHandle.AsUint32);
            continue;
        }
        (*pDimmsPerformanceData)[Index].DimmId = pDimm->DimmID;

        // Get Dimm Performance data
        ReturnCode = FwCmdGetMemoryInfoPage(pDimm, MEMORY_INFO_PAGE_0,
            sizeof(PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE0), (VOID **)&pPayloadMemInfoPage0);
        if (EFI_ERROR(ReturnCode)) {
            NVDIMM_ERR("Could not read the memory info page 0; Return code 0x%08x", ReturnCode);
            ReturnCode = EFI_DEVICE_ERROR;
            FREE_POOL_SAFE(*pDimmsPerformanceData);
            goto Finish;
        }
        ReturnCode = FwCmdGetMemoryInfoPage(pDimm, MEMORY_INFO_PAGE_1,
            sizeof(PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE1), (VOID **)&pPayloadMemInfoPage1);
        if (EFI_ERROR(ReturnCode)) {
            NVDIMM_ERR("Could not read the memory info page 1; Return code 0x%08x", ReturnCode);
            ReturnCode = EFI_DEVICE_ERROR;
            FREE_POOL_SAFE(*pDimmsPerformanceData);
            goto Finish;
        }

        // Copy the data
        (*pDimmsPerformanceData)[Index].MediaReads = pPayloadMemInfoPage0->MediaReads;
        (*pDimmsPerformanceData)[Index].MediaWrites = pPayloadMemInfoPage0->MediaWrites;
        (*pDimmsPerformanceData)[Index].ReadRequests = pPayloadMemInfoPage0->ReadRequests;
        (*pDimmsPerformanceData)[Index].WriteRequests = pPayloadMemInfoPage0->WriteRequests;
        (*pDimmsPerformanceData)[Index].TotalMediaReads = pPayloadMemInfoPage1->TotalMediaReads;
        (*pDimmsPerformanceData)[Index].TotalMediaWrites = pPayloadMemInfoPage1->TotalMediaWrites;
        (*pDimmsPerformanceData)[Index].TotalReadRequests = pPayloadMemInfoPage1->TotalReadRequests;
        (*pDimmsPerformanceData)[Index].TotalWriteRequests = pPayloadMemInfoPage1->TotalWriteRequests;

        FREE_POOL_SAFE(pPayloadMemInfoPage0);
        FREE_POOL_SAFE(pPayloadMemInfoPage1);
    }

Finish:
    FREE_POOL_SAFE(pPayloadMemInfoPage0);
    FREE_POOL_SAFE(pPayloadMemInfoPage1);
    NVDIMM_EXIT_I64(ReturnCode);
    return ReturnCode;
}

/**
  Parse EFI_ACPI_DESCRIPTION_HEADER (DSDT) and fetch NFIT & PCAT pointers to table
  Also, parse PMTT table to check if MM can be configured
  @param[in] pDsdt a pointer to EFI_ACPI_DESCRIPTION_HEADER instance for each of NFIT, PMTT and PCAT
  @param[out] ppFitHead pointer to pointer to store NFIT table
  @param[out] ppPcatHead pointer to pointer to store PCAT table
  @param[out] ppPmttHead pointer to pointer to store PMTT table
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
  OUT ParsedPmttHeader **ppPmttHead,
  OUT BOOLEAN *pIsMemoryModeAllowed
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();
  CHECK_NULL_ARG(pNfit, Finish);
  CHECK_NULL_ARG(pPcat, Finish);
  // PMTT is optional in some situations, check further down
  CHECK_NULL_ARG(ppFitHead, Finish);
  CHECK_NULL_ARG(ppPcatHead, Finish);
  CHECK_NULL_ARG(ppPmttHead, Finish);
  CHECK_NULL_ARG(pIsMemoryModeAllowed, Finish);

  CHECK_RESULT(ParseNfitTable((VOID *)pNfit, ppFitHead), Finish);

  CHECK_RESULT(ParsePcatTable((VOID *)pPcat, ppPcatHead), Finish);

  // Assume that PMTT table is not available at first
  // (skip MM allowed check and let BIOS handle it)
  *pIsMemoryModeAllowed = TRUE;

  // If not Purley, a PMTT table is required! Error out
  if (!IS_ACPI_HEADER_REV_MAJ_0_MIN_VALID((*ppPcatHead)->pPlatformConfigAttr)) {
    CHECK_NULL_ARG(pPMTT, Finish);
  }

  // Only parse a non-NULL PMTT table
  if (pPMTT != NULL) {
    CHECK_RESULT(ParsePmttTable((VOID *)pPMTT, ppPmttHead), Finish);
    // If we didn't fail out in the ParsePmttTable call, then the raw pPMTT table
    // is not NULL and is a valid revision. Check if memory mode is allowed.
    *pIsMemoryModeAllowed = CheckIsMemoryModeAllowed((TABLE_HEADER *)pPMTT);
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Fetch the NFIT and PCAT tables from EFI_SYSTEM_TABLE

  @param[in] pSystemTable is a pointer to the EFI_SYSTEM_TABLE instance
  @param[out] ppNfit is a pointer to EFI_ACPI_DESCRIPTION_HEADER (NFIT)
  @param[out] ppPcat is a pointer to EFI_ACPI_DESCRIPTION_HEADER (PCAT)
  @param[out] ppPMTT is a pointer to EFI_ACPI_DESCRIPTION_HEADER (PMTT)

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *pRsdp = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pRsdt = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pCurrentTable = NULL;
  UINT32 Index = 0;
  UINT64 Tmp = 0;

  NVDIMM_ENTRY();

  if ((pSystemTable == NULL) || (ppNfit == NULL && ppPcat == NULL && ppPMTT == NULL)) {
    goto Finish;
  }

  /**
    get the Root ACPI table pointer
  **/
  NVDIMM_DBG("Looking for the ACPI Root System Descriptor Pointer (RSDP) in UEFI");
  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    /**
      ACPI 2.0
    **/
    if (CompareGuid(&gEfiAcpi20TableGuid, &(gST->ConfigurationTable[Index].VendorGuid))) {
      NVDIMM_DBG("Found the RSDP table");
      pRsdp = gST->ConfigurationTable[Index].VendorTable;
      break;
    }
  }

  if (pRsdp == NULL) {
    /**
      Unable to find the Root ACPI table pointer
    **/
    NVDIMM_WARN("Unable to find the ACPI Root System Descriptor Pointer (RSDP)");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  /**
    Get the RSDT
  **/
  pRsdt = (EFI_ACPI_DESCRIPTION_HEADER *)pRsdp->XsdtAddress;
  if (pRsdt == NULL) {
    NVDIMM_WARN("Unable to find the ACPI Root System Descriptor Table (RSDT)");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  if (NULL != ppNfit) {
    *ppNfit = NULL;
  }

  if (NULL != ppPcat) {
    *ppPcat = NULL;
  }

  if (NULL != ppPMTT) {
    *ppPMTT = NULL;
  }

  /**
    Find the Fixed ACPI Description Table (FADT)
  **/
  NVDIMM_DBG("Looking for the Fixed ACPI Description Table (FADT)");
  for (Index = sizeof (EFI_ACPI_DESCRIPTION_HEADER); Index < pRsdt->Length; Index += sizeof(UINT64)) {
    Tmp = *(UINT64 *) ((UINT8 *) pRsdt + Index);
    pCurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *) (UINT64 *) (UINTN) Tmp;

    if ((NULL != ppNfit) && (pCurrentTable->Signature == NFIT_TABLE_SIG)) {
      NVDIMM_DBG("Found the NFIT table");
      *ppNfit = pCurrentTable;
    }

    if ((NULL != ppPcat) && (pCurrentTable->Signature == PCAT_TABLE_SIG)) {
      NVDIMM_DBG("Found the PCAT table");
      *ppPcat = pCurrentTable;
    }

    if ((NULL != ppPMTT) && (pCurrentTable->Signature == PMTT_TABLE_SIG)) {
      NVDIMM_DBG("Found the PMTT table");
      *ppPMTT = pCurrentTable;
    }

    // Break if we find all tables looking for
    if (((NULL == ppNfit) || (*ppNfit != NULL)) &&
      ((NULL == ppPcat) || (*ppPcat != NULL)) &&
      ((NULL == ppPMTT) || (*ppPMTT != NULL))) {
      break;
    }
  }

  /**
    Failed to find the at least one of the tables
  **/
  if (((NULL != ppNfit) && (NULL == *ppNfit)) ||
    ((NULL != ppPcat) && (NULL == *ppPcat)) ||
    ((NULL != ppPMTT) && (NULL == *ppPMTT))) {
    NVDIMM_WARN("Unable to find requested ACPI table.");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifndef OS_BUILD
  EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE *pMmConfigStruct;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *pRsdp = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pRsdt = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pMcfg = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pCurrentTable = NULL;
  UINT32 Index = 0;
  UINT64 Tmp = 0;

  NVDIMM_ENTRY();

  if (pSystemTable == NULL || pPciBaseAddress == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (CompareGuid(&gEfiAcpi20TableGuid, &(gST->ConfigurationTable[Index].VendorGuid))) {
      NVDIMM_DBG("Found the RSDP table");
      pRsdp = gST->ConfigurationTable[Index].VendorTable;
      break;
    }
  }

  if (pRsdp == NULL) {
    NVDIMM_WARN("Unable to find the ACPI Root System Descriptor Pointer (RSDP)");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  pRsdt = (EFI_ACPI_DESCRIPTION_HEADER *)pRsdp->XsdtAddress;
  if (pRsdt == NULL) {
    NVDIMM_WARN("Unable to find the ACPI Root System Descriptor Table (RSDT)");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  for (Index = sizeof (EFI_ACPI_DESCRIPTION_HEADER); Index < pRsdt->Length; Index += sizeof(UINT64)) {
    Tmp = *(UINT64 *) ((UINT8 *) pRsdt + Index);
    pCurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *) (UINT64 *) (UINTN) Tmp;

    if (pCurrentTable->Signature == EFI_ACPI_3_0_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE) {
      pMcfg = pCurrentTable;
      NVDIMM_DBG("Found MCFG table too");
    }

    if (pMcfg != NULL) {
      break;
    }
  }

  if (pMcfg == NULL) {
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  pMmConfigStruct = (EFI_ACPI_MEMORY_MAPPED_ENHANCED_CONFIGURATION_SPACE_BASE_ADDRESS_ALLOCATION_STRUCTURE *) ((UINT8 *)pMcfg + sizeof(EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_BASE_ADDRESS_TABLE_HEADER));

  if (pMmConfigStruct == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pPciBaseAddress = pMmConfigStruct->BaseAddress;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
#endif
  return ReturnCode;
}

/**
  Check the memory map against the NFIT SPA memory for consistency.

  @retval EFI_SUCCESS   on success
  @retval EFI_OUT_OF_RESOURCES for a failed allocation
  @retval EFI_BAD_BUFFER_SIZE if the nfit spa memory is more than the one in memmap
**/
EFI_STATUS
CheckMemoryMap(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  UINTN Size = 0;
  EFI_MEMORY_DESCRIPTOR *Buffer = NULL;
  UINTN MapKey;
  UINTN ItemSize;
  UINT32 Version;
  UINT8 *Walker;
  UINT64 PhysicalStart = 0;
  UINT64 SizeFromMemmap = 0;
  UINT32 Index = 0;
  BOOLEAN FoundInMemoryMap = FALSE;

  /**
    Get the Memory Map.
  **/
  if (gBS->GetMemoryMap(&Size, Buffer, &MapKey, &ItemSize, &Version) == EFI_BUFFER_TOO_SMALL) {
    Size += SIZE_1KB;
    Buffer = AllocateZeroPool(Size);
    if (Buffer == NULL) {
      NVDIMM_ERR("Failed on allocating the buffer, NULL returned.");
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    ReturnCode = gBS->GetMemoryMap(&Size, Buffer, &MapKey, &ItemSize, &Version);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_ERR("Error: Failed to get Memory Map %d", ReturnCode);
      goto Finish;
    }
  }

  if (Buffer == NULL) {
    NVDIMM_WARN("GetMemoryMap returned unexpected error.");
    goto Finish;
  }

  ParsedFitHeader *pNFit = gNvmDimmData->PMEMDev.pFitHead;
  for(Index = 0; Index < pNFit->SpaRangeTblesNum; Index++) {
    SubTableHeader *pTable = (SubTableHeader *)pNFit->ppSpaRangeTbles[Index];
    SpaRangeTbl *pTableSpaRange = (SpaRangeTbl *)pTable;
    if (CompareMem(&pTableSpaRange->AddressRangeTypeGuid,
          &gSpaRangePmRegionGuid, sizeof(pTableSpaRange->AddressRangeTypeGuid)) == 0) {

      FoundInMemoryMap = FALSE;

      /**
        Parse through the memory map to find each persistent memory region.
      **/
      for (Walker = (UINT8*)Buffer; Walker < (((UINT8*)Buffer)+Size) && Walker != NULL; Walker += ItemSize){
        if (((EFI_MEMORY_DESCRIPTOR*)Walker)->Type == EFI_PERSISTENT_MEMORY_REGION) {
          PhysicalStart = ((EFI_MEMORY_DESCRIPTOR*)Walker)->PhysicalStart;
          SizeFromMemmap = ((EFI_MEMORY_DESCRIPTOR*)Walker)->NumberOfPages * SIZE_4KB;

          if (pTableSpaRange->SystemPhysicalAddressRangeBase >= PhysicalStart &&
            (pTableSpaRange->SystemPhysicalAddressRangeBase + pTableSpaRange->SystemPhysicalAddressRangeLength) <=
            (PhysicalStart + SizeFromMemmap)) {
            FoundInMemoryMap = TRUE;
            break;
          }
        }
      }

      if (!FoundInMemoryMap) {
        ReturnCode = EFI_BAD_BUFFER_SIZE;
        NVDIMM_ERR("Error: Nfit SPA Range not found in Memory Map");
        break;
      }
    }
  }

  Finish:
  FREE_POOL_SAFE(Buffer);
  NVDIMM_EXIT_I64(ReturnCode);
#endif
  return ReturnCode;
}

/**
  Initialize ACPI tables (NFit and PCAT)

  @retval EFI_SUCCESS   on success
  @retval EFI_NOT_FOUND Nfit table not found
**/
#ifndef OS_BUILD
EFI_STATUS
initAcpiTables(
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  EFI_ACPI_DESCRIPTION_HEADER *pNfit = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pPcat = NULL;
  EFI_ACPI_DESCRIPTION_HEADER *pPMTT = NULL;
  UINT64 PciBaseAddress;
  RETURN_STATUS PcdStatus = RETURN_NOT_STARTED;
  PbrContext *pContext = PBR_CTX();
  NVDIMM_ENTRY();

  NVDIMM_DBG("InitAcpiTables\n");
  if (PBR_NORMAL_MODE == PBR_GET_MODE(pContext) || PBR_RECORD_MODE == PBR_GET_MODE(pContext)) {
    /**
      Find the Differentiated System Description Table (DSDT) from EFI_SYSTEM_TABLE
    **/
    // Ignore errors in retrieving the tables for now. We'll handle them properly
    // in ParseAcpiTables
    CHECK_RESULT_CONTINUE(GetAcpiTables(gST, &pNfit, &pPcat, &pPMTT));
  }

  if (PBR_RECORD_MODE == PBR_GET_MODE(pContext)) {
    if (pNfit) {
      NVDIMM_DBG("Found NFIT, recording it.");
      ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_NFIT, pNfit, pNfit->Length);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to record NFIT");
      }
    }

    if (pPcat) {
      NVDIMM_DBG("Found PCAT, recording it.");
      ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_PCAT, pPcat, pPcat->Length);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to record PCAT");
      }
    }

    if (pPMTT) {
      NVDIMM_DBG("Found PMTT, recording it.");
      ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_PMTT, pPMTT, pPMTT->Length);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to record PMTT");
      }
    }
  }
  else if (PBR_PLAYBACK_MODE == PBR_GET_MODE(pContext)) {
      ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_NFIT, (VOID**)&pNfit, (UINT32*)&(pNfit->Length));
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to record NFIT");
      }

      ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_PCAT, (VOID**)&pPcat, (UINT32*)&(pPcat->Length));
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to record PCAT");
      }

      ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_PMTT, (VOID**)&pPMTT, (UINT32*)&(pPMTT->Length));
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to record PMTT");
      }
  }
  /**
    Find the MCFG table from the EFI_SYSTEM_TABLE
    Get the PCI Base Address from the MCFG table
  **/
  ReturnCode = GetPciBaseAddress(gST, &PciBaseAddress);
  if (EFI_ERROR(ReturnCode) || (PciBaseAddress == 0)) {
    NVDIMM_WARN("Failed to get PCI Base Address");
    goto Finish;
  }

  /**
    Find the NVDIMM FW Interface Table (NFIT) & PCAT
  **/
  CHECK_RESULT(ParseAcpiTables(pNfit, pPcat, pPMTT, &gNvmDimmData->PMEMDev.pFitHead, &gNvmDimmData->PMEMDev.pPcatHead,
    &gNvmDimmData->PMEMDev.pPmttHead, &gNvmDimmData->PMEMDev.IsMemModeAllowedByBios), Finish);

  /** Set the Base address **/
  PcdStatus = PcdSet64S(PcdPciExpressBaseAddress, PciBaseAddress);
  if (RETURN_ERROR(PcdStatus)) {
    NVDIMM_WARN("PcdSet64S failed");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }
  if(PcdGet64(PcdPciExpressBaseAddress) != PciBaseAddress) {
    NVDIMM_WARN("Could not set the PCI Base Address");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif
/**
  Get System Capabilities information from PCAT and NFIT tables
  Pointer to variable length pInterleaveFormatsSupported is allocated here and must be freed by
  caller.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[out] pSysCapInfo is a pointer to table with System Capabilities information

  @retval EFI_SUCCESS   on success
  @retval EFI_INVALID_PARAMETER NULL argument
  @retval EFI_NOT_STARTED Pcat tables not parsed
**/
EFI_STATUS
EFIAPI
GetSystemCapabilitiesInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT SYSTEM_CAPABILITIES_INFO *pSysCapInfo
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Capabilities = 0;
  PlatformCapabilitiesTbl *pNfitPlatformCapability = NULL;

  NVDIMM_ENTRY();

  if (pThis == NULL || pSysCapInfo == NULL) {
    NVDIMM_DBG("Invalid parameter");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead == NULL) {
    NVDIMM_DBG("Pcat tables not parsed");
    ReturnCode = EFI_NOT_STARTED;
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pFitHead == NULL) {
    NVDIMM_DBG("Nfit tables not parsed");
    ReturnCode = EFI_NOT_STARTED;
    goto Finish;
  }

  pSysCapInfo->PartitioningAlignment = gNvmDimmData->Alignments.RegionPartitionAlignment;
  pSysCapInfo->InterleaveSetsAlignment = gNvmDimmData->Alignments.RegionPersistentAlignment;

  if (IS_ACPI_HEADER_REV_MAJ_1_OR_MAJ_3(gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr)) {
    if (gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum == 1 &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo != NULL &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo[0] != NULL) {
      PLATFORM_CAPABILITY_INFO3 *pPlatformCapability3 = gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat3Tables.ppPlatformCapabilityInfo[0];
      pSysCapInfo->OperatingModeSupport = pPlatformCapability3->MemoryModeCapabilities.MemoryModes;
      pSysCapInfo->PlatformConfigSupported = pPlatformCapability3->MgmtSwConfigInputSupport;
      pSysCapInfo->CurrentOperatingMode = pPlatformCapability3->CurrentMemoryMode.MemoryMode;

      /**
        Platform capabilities
      **/
      pSysCapInfo->AppDirectMirrorSupported = 0;
      pSysCapInfo->DimmSpareSupported = 0;
      pSysCapInfo->AppDirectMigrationSupported = 0;
    }
    else {
      NVDIMM_DBG("Number of Platform  Capability Information tables: %d",
        gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }
  }
  else {
    if (gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum == 1 &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo != NULL &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo[0] != NULL) {
      PLATFORM_CAPABILITY_INFO *pPlatformCapability = gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat2Tables.ppPlatformCapabilityInfo[0];
      pSysCapInfo->OperatingModeSupport = pPlatformCapability->MemoryModeCapabilities.MemoryModes;
      pSysCapInfo->PlatformConfigSupported = pPlatformCapability->MgmtSwConfigInputSupport;
      pSysCapInfo->CurrentOperatingMode = pPlatformCapability->CurrentMemoryMode.MemoryMode;

      /**
        Platform capabilities
      **/
      pSysCapInfo->AppDirectMirrorSupported = IS_BIT_SET_VAR(pPlatformCapability->PersistentMemoryRasCapability, BIT0);
      pSysCapInfo->DimmSpareSupported = IS_BIT_SET_VAR(pPlatformCapability->PersistentMemoryRasCapability, BIT1);
      pSysCapInfo->AppDirectMigrationSupported = IS_BIT_SET_VAR(pPlatformCapability->PersistentMemoryRasCapability, BIT2);
    }
    else {
      NVDIMM_DBG("Number of Platform  Capability Information tables: %d",
        gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }
  }

  if (IS_ACPI_HEADER_REV_MAJ_1_OR_MAJ_3(gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr)) {
    if (gNvmDimmData->PMEMDev.pPcatHead->MemoryInterleaveCapabilityInfoNum == 1 &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo != NULL &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo[0] != NULL) {
      MEMORY_INTERLEAVE_CAPABILITY_INFO3 *pInterleaveCapability3 = gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat3Tables.ppMemoryInterleaveCapabilityInfo[0];
      pSysCapInfo->InterleaveAlignmentSize = pInterleaveCapability3->InterleaveAlignmentSize;
      pSysCapInfo->InterleaveFormatsSupportedNum = pInterleaveCapability3->NumOfFormatsSupported;
      pSysCapInfo->PtrInterleaveSize = (HII_POINTER)AllocateZeroPool(sizeof(INTERLEAVE_SIZE));
      if ((INTERLEAVE_FORMAT *)pSysCapInfo->PtrInterleaveSize == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      CopyMem_S((INTERLEAVE_SIZE *)pSysCapInfo->PtrInterleaveSize, sizeof(INTERLEAVE_SIZE),
        &pInterleaveCapability3->InterleaveSize, sizeof(INTERLEAVE_SIZE));
      pSysCapInfo->PtrInterleaveFormatsSupported = (HII_POINTER)AllocateZeroPool(sizeof(INTERLEAVE_FORMAT)
        * pSysCapInfo->InterleaveFormatsSupportedNum);
      if ((INTERLEAVE_FORMAT *)pSysCapInfo->PtrInterleaveFormatsSupported == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      CopyMem_S((INTERLEAVE_FORMAT *)pSysCapInfo->PtrInterleaveFormatsSupported,
        sizeof(INTERLEAVE_FORMAT) * pSysCapInfo->InterleaveFormatsSupportedNum,
        pInterleaveCapability3->InterleaveFormatList,
        sizeof(INTERLEAVE_FORMAT) * pSysCapInfo->InterleaveFormatsSupportedNum);
    }
  }
  else {
    if (gNvmDimmData->PMEMDev.pPcatHead->MemoryInterleaveCapabilityInfoNum == 1 &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo != NULL &&
      gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo[0] != NULL) {
      MEMORY_INTERLEAVE_CAPABILITY_INFO *pInterleaveCapability = gNvmDimmData->PMEMDev.pPcatHead->pPcatVersion.Pcat2Tables.ppMemoryInterleaveCapabilityInfo[0];
      pSysCapInfo->InterleaveAlignmentSize = pInterleaveCapability->InterleaveAlignmentSize;
      pSysCapInfo->InterleaveFormatsSupportedNum = pInterleaveCapability->NumOfFormatsSupported;
      pSysCapInfo->PtrInterleaveSize = 0;
      pSysCapInfo->PtrInterleaveFormatsSupported = (HII_POINTER)AllocateZeroPool(sizeof(INTERLEAVE_FORMAT)
        * pSysCapInfo->InterleaveFormatsSupportedNum);
      if ((INTERLEAVE_FORMAT *)pSysCapInfo->PtrInterleaveFormatsSupported == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      CopyMem_S((INTERLEAVE_FORMAT *)pSysCapInfo->PtrInterleaveFormatsSupported,
        sizeof(INTERLEAVE_FORMAT) * pSysCapInfo->InterleaveFormatsSupportedNum,
        pInterleaveCapability->InterleaveFormatList,
        sizeof(INTERLEAVE_FORMAT) * pSysCapInfo->InterleaveFormatsSupportedNum);
    }
  }

  if (gNvmDimmData->PMEMDev.pFitHead->PlatformCapabilitiesTblesNum == 1 &&
      gNvmDimmData->PMEMDev.pFitHead->ppPlatformCapabilitiesTbles != NULL &&
      gNvmDimmData->PMEMDev.pFitHead->ppPlatformCapabilitiesTbles[0] != NULL) {
    pNfitPlatformCapability = gNvmDimmData->PMEMDev.pFitHead->ppPlatformCapabilitiesTbles[0];
    Capabilities = pNfitPlatformCapability->Capabilities & NFIT_MEMORY_CONTROLLER_FLUSH_BIT1;
    pSysCapInfo->AdrSupported = (Capabilities == NFIT_MEMORY_CONTROLLER_FLUSH_BIT1);
  }
  for (Index = 0; Index < SUPPORTED_BLOCK_SIZES_COUNT; Index++) {
    CopyMem_S(&pSysCapInfo->NsBlockSizes[Index], sizeof(pSysCapInfo->NsBlockSizes[Index]),  &gSupportedBlockSizes[Index], sizeof(pSysCapInfo->NsBlockSizes[Index]));
  }
  pSysCapInfo->MinNsSize = gNvmDimmData->Alignments.PmNamespaceMinSize;

  /**
    Features supported by the driver
  **/
  pSysCapInfo->RenameNsSupported = FEATURE_NOT_SUPPORTED;

  /**
    The UEFI driver does not support this feature
  **/
  pSysCapInfo->GrowPmNsSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->ShrinkPmNsSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->GrowBlkNsSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->ShrinkBlkNsSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->InitiateScrubSupported = FEATURE_NOT_SUPPORTED;
#ifdef OS_BUILD
  pSysCapInfo->EraseDeviceDataSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->EnableDeviceSecuritySupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->DisableDeviceSecuritySupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->UnlockDeviceSecuritySupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->FreezeDeviceSecuritySupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->ChangeDevicePassphraseSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->MasterEraseDeviceDataSupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->ChangeMasterPassphraseSupported = FEATURE_NOT_SUPPORTED;
#else
  pSysCapInfo->EraseDeviceDataSupported = FEATURE_SUPPORTED;
  pSysCapInfo->EnableDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->DisableDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->UnlockDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->FreezeDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->ChangeDevicePassphraseSupported = FEATURE_SUPPORTED;
  pSysCapInfo->MasterEraseDeviceDataSupported = FEATURE_SUPPORTED;
  pSysCapInfo->ChangeMasterPassphraseSupported = FEATURE_SUPPORTED;
#endif

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


EFI_STATUS
ValidateImageVersion(
  IN       NVM_FW_IMAGE_HEADER *pImage,
  IN       BOOLEAN Force,
  IN       DIMM *pDimm,
      OUT  NVM_STATUS *pNvmStatus,
      OUT  COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM_BSR Bsr;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  CHAR16 *pDimmSteppingStr = NULL;
  CHAR16 *pImgSteppingStr = NULL;

  NVDIMM_ENTRY();

  if (pImage == NULL || pDimm == NULL || pNvmStatus == NULL) {
    goto Finish;
  }

  *pNvmStatus = NVM_SUCCESS; //assume success

  ZeroMem(&Bsr, sizeof(Bsr));

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (pDimm->FwVer.FwProduct == 0) {
    // Dimm seems inaccessible over DDRT and SMBUS, as we couldn't get the proper
    // firmware version
    *pNvmStatus = NVM_ERR_TIMEOUT;
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pDimm->FwVer.FwProduct != pImage->ImageVersion.ProductNumber.Version) {
    *pNvmStatus = NVM_ERR_FIRMWARE_VERSION_NOT_VALID;
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_PRODUCT_NUMBER_MISMATCH);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (!Force && pDimm->FwVer.FwRevision > pImage->ImageVersion.RevisionNumber.Version) {
    *pNvmStatus = NVM_ERR_FIRMWARE_VERSION_NOT_VALID;
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_REVISION_NUMBER_MISMATCH);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pDimm->FwVer.FwSecurityVersion > pImage->ImageVersion.SecurityRevisionNumber.Version) {

    ReturnCode = pNvmDimmConfigProtocol->GetBSRAndBootStatusBitMask(pNvmDimmConfigProtocol, pDimm->DimmID, &Bsr.AsUint64, NULL);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not get the DIMM BSR register, can't check if it is safe to send the command.");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
    if (Bsr.Separated_Current_FIS.SVNDE != DIMM_BSR_SVNDE_ENABLED) {
      *pNvmStatus = NVM_ERR_FIRMWARE_VERSION_NOT_VALID;
      CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
          DETAILS_SVNDE_NOT_ENABLED);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    if (!Force) {
      *pNvmStatus = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  if (pDimm->FwVer.FwRevision == pImage->ImageVersion.RevisionNumber.Version &&
    pDimm->FwVer.FwSecurityVersion == pImage->ImageVersion.SecurityRevisionNumber.Version &&
    pDimm->FwVer.FwBuild > pImage->ImageVersion.BuildNumber.Build) {
    if (!Force) {
      *pNvmStatus = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  if (((BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit1) <  MIN_FIS_SUPPORTED_BY_THIS_SW_MAJOR) ||
       (BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit1) == MIN_FIS_SUPPORTED_BY_THIS_SW_MAJOR &&
        BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit2) <  MIN_FIS_SUPPORTED_BY_THIS_SW_MINOR)) ||
       (BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit1) >  MAX_FIS_SUPPORTED_BY_THIS_SW_MAJOR)) {
    *pNvmStatus = NVM_ERR_FIRMWARE_API_NOT_VALID;
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if ((pDimm->ControllerRid != pImage->RevisionId)) {
    *pNvmStatus = NVM_ERR_IMAGE_FILE_NOT_COMPATIBLE_TO_CTLR_STEPPING;
    pDimmSteppingStr = ControllerRidToStr(pDimm->ControllerRid, pDimm->SubsystemDeviceId);
    pImgSteppingStr = ControllerRidToStr(pImage->RevisionId, pImage->DeviceId);
    CatSPrintNCopy(pCommandStatus->StatusDetails, MAX_STATUS_DETAILS_STR_LEN,
        DETAILS_CANT_USE_IMAGE, pImgSteppingStr, pDimmSteppingStr);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/*
 * Helper function for writing a spi image to a backup file.
 * Does not overwrite an existing file
 */
EFI_STATUS
DebugWriteSpiImageToFile(
    IN     CHAR16 *pWorkingDirectory OPTIONAL,
    IN     UINT32 DimmHandle,
    IN     CONST VOID *pSpiImageBuffer,
    IN     UINT64 ImageBufferSize
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE FileHandle = NULL;
  CHAR16 *FileNameFconfig = NULL;

  FileHandle = NULL;
  FileNameFconfig = CatSPrint(NULL, L"new_spi_w_merged_fconfig_0x%04x.bin", DimmHandle);

  // Do not overwrite if file exists
  ReturnCode = OpenFile(FileNameFconfig, &FileHandle, pWorkingDirectory, FALSE);
  if (ReturnCode != EFI_NOT_FOUND) {
    ReturnCode = EFI_WRITE_PROTECTED;
    NVDIMM_ERR("Found existing file when trying to write backup file %s", FileNameFconfig);
    NVDIMM_ERR("Move the existing file to a safe location, as it likely has the original fconfigs");
    goto Finish;
  }
  // If a handle was opened for some reason, make sure to close it
  if (FileHandle != NULL) {
    FileHandle->Close(FileHandle);
  }

  ReturnCode = OpenFile(FileNameFconfig, &FileHandle, pWorkingDirectory, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  ReturnCode = FileHandle->Write(FileHandle, &ImageBufferSize, (VOID *)pSpiImageBuffer);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
Finish:
  FREE_POOL_SAFE(FileNameFconfig);
  if (FileHandle != NULL) {
    FileHandle->Close(FileHandle);
  }
  return ReturnCode;
}

/**
  Flash new SPI image to a specified DCPMM

  @param[in] DimmPid Dimm ID of a DCPMM on which recovery is to be performed
  @param[in] pNewSpiImageBuffer is a pointer to new SPI FW image
  @param[in] ImageBufferSize is SPI image size in bytes

  @param[out] pNvmStatus NVM error code
  @param[out] pCommandStatus  command status list

  @retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
  @retval EFI_NOT_FOUND there is no DCPMM with such Pid
  @retval EFI_DEVICE_ERROR Unable to communicate with Dimm SPI
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_ACCESS_DENIED When SPI access is not unlocked
  @retval EFI_SUCCESS Update has completed successfully
**/
EFI_STATUS
EFIAPI
RecoverDimmFw(
  IN     UINT32 DimmHandle,
  IN     CONST VOID *pNewSpiImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     CHAR16 *pWorkingDirectory OPTIONAL,
     OUT NVM_STATUS *pNvmStatus,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifndef OS_BUILD
  DIMM *pCurrentDimm = NULL;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  NVM_SPI_DIRECTORY_GEN2 *pSpiDirectoryNewSpiImageBuffer;
  NVM_SPI_DIRECTORY_GEN2 SpiDirectoryTarget;
  UINT8 *pFconfigRegionNewSpiImageBuffer = NULL;
  UINT8 *pFconfigRegionTemp = NULL;
  UINT16 DeviceId;

  NVDIMM_ENTRY();

  if (pNewSpiImageBuffer == NULL || pCommandStatus == NULL || pNvmStatus == NULL) {
    goto Finish;
  }

  pSpiDirectoryNewSpiImageBuffer = (NVM_SPI_DIRECTORY_GEN2 *) pNewSpiImageBuffer;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **) &pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pCurrentDimm = GetDimmByHandle(DimmHandle, &gNvmDimmData->PMEMDev.Dimms);
  if (pCurrentDimm == NULL) {
    NVDIMM_ERR("Failed to find handle 0x%x in dimm list", DimmHandle);
    *pNvmStatus = NVM_ERR_DIMM_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = SpiFlashAccessible(pCurrentDimm);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Spi access is not enabled on DIMM 0x%x", pCurrentDimm->DeviceHandle.AsUint32);
    *pNvmStatus = NVM_ERR_SPI_ACCESS_NOT_ENABLED;
    goto Finish;
  }

  // Make sure we are working with a DCPMM 1st gen device
  ReturnCode = GetDeviceIdSpd(pCurrentDimm->SmbusAddress, &DeviceId);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Cannot access spd data over smbus: 0x%x", ReturnCode);
    *pNvmStatus = NVM_ERR_SPD_NOT_ACCESSIBLE;
    goto Finish;
  }

  if (DeviceId != SPD_DEVICE_ID_15) {
    NVDIMM_ERR("Incompatible hardware revision 0x%x", DeviceId);
    *pNvmStatus = NVM_ERR_INCOMPATIBLE_HARDWARE_REVISION;
    goto Finish;
  }

  pFconfigRegionTemp = (UINT8 *)AllocateZeroPool(SPI_FCONFIG_REGION_MAX_SIZE_BYTES);
  if (pFconfigRegionTemp == NULL) {
    NVDIMM_ERR("Out of memory");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Gather fconfig data
  ReturnCode = ReadSpiDirectoryTarget(pCurrentDimm, &SpiDirectoryTarget);
  if (!EFI_ERROR(ReturnCode)) {
    ReturnCode = ReadAndVerifyFconfigTarget(pCurrentDimm, &SpiDirectoryTarget, pFconfigRegionTemp);
  }
  // If there are any errors in recovering configuration data from the existing module spi image,
  // we need to recreate the fconfig data by merging entries from the spd
  // and the new spi image fconfig. Copy them to pFconfigRegionTemp
  if (EFI_ERROR(ReturnCode)) {
    ZeroMem(pFconfigRegionTemp, SPI_FCONFIG_REGION_MAX_SIZE_BYTES);
    ReturnCode = RecreateFconfigFromSpdAndNewSpiImage((UINT8 *)pNewSpiImageBuffer,
        ImageBufferSize, pCurrentDimm, pFconfigRegionTemp);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  // Copy original fconfig or generated fconfig to the new image
  pFconfigRegionNewSpiImageBuffer = ((UINT8 *)pNewSpiImageBuffer +
      pSpiDirectoryNewSpiImageBuffer->FconfigDataOffset);
  CopyMem((VOID *)pFconfigRegionNewSpiImageBuffer, (VOID *)pFconfigRegionTemp,
          sizeof(FconfigContainerHeader) + ((FconfigContainerHeader *)pFconfigRegionTemp)->DwordLen * sizeof(UINT32));

  // Copy migration data from the existing dimm spi image, if applicable
  CHECK_RESULT(ReadAndCopyMigrationData(pCurrentDimm, &SpiDirectoryTarget,
               pSpiDirectoryNewSpiImageBuffer), Finish);

  ////////////////////////////////////////////////////
  // Write out the new image to a file in order to recover the
  // dimm back to a good state after testing.
  // TODO: Needed for validation only! We do not need this after product PRQ and will
  // probably confuse people
  DebugWriteSpiImageToFile(pWorkingDirectory, pCurrentDimm->DeviceHandle.AsUint32, pNewSpiImageBuffer, ImageBufferSize);
  ///////////////////////////////////////////////////

  CHECK_RESULT(SpiEraseChip(pCurrentDimm, pCommandStatus), Finish);

  CHECK_RESULT(SpiWrite(pCurrentDimm, pNewSpiImageBuffer, (UINT32)ImageBufferSize,
      SPI_START_ADDRESS, FALSE, pCommandStatus), Finish);

Finish:
  FREE_POOL_SAFE(pFconfigRegionTemp);
  NVDIMM_EXIT_I64(ReturnCode);
  #endif
  return ReturnCode;
}

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
@param[in] Reserved Set to FALSE

@param[out] pFwImageInfo is a pointer to a structure containing FW image information
need to be provided if examine flag is set
@param[out] pCommandStatus Structure containing detailed NVM error codes

@retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
@retval EFI_NOT_FOUND there is no NVDIMM with such Pid
@retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
@retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
@retval EFI_SUCCESS Update has completed successfully
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
  IN     BOOLEAN Reserved,
  OUT NVM_FW_IMAGE_INFO *pFwImageInfo OPTIONAL,
  OUT COMMAND_STATUS *pCommandStatus
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  UINT64 FWImageMaxSize = 0;
  NVM_FW_IMAGE_HEADER *pFileHeader = NULL;
  EFI_FILE_HANDLE FileHandle = NULL;
  VOID *pImageBuffer = NULL;
  CHAR16 *pErrorMessage = NULL;
  UINTN BuffSize = 0;
  UINTN TempBuffSize = 0;
  NVM_STATUS NvmStatus = NVM_ERR_OPERATION_NOT_STARTED;
  BOOLEAN* pDimmsCanBeUpdated = NULL;
  UINT32 DimmsToUpdate = 0;
  UINT32 UpdateFailures = 0;
  UINT32 VerificationFailures = 0;
  UINT32 ForceRequiredDimms = 0;
  UINT16 SubsystemDeviceId = 0x0;
  REQUIRE_DCPMMS RequireDcpmmsBitfield = REQUIRE_DCPMMS_MANAGEABLE;
  // FlashSPI is unsupported. Will remove more completely in a future change
  BOOLEAN FlashSPI = FALSE;

  EFI_STATUS LongOpStatusReturnCode = 0;
  NVM_STATUS LongOpNvmStatus = NVM_ERR_OPERATION_NOT_STARTED;

  ZeroMem(pDimms, sizeof(pDimms));

  NVDIMM_ENTRY();
  if (pCommandStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  if (pThis == NULL || (pDimmIds == NULL && DimmIdsCount > 0)) {
    pCommandStatus->GeneralStatus = NVM_ERR_INVALID_PARAMETER;
    goto Finish;
  }

  pCommandStatus->GeneralStatus = NvmStatus;

  if (pFileName == NULL) {
    pCommandStatus->GeneralStatus = NVM_ERR_FILENAME_NOT_PROVIDED;
    goto Finish;
  }

  if (Reserved == TRUE) {
    pCommandStatus->GeneralStatus = NVM_ERR_FLASH_SPI_NO_LONGER_SUPPORTED;
    goto Finish;
  }

  if (!Recovery && FlashSPI) {
    pCommandStatus->GeneralStatus = NVM_ERR_INVALID_PARAMETER;
    goto Finish;
  }

  if (Recovery && FlashSPI) {
    // In FlashSPI scenario.
    // For now, keep existing behavior of skipping the standard VerifyTargetDimms()
    ReturnCode = VerifyNonfunctionalTargetDimms(pDimmIds, DimmIdsCount, pDimms, &DimmsNum, pCommandStatus);
  } else {
    if (Recovery) {
      // Not FlashSPI.
      // For backwards compatibility, keep "-recover" meaning
      // "only run on non-functional DCPMMs". Reject functional DCPMMs.
      RequireDcpmmsBitfield |= REQUIRE_DCPMMS_NON_FUNCTIONAL;
    }
    // Note: By default non-functional DCPMMs are included in normal firmware
    // update (don't require "-recover" to be passed in)
    ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, RequireDcpmmsBitfield, pDimms, &DimmsNum, pCommandStatus);
  }
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to verify the target dimms");
    pCommandStatus->GeneralStatus = NVM_ERR_DIMM_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = OpenFileBinary(pFileName, &FileHandle, pWorkingDirectory, FALSE);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("OpenFile returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    pCommandStatus->GeneralStatus = NVM_ERR_FILE_NOT_FOUND;
    goto Finish;
  }

  FileHandle->Close(FileHandle);
  FileHandle = NULL;

  // find the device id.  Must be same for all Dimms
  if (DimmsNum > 0)
  {
    SubsystemDeviceId = pDimms[0]->SubsystemDeviceId;
    for (Index = 1; Index < DimmsNum; ++Index)
    {
      if (SubsystemDeviceId != pDimms[Index]->SubsystemDeviceId)
      {
        NVDIMM_DBG("Dimms with different subsystem device ids are not allowed.");
        pCommandStatus->GeneralStatus = NVM_ERR_MIXED_GENERATIONS_NOT_SUPPORTED;
        goto Finish;
      }
    }
  }

  FWImageMaxSize = GetMinFWImageMaxSize(pThis, pDimms, DimmsNum);
  if (FWImageMaxSize == MAX_UINT64){
    NVDIMM_DBG("GetMinFWImageMaxSize failed, maximum allowed firmware image size was not specified");
    goto Finish;
  }

  if (!LoadFileAndCheckHeader(pFileName, pWorkingDirectory, FlashSPI, SubsystemDeviceId, FWImageMaxSize, &pFileHeader, pCommandStatus)) {
    for (Index = 0; Index < DimmsNum; Index++) {
      VerificationFailures++;
      SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_FILE_NOT_VALID, TRUE);
    }
    NVDIMM_DBG("LoadFileAndCheckHeader Failed");
    goto Finish;
  }

  if (pFwImageInfo != NULL) {
    pFwImageInfo->Date = pFileHeader->Date;
    pFwImageInfo->ImageVersion = pFileHeader->ImageVersion;
    pFwImageInfo->FirmwareType = pFileHeader->ImageType;
    pFwImageInfo->ModuleVendor = pFileHeader->ModuleVendor;
    pFwImageInfo->Size = pFileHeader->Size;
  }

  ReturnCode = OpenFileBinary(pFileName, &FileHandle, pWorkingDirectory, FALSE);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to open the file");
    pCommandStatus->GeneralStatus = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  ReturnCode = GetFileSize(FileHandle, &BuffSize);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed get the file size");
    pCommandStatus->GeneralStatus = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  pImageBuffer = AllocateZeroPool(BuffSize);
  if (pImageBuffer == NULL) {
    NVDIMM_ERR("Out of memory");
    pCommandStatus->GeneralStatus = NVM_ERR_UNKNOWN;
    goto Finish;
  }

  TempBuffSize = BuffSize;
  ReturnCode = FileHandle->Read(FileHandle, &BuffSize, pImageBuffer);
  if (EFI_ERROR(ReturnCode) || BuffSize != TempBuffSize) {
    if (Examine) {
      pCommandStatus->GeneralStatus = NVM_ERR_IMAGE_EXAMINE_INVALID;
    }
    else {
      pCommandStatus->GeneralStatus = NVM_ERR_IMAGE_FILE_NOT_VALID;
    }

    goto Finish;
  }

  pDimmsCanBeUpdated = AllocatePool(sizeof(BOOLEAN) * DimmsNum);
  if (pDimmsCanBeUpdated == NULL) {
    NVDIMM_ERR("Out of memory");
    pCommandStatus->GeneralStatus = NVM_ERR_NO_MEM;
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    pDimmsCanBeUpdated[Index] = FALSE;
    if (Recovery && FlashSPI) {
      //We will only be able to flash spi if we can access the
      //spi interface over smbus
#ifdef OS_BUILD
      // Spi check access will fail with unsupported on OS
      SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_ERR_SPI_ACCESS_NOT_ENABLED, TRUE);
      VerificationFailures++;
#else
      ReturnCode = SpiFlashAccessible(pDimms[Index]);
      if (EFI_ERROR(ReturnCode)) {
        SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_ERR_SPI_ACCESS_NOT_ENABLED, TRUE);
        VerificationFailures++;
      }
      else {
        pDimmsCanBeUpdated[Index] = TRUE;
        SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_SUCCESS_IMAGE_EXAMINE_OK, TRUE);
      }
#endif
    }
    else {
      ReturnCode = ValidateImageVersion(pFileHeader, Force, pDimms[Index], &NvmStatus, pCommandStatus);
      if (EFI_ERROR(ReturnCode)) {
        VerificationFailures++;
        pCommandStatus->GeneralStatus = NvmStatus;
        if (ReturnCode == EFI_ABORTED) {
          if (NvmStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {
            SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION, TRUE);
            ForceRequiredDimms++;
          }
          else {
            SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NvmStatus, TRUE);
          }
        }
      }
      else {
        pDimmsCanBeUpdated[Index] = TRUE;
        SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_SUCCESS_IMAGE_EXAMINE_OK, TRUE);
      }
    }
  }

  if (TRUE == Examine) {
    if (VerificationFailures == 0) {
      pCommandStatus->GeneralStatus = NVM_SUCCESS;
    }
    goto Finish;
  }

  //count up the number of DIMMs to update
  DimmsToUpdate = 0;
  for (Index = 0; Index < DimmsNum; Index++) {
    if (pDimmsCanBeUpdated[Index] == TRUE) {
      DimmsToUpdate++;
    }
  }

  if (DimmsToUpdate == 0) {
    if (pCommandStatus->GeneralStatus == NVM_ERR_OPERATION_NOT_STARTED) {
      NVDIMM_DBG("Found no DIMMs to update - either none were passed or none passed the verification checks");
      pCommandStatus->GeneralStatus = NVM_ERR_DIMM_NOT_FOUND;
    }
    goto Finish;
  }

  // upload FW image to all specified DIMMs
  for (Index = 0; Index < DimmsNum; Index++) {
    if (pDimmsCanBeUpdated[Index] == FALSE) {
      NVDIMM_DBG("Skipping dimm %d. It is marked as not being currently capable of this update", pDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    if (Recovery && FlashSPI) {
      ReturnCode = RecoverDimmFw(pDimms[Index]->DeviceHandle.AsUint32,
      pImageBuffer, BuffSize, pWorkingDirectory, &NvmStatus, pCommandStatus);
      if (EFI_ERROR(ReturnCode))
      {
        NVDIMM_ERR("RecoverDimmFw returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
      }
    }
    else {
      ReturnCode = FwCmdUpdateFw(pDimms[Index], pImageBuffer, BuffSize, &NvmStatus, pCommandStatus);
    }

    if (ReturnCode != EFI_SUCCESS) {
      UpdateFailures++;

      //Perform a check to see if it was a long operation that blocked the update and get more details about it
      LongOpStatusReturnCode = CheckForLongOpStatusInProgress(pDimms[Index], &LongOpNvmStatus);
      if (LongOpStatusReturnCode == EFI_SUCCESS && LongOpNvmStatus != NVM_SUCCESS) {
        pCommandStatus->GeneralStatus = LongOpNvmStatus;
        SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], LongOpNvmStatus, TRUE);
      }
      else {
        if (NvmStatus == NVM_SUCCESS) {
          pCommandStatus->GeneralStatus = NVM_ERR_OPERATION_FAILED;
          SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED, TRUE);
        }
        else
        {
          pCommandStatus->GeneralStatus = NvmStatus;
          SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NvmStatus, TRUE);
        }
      }
    }
    else
    {
      SetObjStatusForDimmWithErase(pCommandStatus, pDimms[Index], NvmStatus, TRUE);
    }
  }

  if (0 == UpdateFailures) {
    pCommandStatus->GeneralStatus = NVM_SUCCESS;
  }

Finish:
  if (ForceRequiredDimms > 0 && !Force) {
    pCommandStatus->GeneralStatus = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
  }

  ReturnCode = EFI_SUCCESS;
  if (pCommandStatus->GeneralStatus != NVM_SUCCESS) {
    ReturnCode = EFI_ABORTED;
  }

  if (FileHandle != NULL) {
    FileHandle->Close(FileHandle);
  }
  FREE_POOL_SAFE(pFileHeader);
  FREE_POOL_SAFE(pImageBuffer);
  FREE_POOL_SAFE(pErrorMessage);
  FREE_POOL_SAFE(pDimmsCanBeUpdated);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT8 LongOpStatusCode = 0;
  PT_OUTPUT_PAYLOAD_FW_LONG_OP_STATUS LongOpStatus;
  EFI_STATUS LongOpCheckRetCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (pNvmStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pNvmStatus = NVM_SUCCESS;
  LongOpCheckRetCode = FwCmdGetLongOperationStatus(pDimm, &LongOpStatusCode, &LongOpStatus);
  if (EFI_ERROR(LongOpCheckRetCode)) {
    //fatal error
    *pNvmStatus = NVM_ERR_UNKNOWN;
    ReturnCode = LongOpCheckRetCode;
    goto Finish;
  }
  else if (LongOpStatus.Status != FW_DEVICE_BUSY) {
    //no long op in progress
    goto Finish;
  }

  if (LongOpStatus.CmdOpcode == PtSetFeatures && LongOpStatus.CmdSubOpcode == SubopAddressRangeScrub) {
    *pNvmStatus = NVM_ERR_ARS_IN_PROGRESS;
  }
  else if (LongOpStatus.CmdOpcode == PtUpdateFw && LongOpStatus.CmdSubOpcode == SubopUpdateFw) {
    *pNvmStatus = NVM_ERR_FWUPDATE_IN_PROGRESS;
  }
  else if (LongOpStatus.CmdOpcode == PtSetSecInfo && LongOpStatus.CmdSubOpcode == SubopOverwriteDimm) {
    *pNvmStatus = NVM_ERR_OVERWRITE_DIMM_IN_PROGRESS;
  }
  else {
    *pNvmStatus = NVM_ERR_UNKNOWN_LONG_OP_IN_PROGRESS;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Filter a list of dimms by a given socket and return an array of dimms
  that exist on a given socket

  @param[in] SocketId Socket to retrieve dimms for
  @param[in] pDimms Array of pointers to manageable DIMMs only
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pDimmsOnSocket Array of Dimms
  @param[out] pNumberDimmsOnSocket Returned number of items
  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one or more parameters are NULL
**/
STATIC
EFI_STATUS
FilterDimmBySocket(
  IN     UINT32 SocketId,
  IN     DIMM *pDimms[MAX_DIMMS],
  IN     UINT32 DimmsNum,
     OUT DIMM *pDimmsOnSocket[MAX_DIMMS],
     OUT UINT32 *pNumberDimmsOnSocket
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsOnSocket == NULL || pNumberDimmsOnSocket == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pNumberDimmsOnSocket = 0;
  ZeroMem(pDimmsOnSocket, sizeof(pDimmsOnSocket[0]) * MAX_DIMMS);

  for (Index = 0; Index < DimmsNum; Index++) {
    if (pDimms[Index]->SocketId == SocketId) {
      pDimmsOnSocket[*pNumberDimmsOnSocket] = pDimms[Index];
      (*pNumberDimmsOnSocket)++;
    }
  }

Finish:
    NVDIMM_EXIT_I64(ReturnCode);
    return ReturnCode;
  }


EFI_STATUS
ReturnErrorWithMediaDisabledPMemModule(
    OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pDimmNode = NULL;
  DIMM *pDimm = NULL;

  // Because of the difficulties in refactoring the create goal code
  // (specifically Region.c) to work with a media disabled PMem module,
  // error out with a requirement that the user replace the module.
  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);
    if (DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask)) {
      // Want to set an error for each module
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_MEDIA_NOT_ACCESSIBLE_CANNOT_CONTINUE);
      ReturnCode = EFI_UNSUPPORTED;
    }
  }
  return ReturnCode;
}

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
  @param[in] ReserveDimm Reserve one DIMM for use as a not interleaved AppDirect memory
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  REGION_GOAL_DIMM *pDimmsSymPerSocket = NULL;
  UINT32 DimmsSymNumPerSocket = 0;
  REGION_GOAL_DIMM *pDimmsAsymPerSocket = NULL;
  UINT32 DimmsAsymNumPerSocket = 0;
  DIMM *pReserveDimm = NULL;
  UINT64 ActualVolatileSize = 0;
  REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM];
  UINT32 RegionGoalTemplatesNum = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  BOOLEAN Found = FALSE;
  MEMORY_MODE AllowedMode = MEMORY_MODE_1LM;
  UINT32 DimmSecurityStateMask = 0;
  DIMM *pDimmsOnSocket[MAX_DIMMS];
  UINT32 NumDimmsOnSocket = 0;
  UINT64 TotalInputVolatileSize = 0;
  UINT64 TotalActualVolatileSize = 0;
  UINT32 Socket = 0;
  REGION_GOAL_DIMM *pDimmsSym = NULL;
  UINT32 DimmsSymNum = 0;
  REGION_GOAL_DIMM *pDimmsAsym = NULL;
  UINT32 DimmsAsymNum = 0;
  MAX_PMINTERLEAVE_SETS MaxPMInterleaveSets;
  ACPI_REVISION PcatRevision;
  BOOLEAN IsDimmUnlocked = FALSE;
  REQUIRE_DCPMMS RequireDcpmmsBitfield = REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL;

  NVDIMM_ENTRY();

  CHECK_RESULT(ReturnErrorWithMediaDisabledPMemModule(pCommandStatus), Finish);

  ZeroMem(RegionGoalTemplates, sizeof(RegionGoalTemplates));
  ZeroMem(&MaxPMInterleaveSets, sizeof(MaxPMInterleaveSets));
  ZeroMem(&PcatRevision, sizeof(PcatRevision));

  if (pThis == NULL || pCommandStatus == NULL
    || pVolatilePercent == NULL || pConfigGoals == NULL
    || pConfigGoalsCount == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pConfigGoalsCount = 0;

  if (gNvmDimmData->PMEMDev.pPcatHead != NULL) {
    PcatRevision.AsUint8 = gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr->Header.Revision.AsUint8;
  }

  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * MAX_DIMMS);
  if (ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pDimmsSym = AllocateZeroPool(sizeof(*pDimmsSym) * MAX_DIMMS);
  if (pDimmsSym == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pDimmsAsym = AllocateZeroPool(sizeof(*pDimmsAsym) * MAX_DIMMS);
  if (pDimmsAsym == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }


  //DCPMMs in population violation are ignored from all goal requests except in the case that the goal
  //request is for ADx1 100%.  In this case DCPMMs in population violation can be used.
  if (!((PM_TYPE_AD_NI == PersistentMemType) && (0 == *pVolatilePercent))) {
    RequireDcpmmsBitfield |= REQUIRE_DCPMMS_NO_POPULATION_VIOLATION;
  }
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, RequireDcpmmsBitfield,
      ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  if (pNumOfDimmsTargeted != NULL) {
    *pNumOfDimmsTargeted = DimmsNum;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    else if (ReturnCode == EFI_VOLUME_CORRUPTED) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_PCD_BAD_DEVICE_CONFIG);
    }
    goto Finish;
  }

  ReturnCode = CheckForExistingGoalConfigPerSocket(ppDimms, &DimmsNum);
  if (ReturnCode == EFI_ABORTED) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_CURR_CONF_EXISTS);
    NVDIMM_DBG("Current Goal Configuration exists. Operation Aborted");
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityStateMask);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityStateMask)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is in locked state.");
      goto Finish;
    }

    if ((DimmSecurityStateMask & SECURITY_MASK_ENABLED) && !(DimmSecurityStateMask & SECURITY_MASK_LOCKED)) {
      IsDimmUnlocked = TRUE;
    }
  }

  if (IsDimmUnlocked) {
    SetCmdStatus(pCommandStatus, NVM_WARN_GOAL_CREATION_SECURITY_UNLOCKED);
  }

  ReturnCode = PersistentMemoryTypeValidation(PersistentMemType);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ReserveDimm != RESERVE_DIMM_NONE && ReserveDimm != RESERVE_DIMM_AD_NOT_INTERLEAVED) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Mark reserve Dimm **/
  if (ReserveDimm != RESERVE_DIMM_NONE) {
    ReturnCode = SelectReserveDimm(ppDimms, &DimmsNum, &pReserveDimm);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (DimmsNum == 0) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
  // allocate helper structures
  pDimmsSymPerSocket = AllocateZeroPool(sizeof(*pDimmsSym) * MAX_DIMMS);
  if (pDimmsSymPerSocket == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  pDimmsAsymPerSocket = AllocateZeroPool(sizeof(*pDimmsAsym) * MAX_DIMMS);
  if (pDimmsAsymPerSocket == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (IS_ACPI_REV_MAJ_1_OR_MAJ_3(PcatRevision)) {
    ReturnCode = RetrieveMaxPMInterleaveSets(&MaxPMInterleaveSets);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (pMaxPMInterleaveSetsPerDie != NULL) {
      *pMaxPMInterleaveSetsPerDie = MaxPMInterleaveSets.MaxInterleaveSetsSplit.PerDie;
    }
  }

  for (Socket = 0; Socket < MAX_SOCKETS; Socket++) {
    DimmsAsymNumPerSocket = 0;
    DimmsSymNumPerSocket = 0;
    ZeroMem(pDimmsOnSocket, sizeof(pDimmsOnSocket[0]) * MAX_DIMMS);

    FilterDimmBySocket(Socket, ppDimms, DimmsNum, pDimmsOnSocket, &NumDimmsOnSocket);

     /**User might have created goal for 2nd socket alone*/
    if (NumDimmsOnSocket <= 0) {
      continue;
    }

    /** Calculate volatile percent **/
    ReturnCode = CalculateDimmCapacityFromPercent(pDimmsOnSocket, NumDimmsOnSocket, *pVolatilePercent, &ActualVolatileSize);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

    /* calculate the total requested volatile size */
    TotalInputVolatileSize += ActualVolatileSize;

    ReturnCode = MapRequestToActualRegionGoalTemplates(pDimmsOnSocket, NumDimmsOnSocket,
        pDimmsSymPerSocket, &DimmsSymNumPerSocket, pDimmsAsymPerSocket, &DimmsAsymNumPerSocket,
        PersistentMemType, ActualVolatileSize, ReservedPercent, ((IS_ACPI_REV_MAJ_1_OR_MAJ_3(PcatRevision)) ? &MaxPMInterleaveSets : NULL),
        &ActualVolatileSize, RegionGoalTemplates, &RegionGoalTemplatesNum, pCommandStatus);

    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    TotalActualVolatileSize += ActualVolatileSize;

    ReturnCode = ReduceCapacityForSocketSKU(Socket, pDimmsOnSocket, NumDimmsOnSocket, pDimmsSymPerSocket,
        &DimmsSymNumPerSocket, pDimmsAsymPerSocket,
      &DimmsAsymNumPerSocket, RegionGoalTemplates, &RegionGoalTemplatesNum, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    // Check for NM:FM ratio and set command status if not within limits
    ReturnCode = CheckNmFmLimits((UINT16)Socket, pDimmsSymPerSocket, DimmsSymNumPerSocket, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (pDimmsSym != NULL && pDimmsSymPerSocket != NULL && (DimmsSymNum < MAX_DIMMS)) {
      for (Index = 0; Index < DimmsSymNumPerSocket; Index++) {
        pDimmsSym[DimmsSymNum] = pDimmsSymPerSocket[Index];
        DimmsSymNum++;
      }
    }

    if (pDimmsAsym != NULL && pDimmsAsymPerSocket != NULL && (DimmsAsymNum < MAX_DIMMS)) {
      for (Index = 0; Index < DimmsAsymNumPerSocket; Index++) {
        pDimmsAsym[DimmsAsymNum] = pDimmsAsymPerSocket[Index];
        DimmsAsymNum++;
      }
    }
  }

  // We have removed all Asymmetrical memory from the system region templates should be reduced
  if (DimmsAsymNum == 0) {
    RegionGoalTemplatesNum = 1;
  }
  // We have removed all symmetrical memory from the system and region templates should be reduced
  if (DimmsSymNum == 0) {
    RegionGoalTemplatesNum = 0;
  }

  /** Calculate actual volatile percent **/
  if (TotalInputVolatileSize != 0) {
    *pVolatilePercent = (UINT32) ((*pVolatilePercent) * TotalActualVolatileSize / TotalInputVolatileSize);
  } else {
    *pVolatilePercent = 0;
  }

  // Fill in the ConfigGoals with enough information to be used by show goal
  for (Index = 0; Index < DimmsSymNum; Index++) {
    pConfigGoals[*pConfigGoalsCount].SocketId = pDimmsSym[Index].pDimm->SocketId;

    pConfigGoals[*pConfigGoalsCount].DimmID = pDimmsSym[Index].pDimm->DeviceHandle.AsUint32;

    ReturnCode = GetDimmUid(pDimmsSym[Index].pDimm, pConfigGoals[*pConfigGoalsCount].DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    pConfigGoals[*pConfigGoalsCount].VolatileSize = pDimmsSym[Index].VolatileSize;

    pConfigGoals[*pConfigGoalsCount].AppDirectSize[0] = pDimmsSym[Index].RegionSize;
    (*pConfigGoalsCount)++;
  }

  for (Index = 0; Index < DimmsAsymNum; Index++) {
    Found = FALSE;
    for (Index2 = 0; Index < *pConfigGoalsCount; Index2++) {
      if (pDimmsAsym[Index].pDimm->DeviceHandle.AsUint32 == pConfigGoals[Index2].DimmID) {
        pConfigGoals[Index2].AppDirectSize[1] = pDimmsAsym[Index].RegionSize;
        Found = TRUE;
        break;
      }
    }

    if (!Found) {
      pConfigGoals[*pConfigGoalsCount].DimmID = pDimmsAsym[Index].pDimm->DeviceHandle.AsUint32;

      ReturnCode = GetDimmUid(pDimmsAsym[Index].pDimm, pConfigGoals[*pConfigGoalsCount].DimmUid, MAX_DIMM_UID_LENGTH);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      pConfigGoals[*pConfigGoalsCount].AppDirectSize[0] = pDimmsAsym[Index].RegionSize;
      (*pConfigGoalsCount)++;
    }
  }

  ReturnCode = AllowedMemoryMode(&AllowedMode);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to determine system memory mode");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  if (!IS_BIOS_VOLATILE_MEMORY_MODE_2LM(AllowedMode)) {
    /** Check if volatile memory has been requested **/
    for (Index = 0; Index < *pConfigGoalsCount; Index++) {
      if (pConfigGoals[Index].VolatileSize > 0) {
        ReturnCode = EFI_UNSUPPORTED;
        ResetCmdStatus(pCommandStatus, NVM_ERR_PLATFORM_NOT_SUPPORT_2LM_MODE);
        break;
      }
    }
  }

  // Only overwrite the general status if we had nothing else to say
  if (pCommandStatus->GeneralStatus == NVM_ERR_OPERATION_NOT_STARTED) {
    SetCmdStatus(pCommandStatus, NVM_SUCCESS);
  }

Finish:
  FREE_POOL_SAFE(ppDimms);
  FREE_POOL_SAFE(pDimmsSym);
  FREE_POOL_SAFE(pDimmsAsym);
  FREE_POOL_SAFE(pDimmsSymPerSocket);
  FREE_POOL_SAFE(pDimmsAsymPerSocket);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  @param[in] ReserveDimm Reserve one DIMM for use as a not interleaved AppDirect memory
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
CreateGoalConfig(
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  REGION_GOAL_DIMM *pDimmsSymPerSocket = NULL;
  UINT32 DimmsSymNumPerSocket = 0;
  REGION_GOAL_DIMM *pDimmsAsymPerSocket = NULL;
  UINT32 DimmsAsymNumPerSocket = 0;
  DIMM *pReserveDimm = NULL;
  UINT32 DimmSecurityStateMask = 0;
  UINT64 VolatileSize = 0;
  UINT64 ReservedSize = 0;
  BOOLEAN Found = FALSE;
  DRIVER_PREFERENCES DriverPreferences;
  BOOLEAN Conflict = FALSE;
  REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM];
  UINT32 RegionGoalTemplatesNum = 0;
  UINT32 Index = 0;
  UINT32 Socket = 0;
  DIMM *pDimmsOnSocket[MAX_DIMMS];
  UINT32 NumDimmsOnSocket = 0;
  REGION_GOAL_DIMM *pDimmsSym = NULL;
  UINT32 DimmsSymNum = 0;
  REGION_GOAL_DIMM *pDimmsAsym = NULL;
  UINT32 DimmsAsymNum = 0;
  MAX_PMINTERLEAVE_SETS MaxPMInterleaveSets;
  ACPI_REVISION PcatRevision;
  BOOLEAN SendGoalConfigWarning = FALSE;
  REQUIRE_DCPMMS RequireDcpmmsBitfield = REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL;

  NVDIMM_ENTRY();

  CHECK_RESULT(ReturnErrorWithMediaDisabledPMemModule(pCommandStatus), Finish);

  ZeroMem(RegionGoalTemplates, sizeof(RegionGoalTemplates));
  ZeroMem(&DriverPreferences, sizeof(DriverPreferences));
  ZeroMem(&MaxPMInterleaveSets, sizeof(MaxPMInterleaveSets));
  ZeroMem(&PcatRevision, sizeof(PcatRevision));

  if (pThis == NULL || pCommandStatus == NULL || VolatilePercent > 100 || ReservedPercent > 100 || VolatilePercent + ReservedPercent > 100) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    NVDIMM_DBG("Invalid Parameter");
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPcatHead != NULL) {
    PcatRevision.AsUint8 = gNvmDimmData->PMEMDev.pPcatHead->pPlatformConfigAttr->Header.Revision.AsUint8;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * MAX_DIMMS);
  if (ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pDimmsSym = AllocateZeroPool(sizeof(*pDimmsSym) * MAX_DIMMS);
  if (pDimmsSym == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pDimmsAsym = AllocateZeroPool(sizeof(*pDimmsAsym) * MAX_DIMMS);
  if (pDimmsAsym == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }


  //DCPMMs in population violation are ignored from all goal requests except in the case that the goal
  //request is for ADx1 100%.  In this case DCPMMs in population violation can be used.
  if (!((PM_TYPE_AD_NI == PersistentMemType) && (0 == VolatilePercent))) {
    RequireDcpmmsBitfield |= REQUIRE_DCPMMS_NO_POPULATION_VIOLATION;
  }
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, RequireDcpmmsBitfield,
      ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  /** Verify Command Access Policy for Set PCD command **/
  UINT8 CapRestricted = 0;
  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS Attribs;

  CHECK_RESULT(GetFisTransportAttributes(pThis, &Attribs), Finish);

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = FwCmdGetCommandAccessPolicy(ppDimms[Index], PtSetAdminFeatures, SubopPlatformDataInfo, &CapRestricted);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve Command Access Policy for %d:%d. DimmID=0x%04x ReturnCode=%d.",
        PtSetAdminFeatures, SubopPlatformDataInfo, ppDimms[Index]->DimmID, ReturnCode);
      goto Finish;
    }

    // We're doing this pre-check (can we run Set PCD?) so the user
    // doesn't have to go through the process of creating and confirming a
    // goal only to have it fail to apply.
    // There are a few levels of restriction. First, we can't do anything if
    // we're restricted to BIOS mailbox only. Sometimes only DDRT is disabled,
    // in which case we can use SMBus. However, if SMBus is not requested or
    // enabled via -smbus flag or default configuration, then we can't do
    // anything if there's CAP is restricted to SMBus.
    if ((CapRestricted == COMMAND_ACCESS_POLICY_RESTRICTION_BIOSONLY)
      || (!IS_SMBUS_FLAG_ENABLED(Attribs) && (CapRestricted == COMMAND_ACCESS_POLICY_RESTRICTION_SMBUSONLY || CapRestricted == COMMAND_ACCESS_POLICY_RESTRICTION_BIOSSMBUSONLY))) {
      ReturnCode = EFI_UNSUPPORTED;
      NVDIMM_WARN("Command access policy disallows Set PCD command");
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED);
      goto Finish;
    }

  }

#ifdef OS_BUILD
  // TODO: Optimize the number of LSA reads happening when interleave sets
  // are initialized using NFIT & PCD both and namespaces are initialized.

  // Trigger Interleave Set initialization from PCD
  InitializeInterleaveSets(FALSE);

  // Trigger Interleave Set initialization from NFIT
  InitializeInterleaveSets(TRUE);

  ReturnCode = InitializeNamespaces();
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to initialize Namespaces, error = " FORMAT_EFI_STATUS ".", ReturnCode);
  }
#endif

  ReturnCode = IsNamespaceOnDimms(ppDimms, DimmsNum, &Found);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  if (Found) {
    ReturnCode = EFI_ABORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_GOAL_NAMESPACE_EXISTS);
    NVDIMM_DBG("Operation Aborted. Namespaces exists on current Region, need to be deleted before creating Goal");
    goto Finish;
  }

  if (ReserveDimm != RESERVE_DIMM_NONE && ReserveDimm != RESERVE_DIMM_AD_NOT_INTERLEAVED) {
    ReturnCode = EFI_INVALID_PARAMETER;

    goto Finish;
  }

  if (LabelVersionMajor != NSINDEX_MAJOR) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if ((LabelVersionMinor != NSINDEX_MINOR_1) &&
      (LabelVersionMinor != NSINDEX_MINOR_2)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, TRUE, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    goto Finish;
  }

  ReturnCode = CheckForExistingGoalConfigPerSocket(ppDimms, &DimmsNum);
  if (ReturnCode == EFI_ABORTED) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_CURR_CONF_EXISTS);
    NVDIMM_DBG("Current Goal Configuration exists. Operation Aborted");
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityStateMask);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityStateMask)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is in locked state.");
      goto Finish;
    }

    if (IS_DIMM_SECURITY_ENABLED(DimmSecurityStateMask)) {
      SendGoalConfigWarning = TRUE;
    }
  }

  ReturnCode = PersistentMemoryTypeValidation(PersistentMemType);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** User has to configure all the unconfigured DIMMs or all DIMMs on a given socket at once **/
  ReturnCode = VerifyCreatingSupportedRegionConfigs(ppDimms, DimmsNum, PersistentMemType, VolatilePercent, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("VerifyCreatingSupportedRegionConfigs Error");
    goto Finish;
  }

  /** Mark reserve Dimm **/
  if (ReserveDimm != RESERVE_DIMM_NONE) {
    ReturnCode = SelectReserveDimm(ppDimms, &DimmsNum, &pReserveDimm);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (DimmsNum == 0) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_RESERVE_DIMM_REQUIRES_AT_LEAST_TWO_DIMMS);
      ReturnCode = EFI_INVALID_PARAMETER;
      NVDIMM_DBG("Marking Reserve Dimm requires at least two dimms");
      goto Finish;
    }
  }

  /** Calculate total volatile size after including all Dimms in the request**/
  ReturnCode = CalculateDimmCapacityFromPercent(ppDimms, DimmsNum, VolatilePercent, &VolatileSize);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Calculate total Reserved size after including all the Dimms in the request**/
  ReturnCode = CalculateDimmCapacityFromPercent(ppDimms, DimmsNum, ReservedPercent, &ReservedSize);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // TODO: need to refactor this more.
  // PM_TYPE_RESERVED is used to NOT calculate AD capacity
  /** If Volatile and Reserved Percent sum to 100 then never map Appdirect even if alignment would allow it **/
  if (VolatilePercent + ReservedPercent == 100) {
    PersistentMemType = PM_TYPE_RESERVED;
  }

  /** Check platform support **/
  ReturnCode = VerifyPlatformSupport(VolatileSize, PersistentMemType, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = ReadRunTimeDriverPreferences(&DriverPreferences);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Check interleaving settings are correct **/
  ReturnCode = AppDirectSettingsValidation(&DriverPreferences);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("AppDirectSettingValidation Error");
    goto Finish;
  }

  /** Check that the AppDirect Memory in the system does not conflict with preferences **/
  ReturnCode = AppDirectSettingsConflict(&DriverPreferences, &Conflict, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || Conflict) {
    NVDIMM_ERR("AppDirectSettingConflict Errors with CommandStatus set to NVM_ERR_APPDIRECT_IN_SYSTEM");
    goto Finish;
  }
  // allocate helper structures
  pDimmsSymPerSocket = AllocateZeroPool(sizeof(*pDimmsSym) * MAX_DIMMS);
  if (pDimmsSymPerSocket == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  pDimmsAsymPerSocket = AllocateZeroPool(sizeof(*pDimmsAsym) * MAX_DIMMS);
  if (pDimmsAsymPerSocket == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (IS_ACPI_REV_MAJ_1_OR_MAJ_3(PcatRevision)) {
    ReturnCode = RetrieveMaxPMInterleaveSets(&MaxPMInterleaveSets);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (pMaxPMInterleaveSetsPerDie != NULL) {
      *pMaxPMInterleaveSetsPerDie = MaxPMInterleaveSets.MaxInterleaveSetsSplit.PerDie;
    }
  }

  /** Calculate volatile and AD capacities at the socket level **/
  for (Socket = 0; Socket < MAX_SOCKETS; Socket++) {
    DimmsAsymNumPerSocket = 0;
    DimmsSymNumPerSocket = 0;
    ZeroMem(pDimmsOnSocket, sizeof(pDimmsOnSocket[0]) * MAX_DIMMS);

    FilterDimmBySocket(Socket, ppDimms, DimmsNum, pDimmsOnSocket, &NumDimmsOnSocket);

    if (NumDimmsOnSocket <= 0) {
      continue;
    }

    /** Calculate volatile percent **/
    ReturnCode = CalculateDimmCapacityFromPercent(pDimmsOnSocket, NumDimmsOnSocket, VolatilePercent, &VolatileSize);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = MapRequestToActualRegionGoalTemplates(pDimmsOnSocket, NumDimmsOnSocket,
        pDimmsSymPerSocket, &DimmsSymNumPerSocket, pDimmsAsymPerSocket, &DimmsAsymNumPerSocket,
        PersistentMemType, VolatileSize , ReservedPercent, ((IS_ACPI_REV_MAJ_1_OR_MAJ_3(PcatRevision)) ? &MaxPMInterleaveSets : NULL),
        NULL, RegionGoalTemplates, &RegionGoalTemplatesNum, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = ReduceCapacityForSocketSKU(Socket, pDimmsOnSocket, NumDimmsOnSocket, pDimmsSymPerSocket,
      &DimmsSymNumPerSocket, pDimmsAsymPerSocket,
      &DimmsAsymNumPerSocket, RegionGoalTemplates, &RegionGoalTemplatesNum, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("ReduceCapacityForSocketSKU Error");
      goto Finish;
    }

    // Check for NM:FM ratio and set command status if not within limits
    ReturnCode = CheckNmFmLimits((UINT16)Socket, pDimmsSymPerSocket, DimmsSymNumPerSocket, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    /**Add the symmetrical and Asymmetrical size  to the global list **/
    if (pDimmsSym != NULL && pDimmsSymPerSocket != NULL && (DimmsSymNum < MAX_DIMMS)) {
      for (Index = 0; Index < DimmsSymNumPerSocket; Index++) {
        pDimmsSym[DimmsSymNum] = pDimmsSymPerSocket[Index];
        DimmsSymNum++;
      }
    }

    if (pDimmsAsym != NULL && pDimmsAsymPerSocket != NULL && (DimmsAsymNum < MAX_DIMMS)) {
      for (Index = 0; Index < DimmsAsymNumPerSocket; Index++) {
        pDimmsAsym[DimmsAsymNum] = pDimmsAsymPerSocket[Index];
        DimmsAsymNum++;
      }
    }
  }

  // We have removed all Asymmetrical memory, so decrease the number of goal templates
  if (DimmsAsymNum == 0) {
    RegionGoalTemplatesNum = 1;
  }

  // We have removed all symmetrical memory from the system, make the goal template num 0
  if (DimmsSymNum == 0) {
    RegionGoalTemplatesNum = 0;
  }

  /** Update internal driver's structures **/
  ReturnCode = MapRegionsGoal(pDimmsSym, DimmsSymNum, pDimmsAsym, DimmsAsymNum, pReserveDimm, ReserveDimm,
    VolatileSize, RegionGoalTemplates, RegionGoalTemplatesNum, &DriverPreferences, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("MapRegionsGoal Error");
    goto Finish;
  }

  ReturnCode = VerifySKUSupportForCreateGoal(ppDimms, DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("VerifySKUSupportForCreateGoal Error");
    goto Finish;
  }

  if (Examine) {
    SetCmdStatus(pCommandStatus, NVM_SUCCESS);
  } else {
    /** Send Platform Config Data to DIMMs **/
    ReturnCode = ApplyGoalConfigsToDimms(&gNvmDimmData->PMEMDev.Dimms, pCommandStatus);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("ApplyGoalConfigsToDimms Error");
      goto Finish;
    }
    /** Initialize namespace label storage area **/
    ReturnCode = ClearAndInitializeAllLabelStorageAreas(ppDimms, DimmsNum, LabelVersionMajor,
      LabelVersionMinor, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("ClearAndInitializeAllLabelStorageAreas Error");
      goto Finish;
    }

    /* Set pCommandStatus to warning if security state unlocked */
    if (SendGoalConfigWarning) {
      SetCmdStatus(pCommandStatus, NVM_WARN_GOAL_CREATION_SECURITY_UNLOCKED);
    }
  }

Finish:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
  ClearPcdCacheOnDimmList();
  FREE_POOL_SAFE(ppDimms);
  FREE_POOL_SAFE(pDimmsSym);
  FREE_POOL_SAFE(pDimmsAsym);
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pDimmsSymPerSocket);
  FREE_POOL_SAFE(pDimmsAsymPerSocket);
  return ReturnCode;
}

/**
  Delete region goal configuration

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_SUCCESS All Ok
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 DimmSecurityState = 0;
  UINT32 Index = 0;

  SetMem(pDimms, sizeof(pDimms), 0x0);

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  /** Verify input parameters and determine a list of DIMMs **/
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount,
      REQUIRE_DCPMMS_MANAGEABLE |
      REQUIRE_DCPMMS_FUNCTIONAL,
      pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is in locked state.");
      goto Finish;
    }
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE, TRUE);
  if (EFI_VOLUME_CORRUPTED == ReturnCode) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_PCD_BAD_DEVICE_CONFIG);
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    goto Finish;
  }

  /** User has to deconfigure all dimms with goal config on a given socket at once to keep supported region configs **/
  ReturnCode = VerifyDeletingSupportedRegionConfigs(pDimms, DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Update internal driver's structures **/
  ReturnCode = DeleteRegionsGoalConfigs(pDimms, DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Send Platform Config Data to DIMMs **/
  ReturnCode = ApplyGoalConfigsToDimms(&gNvmDimmData->PMEMDev.Dimms, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Dump region goal configuration into the file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pFilePath Name is a pointer to a dump file path
  @param[in] pDevicePath is a pointer to a device where dump file will be stored
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
DumpGoalConfig(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     CHAR16 *pFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
//#ifndef OS_BUILD
  EFI_FILE_HANDLE pFileHandle = NULL;
  DIMM_CONFIG *pDimmConfigs = NULL;
  UINT32 DimmConfigsNum = 0;

#ifndef OS_BUILD
  UINT64 FileSize = 0;
#endif

  NVDIMM_ENTRY();

  if (pThis == NULL || pFilePath == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

#ifndef OS_BUILD
  if (pDevicePath == NULL) {
     goto Finish;
  }
#endif
  ReturnCode = ReenumerateNamespacesAndISs(FALSE);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    goto Finish;
  }

#ifdef OS_BUILD
  //triggers PCD read
  InitializeInterleaveSets(FALSE);
#endif
  /** Get an array of dimms' current config **/
  ReturnCode = GetDimmsCurrentConfig(&pDimmConfigs, &DimmConfigsNum);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (DimmConfigsNum == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_NO_CONFIGURED_DIMMS);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }
#ifndef OS_BUILD
  /** Create new file for dump **/
  ReturnCode = OpenFileByDevice(pFilePath, pDevicePath, TRUE, &pFileHandle);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed on create file to dump Region goal configuration. (" FORMAT_EFI_STATUS ")", ReturnCode);
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPEN_FILE_WITH_WRITE_MODE_FAILED);
    goto Finish;
  }

  /** Get File Size **/
  ReturnCode = GetFileSize(pFileHandle, &FileSize);
  /**
    If file already exists and has some size, delete it
    If file already exists and is empty, use it
  **/
  if (FileSize != 0) {
    ReturnCode = pFileHandle->Delete(pFileHandle);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed on deleting old dump file! (" FORMAT_EFI_STATUS ")", ReturnCode);
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    /** Create new file for dump **/
    ReturnCode = OpenFileByDevice(pFilePath, pDevicePath, TRUE, &pFileHandle);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed on create file to dump Region Goal Configuration. (" FORMAT_EFI_STATUS ")", ReturnCode);
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }
  }
#else
  ReturnCode = OpenFile(pFilePath, &pFileHandle, NULL, 1);
  if (EFI_ERROR(ReturnCode)) {
     NVDIMM_WARN("Failed on open dump file header info. (" FORMAT_EFI_STATUS ")", ReturnCode);
     ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_FILE_OPERATION_FAILED);
     goto Finish;
  }
#endif
  /** Write dump file header **/
  ReturnCode = WriteDumpFileHeader(pFileHandle);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed on write dump file header info. (" FORMAT_EFI_STATUS ")", ReturnCode);
    ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_FILE_OPERATION_FAILED);
    goto Finish;
  }

  /** Perform Dump to File **/
  ReturnCode = DumpConfigToFile(pFileHandle, pDimmConfigs, DimmConfigsNum);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed on write dump. (" FORMAT_EFI_STATUS ")", ReturnCode);
    ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_FILE_OPERATION_FAILED);
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
  ReturnCode = EFI_SUCCESS;

Finish:
  if (pFileHandle != NULL) {
    pFileHandle->Close(pFileHandle);
  }
  FREE_POOL_SAFE(pDimmConfigs);
  NVDIMM_EXIT_I64(ReturnCode);
 // #endif
  return ReturnCode;
}

/**
  Load region goal configuration from file

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] pSocketIds Pointer to an array of Socket IDs
  @param[in] SocketIdsCount Number of items in array of Socket IDs
  @param[in] pFileString Buffer for Region Goal configuration from file
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Socket = 0;
  DIMM_CONFIG *pDimmsConfig = NULL;
  UINT16 DimmIDs[MAX_DIMMS_PER_SOCKET];
  UINT32 DimmIDsNum = 0;
  COMMAND_STATUS *pCmdStatusInternal = NULL;
  UINT32 DimmSecurityState = 0;
  UINT8 PersistentMemType = 0;
  UINT32 VolatilePercent = 0;
  UINT32 ReservedPercent = 0;
  UINT16 LabelVersionMajor = 0;
  UINT16 LabelVersionMinor = 0;
  UINT32 Index = 0;

  ZeroMem(pDimms, sizeof(pDimms));
  ZeroMem(DimmIDs, sizeof(DimmIDs));

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL || pFileString == NULL ||
      (pDimmIds == NULL && DimmIdsCount > 0) || (pSocketIds == NULL && SocketIdsCount > 0)) {
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);
  CHECK_RESULT(ReturnErrorWithMediaDisabledPMemModule(pCommandStatus), Finish);


  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount,
      REQUIRE_DCPMMS_MANAGEABLE |
      REQUIRE_DCPMMS_FUNCTIONAL |
      REQUIRE_DCPMMS_NO_POPULATION_VIOLATION,
      pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }


  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is in locked state.");
      goto Finish;
    }
  }

  // Allocate memory for load Region Goal Configuration
  pDimmsConfig = AllocateZeroPool(sizeof(*pDimmsConfig) * DimmsNum);
  if (pDimmsConfig == NULL) {
    NVDIMM_DBG("Failed to allocate memory for Dimm Config array.");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Parse source file
  ReturnCode = SetUpGoalStructures(pDimms, pDimmsConfig, DimmsNum, pFileString, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("SetUpGoalStructures failed. (" FORMAT_EFI_STATUS ")",ReturnCode);
    goto Finish;
  }

  for (Socket = 0; Socket < MAX_SOCKETS; Socket++) {
    pCmdStatusInternal = NULL;
    ReturnCode = InitializeCommandStatus(&pCmdStatusInternal);
    if (EFI_ERROR(ReturnCode)) {
      FreeCommandStatus(&pCmdStatusInternal);
      goto Finish;
    }

    ReturnCode = ValidateAndPrepareLoadConfig((UINT16)Socket, pDimmsConfig, DimmsNum, DimmIDs, &DimmIDsNum,
      &PersistentMemType, &VolatilePercent, &ReservedPercent, &LabelVersionMajor, &LabelVersionMinor,
      pCmdStatusInternal);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("ValidAndPrepareLoadConfig failed. (" FORMAT_EFI_STATUS ")", ReturnCode);
      SetObjStatus(pCommandStatus, Socket, NULL, 0, pCmdStatusInternal->GeneralStatus, ObjectTypeSocket);
      FreeCommandStatus(&pCmdStatusInternal);
      goto Finish;
    }

    FreeCommandStatus(&pCmdStatusInternal);

    /** No dimm specified for this socket **/
    if (DimmIDsNum == 0) {
      continue;
    }

    pCmdStatusInternal = NULL;
    ReturnCode = InitializeCommandStatus(&pCmdStatusInternal);
    if (EFI_ERROR(ReturnCode)) {
      FreeCommandStatus(&pCmdStatusInternal);
      goto Finish;
    }

    // sanity check
    if (DimmIDsNum > MAX_DIMMS_PER_SOCKET) {
      FreeCommandStatus(&pCmdStatusInternal);
      goto Finish;
    }

    ReturnCode = CreateGoalConfig(pThis, FALSE, DimmIDs, DimmIDsNum, NULL, 0, PersistentMemType, VolatilePercent, ReservedPercent,
        RESERVE_DIMM_NONE, LabelVersionMajor, LabelVersionMinor, NULL, pCmdStatusInternal);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("CreateGoalConfig failed. (" FORMAT_EFI_STATUS ")", ReturnCode);
      SetObjStatus(pCommandStatus, Socket, NULL, 0, pCmdStatusInternal->GeneralStatus, ObjectTypeSocket);
      FreeCommandStatus(&pCmdStatusInternal);
      goto Finish;
    }

    if (NVM_WARN_GOAL_CREATION_SECURITY_UNLOCKED == pCmdStatusInternal->GeneralStatus) {
      SetObjStatus(pCommandStatus, Socket, NULL, 0, pCmdStatusInternal->GeneralStatus, ObjectTypeSocket);
    }

    FreeCommandStatus(&pCmdStatusInternal);

    if (NVM_WARN_GOAL_CREATION_SECURITY_UNLOCKED != pCommandStatus->GeneralStatus) {
      SetObjStatus(pCommandStatus, Socket, NULL, 0, NVM_SUCCESS, ObjectTypeSocket);
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimmsConfig);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Start Diagnostic

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds Pointer to an array of DIMM IDs
  @param[in] DimmIdsCount Number of items in array of DIMM IDs
  @param[in] DiagnosticTests bitfield with selected diagnostic tests to be started
  @param[in] DimmIdPreference Preference for the Dimm ID (handle or UID)
  @param[out] ppResult Pointer to the structure with information about test

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NOT_STARTED Test was not executed
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
EFIAPI
StartDiagnostic(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     CONST UINT8 DiagnosticTests,
  IN     UINT8 DimmIdPreference,
  OUT  DIAG_INFO **ppResultStr
)
{
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  LIST_ENTRY *pDimmList = NULL;
  UINT32 PlatformDimmsCount = 0;
  DIMM *pCurrentDimm = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  ZeroMem(pDimms, sizeof(pDimms));

  NVDIMM_ENTRY();

  if (pThis == NULL || ppResultStr == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  //Validating user-passed Dimm IDs, include all dimms if no IDs are passed
  pDimmList = &gNvmDimmData->PMEMDev.Dimms;
  ReturnCode = GetListSize(pDimmList, &PlatformDimmsCount);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on DimmListSize");
    goto Finish;
  }

  LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (pCurrentDimm == NULL) {
      NVDIMM_DBG("Failed on Get Dimm from node %d", DimmsNum);
      goto Finish;
    }

    pDimms[DimmsNum] = pCurrentDimm;
    DimmsNum++;
  }

  ReturnCode = CoreStartDiagnostics(pDimms, DimmsNum, pDimmIds, DimmIdsCount,
    DiagnosticTests, DimmIdPreference, ppResultStr);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get Driver API Version

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pVersion output version

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetDriverApiVersion(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT CHAR16 pVersion[FW_API_VERSION_LEN]
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NVDIMM_ENTRY();

  if (pVersion == NULL) {
    goto Finish;
  }
#ifdef OS_BUILD
  GetVendorDriverVersion(pVersion, FW_API_VERSION_LEN);
  ReturnCode = EFI_SUCCESS;
#else
  ConvertFwApiVersion(pVersion, NVMDIMM_MAJOR_API_VERSION, NVMDIMM_MINOR_API_VERSION);
  ReturnCode = EFI_SUCCESS;
#endif
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Create namespace
  Creates a AppDirect namespace on the provided region/dimm.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] RegionId the ID of the region that the Namespace is supposed to be created.
  @param[in] Reserved
  @param[in] BlockSize the size of each of the block in the device.
    Valid block sizes are: 1 (for AppDirect Namespace), 512 (default), 514, 520, 528, 4096, 4112, 4160, 4224.
  @param[in] BlockCount the amount of block that this namespace should consist
  @param[in] pName - Namespace name. If NULL, name will be empty.
  @param[in] Mode -  boolean value to decide when the namespace
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
  @retval EFI_OUT_OF_RESOURCES if there is not enough free space on the DIMM/Region.
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system.
**/
  EFI_STATUS
  EFIAPI
  CreateNamespace(
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
  )
  {
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS_PER_SOCKET];
  NVM_IS *pIS = NULL;
  LIST_ENTRY *pRegionList = NULL;
  NAMESPACE *pNamespace = NULL;
  NAMESPACE_LABEL *pLabel = NULL;
  NAMESPACE_LABEL **ppLabels = NULL;
  UINT8 NamespaceType = APPDIRECT_NAMESPACE;
  UINT16 LabelsCount = 0;
  GUID NamespaceGUID;
  UINT64 NamespaceCapacity = 0;
  UINT64 ActualBlockCount = 0;
  UINT64 Index = 0;
  UINT16 Index2 = 0;
  UINT16 LabelsToRemove = 0;
  UINT32 Flags = 0;
  LIST_ENTRY *pNode = NULL;
  BOOLEAN FailFlag = FALSE;
  DIMM_REGION *pDimmRegion = NULL;
  UINT64 ISAvailableCapacity = 0;
  BOOLEAN CapacitySpecified = FALSE;
  UINT64 RequestedCapacity = 0;
  UINT32 DimmCount = 0;
  UINT64 AlignedNamespaceCapacitySize = 0;
  UINT64 RegionSize = 0;
  UINT64 MinSize = 0;
  UINT64 MaxSize = 0;
  MEMMAP_RANGE AppDirectRange;
  BOOLEAN UseLatestLabelVersion = FALSE;
  NAMESPACE *pNamespace1 = NULL;
  ZeroMem(pDimms, sizeof(pDimms));
  ZeroMem(&NamespaceGUID, sizeof(NamespaceGUID));
  ZeroMem(&AppDirectRange, sizeof(AppDirectRange));
  REGION_INFO Region;
  LIST_ENTRY *pDimmList = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  BOOLEAN AreNotPartOfPendingGoal = TRUE;
  DIMM *pCurrentDimm = NULL;

  NVDIMM_ENTRY();

  SetMem(&Region, sizeof(Region), 0x0);

  ReturnCode = GetRegionList(&pRegionList, FALSE);

  if (pThis == NULL || pCommandStatus == NULL || RegionId == REGION_ID_NOTSET || BlockSize == 0 ||
    pActualNamespaceCapacity == NULL || pNamespaceId == NULL || EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  *pActualNamespaceCapacity = 0;

  if (BlockSize != AD_NAMESPACE_BLOCK_SIZE) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_UNSUPPORTED_BLOCK_SIZE);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  CapacitySpecified = BlockCount != NAMESPACE_BLOCK_COUNT_UNDEFINED;

  pIS = GetRegionById(pRegionList, RegionId);
  if (pIS == NULL) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_NOT_FOUND);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = ADNamespaceMinAndMaxAvailableSizeOnIS(pIS, &MinSize, &MaxSize);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NO_RESPONSE) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    goto Finish;
  }

  ReturnCode = GetRegion(pThis, RegionId, &Region, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NO_RESPONSE) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    goto Finish;
  }

  pDimmList = &gNvmDimmData->PMEMDev.Dimms;
  // Loop through all dimms associated with specified region until a match is found
  for (Index = 0; (Index < Region.DimmIdCount) && (AreNotPartOfPendingGoal == TRUE); Index++) {
    UINT32 NodeCount = 0;
    LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
      pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
      if (pCurrentDimm == NULL) {
        NVDIMM_DBG("Failed on Get Dimm from node %d", NodeCount);
        ReturnCode = EFI_NOT_FOUND;
        goto Finish;
      }
      NodeCount++;
      // Valid match found and goal config is pending
      if ((pCurrentDimm->DeviceHandle.AsUint32 == Region.DimmId[Index]) &&
        (pCurrentDimm->GoalConfigStatus == GOAL_CONFIG_STATUS_NEW)) {
        AreNotPartOfPendingGoal = FALSE;
        break;
      }
    }
  }

  if (!AreNotPartOfPendingGoal) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_NAMESPACE_NOT_ALLOWED);
    ReturnCode = EFI_ACCESS_DENIED;
    goto Finish;
  }

  if (CapacitySpecified) {
    /** Calculate namespace capacity to provide **/
    RequestedCapacity = BlockCount * BlockSize;
    if (RequestedCapacity > MaxSize) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_NOT_ENOUGH_FREE_SPACE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    ReturnCode = ConvertUsableSizeToActualSize(BlockSize, RequestedCapacity, Mode,
    &ActualBlockCount, pActualNamespaceCapacity, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  } else {
    // if size is not specified for Namespace, then find the maximum available size
    NamespaceCapacity = 0;
    ReturnCode = GetListSize(&pIS->DimmRegionList, &DimmCount);
    if (EFI_ERROR(ReturnCode) || DimmCount == 0) {
      goto Finish;
    }
    /** Find the free capacity**/
    ReturnCode = FindADMemmapRangeInIS(pIS, MAX_UINT64_VALUE, &AppDirectRange);
    if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_NOT_FOUND) {
      if (ReturnCode == EFI_NO_RESPONSE) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
      }
      goto Finish;
    }
    ReturnCode = EFI_SUCCESS;
    ISAvailableCapacity = AppDirectRange.RangeLength * DimmCount;
    if (ISAvailableCapacity > NamespaceCapacity) {
      NamespaceCapacity = ISAvailableCapacity;
    }
    if (NamespaceCapacity == 0) {
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
    *pActualNamespaceCapacity = NamespaceCapacity;
    ActualBlockCount = *pActualNamespaceCapacity / BlockSize;
  }

  if (*pActualNamespaceCapacity < MinSize || MinSize == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_NAMESPACE_CAPACITY);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  /** Namespace capacity that doesn't include block size with 64B cache lane size **/
  NamespaceCapacity = ActualBlockCount * BlockSize;
  ReturnCode = GetListSize(&pIS->DimmRegionList, &DimmCount);
  if (EFI_ERROR(ReturnCode) || DimmCount == 0) {
    goto Finish;
  }

  ReturnCode = AlignNamespaceCapacity(NamespaceCapacity, DimmCount, &AlignedNamespaceCapacitySize);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Namespace Capacity is an invalid parameter");
    ReturnCode = EFI_INVALID_PARAMETER;
  }

  RegionSize = AlignedNamespaceCapacitySize / DimmCount;

  ReturnCode = FindADMemmapRangeInIS(pIS, RegionSize, &AppDirectRange);
  if (EFI_NOT_FOUND == ReturnCode) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_NOT_ENOUGH_FREE_SPACE);
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG(" AD Mem Map Range not found for pIS");
    goto Finish;
  }

/**
Build NAMESPACE structure
**/
  pNamespace = (NAMESPACE *) AllocateZeroPool(sizeof(*pNamespace));
  if (pNamespace == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pNamespace->NamespaceId = GenerateNamespaceId(RegionId);
  pNamespace->Enabled = TRUE;
  pNamespace->Signature = NAMESPACE_SIGNATURE;
  pNamespace->Flags.Values.ReadOnly = FALSE;

#ifdef WA_ENABLE_LOCAL_FLAG_ON_NS_LABEL_1_2
  // Label spec 1.1 LOCAL flag only used for storage mode
  // Label spec 1.2 LOCAL flag *may* be used for interleave sets on a single device
  if(!(pNamespace->Major == 1 && pNamespace->Minor < 2)) {
    if (RegionCount == 1) {
      pNamespace->Flags.Values.Local = TRUE;
    }
    else {
      pNamespace->Flags.Values.Local = FALSE;
    }
  }
#endif // WA_ENABLE_LOCAL_FLAG_ON_NS_LABEL_1_2

  pNamespace->NamespaceType = NamespaceType;
  if (Mode) {
    pNamespace->IsBttEnabled = TRUE;
  } else {
    pNamespace->IsRawNamespace = TRUE;
  }
  pNamespace->BlockSize = BlockSize;

  // AppDirect namespaces initially stored with 'updating flag'.
  pNamespace->Flags.Values.Updating = TRUE;
  pNamespace->pParentIS = pIS;

  if (pName != NULL) {
    CopyMem_S(&pNamespace->Name, sizeof(pNamespace->Name), pName, MIN(AsciiStrLen(pName), NSLABEL_NAME_LEN));
  }

  GenerateNSGUID:
  GenerateRandomGuid(&NamespaceGUID);

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Namespaces) {
    pNamespace1 = NAMESPACE_FROM_NODE(pNode, NamespaceNode);
    if (CompareMem(&pNamespace1->NamespaceGuid, &NamespaceGUID, NSGUID_LEN) == 0)
    {
      goto GenerateNSGUID;
    }
  }

  CopyMem_S(&pNamespace->NamespaceGuid, sizeof(pNamespace->NamespaceGuid), &NamespaceGUID, NSGUID_LEN);
  /** Provision Namespace Capacity using only Region Id **/
  ReturnCode = AllocateNamespaceCapacity(NULL, pIS, pActualNamespaceCapacity, pNamespace);
  pNamespace->BlockCount = *pActualNamespaceCapacity / GetPhysicalBlockSize(pNamespace->BlockSize);
  pNamespace->UsableSize = *pActualNamespaceCapacity;
  if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_NOT_ENOUGH_FREE_SPACE);
    FailFlag = TRUE;
    goto Finish;
  }
  if (pNamespace->RangesCount > MAX_NAMESPACE_RANGES) {
    FailFlag = TRUE;
    goto Finish;
  }

  if (!ForceAlignment && CapacitySpecified && (RequestedCapacity != *pActualNamespaceCapacity)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_BADALIGNMENT);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Find the label version to use
  ReturnCode = UseLatestNsLabelVersion(pIS, pNamespace->pParentDimm, &UseLatestLabelVersion);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ppLabels = AllocateZeroPool(MAX_NAMESPACE_RANGES * sizeof(NAMESPACE_LABEL *));
  if (ppLabels == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  for (Index2 = 0; Index2 < pNamespace->RangesCount; Index2++) {
    pLabel = (NAMESPACE_LABEL *) AllocateZeroPool(sizeof(*pLabel));
    if (pLabel == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    if (UseLatestLabelVersion) {
      pNamespace->Media.BlockSize = AD_NAMESPACE_LABEL_LBA_SIZE_4K;
      pNamespace->Major = NSINDEX_MAJOR;
      pNamespace->Minor = NSINDEX_MINOR_2;
      ReturnCode = CreateNamespaceLabels(pNamespace, Index2, TRUE, pLabel);
      if (EFI_ERROR(ReturnCode)) {
        FailFlag = TRUE;
        goto Finish;
      }
    } else {
      pNamespace->Media.BlockSize = AD_NAMESPACE_LABEL_LBA_SIZE_512;
      pNamespace->Major = NSINDEX_MAJOR;
      pNamespace->Minor = NSINDEX_MINOR_1;
      ReturnCode = CreateNamespaceLabels(pNamespace, Index2, FALSE, pLabel);
      if (EFI_ERROR(ReturnCode)) {
        FailFlag = TRUE;
        goto Finish;
      }
    }
    ppLabels[Index2] = pLabel;
    LabelsCount++;
  }

  /** For BTT Namespaces the structures need to be laid out before updating LSA **/
  if (pNamespace->IsBttEnabled) {
    pNamespace->pBtt = BttInit(
                        GetAccessibleCapacity(pNamespace),
                        (UINT32)GetBlockDeviceBlockSize(pNamespace),
                        (GUID *)pNamespace->NamespaceGuid,
                        pNamespace
                        );
    if (pNamespace->pBtt == NULL) {
      NVDIMM_DBG("Failed to initialize the BTT. Namespace GUID: %g\n", (GUID *)pNamespace->NamespaceGuid);
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      FailFlag = TRUE;
      goto Finish;
    }
    // Crating the namespace triggers writing BTT structures
    ReturnCode = BttWriteLayout(pNamespace->pBtt, TRUE);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to write BTT data");
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      FailFlag = TRUE;
      goto Finish;
    }
  }

/**
Update NAMESPACE_INDEX with NAMESPACE_LABEL(s)
**/
  Index2 = 0;
  LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pNode);
    ReturnCode = InsertNamespaceLabels(pDimmRegion->pDimm, &ppLabels[Index2], 1,
      pNamespace->Major, pNamespace->Minor);
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_SECURITY_STATE);
      }
      //cleanup already written labels and exit
      LabelsToRemove = (UINT16)Index2;
      Index2 = 0;
      LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {
        pDimmRegion = DIMM_REGION_FROM_NODE(pNode);
        if (Index2 >= LabelsToRemove) {
          break;
        }
        RemoveNamespaceLabels(pDimmRegion->pDimm, &ppLabels[Index2]->Uuid, 0);
        Index2++;
      }
      FailFlag = TRUE;
      goto Finish;
    }
    Index2++;
  }
  if (Index2 != LabelsCount) {
    NVDIMM_DBG("Number of labels written is not equal to number of labels.");
    ResetCmdStatus(pCommandStatus, NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN);
    FailFlag = TRUE;
    goto Finish;
  }
/**
All ok at this point but one more run is needed to clear updating flag
**/
  Index2 = 0;
  LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pNode);
    ppLabels[Index2]->Flags.Values.Updating = FALSE;
    Flags = ppLabels[Index2]->Flags.AsUint32;
    ReturnCode = ModifyNamespaceLabels(pDimmRegion->pDimm, &ppLabels[Index2]->Uuid, &Flags, NULL, 0);
    if (EFI_ERROR(ReturnCode)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN);
      FailFlag = TRUE;
      goto Finish;
    }
    Index2++;
  }
  InsertTailList(&pIS->AppDirectNamespaceList, &pNamespace->IsNode);

  pNamespace->HealthState = NAMESPACE_HEALTH_OK;

  InsertTailList(&gNvmDimmData->PMEMDev.Namespaces, &pNamespace->NamespaceNode);
  *pNamespaceId = pNamespace->NamespaceId;

  ReturnCode = InstallNamespaceProtocols(pNamespace);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NOT_READY || ReturnCode == EFI_ACCESS_DENIED) {
      NVDIMM_WARN("Namespace not enabled or invalid DIMM security state! Skipping the protocols installation.");
      ReturnCode = EFI_SUCCESS;
    } else if (ReturnCode == EFI_ABORTED) {
      // The Block Window was not initialized so the platform did not enable the block mode.
      pNamespace->Enabled = FALSE;
      NVDIMM_DBG("Block Window equals NULL");
      ResetCmdStatus(pCommandStatus, NVM_WARN_BLOCK_MODE_DISABLED);
      goto Finish;
    }
  }

  if (!EFI_ERROR(ReturnCode)) {
    SetCmdStatus(pCommandStatus, NVM_SUCCESS);
    ReturnCode = EFI_SUCCESS;
  } else {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
  }

Finish:
  for (Index = 0; Index < LabelsCount; Index++) {
    FREE_POOL_SAFE(ppLabels[Index]);
  }

  FREE_POOL_SAFE(ppLabels);

  if (FailFlag) {
    if (pNamespace->DimmNode.BackLink != NULL && pNamespace->DimmNode.ForwardLink != NULL) {
      RemoveEntryList(&pNamespace->DimmNode);
    }
    FREE_POOL_SAFE(pNamespace);
    ReturnCode = EFI_ABORTED;
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get namespaces info

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pNamespaceListNode - pointer to namespace list node
  @param[out] pNamespacesCount - namespace count
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetNamespaces (
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN OUT LIST_ENTRY *pNamespaceListNode,
     OUT UINT32 *pNamespacesCount,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NAMESPACE *pNamespace = NULL;
  NAMESPACE_INFO *pNamespaceInfo = NULL;
  LIST_ENTRY *pNode = NULL;

  if (pThis == NULL || pNamespaceListNode == NULL || pNamespacesCount == NULL || pCommandStatus == NULL) {
    goto Finish;
  }
  ReturnCode = ReenumerateNamespacesAndISs(FALSE);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_BUSY_DEVICE);
    }
    else if (EFI_VOLUME_CORRUPTED == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_PCD_BAD_DEVICE_CONFIG);
    }
#ifdef OS_BUILD
      goto Finish;
#else
      if ((ReturnCode == EFI_NOT_FOUND && IsLsaNotInitializedOnADimm()))
      {
          NVDIMM_WARN("Failure to refresh Namespaces is because LSA not initialized");
      }
      else
      {
          goto Finish;
      }
#endif
  }

  *pNamespacesCount = 0;
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Namespaces) {
    pNamespace = NAMESPACE_FROM_NODE(pNode, NamespaceNode);
    pNamespaceInfo = AllocateZeroPool(sizeof(*pNamespaceInfo));
    if (pNamespaceInfo == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    pNamespaceInfo->NamespaceId = pNamespace->NamespaceId;
    CopyMem_S(pNamespaceInfo->NamespaceGuid, sizeof(pNamespaceInfo->NamespaceGuid), pNamespace->NamespaceGuid, sizeof(pNamespaceInfo->NamespaceGuid));
    CopyMem_S(pNamespaceInfo->Name, sizeof(pNamespaceInfo->Name), pNamespace->Name, sizeof(pNamespaceInfo->Name));
    pNamespaceInfo->HealthState = pNamespace->HealthState;
    pNamespaceInfo->BlockSize = pNamespace->BlockSize;
    pNamespaceInfo->LogicalBlockSize = pNamespace->Media.BlockSize;
    pNamespaceInfo->BlockCount = pNamespace->BlockCount;
    pNamespaceInfo->UsableSize = pNamespace->UsableSize;
    pNamespaceInfo->Major = pNamespace->Major;
    pNamespaceInfo->Minor = pNamespace->Minor;

    pNamespaceInfo->NamespaceMode = pNamespace->IsBttEnabled ? SECTOR_MODE :
                                    ((pNamespace->IsPfnEnabled) ? FSDAX_MODE : NONE_MODE);
    pNamespaceInfo->NamespaceType = pNamespace->NamespaceType;

    pNamespaceInfo->RegionId = pNamespace->pParentIS->RegionId;

    pNamespaceInfo->Signature = NAMESPACE_INFO_SIGNATURE;
    InsertTailList(pNamespaceListNode, (LIST_ENTRY *) pNamespaceInfo->NamespaceInfoNode);
    (*pNamespacesCount)++;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;

}

/**
  Delete namespace
  Deletes a block or persistent memory namespace.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] Force Force to perform deleting namespace configs on all affected DIMMs
  @param[in] NamespaceId the ID of the namespace to be removed.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS if the operation was successful.
  @retval EFI_NOT_FOUND if a namespace with the provided GUID does not exist in the system.
  @retval EFI_DEVICE_ERROR if there was a problem with writing the configuration to the device.
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system.
**/
EFI_STATUS
EFIAPI
DeleteNamespace(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     BOOLEAN Force,
  IN     UINT16 NamespaceId,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NAMESPACE *pNamespace = NULL;
  UINT16 Index = 0;
  UINT16 RangesRemoved = 0;
  BOOLEAN NamespaceLocked = FALSE;

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  pNamespace = GetNamespaceById(NamespaceId);

  if (pNamespace == NULL) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_NAMESPACE_DOES_NOT_EXIST);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = IsNamespaceLocked(pNamespace, &NamespaceLocked);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (NamespaceLocked) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_SECURITY_STATE);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pNamespace->Flags.Values.ReadOnly) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_NAMESPACE_READ_ONLY);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (!Force) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_FORCE_REQUIRED);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pNamespace->Enabled) {
    ReturnCode = UninstallNamespaceProtocols(pNamespace);
    if (EFI_ERROR(ReturnCode)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_NAMESPACE_COULD_NOT_UNINSTALL);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    if (pNamespace->IsBttEnabled && pNamespace->pBtt != NULL) {
      BttRelease(pNamespace->pBtt);
      pNamespace->pBtt = NULL;
    }
  }

  for (Index = 0; Index < pNamespace->RangesCount; Index++) {
    NVDIMM_DBG("pDimm=%p, GUID=%g", pNamespace->Range[Index].pDimm,
      (GUID *) &pNamespace->NamespaceGuid);
    ReturnCode = RemoveNamespaceLabels(pNamespace->Range[Index].pDimm,
      (GUID *) &pNamespace->NamespaceGuid, 0);
    if (!EFI_ERROR(ReturnCode)) {
      RangesRemoved++;
    }
  }

  if (RangesRemoved == 0) {
    // Nothing removed, NS config structure should be still consistent
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  } else if (RangesRemoved < pNamespace->RangesCount) {
    // Not all labels removed, NS configuration structure has been broken
    // NS will be removed but need to re-enumerate volumes and pools
    ReenumerateNamespacesAndISs(TRUE);

    ResetCmdStatus(pCommandStatus, NVM_ERR_NAMESPACE_CONFIGURATION_BROKEN);
    ReturnCode = EFI_ABORTED;
  }

  if (pNamespace->pParentIS != NULL) {
    RemoveEntryList(&pNamespace->IsNode);
  } else if (pNamespace->pParentDimm != NULL) {
    /** Remove Block Namespace from list of Namespaces on Dimm **/
    RemoveEntryList(&pNamespace->DimmNode);
  }
  RemoveEntryList(&pNamespace->NamespaceNode);
  FREE_POOL_SAFE(pNamespace);
  SetCmdStatus(pCommandStatus, NVM_SUCCESS);
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get Error log for given dimm

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
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
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     CONST UINT32 DimmsCount,
  IN     CONST BOOLEAN ThermalError,
  IN     CONST UINT16 SequenceNumber,
  IN     CONST BOOLEAN HighLevel,
  IN OUT UINT32 *pErrorLogCount,
     OUT ERROR_LOG_INFO *pErrorLogs,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  UINT32 SingleDimmErrorsFetched = 0;
  UINT32 AllErrorsFetched = 0;

  SetMem(pDimms, sizeof(pDimms), 0x0);

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL || pErrorLogCount == NULL || pErrorLogs == NULL ||
      (pDimmIds == NULL && DimmsCount > 0)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  /** Verify input parameters and determine a list of DIMMs **/
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmsCount, NULL, 0,
      REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL,
      pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
  for (Index = 0; Index < DimmsNum && AllErrorsFetched < *pErrorLogCount; ++Index) {
    ReturnCode = GetAndParseFwErrorLogForDimm(pDimms[Index],
      ThermalError,
      HighLevel,
      SequenceNumber,
      (*pErrorLogCount - AllErrorsFetched),
      &SingleDimmErrorsFetched,
      &pErrorLogs[AllErrorsFetched]);

    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    AllErrorsFetched += SingleDimmErrorsFetched;
  }

Finish:
  if (pErrorLogCount != NULL) {
    *pErrorLogCount = AllErrorsFetched;
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get the debug log from a specified dimm and fw debug log source

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID identifier of what dimm to get log pages from
  @param[in] LogSource debug log source buffer to retrieve
  @param[in] Reserved for future use. Must be 0 for now.
  @param[out] ppDebugLogBuffer - an allocated buffer containing the raw debug log
  @param[out] pDebugLogBufferSize - the size of the raw debug log buffer
  @param[out] pCommandStatus structure containing detailed NVM error codes

  Note: The caller is responsible for freeing the returned buffer

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;

  NVDIMM_ENTRY();

  if (pThis == NULL || ppDebugLogBuffer == NULL || pDebugLogBufferSize == NULL ||
    pCommandStatus == NULL || Reserved != 0 || LogSource > FW_DEBUG_LOG_SOURCE_MAX) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.Dimms);

  // If we still can't find the dimm, fail out
  if (pDimm == NULL) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_DIMM_NOT_FOUND);
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    SetObjStatus(pCommandStatus, pDimm->DeviceHandle.AsUint32, NULL, 0, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND, ObjectTypeDimm);
    goto Finish;
  }

  ReturnCode = FwCmdGetFwDebugLog(pDimm, LogSource, ppDebugLogBuffer, pDebugLogBufferSize, pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_SECURITY_VIOLATION) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE);
    } else if (ReturnCode == EFI_NO_MEDIA) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_MEDIA_DISABLED);
    } else {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_OPERATION_FAILED);
    }
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set Optional Configuration Data Policy using FW command

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDimmIds - pointer to array of UINT16 PMem module ids to set
  @param[in] DimmIdsCount - number of elements in pDimmIds
  @param[in] Reserved
  @param[in] AveragePowerReportingTimeConstant - (FIS 2.1 and greater) AveragePowerReportingTimeConstant value to set
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_UNSUPPORTED Mixed Sku of PMem modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
  @retval EFI_NO_RESPONSE FW busy for one or more PMem modules
**/
EFI_STATUS
EFIAPI
SetOptionalConfigurationDataPolicy(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 *Reserved,
  IN     UINT32 *pAveragePowerReportingTimeConstant,
     OUT COMMAND_STATUS *pCommandStatus
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PT_OPTIONAL_DATA_POLICY_PAYLOAD OptionalDataPolicyPayload;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  UINT8 DimmARSStatus = 0;

  SetMem(pDimms, sizeof(pDimms), 0x0);
  ZeroMem(&OptionalDataPolicyPayload, sizeof(OptionalDataPolicyPayload));

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL || (pDimmIds == NULL && DimmIdsCount > 0)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  CHECK_RESULT(BlockMixedSku(pCommandStatus), Finish);

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0,
      REQUIRE_DCPMMS_MANAGEABLE |
      REQUIRE_DCPMMS_FUNCTIONAL,
      pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = FwCmdGetOptionalConfigurationDataPolicy(pDimms[Index], &OptionalDataPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_DEVICE_ERROR;
      goto Finish;
    }

    if (NULL != pAveragePowerReportingTimeConstant) {
      OptionalDataPolicyPayload.Payload.Fis_2_01.AveragePowerReportingTimeConstant = *pAveragePowerReportingTimeConstant;
    }
    else {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_NOT_SUPPORTED);
      continue;
    }

    ReturnCode = FwCmdGetARS(pDimms[Index], &DimmARSStatus);
    if (LONG_OP_STATUS_IN_PROGRESS == DimmARSStatus) {
      NVDIMM_ERR("ARS in progress.\n");
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ARS_IN_PROGRESS);
      pCommandStatus->GeneralStatus = NVM_ERR_ARS_IN_PROGRESS;
      ReturnCode = EFI_DEVICE_ERROR;
      goto Finish;
    }

    ReturnCode = FwCmdSetOptionalConfigurationDataPolicy(pDimms[Index], &OptionalDataPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      }
      else if (ReturnCode == EFI_NO_RESPONSE) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_BUSY_DEVICE);
      }
      else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_FW_SET_OPTIONAL_DATA_POLICY_FAILED);
      }
    }
    else {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
    }
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get requested number of specific DIMM registers for given DIMM id

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmId - ID of a DIMM.
  @param[out] pBsr - Pointer to buffer for Boot Status register, contains
              high and low 4B register.
  @param[out] Reserved
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
RetrieveDimmRegisters(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmId,
     OUT UINT64 *pBsr,
     OUT UINT8 *Reserved,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT16 BootStatusBitmask = 0;

  NVDIMM_ENTRY();

  if (pThis == NULL || pBsr == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  CHECK_RESULT(VerifyTargetDimms(&DimmId, 1, NULL, 0,
    REQUIRE_DCPMMS_MANAGEABLE, pDimms, &DimmsNum, pCommandStatus), Finish);

  ReturnCode = pThis->GetBSRAndBootStatusBitMask(pThis, pDimms[0]->DimmID, pBsr, &BootStatusBitmask);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS);
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;

}

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
  @retval EFI_NOT_FOUND The driver could not find the encoded in pCmd DIMM in the system.
  @retval EFI_DEVICE_ERROR FW error received.
  @retval EFI_UNSUPPORTED if the command is ran not in the DEBUG version of the driver.
  @retval EFI_TIMEOUT A timeout occurred while waiting for the protocol command to execute.
**/
EFI_STATUS
EFIAPI
PassThruCommand(
  IN OUT NVM_FW_CMD *pCmd,
  IN     UINT64 Timeout
  )
{
#ifdef MDEPKG_NDEBUG
  return EFI_UNSUPPORTED;
#else /* MDEPKG_NDEBUG */
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pDimm = GetDimmByPid(pCmd->DimmID, &gNvmDimmData->PMEMDev.Dimms);

  if (pDimm == NULL || !IsDimmManageable(pDimm)) {
    NVDIMM_DBG("Could not find the specified DIMM or it is unmanageable.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = PassThru(pDimm, pCmd, Timeout);
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
#endif
}

EFI_STATUS
EFIAPI
DimmFormat(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 *pDimmIds,
  IN     UINT32 DimmIdsCount,
  IN     BOOLEAN Recovery,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS *pReturnCodes = NULL;
#ifdef FORMAT_SUPPORTED
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  DIMM_BSR Bsr;
#endif
  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

#ifndef FORMAT_SUPPORTED
  ReturnCode = EFI_UNSUPPORTED;
  ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED);
#else
  ZeroMem(pDimms, sizeof(pDimms));
  ZeroMem(&Bsr, sizeof(Bsr));


  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL, pDimms, &DimmsNum, pCommandStatus);

  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  pReturnCodes = AllocateZeroPool(sizeof(EFI_STATUS) * DimmsNum);

  if (!pReturnCodes) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    pReturnCodes[Index] = FwCmdFormatDimm(pDimms[Index]);
  }
#ifndef OS_BUILD
  UINT64 FwMailboxStatus = 0;
  /* If previous timeout then go back and wait max format time */
  for (Index = 0; Index < DimmsNum; Index++) {
    if (pReturnCodes[Index] == EFI_TIMEOUT) {
      if (Recovery) {
        pReturnCodes[Index] = PollSmbusCmdCompletion(pDimms[Index]->SmbusAddress,
        PT_FORMAT_DIMM_MAX_TIMEOUT, &FwMailboxStatus, &Bsr);
      }

      if (EFI_ERROR(pReturnCodes[Index])) {
        continue;
      }
      }
    }
#endif
  for (Index = 0; Index < DimmsNum; Index++) {
    if (!EFI_ERROR(pReturnCodes[Index])) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
    } else if (pReturnCodes[Index] == EFI_TIMEOUT) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_TIMEOUT);
    } else {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
    }
    KEEP_ERROR(ReturnCode,pReturnCodes[Index]);
  }
#endif /* MDEPKG_NDEBUG */
Finish:
  FREE_POOL_SAFE(pReturnCodes);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Parse ACPI tables and create DIMM list

  @retval EFI_SUCCESS  Success
  @retval EFI_...      Other errors from subroutines
**/
EFI_STATUS
FillDimmList(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  EFI_STATUS TmpReturnCode = EFI_SUCCESS;
  NvmStatusCode StatusCode = NVM_SUCCESS;
  LIST_ENTRY *pNode = NULL;
  DIMM *pCurDimm = NULL;
  DIMM *pFirstDimm = NULL;
  UINT32 ListSize = 0;
  NVDIMM_ENTRY();

  /* initialize the dimm inventory from the ACPI tables */
  TmpReturnCode = InitializeDimmInventory(&gNvmDimmData->PMEMDev);
  if (EFI_ERROR(TmpReturnCode)) {
    NVDIMM_WARN("Initialization of one or more Dimms failed.");
    ReturnCode = TmpReturnCode;
    /** Continue even if error occurs **/
  }

  TmpReturnCode = GetListSize(&gNvmDimmData->PMEMDev.Dimms, &ListSize);
  if (EFI_ERROR(TmpReturnCode)) {
    ReturnCode = TmpReturnCode;
    goto Finish;
  }

  gNvmDimmData->PMEMDev.DimmSkuConsistency = TRUE;
  pFirstDimm = DIMM_FROM_NODE(GetFirstNode(&gNvmDimmData->PMEMDev.Dimms));

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurDimm = DIMM_FROM_NODE(pNode);

    StatusCode = IsDimmSkuModeMismatch(pFirstDimm, pCurDimm);
    if (StatusCode != NVM_SUCCESS) {
      gNvmDimmData->PMEMDev.DimmSkuConsistency = FALSE;
      pCurDimm->MixedSKUOffender = TRUE;
    }
  }

  NVDIMM_DBG("Found %d DCPMMs", ListSize);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

/**
  Clean up the in memory DIMM inventory

  @retval EFI_SUCCESS  Success
  @retval EFI_...      Other errors from subroutines
**/
EFI_STATUS
FreeDimmList(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;


  NVDIMM_ENTRY();

  ReturnCode = RemoveDimmInventory(&gNvmDimmData->PMEMDev);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to remove dimm inventory.");
  }
  if (gNvmDimmData->PMEMDev.pFitHead != NULL) {
    FreeParsedNfit(&gNvmDimmData->PMEMDev.pFitHead);
    gNvmDimmData->PMEMDev.pFitHead = NULL;
  }

  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

/**
  Get Total DCPMM Volatile, AppDirect, Unconfigured, Reserved and Inaccessible capacities

  @param[in]  pDimms The head of the dimm list
  @param[out] pRawCapacity  pointer to raw capacity
  @param[out] pVolatileCapacity  pointer to volatile capacity
  @param[out] pAppDirectCapacity pointer to appdirect capacity
  @param[out] pUnconfiguredCapacity pointer to unconfigured capacity
  @param[out] pReservedCapacity pointer to reserved capacity
  @param[out] pInaccessibleCapacity pointer to inaccessible capacity

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_LOAD_ERROR PCD CCUR table missing in one or more DIMMs
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetTotalDcpmmCapacities(
  IN     LIST_ENTRY *pDimms,
  OUT UINT64 *pRawCapacity,
  OUT UINT64 *pVolatileCapacity,
  OUT UINT64 *pAppDirectCapacity,
  OUT UINT64 *pUnconfiguredCapacity,
  OUT UINT64 *pReservedCapacity,
  OUT UINT64 *pInaccessibleCapacity
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT64 RawCapacity = 0;
  UINT64 VolatileCapacity = 0;
  UINT64 AppDirectCapacity = 0;
  UINT64 UnconfiguredCapacity = 0;
  UINT64 ReservedCapacity = 0;
  UINT64 InaccessibleCapacity = 0;
  BOOLEAN ArgumentsNotNull = FALSE;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pRawCapacity == NULL || pVolatileCapacity == NULL || pUnconfiguredCapacity == NULL ||
    pReservedCapacity == NULL || pAppDirectCapacity == NULL || pInaccessibleCapacity == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  ArgumentsNotNull = TRUE;

  // All shall be zero to start
  *pRawCapacity = *pUnconfiguredCapacity = *pAppDirectCapacity = *pReservedCapacity = *pInaccessibleCapacity = *pVolatileCapacity = 0;

  LIST_FOR_EACH(pDimmNode, pDimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    ReturnCode = GetDcpmmCapacities(pDimm->DimmID, &RawCapacity, &VolatileCapacity,
      &AppDirectCapacity, &UnconfiguredCapacity, &ReservedCapacity, &InaccessibleCapacity);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to retrieve capacities for DIMM: 0x%04x", pDimm->DeviceHandle.AsUint32);
      goto Finish;
    }

    *pRawCapacity += RawCapacity;
    *pVolatileCapacity += VolatileCapacity;
    *pReservedCapacity += ReservedCapacity;
    *pAppDirectCapacity += AppDirectCapacity;
    *pInaccessibleCapacity += InaccessibleCapacity;
    *pUnconfiguredCapacity += UnconfiguredCapacity;
  }

Finish:
  if (EFI_ERROR(ReturnCode) && ArgumentsNotNull == TRUE) {
    // Data is invalid, set to unknown
    *pRawCapacity = *pUnconfiguredCapacity = *pAppDirectCapacity = *pReservedCapacity = *pInaccessibleCapacity = *pVolatileCapacity = ACPI_TABLE_VALUE_UNKNOWN;
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Gather capacities from dimm

  @param[in]  DimmPid The ID of the DIMM
  @param[out] pRawCapacity pointer to raw capacity
  @param[out] pVolatileCapacity pointer to volatile capacity
  @param[out] pAppDirectCapacity pointer to appdirect capacity
  @param[out] pUnconfiguredCapacity pointer to unconfigured capacity
  @param[out] pReservedCapacity pointer to reserved capacity
  @param[out] pInaccessibleCapacity pointer to inaccessible capacity

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval Other errors failure of FW commands
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetDcpmmCapacities(
  IN     UINT16 DimmPid,
     OUT UINT64 *pRawCapacity,
     OUT UINT64 *pVolatileCapacity,
     OUT UINT64 *pAppDirectCapacity,
     OUT UINT64 *pUnconfiguredCapacity,
     OUT UINT64 *pReservedCapacity,
     OUT UINT64 *pInaccessibleCapacity
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  MEMORY_MODE CurrentMode = MEMORY_MODE_1LM;

  NVDIMM_ENTRY();

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);

  if (pDimm == NULL || pRawCapacity == NULL || pVolatileCapacity == NULL || pUnconfiguredCapacity == NULL ||
      pReservedCapacity == NULL || pAppDirectCapacity == NULL || pInaccessibleCapacity == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // All shall be zero to start
  *pRawCapacity = *pUnconfiguredCapacity = *pAppDirectCapacity = *pReservedCapacity = *pInaccessibleCapacity = *pVolatileCapacity = 0;

  *pRawCapacity = pDimm->RawCapacity;

  if (!IsDimmManageable(pDimm) || DIMM_MEDIA_NOT_ACCESSIBLE(pDimm->BootStatusBitmask)) {
    *pInaccessibleCapacity = pDimm->RawCapacity;
    goto Finish;
  }

#ifdef OS_BUILD
  ReturnCode = GetDimmMappedMemSize(pDimm);
  if (EFI_DEVICE_ERROR == ReturnCode) {
    NVDIMM_WARN("Failed to retrieve PCD data on DIMM: 04x%x", pDimm->DeviceHandle.AsUint32);
  }
  else if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
#endif // OS_BUILD

  // PCD CCUR table missing in DIMM
  if (pDimm->ConfigStatus == DIMM_CONFIG_UNDEFINED) {
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  ReturnCode = CurrentMemoryMode(&CurrentMode);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to determine current memory mode");
    goto Finish;
  }

  if (pDimm->NonFunctional || IsDimmInUnmappedPopulationViolation(pDimm)) {
    // DIMM not mapped into SPA space
    *pInaccessibleCapacity = pDimm->RawCapacity;
    // No usable capacity
    *pAppDirectCapacity = *pReservedCapacity = *pUnconfiguredCapacity = *pVolatileCapacity = 0;
    goto Finish;
  } else if (!IS_BIOS_VOLATILE_MEMORY_MODE_2LM(CurrentMode) && !pDimm->Configured) {
    //DIMM is unconfigured and system is in 1LM mode
    *pUnconfiguredCapacity = pDimm->RawCapacity;
    // No usable capacity
    *pAppDirectCapacity = *pReservedCapacity = *pInaccessibleCapacity = *pVolatileCapacity = 0;
    goto Finish;
  } else {
    // Any capacity not mapped to a partition
    *pInaccessibleCapacity = pDimm->RawCapacity - pDimm->VolatileCapacity - pDimm->PmCapacity;
  }

  // Calculate Volatile Capacity
  if ((pDimm->SkuInformation.MemoryModeEnabled == MODE_ENABLED) && IS_BIOS_VOLATILE_MEMORY_MODE_2LM(CurrentMode)) {
    *pVolatileCapacity = pDimm->MappedVolatileCapacity;
    *pInaccessibleCapacity += pDimm->VolatileCapacity - pDimm->MappedVolatileCapacity;
  } else {
    // 1LM so none of the partitioned volatile is mapped. Set it as inaccessible.
    *pInaccessibleCapacity += pDimm->VolatileCapacity;
  }

  // Calculate AppDirect Capacity
  if (pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
    *pAppDirectCapacity = pDimm->MappedPersistentCapacity;

    // Calculate Reserved Capacity
    ReturnCode = GetReservedCapacity(pDimm, pReservedCapacity);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to retrieve DCPMM reserved capacity.");
      goto Finish;
    }

    // PM partition inaccessible due to alignment/rounding
    *pInaccessibleCapacity += pDimm->PmCapacity - pDimm->MappedPersistentCapacity - *pReservedCapacity;
  } else {
    *pInaccessibleCapacity += pDimm->PmCapacity;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate the number of channels with at least one usable DDR for 1LM+2LM

  @param[in]  SocketId Socket Id
  @param[out] pChannelCount Pointer to Channel Count

  @retval EFI_INVALID_PARAMETER Passed NULL argument
  @retval EFI_LOAD_ERROR Failure to calculate DDR memory size
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetChannelCountWithUsableDDRCache(
  IN     UINT16 SocketId,
  OUT UINT32 *pChannelCount
  )
{
  EFI_STATUS ReturnCode = EFI_LOAD_ERROR;
  DIMM *pDimm = NULL;
  UINT32 ChannelCount = 0;
  UINT32 Socket = MAX_UINT32_VALUE;
  UINT32 Die = MAX_UINT32_VALUE;
  UINT32 MemController = MAX_UINT32_VALUE;
  UINT32 Channel = MAX_UINT32_VALUE;
  UINT32 Index1 = 0, Index2 = 0;
  BOOLEAN CrossTileCachingSupported = FALSE;
  ParsedPmttHeader *pPmttHead = NULL;

  NVDIMM_ENTRY();

  if (NULL == pChannelCount) {
    NVDIMM_DBG("Null pointer passed.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = CheckIsCrossTileCachingSupported(&CrossTileCachingSupported);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to determine if cross-tile caching supported.");
    goto Finish;
  }

  pPmttHead = gNvmDimmData->PMEMDev.pPmttHead; // Only 0.2 is parsed and placed here
  if (NULL == pPmttHead) {
    NVDIMM_DBG("Pmtt head not found.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  // Enlist all channels having at least one DDR paired with a DCPMM on an iMC
  for (Index1 = 0; Index1 < pPmttHead->DDRModulesNum; Index1++) {
    if (SocketId != SOCKET_ID_ALL && pPmttHead->ppDDRModules[Index1]->SocketId != SocketId) {
      continue;
    }

    // Check for only 1 DDR per Channel
    if ((Socket == pPmttHead->ppDDRModules[Index1]->SocketId) &&
      (Die == pPmttHead->ppDDRModules[Index1]->DieId) &&
      (MemController == pPmttHead->ppDDRModules[Index1]->MemControllerId) &&
      (Channel == pPmttHead->ppDDRModules[Index1]->ChannelId)) {
      continue;
    }

    // Taking advantage of the fact that DDRs are stored in the array in the consecutive order of socket, die, iMc, channel & slot
    Channel = pPmttHead->ppDDRModules[Index1]->ChannelId;
    MemController = pPmttHead->ppDDRModules[Index1]->MemControllerId;
    Die = pPmttHead->ppDDRModules[Index1]->DieId;
    Socket = pPmttHead->ppDDRModules[Index1]->SocketId;

    if (CrossTileCachingSupported) {
      ChannelCount++;
    }
    else {
      // Check to see if this DDR is paired with a DCPMM on the iMc
      for (Index2 = 0; Index2 < pPmttHead->DCPMModulesNum; Index2++) {
        pDimm = GetDimmByPid(pPmttHead->ppDCPMModules[Index2]->SmbiosHandle, &gNvmDimmData->PMEMDev.Dimms);
        if (pDimm == NULL) {
          NVDIMM_DBG("Failed to retrieve the DCPMM pid %x", pPmttHead->ppDCPMModules[Index2]->SmbiosHandle);
          goto Finish;
        }

        // Unmanageable, non-functional and population violation DCPMMs excluded for Memory Mode
        if (!IsDimmManageable(pDimm) || pDimm->NonFunctional || IsDimmInPopulationViolation(pDimm)) {
          continue;
        }

        if ((Socket == pPmttHead->ppDCPMModules[Index2]->SocketId) &&
          (Die == pPmttHead->ppDCPMModules[Index2]->DieId) &&
          (MemController == pPmttHead->ppDCPMModules[Index2]->MemControllerId)) {
          ChannelCount++;
          break;
        }
      }
    }
  }

  *pChannelCount = ChannelCount;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate the total unusable DDR Cache Size in bytes for 2LM

  @param[in]  SocketId Socket Id, 0xFFFF indicates all sockets
  @param[out] pTotalUnusableDDRCacheSize Pointer to total unusable DDR cache size for 2LM

  @retval EFI_INVALID_PARAMETER Passed NULL argument
  @retval EFI_LOAD_ERROR Failure to calculate total unusable DDR Cache Size
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetUnusableDDRCacheSizeFor2LM(
  IN     UINT16 SocketId,
  OUT UINT64 *pTotalUnusableDDRCacheSize
  )
{
  EFI_STATUS ReturnCode = EFI_LOAD_ERROR;
  DIMM *pDimm = NULL;
  DIMM_INFO *pDimmInfo = NULL;
  UINT32 Index1 = 0, Index2 = 0;
  BOOLEAN IsDdrPairedWithDcpmm = FALSE;
  BOOLEAN CrossTileCachingSupported = FALSE;
  BOOLEAN NonPorCrossTileSupportedConfig = FALSE;
  UINT32 NumOfDdrPairedWithDcpmm = 0;
  UINT64 TotalUnusableDDRCacheSize = 0;
  UINT64 TotalDDRCapacity = 0;

  NVDIMM_ENTRY();

  if (NULL == pTotalUnusableDDRCacheSize) {
    NVDIMM_DBG("Null pointer passed.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pTotalUnusableDDRCacheSize = 0;

  pDimmInfo = (DIMM_INFO *)AllocateZeroPool(sizeof(*pDimmInfo));
  if (pDimmInfo == NULL) {
    NVDIMM_WARN("Memory allocation error");
    goto Finish;
  }

  ReturnCode = CheckIsCrossTileCachingSupported(&CrossTileCachingSupported);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to determine if cross-tile caching supported.");
    goto Finish;
  }

  ReturnCode = CheckIsNonPorCrossTileSupportedConfig(&NonPorCrossTileSupportedConfig);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CheckIsNonPorCrossTileSupportedConfig failed.");
    goto Finish;
  }

  ParsedPmttHeader *pPmttHead = NULL;

  pPmttHead = gNvmDimmData->PMEMDev.pPmttHead;
  if (NULL == pPmttHead) {
    NVDIMM_DBG("Pmtt head not found.");
    // Assuming all DDRs can be used as cache on Purley platforms (PMTT Rev: 0x1)
    // pPmttHead is NULL for older PMTT revisions
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  for (Index1 = 0; Index1 < pPmttHead->DDRModulesNum; Index1++) {
    if (SocketId != SOCKET_ID_ALL && pPmttHead->ppDDRModules[Index1]->SocketId != SocketId) {
      continue;
    }

    pDimmInfo->DimmID = pPmttHead->ppDDRModules[Index1]->SmbiosHandle;
    ReturnCode = FillSmbiosInfo(pDimmInfo);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Error in retrieving Information from SMBIOS tables");
      goto Finish;
    }
    TotalDDRCapacity += pDimmInfo->CapacityFromSmbios;

    // Check to see if this DDR is paired with a PMem module on the iMc
    for (Index2 = 0; Index2 < pPmttHead->DCPMModulesNum; Index2++) {
      pDimm = GetDimmByPid(pPmttHead->ppDCPMModules[Index2]->SmbiosHandle, &gNvmDimmData->PMEMDev.Dimms);
      if (pDimm == NULL) {
        NVDIMM_DBG("Failed to retrieve the DCPMM pid %x", pPmttHead->ppDCPMModules[Index2]->SmbiosHandle);
        goto Finish;
      }

      // Unmanageable, non-functional and population violation PMem modules excluded for Memory Mode
      if (!IsDimmManageable(pDimm) || pDimm->NonFunctional || IsDimmInPopulationViolation(pDimm)) {
        continue;
      }

      if ((pPmttHead->ppDDRModules[Index1]->SocketId == pPmttHead->ppDCPMModules[Index2]->SocketId) &&
        (pPmttHead->ppDDRModules[Index1]->DieId == pPmttHead->ppDCPMModules[Index2]->DieId) &&
        (pPmttHead->ppDDRModules[Index1]->MemControllerId == pPmttHead->ppDCPMModules[Index2]->MemControllerId)) {
        IsDdrPairedWithDcpmm = TRUE;
        NumOfDdrPairedWithDcpmm++;
        break;
      }
    }

    if (!IsDdrPairedWithDcpmm && !CrossTileCachingSupported) {
      TotalUnusableDDRCacheSize += pDimmInfo->CapacityFromSmbios;
    }

    IsDdrPairedWithDcpmm = FALSE;
  }

  /**
    Except in the case of a non-por cross-tile supported configuration,
    there should be at least one DDR paired with a PMem module on any iMC
    for cross-tile caching.
  **/
  if (NumOfDdrPairedWithDcpmm == 0 && !NonPorCrossTileSupportedConfig) {
    *pTotalUnusableDDRCacheSize = TotalDDRCapacity;
  }
  else {
    *pTotalUnusableDDRCacheSize = TotalUnusableDDRCacheSize;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimmInfo);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate the total DDR Cache Size

  @param[in]  SocketId Socket Id
  @param[in]  VolatileMode BIOS Volatile Mode
  @param[out] pTotalDDRCacheSize Pointer to total DDR Cache Size

  @retval EFI_INVALID_PARAMETER Passed NULL argument
  @retval EFI_NOT_FOUND Failure Unsupported VolatileMode
  @retval EFI_UNSUPPORTED Failure to calculate total DDR Cache Size
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetTotalUsableDDRCacheSize(
  IN     UINT16 SocketId,
  IN     MEMORY_MODE VolatileMode,
  OUT UINT64 *pTotalDDRCacheSize
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  UINT64 DDRCacheSizePerChannel = 0;
  UINT32 ChannelCount = 0;
  UINT64 DDRPhysicalSize = 0;
  UINT64 TotalDDRCacheSize = 0;
  UINT64 UnusableDDRCacheSize = 0;

  NVDIMM_ENTRY();

  if (pTotalDDRCacheSize == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (MEMORY_MODE_2LM == VolatileMode) {
    ReturnCode = GetDDRPhysicalSize(SocketId, &DDRPhysicalSize);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not retrieve memory capacity.");
      goto Finish;
    }

    ReturnCode = GetUnusableDDRCacheSizeFor2LM(SocketId, &UnusableDDRCacheSize);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not retrieve memory capacity.");
      goto Finish;
    }

    TotalDDRCacheSize = DDRPhysicalSize - UnusableDDRCacheSize;
  }
  else if (MEMORY_MODE_1LM_PLUS_2LM == VolatileMode) {
    ReturnCode = RetrievePcatDDRCacheSize(&DDRCacheSizePerChannel);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not retrieve DDR cache size.");
      goto Finish;
    }

    ReturnCode = GetChannelCountWithUsableDDRCache(SocketId, &ChannelCount);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("GetChannelCountWithUsableDDRCache failed.");
      goto Finish;
    }

    TotalDDRCacheSize = DDRCacheSizePerChannel * ChannelCount;
  }
  else if (MEMORY_MODE_1LM == VolatileMode) {
    TotalDDRCacheSize = 0;
  }
  else {
    ReturnCode = EFI_UNSUPPORTED;
    NVDIMM_DBG("Unsupported mode discovered.");
    goto Finish;
  }

  *pTotalDDRCacheSize = TotalDDRCacheSize;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve and calculate DDR cache and memory capacity to return.

  @param[in]  SocketId Socket Id, value 0xFFFF indicates include all socket values
  @param[out] pDDRRawCapacity Pointer to value of the total cache capacity
  @param[out] pDDRCacheCapacity Pointer to value of the DDR cache capacity
  @param[out] pDDRVolatileCapacity Pointer to value of the DDR memory capacity
  @param[out] pDDRInaccessibleCapacity Pointer to value of the DDR inaccessible capacity

  @retval EFI_INVALID_PARAMETER passed NULL argument
  @retval EFI_DEVICE_ERROR Total DCPMM Persistent & Volatile capacity is larger than total mapped memory
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
EFIAPI
GetDDRCapacities(
  IN     UINT16 SocketId,
     OUT UINT64 *pDDRRawCapacity,
     OUT UINT64 *pDDRCacheCapacity OPTIONAL,
     OUT UINT64 *pDDRVolatileCapacity OPTIONAL,
     OUT UINT64 *pDDRInaccessibleCapacity OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS PreservedReturnCode = EFI_SUCCESS;
  UINT64 SocketSkuTotalMappedMemory = 0;
  UINT64 DcpmmRawCapacity = 0;
  UINT64 DcpmmVolatileCapacity = 0;
  UINT64 DcpmmAppDirectCapacity = 0;
  UINT64 DcpmmUnconfiguredCapacity = 0;
  UINT64 DcpmmReservedCapacity = 0;
  UINT64 DcpmmInaccessibleCapacity = 0;
  MEMORY_MODE CurrentMode = MEMORY_MODE_1LM;

  NVDIMM_ENTRY();

  if (pDDRRawCapacity == NULL ||
    (pDDRInaccessibleCapacity != NULL && (pDDRCacheCapacity == NULL || pDDRVolatileCapacity == NULL))) {
    NVDIMM_DBG("Invalid parameter");
    goto Finish;
  }

  ReturnCode = CurrentMemoryMode(&CurrentMode);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to determine current memory mode");
    goto Finish;
  }

  // Get total physical size of all non-DCPMM DIMMs through PMTT
  // Semi-ignoring error since we expect the PMTT table to be missing
  // in Purley
  // Initialize to unknown
  *pDDRRawCapacity = ACPI_TABLE_VALUE_UNKNOWN;
  CHECK_RESULT_CONTINUE_PRESERVE_ERROR(GetDDRPhysicalSize(SocketId, pDDRRawCapacity));

  // Get total DDR cache size
  if (pDDRCacheCapacity != NULL) {
    // Initialize to unknown
    *pDDRCacheCapacity = ACPI_TABLE_VALUE_UNKNOWN;
    CHECK_RESULT(RetrievePcatSocketSkuCachedMemory(SOCKET_ID_ALL, pDDRCacheCapacity), Finish);
  }

  // Get DDR volatile capacity: Subtract mapped DCPMM Persistent & Volatile capacity from total mapped memory
  if (pDDRVolatileCapacity != NULL) {
    // Initialize to unknown
    *pDDRVolatileCapacity = ACPI_TABLE_VALUE_UNKNOWN;
    CHECK_RESULT(RetrievePcatSocketSkuTotalMappedMemory(SOCKET_ID_ALL, &SocketSkuTotalMappedMemory), Finish);

    CHECK_RESULT(GetTotalDcpmmCapacities(&gNvmDimmData->PMEMDev.Dimms, &DcpmmRawCapacity, &DcpmmVolatileCapacity,
      &DcpmmAppDirectCapacity, &DcpmmUnconfiguredCapacity, &DcpmmReservedCapacity, &DcpmmInaccessibleCapacity), Finish);


    if ((DcpmmVolatileCapacity + DcpmmAppDirectCapacity) <= SocketSkuTotalMappedMemory) {
      *pDDRVolatileCapacity = SocketSkuTotalMappedMemory - DcpmmVolatileCapacity - DcpmmAppDirectCapacity;
    }
    else {
      NVDIMM_DBG("Total mapped DCPMM Persistent & Volatile capacity cannot be larger than total mapped memory.");
      if (CurrentMode != MEMORY_MODE_2LM) {
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }
      NVDIMM_DBG("But, in Memory Mode DDR volatile capacity is always 0 (it is used as cache)");
      // Added to make ipmctl 3.x backwards compatible with Purley BIOS
      *pDDRVolatileCapacity = 0;
    }
  }

  // Get DDR inaccessible capacity
  if (pDDRInaccessibleCapacity != NULL) {
    // Allow for the PMTT table to be missing. Just set the derived value to unknown
    if (*pDDRRawCapacity == ACPI_TABLE_VALUE_UNKNOWN) {
      *pDDRInaccessibleCapacity = ACPI_TABLE_VALUE_UNKNOWN;
    } else {
      *pDDRInaccessibleCapacity = *pDDRRawCapacity - *pDDRVolatileCapacity - *pDDRCacheCapacity;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  if (EFI_ERROR(PreservedReturnCode) && !EFI_ERROR(ReturnCode)) {
    ReturnCode = PreservedReturnCode;
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate the total size of available memory in the PMem modules
  according to the smbios and return the result

  @param[in]  SocketId Socket Id, value 0xFFFF indicates include all socket values
  @param[out] pDDRPhysicalSize Pointer to total memory size

  @retval EFI_INVALID_PARAMETER Passed NULL argument
  @retval EFI_LOAD_ERROR Failure to calculate DDR memory size
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
GetDDRPhysicalSize(
  IN     UINT16 SocketId,
     OUT UINT64 *pDDRPhysicalSize
)
{
  EFI_STATUS ReturnCode = EFI_LOAD_ERROR;
  TABLE_HEADER *pPmttRaw = NULL;
  DIMM_INFO *pDimmInfo = NULL;
  UINT64 TotalDDRMemorySize = 0;
  UINT16 Index = 0;

  NVDIMM_ENTRY();

  if (NULL == pDDRPhysicalSize) {
    NVDIMM_DBG("Null pointer passed.");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // If PMTT table is NULL here, that's ok, let the caller handle whether to
  // ignore this error or not
  CHECK_RESULT(GetAcpiPMTT(NULL, (VOID *)&pPmttRaw), Finish);

  // Get and parse raw PMTT table for 0.1 table
  if (IS_ACPI_REV_MAJ_0_MIN_1(pPmttRaw->Revision)) {
    PMTT_TABLE *pPMTT = (PMTT_TABLE *)pPmttRaw;
    PMTT_COMMON_HEADER *pCommonHeader = NULL;
    PMTT_MODULE *pModule = NULL;
    UINT64 Offset = 0;

    Offset = sizeof(pPMTT->Header) + sizeof(pPMTT->Reserved);
    //Iterate through the table and look for DDR Modules
    while (Offset < pPMTT->Header.Length) {
      pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
      if (pCommonHeader->Type == PMTT_TYPE_SOCKET) {
        Offset += sizeof(PMTT_SOCKET) + PMTT_COMMON_HDR_LEN;
      }
      else if (pCommonHeader->Type == PMTT_TYPE_iMC) {
        Offset += sizeof(PMTT_iMC) + PMTT_COMMON_HDR_LEN;
      }
      else if (pCommonHeader->Type == PMTT_TYPE_MODULE) {
        pModule = (PMTT_MODULE *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
        if (pModule->SmbiosHandle != PMTT_INVALID_SMBIOS_HANDLE) {
          if (!(pCommonHeader->Flags & PMTT_DDR_DCPM_FLAG) && (pModule->SizeOfDimm > 0)) {
            TotalDDRMemorySize += MIB_TO_BYTES(pModule->SizeOfDimm);
          }
        }
        Offset += sizeof(PMTT_MODULE) + PMTT_COMMON_HDR_LEN;
      }
    }
  }
  else if (IS_ACPI_REV_MAJ_0_MIN_2(pPmttRaw->Revision)) {
    // Use our pre-parsed global struct derived from the raw PMTT table
    ParsedPmttHeader *pPmttHead = NULL;
    UINT32 DDRModulesNum;
    PMTT_MODULE_INFO **ppDDRModules = NULL;

    pPmttHead = gNvmDimmData->PMEMDev.pPmttHead;
    if (NULL == pPmttHead) {
      NVDIMM_DBG("Pmtt head not found.");
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }

    // Get list of DIMM modules from PMTT
    DDRModulesNum = pPmttHead->DDRModulesNum;
    ppDDRModules = pPmttHead->ppDDRModules;

    pDimmInfo = (DIMM_INFO *)AllocateZeroPool(sizeof(*pDimmInfo));
    if (pDimmInfo == NULL) {
      NVDIMM_WARN("Memory allocation error");
      goto Finish;
    }
    SetMem(pDimmInfo, sizeof(*pDimmInfo), 0);

    /* For every module, get its total capacity and add it to the system total capacity */
    for (Index = 0; Index < DDRModulesNum; Index++) {
    if (SocketId != SOCKET_ID_ALL && ppDDRModules[Index]->SocketId != SocketId) {
      continue;
    }
      // Get dimm information from smbios handle
      pDimmInfo->DimmID = ppDDRModules[Index]->SmbiosHandle;
      ReturnCode = FillSmbiosInfo(pDimmInfo);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Smbios information could not be retrieved.");
        goto Finish;
      }
      // Get dimm capacity and add it to the total
      TotalDDRMemorySize += pDimmInfo->CapacityFromSmbios;
    }
  }

  // Set total to the result output
  *pDDRPhysicalSize = TotalDDRMemorySize;

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimmInfo);
  FREE_POOL_SAFE(pPmttRaw);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve Smbios tables dynamically, and populate Smbios table structures
  of type 17/20 for the specified Dimm Pid

  @param[in]  DimmPid The ID of the DIMM
  @param[out] pDmiPhysicalDev Pointer to smbios table structure of type 17
  @param[out] pDmiDeviceMappedAddr Pointer to smbios table structure of type 20

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
  )
{
  EFI_STATUS ReturnCode = EFI_DEVICE_ERROR;

  SMBIOS_STRUCTURE_POINTER SmBiosStruct;
  SMBIOS_STRUCTURE_POINTER BoundSmBiosStruct;

  NVDIMM_ENTRY();

  ZeroMem(&SmBiosStruct, sizeof(SmBiosStruct));
  ZeroMem(&BoundSmBiosStruct, sizeof(BoundSmBiosStruct));

  if (pDmiPhysicalDev == NULL || pDmiDeviceMappedAddr == NULL || pSmbiosVersion == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  GetFirstAndBoundSmBiosStructPointer(&SmBiosStruct, &BoundSmBiosStruct, pSmbiosVersion);
  if (SmBiosStruct.Raw == NULL || BoundSmBiosStruct.Raw == NULL) {
    goto Finish;
  }
  while (SmBiosStruct.Raw < BoundSmBiosStruct.Raw) {
    if (SmBiosStruct.Hdr != NULL) {
      if (SmBiosStruct.Hdr->Type == SMBIOS_TYPE_MEM_DEV && SmBiosStruct.Hdr->Handle == DimmPid) {
        pDmiPhysicalDev->Raw = SmBiosStruct.Raw;
      } else if (SmBiosStruct.Hdr->Type == SMBIOS_TYPE_MEM_DEV_MAPPED_ADDR &&
                 SmBiosStruct.Type20->MemoryDeviceHandle == DimmPid) {
        pDmiDeviceMappedAddr->Raw = SmBiosStruct.Raw;
      }
    } else {
      NVDIMM_ERR("SmBios entry has invalid pointers set");
    }

    ReturnCode = GetNextSmbiosStruct(&SmBiosStruct);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}


/**
  Free resources of PMTT_INFO list items

  @param[in] pPmttInfo - PMTT_INFO list that items will be freed for
**/
STATIC VOID
FreePmttItems(
  IN OUT LIST_ENTRY *pPmttInfo
  )
{
  PMTT_INFO *pPmttItem = NULL;
  LIST_ENTRY *pNode = NULL;
  LIST_ENTRY *pNext = NULL;

  NVDIMM_ENTRY();

  if ((NULL == pPmttInfo) || (NULL == pPmttInfo->ForwardLink)){
     goto Finish;
  }

  LIST_FOR_EACH_SAFE(pNode, pNext, pPmttInfo) {
    pPmttItem = PMTT_INFO_FROM_NODE(pNode);
    RemoveEntryList(pNode);
    FREE_POOL_SAFE(pPmttItem);
  }

Finish:
  NVDIMM_EXIT();
}

/**
  Step through PMTT and generate linked list of DIMM ID/Socket ID pairs
  If there is no PMTT, list will be empty

  @param[in] pPmttInfo - pointer to generic list head
**/
STATIC VOID
MapSockets(
   LIST_ENTRY * pPmttInfo
)
{
  EFI_STATUS ReturnCode;
  TABLE_HEADER *pPmttRaw = NULL;
  PMTT_TABLE *pPMTT = NULL;
  PMTT_INFO  *DdrEntry = NULL;
  PMTT_COMMON_HEADER *pCommonHeader = NULL;
  PMTT_SOCKET *pSocket = NULL;
  PMTT_MODULE *pModule = NULL;
  ParsedPmttHeader *pPmttHead = NULL;
  PMTT_MODULE_INFO **ppAllMemModules[2];
  UINT32 NumOfMemModules[2];
  UINT64 PmttLen = 0;
  UINT64 Offset = 0;
  UINT32 Index1 = 0, Index2 = 0;
#ifndef MDEPKG_NDEBUG
  LIST_ENTRY *pNode = NULL;
#endif

  if (NULL == pPmttInfo) {
    NVDIMM_ERR("Invalid Parameter");
    return;
  }

  InitializeListHead(pPmttInfo);

  ReturnCode = GetAcpiPMTT(NULL, (VOID *)&pPmttRaw);

  if (EFI_ERROR(ReturnCode)) {
    //error getting PMTT. Nothing to map
    //to see ddr4 topology in this case, do not use -socket
    return;
  }

  if (IS_ACPI_REV_MAJ_0_MIN_1(pPmttRaw->Revision)) {
    pPMTT = (PMTT_TABLE *)pPmttRaw;
    PmttLen = pPMTT->Header.Length;
    Offset = sizeof(pPMTT->Header) + sizeof(pPMTT->Reserved);

    //step through table and create socket list
    while (Offset < PmttLen) {
      pCommonHeader = (PMTT_COMMON_HEADER *)(((UINT8 *)pPMTT) + Offset);
      if (pCommonHeader->Type == PMTT_TYPE_SOCKET) {
        pSocket = (PMTT_SOCKET *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
        Offset += sizeof(PMTT_SOCKET) + PMTT_COMMON_HDR_LEN;
      }
      else if (pCommonHeader->Type == PMTT_TYPE_iMC) {
        Offset += sizeof(PMTT_iMC) + PMTT_COMMON_HDR_LEN;
      }
      else if (pCommonHeader->Type == PMTT_TYPE_MODULE) {
        if (NULL != pSocket) {
          pModule = (PMTT_MODULE *)(((UINT8 *)pPMTT) + Offset + PMTT_COMMON_HDR_LEN);
          if (pModule->SmbiosHandle != PMTT_INVALID_SMBIOS_HANDLE) {
            DdrEntry = AllocateZeroPool(sizeof(*DdrEntry));
            if (DdrEntry == NULL) {
              NVDIMM_ERR("Out of memory");
              goto Finish;
            }
            DdrEntry->PmttVersion.Revision.AsUint8 = PMTT_HEADER_REVISION_1;
            DdrEntry->DimmID = pModule->SmbiosHandle & SMBIOS_HANDLE_MASK;
            DdrEntry->SocketID = pSocket->SocketId;
            DdrEntry->Signature = PMTT_INFO_SIGNATURE;
            InsertTailList(pPmttInfo, &DdrEntry->PmttNode);
          }
        }
        Offset += sizeof(PMTT_MODULE) + PMTT_COMMON_HDR_LEN;
      }
    }
  } else if (IS_ACPI_REV_MAJ_0_MIN_2(pPmttRaw->Revision)) {
    // Use our pre-parsed 0.2-only global struct derived from the raw PMTT table
    pPmttHead = gNvmDimmData->PMEMDev.pPmttHead;
    if (pPmttHead == NULL) {
      ReturnCode = EFI_NOT_FOUND;
      NVDIMM_ERR("Can't find global pPmtt struct. Exiting");
      goto Finish;
    }
    ppAllMemModules[0] = pPmttHead->ppDDRModules;
    ppAllMemModules[1] = pPmttHead->ppDCPMModules;
    NumOfMemModules[0] = pPmttHead->DDRModulesNum;
    NumOfMemModules[1] = pPmttHead->DCPMModulesNum;
    for (Index1 = 0; Index1 < 2; Index1++) {
      PMTT_MODULE_INFO **pMemModules = ppAllMemModules[Index1];
      for (Index2 = 0; Index2 < NumOfMemModules[Index1]; Index2++) {
        DdrEntry = AllocateZeroPool(sizeof(*DdrEntry));
        if (DdrEntry == NULL) {
          NVDIMM_ERR("Out of memory");
          goto Finish;
        }

        DdrEntry->PmttVersion.Revision.AsUint8 = PMTT_HEADER_REVISION_2;
        DdrEntry->DimmID = pMemModules[Index2]->SmbiosHandle;
        DdrEntry->NodeControllerID = (UINT16)SOCKET_INDEX_TO_NFIT_NODE_ID(pMemModules[Index2]->SocketId);
        DdrEntry->SocketID = pMemModules[Index2]->SocketId;
        DdrEntry->PmttVersion.VendorData.DieID = pMemModules[Index2]->DieId;
        DdrEntry->MemControllerID = pMemModules[Index2]->MemControllerId;
        DdrEntry->PmttVersion.VendorData.ChannelID = pMemModules[Index2]->ChannelId;
        DdrEntry->PmttVersion.VendorData.SlotID = pMemModules[Index2]->SlotId;
        DdrEntry->Signature = PMTT_INFO_SIGNATURE;

        InsertTailList(pPmttInfo, &DdrEntry->PmttNode);
      }
    }
  }

#ifndef MDEPKG_NDEBUG
  LIST_FOR_EACH(pNode, pPmttInfo) {
     DdrEntry = PMTT_INFO_FROM_NODE(pNode);
     NVDIMM_DBG("Dimm: 0x%X  Socket: %d\n", DdrEntry->DimmID, DdrEntry->SocketID);
  }
#endif

Finish:
  FREE_POOL_SAFE(pPmttRaw);
  return;
}

/**
  Sorts the Dimm topology list by Memory Type

  @param[in out] pMemType1 A pointer to the pDimmId list.
  @param[in out] pMemType2 A pointer to the copy of pDimmId list.

  @retval int returns 0,-1, 0
**/
INT32 SortDimmTopologyByMemType(VOID *ppTopologyDimm1, VOID *ppTopologyDimm2)
{
  TOPOLOGY_DIMM_INFO *ppTopologyDimma = (TOPOLOGY_DIMM_INFO *)ppTopologyDimm1;
  TOPOLOGY_DIMM_INFO *ppTopologyDimmb = (TOPOLOGY_DIMM_INFO *)ppTopologyDimm2;
  if (ppTopologyDimma->MemoryType == ppTopologyDimmb->MemoryType) {
    return 0;
  }
  else if (ppTopologyDimma->MemoryType < ppTopologyDimmb->MemoryType) {
    return 1;
  }
  else {
    return -1;
  }
}

/**
  Get system topology from SMBIOS table

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.

  @param[out] ppTopologyDimm Structure containing information about DDR entries from SMBIOS.
  @param[out] pTopologyDimmsNumber Number of DDR entries found in SMBIOS.

  @retval EFI_SUCCESS All ok.
  @retval EFI_DEVICE_ERROR Unable to find SMBIOS table in system configuration tables.
  @retval EFI_OUT_OF_RESOURCES Problem with allocating memory
**/
EFI_STATUS
EFIAPI
GetSystemTopology(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT TOPOLOGY_DIMM_INFO **ppTopologyDimm,
     OUT UINT16 *pTopologyDimmsNumber
  )
{
  EFI_STATUS ReturnCode = EFI_DEVICE_ERROR;

  UINT16 Index = 0;
  LIST_ENTRY PmttInfo;
  LIST_ENTRY *pNode = NULL;
  PMTT_INFO  *DdrEntry = NULL;
  DIMM_INFO *pDimmInfo = NULL;
  DIMM *pDimm = NULL;
  ACPI_REVISION Revision;

  NVDIMM_ENTRY();

  ZeroMem(&PmttInfo, sizeof(PmttInfo));
  if (pThis == NULL || ppTopologyDimm == NULL || pTopologyDimmsNumber == NULL) {
    goto Finish;
  }

  *ppTopologyDimm = AllocateZeroPool(sizeof(**ppTopologyDimm) * MAX_DIMMS);
  if (*ppTopologyDimm == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  MapSockets(&PmttInfo);

  if (NULL == (&PmttInfo)->ForwardLink) {
    NVDIMM_WARN("Error in getting PMTT Info for topology information");
    goto Finish;
  }

  pDimmInfo = (DIMM_INFO *)AllocateZeroPool(sizeof(*pDimmInfo));
  if (pDimmInfo == NULL) {
    NVDIMM_WARN("Memory allocation error");
    goto Finish;
  }
  SetMem(pDimmInfo, sizeof(*pDimmInfo), 0);

  LIST_FOR_EACH(pNode, &PmttInfo) {
    DdrEntry = PMTT_INFO_FROM_NODE(pNode);
    pDimmInfo->DimmID = DdrEntry->DimmID;
    ReturnCode = FillSmbiosInfo(pDimmInfo);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Error in retrieving Information from SMBIOS tables");
      goto Finish;
    }

    (*ppTopologyDimm)[Index].DimmHandle = 0;
    // Retrieve DIMM Device Handle for Memory Type: DCPM
    if (pDimmInfo->MemoryType == MEMORYTYPE_DCPM) {
      pDimm = GetDimmByPid(DdrEntry->DimmID, &gNvmDimmData->PMEMDev.Dimms);
      if (pDimm == NULL) {
        NVDIMM_WARN("Dimm ID: 0x%04x not found!", DdrEntry->DimmID);
        goto Finish;
      }
      (*ppTopologyDimm)[Index].DimmHandle = pDimm->DeviceHandle.AsUint32;
      (*ppTopologyDimm)[Index].ChannelID = pDimm->ChannelId;
      (*ppTopologyDimm)[Index].SlotID = pDimm->ChannelPos;
      (*ppTopologyDimm)[Index].MemControllerID = pDimm->ImcId;
    }

    (*ppTopologyDimm)[Index].VolatileCapacity = pDimmInfo->CapacityFromSmbios;
    StrnCpyS((*ppTopologyDimm)[Index].DeviceLocator, DEVICE_LOCATOR_LEN, pDimmInfo->DeviceLocator, DEVICE_LOCATOR_LEN - 1);
    StrnCpyS((*ppTopologyDimm)[Index].BankLabel, BANKLABEL_LEN, pDimmInfo->BankLabel, BANKLABEL_LEN - 1);
    (*ppTopologyDimm)[Index].MemoryType = pDimmInfo->MemoryType;
    (*ppTopologyDimm)[Index].DimmID = DdrEntry->DimmID;
    (*ppTopologyDimm)[Index].NodeControllerID = SOCKET_INDEX_TO_NFIT_NODE_ID(DdrEntry->SocketID);
    (*ppTopologyDimm)[Index].SocketID = DdrEntry->SocketID;
    (*ppTopologyDimm)[Index].PmttVersion = DdrEntry->PmttVersion.Revision.AsUint8;

    CopyMem(&Revision, &DdrEntry->PmttVersion.Revision, sizeof(Revision));
    if (IS_ACPI_REV_MAJ_0_MIN_2(Revision)) {
      (*ppTopologyDimm)[Index].DieID = DdrEntry->PmttVersion.VendorData.DieID;

      if (pDimmInfo->MemoryType == MEMORYTYPE_DDR4 || pDimmInfo->MemoryType == MEMORYTYPE_DDR5) {
        (*ppTopologyDimm)[Index].ChannelID = DdrEntry->PmttVersion.VendorData.ChannelID;
        (*ppTopologyDimm)[Index].SlotID = DdrEntry->PmttVersion.VendorData.SlotID;
        (*ppTopologyDimm)[Index].MemControllerID = DdrEntry->MemControllerID;
      }
    }

    Index++;
    (*pTopologyDimmsNumber) = Index;
  }

  ReturnCode = BubbleSort(*ppTopologyDimm, *pTopologyDimmsNumber, sizeof(**ppTopologyDimm), SortDimmTopologyByMemType);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Error in sorting the DIMM topology list");
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FreePmttItems(&PmttInfo);
  FREE_POOL_SAFE(pDimmInfo);
  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

/**
  Get the system-wide ARS status for the persistent memory capacity of the system.
  In this function, the system-wide ARS status is determined based on the ARS status
  values for the individual DIMMs.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.

  @param[out] pARSStatus pointer to the current system ARS status.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetARSStatus(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT8 *pARSStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT8 DimmARSStatus = LONG_OP_STATUS_IDLE;
  UINT8 ARSStatusBitmask = 0;

  NVDIMM_ENTRY();
  if (pThis == NULL || pARSStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pARSStatus = LONG_OP_STATUS_IDLE;

  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    if (pDimm->PmCapacity > 0) {
      ReturnCode = FwCmdGetARS(pDimm, &DimmARSStatus);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("FwCmdGetARS failed with error " FORMAT_EFI_STATUS " for DIMM 0x%x", ReturnCode, pDimm->DeviceHandle.AsUint32);
      }

      switch(DimmARSStatus) {
        case LONG_OP_STATUS_IN_PROGRESS:
          *pARSStatus = LONG_OP_STATUS_IN_PROGRESS;
          goto Finish;
          break;
        case LONG_OP_STATUS_UNKNOWN:
          ARSStatusBitmask |= ARS_STATUS_MASK_UNKNOWN;
          break;
        case LONG_OP_STATUS_COMPLETED:
          ARSStatusBitmask |= ARS_STATUS_MASK_COMPLETED;
          break;
        case LONG_OP_STATUS_IDLE:
          ARSStatusBitmask |= ARS_STATUS_MASK_IDLE;
          break;
        case LONG_OP_STATUS_ABORTED:
          ARSStatusBitmask |= ARS_STATUS_MASK_ABORTED;
          break;
        case LONG_OP_STATUS_ERROR:
        default:
          ARSStatusBitmask |= ARS_STATUS_MASK_ERROR;
          break;
      }
    }
  }

  if (ARSStatusBitmask & ARS_STATUS_MASK_UNKNOWN) {
    *pARSStatus = LONG_OP_STATUS_UNKNOWN;
  } else if (ARSStatusBitmask & ARS_STATUS_MASK_ERROR) {
    *pARSStatus = LONG_OP_STATUS_ERROR;
  } else if (ARSStatusBitmask & ARS_STATUS_MASK_ABORTED) {
    *pARSStatus = LONG_OP_STATUS_ABORTED;
  } else if (ARSStatusBitmask & ARS_STATUS_MASK_COMPLETED) {
    *pARSStatus = LONG_OP_STATUS_COMPLETED;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[out] pDriverPreferences pointer to the current driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
GetDriverPreferences(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDriverPreferences == NULL || pCommandStatus == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  ReturnCode = ReadRunTimeDriverPreferences(pDriverPreferences);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve DriverPreferences");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Set the User Driver Preferences.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] pDriverPreferences pointer to the desired driver preferences.
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
SetDriverPreferences(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN VariableSize = 0;
  BOOLEAN Conflict = FALSE;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDriverPreferences == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  ReturnCode = AppDirectSettingsValidation(pDriverPreferences);

  if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  ReturnCode = AppDirectSettingsConflict(pDriverPreferences, &Conflict, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || Conflict) {
    goto Finish;
  }

  VariableSize = sizeof(pDriverPreferences->ChannelInterleaving);
  ReturnCode = SET_VARIABLE_NV(
    CHANNEL_INTERLEAVE_SIZE_VARIABLE_NAME,
    gNvmDimmNgnvmVariableGuid,
    VariableSize,
    &pDriverPreferences->ChannelInterleaving);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to set Channel Interleave Variable");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  VariableSize = sizeof(pDriverPreferences->ImcInterleaving);
  ReturnCode = SET_VARIABLE_NV(
    IMC_INTERLEAVE_SIZE_VARIABLE_NAME,
    gNvmDimmNgnvmVariableGuid,
    VariableSize,
    &pDriverPreferences->ImcInterleaving);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to set iMC Interleave Variable");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get DDRT IO init info

  @param[in] pThis Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param[in] DimmID DimmID of device to retrieve support data from
  @param[out] pDdrtTrainingStatus pointer to the dimms DDRT training status

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
EFIAPI
GetDdrtIoInitInfo(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pDdrtTrainingStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  PT_OUTPUT_PAYLOAD_GET_DDRT_IO_INIT_INFO DdrtIoInitInfo;

  NVDIMM_ENTRY();

  ZeroMem(&DdrtIoInitInfo, sizeof(DdrtIoInitInfo));

  pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    goto Finish;
  }

  ReturnCode = FwCmdGetDdrtIoInitInfo(pDimm, &DdrtIoInitInfo);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  *pDdrtTrainingStatus = DdrtIoInitInfo.DdrtTrainingStatus;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
EFI_STATUS
EFIAPI
GetLongOpStatus(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT UINT8 *pOpcode OPTIONAL,
     OUT UINT8 *pSubOpcode OPTIONAL,
     OUT UINT16 *pPercentComplete OPTIONAL,
     OUT UINT32 *pEstimatedTimeLeft OPTIONAL,
     OUT EFI_STATUS *pLongOpEfiStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  UINT8 FwStatus = FW_SUCCESS;
  PT_OUTPUT_PAYLOAD_FW_LONG_OP_STATUS LongOpStatus;

  NVDIMM_ENTRY();

  ZeroMem(&LongOpStatus, sizeof(LongOpStatus));

  pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    goto Finish;
  }

  ReturnCode = FwCmdGetLongOperationStatus(pDimm, &FwStatus, &LongOpStatus);
  if (EFI_ERROR(ReturnCode)) {
    // If FIS <= 1.5, getting long op might not be supported
    if (((pDimm->FwVer.FwApiMajor == 1) && (pDimm->FwVer.FwApiMinor <= 5)) &&
        ((FwStatus == FW_INTERNAL_DEVICE_ERROR) ||
         (FwStatus == FW_DATA_NOT_SET) ||
         (FwStatus == FW_UNSUPPORTED_COMMAND))) {
      ReturnCode = EFI_UNSUPPORTED;
    }
    goto Finish;
  }

  *pLongOpEfiStatus = MatchFwReturnCode(LongOpStatus.Status);
  NVDIMM_DBG("Long operation status converted from FW code %d to EFI code %d", LongOpStatus.Status, *pLongOpEfiStatus);

  if (pOpcode != NULL) {
    *pOpcode = LongOpStatus.CmdOpcode;
  }

  if (pSubOpcode != NULL) {
    *pSubOpcode = LongOpStatus.CmdSubOpcode;
  }

  if (pPercentComplete != NULL) {
    *pPercentComplete = LongOpStatus.Percent;
  }

  if (pEstimatedTimeLeft != NULL) {
    *pEstimatedTimeLeft = LongOpStatus.EstimatedTimeLeft;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN GoalSuccess = FALSE;
  BOOLEAN VarsMatch = TRUE;
  BOOLEAN TopologyChanged = FALSE;

  NVDIMM_ENTRY();

  if (pIntelDIMMConfig == NULL) {
    goto FinishNoUpdate;
  }

  if (pIntelDIMMConfig->Revision != INTEL_DIMM_CONFIG_REVISION) {
    NVDIMM_DBG("Revision not supported: %d. Replacing with current supported version: %d.",
      pIntelDIMMConfig->Revision, INTEL_DIMM_CONFIG_REVISION);
    pIntelDIMMConfig->Revision = INTEL_DIMM_CONFIG_REVISION;
  }

  if (pIntelDIMMConfig->ProvisionCapacityMode != PROVISION_CAPACITY_MODE_AUTO) {
    // Sanity check, this should never happen
    pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  switch (pIntelDIMMConfig->ProvisionCapacityStatus) {
    case PROVISION_CAPACITY_STATUS_NEW_UNKNOWN:
    case PROVISION_CAPACITY_STATUS_SUCCESS:
    case PROVISION_CAPACITY_STATUS_ERROR:
    case PROVISION_CAPACITY_STATUS_PENDING_SECURITY_DISABLED:
      // Check PCD copy of variables
      ReturnCode = CheckPCDAutoConfVars(pIntelDIMMConfig, &VarsMatch);
      if (EFI_ERROR(ReturnCode)) {
        pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
        goto Finish;
      }

      // Check if the DIMM topology changed
      ReturnCode = CheckTopologyChange(&TopologyChanged);
      if (EFI_ERROR(ReturnCode)) {
        pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
        goto Finish;
      }

      if (!VarsMatch || TopologyChanged) {
        ReturnCode = AutomaticCreateGoal(pIntelDIMMConfig);
        if (ReturnCode == EFI_ACCESS_DENIED) {
          // If we already tried to signal security needs to be disabled and it was not,
          // error here so we don't boot loop.
          if (pIntelDIMMConfig->ProvisionCapacityStatus ==
            PROVISION_CAPACITY_STATUS_PENDING_SECURITY_DISABLED) {
            pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
          } else {
            pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_PENDING_SECURITY_DISABLED;
          }
        } else if (EFI_ERROR(ReturnCode)) {
          pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
        } else {
          pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_PENDING;
          // Indicate namespaces have been deleted creating the new goal
          pIntelDIMMConfig->ProvisionNamespaceStatus = PROVISION_NAMESPACE_STATUS_NEW_UNKNOWN;
        }
      } else {
        pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_SUCCESS;
      }
      break;

    case PROVISION_CAPACITY_STATUS_PENDING:
      CheckGoalStatus(&GoalSuccess);
      if (GoalSuccess) {
        pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_SUCCESS;
      } else {
        pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
        goto Finish;
      }
      break;

    default:
      NVDIMM_DBG("Invalid ProvisionCapacityStatus: %d", pIntelDIMMConfig->ProvisionCapacityStatus);
      pIntelDIMMConfig->ProvisionCapacityStatus = PROVISION_CAPACITY_STATUS_ERROR;
      break;
  }

Finish:
  // Update status
  NVDIMM_DBG("New ProvisionCapacityStatus: %d", pIntelDIMMConfig->ProvisionCapacityStatus);
  UpdateIntelDIMMConfig(pIntelDIMMConfig);

  if (pIntelDIMMConfig->ProvisionCapacityStatus == PROVISION_CAPACITY_STATUS_PENDING) {
    DEBUG((EFI_D_INFO, "Resetting for automatic provisioning.\n"));
    gRT->ResetSystem(EfiResetCold, ReturnCode, 0, NULL);
  } else if (pIntelDIMMConfig->ProvisionCapacityStatus == PROVISION_CAPACITY_STATUS_PENDING_SECURITY_DISABLED) {
    DEBUG((EFI_D_INFO, "Security enabled, resetting. Provisioning pending security disabled.\n"));
    gRT->ResetSystem(EfiResetCold, ReturnCode, 0, NULL);
  }

FinishNoUpdate:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pIntelDIMMConfig == NULL) {
    goto Finish;
  }

  // Check if namespace provisioning is needed
  if ((pIntelDIMMConfig->ProvisionNamespaceStatus == PROVISION_NAMESPACE_STATUS_NEW_UNKNOWN) &&
      (pIntelDIMMConfig->ProvisionCapacityStatus != PROVISION_CAPACITY_STATUS_ERROR)) {
    ReturnCode = AutomaticCreateNamespace(pIntelDIMMConfig);
    if (EFI_ERROR(ReturnCode)) {
      pIntelDIMMConfig->ProvisionNamespaceStatus = PROVISION_NAMESPACE_STATUS_ERROR;
    } else {
      pIntelDIMMConfig->ProvisionNamespaceStatus = PROVISION_NAMESPACE_STATUS_SUCCESS;
    }
  }

  // Update status
  NVDIMM_DBG("New ProvisionNamespaceStatus: %d", pIntelDIMMConfig->ProvisionNamespaceStatus);
  UpdateIntelDIMMConfig(pIntelDIMMConfig);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT8 PersistentMemType = 0;
  UINT16 Major = 0;
  UINT16 Minor = 0;
  LIST_ENTRY NamespaceListHead = {0};
  LIST_ENTRY *pNextNode = NULL;
  LIST_ENTRY *pNode = NULL;
  UINT32 NamespaceCount = 0;
  NAMESPACE_INFO *pCurNamespace = NULL;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  UINT32 DimmSecurityState = 0;
  UINT32 Index = 0;
  REQUIRE_DCPMMS RequireDcpmmsBitfield = REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL;

  NVDIMM_ENTRY();

  InitializeListHead(&NamespaceListHead);

  if (pIntelDIMMConfig == NULL) {
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  /** Check IntelDimmConfig variables **/
  if (pIntelDIMMConfig->MemorySize > MEMORY_SIZE_MAX_PERCENT) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pIntelDIMMConfig->PMType == PM_TYPE_APPDIRECT) {
    PersistentMemType = PM_TYPE_AD;
  } else if (pIntelDIMMConfig->PMType == PM_TYPE_APPDIRECT_NOT_INTERLEAVED) {
    PersistentMemType = PM_TYPE_AD_NI;
  } else {
    NVDIMM_DBG("Invalid PMType: %d", pIntelDIMMConfig->PMType);
    ReturnCode = EFI_INVALID_PARAMETER;
  }

  ReturnCode = GetNSLabelMajorMinorVersion(pIntelDIMMConfig->NamespaceLabelVersion, &Major, &Minor);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * MAX_DIMMS);
  if (ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  //DCPMMs in population violation are ignored from all goal requests except in the case that the goal
  //request is for ADx1 100%.  In this case DCPMMs in population violation can be used.
  if (!((PM_TYPE_AD_NI == PersistentMemType) && (0 == pIntelDIMMConfig->MemorySize))) {
    RequireDcpmmsBitfield |= REQUIRE_DCPMMS_NO_POPULATION_VIOLATION;
  }
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0, RequireDcpmmsBitfield,
      ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // Check for security
  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is in locked state.");
      goto Finish;
    }
  }

  /** Get and delete namespaces **/
  ReturnCode = GetNamespaces(&gNvmDimmDriverNvmDimmConfig, &NamespaceListHead, &NamespaceCount,
    pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (NamespaceCount > 0) {
    LIST_FOR_EACH(pNode, &NamespaceListHead) {
      pCurNamespace = NAMESPACE_INFO_FROM_NODE(pNode);
      ReturnCode = DeleteNamespace(&gNvmDimmDriverNvmDimmConfig, TRUE, pCurNamespace->NamespaceId,
      pCommandStatus);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to delete namespaces");
        goto Finish;
      }
    }
  }

  ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);

  // Set Alignment to 1GiB
  gNvmDimmData->Alignments.RegionPartitionAlignment = SIZE_1GB;

  /** Run create goal **/
  ReturnCode = CreateGoalConfig(&gNvmDimmDriverNvmDimmConfig,
                                FALSE,                        // Not a dry run
                                NULL,                         // Use all DIMMs
                                0,                            // Use all DIMMs
                                NULL,                         // Use all sockets
                                0,                            // Use all sockets
                                PersistentMemType,            // Get PMType from variable
                                pIntelDIMMConfig->MemorySize, // Get MemorySize from variable
                                0,                            // No reserved
                                RESERVE_DIMM_NONE,            // No reserve DIMM
                                Major,                        // Major label version from variable
                                Minor,                        // Minor label version from variable
                                NULL,
                                pCommandStatus);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("CreateGoalConfig failed with ReturnCode: %d", ReturnCode);
    goto Finish;
  }

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(ppDimms);
  LIST_FOR_EACH_SAFE(pNode, pNextNode, &NamespaceListHead) {
    FreePool(NAMESPACE_INFO_FROM_NODE(pNode));
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks inputs and executes create namespace on empty ISets

  @param[in] pIntelDIMMConfig Pointer to struct containing EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
AutomaticCreateNamespace(
  IN     INTEL_DIMM_CONFIG *pIntelDIMMConfig
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  BOOLEAN BttEnabled = FALSE;
  UINT32 RegionCount = 0;
  REGION_INFO *pRegions = NULL;
  UINT32 Index = 0;
  UINT64 AdjustedCapacity = 0;
  UINT16 CreatedNamespaceId = 0;

  NVDIMM_ENTRY();

  if (pIntelDIMMConfig == NULL) {
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  if (pIntelDIMMConfig->NamespaceFlags == NAMESPACE_FLAG_BTT) {
    BttEnabled = TRUE;
  } else if (pIntelDIMMConfig->NamespaceFlags != NAMESPACE_FLAG_NONE) {
    NVDIMM_DBG("Invalid NamespaceFlags: %d", pIntelDIMMConfig->NamespaceFlags);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Find all Regions
  GetRegionCount(&gNvmDimmDriverNvmDimmConfig, FALSE, &RegionCount);

  pRegions = AllocateZeroPool(sizeof(REGION_INFO) * RegionCount);
  if (pRegions == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  ReturnCode = GetRegions(&gNvmDimmDriverNvmDimmConfig, RegionCount, FALSE, pRegions, pCommandStatus);

  for (Index = 0; Index < RegionCount; Index++) {
    // Check if Region is empty
    if (pRegions[Index].Capacity != pRegions[Index].FreeCapacity) {
      NVDIMM_DBG("Region %d is not empty. Skip automatic namespace provision", pRegions[Index].RegionId);
      // No interleave sets to provision is success
      ReturnCode = EFI_SUCCESS;
      continue;
    }
      ReturnCode = CreateNamespace(&gNvmDimmDriverNvmDimmConfig,
                                   pRegions[Index].RegionId,          // Iterate through Regions
                                   DIMM_PID_NOTSET,
                                   NAMESPACE_PM_NAMESPACE_BLOCK_SIZE,
                                   NAMESPACE_BLOCK_COUNT_UNDEFINED,   // Use all free space
                                   NULL,                              // No name
                                   BttEnabled,                        // BTT from variable
                                   TRUE,                              // ForceAll
                                   TRUE,                              // ForceAlignment
                                   &AdjustedCapacity,
                                   &CreatedNamespaceId,
                                   pCommandStatus);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Automatic ns provisioning failed. ReturnCode: %d", ReturnCode);
        goto Finish;
      }
    }

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(pRegions);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Updates ProvisionCapacityStatus

  @param[in] ProvisionCapacityStatus New status to write
**/
VOID
UpdateIntelDIMMConfig(
  IN    INTEL_DIMM_CONFIG *pIntelDIMMConfig
  )
{
  SET_VARIABLE_NV(
    INTEL_DIMM_CONFIG_VARIABLE_NAME,
    gIntelDimmConfigVariableGuid,
    sizeof(INTEL_DIMM_CONFIG),
    pIntelDIMMConfig);
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  NVDIMM_CONFIGURATION_HEADER *pConfHeader = NULL;
  NVDIMM_PLATFORM_CONFIG_INPUT *pConfigInput = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  INTEL_DIMM_CONFIG *pIntelDIMMConfigPCD = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  UINT32 Index = 0;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  BOOLEAN MatchingTableFound = FALSE;

  NVDIMM_ENTRY();

  if (pIntelDIMMConfigEfiVar == NULL) {
    goto Finish;
  }


  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * MAX_DIMMS);
  if (ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Get all DIMMs
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0,
      REQUIRE_DCPMMS_MANAGEABLE | REQUIRE_DCPMMS_FUNCTIONAL,
      ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], FALSE, &pConfHeader);

#ifdef MEMORY_CORRUPTION_WA
  if (ReturnCode == EFI_DEVICE_ERROR) {
    ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], FALSE, &pConfHeader);
  }
#endif // MEMORY_CORRUPTIO_WA
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Error in retrieving Input Config table");
      goto Finish;
    }

    if (pConfHeader->ConfInputStartOffset == 0 || pConfHeader->ConfInputDataSize == 0) {
      // No input Config table
      NVDIMM_DBG("No input config table");
      *pVarsMatch = FALSE;
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }

    pConfigInput = GET_NVDIMM_PLATFORM_CONFIG_INPUT(pConfHeader);

    pCurPcatTable = (PCAT_TABLE_HEADER *) &pConfigInput->pPcatTables;
    SizeOfPcatTables = pConfigInput->Header.Length - (UINT32) ((UINT8 *)pCurPcatTable - (UINT8 *)pConfigInput);

    MatchingTableFound = FALSE;

    while ((UINT32) ((UINT8 *) pCurPcatTable - (UINT8 *) &pConfigInput->pPcatTables) < SizeOfPcatTables) {
      if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
        pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *) pCurPcatTable;
        if (CompareGuid(&pConfigManagementAttributesInfo->Guid, &gIntelDimmConfigVariableGuid)) {
          pIntelDIMMConfigPCD = (INTEL_DIMM_CONFIG *) pConfigManagementAttributesInfo->pGuidData;

          if ((pIntelDIMMConfigPCD->Revision == pIntelDIMMConfigEfiVar->Revision) &&
              (pIntelDIMMConfigPCD->ProvisionCapacityMode == pIntelDIMMConfigEfiVar->ProvisionCapacityMode) &&
              (pIntelDIMMConfigPCD->MemorySize == pIntelDIMMConfigEfiVar->MemorySize) &&
              (pIntelDIMMConfigPCD->PMType == pIntelDIMMConfigEfiVar->PMType) &&
              (pIntelDIMMConfigPCD->NamespaceLabelVersion == pIntelDIMMConfigEfiVar->NamespaceLabelVersion) &&
              (pIntelDIMMConfigPCD->ProvisionNamespaceMode == pIntelDIMMConfigEfiVar->ProvisionNamespaceMode) &&
              (pIntelDIMMConfigPCD->NamespaceFlags == pIntelDIMMConfigEfiVar->NamespaceFlags)) {
            NVDIMM_DBG("EFI Vars and PCD data match");
            MatchingTableFound = TRUE;
            break;
          }
        }
      }
      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, ((PCAT_TABLE_HEADER *) pCurPcatTable)->Length);
    }
    if (!MatchingTableFound) {
      // Did not find an extension table with guid
      // Or found one but with different variables
      NVDIMM_DBG("Matching auto conf extension table not found");
      *pVarsMatch = FALSE;
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }
  }

  NVDIMM_DBG("All DIMM's have matching EFI vars");
  *pVarsMatch = TRUE;
  ReturnCode = EFI_SUCCESS;

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(ppDimms);
  FREE_POOL_SAFE(pConfHeader);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if the topology has changed based on CCUR config status

  @param[out] pTopologyChanged True if ConfigStatus indicates topology change

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
CheckTopologyChange(
     OUT BOOLEAN *pTopologyChanged
  )
{
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pDimmList = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  DIMM *pCurrentDimm = NULL;

  NVDIMM_ENTRY();

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  // Check if any DIMMs are non-functional
  // Can't check topology if a DIMM is non-functional
  pDimmList = &gNvmDimmData->PMEMDev.Dimms;

  LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
    pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
    if (pCurrentDimm->NonFunctional == TRUE) {
      NVDIMM_ERR("Non-functional DIMM found. Cannot check topology change");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    if (pCurrentDimm->ConfigStatus != DIMM_CONFIG_SUCCESS) {
      if ((pCurrentDimm->ConfigStatus == DIMM_CONFIG_IS_INCOMPLETE) ||
          (pCurrentDimm->ConfigStatus == DIMM_CONFIG_NO_MATCHING_IS) ||
          (pCurrentDimm->ConfigStatus == DIMM_CONFIG_NEW_DIMM)) {
        NVDIMM_DBG("Topology changed detected");
        *pTopologyChanged = TRUE;
         ReturnCode = EFI_SUCCESS;
         goto Finish;
      } else {
        // Config error
        // This should not happen as the goal status was checked to enter this state
        ReturnCode = EFI_ABORTED;
        goto Finish;
      }
    }
  }

  // All DIMM_CONFIG_SUCCESS means no topology change
  NVDIMM_DBG("No Topology change detected");
  *pTopologyChanged = FALSE;
  ReturnCode = EFI_SUCCESS;

Finish:
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks if the previous goal was applied successfully

  @param[out] pGoalSuccess True if goal was applied successfully

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
CheckGoalStatus(
     OUT BOOLEAN *pGoalSuccess
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on InitializeCommandStatus");
    goto Finish;
  }

  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * MAX_DIMMS);
  if (ppDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Get all DIMMs
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0,
      REQUIRE_DCPMMS_MANAGEABLE |
      REQUIRE_DCPMMS_FUNCTIONAL,
      ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms, FALSE, TRUE);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (ppDimms[Index]->GoalConfigStatus != GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS) {
      *pGoalSuccess = FALSE;
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  *pGoalSuccess = TRUE;
  ReturnCode = EFI_SUCCESS;

Finish:
  FreeCommandStatus(&pCommandStatus);
  FREE_POOL_SAFE(ppDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  @param[IN] pPercentageRemaining - Pointer to percentage remaining
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMMs has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
InjectError(
    IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
    IN     UINT16 *pDimmIds OPTIONAL,
    IN     UINT32 DimmIdsCount,
    IN     UINT8  ErrorInjType,
    IN     UINT8  ClearStatus,
    IN     UINT64 *pInjectTemperatureValue,
    IN     UINT64 *pInjectPoisonAddress,
    IN     UINT8  *pPoisonType,
    IN     UINT8  *pPercentageRemaining,
    OUT COMMAND_STATUS *pCommandStatus
)
{
    EFI_STATUS ReturnCode = EFI_SUCCESS;
    VOID *pInputPayload = NULL;
    DIMM *pDimms[MAX_DIMMS];
    UINT32 DimmsNum = 0;
    UINT32 Index = 0;
    UINT32 SecurityState = 0;
    PT_PAYLOAD_GET_PACKAGE_SPARING_POLICY *pPayloadPackageSparingPolicy = NULL;
    UINT8 FwStatus = FW_SUCCESS;
    UINT8 DimmARSStatus = 0;

    SetMem(pDimms, sizeof(pDimms), 0x0);

    NVDIMM_ENTRY();

    if (pThis == NULL || pCommandStatus == NULL || (pDimmIds == NULL && DimmIdsCount > 0)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }


    ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0,
      REQUIRE_DCPMMS_MANAGEABLE |
      REQUIRE_DCPMMS_FUNCTIONAL,
      pDimms, &DimmsNum, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    switch (ErrorInjType) {
    case ERROR_INJ_TEMPERATURE:
      pInputPayload = AllocateZeroPool(sizeof(PT_INPUT_PAYLOAD_INJECT_TEMPERATURE));
      if (pInputPayload == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_NO_MEM);
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      if (pInjectTemperatureValue == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_PARAMETER);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      ((PT_INPUT_PAYLOAD_INJECT_TEMPERATURE *)pInputPayload)->Enable = !ClearStatus;
      ((PT_INPUT_PAYLOAD_INJECT_TEMPERATURE *)pInputPayload)->Temperature.Separated.TemperatureInteger =
        (UINT16) *pInjectTemperatureValue;
      for (Index = 0; Index < DimmsNum; Index++) {
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopMediaErrorTemperature, (VOID *)pInputPayload, &FwStatus);
        if (EFI_ERROR(ReturnCode)) {
          if (FwStatus == FW_INJECTION_NOT_ENABLED) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ERROR_INJECTION_BIOS_KNOB_NOT_ENABLED);
            continue;
          }
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
          ReturnCode = EFI_DEVICE_ERROR;
          continue;
        }
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
      }
      break;
    case  ERROR_INJ_PACKAGE_SPARING:
      pInputPayload = AllocateZeroPool(sizeof(PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS));
      if (pInputPayload == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_NO_MEM);
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->TriggersToModify = PACKAGE_SPARING_TRIGGER;
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->PackageSparingTrigger = !ClearStatus;

      for (Index = 0; Index < DimmsNum; Index++) {
        ReturnCode =  FwCmdGetPackageSparingPolicy(pDimms[Index], &pPayloadPackageSparingPolicy);
        if (EFI_ERROR(ReturnCode)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
          ReturnCode = EFI_DEVICE_ERROR;
          continue;
        }
        /* If the package sparing policy has been enabled and executed,
           Support Bit will be 0x00 but the Enable Bit will still be 0x01
           to indicate that the dimm has PackageSparing Policy Enabled before.
         */
        if (pDimms[Index]->SkuInformation.PackageSparingCapable && pPayloadPackageSparingPolicy->Enable &&
           ((!ClearStatus && pPayloadPackageSparingPolicy->Supported) ||
            (ClearStatus && !pPayloadPackageSparingPolicy->Supported))) {
          //Inject the error if PackageSparing policy is available and is supported
          ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, (VOID *)pInputPayload, &FwStatus);
          if (EFI_ERROR(ReturnCode)) {
            if (FwStatus == FW_INJECTION_NOT_ENABLED) {
              SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ERROR_INJECTION_BIOS_KNOB_NOT_ENABLED);
              continue;
            }
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
            ReturnCode = EFI_DEVICE_ERROR;
            FREE_POOL_SAFE(pPayloadPackageSparingPolicy);
            continue;
          }
        } else {
          ReturnCode = EFI_UNSUPPORTED;
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_NOT_SUPPORTED);
          FREE_POOL_SAFE(pPayloadPackageSparingPolicy);
          continue;
        }
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
          FREE_POOL_SAFE(pPayloadPackageSparingPolicy);
      }
      break;
    case ERROR_INJ_DIRTY_SHUTDOWN:
      pInputPayload = AllocateZeroPool(sizeof(PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS));
      if (NULL == pInputPayload) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_NO_MEM);
        goto Finish;
      }
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->TriggersToModify = DIRTY_SHUTDOWN_TRIGGER;
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->DirtyShutdownTrigger = !ClearStatus;
      for (Index = 0; Index < DimmsNum; Index++) {
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, pInputPayload, &FwStatus);
        if (EFI_ERROR(ReturnCode)) {
          if (FwStatus == FW_INJECTION_NOT_ENABLED) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ERROR_INJECTION_BIOS_KNOB_NOT_ENABLED);
            continue;
          }
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
          ReturnCode = EFI_DEVICE_ERROR;
          continue;
        }
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
      }
      break;
    case ERROR_INJ_FATAL_MEDIA_ERR:
      pInputPayload = AllocateZeroPool(sizeof(PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS));
      if (pInputPayload == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_NO_MEM);
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }

    ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->TriggersToModify = FATAL_ERROR_TRIGGER;
    ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->FatalErrorTrigger = !ClearStatus;

    for (Index = 0; Index < DimmsNum; Index++) {
      ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, (VOID *) pInputPayload, &FwStatus);
      if (EFI_ERROR(ReturnCode)) {
        if (FwStatus == FW_INJECTION_NOT_ENABLED) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ERROR_INJECTION_BIOS_KNOB_NOT_ENABLED);
          continue;
        }
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
        ReturnCode = EFI_DEVICE_ERROR;
        continue;
      }
      if (ClearStatus == 1) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS_REQUIRES_POWER_CYCLE);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
      }
    }

    break;
    case ERROR_INJ_POISON:
      pInputPayload = AllocateZeroPool(sizeof(PT_INPUT_PAYLOAD_INJECT_POISON));
      if (pInputPayload == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_NO_MEM);
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      if (pPoisonType == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_PARAMETER);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      ((PT_INPUT_PAYLOAD_INJECT_POISON *)pInputPayload)->Memory = *pPoisonType;

      if (pInjectPoisonAddress == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_PARAMETER);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      ((PT_INPUT_PAYLOAD_INJECT_POISON *)pInputPayload)->DpaAddress = *pInjectPoisonAddress;
      ((PT_INPUT_PAYLOAD_INJECT_POISON *)pInputPayload)->Enable = !ClearStatus;

      for (Index = 0; Index < DimmsNum; Index++) {
        ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &SecurityState);
        if (EFI_ERROR(ReturnCode)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
          continue;
        }
        if (SecurityState & SECURITY_MASK_LOCKED) {
          NVDIMM_DBG("Invalid security check- poison inject error cannot be applied");
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
          ReturnCode = EFI_INVALID_PARAMETER;
          continue;
        }
        ReturnCode = FwCmdGetARS(pDimms[Index], &DimmARSStatus);
        if (LONG_OP_STATUS_IN_PROGRESS == DimmARSStatus) {
          NVDIMM_ERR("ARS in progress.\n");
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ARS_IN_PROGRESS);
          ReturnCode = EFI_DEVICE_ERROR;
          goto Finish;
        }
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopErrorPoison, (VOID *) pInputPayload, &FwStatus);
        if (EFI_ERROR(ReturnCode)) {
          if (FwStatus == FW_INJECTION_NOT_ENABLED) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ERROR_INJECTION_BIOS_KNOB_NOT_ENABLED);
            continue;
          }
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
          ReturnCode = EFI_DEVICE_ERROR;
          continue;
        }
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
      }
       break;
    case ERROR_INJ_PERCENTAGE_REMAINING:
      pInputPayload = AllocateZeroPool(sizeof(PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS));
      if (pInputPayload == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_NO_MEM);
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }
      if (pPercentageRemaining == NULL) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_PARAMETER);
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->TriggersToModify = SPARE_BLOCK_PERCENTAGE_TRIGGER;
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->SpareBlockPercentageTrigger.Separated.Enable = !ClearStatus;
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->SpareBlockPercentageTrigger.Separated.Value = *pPercentageRemaining;
      for (Index = 0; Index < DimmsNum; Index++) {
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, (VOID *)pInputPayload, &FwStatus);
        if (EFI_ERROR(ReturnCode)) {
          if (FwStatus == FW_INJECTION_NOT_ENABLED) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_ERROR_INJECTION_BIOS_KNOB_NOT_ENABLED);
            continue;
          }
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
          ReturnCode = EFI_DEVICE_ERROR;
          continue;
        }
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
      }
    }

    // Repopulate boot status register & bitmask values for all targeted DIMMs whenever FwCmdInjectError has been attempted
    for (Index = 0; Index < DimmsNum; Index++) {
      CHECK_RESULT(GetBSRAndBootStatusBitMask(pThis, pDimms[Index]->DimmID, &pDimms[Index]->Bsr.AsUint64,
        &pDimms[Index]->BootStatusBitmask), Finish);
    }

Finish:
  FREE_POOL_SAFE(pInputPayload);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
EFI_STATUS
EFIAPI
GetBSRAndBootStatusBitMask(
  IN      EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN      UINT16 DimmID,
  OUT     UINT64 *pBsrValue OPTIONAL,
  OUT     UINT16 *pBootStatusBitmask OPTIONAL
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  COMMAND_STATUS *pCommandStatus = NULL;
  UINT64 *pLocalBsr = NULL;
  UINT16 *pLocalBootStatusBitmask = NULL;
  DIMM_BSR JunkBsr;  // passed to function with value never used
  UINT16 JunkBootStatusBitmask = 0; // passed to function with value never used
  NVDIMM_ENTRY();

  if (pThis == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ZeroMem(&JunkBsr, sizeof(JunkBsr));

  pLocalBsr = pBsrValue;
  if (pLocalBsr == NULL) {
    pLocalBsr = (UINT64*)&JunkBsr;
  }

  pLocalBootStatusBitmask = pBootStatusBitmask;
  if (pLocalBootStatusBitmask == NULL) {
    pLocalBootStatusBitmask = &JunkBootStatusBitmask;
  }

  // Set BSR and BootStatusBitmask to default values
  *pLocalBsr = 0;
  *pLocalBootStatusBitmask = DIMM_BOOT_STATUS_UNKNOWN;

  // Initialize pCommandStatus and throw away eventually because API
  // doesn't provide it and it is required for VerifyTargetDimms()
  CHECK_RESULT(InitializeCommandStatus(&pCommandStatus), Finish);

  CHECK_RESULT(VerifyTargetDimms(&DimmID, 1, NULL, 0, REQUIRE_DCPMMS_SELECT_ALL,
    pDimms, &DimmsNum, pCommandStatus), Finish);

  // Populate boot status bitmask based on DDRT/SMBUS interface status
  CHECK_RESULT(PopulateDimmBootStatusBitmaskInterfaceBits(pDimms[0], pLocalBootStatusBitmask), Finish);

  CHECK_RESULT(FwCmdGetBsr(pDimms[0], pLocalBsr), Finish);

  // Populate boot status bitmask based on DIMM BSR value
  CHECK_RESULT(PopulateDimmBootStatusBitmaskBsrBits(pDimms[0], (DIMM_BSR *)pLocalBsr, pLocalBootStatusBitmask), Finish);

Finish:
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get Command Access Policy is used to retrieve a list of FW commands that may be restricted. Passing pCapInfo as NULL
  will provide the maximum number of possible return elements by updating pCount.

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID Handle of the DIMM
  @param[in,out] pCount IN: Count is number of elements in the pCapInfo array. OUT: number of elements written to pCapInfo
  @param[out] pCapInfo Array of Command Access Policy Entries. If NULL, pCount will be updated with maximum number of elements possible. OPTIONAL

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetCommandAccessPolicy(
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  UINT16 DimmID,
  IN OUT UINT32 *pCount,
  OUT COMMAND_ACCESS_POLICY_ENTRY *pCapInfo OPTIONAL
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  UINT32 Index = 0;
  COMMAND_ACCESS_POLICY_ENTRY *pCapEntries;

  //The following CAP entries apply up to and including FIS 2.0
  COMMAND_ACCESS_POLICY_ENTRY CapEntriesOrig[] = {
  { PtSetSecInfo, SubopOverwriteDimm, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID},
  { PtSetSecInfo, SubopSetMasterPass, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetSecInfo, SubopSetPass, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetSecInfo, SubopSecFreezeLock, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetFeatures, SubopAlarmThresholds, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetFeatures, SubopConfigDataPolicy, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetFeatures, SubopAddressRangeScrub, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetAdminFeatures, SubopPlatformDataInfo, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtSetAdminFeatures, SubopLatchSystemShutdownState, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
  { PtUpdateFw, SubopUpdateFw, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID }
  };

  //The following CAP entries apply to FIS 2.1-2.2
  COMMAND_ACCESS_POLICY_ENTRY CapEntries_2_1[] = {
    { PtSetSecInfo, SubopOverwriteDimm, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID},
    { PtSetSecInfo, SubopSetMasterPass, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetSecInfo, SubopSetPass, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetSecInfo, SubopSecEraseUnit, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetSecInfo, SubopSecFreezeLock, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetFeatures, SubopAlarmThresholds, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetFeatures, SubopConfigDataPolicy, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetFeatures, SubopAddressRangeScrub, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetAdminFeatures, SubopPlatformDataInfo, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetAdminFeatures, SubopLatchSystemShutdownState, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtUpdateFw, SubopUpdateFw, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID }
  };

  //The following CAP entries apply to FIS 2.3+
  COMMAND_ACCESS_POLICY_ENTRY CapEntries_2_3[] = {
    { PtSetSecInfo, SubopOverwriteDimm, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID},
    { PtSetSecInfo, SubopSetMasterPass, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetSecInfo, SubopSetPass, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetSecInfo, SubopSecEraseUnit, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetSecInfo, SubopSecFreezeLock, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetFeatures, SubopAlarmThresholds, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetFeatures, SubopConfigDataPolicy, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetFeatures, SubopAddressRangeScrub, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetAdminFeatures, SubopPlatformDataInfo, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtSetAdminFeatures, SubopLatchSystemShutdownState, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtUpdateFw, SubopUpdateFw, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID },
    { PtUpdateFw, SubopFwActivate, COMMAND_ACCESS_POLICY_RESTRICTION_INVALID }
  };

  NVDIMM_ENTRY();

  if (pThis == NULL || pCount == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL || !IsDimmManageable(pDimm)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (NULL == pCapInfo) {  // pCapInfo is NULL so just getting size
    if (((pDimm->FwVer.FwApiMajor == 0x2) &&
      (pDimm->FwVer.FwApiMinor >= 0x3)) ||
      (pDimm->FwVer.FwApiMajor >= 0x3))
    {
      *pCount = COUNT_OF(CapEntries_2_3);
      ReturnCode = EFI_SUCCESS;
    }
    else if ((pDimm->FwVer.FwApiMajor == 0x2) &&
      (pDimm->FwVer.FwApiMinor >= 0x1))
    {
      *pCount = COUNT_OF(CapEntries_2_1);
      ReturnCode = EFI_SUCCESS;
    }
    else {
      *pCount = COUNT_OF(CapEntriesOrig);
      ReturnCode = EFI_SUCCESS;
    }
    NVDIMM_DBG("Setting pCount to %d.", *pCount);
    goto Finish;
  }

  if (((pDimm->FwVer.FwApiMajor == 0x2) &&
    (pDimm->FwVer.FwApiMinor >= 0x3)) ||
    (pDimm->FwVer.FwApiMajor >= 0x3))
  {
    if (*pCount == COUNT_OF(CapEntries_2_3)) {
      pCapEntries = CapEntries_2_3;
    }
    else
    {
      NVDIMM_DBG("Parameter pCount should be %d for FIS2.3+ DIMM.  Received pCount = %d.", COUNT_OF(CapEntries_2_3), *pCount);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
  else if ((pDimm->FwVer.FwApiMajor == 0x2) &&
    (pDimm->FwVer.FwApiMinor >= 0x1))
  {
    if (*pCount == COUNT_OF(CapEntries_2_1)) {
      pCapEntries = CapEntries_2_1;
    }
    else
    {
      NVDIMM_DBG("Parameter pCount should be %d for FIS2.1-2 DIMM.  Received pCount = %d.", COUNT_OF(CapEntries_2_1), *pCount);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }
  else
  {
    if (*pCount == COUNT_OF(CapEntriesOrig)) {
      pCapEntries = CapEntriesOrig;
    }
    else
    {
      NVDIMM_DBG("Parameter pCount should be %d for pre-FIS2.1 DIMM.  Received pCount = %d.", COUNT_OF(CapEntriesOrig), *pCount);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  for (Index = 0; Index < *pCount; Index++) {
    ReturnCode = FwCmdGetCommandAccessPolicy(pDimm, pCapEntries[Index].Opcode,
      pCapEntries[Index].SubOpcode, &pCapEntries[Index].Restriction);

    if (EFI_UNSUPPORTED == ReturnCode) {
      pCapEntries[Index].Restriction = COMMAND_ACCESS_POLICY_RESTRICTION_UNSUPPORTED;
    } else if (EFI_ERROR(ReturnCode)) {
      // If error, leave entry as invalid, but make sure it's still set
      pCapEntries[Index].Restriction = COMMAND_ACCESS_POLICY_RESTRICTION_INVALID;
    }

    CopyMem_S(&pCapInfo[Index], (sizeof(*pCapInfo)), &pCapEntries[Index], (sizeof(*pCapInfo)));
    NVDIMM_DBG("Retrieved Command Access Policy for 0x%x:0x%x. ReturnCode=0x%x.",
      pCapEntries[Index].Opcode, pCapEntries[Index].SubOpcode, ReturnCode);
  }

  ReturnCode = EFI_SUCCESS;
  goto Finish;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get Command Effect Log is used to retrieve a list of FW commands and their
  effects on the PMem module subsystem.

  @param[in] pThis - A pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance.
  @param[in] DimmID - Handle of the PMem module
  @param[out] ppLogEntry - A pointer to the CEL entry table for a given PMem module.
  @param[out] pEntryCount - The number of CEL entries

  @retval EFI_SUCCESS Success
  @retval ERROR any non-zero value is an error (more details in Base.h)
**/
EFI_STATUS
EFIAPI
GetCommandEffectLog(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT16 DimmID,
     OUT COMMAND_EFFECT_LOG_ENTRY **ppLogEntry,
     OUT UINT32 *pEntryCount
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  COMMAND_STATUS *pCommandStatus = NULL;

  if (pThis == NULL) {
    NVDIMM_DBG("One or more parameters are NULL");
    goto Finish;
  }

  // Make a dummy command status for now for VerifyTargetDimms. Hopefully
  // we can correct the ShowCEL command and this api call to pass it in and
  // use it
  CHECK_RESULT(InitializeCommandStatus(&pCommandStatus), Finish);

  CHECK_RESULT(VerifyTargetDimms(&DimmID, 1, NULL, 0, REQUIRE_DCPMMS_MANAGEABLE, pDimms,
      &DimmsNum, pCommandStatus), Finish);

  CHECK_RESULT(FwCmdGetCommandEffectLog(pDimms[0], ppLogEntry, pEntryCount), Finish);

Finish:
  FreeCommandStatus(&pCommandStatus);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#ifndef OS_BUILD
/**
  This function makes calls to the dimms required to initialize the driver.

  @retval EFI_SUCCESS if no errors.
  @retval EFI_xxxx depending on error encountered.
**/
EFI_STATUS
LoadArsList()
{
  UINT32 x = 0;
  UINT32 records = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  if (gArsBadRecordsCount >= 0) {
    NVDIMM_DBG("ARS list already loaded.\n");
    goto Finish;
  }
  gArsBadRecordsCount = 0;

  //First check how many records exist by passing NULL
  ReturnCode = gNvmDimmData->pDcpmmProtocol->DcpmmArsStatus(&records, NULL);
  if (ReturnCode == EFI_NOT_READY)
  {
    NVDIMM_WARN("BIOS reports not ready for full ARS list. The returned list may be partial.");
    ReturnCode = EFI_SUCCESS;
  }

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Could not obtain the ARS bad address list count");
    goto Finish;
  }

  //if there are records, allocate the space for them and obtain them
  if (records > 0) {
    gArsBadRecords = (DCPMM_ARS_ERROR_RECORD *)AllocateZeroPool(sizeof(DCPMM_ARS_ERROR_RECORD) * records);
    if (gArsBadRecords == NULL) {
      NVDIMM_WARN("Failed to allocate memory for the bad ARS records");
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    ReturnCode = gNvmDimmData->pDcpmmProtocol->DcpmmArsStatus(&records, gArsBadRecords);
    if (ReturnCode == EFI_NOT_READY)
    {
      NVDIMM_WARN("BIOS reports not ready for full ARS list. The returned list may be partial.");
      ReturnCode = EFI_SUCCESS;
    }

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Could not obtain the ARS bad address list");
      FreePool(gArsBadRecords);
      goto Finish;
    }

    gArsBadRecordsCount = records;
    for (; x < records; x++)
    {
      NVDIMM_DBG("ArsBadRecords[%d] = 0x%llx, len = 0x%llx, nfit handle = 0x%llx",
        x, gArsBadRecords[x].SpaOfErrLoc, gArsBadRecords[x].Length, gArsBadRecords[x].NfitHandle);
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif

/**
  Gets value of Smbus protocol configuration global variable from platform

  @param[in]     pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in,out] pAttribs A pointer to a variable used to store protocol and payload settings

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
**/
EFI_STATUS
EFIAPI
GetFisTransportAttributes(
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  OUT EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS *pAttribs
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (NULL == pThis || NULL == pAttribs) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  pAttribs->Protocol = gTransportAttribs.Protocol;
  pAttribs->PayloadSize = gTransportAttribs.PayloadSize;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Sets value of Smbus protocol configuration global variable for platform

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in] Attribs A pointer to a variable used to store protocol and payload settings

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid FW Command Parameter.
**/
EFI_STATUS
EFIAPI
SetFisTransportAttributes(
  IN  EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS Attribs
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (NULL == pThis) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  if ((Attribs.Protocol > FisTransportAuto) || (Attribs.PayloadSize > FisTransportSizeAuto)) {
    // Covers case where multiple values are somehow set
    NVDIMM_DBG("Incorrect transport attributes detected");
    goto Finish;
  }

  gTransportAttribs.Protocol = Attribs.Protocol;
  gTransportAttribs.PayloadSize = Attribs.PayloadSize;

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Get minimal FWImageMaxSize value for all designated DIMMs

@param[in] pNvmDimmConfigProtocol - The open config protocol
@param[in] pDimms - Pointer to an array of DIMMs
@param[in] DimmsNum - Number of items in array of DIMMs

@retval The minimal allowed size of firmware image buffer in bytes
@retval MAX_UINT64 is an error
**/
UINT64
GetMinFWImageMaxSize(
  IN   EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN   DIMM *pDimms[MAX_DIMMS],
  IN   UINT32 DimmsNum
) {

  UINT64 RetVal = 0;
  UINT64 MinSize = MAX_UINT64;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM_INFO *pDimm = NULL;
  UINT32 Index = 0;
  NVDIMM_ENTRY();

  if (NULL == pNvmDimmConfigProtocol || NULL == pDimms) {
    NVDIMM_DBG("One or more parameters are NULL");
    goto Finish;
  }

  if (0 == DimmsNum) {
    NVDIMM_DBG("Invalid DIMMs number passed");
    goto Finish;
  }

  pDimm = AllocateZeroPool(sizeof(*pDimm));
  if (pDimm == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    CHECK_RESULT((GetDimmInfo(pDimms[Index], DIMM_INFO_CATEGORY_FW_IMAGE_INFO, pDimm)), Finish);
    if (pDimm->FWImageMaxSize > 0 && pDimm->FWImageMaxSize < MinSize) {
      MinSize = pDimm->FWImageMaxSize;
    }
  }

  RetVal = MinSize;

Finish:
  FREE_POOL_SAFE(pDimm);
  NVDIMM_EXIT_I64(ReturnCode);
  return RetVal;
}

#ifndef OS_BUILD
/**
  Gets value of PcdDebugPrintErrorLevel for the pmem driver

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[out] ErrorLevel A pointer used to store the value of debug print error level

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid ErrorLevel Parameter.
**/
EFI_STATUS
EFIAPI
GetDriverDebugPrintErrorLevel(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
     OUT UINT32 *pErrorLevel
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (NULL == pThis || NULL == pErrorLevel) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  *pErrorLevel = PatchPcdGet32(PcdDebugPrintErrorLevel);

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Sets value of PcdDebugPrintErrorLevel for the pmem driver

  @param[in] pThis A pointer to EFI DCPMM CONFIG PROTOCOL structure
  @param[in] ErrorLevel The new value to assign to debug print error level

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Invalid ErrorLevel Parameter.
**/
EFI_STATUS
EFIAPI
SetDriverDebugPrintErrorLevel(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pThis,
  IN     UINT32 ErrorLevel
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (NULL == pThis) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  PatchPcdSet32(PcdDebugPrintErrorLevel, ErrorLevel);

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif //OS_BUILD
