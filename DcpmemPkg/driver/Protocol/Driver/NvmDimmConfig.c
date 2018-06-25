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
#include <DumpLoadPools.h>
#include <NvmInterface.h>
#include "NvmSecurity.h"
#include <NvmWorkarounds.h>
#include <NvmTables.h>
#include <Convert.h>
#include <ShowAcpi.h>
#include <CoreDiagnostics.h>
#include <NvmHealth.h>
#include <Utility.h>

#ifndef OS_BUILD
#include <SpiRecovery.h>
#include <FConfig.h>
#include <Spi.h>
#include <Smbus.h>
#endif

#ifdef OS_BUILD
#include <os_efi_api.h>
#include "event.h"
#endif // OS_BUILD
#ifdef __MFG__
#include <mfg/Mfg.h>
#endif

/** Memory Device SMBIOS Table **/
#define SMBIOS_TYPE_MEM_DEV             17
/** Memory Device Mapped Address SMBIOS Table **/
#define SMBIOS_TYPE_MEM_DEV_MAPPED_ADDR 20

#define TEST_NAMESPACE_NAME_LEN         14

EFI_GUID gNvmDimmConfigProtocolGuid = EFI_DCPMM_CONFIG_PROTOCOL_GUID;

extern NVMDIMMDRIVER_DATA *gNvmDimmData;
extern CONST UINT64 gSupportedBlockSizes[SUPPORTED_BLOCK_SIZES_COUNT];

/**
  Instance of NvmDimmConfigProtocol
**/
EFI_DCPMM_CONFIG_PROTOCOL gNvmDimmDriverNvmDimmConfig =
{
  NVMD_CONFIG_PROTOCOL_VERSION,
  InitializeNvmDimmDriver,
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
  DeletePcd,
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
  ModifyNamespace,
  DeleteNamespace,
  GetErrorLog,
  DumpFwDebugLog,
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
#ifdef __MFG__
  InjectAndUpdateMfgToProdfw,
#endif
#ifndef MDEPKG_NDEBUG
  PassThruCommand
#endif /* MDEPKG_NDEBUG */
};

/**
  Run the time intensive initialization routines. This should be called by
  any module prior to using the driver protocols.

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.

  @retval EFI_SUCCESS  Initialization succeeded
  @retval EFI_XXX Any number of EFI error codes
**/
EFI_STATUS
EFIAPI
InitializeNvmDimmDriver (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis
  )
{
   EFI_STATUS ReturnCode = EFI_SUCCESS;

   if (!gNvmDimmData->DriverInitialized) {
      ReturnCode = InitializeDimms();
      if (!EFI_ERROR(ReturnCode)) {
         gNvmDimmData->DriverInitialized = TRUE;
      }
   }
   return ReturnCode;
}

#ifdef OS_BUILD
#include "event.h"
#include <stdio.h>
#include <stdarg.h>

/*
* Store an event log entry in the system event log for the dimms list
*/
EFI_STATUS StoreSystemEntryForDimm(OBJECT_STATUS *pObjectStatus, CONST CHAR16 *source, UINT32 event_type, CONST CHAR16  *message, ...)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  VA_LIST args;
  NVM_EVENT_MSG_W event_message = { 0 };

  if ((pObjectStatus == NULL) || (NULL == message)) {
    ReturnCode = EFI_INVALID_PARAMETER;
  } else {
    // Prepare the string
    VA_START(args, message);
    UnicodeVSPrint(event_message, sizeof(event_message), message, args);
        VA_END(args);
    // Store the log
    nvm_store_system_entry_widechar(source, event_type, pObjectStatus->ObjectIdStr, event_message);
  }

  return ReturnCode;
}
/*
* Store an event log entry in the system event log for the dimms list
*/
EFI_STATUS StoreSystemEntryForDimmList(COMMAND_STATUS *pCommandStatus, CONST CHAR16 *source, UINT32 event_type, CONST CHAR16  *message)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pObjectStatusNode = NULL;
  OBJECT_STATUS *pObjectStatus = NULL;
  UINT32 Index = 0;
  BOOLEAN IsDimmStatusSuccess = TRUE;

  if (pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
  }
  else
  {
    LIST_FOR_EACH(pObjectStatusNode, &pCommandStatus->ObjectStatusList) {
      pObjectStatus = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);
      // Check the DIMM status
      IsDimmStatusSuccess = TRUE;
      for (Index = 0; Index < ((NVM_LAST_STATUS_VALUE / 64) + 1); Index++) {
        if ((0 == Index) && (NVM_SUCCESS_FW_RESET_REQUIRED == pObjectStatus->StatusBitField.BitField[Index])) {
          continue;
        }
        if (pObjectStatus->StatusBitField.BitField[Index] != NVM_SUCCESS) {
          IsDimmStatusSuccess = FALSE;
        }
      }
      if (IsDimmStatusSuccess) {
            // Store the log
        StoreSystemEntryForDimm(pObjectStatus, source, event_type, message, pObjectStatus->ObjectId);
        }
    }
  }

    return ReturnCode;
}

/*
* Converts the sensor Id value to the wide character string
*/
STATIC CHAR16* ConvertSensorIdToStringW(UINT8 SensorId)
{
  if (SensorId == SENSOR_TYPE_CONTROLLER_TEMPERATURE) {
    return L"ControllerTemperature";
  } else if (SensorId == SENSOR_TYPE_MEDIA_TEMPERATURE) {
    return L"MediaTemperature";
  } else if (SensorId == SENSOR_TYPE_PERCENTAGE_REMAINING) {
    return L"PercentageRemaining";
  }
  return L"";
}

#endif // OS_BUILD

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

  VariableSize = sizeof(pDriverPreferences->AppDirectGranularity);
  ReturnCode = GET_VARIABLE(
    APPDIRECT_GRANULARITY_VARIABLE_NAME,
    gNvmDimmNgnvmVariableGuid,
    &VariableSize,
    &pDriverPreferences->AppDirectGranularity);

  if(ReturnCode == EFI_NOT_FOUND) {
    pDriverPreferences->AppDirectGranularity = APPDIRECT_GRANULARITY_DEFAULT;
    ReturnCode = EFI_SUCCESS;
  } else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve AppDirect Granularity Variable");
    goto Finish;
  }

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
        pDimm->SkuInformation.StorageModeEnabled == MODE_DISABLED &&
        pDimm->SkuInformation.AppDirectModeEnabled == MODE_DISABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuAppDirectModeOnly:
    if (pDimm->SkuInformation.MemoryModeEnabled == MODE_DISABLED &&
        pDimm->SkuInformation.StorageModeEnabled == MODE_DISABLED &&
        pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuAppDirectStorageMode:
    if (pDimm->SkuInformation.MemoryModeEnabled == MODE_DISABLED &&
        pDimm->SkuInformation.StorageModeEnabled == MODE_ENABLED &&
        pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
      ReturnValue = TRUE;
    }
    break;

  case SkuTriMode:
    if (pDimm->SkuInformation.MemoryModeEnabled == MODE_ENABLED &&
        pDimm->SkuInformation.StorageModeEnabled == MODE_ENABLED &&
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
    if (pDimm->SkuInformation.SoftProgramableSku == MODE_ENABLED) {
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDimmCount == NULL) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  GetListSize(&gNvmDimmData->PMEMDev.Dimms, pDimmCount);

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NVDIMM_ENTRY();

  if (pThis == NULL || pDimmCount == NULL) {
    NVDIMM_DBG("Input parameter is NULL");
    goto Finish;
  }

  GetListSize(&gNvmDimmData->PMEMDev.UninitializedDimms, pDimmCount);

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Populate the boot status bitmask

  @param[in] pBsr BSR Boot Status Register to convert to bitmask
  @param[in] pDimm to retrieve DDRT Training status from
  @param[out] pBootStatusBitmask Pointer to the boot status bitmask

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
STATIC
EFI_STATUS
PopulateDimmBootStatusBitmask(
  IN     DIMM_BSR *pBsr,
  IN     DIMM *pDimm,
     OUT UINT16 *pBootStatusBitmask
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  UINT16 BootStatusBitmask = 0;
  UINT8 DdrtTrainingStatus = DDRT_TRAINING_UNKNOWN;

  NVDIMM_ENTRY();

  if (pBsr == NULL || pBootStatusBitmask == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if ((pBsr->AsUint64 == MAX_UINT64_VALUE) || (pBsr->AsUint64 == 0)) {
    BootStatusBitmask = DIMM_BOOT_STATUS_UNKNOWN;
  } else {
    if (pBsr->Separated_Current_FIS.MR == DIMM_BSR_MEDIA_NOT_TRAINED) {
      BootStatusBitmask |= DIMM_BOOT_STATUS_MEDIA_NOT_READY;
    }
    if (pBsr->Separated_Current_FIS.MR == DIMM_BSR_MEDIA_ERROR) {
      BootStatusBitmask |= DIMM_BOOT_STATUS_MEDIA_ERROR;
    }
    if (pBsr->Separated_Current_FIS.MD == DIMM_BSR_MEDIA_DISABLED) {
      BootStatusBitmask |= DIMM_BOOT_STATUS_MEDIA_DISABLED;
    }
    GetDdrtIoInitInfo(NULL, pDimm->DimmID, &DdrtTrainingStatus);
    if (DdrtTrainingStatus == DDRT_TRAINING_UNKNOWN) {
      NVDIMM_DBG("Could not retrieve DDRT training status");
    }
    if (DdrtTrainingStatus != DDRT_TRAINING_COMPLETE && DdrtTrainingStatus != DDRT_S3_COMPLETE) {
      BootStatusBitmask |= DIMM_BOOT_STATUS_DDRT_NOT_READY;
    }
    if (pBsr->Separated_Current_FIS.MBR == DIMM_BSR_MAILBOX_NOT_READY) {
      BootStatusBitmask |= DIMM_BOOT_STATUS_MAILBOX_NOT_READY;
    }
    if (pBsr->Separated_Current_FIS.RR == DIMM_BSR_REBOOT_REQUIRED) {
      BootStatusBitmask |= DIMM_BOOT_STATUS_REBOOT_REQUIRED;
    }
  }

  *pBootStatusBitmask = BootStatusBitmask;

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

  ReturnCode = GetPlatformConfigDataOemPartition(pDimm, &pPcdConfHeader);
  if (EFI_ERROR(ReturnCode)) {
    return EFI_DEVICE_ERROR;
  }

  if (pPcdConfHeader->CurrentConfStartOffset == 0 || pPcdConfHeader->CurrentConfDataSize == 0) {
    NVDIMM_DBG("There is no Current Config table");
    FreePool(pPcdConfHeader);
    return EFI_LOAD_ERROR;
  }

  pPcdCurrentConf = GET_NVDIMM_CURRENT_CONFIG(pPcdConfHeader);

  if (pPcdCurrentConf->Header.Signature != NVDIMM_CURRENT_CONFIG_SIG) {
    NVDIMM_DBG("Incorrect signature of the DIMM Current Config table");
    FreePool(pPcdConfHeader);
    return EFI_VOLUME_CORRUPTED;
  } else if (pPcdCurrentConf->Header.Length > pDimm->PcdOemPartitionSize) {
    NVDIMM_DBG("Length of PCD Current Config header is greater than max PCD OEM partition size");
    FreePool(pPcdConfHeader);
    return EFI_VOLUME_CORRUPTED;
  } else if ((pPcdCurrentConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_1) &&
    (pPcdCurrentConf->Header.Revision != NVDIMM_CONFIGURATION_TABLES_REVISION_2)) {
    NVDIMM_DBG("Revision of PCD Current Config table is invalid");
    FreePool(pPcdConfHeader);
    return EFI_VOLUME_CORRUPTED;
  } else if (!IsChecksumValid(pPcdCurrentConf, pPcdCurrentConf->Header.Length)) {
    NVDIMM_DBG("The Current Config table checksum is invalid.");
    FreePool(pPcdConfHeader);
    return EFI_VOLUME_CORRUPTED;
  }

  pDimm->ConfigStatus = (UINT8)pPcdCurrentConf->ConfigStatus;
  pDimm->IsNew = (pDimm->ConfigStatus == DIMM_CONFIG_NEW_DIMM) ? 1 : 0;

  switch (pPcdCurrentConf->ConfigStatus) {
    case DIMM_CONFIG_SUCCESS:
    case DIMM_CONFIG_OLD_CONFIG_USED:
      pDimm->Configured = TRUE;
      break;
    default:
      pDimm->Configured = FALSE;
    break;
  }

  pDimm->MappedVolatileCapacity = pPcdCurrentConf->VolatileMemSizeIntoSpa;
  pDimm->MappedPersistentCapacity = pPcdCurrentConf->PersistentMemSizeIntoSpa;

  FreePool(pPcdConfHeader);
  return EFI_SUCCESS;
}

#endif // OS_BUILD

/*
 * Helper function for initializing information from the NFIT for non-functional
 * dimms only. This should eventually include functional dimms as well
 * (GetDimmInfo), but currently avoiding as it's hard to extract the NFIT-only
 * calls from GetDimmInfo.
 */
VOID
InitializeNfitDimmInfoFieldsFromDimm(
  IN     DIMM *pDimm,
     OUT DIMM_INFO *pDimmInfo
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

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
#ifdef OS_BUILD
  if (ReturnCode == EFI_SUCCESS) {
    CHAR8 AsciiDimmUid[MAX_DIMM_UID_LENGTH + 1] = { 0 };
    // Prepare DIMM UID
    UnicodeStrToAsciiStrS(pDimmInfo->DimmUid, AsciiDimmUid, MAX_DIMM_UID_LENGTH + 1);
    // Get the action required status
    pDimmInfo->ActionRequired = nvm_get_action_required(AsciiDimmUid);
  }
#endif // OS_BUILD
  if ((pDimmInfo->DimmUid == NULL) || !(StrLen(pDimmInfo->DimmUid) > 0)) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_UID;
  }
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
  PT_PAYLOAD_GET_PACKAGE_SPARING_POLICY *pGetPackageSparingPayload = NULL;
  LIST_ENTRY *pNodeNamespace = NULL;
  NAMESPACE *pCurNamespace = NULL;
  SENSOR_INFO SensorInfo;
  PT_OPTIONAL_DATA_POLICY_PAYLOAD OptionalDataPolicyPayload;
  PT_PAYLOAD_POWER_MANAGEMENT_POLICY PowerManagementPolicyPayload;
  PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE3 *pPayloadMemInfoPage3 = NULL;
  PT_PAYLOAD_FW_IMAGE_INFO *pPayloadFwImage = NULL;
  SMBIOS_STRUCTURE_POINTER DmiPhysicalDev;
  SMBIOS_STRUCTURE_POINTER DmiDeviceMappedAddr;
  SMBIOS_VERSION SmbiosVersion;
  UINT32 LastShutdownStatus = 0;
  UINT64 LastShutdownTime = 0;
  UINT8 AitDramEnabled = 0;
  UINT32 Index = 0;
  UINT64 CapacityFromSmbios = 0;

  NVDIMM_ENTRY();

  ZeroMem(&SensorInfo, sizeof(SensorInfo));
  ZeroMem(&OptionalDataPolicyPayload, sizeof(OptionalDataPolicyPayload));
  ZeroMem(&PowerManagementPolicyPayload, sizeof(PowerManagementPolicyPayload));
  ZeroMem(&DmiPhysicalDev, sizeof(DmiPhysicalDev));
  ZeroMem(&DmiDeviceMappedAddr, sizeof(DmiDeviceMappedAddr));
  ZeroMem(&SmbiosVersion, sizeof(SmbiosVersion));

  if (pDimm == NULL || pDimmInfo == NULL) {
    NVDIMM_DBG("Convert operation failed. Invalid pointer");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pDimmInfo->DimmID = pDimm->DimmID;
  pDimmInfo->SocketId = pDimm->SocketId;
  pDimmInfo->ChannelId = pDimm->ChannelId;
  pDimmInfo->ChannelPos = pDimm->ChannelPos;
  for (Index = 0; Index < pDimm->FmtInterfaceCodeNum; Index++) {
    pDimmInfo->InterfaceFormatCode[Index] = pDimm->FmtInterfaceCode[Index];
  }
  pDimmInfo->InterfaceFormatCodeNum = pDimm->FmtInterfaceCodeNum;

  pDimmInfo->Capacity = pDimm->RawCapacity;
  pDimmInfo->DimmHandle = pDimm->DeviceHandle.AsUint32;
  pDimmInfo->FwVer = pDimm->FwVer;

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

  if (pDimm->SkuInformation.StorageModeEnabled == MODE_ENABLED) {
    pDimmInfo->ModesSupported |= BIT1;
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

  pDimmInfo->ManufacturingInfoValid = pDimm->ManufacturingInfoValid;
  pDimmInfo->ManufacturingLocation = pDimm->ManufacturingLocation;
  pDimmInfo->ManufacturingDate = pDimm->ManufacturingDate;

  pDimmInfo->ManufacturerId = pDimm->Manufacturer;
  pDimmInfo->SerialNumber = pDimm->SerialNumber;

  ReturnCode = GetDmiMemdevInfo(pDimmInfo->DimmID, &DmiPhysicalDev, &DmiDeviceMappedAddr, &SmbiosVersion);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failure to retrieve SMBIOS tables");
  }

  /* SMBIOS type 17 table info */
  if (DmiPhysicalDev.Type17 != NULL) {
    if (DmiPhysicalDev.Type17->MemoryType == SMBIOS_MEMORY_TYPE_DDR4) {
      if (DmiPhysicalDev.Type17->TypeDetail.Nonvolatile) {
        pDimmInfo->MemoryType = MEMORYTYPE_DCPMEM;
      } else {
        pDimmInfo->MemoryType = MEMORYTYPE_DDR4;
      }
    } else {
      pDimmInfo->MemoryType = MEMORYTYPE_UNKNOWN;
    }

    pDimmInfo->FormFactor = DmiPhysicalDev.Type17->FormFactor;
    pDimmInfo->DataWidth = DmiPhysicalDev.Type17->DataWidth;
    pDimmInfo->TotalWidth = DmiPhysicalDev.Type17->TotalWidth;
    pDimmInfo->Speed = DmiPhysicalDev.Type17->Speed;

    ReturnCode = GetSmbiosCapacity(DmiPhysicalDev.Type17->Size, DmiPhysicalDev.Type17->ExtendedSize,
                                    SmbiosVersion, &CapacityFromSmbios);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve capacity from SMBIOS table (%r)", ReturnCode);
    }

    pDimmInfo->CapacityFromSmbios = CapacityFromSmbios;

    ReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &(DmiPhysicalDev.Type17),
                DmiPhysicalDev.Type17->DeviceLocator,
                pDimmInfo->DeviceLocator, sizeof(pDimmInfo->DeviceLocator));
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve the device locator from SMBIOS table (%r)", ReturnCode);
    }
    ReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &(DmiPhysicalDev.Type17),
                DmiPhysicalDev.Type17->BankLocator,
                pDimmInfo->BankLabel, sizeof(pDimmInfo->BankLabel));
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve the bank locator from SMBIOS table (%r)", ReturnCode);
    }
    ReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &(DmiPhysicalDev.Type17),
                DmiPhysicalDev.Type17->Manufacturer,
                pDimmInfo->ManufacturerStr, sizeof(pDimmInfo->ManufacturerStr));
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve the manufacturer string from SMBIOS table (%r)", ReturnCode);
    }
  } else {
    NVDIMM_ERR("SMBIOS table of type 17 for DIMM 0x%x was not found.", pDimmInfo->DimmHandle);
  }

  CHECK_RESULT_CONTINUE(GetDimmUid(pDimm, pDimmInfo->DimmUid, MAX_DIMM_UID_LENGTH));
#ifdef OS_BUILD
  if (ReturnCode == EFI_SUCCESS) {
    CHAR8 AsciiDimmUid[MAX_DIMM_UID_LENGTH + 1] = { 0 };
    // Prepare DIMM UID
    UnicodeStrToAsciiStrS(pDimmInfo->DimmUid, AsciiDimmUid, MAX_DIMM_UID_LENGTH + 1);
    // Get the action required status
    pDimmInfo->ActionRequired = nvm_get_action_required(AsciiDimmUid);
  }
#endif // OS_BUILD
  if ((pDimmInfo->DimmUid == NULL) || !(StrLen(pDimmInfo->DimmUid) > 0)) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_UID;
  }

  ReturnCode = GetDimmUid(pDimm, pDimmInfo->DimmUid, MAX_DIMM_UID_LENGTH);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  if ((pDimmInfo->DimmUid == NULL) || !(StrLen(pDimmInfo->DimmUid) > 0)) {
    pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_UID;
  }

  if (!IsDimmManageable(pDimm)) {
    pDimmInfo->ManageabilityState = MANAGEMENT_INVALID_CONFIG;
    pDimmInfo->HealthState = HEALTH_UNMANAGEABLE;
  } else {
    pDimmInfo->ManageabilityState = MANAGEMENT_VALID_CONFIG;
    pDimmInfo->HealthState = HEALTH_UNKNOWN;
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
    case DIMM_CONFIG_BAD_CONFIG:
    case DIMM_CONFIG_IN_CHECKSUM_NOT_VALID:
    case DIMM_CONFIG_CURR_CHECKSUM_NOT_VALID:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_BAD_CONFIG;
      break;
    default:
      pDimmInfo->ConfigStatus = DIMM_INFO_CONFIG_NOT_CONFIG;
      break;
  }

  pDimmInfo->SkuInformation = *((UINT32 *) &pDimm->SkuInformation);

  /* SKU Violation */
  pDimmInfo->SKUViolation = FALSE;
  if (pDimm->MappedVolatileCapacity > 0 && pDimm->SkuInformation.MemoryModeEnabled == MODE_DISABLED) {
    pDimmInfo->SKUViolation = TRUE;
  } else if (pDimm->MappedPersistentCapacity > 0 && pDimm->SkuInformation.AppDirectModeEnabled == MODE_DISABLED) {
    pDimmInfo->SKUViolation = TRUE;
  } else {
    LIST_FOR_EACH(pNodeNamespace, &pDimm->StorageNamespaceList) {
      pCurNamespace = NAMESPACE_FROM_NODE(pNodeNamespace, DimmNode);
      if (pCurNamespace->NamespaceType == STORAGE_NAMESPACE &&
        pDimm->SkuInformation.StorageModeEnabled == MODE_DISABLED) {
        pDimmInfo->SKUViolation = TRUE;
        break;
      }
    }
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
    /* security state */
    pSecurityPayload = AllocateZeroPool(sizeof(*pSecurityPayload));
    if (pSecurityPayload == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    ReturnCode = FwCmdGetSecurityInfo(pDimm, pSecurityPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FW CMD Error: %r", ReturnCode);
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_SECURITY_INFO;
    }
    ConvertSecurityBitmask(pSecurityPayload->SecurityStatus, &pDimmInfo->SecurityState);
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_PACKAGE_SPARING)
  {
    ReturnCode = FwCmdGetPackageSparingPolicy(pDimm, &pGetPackageSparingPayload);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Get package sparing policy failed with error %r for DIMM 0x%x", ReturnCode, pDimm->DeviceHandle.AsUint32);
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
      NVDIMM_DBG("FwCmdGetARS failed with error %r for DIMM %d", ReturnCode, pDimm->DeviceHandle.AsUint32);
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_SMART_AND_HEALTH)
  {
    /* Get current health state */
    ReturnCode = GetSmartAndHealth(&gNvmDimmDriverNvmDimmConfig,pDimm->DimmID,
      &SensorInfo, &LastShutdownStatus, &LastShutdownTime, &AitDramEnabled);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_SMART_AND_HEALTH;
    }
    if (HEALTH_UNMANAGEABLE != pDimmInfo->HealthState) {
    ConvertHealthBitmask(SensorInfo.HealthStatus, &pDimmInfo->HealthState);
    }
    pDimmInfo->HealthStatusReason = SensorInfo.HealthStatusReason;
    pDimmInfo->LastShutdownStatus = LastShutdownStatus;
    pDimmInfo->LastShutdownTime = LastShutdownTime;
    pDimmInfo->AitDramEnabled = AitDramEnabled;
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_POWER_MGMT_POLICY)
  {
    /* Get current Power Management Policy info */
    ReturnCode = FwCmdGetPowerManagementPolicy(pDimm, &PowerManagementPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_POWER_MGMT;
    }

    pDimmInfo->PeakPowerBudget = PowerManagementPolicyPayload.PeakPowerBudget;
    pDimmInfo->AvgPowerBudget = PowerManagementPolicyPayload.AveragePowerBudget;
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_OPTIONAL_CONFIG_DATA_POLICY)
  {
    /* Get current FirstFastRefresh state */
    ReturnCode = FwCmdGetOptionalConfigurationDataPolicy(pDimm, &OptionalDataPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_OPTIONAL_CONFIG_DATA;
    }
    if (OptionalDataPolicyPayload.FirstFastRefresh == FIRST_FAST_REFRESH_ENABLED) {
      pDimmInfo->FirstFastRefresh = FIRST_FAST_REFRESH_ENABLED;
    } else {
      pDimmInfo->FirstFastRefresh = FIRST_FAST_REFRESH_DISABLED;
    }
    pDimmInfo->ViralPolicyEnable = OptionalDataPolicyPayload.ViralPolicyEnable;
    pDimmInfo->ViralStatus = OptionalDataPolicyPayload.ViralStatus;
  }


  if (dimmInfoCategories & DIMM_INFO_CATEGORY_OVERWRITE_DIMM_STATUS)
  {
    ReturnCode = GetOverwriteDimmStatus(pDimm, &pDimmInfo->OverwriteDimmStatus);
    if (EFI_ERROR(ReturnCode)) {
      pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_OVERWRITE_STATUS;
    }
  }

#ifdef OS_BUILD
   GetDimmMappedMemSize(pDimm);
#endif // OS_BUILD

  // Data already in pDimm
  pDimmInfo->Configured = pDimm->Configured;
  ReturnCode = GetCapacities(pDimm->DimmID, &pDimmInfo->VolatileCapacity, &pDimmInfo->AppDirectCapacity,
    &pDimmInfo->UnconfiguredCapacity, &pDimmInfo->ReservedCapacity, &pDimmInfo->InaccessibleCapacity);
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
      pDimmInfo->FWImageMaxSize = pPayloadFwImage->FWImageMaxSize * 4096;
    }
  }

  if (dimmInfoCategories & DIMM_INFO_CATEGORY_MEM_INFO_PAGE_3)
  {
      ReturnCode = FwCmdGetMemoryInfoPage(pDimm, MEMORY_INFO_PAGE_3, sizeof(PT_OUTPUT_PAYLOAD_MEMORY_INFO_PAGE3), (VOID **)&pPayloadMemInfoPage3);
      if (EFI_ERROR(ReturnCode)) {
        pDimmInfo->ErrorMask |= DIMM_INFO_ERROR_MEM_INFO_PAGE;
      }
      else {
        pDimmInfo->ErrorInjectionEnabled = pPayloadMemInfoPage3->ErrorInjectStatus & ERR_INJECTION_ENABLED_BIT;
        pDimmInfo->MediaTemperatureInjectionEnabled = pPayloadMemInfoPage3->ErrorInjectStatus & ERR_INJECTION_MEDIA_TEMP_ENABLED_BIT;
        pDimmInfo->SoftwareTriggersEnabled = pPayloadMemInfoPage3->ErrorInjectStatus & ERR_INJECTION_SW_TRIGGER_ENABLED_BIT;
        pDimmInfo->PoisonErrorInjectionsCounter = pPayloadMemInfoPage3->PoisonErrorInjectionsCounter;
        pDimmInfo->PoisonErrorClearCounter = pPayloadMemInfoPage3->PoisonErrorClearCounter;
        pDimmInfo->MediaTemperatureInjectionsCounter = pPayloadMemInfoPage3->MediaTemperatureInjectionsCounter;
        pDimmInfo->SoftwareTriggersCounter = pPayloadMemInfoPage3->SoftwareTriggersCounter;
        pDimmInfo->SoftwareTriggersEnabledDetails = pPayloadMemInfoPage3->SoftwareTriggersEnabledDetails;
      }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPayloadMemInfoPage3);
  FREE_POOL_SAFE(pPayloadFwImage);
  FREE_POOL_SAFE(pGetPackageSparingPayload);
  FREE_POOL_SAFE(pSecurityPayload);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check if there is at least one DIMM on specified socket

  @param[in] SocketId

  @retval FALSE SocketId is not valid, it's out of allowed range or
    there was a problem with getting DIMM list
**/
STATIC
BOOLEAN
IsSocketIdValid(
  IN     UINT16 SocketId
  )
{
//todo: verify we can remove this hard-coded return code (story in backlog to investigate)
#if OS_BUILD
	return TRUE;
#else
  NVDIMM_ENTRY();
  BOOLEAN SocketIdValid = FALSE;
  UINT32 Index = 0;
  UINT32 DimmCount = 0;
  DIMM *pDimm = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  // socket id must be within allowed range
  if (SocketId > MAX_SOCKETS) {
    goto Finish;
  }

  ReturnCode = GetListSize(&gNvmDimmData->PMEMDev.Dimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed on DimmListSize");
    goto Finish;
  }
  // at least one dimm must be plugged to specified socket
  for (Index = 0; Index < DimmCount; Index++) {
    pDimm = GetDimmByIndex(Index, &gNvmDimmData->PMEMDev);
    if (pDimm != NULL && pDimm->SocketId == SocketId) {
      SocketIdValid = TRUE;
    }
  }

Finish:
  NVDIMM_EXIT();

  return SocketIdValid;
#endif
}

/**
  Check if security option is supported

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
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
  }

Finish:
  FREE_HII_POINTER(SystemCapabilitiesInfo.PtrInterleaveFormatsSupported);
  NVDIMM_EXIT();
  return Result;
}
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
VerifyTargetDimms (
  IN     UINT16 DimmIds[]      OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT16 SocketIds[]    OPTIONAL,
  IN     UINT32 SocketIdsCount,
  IN     BOOLEAN UninitializedDimms,
     OUT DIMM *pDimms[MAX_DIMMS],
     OUT UINT32 *pDimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS LocalReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  LIST_ENTRY *pDimmList = NULL;
  UINT32 PlatformDimmsCount = 0;
  DIMM *pCurrentDimm = NULL;
  UINT16 Index = 0;
  UINT16 Index2 = 0;
  BOOLEAN Found = FALSE;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pDimmsNum == NULL || pCommandStatus == NULL) {

    goto Finish;
  }

  if (UninitializedDimms && SocketIdsCount > 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);

    goto Finish;
  }

  *pDimmsNum = 0;

  if (UninitializedDimms) {
    pDimmList = &gNvmDimmData->PMEMDev.UninitializedDimms;
  } else {
    pDimmList = &gNvmDimmData->PMEMDev.Dimms;
  }

  // get system DIMMs count
  LocalReturnCode = GetListSize(pDimmList, &PlatformDimmsCount);
  if (EFI_ERROR(LocalReturnCode)) {
    NVDIMM_DBG("Failed on DimmListSize");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }
  if (DimmIdsCount > PlatformDimmsCount) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_DIMM_NOT_FOUND);
    goto Finish;
  }

  /** if dimms and sockets are not specified, then take all dimms **/
  if (SocketIdsCount == 0 && DimmIdsCount == 0) {
    LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
      pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
      if (pCurrentDimm == NULL) {
        NVDIMM_DBG("Failed on Get Dimm from node %d", *pDimmsNum);
        ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
        goto Finish;
      }

      if (IsDimmManageable(pCurrentDimm) || UninitializedDimms) {
        pDimms[(*pDimmsNum)] = pCurrentDimm;
        (*pDimmsNum)++;
      }
    }
  } else {
    if (SocketIdsCount > 0) {
      if (SocketIds == NULL) {
        goto Finish;
      }
      // verify the sockets first
      for (Index = 0; Index < SocketIdsCount; Index++) {
        if (!IsSocketIdValid(SocketIds[Index])) {
          ResetCmdStatus(pCommandStatus, NVM_ERR_SOCKET_ID_NOT_VALID);
          goto Finish;
        }
        // check for duplicate entries
        for (Index2 = 0; Index2 < SocketIdsCount; Index2++) {
          if (Index == Index2) {
            continue;
          }
          if (SocketIds[Index] == SocketIds[Index2]) {
            ResetCmdStatus(pCommandStatus, NVM_ERR_SOCKET_ID_DUPLICATED);
            goto Finish;
          }
        }
      }
    }

    if (DimmIdsCount > 0) {
      if (DimmIds == NULL) {
        goto Finish;
      }
      // user specified a list of dimms, check for duplicates
      for (Index = 0; Index < DimmIdsCount; Index++) {
        pCurrentDimm = GetDimmByPid(DimmIds[Index], pDimmList);
        if (pCurrentDimm == NULL) {
          NVDIMM_DBG("Failed on GetDimmByPid. Does DIMM 0x%04x exist?", DimmIds[Index]);
          SetObjStatus(pCommandStatus, DimmIds[Index], NULL, 0, NVM_ERR_DIMM_NOT_FOUND);
          goto Finish;
        }

        for (Index2 = 0; Index2 < DimmIdsCount; Index2++) {
          if (Index == Index2) {
            continue;
          }
          if (DimmIds[Index] == DimmIds[Index2]) {
            SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_DIMM_ID_DUPLICATED);
            goto Finish;
          }
        }
      }
    }

    if (SocketIdsCount > 0 && DimmIdsCount == 0) {
      /** if sockets are specified and dimms are not, then get all Manageable DIMMs from that sockets **/
      LIST_FOR_EACH(pCurrentDimmNode, pDimmList) {
        pCurrentDimm = DIMM_FROM_NODE(pCurrentDimmNode);
        for (Index2 = 0; Index2 < SocketIdsCount; Index2++) {
          if (pCurrentDimm != NULL && pCurrentDimm->SocketId == SocketIds[Index2]) {
            if (IsDimmManageable(pCurrentDimm) || UninitializedDimms) {
              pDimms[(*pDimmsNum)] = pCurrentDimm;
              (*pDimmsNum)++;
            }
          }
        }
      }
    } else if (DimmIdsCount > 0) {
      // check if specified DIMMs exist
      for (Index = 0; Index < DimmIdsCount; Index++) {
        pCurrentDimm = GetDimmByPid(DimmIds[Index], pDimmList);
        if (pCurrentDimm == NULL) {
          NVDIMM_DBG("Failed on GetDimmByPid. Does DIMM 0x%04x exist?", DimmIds[Index]);
          SetObjStatus(pCommandStatus, DimmIds[Index], NULL, 0, NVM_ERR_DIMM_NOT_FOUND);
          goto Finish;
        } else {
          Found = FALSE;
          for (Index2 = 0; Index2 < SocketIdsCount; Index2++) {
            if (pCurrentDimm->SocketId == SocketIds[Index2]) {
              Found = TRUE;
              break;
            }
          }
          if (SocketIdsCount > 0 && !Found) {
            SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_DIMM_NOT_FOUND);
            goto Finish;
          }

          if (IsDimmManageable(pCurrentDimm) || UninitializedDimms) {
            pDimms[(*pDimmsNum)] = pCurrentDimm;
            (*pDimmsNum)++;
          }
        }
      }
    }
  }

  // sanity checks
  if (*pDimmsNum == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  } else if (*pDimmsNum > MAX_DIMMS) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve the list of DCPMEM modules found in NFIT

  @param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] DimmCount The size of pDimms.
  @param[in] dimmInfoCategories DIMM_INFO_CATEGORIES specifies which (if any)
  additional FW api calls is desired. If DIMM_INFO_CATEGORY_NONE, then only
  the properties from the pDimm struct will be populated.
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
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  DIMM *pDimm = NULL;
  UINT32 ListSize = 0;

  NVDIMM_ENTRY();

  /* check input parameters */
  if (pThis == NULL || pDimms == NULL) {
    NVDIMM_DBG("pDimms is NULL");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  GetListSize(&gNvmDimmData->PMEMDev.Dimms, &ListSize);
  if (DimmCount > ListSize) {
    NVDIMM_DBG("Requested more DIMM's than available.");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  SetMem(pDimms, sizeof(*pDimms) * DimmCount, 0); // this clears error mask as well

  for (Index = 0; Index < ListSize; Index++) {
    if (DimmCount <= Index) {
      NVDIMM_DBG("Array is too small to hold entire DIMM list");
      Rc = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    pDimm = GetDimmByIndex(Index, &gNvmDimmData->PMEMDev);
    if (pDimm == NULL) {
      NVDIMM_DBG("Failed to retrieve the CR-DIMM index %d", Index);
      Rc = EFI_NOT_FOUND;
      goto Finish;
    }
    GetDimmInfo(pDimm, dimmInfoCategories, &pDimms[Index]);
  }

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}


/**
  Retrieve the list of uninitialized DCPMEM modules found in NFIT and partially
  populated thru SMBUS

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

  SetMem(pDimms, sizeof(*pDimms) * DimmCount, 0); // this clears error mask as well

  Index = 0;
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.UninitializedDimms) {
    if (DimmCount <= Index) {
      NVDIMM_DBG("Array is too small to hold entire Smbus DIMM list");
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    pCurDimm = DIMM_FROM_NODE(pNode);
    InitializeNfitDimmInfoFieldsFromDimm(pCurDimm, &(pDimms[Index]));

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
    NVDIMM_DBG("Failed to retrieve the CR-DIMM pid %x", Pid);
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
    NVDIMM_DBG("Failed to retrieve the CR-DIMM pid %x", Pid);
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
    NVDIMM_DBG("Failed to retrieve the CR-DIMM pid %x", Pid);
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  ParsedPcatHeader *pPcat = NULL;
  UINT32 Index = 0;
  UINT32 SocketCount = 0;

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

  if (SocketCount == 0 || pPcat->ppSocketSkuInfoTable == NULL) {
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
    if (pPcat->ppSocketSkuInfoTable[Index] == NULL) {
      ReturnCode = EFI_DEVICE_ERROR;
      goto FinishError;
    }
    (*ppSockets)[Index].SocketId = pPcat->ppSocketSkuInfoTable[Index]->SocketId;
    (*ppSockets)[Index].MappedMemoryLimit = pPcat->ppSocketSkuInfoTable[Index]->MappedMemorySizeLimit;
    (*ppSockets)[Index].TotalMappedMemory = pPcat->ppSocketSkuInfoTable[Index]->TotalMemorySizeMappedToSpa;
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  UINT8 SystemSecurityState = 0;
  UINT8 DimmSecurityState = 0;
  BOOLEAN IsMixed = FALSE;

  NVDIMM_ENTRY();

  SetMem(pDimms, sizeof(pDimms), 0x0);

  if (pThis == NULL || pSecurityState == NULL || pDimmIds == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsNum, pCommandStatus);
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
        SetObjStatusForDimm(pCommandStatus, pDimms[Index],NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
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
  BOOLEAN AppDirectIndexFound = FALSE;
  UINT32 SequenceIndex = 0;
  struct {
    REGION_GOAL *pRegionGoal;
    UINT32 AppDirectIndex;
  } NumberedGoals[MAX_IS_PER_DIMM * MAX_DIMMS];
  MEMORY_MODE AllowedMode = MEMORY_MODE_1LM;

  SetMem(pDimms, sizeof(pDimms), 0x0);
  SetMem(&NumberedGoals, sizeof(NumberedGoals), 0x0);

  if (pConfigGoalsCount == NULL || pConfigGoals == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_ERR("Some of required pointers are null");
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    NVDIMM_ERR("ERROR: DimmSkuConsistency");
    goto Finish;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, FALSE, pDimms, &DimmsCount,
      pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    NVDIMM_ERR("ERROR: VerifyTargetDimms");
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_VOLUME_CORRUPTED == ReturnCode) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_PCD_BAD_DEVICE_CONFIG);
    }
    NVDIMM_ERR("ERROR: RetrieveGoalConfigsFromPlatformConfigData");
    goto Finish;
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

	  NVDIMM_DBG("dimm idx %d", Index1);
	  NVDIMM_DBG("dimm sockid %x", pCurrentDimm->SocketId);
	  NVDIMM_DBG("dimm vsize %x", pCurrentDimm->VolatileSizeGoal);
	  NVDIMM_DBG("dimm goal %x", pCurrentDimm->RegionsGoalNum);

    for (Index2 = 0; Index2 < pCurrentDimm->RegionsGoalNum; ++Index2) {
      SequenceIndex = pCurrentDimm->pRegionsGoal[Index2]->SequenceIndex;
	    NVDIMM_DBG("region loop %d, region goal size %x, dimmsnum %x", pCurrentDimm->pRegionsGoal[Index2]->Size, pCurrentDimm->pRegionsGoal[Index2]->DimmsNum);
      pCurrentGoal->NumberOfInterleavedDimms[SequenceIndex] = (UINT8)pCurrentDimm->pRegionsGoal[Index2]->DimmsNum;
      pCurrentGoal->AppDirectSize[SequenceIndex] =
          pCurrentDimm->pRegionsGoal[Index2]->Size / pCurrentDimm->pRegionsGoal[Index2]->DimmsNum;
      pCurrentGoal->InterleaveSetType[SequenceIndex] = pCurrentDimm->pRegionsGoal[Index2]->InterleaveSetType;
      pCurrentGoal->ImcInterleaving[SequenceIndex] = pCurrentDimm->pRegionsGoal[Index2]->ImcInterleaving;
      pCurrentGoal->ChannelInterleaving[SequenceIndex] = pCurrentDimm->pRegionsGoal[Index2]->ChannelInterleaving;

      /**
        Fill array with goal indices or retrieve index from it
      **/
      AppDirectIndexFound = FALSE;
      for (Index3 = 0; Index3 < NumberedGoalsNum; Index3++) {
		    NVDIMM_DBG("appdir loop %d", Index3);
        if (NumberedGoals[Index3].pRegionGoal == pCurrentDimm->pRegionsGoal[Index2]) {
			    NVDIMM_DBG("appdir found!");
          pCurrentGoal->AppDirectIndex[SequenceIndex] = (UINT8)NumberedGoals[Index3].AppDirectIndex;
          AppDirectIndexFound = TRUE;
          break;
        }
      }
      if (!AppDirectIndexFound) {
        pCurrentGoal->AppDirectIndex[SequenceIndex] = (UINT8)AppDirectIndex;
        NumberedGoals[NumberedGoalsNum].pRegionGoal = pCurrentDimm->pRegionsGoal[Index2];
        NumberedGoals[NumberedGoalsNum].AppDirectIndex = AppDirectIndex;
        NumberedGoalsNum++;
        AppDirectIndex++;
      }
    }

    pCurrentGoal->StorageCapacity = pCurrentDimm->RawCapacity -
      (pCurrentGoal->VolatileSize + pCurrentGoal->AppDirectSize[0] + pCurrentGoal->AppDirectSize[1]);

    pCurrentGoal->Status = pCurrentDimm->GoalConfigStatus;

    ConfigRegionCount++;
  }

  /** Warning when 2LM mode is off (1LM is selected in the BIOS setup) **/
  ReturnCode = AllowedMemoryMode(&AllowedMode);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  if (AllowedMode != MEMORY_MODE_2LM) {
    ResetCmdStatus(pCommandStatus, NVM_WARN_2LM_MODE_OFF);
  }

  *pConfigGoalsCount = ConfigRegionCount;
Finish:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
  return ReturnCode;
}

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

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    SetObjStatus(pCommandStatus, DimmPid, NULL, 0, NVM_ERR_DIMM_NOT_FOUND);
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    ReturnCode = EFI_UNSUPPORTED;
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  PT_PAYLOAD_ALARM_THRESHOLDS *pPayloadAlarmThresholds = NULL;
  UINT32 Index = 0;
#ifdef OS_BUILD
  LIST_ENTRY *pObjectStatusNode = NULL;
  OBJECT_STATUS *pObjectStatus = NULL;
#endif // OS_BUILD

  NVDIMM_ENTRY();

  SetMem(pDimms, sizeof(pDimms), 0x0);

  if (pCommandStatus == NULL) {
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;

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

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (NonCriticalThreshold == THRESHOLD_UNDEFINED && EnabledState == ENABLED_STATE_UNDEFINED) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
    if ((SensorId == SENSOR_TYPE_CONTROLLER_TEMPERATURE) &&
        !IS_IN_RANGE(NonCriticalThreshold, TEMPERATURE_THRESHOLD_MIN, TEMPERATURE_CONTROLLER_THRESHOLD_MAX)) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SENSOR_CONTROLLER_TEMP_OUT_OF_RANGE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    if ((SensorId == SENSOR_TYPE_MEDIA_TEMPERATURE) &&
        !IS_IN_RANGE(NonCriticalThreshold, TEMPERATURE_THRESHOLD_MIN, TEMPERATURE_MEDIA_THRESHOLD_MAX)) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SENSOR_MEDIA_TEMP_OUT_OF_RANGE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    if ((SensorId == SENSOR_TYPE_PERCENTAGE_REMAINING) &&
        !IS_IN_RANGE(NonCriticalThreshold, CAPACITY_THRESHOLD_MIN, CAPACITY_THRESHOLD_MAX)) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SENSOR_CAPACITY_OUT_OF_RANGE);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  for ((Index = 0)
#ifdef OS_BUILD
    ,(pObjectStatusNode = (&pCommandStatus->ObjectStatusList)->ForwardLink)
#endif // OS_BUILD
    ; Index < DimmsNum;
    (Index++)
#ifdef OS_BUILD
    ,(pObjectStatusNode = pObjectStatusNode->ForwardLink)
#endif // OS_BUILD
    ) {
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

    if (SensorId == SENSOR_TYPE_CONTROLLER_TEMPERATURE) {
      if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
        pPayloadAlarmThresholds->ControllerTemperatureThreshold = TransformRealValueToFwTemp(NonCriticalThreshold);
        pPayloadAlarmThresholds->Enable.Separated.ControllerTemperature = TRUE;
      }
      if (EnabledState != ENABLED_STATE_UNDEFINED) {
        pPayloadAlarmThresholds->Enable.Separated.ControllerTemperature = EnabledState;
      }
    }
    if (SensorId == SENSOR_TYPE_MEDIA_TEMPERATURE) {
      if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
        pPayloadAlarmThresholds->MediaTemperatureThreshold = TransformRealValueToFwTemp(NonCriticalThreshold);
        pPayloadAlarmThresholds->Enable.Separated.MediaTemperature = TRUE;
      }
      if (EnabledState != ENABLED_STATE_UNDEFINED) {
        pPayloadAlarmThresholds->Enable.Separated.MediaTemperature = EnabledState;
      }
    }
    if (SensorId == SENSOR_TYPE_PERCENTAGE_REMAINING) {
      if (NonCriticalThreshold != THRESHOLD_UNDEFINED) {
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
#ifdef OS_BUILD
      pObjectStatus = OBJECT_STATUS_FROM_NODE(pObjectStatusNode);
      CHAR16 *pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_SENSOR_SET_CHANGED), NULL);
      nvm_store_system_entry_widechar(NVM_SYSLOG_SRC_W,
        SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, EVENT_CONFIG_CHANGE_305, FALSE, TRUE, TRUE, FALSE, 0),
        pObjectStatus->ObjectIdStr, pTmpStr, ConvertSensorIdToStringW(SensorId), pDimms[Index]->DeviceHandle.AsUint32);
      FREE_POOL_SAFE(pTmpStr);
#endif // OS_BUILD
    }
  }

Finish:
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
  * Last shutdown status
  * Dirty shutdowns
  * Last shutdown time.
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  DIMM *pDimm = NULL;
  PT_PAYLOAD_SMART_AND_HEALTH *pPayloadSmartAndHealth = NULL;
  PT_DEVICE_CHARACTERISTICS_PAYLOAD *pDevCharacteristics = NULL;
  BOOLEAN FIS_1_3 = FALSE;

  NVDIMM_ENTRY();

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL || !IsDimmManageable(pDimm) || pSensorInfo == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // @todo Remove FIS 1.3 backwards compatibility workaround
  if (pDimm->FwVer.FwApiMajor == 1 && pDimm->FwVer.FwApiMinor <= 3) {
    FIS_1_3 = TRUE;
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
  pSensorInfo->SpareBlocksValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.PercentageRemaining;
  pSensorInfo->MediaTemperatureValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.MediaTemperature;
  pSensorInfo->ControllerTemperatureValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.ControllerTemperature;
  pSensorInfo->PercentageUsedValid = (BOOLEAN) pPayloadSmartAndHealth->ValidationFlags.Separated.PercentageUsed;
  pSensorInfo->MediaTemperature = TransformFwTempToRealValue(pPayloadSmartAndHealth->MediaTemperature);
  pSensorInfo->HealthStatus = pPayloadSmartAndHealth->HealthStatus;
  pSensorInfo->HealthStatusReason = (pPayloadSmartAndHealth->ValidationFlags.Separated.HealthStatusReason) ?
         pPayloadSmartAndHealth->HealthStatusReason : (UINT16)HEALTH_STATUS_REASON_NONE;
  pSensorInfo->PercentageRemaining = pPayloadSmartAndHealth->PercentageRemaining;
  pSensorInfo->PercentageUsed = pPayloadSmartAndHealth->PercentageUsed;
  pSensorInfo->LastShutdownStatus = pPayloadSmartAndHealth->LastShutdownStatus;
  /** Get Vendor specific data **/
  pSensorInfo->ControllerTemperature = TransformFwTempToRealValue(pPayloadSmartAndHealth->ControllerTemperature);
  pSensorInfo->UpTime = (UINT32)pPayloadSmartAndHealth->VendorSpecificData.UpTime;
  pSensorInfo->PowerCycles = pPayloadSmartAndHealth->VendorSpecificData.PowerCycles;
  pSensorInfo->PowerOnTime = (UINT32)pPayloadSmartAndHealth->VendorSpecificData.PowerOnTime;
  pSensorInfo->DirtyShutdowns = pPayloadSmartAndHealth->VendorSpecificData.DirtyShutdowns;
  /** Get Device Characteristics data **/
  pSensorInfo->ContrTempShutdownThresh =
      TransformFwTempToRealValue(pDevCharacteristics->ControllerShutdownThreshold);
  pSensorInfo->MediaTempShutdownThresh =
      TransformFwTempToRealValue(pDevCharacteristics->MediaShutdownThreshold);
  pSensorInfo->MediaThrottlingStartThresh =
      TransformFwTempToRealValue(pDevCharacteristics->MediaThrottlingStartThreshold);
  pSensorInfo->MediaThrottlingStopThresh =
      TransformFwTempToRealValue(pDevCharacteristics->MediaThrottlingStopThreshold);
  /** Check triggered alarms **/
  pSensorInfo->MediaTemperatureTrip = (pPayloadSmartAndHealth->AlarmTrips.Separated.MediaTemperature != 0);
  pSensorInfo->ControllerTemperatureTrip = (pPayloadSmartAndHealth->AlarmTrips.Separated.ControllerTemperature != 0);
  pSensorInfo->PercentageRemainingTrip = (pPayloadSmartAndHealth->AlarmTrips.Separated.PercentageRemaining != 0);

  if (pLastShutdownStatus != NULL) {
    /** Copy extended detail bits **/
    CopyMem(pLastShutdownStatus, pPayloadSmartAndHealth->VendorSpecificData.LastShutdownExtendedDetails.Raw, sizeof(LAST_SHUTDOWN_STATUS_EXTENDED));
    /** Shift extended over, add the original 8 bits **/
    *pLastShutdownStatus = (*pLastShutdownStatus << sizeof(LAST_SHUTDOWN_STATUS) * 8)
                         + pPayloadSmartAndHealth->VendorSpecificData.LastShutdownDetails.AllFlags;
  }

  if (pLastShutdownTime != NULL) {
    *pLastShutdownTime = pPayloadSmartAndHealth->VendorSpecificData.LastShutdownTime;
  }

  if (pAitDramEnabled != NULL) {
    *pAitDramEnabled = pPayloadSmartAndHealth->AITDRAMStatus;

    if (!FIS_1_3) {
      if ((pPayloadSmartAndHealth->ValidationFlags.Separated.AITDRAMStatus == 0) &&
          (pPayloadSmartAndHealth->HealthStatus < ControllerHealthStatusCritical)) {
        *pAitDramEnabled = AIT_DRAM_ENABLED;
      }
    }
  }

  ReturnCode = FwCmdGetErrorCount(pDimm, &pSensorInfo->MediaErrorCount, &pSensorInfo->ThermalErrorCount);
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
  )
{
  EFI_STATUS ReturnCode = EFI_UNSUPPORTED;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT16 PayloadBufferSize = 0;
  UINT8 SubOpcode = 0;
  CHAR8 AsciiPassword[PASSPHRASE_BUFFER_SIZE + 1];
  UINT32 Index = 0;
  UINT8 DimmSecurityState = 0;
  PT_SET_SECURITY_PAYLOAD *pSecurityPayload = NULL;
  BOOLEAN NamespaceFound = FALSE;
  BOOLEAN AreNotPartOfPendingGoal = TRUE;
  BOOLEAN IsSupported = FALSE;
  DIMM *pCurrentDimm = NULL;
  LIST_ENTRY *pCurrentDimmNode = NULL;
  LIST_ENTRY *pDimmList = NULL;

  NVDIMM_ENTRY();

  IsSupported = IsSecurityOpSupported(SecurityOperation);
  if (!IsSupported) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED);
    goto Finish;
  }

  SetMem(pDimms, sizeof(pDimms), 0x0);
  SetMem(AsciiPassword, sizeof(AsciiPassword), 0x0);

  if (pCommandStatus == NULL) {
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;

  if (SecurityOperation != SECURITY_OPERATION_SET_PASSPHRASE &&
      SecurityOperation != SECURITY_OPERATION_CHANGE_PASSPHRASE &&
      SecurityOperation != SECURITY_OPERATION_DISABLE_PASSPHRASE &&
      SecurityOperation != SECURITY_OPERATION_UNLOCK_DEVICE &&
      SecurityOperation != SECURITY_OPERATION_ERASE_DEVICE &&
      SecurityOperation != SECURITY_OPERATION_FREEZE_DEVICE) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_SECURITY_OPERATION);
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // Prevent user from enabling security when goal is pending due to BIOS restrictions
  if (SecurityOperation == SECURITY_OPERATION_SET_PASSPHRASE) {
    // Check if input DIMMs are not part of a goal
    ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms);
    if (EFI_ERROR(ReturnCode)) {
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
    CopyMem(&pSecurityPayload->PassphraseCurrent, AsciiPassword, AsciiStrLen(AsciiPassword));
  }
  if (pNewPassphrase != NULL) {
    UnicodeStrToAsciiStrS(pNewPassphrase, AsciiPassword, PASSPHRASE_BUFFER_SIZE + 1);
    CopyMem(&pSecurityPayload->PassphraseNew, AsciiPassword, AsciiStrLen(AsciiPassword));
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

    if (DimmSecurityState & SECURITY_MASK_COUNTEXPIRED) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_COUNT_EXPIRED);
      ReturnCode = EFI_ABORTED;
      goto Finish;
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
      if (!(DimmSecurityState & SECURITY_MASK_LOCKED) &&
          !(DimmSecurityState & SECURITY_MASK_FROZEN)) {
        SubOpcode = SubopSecEraseUnit;
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
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

    } else {
      ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_SECURITY_OPERATION);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }

    ReturnCode = SetDimmSecurityState(pDimms[Index], PtSetSecInfo, SubOpcode, PayloadBufferSize,
        pSecurityPayload, PT_TIMEOUT_INTERVAL);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on SetDimmSecurityState, ReturnCode=%r", ReturnCode);
      if (ReturnCode == EFI_ACCESS_DENIED) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_PASSPHRASE);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
      }
      ReturnCode = EFI_DEVICE_ERROR;
      goto Finish;
    }
#ifndef OS_BUILD
    /** Need to call WBINV after unlock or secure erase **/
    if (SecurityOperation == SECURITY_OPERATION_ERASE_DEVICE ||
        SecurityOperation == SECURITY_OPERATION_UNLOCK_DEVICE) {
	AsmWbinvd();
    }
#endif
    /** @todo(check on real HW)
      WARNING: SetDimmSecurityState will not return EFI_ERROR on SECURITY_MASK_COUNTEXPIRED
      so we have to check it additionaly with GetDimmSecurityState.
      It could be Simics/FW bug. Check how it is done on real HW.
    **/
    ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed on GetDimmSecurityState, ReturnCode=%r", ReturnCode);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    } else if (DimmSecurityState & SECURITY_MASK_COUNTEXPIRED) {
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_SECURITY_COUNT_EXPIRED);
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);

    pDimms[Index]->EncryptionEnabled = ((DimmSecurityState & SECURITY_MASK_ENABLED) != 0);
  }

  if (!EFI_ERROR(ReturnCode)) {
    SetCmdStatus(pCommandStatus, NVM_SUCCESS);
  }

Finish:
  if (SecurityOperation == SECURITY_OPERATION_UNLOCK_DEVICE) {
    ReturnCode = ReenumerateNamespacesAndISs();
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to re-enumerate namespace on unlocked DIMMs. ReturnCode=%r", ReturnCode);
    }
  }

  CleanStringMemory(AsciiPassword);
  FREE_POOL_SAFE(pSecurityPayload);
  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifndef OS_BUILD
#ifndef MDEPKG_NDEBUG
  SMBIOS_STRUCTURE_POINTER DmiPhysicalDev;
  SMBIOS_STRUCTURE_POINTER DmiDeviceMappedAddr;
  SMBIOS_VERSION SmbiosVersion;
  DIMM *pDimm = NULL;
#endif
  NVDIMM_ENTRY();

#ifdef MDEPKG_NDEBUG
  ReturnCode = EFI_UNSUPPORTED;
#else
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
#endif
  NVDIMM_EXIT_I64(ReturnCode);
#endif
  return ReturnCode;
}

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

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[out] ppPmtt output buffer with PMTT tables

@retval EFI_SUCCESS Ok
@retval EFI_OUT_OF_RESOURCES Problem with allocating memory
@retval EFI_NOT_FOUND PCAT tables not found
**/
EFI_STATUS
EFIAPI
GetAcpiPMTT(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  OUT PMTT_TABLE **ppPMTTtbl
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (ppPMTTtbl == NULL) {
    goto Finish;
  }

  if (gNvmDimmData->PMEMDev.pPMTTTble != NULL) {
    *ppPMTTtbl = gNvmDimmData->PMEMDev.pPMTTTble;
  ReturnCode = EFI_SUCCESS;
  } else {
    NVDIMM_DBG("PMTT does not exist");
    ReturnCode = EFI_NOT_FOUND;
  }

Finish:
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsCount = 0;
  UINT32 Index = 0;
  NVDIMM_CONFIGURATION_HEADER *pPcdConfHeader = NULL;
  LABEL_STORAGE_AREA *pLabelStorageArea = NULL;

  NVDIMM_ENTRY();

  ZeroMem(pDimms, sizeof(pDimms));

  if (pThis == NULL || ppDimmPcdInfo == NULL || pDimmPcdInfoCount == NULL || pCommandStatus == NULL) {
    goto FinishError;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsCount,
      pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto FinishError;
  }

  *ppDimmPcdInfo = AllocateZeroPool(sizeof(**ppDimmPcdInfo) * DimmsCount);
  if (*ppDimmPcdInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto FinishError;
  }

  for (Index = 0; Index < DimmsCount; Index++) {
    pPcdConfHeader = NULL;
    pLabelStorageArea = NULL;
    (*ppDimmPcdInfo)[Index].DimmId = pDimms[Index]->DeviceHandle.AsUint32;

    ReturnCode = GetDimmUid(pDimms[Index], (*ppDimmPcdInfo)[Index].DimmUid, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto FinishError;
    }
	if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_CONFIG) {
		ReturnCode = GetPlatformConfigDataOemPartition(pDimms[Index], &pPcdConfHeader);

		if (ReturnCode == EFI_NO_MEDIA) {
		  continue;
		}
#ifdef MEMORY_CORRUPTION_WA
		if (ReturnCode == EFI_DEVICE_ERROR)
		{
			ReturnCode = GetPlatformConfigDataOemPartition(pDimms[Index], &pPcdConfHeader);
		}
#endif // MEMORY_CORRUPTIO_WA
		if (EFI_ERROR(ReturnCode)) {
			NVDIMM_DBG("GetPlatformConfigDataOemPartition returned: %r", ReturnCode);
			SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_GET_PCD_FAILED);
			goto FinishError;
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
			NVDIMM_DBG("Error in retrieving the LSA: %r", ReturnCode);
			goto Finish;
		}
		pDimms[Index]->LsaStatus = LSA_OK;
#else // OS_BUILD
		if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_ACCESS_DENIED) {
			NVDIMM_DBG("Error in retrieving the LSA: %r", ReturnCode);
			goto Finish;
		}
#endif // OS_BUILD
		(*ppDimmPcdInfo)[Index].pLabelStorageArea = pLabelStorageArea;
	}
  }

  *pDimmPcdInfoCount = DimmsCount;

  ReturnCode = EFI_SUCCESS;
  goto Finish;

FinishError:
  if (ppDimmPcdInfo != NULL) {
    FreeDimmPcdInfoArray(*ppDimmPcdInfo, DimmsCount);
    *ppDimmPcdInfo = NULL;
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS TmpReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsCount = 0;
  UINT32 Index = 0;
  UINT8 SecurityState = 0;

  NVDIMM_ENTRY();

  ZeroMem(pDimms, sizeof(pDimms));

  if (pThis == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsCount,
      pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsCount; Index++) {

    TmpReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &SecurityState);
    if (EFI_ERROR(TmpReturnCode)) {
      KEEP_ERROR(ReturnCode, TmpReturnCode);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
      NVDIMM_DBG("Failed to get DIMM security state");
      goto Finish;
    }

    if (!IsConfiguringAllowed(SecurityState)) {
      KEEP_ERROR(ReturnCode, EFI_ACCESS_DENIED);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      NVDIMM_DBG("Locked DIMM discovered : 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
      continue;
    }

    TmpReturnCode = ZeroLabelStorageArea(pDimms[Index]->DimmID);
    if (EFI_ERROR(TmpReturnCode)) {
      KEEP_ERROR(ReturnCode, TmpReturnCode);
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
      NVDIMM_DBG("Error in zero-ing the LSA: %r", TmpReturnCode);
      continue;
    }

    SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
    NVDIMM_DBG("Zero'ed the LSA on DIMM : 0x%x", pDimms[Index]->DeviceHandle.AsUint32);
  }

  ReenumerateNamespacesAndISs();

Finish:
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

  pRegionMin->RegionId = pRegion->InterleaveSetIndex;
  pRegionMin->SocketId = pRegion->SocketId;
  DetermineRegionType(pRegion, &pRegionMin->RegionType);
  pRegionMin->Capacity = pRegion->Size;
  pRegionMin->CookieId = pRegion->InterleaveSetCookie;

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
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  LIST_ENTRY *pRegionNode = NULL;
  LIST_ENTRY *pRegionList = GetRegionList();

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

  @retval int retruns 0,-1, 0
**/
INT32 SortRegionInfoById(VOID *pRegion1, VOID *pRegion2)
{
  REGION_INFO *pRegiona = (REGION_INFO *)pRegion1;
  REGION_INFO *pRegionb = (REGION_INFO *)pRegion2;

  if (pRegiona->RegionId == pRegionb->RegionId) {
    return 0;
  } else if (pRegiona->RegionId < pRegionb->RegionId) {
    return -1;
  } else {
    return 1;
  }
}

/**
  Sorts the DimmIds list by Id

  @param[in out] pDimmId1 A pointer to the pDimmId list.
  @param[in out] pDimmId2 A pointer to the copy of pDimmId list.

  @retval int retruns 0,-1, 0
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
     OUT REGION_INFO *pRegions,
     OUT COMMAND_STATUS *pCommandStatus
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 Index = 0;
  NVM_IS *pCurRegion = NULL;
  LIST_ENTRY *pCurRegionNode = NULL;
  LIST_ENTRY *pRegionList = GetRegionList();

  NVDIMM_ENTRY();

  /**
    check input parameters
  **/
  if (pRegions == NULL || pCommandStatus == NULL) {
    NVDIMM_DBG("Invalid parameter");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    Rc = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

#ifdef OS_BUILD
  Rc = InitializeNamespaces();
  if (EFI_ERROR(Rc)) {
    NVDIMM_WARN("Failed to initialize Namespaces, error = %r.", Rc);
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
    GetRegionMinimalInfo(pCurRegion, &pRegions[Index]);
    Index++;
  }

  BubbleSort(pRegions, Count, sizeof(*pRegions), SortRegionInfoById);

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

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
     OUT REGION_INFO *pRegionInfo,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  NVM_IS *pRegion = NULL;
  LIST_ENTRY *pRegionList = GetRegionList();

  NVDIMM_ENTRY();
  Rc = EFI_SUCCESS;

  if (pRegionInfo == NULL || pCommandStatus == NULL) {
    NVDIMM_DBG("Invalid parameter");
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    Rc = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT64 VolatileCapacity = 0;
  UINT64 AppDirectCapacity = 0;
  UINT64 UnconfiguredCapacity = 0;
  UINT64 ReservedCapacity = 0;
  UINT64 InaccessibleCapacity = 0;

  NVDIMM_ENTRY();
  if (pThis == NULL || pMemoryResourcesInfo == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  /** Make sure we start with zero values **/
  ZeroMem(pMemoryResourcesInfo, sizeof(*pMemoryResourcesInfo));

  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    pMemoryResourcesInfo->RawCapacity += pDimm->RawCapacity;

#ifdef OS_BUILD
	GetDimmMappedMemSize(pDimm);
#endif // OS_BUILD

    ReturnCode = GetCapacities(pDimm->DimmID, &VolatileCapacity, &AppDirectCapacity,
        &UnconfiguredCapacity, &ReservedCapacity, &InaccessibleCapacity);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    pMemoryResourcesInfo->VolatileCapacity += VolatileCapacity;
    pMemoryResourcesInfo->ReservedCapacity += ReservedCapacity;
    pMemoryResourcesInfo->AppDirectCapacity += AppDirectCapacity;
    pMemoryResourcesInfo->InaccessibleCapacity += InaccessibleCapacity;
    pMemoryResourcesInfo->UnconfiguredCapacity += UnconfiguredCapacity;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
    GetListSize(&gNvmDimmData->PMEMDev.Dimms, pDimmCount);
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pNfit == NULL || pPcat == NULL || ppFitHead == NULL || ppPcatHead == NULL) {
    goto Finish;
  }

  *ppFitHead = ParseNfitTable((VOID *)pNfit);
  if (*ppFitHead == NULL) {
    NVDIMM_DBG("NFIT parsing error.");
    ReturnCode = EFI_DEVICE_ERROR;
    goto Finish;
  }

  *ppPcatHead = ParsePcatTable((VOID *)pPcat);
  if (*ppPcatHead == NULL) {
    NVDIMM_DBG("PCAT parsing error.");
    ReturnCode = EFI_DEVICE_ERROR;
    goto Finish;
  }
  if (pPMTT != NULL) {
    *pIsMemoryModeAllowed = CheckIsMemoryModeAllowed((PMTT_TABLE *) pPMTT);
    gNvmDimmData->PMEMDev.pPMTTTble = (PMTT_TABLE *) pPMTT;
  } else {
    // if PMTT table is Not available skip  MM allowed check and let bios handle it
    *pIsMemoryModeAllowed = TRUE;
    gNvmDimmData->PMEMDev.pPMTTTble = (PMTT_TABLE *) pPMTT;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

/**
  Fetch the NFIT and PCAT tables from EFI_SYSTEM_TABLE

  @param[in] pSystemTable is a pointer to the EFI_SYSTEM_TABLE instance
  @param[out] ppDsdt is a pointer to EFI_ACPI_DESCRIPTION_HEADER (NFIT)
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

  if (pSystemTable == NULL || ppNfit == NULL || ppPcat == NULL || ppPMTT == NULL) {
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

  *ppNfit = NULL;
  *ppPcat = NULL;
  *ppPMTT = NULL;

  /**
    Find the Fixed ACPI Description Table (FADT)
  **/
  NVDIMM_DBG("Looking for the Fixed ACPI Description Table (FADT)");
  for (Index = sizeof (EFI_ACPI_DESCRIPTION_HEADER); Index < pRsdt->Length; Index += sizeof(UINT64)) {
    Tmp = *(UINT64 *) ((UINT8 *) pRsdt + Index);
    pCurrentTable = (EFI_ACPI_DESCRIPTION_HEADER *) (UINT64 *) (UINTN) Tmp;

    if (pCurrentTable->Signature == NFIT_TABLE_SIG) {
      NVDIMM_DBG("Found the NFIT table");
      *ppNfit = pCurrentTable;
    }

    if (pCurrentTable->Signature == PCAT_TABLE_SIG) {
      NVDIMM_DBG("Found the PCAT table");
      *ppPcat = pCurrentTable;
    }

    if (pCurrentTable->Signature == PMTT_TABLE_SIG) {
      NVDIMM_DBG("Found the PMTT table");
      *ppPMTT = pCurrentTable;
    }

    if (*ppNfit != NULL && *ppPcat != NULL && *ppPMTT != NULL) {
      break;
    }
  }

  /**
    Failed to find the at least one of the tables
  **/
  if (*ppNfit == NULL || *ppPcat == NULL) {
    NVDIMM_WARN("Unable to find the NFIT or PCAT table.");
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
  @retval EFI_BAD_BUFFER_SIZE if the nfit spa memory is more than the one in memmmap
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
    if (CompareMem(pTableSpaRange->AddressRangeTypeGuid,
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

  NVDIMM_ENTRY();

  /**
    Find the Differentiated System Description Table (DSDT) from EFI_SYSTEM_TABLE
  **/
  ReturnCode = GetAcpiTables(gST, &pNfit, &pPcat, &pPMTT);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to get NFIT or PCAT or PMTT table.");
    goto Finish;
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
  ReturnCode = ParseAcpiTables(pNfit, pPcat, pPMTT, &gNvmDimmData->PMEMDev.pFitHead, &gNvmDimmData->PMEMDev.pPcatHead,
    &gNvmDimmData->PMEMDev.IsMemModeAllowedByBios);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to parse NFIT or PCAT table.");
    goto Finish;
  }
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Capabilities = 0;
  PLATFORM_CAPABILITY_INFO *pPlatformCapability = NULL;
  MEMORY_INTERLEAVE_CAPABILITY_INFO *pInterleaveCapability = NULL;
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

  if (gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum == 1 &&
      gNvmDimmData->PMEMDev.pPcatHead->ppPlatformCapabilityInfo != NULL &&
      gNvmDimmData->PMEMDev.pPcatHead->ppPlatformCapabilityInfo[0] != NULL) {
    pPlatformCapability = gNvmDimmData->PMEMDev.pPcatHead->ppPlatformCapabilityInfo[0];
    pSysCapInfo->OperatingModeSupport = pPlatformCapability->MemoryModeCapabilities.MemoryModes;
    pSysCapInfo->PlatformConfigSupported = pPlatformCapability->MgmtSwConfigInputSupport;
    pSysCapInfo->CurrentOperatingMode = pPlatformCapability->CurrentMemoryMode.MemoryMode;
  } else {
    NVDIMM_DBG("Number of Platform  Capability Information tables: %d",
      gNvmDimmData->PMEMDev.pPcatHead->PlatformCapabilityInfoNum);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }
  if (gNvmDimmData->PMEMDev.pPcatHead->MemoryInterleaveCapabilityInfoNum == 1 &&
      gNvmDimmData->PMEMDev.pPcatHead->ppMemoryInterleaveCapabilityInfo != NULL &&
      gNvmDimmData->PMEMDev.pPcatHead->ppMemoryInterleaveCapabilityInfo[0] != NULL) {
    pInterleaveCapability = gNvmDimmData->PMEMDev.pPcatHead->ppMemoryInterleaveCapabilityInfo[0];
    pSysCapInfo->InterleaveAlignmentSize = pInterleaveCapability->InterleaveAlignmentSize;
    pSysCapInfo->InterleaveFormatsSupportedNum = pInterleaveCapability->NumOfFormatsSupported;
    pSysCapInfo->PtrInterleaveFormatsSupported = (HII_POINTER) AllocateZeroPool(sizeof(INTERLEAVE_FORMAT)
        * pSysCapInfo->InterleaveFormatsSupportedNum);
    if ((INTERLEAVE_FORMAT *) pSysCapInfo->PtrInterleaveFormatsSupported == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    CopyMem((INTERLEAVE_FORMAT *)pSysCapInfo->PtrInterleaveFormatsSupported,
        pInterleaveCapability->InterleaveFormatList,
        sizeof(INTERLEAVE_FORMAT) * pSysCapInfo->InterleaveFormatsSupportedNum);
  }
  if (gNvmDimmData->PMEMDev.pFitHead->PlatformCapabilitiesTblesNum == 1 &&
      gNvmDimmData->PMEMDev.pFitHead->ppPlatformCapabilitiesTbles != NULL &&
      gNvmDimmData->PMEMDev.pFitHead->ppPlatformCapabilitiesTbles[0] != NULL) {
    pNfitPlatformCapability = gNvmDimmData->PMEMDev.pFitHead->ppPlatformCapabilitiesTbles[0];
    Capabilities = pNfitPlatformCapability->Capabilities & NFIT_MEMORY_CONTROLLER_FLUSH_BIT1;
    pSysCapInfo->AdrSupported = (Capabilities == NFIT_MEMORY_CONTROLLER_FLUSH_BIT1);
  }
  for (Index = 0; Index < SUPPORTED_BLOCK_SIZES_COUNT; Index++) {
    CopyMem(&pSysCapInfo->NsBlockSizes[Index], &gSupportedBlockSizes[Index], sizeof(pSysCapInfo->NsBlockSizes[Index]));
  }
  pSysCapInfo->MinNsSize = gNvmDimmData->Alignments.BlockNamespaceMinSize;

  /**
    Platform capabilities
  **/
  pSysCapInfo->AppDirectMirrorSupported = IS_BIT_SET_VAR(pPlatformCapability->PersistentMemoryRasCapability, BIT0);
  pSysCapInfo->DimmSpareSupported = IS_BIT_SET_VAR(pPlatformCapability->PersistentMemoryRasCapability, BIT1);
  pSysCapInfo->AppDirectMigrationSupported = IS_BIT_SET_VAR(pPlatformCapability->PersistentMemoryRasCapability, BIT2);

  /**
    Features supported by the driver
  **/
  pSysCapInfo->RenameNsSupported = FEATURE_SUPPORTED;

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
  pSysCapInfo->DisableDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->UnlockDeviceSecuritySupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->FreezeDeviceSecuritySupported = FEATURE_NOT_SUPPORTED;
  pSysCapInfo->ChangeDevicePassphraseSupported = FEATURE_NOT_SUPPORTED;
#else
  pSysCapInfo->EraseDeviceDataSupported = FEATURE_SUPPORTED;
  pSysCapInfo->EnableDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->DisableDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->UnlockDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->FreezeDeviceSecuritySupported = FEATURE_SUPPORTED;
  pSysCapInfo->ChangeDevicePassphraseSupported = FEATURE_SUPPORTED;
#endif

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function fills the opcode and subopcode for the FV Firmware command.
  After that it sends the command - update or update and execute, depending on the Exec flag.

  The caller must ensure, that the LargeInputPayloadSize is set and the buffer is already copied to the command.

  @param[in] pDimm Pointer to DIMM
  @param[in] Smbus Execute on the SMBUS mailbox instead of DDRT
  @param[in,out] pPassThruCommand the pointer to the allocated command. After completion the FV response is
    in the structure, so the caller needs to read it after calling this function.

  @retval EFI_SUCCESS if the command was send successfully.
  @retval the PassThru function from the PassThruProtocol return values.
**/
STATIC
EFI_STATUS
EFIAPI
SendUpdatePassThru(
  IN     DIMM *pDimm,
  IN     BOOLEAN Smbus,
  IN OUT FW_CMD *pPassThruCommand
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (pDimm == NULL || pPassThruCommand == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pPassThruCommand->Opcode = PtUpdateFw;       //!< Firmware update category
  pPassThruCommand->SubOpcode = SubopUpdateFw; //!< Execute the firmware image
#ifndef OS_BUILD
  if (Smbus) {
    ReturnCode = SmbusPassThru(pDimm->SmbusAddress, pPassThruCommand, PT_UPDATEFW_TIMEOUT_INTERVAL);
  } else {
#endif
    ReturnCode = PassThru(pDimm, pPassThruCommand, PT_UPDATEFW_TIMEOUT_INTERVAL);
#ifndef OS_BUILD
  }
#endif
  if (EFI_ERROR(ReturnCode)) {
    if (FW_ERROR(pPassThruCommand->Status)) {
      ReturnCode = MatchFwReturnCode(pPassThruCommand->Status);
    }
    goto Finish;
  }

Finish:
  return ReturnCode;
}

STATIC
EFI_STATUS
ValidateImageVersion(
  IN       FW_IMAGE_HEADER *pImage,
  IN       BOOLEAN Force,
  IN       DIMM *pDimm,
      OUT  NVM_STATUS *pNvmStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM_BSR Bsr;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&Bsr, sizeof(Bsr));

  if (pImage == NULL || pDimm == NULL || pNvmStatus == NULL) {
    goto Finish;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (pDimm->FwVer.FwProduct != pImage->ImageVersion.ProductNumber.Version) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_FIRMWARE_VERSION_NOT_VALID;
    }
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pDimm->FwVer.FwRevision > pImage->ImageVersion.RevisionNumber.Version) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_FIRMWARE_VERSION_NOT_VALID;
    }
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pDimm->FwVer.FwSecurityVersion > pImage->ImageVersion.SecurityVersionNumber.Version) {

    ReturnCode = pNvmDimmConfigProtocol->GetBSRAndBootStatusBitMask(pNvmDimmConfigProtocol, pDimm->DimmID, &Bsr.AsUint64, NULL);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not get the DIMM BSR register, can't check if it is safe to send the command.");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
    if (Bsr.Separated_Current_FIS.OIE != DIMM_BSR_OIE_ENABLED) {
      if (pNvmStatus != NULL) {
        *pNvmStatus = NVM_ERR_FIRMWARE_VERSION_NOT_VALID;
      }
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    if (!Force) {
      if (pNvmStatus != NULL) {
        *pNvmStatus = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
      }
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  if (pDimm->FwVer.FwRevision == pImage->ImageVersion.RevisionNumber.Version &&
    pDimm->FwVer.FwSecurityVersion == pImage->ImageVersion.SecurityVersionNumber.Version &&
    pDimm->FwVer.FwBuild > pImage->ImageVersion.BuildNumber.Build) {
    if (!Force) {
      if (pNvmStatus != NULL) {
        *pNvmStatus = NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED;
      }
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }

  if ((BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit1) < DEV_FW_API_VERSION_MAJOR_MIN) ||
      (BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit1) == DEV_FW_API_VERSION_MAJOR_MIN &&
        BCD_TO_TWO_DEC(pImage->FwApiVersion.Byte.Digit2) < DEV_FW_API_VERSION_MINOR_MIN)) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_FIRMWARE_API_NOT_VALID;
    }
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  @retval EFI_ACCESS_DENIED When the firmware major API version is lower than the current on the DIMM
  @retval EFI_SUCCESS Update has completed successfully
**/
STATIC
EFI_STATUS
UpdateSmbusDimmFw(
  IN     UINT16 DimmPid,
  IN     CONST VOID *pImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     BOOLEAN Force,
     OUT NVM_STATUS *pNvmStatus OPTIONAL,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifndef OS_BUILD
  FW_CMD *pPassThruCommand = NULL;
  DIMM *pCurrentDimm = NULL;
  FW_IMAGE_HEADER *pFileHeader = NULL;
  CHAR16 *pErrorMessage = NULL;
  UINT64 PacketsCounter = 0;
  UINT16 CurrentPacket = 0;
  FW_SP_UPDATE_PACKET FwUpdatePacket;
  UINT8 Percent = 0;

  NVDIMM_ENTRY();

  ZeroMem(&FwUpdatePacket, sizeof(FwUpdatePacket));

  if (pImageBuffer == NULL || pCommandStatus == NULL) {
    goto FinishClean;
  }
  pFileHeader = (FW_IMAGE_HEADER *) pImageBuffer;

  // upload FW image to specified DIMMs
  pCurrentDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.UninitializedDimms);
  if (pCurrentDimm == NULL) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_DIMM_NOT_FOUND;
    }
    ReturnCode = EFI_NOT_FOUND;
    goto FinishClean;
  }

  if (!ValidateImage(pFileHeader, ImageBufferSize, &pErrorMessage)) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_IMAGE_FILE_NOT_VALID;
    }
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  ReturnCode = ValidateImageVersion(pFileHeader, Force, pCurrentDimm, pNvmStatus);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /**
    Prepare the FV PassThru command
  **/
  pPassThruCommand = AllocateZeroPool(sizeof(*pPassThruCommand));
  if (pPassThruCommand == NULL) {
    NVDIMM_DBG("Failed on allocating the command, NULL returned.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (ImageBufferSize % UPDATE_FIRMWARE_DATA_PACKET_SIZE != 0) {
    NVDIMM_DBG("The buffer size is not aligned to %d bytes.\n", UPDATE_FIRMWARE_DATA_PACKET_SIZE);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  PacketsCounter = ImageBufferSize / UPDATE_FIRMWARE_DATA_PACKET_SIZE;
  if (PacketsCounter > FW_UPDATE_SP_MAXIMUM_PACKETS || PacketsCounter < FW_UPDATE_SP_MINIMUM_PACKETS) {
    NVDIMM_DBG("The buffer size divided by packet size gave too many packets: packet size - %d got packets - %d.\n",
      UPDATE_FIRMWARE_DATA_PACKET_SIZE, PacketsCounter);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pPassThruCommand->InputPayloadSize = sizeof(FwUpdatePacket);
  FwUpdatePacket.PayloadTypeSelector = FW_UPDATE_SP_SELECTOR;

  FwUpdatePacket.TransactionType = FW_UPDATE_SP_INIT_TRANSFER;
  FwUpdatePacket.PacketNumber = CurrentPacket;

  CopyMem(&FwUpdatePacket.Data, (UINT8 *) pImageBuffer + (UPDATE_FIRMWARE_DATA_PACKET_SIZE * CurrentPacket), UPDATE_FIRMWARE_DATA_PACKET_SIZE);

  CopyMem(pPassThruCommand->InputPayload, &FwUpdatePacket, sizeof(FwUpdatePacket));
  CurrentPacket++;
  ReturnCode = SendUpdatePassThru(pCurrentDimm, TRUE, pPassThruCommand);
  if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
    NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_OPERATION_FAILED;
    }
    goto Finish;
  }

  while (CurrentPacket < (PacketsCounter - 1)) {
    Percent = (UINT8)((CurrentPacket*100)/PacketsCounter);
    SetObjProgress(pCommandStatus, pCurrentDimm->DeviceHandle.AsUint32, Percent);

    FwUpdatePacket.TransactionType = FW_UPDATE_SP_CONTINUE_TRANSFER;
    FwUpdatePacket.PacketNumber = CurrentPacket;

    CopyMem(&FwUpdatePacket.Data, (UINT8 *)pImageBuffer + (UPDATE_FIRMWARE_DATA_PACKET_SIZE * CurrentPacket), UPDATE_FIRMWARE_DATA_PACKET_SIZE);
    CopyMem(pPassThruCommand->InputPayload, &FwUpdatePacket, sizeof(FwUpdatePacket));
    CurrentPacket++;

    ReturnCode = SendUpdatePassThru(pCurrentDimm, TRUE, pPassThruCommand);
    if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
      NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
      if (pNvmStatus != NULL) {
        *pNvmStatus = NVM_ERR_OPERATION_FAILED;
      }
      goto Finish;
    }
  }

  FwUpdatePacket.TransactionType = FW_UPDATE_SP_END_TRANSFER;
  FwUpdatePacket.PacketNumber = CurrentPacket;

  CopyMem(&FwUpdatePacket.Data, (UINT8 *) pImageBuffer + (UPDATE_FIRMWARE_DATA_PACKET_SIZE * CurrentPacket), UPDATE_FIRMWARE_DATA_PACKET_SIZE);
  CopyMem(pPassThruCommand->InputPayload, &FwUpdatePacket, sizeof(FwUpdatePacket));

  ReturnCode = SendUpdatePassThru(pCurrentDimm, TRUE, pPassThruCommand);
  if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
    NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_OPERATION_FAILED;
    }
    goto Finish;
  }

  pCurrentDimm->RebootNeeded = TRUE;

  if (pNvmStatus != NULL) {
    *pNvmStatus = NVM_SUCCESS_FW_RESET_REQUIRED;
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  ClearNvmStatus(GetObjectStatus(pCommandStatus, pCurrentDimm->DeviceHandle.AsUint32), NVM_OPERATION_IN_PROGRESS);

FinishClean:
  FREE_POOL_SAFE(pPassThruCommand);
  FREE_POOL_SAFE(pErrorMessage);

  NVDIMM_EXIT_I64(ReturnCode);
  #endif
  return ReturnCode;
}

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
  @retval EFI_ACCESS_DENIED When the firmware major API version is lower than the current on the DIMM
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  FW_CMD *pPassThruCommand = NULL;
  DIMM *pCurrentDimm = NULL;
  FW_IMAGE_HEADER *pFileHeader = NULL;
  CHAR16 *pErrorMessage = NULL;
#ifdef OS_BUILD
  BOOLEAN RetryOccurred = FALSE;
#endif
#ifdef WA_UPDATE_FIRMWARE_VIA_SMALL_PAYLOAD
  UINT64 PacketsCounter = 0;
  UINT16 CurrentPacket = 0;
  FW_SP_UPDATE_PACKET FwUpdatePacket;
#endif

  NVDIMM_ENTRY();

#ifdef WA_UPDATE_FIRMWARE_VIA_SMALL_PAYLOAD
  SetMem(&FwUpdatePacket, sizeof(FwUpdatePacket), 0x0);
#endif

  if (pImageBuffer == NULL) {
    NVDIMM_DBG("pImageBuffer is null");
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  pFileHeader = (FW_IMAGE_HEADER *) pImageBuffer;

  // upload FW image to specified DIMMs
  pCurrentDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pCurrentDimm == NULL) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_DIMM_NOT_FOUND;
    }
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  if (!IsDimmManageable(pCurrentDimm)) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND;
    }
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!ValidateImage(pFileHeader, ImageBufferSize, &pErrorMessage)) {
    if (pNvmStatus != NULL) {
      *pNvmStatus = NVM_ERR_IMAGE_FILE_NOT_VALID;
    }
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  ReturnCode = ValidateImageVersion(pFileHeader, Force, pCurrentDimm, pNvmStatus);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /**
    Prepare the FV PassThru command
  **/
  pPassThruCommand = AllocateZeroPool(sizeof(*pPassThruCommand));
  if (pPassThruCommand == NULL) {
    NVDIMM_DBG("Failed on allocating the command, NULL returned.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

#ifndef WA_UPDATE_FIRMWARE_VIA_SMALL_PAYLOAD
  pPassThruCommand->LargeInputPayloadSize = (UINT32) ImageBufferSize;
  CopyMem(pPassThruCommand->LargeInputPayload, pImageBuffer, ImageBufferSize);

#ifdef OS_BUILD
  do
  {
     ReturnCode = SendUpdatePassThru(pCurrentDimm, FALSE, pPassThruCommand);
     if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
        if (FW_DEVICE_BUSY == pPassThruCommand->Status)
        {
           Print(L"Device 0x%x is busy, will retry...\n", pCurrentDimm->DimmID);
           RetryOccurred = TRUE;
           continue;
        }
        else
        {
           if (pPassThruCommand->Status == FW_UPDATE_ALREADY_OCCURED && RetryOccurred == TRUE)
           {
              break;
           }

           if (pNvmStatus != NULL) {
              if (pPassThruCommand->Status == FW_UPDATE_ALREADY_OCCURED) {
                 *pNvmStatus = NVM_ERR_FIRMWARE_ALREADY_LOADED;
              }
              else {
                 *pNvmStatus = NVM_ERR_OPERATION_FAILED;
              }
           }
        }
        goto Finish;
     }
  } while(FW_DEVICE_BUSY == pPassThruCommand->Status);
#else
  ReturnCode = SendUpdatePassThru(pCurrentDimm, FALSE, pPassThruCommand);
  if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
     NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
     if (pNvmStatus != NULL) {
        if (pPassThruCommand->Status == FW_UPDATE_ALREADY_OCCURED) {
           *pNvmStatus = NVM_ERR_FIRMWARE_ALREADY_LOADED;
        }
        else {
           *pNvmStatus = NVM_ERR_OPERATION_FAILED;
        }
     }
     goto Finish;
  }
#endif
#else
  if (ImageBufferSize % UPDATE_FIRMWARE_DATA_PACKET_SIZE != 0) {
    NVDIMM_DBG("The buffer size is not aligned to %d bytes.\n", UPDATE_FIRMWARE_DATA_PACKET_SIZE);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  PacketsCounter = ImageBufferSize / UPDATE_FIRMWARE_DATA_PACKET_SIZE;
  if (PacketsCounter > FW_UPDATE_SP_MAXIMUM_PACKETS || PacketsCounter < FW_UPDATE_SP_MINIMUM_PACKETS) {
    NVDIMM_DBG("The buffer size divided by packet size gave too many packets: packet size - %d got packets - %d.\n",
      UPDATE_FIRMWARE_DATA_PACKET_SIZE, PacketsCounter);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  pPassThruCommand->InputPayloadSize = sizeof(FwUpdatePacket);
  FwUpdatePacket.PayloadTypeSelector = FW_UPDATE_SP_SELECTOR;

  FwUpdatePacket.TransactionType = FW_UPDATE_SP_INIT_TRANSFER;
  FwUpdatePacket.PacketNumber = CurrentPacket;
  CopyMem(&FwUpdatePacket.Data, (UINT8 *) pImageBuffer + (UPDATE_FIRMWARE_DATA_PACKET_SIZE * CurrentPacket), UPDATE_FIRMWARE_DATA_PACKET_SIZE);
  CopyMem(pPassThruCommand->InputPayload, &FwUpdatePacket, sizeof(FwUpdatePacket));
  CurrentPacket++;
  ReturnCode = SendUpdatePassThru(pCurrentDimm, FALSE, pPassThruCommand);
  if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
    NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
    if (pNvmStatus != NULL) {
      if (pPassThruCommand->Status == FW_UPDATE_ALREADY_OCCURED) {
        *pNvmStatus = NVM_ERR_FIRMWARE_ALREADY_LOADED;
      } else {
        *pNvmStatus = NVM_ERR_OPERATION_FAILED;
      }
    }
    goto Finish;
  }

  while (CurrentPacket < (PacketsCounter - 1)) {
    FwUpdatePacket.TransactionType = FW_UPDATE_SP_CONTINUE_TRANSFER;
    FwUpdatePacket.PacketNumber = CurrentPacket;
    CopyMem(&FwUpdatePacket.Data, (UINT8 *) pImageBuffer + (UPDATE_FIRMWARE_DATA_PACKET_SIZE * CurrentPacket), UPDATE_FIRMWARE_DATA_PACKET_SIZE);
    CopyMem(pPassThruCommand->InputPayload, &FwUpdatePacket, sizeof(FwUpdatePacket));
    CurrentPacket++;

    ReturnCode = SendUpdatePassThru(pCurrentDimm, FALSE, pPassThruCommand);
    if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
      NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
      if (pNvmStatus != NULL) {
        if (pPassThruCommand->Status == FW_UPDATE_ALREADY_OCCURED) {
          *pNvmStatus = NVM_ERR_FIRMWARE_ALREADY_LOADED;
        } else {
          *pNvmStatus = NVM_ERR_OPERATION_FAILED;
        }
      }
      goto Finish;
    }
  }

  FwUpdatePacket.TransactionType = FW_UPDATE_SP_END_TRANSFER;
  FwUpdatePacket.PacketNumber = CurrentPacket;
  CopyMem(&FwUpdatePacket.Data, (UINT8 *)pImageBuffer + (UPDATE_FIRMWARE_DATA_PACKET_SIZE * CurrentPacket), UPDATE_FIRMWARE_DATA_PACKET_SIZE);
  CopyMem(pPassThruCommand->InputPayload, &FwUpdatePacket, sizeof(FwUpdatePacket));
  ReturnCode = SendUpdatePassThru(pCurrentDimm, FALSE, pPassThruCommand);
  if (EFI_ERROR(ReturnCode) || FW_ERROR(pPassThruCommand->Status)) {
    NVDIMM_DBG("Failed on PassThru, efi_status=%r status=%d", ReturnCode, pPassThruCommand->Status);
    if (pNvmStatus != NULL) {
      if (pPassThruCommand->Status == FW_UPDATE_ALREADY_OCCURED) {
        *pNvmStatus = NVM_ERR_FIRMWARE_ALREADY_LOADED;
      } else {
        *pNvmStatus = NVM_ERR_OPERATION_FAILED;
      }
    }
    goto Finish;
  }
#endif

  pCurrentDimm->RebootNeeded = TRUE;

  if (pNvmStatus != NULL) {
    *pNvmStatus = NVM_SUCCESS_FW_RESET_REQUIRED;
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPassThruCommand);
  FREE_POOL_SAFE(pErrorMessage);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/*
 * Helper function for writing a spi image to a backup file.
 * Does note overwrite an existing file
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
  Recover firmware of a specified NVDIMM

  @param[in] DimmPid Dimm ID of a NVDIMM on which recovery is to be performed
  @param[in] pNewSpiImageBuffer is a pointer to new SPI FW image
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
  IN     CONST VOID *pNewSpiImageBuffer,
  IN     UINT64 ImageBufferSize,
  IN     CHAR16 *pWorkingDirectory OPTIONAL,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
#ifndef OS_BUILD
  DIMM *pCurrentDimm = NULL;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  SPI_DIRECTORY *pSpiDirectoryNewSpiImageBuffer;
  SPI_DIRECTORY SpiDirectoryTarget;
  UINT8 *pFconfigRegionNewSpiImageBuffer = NULL;
  UINT8 *pFconfigRegionTemp = NULL;
  UINT16 DeviceId;

  NVDIMM_ENTRY();

  if (pNewSpiImageBuffer == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  pSpiDirectoryNewSpiImageBuffer = (SPI_DIRECTORY *) pNewSpiImageBuffer;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **) &pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pCurrentDimm = GetDimmByHandle(DimmHandle, &gNvmDimmData->PMEMDev.UninitializedDimms);
  if (pCurrentDimm == NULL) {
    NVDIMM_ERR("Failed to find handle 0x%x in uninitialized dimm list", DimmHandle);
    SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_DIMM_NOT_FOUND);
    goto Finish;
  }

  ReturnCode = SpiCheckAccess(pCurrentDimm);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Spi access is not enabled on DIMM 0x%x", pCurrentDimm->DeviceHandle.AsUint32);
    SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_RECOVERY_ACCESS_NOT_ENABLED);
    goto Finish;
  }

  // Make sure we are working with a Dcpmem 1st gen device
  ReturnCode = GetDeviceIdSpd(pCurrentDimm->SmbusAddress, &DeviceId);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Cannot access spd data over smbus: 0x%x", ReturnCode);
    SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_SPD_NOT_ACCESSIBLE);
    goto Finish;
  }
  if (DeviceId != SPD_DEVICE_ID_DCPMEM_GEN1) {
    NVDIMM_ERR("Incompatible hardware revision 0x%x", DeviceId);
    SetObjStatusForDimm(pCommandStatus, pCurrentDimm, NVM_ERR_INCOMPATIBLE_HARDWARE_REVISION);
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
  CHECK_RESULT(DebugWriteSpiImageToFile(pWorkingDirectory, pCurrentDimm->DeviceHandle.AsUint32,
      pNewSpiImageBuffer, ImageBufferSize), Finish);
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

  @retval EFI_INVALID_PARAMETER One of parameters provided is not acceptable
  @retval EFI_NOT_FOUND there is no NVDIMM with such Pid
  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory for a data structure
  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS TempReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;
  FW_IMAGE_HEADER *pFileHeader = NULL;
  EFI_FILE_HANDLE FileHandle = NULL;
  VOID *pImageBuffer = NULL;
  CHAR16 *pErrorMessage = NULL;
  UINTN BuffSize = 0;
  UINTN TempBuffSize = 0;
  NVM_STATUS NvmStatus = NVM_ERR_OPERATION_NOT_STARTED;

  ZeroMem(pDimms, sizeof(pDimms));

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL || (pDimmIds == NULL && DimmIdsCount > 0)) {
    goto Finish;
  }

#ifdef MDEPKG_NDEBUG
  if (Recovery) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_STARTED);
    goto Finish;
  }
#endif /* MDEPKG_NDEBUG */

  if (pFileName == NULL) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_FILENAME_NOT_PROVIDED);
    goto Finish;
  }

  if (!Recovery && !gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }


  if (!Recovery && FlashSPI) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  if (!Examine) {
    pCommandStatus->ObjectType = ObjectTypeDimm;
  }

  TempReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, Recovery, pDimms, &DimmsNum, pCommandStatus);

  if (EFI_ERROR(TempReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  if (!LoadFileAndCheckHeader(pFileName, pWorkingDirectory, FlashSPI, &pFileHeader, &pErrorMessage)) {
    for (Index = 0; Index < DimmsNum; ++Index) {
      if (Examine) {
        pCommandStatus->ObjectType = ObjectTypeDimm;
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_EXAMINE_INVALID);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_FILE_NOT_VALID);
      }
    }
    ReturnCode = EFI_LOAD_ERROR;
    NVDIMM_DBG("LoadFileAndCheckHeader Failed");
    goto Finish;
  }

  TempReturnCode = OpenFile(pFileName, &FileHandle, pWorkingDirectory, FALSE);
  TempReturnCode = GetFileSize(FileHandle, &BuffSize);

  pImageBuffer = AllocateZeroPool(BuffSize);
  if (pImageBuffer == NULL) {
    NVDIMM_ERR("Out of memory");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  TempBuffSize = BuffSize;
  TempReturnCode = FileHandle->Read(FileHandle, &BuffSize, pImageBuffer);
  if (EFI_ERROR(TempReturnCode) || BuffSize != TempBuffSize) {
    if (Examine) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_IMAGE_EXAMINE_INVALID);
    } else {
      ResetCmdStatus(pCommandStatus, NVM_ERR_IMAGE_FILE_NOT_VALID);
    }
    goto Finish;
  }

  if (Examine) {
    // don't update, just get image information
    if (pFwImageInfo == NULL) {
      goto Finish;
    }
    for (Index = 0; Index < DimmsNum; Index++) {
      if (Recovery && FlashSPI) {
        // We will only be able to flash spi if we can access the
        // spi interface over smbus
#ifdef OS_BUILD
        // Spi check access will fail with unsupported on OS
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
#else
        TempReturnCode = SpiCheckAccess(pDimms[Index]);
#endif
        if (EFI_ERROR(TempReturnCode)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_RECOVERY_ACCESS_NOT_ENABLED);
        } else {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS_IMAGE_EXAMINE_OK);
        }
      } else {
      TempReturnCode = ValidateImageVersion(pFileHeader, FALSE, pDimms[Index], &NvmStatus);

      if (EFI_ERROR(TempReturnCode)) {
        if (TempReturnCode == EFI_ABORTED) {
          if (NvmStatus == NVM_ERR_FIRMWARE_TOO_LOW_FORCE_REQUIRED) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_EXAMINE_LOWER_VERSION);
            } else if (NvmStatus == NVM_ERR_FIRMWARE_VERSION_NOT_VALID) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_EXAMINE_INVALID);
            } else if (NvmStatus == NVM_ERR_FIRMWARE_API_NOT_VALID) {
            SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_IMAGE_EXAMINE_INVALID);
          }
        } else {
          goto Finish;
        }
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS_IMAGE_EXAMINE_OK);
      }
    }
    }
    SetCmdStatus(pCommandStatus, NVM_SUCCESS);
    pFwImageInfo->Date = pFileHeader->Date;
    pFwImageInfo->ImageVersion = pFileHeader->ImageVersion;
    pFwImageInfo->FirmwareType = pFileHeader->ImageType;
    pFwImageInfo->ModuleVendor = pFileHeader->ModuleVendor;
    pFwImageInfo->Size = pFileHeader->Size;

  } else {
    // upload FW image to all specified DIMMs
    for (Index = 0; Index < DimmsNum; Index++) {
      if (Recovery && FlashSPI) {
        ReturnCode = RecoverDimmFw(pDimms[Index]->DeviceHandle.AsUint32,
            pImageBuffer, BuffSize, pWorkingDirectory, pCommandStatus);
      } else if (Recovery) {
        ReturnCode = UpdateSmbusDimmFw(pDimms[Index]->DimmID, pImageBuffer, BuffSize, Force, &NvmStatus, pCommandStatus);
      } else {
        ReturnCode = UpdateDimmFw(pDimms[Index]->DimmID, pImageBuffer, BuffSize, Force, &NvmStatus);
      }

      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NvmStatus);
      if (EFI_ERROR(ReturnCode) && (NvmStatus != NVM_ERR_FIRMWARE_ALREADY_LOADED)) {
        goto Finish;
      }
    }
  }


  ReturnCode = EFI_SUCCESS;

Finish:
  if (FileHandle != NULL) {
    FileHandle->Close(FileHandle);
  }
  FREE_POOL_SAFE(pFileHeader);
  FREE_POOL_SAFE(pImageBuffer);
  FREE_POOL_SAFE(pErrorMessage);
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
  UINT64 ReservedSize = 0;
  UINT64 ActualVolatileSize = 0;
  REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM];
  UINT32 RegionGoalTemplatesNum = 0;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  BOOLEAN Found = FALSE;
  MEMORY_MODE AllowedMode = MEMORY_MODE_1LM;
  UINT8 DimmSecurityState = 0;
  DIMM *pDimmsOnSocket[MAX_DIMMS];
  UINT32 NumDimmsOnSocket = 0;
  UINT64 TotalInputVolatileSize = 0;
  UINT64 TotalActualVolatileSize = 0;
  UINT32 Socket = 0;
  REGION_GOAL_DIMM *pDimmsSym = NULL;
  UINT32 DimmsSymNum = 0;
  REGION_GOAL_DIMM *pDimmsAsym = NULL;
  UINT32 DimmsAsymNum = 0;

  NVDIMM_ENTRY();

  ZeroMem(RegionGoalTemplates, sizeof(RegionGoalTemplates));

  if (pThis == NULL || RegionGoalTemplates == NULL || pCommandStatus == NULL
    || pVolatilePercent == NULL || pConfigGoals == NULL
    || pConfigGoalsCount == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pConfigGoalsCount = 0;

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
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

  pCommandStatus->ObjectType = ObjectTypeDimm;

  /** Verify input parameters and determine a list of DIMMs **/
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, FALSE, ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (ppDimms[Index]->GoalConfigStatus != GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS) {
      ReturnCode = EFI_ABORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_CURR_CONF_EXISTS);
      goto Finish;
    }

    ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is enabled.");
      goto Finish;
    }
  }

  ReturnCode = PersistentMemoryTypeValidation(PersistentMemType);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ReserveDimm != RESERVE_DIMM_NONE && ReserveDimm != RESERVE_DIMM_STORAGE &&
      ReserveDimm != RESERVE_DIMM_AD_NOT_INTERLEAVED) {
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

    /* caclulate the  total requested volatile size */
    TotalInputVolatileSize += ActualVolatileSize;

    /** Calculate Reserved size **/
    ReturnCode = CalculateDimmCapacityFromPercent(pDimmsOnSocket, NumDimmsOnSocket, ReservedPercent, &ReservedSize);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = MapRequestToActualRegionGoalTemplates(pDimmsOnSocket, NumDimmsOnSocket,
        pDimmsSymPerSocket, &DimmsSymNumPerSocket, pDimmsAsymPerSocket,
        &DimmsAsymNumPerSocket, PersistentMemType, ActualVolatileSize,
        ReservedSize, &ActualVolatileSize, RegionGoalTemplates, &RegionGoalTemplatesNum, pCommandStatus);

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
  // We have removed all symetrical memory from the system and region templates should be reduced
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

  if (AllowedMode != MEMORY_MODE_2LM) {
    /** Check if volatile memory has been requested **/
    for (Index = 0; Index < *pConfigGoalsCount; Index++) {
      if (pConfigGoals[Index].VolatileSize > 0) {
        SetCmdStatus(pCommandStatus, NVM_WARN_2LM_MODE_OFF);
        break;
      }
    }
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
CreateGoalConfig(
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
  UINT8 DimmSecurityState = 0;
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

  NVDIMM_ENTRY();

  ZeroMem(RegionGoalTemplates, sizeof(RegionGoalTemplates));
  ZeroMem(&DriverPreferences, sizeof(DriverPreferences));

  if (pThis == NULL || pCommandStatus == NULL || VolatilePercent > 100 || ReservedPercent > 100 || VolatilePercent + ReservedPercent > 100) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    NVDIMM_DBG("Invalid Parameter");
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    NVDIMM_DBG("Operation not supported by mixed SKU");
    goto Finish;
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

  pCommandStatus->ObjectType = ObjectTypeDimm;

  /** Verify input parameters and determine a list of DIMMs **/
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, FALSE, ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

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

  if (ReserveDimm != RESERVE_DIMM_NONE && ReserveDimm != RESERVE_DIMM_STORAGE &&
      ReserveDimm != RESERVE_DIMM_AD_NOT_INTERLEAVED) {
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

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms);
  if (EFI_ERROR(ReturnCode)) {

    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (ppDimms[Index]->GoalConfigStatus != GOAL_CONFIG_STATUS_NO_GOAL_OR_SUCCESS) {
      ReturnCode = EFI_ABORTED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_REGION_CURR_CONF_EXISTS);
      NVDIMM_DBG("Current Goal Configuration exists. Operation Aborted");
      goto Finish;
    }

    ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is enabled.");
      goto Finish;
    }
   }

  ReturnCode = PersistentMemoryTypeValidation(PersistentMemType);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** User has to configure all the unconfigured DIMMs or all DIMMs on a given socket at once **/
  ReturnCode = VerifyCreatingSupportedRegionConfigs(ppDimms, DimmsNum, pCommandStatus);
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

  // TODO: need to refactor this more. We have to remove PM_TYPE_STORAGE as it is no longer used
  // simple refactoring would be to rename the PM_TYPE_STORAGE to something like PM_TYPE_NO_AD
  // PM_TYPE_STORAGE is currently used to NOT calculate AD capacity
  /** If Volatile and Reserved Percent sum to 100 then never map Appdirect even if alignment would allow it **/
  if (VolatilePercent + ReservedPercent == 100) {
    PersistentMemType = PM_TYPE_STORAGE;
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

    /** Calculate Reserved size **/
    ReturnCode = CalculateDimmCapacityFromPercent(pDimmsOnSocket, NumDimmsOnSocket, ReservedPercent, &ReservedSize);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = MapRequestToActualRegionGoalTemplates(pDimmsOnSocket, NumDimmsOnSocket,
        pDimmsSymPerSocket, &DimmsSymNumPerSocket, pDimmsAsymPerSocket, &DimmsAsymNumPerSocket,
        PersistentMemType, VolatileSize , ReservedSize, NULL,
        RegionGoalTemplates, &RegionGoalTemplatesNum, pCommandStatus);
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

  // We have removed all symetrical memory from the system, make the goal template num 0
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
    ReturnCode = InitializeAllLabelStorageAreas(ppDimms, DimmsNum, LabelVersionMajor,
      LabelVersionMinor, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("InitializeAllLabelStorageAreas Error");
      goto Finish;
    }
  }

#ifdef OS_BUILD
  if (!EFI_ERROR(ReturnCode))
  {
      CHAR16 *pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_CHANGE_NEW_GOAL), NULL);
      StoreSystemEntryForDimmList(pCommandStatus, NVM_SYSLOG_SRC_W,
          SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, EVENT_CONFIG_CHANGE_300, FALSE, TRUE, TRUE, FALSE, 0),
          pTmpStr);
      FREE_POOL_SAFE(pTmpStr);
  }
#endif // OS_BUILD

Finish:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT8 DimmSecurityState = 0;
  UINT32 Index = 0;

  SetMem(pDimms, sizeof(pDimms), 0x0);

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;

  /** Verify input parameters and determine a list of DIMMs **/
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, FALSE, pDimms, &DimmsNum, pCommandStatus);
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
      goto Finish;
    }
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms);
  if (EFI_ERROR(ReturnCode)) {
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

#ifdef OS_BUILD
  if (!EFI_ERROR(ReturnCode))
  {
    CHAR16 *pTmpStr = HiiGetString(gNvmDimmData->HiiHandle, STRING_TOKEN(STR_CONFIG_CHANGE_DELETE_GOAL), NULL);
    StoreSystemEntryForDimmList(pCommandStatus, NVM_SYSLOG_SRC_W,
      SYSTEM_EVENT_CREATE_EVENT_TYPE(SYSTEM_EVENT_CAT_MGMT, SYSTEM_EVENT_TYPE_INFO, EVENT_CONFIG_CHANGE_301, FALSE, TRUE, TRUE, FALSE, 0),
      pTmpStr);
    FREE_POOL_SAFE(pTmpStr);
  }
#endif // OS_BUILD

Finish:
  ClearInternalGoalConfigsInfo(&gNvmDimmData->PMEMDev.Dimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
//#ifndef OS_BUILD
  EFI_FILE_HANDLE pFileHandle = NULL;
  DIMM_CONFIG *pDimmConfigs = NULL;
  UINT32 DimmConfigsNum = 0;

#ifdef OS_BUILD
  MEMORY_RESOURCES_INFO MemoryResourcesInfo;
#else
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

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;

#ifdef OS_BUILD
  //triggers PCD read
  GetMemoryResourcesInfo(pThis, &MemoryResourcesInfo);
  GetRegionList();
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
    NVDIMM_WARN("Failed on create file to dump Region goal configuration. (%r)", ReturnCode);
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
      NVDIMM_WARN("Failed on deleting old dump file! (%r)", ReturnCode);
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }

    /** Create new file for dump **/
    ReturnCode = OpenFileByDevice(pFilePath, pDevicePath, TRUE, &pFileHandle);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed on create file to dump Region Goal Configuration. (%r)", ReturnCode);
      ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
      goto Finish;
    }
  }
#else
  ReturnCode = OpenFile(pFilePath, &pFileHandle, NULL, 1);
  if (EFI_ERROR(ReturnCode)) {
     NVDIMM_WARN("Failed on open dump file header info. (%r)", ReturnCode);
     ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_FILE_OPERATION_FAILED);
     goto Finish;
  }
#endif
  /** Write dump file header **/
  ReturnCode = WriteDumpFileHeader(pFileHandle);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed on write dump file header info. (%r)", ReturnCode);
    ResetCmdStatus(pCommandStatus, NVM_ERR_DUMP_FILE_OPERATION_FAILED);
    goto Finish;
  }

  /** Perform Dump to File **/
  ReturnCode = DumpConfigToFile(pFileHandle, pDimmConfigs, DimmConfigsNum);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed on write dump. (%r)", ReturnCode);
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
  UINT8 DimmSecurityState = 0;
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

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, pSocketIds, SocketIdsCount, FALSE, pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode) || pCommandStatus->GeneralStatus != NVM_ERR_OPERATION_NOT_STARTED) {
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeSocket;

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(pDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is enabled.");
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
    NVDIMM_DBG("SetUpGoalStructures failed. (%r)",ReturnCode);
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
      NVDIMM_DBG("ValidAndPrepareLoadConfig failed. (%r)", ReturnCode);
      SetObjStatus(pCommandStatus, Socket, NULL, 0, pCmdStatusInternal->GeneralStatus);
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
        RESERVE_DIMM_NONE, LabelVersionMajor, LabelVersionMinor, pCmdStatusInternal);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("CreateGoalConfig failed. (%r)", ReturnCode);
      SetObjStatus(pCommandStatus, Socket, NULL, 0, pCmdStatusInternal->GeneralStatus);
      FreeCommandStatus(&pCmdStatusInternal);
      goto Finish;
    }

    FreeCommandStatus(&pCmdStatusInternal);

    SetObjStatus(pCommandStatus, Socket, NULL, 0, NVM_SUCCESS);
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pDimmsConfig);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
               * 0 - Ignore
               * 1 - Yes
               * 2 - No
  @param[in] Encryption Create namespace on an NVM DIMM with encryption enabled. One of:
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
  )
  {
	EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimms[MAX_DIMMS_PER_SOCKET];
  NVM_IS *pIS = NULL;
    LIST_ENTRY *pRegionList = GetRegionList();
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
  CHAR16 *pTempName16 = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  UINT64 ISAvailableCapacity = 0;
  BOOLEAN CapacitySpecified = FALSE;
  UINT64 RequestedCapacity = 0;
  UINT32 RegionCount = 0;
  UINT64 AlignedNamespaceCapacity = 0;
  UINT64 RegionSize = 0;
  UINT64 MinSize = 0;
  UINT64 MaxSize = 0;
  MEMMAP_RANGE AppDirectRange;
  BOOLEAN UseLatestLabelVersion = FALSE;

  ZeroMem(pDimms, sizeof(pDimms));
  ZeroMem(&NamespaceGUID, sizeof(NamespaceGUID));
  ZeroMem(&AppDirectRange, sizeof(AppDirectRange));

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL || ((DimmPid == DIMM_PID_NOTSET) == (RegionId == REGION_ID_NOTSET)) ||
    BlockSize == 0 || pActualNamespaceCapacity == NULL || pNamespaceId == NULL) {
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

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

  if (CapacitySpecified) {
    /** Calculate namespace capacity to provide **/
    RequestedCapacity = BlockCount * BlockSize;
      ReturnCode = ConvertUsableSizeToActualSize(BlockSize, RequestedCapacity, Mode,
      &ActualBlockCount, pActualNamespaceCapacity, pCommandStatus);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  } else {
      // if size is not specified for Namespace, then find the maximum available size
    NamespaceCapacity = 0;
      ReturnCode = GetListSize(&pIS->DimmRegionList, &RegionCount);
        if (EFI_ERROR(ReturnCode) || RegionCount == 0) {
          goto Finish;
        }
      /** Find the free capacity**/
      ReturnCode = FindADMemmapRangeInIS(pIS, MAX_UINT64_VALUE, &AppDirectRange);
        if (EFI_ERROR(ReturnCode) && ReturnCode != EFI_NOT_FOUND) {
          goto Finish;
        }
        ReturnCode = EFI_SUCCESS;
        ISAvailableCapacity = AppDirectRange.RangeLength * RegionCount;
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
    /** Namespace capacity that doesn't include block size with 64B cache lane size **/
    NamespaceCapacity = ActualBlockCount * BlockSize;
    ReturnCode = GetListSize(&pIS->DimmRegionList, &RegionCount);
    if (EFI_ERROR(ReturnCode) || RegionCount == 0) {
      goto Finish;
    }
    AlignedNamespaceCapacity = ROUNDUP(NamespaceCapacity, NAMESPACE_4KB_ALIGNMENT_SIZE * RegionCount);
    RegionSize = AlignedNamespaceCapacity / RegionCount;
    ReturnCode = FindADMemmapRangeInIS(pIS, RegionSize, &AppDirectRange);
    if (EFI_ERROR(ReturnCode)) {
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

  pNamespace->NamespaceId = GenerateNamespaceId();
  pNamespace->Enabled = TRUE;
  pNamespace->Signature = NAMESPACE_SIGNATURE;
  pNamespace->Flags.Values.ReadOnly = FALSE;
    pNamespace->NamespaceType = NamespaceType;
  if (Mode) {
    pNamespace->IsBttEnabled = TRUE;
  } else {
    pNamespace->IsRawNamespace = TRUE;
  }
  pNamespace->BlockSize = BlockSize;

    if (pIS == NULL) {
      NVDIMM_DBG("No target IS for namespace");
      FailFlag = TRUE;
      goto Finish;
    }
    // AppDirect namespaces initially stored with 'updating flag'.
    pNamespace->Flags.Values.Updating = TRUE;
    pNamespace->pParentIS = pIS;

  // No name was provided by the user, lets use our default
  if (pName == NULL) {
    pTempName16 = CatSPrint(NULL, L"NvDimmVol%d", pNamespace->NamespaceId);
    // If we failed to allocate the Dimm name - no big deal, just leave it as it is
    if (pTempName16 != NULL) {
        pName = AllocateZeroPool((StrLen(pTempName16) + 1) * sizeof(CHAR8));
      if (pName != NULL) {
          UnicodeStrToAsciiStrS(pTempName16, pName, StrLen(pTempName16) + 1);
      }
    }
  }

  // Lets leave this check in case that the allocation failed
  if (pName != NULL) {
    CopyMem(&pNamespace->Name, pName, MIN(AsciiStrLen(pName), NSLABEL_NAME_LEN));
    // If we allocated the buffer here, not in the CLI - we need to free it after its copied
    if (pTempName16 != NULL) {
      FREE_POOL_SAFE(pName);
    }
  }

  GenerateRandomGuid(&NamespaceGUID);
  CopyMem(&pNamespace->NamespaceGuid, &NamespaceGUID, NSGUID_LEN);
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

  // Check if capacity meets minimum requirements. This is the aligned capacity.
    ReturnCode = ADNamespaceMinAndMaxAvailableSizeOnIS(pIS, &MinSize, &MaxSize);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (*pActualNamespaceCapacity < MinSize || MinSize == 0) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_NAMESPACE_CAPACITY);
      ReturnCode = EFI_INVALID_PARAMETER;
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
  FREE_POOL_SAFE(pTempName16);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
  }


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
EFIAPI
GetNamespaces (
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
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

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
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
    CopyMem(pNamespaceInfo->NamespaceGuid, pNamespace->NamespaceGuid, sizeof(pNamespaceInfo->NamespaceGuid));
    CopyMem(pNamespaceInfo->Name, pNamespace->Name, sizeof(pNamespaceInfo->Name));
    pNamespaceInfo->HealthState = pNamespace->HealthState;
    pNamespaceInfo->BlockSize = pNamespace->BlockSize;
    pNamespaceInfo->LogicalBlockSize = pNamespace->Media.BlockSize;
    pNamespaceInfo->BlockCount = pNamespace->BlockCount;
    pNamespaceInfo->UsableSize = pNamespace->UsableSize;
    pNamespaceInfo->Major = pNamespace->Major;
    pNamespaceInfo->Minor = pNamespace->Minor;

    pNamespaceInfo->NamespaceMode = pNamespace->IsBttEnabled ? SECTOR_MODE :
                                    ((pNamespace->IsPfnEnabled) ? FSDAX_MODE : NONE_MODE);

    pNamespaceInfo->RegionId = pNamespace->pParentIS->InterleaveSetIndex;

    pNamespaceInfo->Signature = NAMESPACE_INFO_SIGNATURE;
    InsertTailList(pNamespaceListNode, (LIST_ENTRY *) pNamespaceInfo->NamespaceInfoNode);
    (*pNamespacesCount)++;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;

}

/**
  Modify namespace
  Modifies a block or persistent memory namespace on the provided region/dimm.

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance
  @param[in] NamespaceId the ID of the namespace to be modified.
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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NAMESPACE *pNamespace = NULL;
  LIST_ENTRY *pNode = NULL;
  DIMM_REGION *pRegion = NULL;
  SYSTEM_CAPABILITIES_INFO SysCapInfo;
  BOOLEAN NamespaceLocked = FALSE;
  UINT32 Flags = 0;

  NVDIMM_ENTRY();

  ZeroMem(&SysCapInfo, sizeof(SysCapInfo));

  if (pThis == NULL || pName == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  ReturnCode = GetSystemCapabilitiesInfo(&gNvmDimmDriverNvmDimmConfig, &SysCapInfo);
  if (EFI_ERROR(ReturnCode)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

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


  if (SysCapInfo.RenameNsSupported == FEATURE_NOT_SUPPORTED) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_RENAME_NAMESPACE_NOT_SUPPORTED);
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  if (pNamespace->pParentDimm != NULL) {
    ReturnCode = ModifyNamespaceLabels(pNamespace->pParentDimm,
        (GUID *)pNamespace->NamespaceGuid, NULL, pName, 0);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to set the new label name.");
      goto Finish;
    }
  } else {
    /* Write labels with UPDATING set */
    pNamespace->Flags.Values.Updating = TRUE;
    Flags = pNamespace->Flags.AsUint32;

    LIST_FOR_EACH(pNode, &pNamespace->pParentIS->DimmRegionList) {
      pRegion = DIMM_REGION_FROM_NODE(pNode);
      ReturnCode = ModifyNamespaceLabels(pRegion->pDimm,
          (GUID *)pNamespace->NamespaceGuid, &Flags, pName, 0);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to set the new label name.");
        goto Finish;
      }
    }

    /* Write labels with UPDATING clear */
    pNamespace->Flags.Values.Updating = FALSE;
    Flags = pNamespace->Flags.AsUint32;

    LIST_FOR_EACH(pNode, &pNamespace->pParentIS->DimmRegionList) {
      pRegion = DIMM_REGION_FROM_NODE(pNode);
      ReturnCode = ModifyNamespaceLabels(pRegion->pDimm,
          (GUID *)pNamespace->NamespaceGuid, &Flags, pName, 0);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to clear updating flag.");
        goto Finish;
      }
    }
  }

  ZeroMem(pNamespace->Name, sizeof(pNamespace->Name));
  CopyMem(pNamespace->Name, pName, AsciiStrLen(pName));

  SetCmdStatus(pCommandStatus, NVM_SUCCESS);
  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_HII_POINTER(SysCapInfo.PtrInterleaveFormatsSupported);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

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
    ReenumerateNamespacesAndISs();

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

  if (pThis == NULL || pCommandStatus == NULL || pMaxErrorsToFetch == NULL ||
      (pDimmIds == NULL && DimmsCount > 0)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  /** Verify input parameters and determine a list of DIMMs **/
  ReturnCode = VerifyTargetDimms(pDimmIds, DimmsCount, NULL, 0, FALSE, pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
  for (Index = 0; Index < DimmsNum && AllErrorsFetched < *pMaxErrorsToFetch; ++Index) {
    ReturnCode = GetAndParseFwErrorLogForDimm(pDimms[Index],
      ThermalError,
      HighLevel,
      SequenceNumber,
      (*pMaxErrorsToFetch - AllErrorsFetched),
      &SingleDimmErrorsFetched,
      &pErrorLogs[AllErrorsFetched]);

    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    AllErrorsFetched += SingleDimmErrorsFetched;
  }

Finish:
  if (pMaxErrorsToFetch != NULL) {
    *pMaxErrorsToFetch = AllErrorsFetched;
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 LogSizeInMib = 0;
  DIMM *pDimm = NULL;
  UINT64 DebugLogSize = 0;
  NVDIMM_ENTRY();

  if (pThis == NULL || pBytesWritten == NULL || pCommandStatus == NULL) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmDimmID, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_DIMM_NOT_FOUND);
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    SetObjStatus(pCommandStatus, pDimm->DeviceHandle.AsUint32, NULL, 0, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND);
    goto Finish;
  }

  ReturnCode = FwCmdGetFWDebugLogSize(pDimm, &LogSizeInMib);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_SECURITY_VIOLATION) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_INVALID_SECURITY_STATE);
    } else {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE);
    }
    goto Finish;
  }

  if (LogSizeInMib == 0) {
    SetObjStatusForDimm(pCommandStatus, pDimm, NVM_INFO_FW_DBG_LOG_NO_LOGS_TO_FETCH);
    goto Finish; // No data to be dumped on disk. It is not an error.
  }

  DebugLogSize = MIB_TO_BYTES(LogSizeInMib);

  *ppDebugLogs = AllocateZeroPool(DebugLogSize);
  if (*ppDebugLogs == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = FwCmdGetFWDebugLog(pDimm, DebugLogSize, pBytesWritten, *ppDebugLogs);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_SECURITY_VIOLATION) {
      SetObjStatusForDimm(pCommandStatus, pDimm, NVM_ERR_FW_DBG_LOG_FAILED_TO_GET_SIZE);
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
  IN     UINT16 *pDimmIds OPTIONAL,
  IN     UINT32 DimmIdsCount,
  IN     UINT8 FirstFastRefresh,
  IN     UINT8 ViralPolicy,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  PT_OPTIONAL_DATA_POLICY_PAYLOAD InputPayload, OptionalDataPolicyPayload;
  DIMM *pDimms[MAX_DIMMS];
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;

  SetMem(pDimms, sizeof(pDimms), 0x0);
  SetMem(&InputPayload, sizeof(InputPayload), 0x0);

  NVDIMM_ENTRY();

  if (pThis == NULL || pCommandStatus == NULL || (pDimmIds == NULL && DimmIdsCount > 0)) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!gNvmDimmData->PMEMDev.DimmSkuConsistency) {
    ReturnCode = EFI_UNSUPPORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_NOT_SUPPORTED_BY_MIXED_SKU);
    goto Finish;
  }

  pCommandStatus->ObjectType = ObjectTypeDimm;

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = FwCmdGetOptionalConfigurationDataPolicy(pDimms[Index], &OptionalDataPolicyPayload);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_DEVICE_ERROR;
      goto Finish;
    }
    // Get the original values and overwrite only the ones requested
    InputPayload.FirstFastRefresh = OptionalDataPolicyPayload.FirstFastRefresh;
    InputPayload.ViralPolicyEnable = OptionalDataPolicyPayload.ViralPolicyEnable;
    if (FirstFastRefresh != OPTIONAL_DATA_UNDEFINED) {
      InputPayload.FirstFastRefresh = FirstFastRefresh;
    }
    if (ViralPolicy != OPTIONAL_DATA_UNDEFINED) {
      InputPayload.ViralPolicyEnable = ViralPolicy;
    }
    ReturnCode = FwCmdSetOptionalConfigurationDataPolicy(pDimms[Index], &InputPayload);
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_SECURITY_VIOLATION) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_INVALID_SECURITY_STATE);
      } else {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_FW_SET_OPTIONAL_DATA_POLICY_FAILED);
      }
      goto Finish;
    }
    SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef MDEPKG_NDEBUG
  DIMM *pDimm = NULL;
#endif
  NVDIMM_ENTRY();

#ifdef MDEPKG_NDEBUG
  ReturnCode = EFI_UNSUPPORTED;
#else
  if (pThis == NULL || pBsr == NULL || pFwMailboxStatus == NULL || pFwMailboxOutput == NULL ||
      SmallOutputRegisterCount > OUT_PAYLOAD_NUM) {
    ReturnCode = EFI_INVALID_PARAMETER;
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmId, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    ReturnCode = EFI_NOT_FOUND;
    ResetCmdStatus(pCommandStatus, NVM_ERR_DIMM_NOT_FOUND);
    goto Finish;
  }

  if (!IsDimmManageable(pDimm)) {
    ReturnCode = EFI_NOT_FOUND;
    ResetCmdStatus(pCommandStatus, NVM_ERR_MANAGEABLE_DIMM_NOT_FOUND);
    goto Finish;
  }

  ReturnCode = GetKeyDimmRegisters(pDimm, pBsr, pFwMailboxStatus, SmallOutputRegisterCount, pFwMailboxOutput);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    ResetCmdStatus(pCommandStatus, NVM_ERR_FAILED_TO_GET_DIMM_REGISTERS);
    goto Finish;
  }

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);
Finish:
#endif
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;

#ifdef MDEPKG_NDEBUG
  ReturnCode = EFI_UNSUPPORTED;
  goto Finish;
#endif /* MDEPKG_NDEBUG */

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  pDimm = GetDimmByPid(pCmd->DimmID, &gNvmDimmData->PMEMDev.Dimms);

  if (pDimm == NULL || !IsDimmManageable(pDimm)) {
    NVDIMM_DBG("Could not find the specified DIMM or it's unmanageable.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = PassThru(pDimm, pCmd, Timeout);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STATUS
EFIAPI
DimmFormat(
  IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
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

  pCommandStatus->ObjectType = ObjectTypeDimm;

  ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, Recovery, pDimms, &DimmsNum, pCommandStatus);

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
    pReturnCodes[Index] = FwCmdFormatDimm(pDimms[Index], Recovery);
  }
#ifndef OS_BUILD
  UINT64 FwMailboxStatus = 0;
  /* If previous timeout then go back and wait max format time */
  for (Index = 0; Index < DimmsNum; Index++) {
    if (pReturnCodes[Index] == EFI_TIMEOUT) {
      if (Recovery) {
        pReturnCodes[Index] = PollSmbusCmdCompletion(pDimms[Index]->SmbusAddress,
        PT_FORMAT_DIMM_MAX_TIMEOUT, &FwMailboxStatus, &Bsr);
      } else {
        pReturnCodes[Index] = PollCmdCompletion(pDimms[Index]->pHostMailbox,
        PT_FORMAT_DIMM_MAX_TIMEOUT, &FwMailboxStatus, &Bsr.AsUint64);
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
  NvmStatusCode StatusCode= NVM_SUCCESS;
  LIST_ENTRY *pNode = NULL;
  DIMM *pCurDimm = NULL;
  DIMM *pPrevDimm = NULL;
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
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pCurDimm = DIMM_FROM_NODE(pNode);

    if (pPrevDimm != NULL) {
      StatusCode = IsDimmSkuModeMismatch(pPrevDimm, pCurDimm);
      if (StatusCode != NVM_SUCCESS) {
        gNvmDimmData->PMEMDev.DimmSkuConsistency = FALSE;
      }
    }
    pPrevDimm = pCurDimm;
  }

  NVDIMM_DBG("Found %d DCPMEM modules", ListSize);

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
    FreeParsedNfit(gNvmDimmData->PMEMDev.pFitHead);
    gNvmDimmData->PMEMDev.pFitHead = NULL;
  }

  NVDIMM_EXIT_I64(ReturnCode);

  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  UINT64 VolatileCapacity = 0;
  UINT64 AppDirectCapacity = 0;
  UINT64 ReservedCapacity = 0;
  MEMORY_MODE CurrentMode = MEMORY_MODE_1LM;

  NVDIMM_ENTRY();

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);

  if (pDimm == NULL || pVolatileCapacity == NULL || pUnconfiguredCapacity == NULL || pReservedCapacity == NULL ||
      pAppDirectCapacity == NULL || pInaccessibleCapacity == NULL) {
    goto Finish;
  }

  // All shall be zero to start
  *pUnconfiguredCapacity = *pAppDirectCapacity = *pReservedCapacity = *pInaccessibleCapacity = *pVolatileCapacity = 0;

  ReturnCode = CurrentMemoryMode(&CurrentMode);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Unable to determine current memory mode");
    goto Finish;
  }

  if (pDimm->Configured) {
    VolatileCapacity = pDimm->MappedVolatileCapacity;
    ReservedCapacity = GetReservedCapacity(pDimm);
    AppDirectCapacity = pDimm->MappedPersistentCapacity;
  } else {
    if (CurrentMode == MEMORY_MODE_2LM) {
      VolatileCapacity = ROUNDDOWN(pDimm->VolatileCapacity, REGION_VOLATILE_SIZE_ALIGNMENT_B);
      ReservedCapacity = GetReservedCapacity(pDimm);
    }
  }

  *pReservedCapacity = ReservedCapacity;

  if ((pDimm->SkuInformation.MemoryModeEnabled == MODE_ENABLED) && (MEMORY_MODE_2LM == CurrentMode)) {
    *pVolatileCapacity = VolatileCapacity;
  } else {
    // 1LM so none of the partitioned volatile is mapped. Set it as inaccessible.
    *pInaccessibleCapacity = ROUNDDOWN(pDimm->VolatileCapacity, REGION_VOLATILE_SIZE_ALIGNMENT_B);
  }

  if (pDimm->SkuInformation.AppDirectModeEnabled == MODE_ENABLED) {
    *pAppDirectCapacity = AppDirectCapacity;
  } else {
    *pInaccessibleCapacity += AppDirectCapacity;
  }

  if (CurrentMode != MEMORY_MODE_2LM && !pDimm->Configured) {
    //DIMM is unconfigured and system is in 1LM mode
    *pUnconfiguredCapacity = pDimm->RawCapacity;
    // No useable capacity
    *pAppDirectCapacity = *pReservedCapacity = *pInaccessibleCapacity = *pVolatileCapacity = 0;
  } else {
    *pUnconfiguredCapacity = pDimm->PmCapacity - AppDirectCapacity;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_DEVICE_ERROR;

  SMBIOS_STRUCTURE_POINTER SmBiosStruct;
  SMBIOS_STRUCTURE_POINTER BoundSmBiosStruct;
  SMBIOS_VERSION SmbiosVersion;
  UINT16 Index = 0;
  UINT64 Capacity = 0;
  BOOLEAN IsTopologyDimm = FALSE;
  NVDIMM_ENTRY();

  if (pThis == NULL || ppTopologyDimm == NULL || pTopologyDimmsNumber == NULL) {
    goto Finish;
  }

  *ppTopologyDimm = AllocateZeroPool(sizeof(**ppTopologyDimm) * MAX_DIMMS);
  if (*ppTopologyDimm == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ZeroMem(&SmBiosStruct, sizeof(SmBiosStruct));
  ZeroMem(&BoundSmBiosStruct, sizeof(BoundSmBiosStruct));
  ZeroMem(&SmbiosVersion, sizeof(SmbiosVersion));

  GetFirstAndBoundSmBiosStructPointer(&SmBiosStruct, &BoundSmBiosStruct, &SmbiosVersion);
  if (SmBiosStruct.Raw == NULL || BoundSmBiosStruct.Raw == NULL) {
    goto Finish;
  }

  while (SmBiosStruct.Raw < BoundSmBiosStruct.Raw) {
    if (SmBiosStruct.Hdr != NULL) {
      IsTopologyDimm = ((SmBiosStruct.Hdr->Type == SMBIOS_TYPE_MEM_DEV && !SmBiosStruct.Type17->TypeDetail.Nonvolatile
             && SmBiosStruct.Type17->Size != 0) ? TRUE : FALSE);
      if (IsTopologyDimm) {
        (*ppTopologyDimm)[Index].DimmID = SmBiosStruct.Hdr->Handle;
         ReturnCode = GetSmbiosCapacity(SmBiosStruct.Type17->Size, SmBiosStruct.Type17->ExtendedSize, SmbiosVersion,
                                         &Capacity);
        (*ppTopologyDimm)[Index].VolatileCapacity  = Capacity;
        ReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &SmBiosStruct.Type17,
             SmBiosStruct.Type17->DeviceLocator, (*ppTopologyDimm)[Index].DeviceLocator,
             sizeof((*ppTopologyDimm)[Index].DeviceLocator));
        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_WARN("Failed to retrieve attribute pDmiPhysicalDev->Type17->DeviceLocator (%r)", ReturnCode);
        }
        ReturnCode = GetSmbiosString((SMBIOS_STRUCTURE_POINTER *) &SmBiosStruct.Type17,
              SmBiosStruct.Type17->BankLocator, (*ppTopologyDimm)[Index].BankLabel,
              sizeof((*ppTopologyDimm)[Index].BankLabel));
        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_WARN("Failed to retrieve attribute pDmiPhysicalDev->Type17->BankLocator (%r)", ReturnCode);
        }
        // override types to keep consistency with dimm_info values
        if (SmBiosStruct.Type17->MemoryType == SMBIOS_MEMORY_TYPE_DDR4) {
          (*ppTopologyDimm)[Index].MemoryType = MEMORYTYPE_DDR4;
        } else if (SmBiosStruct.Type17->MemoryType == SMBIOS_MEMORY_TYPE_DCPMEM) {
          (*ppTopologyDimm)[Index].MemoryType = MEMORYTYPE_DCPMEM;
        } else {
          (*ppTopologyDimm)[Index].MemoryType = MEMORYTYPE_UNKNOWN;
        }
        Index++;
        (*pTopologyDimmsNumber) = Index;
      }
    } else {
      NVDIMM_DBG("SmBios entry has invalid pointers set");
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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT8 DimmARSStatus = ARS_STATUS_UNKNOWN;
  UINT8 ARSStatusBitmask = 0;

  NVDIMM_ENTRY();
  if (pThis == NULL || pARSStatus == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pARSStatus = ARS_STATUS_NOT_STARTED;

  LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    if (pDimm->PmCapacity > 0) {
      ReturnCode = FwCmdGetARS(pDimm, &DimmARSStatus);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("FwCmdGetARS failed with error %r for DIMM 0x%x", ReturnCode, pDimm->DeviceHandle.AsUint32);
      }

      switch(DimmARSStatus) {
        case ARS_STATUS_IN_PROGRESS:
          *pARSStatus = ARS_STATUS_IN_PROGRESS;
          goto Finish;
          break;
        case ARS_STATUS_UNKNOWN:
          ARSStatusBitmask |= ARS_STATUS_MASK_UNKNOWN;
          break;
        case ARS_STATUS_COMPLETED:
          ARSStatusBitmask |= ARS_STATUS_MASK_COMPLETED;
          break;
        case ARS_STATUS_NOT_STARTED:
          ARSStatusBitmask |= ARS_STATUS_MASK_NOT_STARTED;
          break;
        case ARS_STATUS_ABORTED:
          ARSStatusBitmask |= ARS_STATUS_MASK_ABORTED;
          break;
        default:
          ARSStatusBitmask |= ARS_STATUS_MASK_UNKNOWN;
          break;
      }
    }
  }

  if ((ARSStatusBitmask & ARS_STATUS_MASK_UNKNOWN) != 0) {
    *pARSStatus = ARS_STATUS_UNKNOWN;
  } else if ((ARSStatusBitmask & ARS_STATUS_MASK_ABORTED) != 0) {
    *pARSStatus = ARS_STATUS_ABORTED;
  } else if ((ARSStatusBitmask & ARS_STATUS_MASK_COMPLETED) != 0) {
    *pARSStatus = ARS_STATUS_COMPLETED;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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

  VariableSize = sizeof(pDriverPreferences->AppDirectGranularity);
  ReturnCode = SET_VARIABLE_NV(
    APPDIRECT_GRANULARITY_VARIABLE_NAME,
    gNvmDimmNgnvmVariableGuid,
    VariableSize,
    &pDriverPreferences->AppDirectGranularity);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to set AppDirect Granularity Variable");
    ResetCmdStatus(pCommandStatus, NVM_ERR_OPERATION_FAILED);
    goto Finish;
  }

  gNvmDimmData->Alignments.RegionPartitionAlignment = ConvertAppDirectGranularityPreference(pDriverPreferences->AppDirectGranularity);

  ResetCmdStatus(pCommandStatus, NVM_SUCCESS);

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

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
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  PT_OUTPUT_PAYLOAD_GET_DDRT_IO_INIT_INFO DdrtIoInitInfo;
  BOOLEAN Smbus = FALSE;

  NVDIMM_ENTRY();

  ZeroMem(&DdrtIoInitInfo, sizeof(DdrtIoInitInfo));

  pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL) {
    pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.UninitializedDimms);
    Smbus = TRUE;
    if (pDimm == NULL) {
      goto Finish;
    }
  }

  if (!IsDimmManageable(pDimm)) {
    goto Finish;
  }

  ReturnCode = FwCmdGetDdrtIoInitInfo(pDimm, Smbus, &DdrtIoInitInfo);
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

  if (pOpcode != NULL) {
    *pOpcode = LongOpStatus.CmdOpcode;
  }

  if (pSubOpcode != NULL) {
    *pSubOpcode = LongOpStatus.CmdSubcode;
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
  UINT8 DimmSecurityState = 0;
  UINT32 Index = 0;

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

  // Check for security
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0, FALSE, ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &DimmSecurityState);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if (!IsConfiguringForCreateGoalAllowed(DimmSecurityState)) {
      ReturnCode = EFI_ACCESS_DENIED;
      ResetCmdStatus(pCommandStatus, NVM_ERR_CREATE_GOAL_NOT_ALLOWED);
      NVDIMM_DBG("Invalid request to create goal while security is enabled.");
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
  GetRegionCount(&gNvmDimmDriverNvmDimmConfig, &RegionCount);

  pRegions = AllocateZeroPool(sizeof(REGION_INFO) * RegionCount);
  if (pRegions == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  ReturnCode = GetRegions(&gNvmDimmDriverNvmDimmConfig, RegionCount, pRegions, pCommandStatus);

  for (Index = 0; Index < RegionCount; Index++) {
    // Check if Region is empty
    if (pRegions[Index].Capacity != pRegions[Index].FreeCapacity) {
      NVDIMM_DBG("Region %d is not empty. Skip automatic namespace provision", pRegions[Index].RegionId);
      // No isets to provision is success
      ReturnCode = EFI_SUCCESS;
      continue;
    }
      ReturnCode = CreateNamespace(&gNvmDimmDriverNvmDimmConfig,
                                 pRegions[Index].RegionId,          // Iterate through Regions
                                   DIMM_PID_NOTSET,                   // Use all DIMMs
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
  Retrives Intel Dimm Config EFI vars

  User is responsible for freeing ppIntelDIMMConfig

  @param[out] pIntelDIMMConfig Pointer to struct to fill with EFI vars

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
RetrieveIntelDIMMConfig(
     OUT INTEL_DIMM_CONFIG **ppIntelDIMMConfig
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINTN VariableSize = 0;

  NVDIMM_ENTRY();

  *ppIntelDIMMConfig = AllocateZeroPool(sizeof(INTEL_DIMM_CONFIG));
  if (*ppIntelDIMMConfig == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  VariableSize = sizeof(INTEL_DIMM_CONFIG);
  ReturnCode = GET_VARIABLE(
    INTEL_DIMM_CONFIG_VARIABLE_NAME,
    gIntelDimmConfigVariableGuid,
    &VariableSize,
    *ppIntelDIMMConfig);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not find IntelDIMMConfigs");
    FREE_POOL_SAFE(*ppIntelDIMMConfig);
    goto Finish;
  }

  NVDIMM_DBG("Revision: %d", (*ppIntelDIMMConfig)->Revision);
  NVDIMM_DBG("ProvisionCapacityMode: %d", (*ppIntelDIMMConfig)->ProvisionCapacityMode);
  NVDIMM_DBG("MemorySize: %d", (*ppIntelDIMMConfig)->MemorySize);
  NVDIMM_DBG("PMType: %d", (*ppIntelDIMMConfig)->PMType);
  NVDIMM_DBG("ProvisionNamespaceMode: %d", (*ppIntelDIMMConfig)->ProvisionNamespaceMode);
  NVDIMM_DBG("NamespaceFlags: %d", (*ppIntelDIMMConfig)->NamespaceFlags);
  NVDIMM_DBG("ProvisionCapacityStatus: %d", (*ppIntelDIMMConfig)->ProvisionCapacityStatus);
  NVDIMM_DBG("ProvisionNamespaceStatus: %d", (*ppIntelDIMMConfig)->ProvisionNamespaceStatus);
  NVDIMM_DBG("NamespaceLabelVersion: %d", (*ppIntelDIMMConfig)->NamespaceLabelVersion);

Finish:
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
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0, FALSE, ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], &pConfHeader);
#ifdef MEMORY_CORRUPTION_WA
  if (ReturnCode == EFI_DEVICE_ERROR) {
		ReturnCode = GetPlatformConfigDataOemPartition(ppDimms[Index], &pConfHeader);
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
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  UINT32 DimmsNumUninitialized = 0;
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

  // Check if any DIMMs are uninitialized
  // Can't check topology if a DIMM is non-functional
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0, TRUE, ppDimms, &DimmsNumUninitialized,
    pCommandStatus);
  if (ReturnCode != EFI_NOT_FOUND) {
      NVDIMM_DBG("Uninitialized DIMM found. Aborting auto conf.");
      ReturnCode = EFI_ABORTED;
      goto Finish;
  }

  // Get all DIMMs
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0, FALSE, ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (ppDimms[Index]->ConfigStatus != DIMM_CONFIG_SUCCESS) {
      if ((ppDimms[Index]->ConfigStatus == DIMM_CONFIG_IS_INCOMPLETE) ||
          (ppDimms[Index]->ConfigStatus == DIMM_CONFIG_NO_MATCHING_IS) ||
          (ppDimms[Index]->ConfigStatus == DIMM_CONFIG_NEW_DIMM)) {
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
  FREE_POOL_SAFE(ppDimms);
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
  ReturnCode = VerifyTargetDimms(NULL, 0, NULL, 0, FALSE, ppDimms, &DimmsNum, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = RetrieveGoalConfigsFromPlatformConfigData(&gNvmDimmData->PMEMDev.Dimms);
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

  @param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
  @param[in] pDimmIds - pointer to array of UINT16 Dimm ids to get data for
  @param[in] DimmIdsCount - number of elements in pDimmIds

  @param[IN] ErrorInjType - Error Inject type
  @param[IN] ClearStatus - Is clear status set
  @param[IN] pInjectTemperatureValue - Pointer to inject temperature
  @param[IN] pInjectPoisonAddress - Pointer to inject poison address
  @param[IN] pPoisonType - Pointer to poison type
  @param[IN] pPercentageremaining - Pointer to percentage remaining
  @param[out] pCommandStatus Structure containing detailed NVM error codes.

  @retval EFI_UNSUPPORTED Mixed Sku of DCPMEM modules has been detected in the system
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
EFIAPI
InjectError(
    IN     EFI_DCPMM_CONFIG_PROTOCOL *pThis,
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
    UINT8 SecurityState = SECURITY_UNKNOWN;
    PT_PAYLOAD_GET_PACKAGE_SPARING_POLICY *pPayloadPackageSparingPolicy = NULL;

    SetMem(pDimms, sizeof(pDimms), 0x0);

    NVDIMM_ENTRY();

    if (pThis == NULL || pCommandStatus == NULL || (pDimmIds == NULL && DimmIdsCount > 0)) {
      ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_PARAMETER);
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }

    pCommandStatus->ObjectType = ObjectTypeDimm;

    ReturnCode = VerifyTargetDimms(pDimmIds, DimmIdsCount, NULL, 0, FALSE, pDimms, &DimmsNum, pCommandStatus);
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
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopMediaErrorTemperature, (VOID *)pInputPayload);
        if (EFI_ERROR(ReturnCode)) {
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
        if (pDimms[Index]->SkuInformation.PackageSparingCapable &&
          pPayloadPackageSparingPolicy->Supported && pPayloadPackageSparingPolicy->Enable) {
          ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, (VOID *)pInputPayload);
          if (EFI_ERROR(ReturnCode)) {
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
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, pInputPayload);
        if (EFI_ERROR(ReturnCode)) {
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
      ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, (VOID *) pInputPayload);
      if (EFI_ERROR(ReturnCode)) {
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
        ReturnCode = EFI_DEVICE_ERROR;
        continue;
      }
      SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
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
        if (SecurityState == SECURITY_LOCKED) {
          NVDIMM_DBG("Invalid security check- poison inject error cannot be applied");
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_UNABLE_TO_GET_SECURITY_STATE);
          ReturnCode = EFI_INVALID_PARAMETER;
          continue;
        }
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopErrorPoison, (VOID *) pInputPayload);
        if (EFI_ERROR(ReturnCode)) {
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
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->TriggersToModify = SPARE_BLOCK_PERCENTAGE_TRIGGER;
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->SpareBlockPercentageTrigger.Separated.Enable = !ClearStatus;
      ((PT_INPUT_PAYLOAD_INJECT_SW_TRIGGERS *)pInputPayload)->SpareBlockPercentageTrigger.Separated.Value = *pPercentageRemaining;
      for (Index = 0; Index < DimmsNum; Index++) {
        ReturnCode = FwCmdInjectError(pDimms[Index], SubopSoftwareErrorTriggers, (VOID *)pInputPayload);
        if (EFI_ERROR(ReturnCode)) {
          SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_ERR_OPERATION_FAILED);
          ReturnCode = EFI_DEVICE_ERROR;
          continue;
        }
        SetObjStatusForDimm(pCommandStatus, pDimms[Index], NVM_SUCCESS);
      }
    }
Finish:
  FREE_POOL_SAFE(pInputPayload);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
GetBsr value and return bsr or bootstatusbitmask depending on the requested options
UEFI - Read directly from BSR register
OS - Get BSR value from BIOS emulated command
@param[in] pThis A pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[in] DimmID -  dimm handle of the DIMM
@param[out] pBsrValue - pointer to  BSR register value OPTIONAL
@param[out] pBootStatusBitMask  - pointer to bootstatusbitmask OPTIONAL

@retval EFI_INVALID_PARAMETER passed NULL argument
@retval EFI_SUCCESS Success
@retval Other errors failure of FW commands
**/
EFI_STATUS
EFIAPI
GetBSRAndBootStatusBitMask(
  IN      EFI_DCPMM_CONFIG_PROTOCOL *pThis,
  IN      UINT16 DimmID,
  OUT     UINT64 *pBsrValue OPTIONAL,
  OUT     UINT16 *pBootStatusBitmask OPTIONAL
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  DIMM_BSR Bsr;
  NVDIMM_ENTRY();
  ZeroMem(&Bsr, sizeof(DIMM_BSR));
  pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.Dimms);
#ifndef OS_BUILD
  if (pDimm != NULL && pDimm->pHostMailbox != NULL) {
    Bsr.AsUint64 = *pDimm->pHostMailbox->pBsr;
  } else {
    goto Finish;
  }
#endif // !OS_BUILD

  if (pDimm == NULL) {
    pDimm = GetDimmByPid(DimmID, &gNvmDimmData->PMEMDev.UninitializedDimms);
#ifndef OS_BUILD
    if (pDimm != NULL) {
      LIST_ENTRY *pCurDimmInfoNode = NULL;
      for (pCurDimmInfoNode = GetFirstNode(&gNvmDimmData->PMEMDev.UninitializedDimms);
        !IsNull(&gNvmDimmData->PMEMDev.UninitializedDimms, pCurDimmInfoNode);
        pCurDimmInfoNode = GetNextNode(&gNvmDimmData->PMEMDev.UninitializedDimms, pCurDimmInfoNode)) {

        if (DimmID == ((DIMM_INFO *)pCurDimmInfoNode)->DimmID) {
          break;
        }
      }
      if (NULL != pCurDimmInfoNode) {
        ReturnCode = SmbusGetBSR(((DIMM_INFO *)pCurDimmInfoNode)->SmbusAddress, &Bsr);
        if (EFI_ERROR(ReturnCode)) {
          goto Finish;
        }
      }
    }
#endif // !OS_BUILD
  }
  if (pDimm == NULL) {
    goto Finish;
  }
#ifdef OS_BUILD
  ReturnCode = FwCmdGetBsr(pDimm, &Bsr.AsUint64);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
#endif
  if (pBootStatusBitmask != NULL) {
    ReturnCode = PopulateDimmBootStatusBitmask(&Bsr, pDimm, pBootStatusBitmask);
  }
  if (pBsrValue != NULL) {
    // If Bsr value is MAX_UINT64_VALUE, then it is access violation
    if (Bsr.AsUint64 == MAX_UINT64_VALUE) {
      goto Finish;
    }
    *pBsrValue = Bsr.AsUint64;
  }
  ReturnCode = EFI_SUCCESS;
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#ifdef __MFG__
/**
Update from mfg to prod firmware

@param[in] pThis is a pointer to the EFI_DCPMM_CONFIG_PROTOCOL instance.
@param[in] pDimmIds is a pointer to an array of DIMM IDs - if NULL, execute operation on all dimms
@param[in] DimmIdsCount Number of items in array of DIMM IDs
@param[in] pFileName Name is a pointer to a file containing FW image
@param[in] IsFWUpdate - set to 1 if doing FW update else 0
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
  IN     BOOLEAN   IsFWUpdate,
  OUT COMMAND_STATUS *pCommandStatus
)
{
    EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

    ReturnCode  = InjectAndUpdateToProdfw(pThis, pDimmIds, DimmIdsCount, pFileName, IsFWUpdate, pCommandStatus);

    return ReturnCode;
}

#endif /*__MFG__*/

