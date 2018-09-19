/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <NvmTables.h>
#include "NvmDimmDriver.h"
#include "NvmDimmConfig.h"
#include "NvmDimmDriverData.h"
#include <NvmDimmPassThru.h>
#include "NvmHealth.h"
#include <Version.h>
#include <Debug.h>
#include <IndustryStandard/Pci.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/StorageSecurityCommand.h>
#include <Namespace.h>
#include <Dimm.h>
#include <Convert.h>
#include <Protocol/NvdimmLabel.h>
#ifndef OS_BUILD
#include <Smbus.h>
#endif

#if _BullseyeCoverage
extern int cov_dumpData(void);
#endif

#define FIRST_ERR(rc, newRc) { if (rc == EFI_SUCCESS) rc = newRc; }

EFI_SYSTEM_TABLE *gSystemTable = NULL;

EFI_GUID gNfitBindingProtocolGuid =
  { 0x97B4FA0C, 0x4D7E, 0xC2D0, { 0x67, 0x8E, 0xFB, 0x92, 0xE9, 0x6D, 0x2C, 0xC2 }};

EFI_GUID gNvmDimmNgnvmGuid = NVMDIMM_DRIVER_NGNVM_GUID;

EFI_GUID gIntelDimmConfigVariableGuid = INTEL_DIMM_CONFIG_VARIABLE_GUID;

/**
  Adds the DIMM name for the use of the ComponentName protocols.

  @param[in] DimmIndex, the index of the DIMM that the caller wants to register the name for.

  @retval EFI_SUCCESS if the name was added successfully
  Other return values from the function AddStringToUnicodeTable.
**/
STATIC
EFI_STATUS
RegisterDimmName(
  IN     UINT32 DimmIndex
  );

/**
  Array of dimms UEFI-related data structures.
**/
EFI_DIMMS_DATA gDimmsUefiData[MAX_DIMMS];

/**
  Driver Support EFI Version Protocol instance
**/
GLOBAL_REMOVE_IF_UNREFERENCED
EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL gNvmDimmDriverDriverSupportedEfiVersion = {
  sizeof(EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL),
  EFI_2_31_SYSTEM_TABLE_REVISION
};

/**
  Driver Binding Protocol instance
**/
EFI_DRIVER_BINDING_PROTOCOL gNvmDimmDriverDriverBinding = {
  NvmDimmDriverDriverBindingSupported, NvmDimmDriverDriverBindingStart,
  NvmDimmDriverDriverBindingStop, NVMDIMM_VERSION, NULL, NULL
};

/**
  Data structure
**/
NVMDIMMDRIVER_DATA *gNvmDimmData = NULL;

/**
  Driver specific Vendor Device Path definition.
**/
VENDOR_END_DEVICE_PATH gNvmDimmDriverDevicePath = { { {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP, {
        (UINT8) (sizeof(VENDOR_DEVICE_PATH)),
        (UINT8) ((sizeof(VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    NVMDIMM_DRIVER_DEVICE_PATH_GUID
  }, {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE, {
      (UINT8) (END_DEVICE_PATH_LENGTH),
      (UINT8) ((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

/**
  DIMM device path.
  For each DIMM the NFIT device handle should be modified to distinct them.
**/
ACPI_NVDIMM_DEVICE_PATH gNvmDimmDevicePathNode = {
    {
      ACPI_DEVICE_PATH,
      ACPI_NVDIMM_DP, {
        (UINT8) (sizeof(ACPI_NVDIMM_DEVICE_PATH)),
        (UINT8) ((sizeof(ACPI_NVDIMM_DEVICE_PATH)) >> 8)
      }
    },
    0 // Device handle to be set for each DIMM
};

/**
  Function tries to remove all of the block namespaces protocols, then it
  removes all of the enumerated namespaces from the LSA and also the regions.

  @retval EFI_SUCCESS the object were cleared successfully
  Error return codes from CleanBlockNamespaces function
**/
EFI_STATUS
CleanNamespacesAndISs(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  ReturnCode = CleanNamespaces();
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  CleanNamespacesList(&gNvmDimmData->PMEMDev.Namespaces);

  /**
    Remove Interleave Sets
  **/
  CleanISLists(&gNvmDimmData->PMEMDev.Dimms, &gNvmDimmData->PMEMDev.ISs);
  gNvmDimmData->PMEMDev.RegionsAndNsInitialized = FALSE;
Finish:
#endif
  return ReturnCode;
}

/**
  Function that allows to "refresh" the existing DIMMs.
  If a DIMM is unaccessible, all of the ISs and namespaces
  that it was a part of will be removed.

  @retval EFI_SUCCESS if all DIMMs are working.
  @retval EFI_INVALID_PARAMETER if any of pointer parameters in NULL
  @retval EFI_ABORTED if at least one DIMM is not responding.
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
EFI_STATUS
ReenumerateNamespacesAndISs(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  NVDIMM_ENTRY();

  ReturnCode = CleanNamespacesAndISs();
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to clean namespaces and pools");
  }
  /** Initialize Interleave Sets **/
  ReturnCode = InitializeISs(gNvmDimmData->PMEMDev.pFitHead,
    &gNvmDimmData->PMEMDev.Dimms, &gNvmDimmData->PMEMDev.ISs);
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      goto Finish;
    }
    NVDIMM_WARN("Failed to retrieve the Interleave Set and Region list, error = " FORMAT_EFI_STATUS ".", ReturnCode);
  }

  /** Initialize Namespaces (read LSA, enumerate every namespace) **/
  ReturnCode = InitializeNamespaces();
  if (EFI_ERROR(ReturnCode)) {
    if (EFI_NO_RESPONSE == ReturnCode) {
      goto Finish;
    }
    NVDIMM_WARN("Failed to re-initialize namespaces, error = " FORMAT_EFI_STATUS ".", ReturnCode);
  }

  /** Install block and device path protocols on Namespaces **/
  ReturnCode = InstallProtocolsOnNamespaces();
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to install protocols on namespaces, error = " FORMAT_EFI_STATUS ".", ReturnCode);
  }
  gNvmDimmData->PMEMDev.RegionsAndNsInitialized = TRUE;
  NVDIMM_EXIT_I64(ReturnCode);
Finish:
#endif
  return ReturnCode;
}

/**
  Unload the driver
**/
EFI_STATUS
EFIAPI
NvmDimmDriverUnload(
  IN     EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  EFI_HANDLE *pHandleBuffer = NULL;
  UINTN HandleCount = 0;
  UINTN Index = 0;
  CONST BOOLEAN DriverAlreadyUnloaded = (gNvmDimmData == NULL);

  NVDIMM_ENTRY();

  /** Retrieve array of all handles in the handle database **/
  ReturnCode = gBS->LocateHandleBuffer(AllHandles, NULL, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to disconnect the driver from the handles. LocateHandleBuffer failed, error = " FORMAT_EFI_STATUS ".", ReturnCode);
  } else {
    /** Disconnect the current driver from any devices that might be still controlled **/
    for (Index = 0; Index < HandleCount; Index++) {
      ReturnCode = EfiTestManagedDevice(pHandleBuffer[Index],
              gNvmDimmDriverDriverBinding.DriverBindingHandle,
              &gEfiDevicePathProtocolGuid);

      // If the handle is managed - disconnect it.
      if (!EFI_ERROR(ReturnCode)) {
        gBS->DisconnectController(pHandleBuffer[Index], ImageHandle, NULL);
      }
    }
    ReturnCode = EFI_SUCCESS;
    /** Free the array of handles **/
    if (pHandleBuffer) {
      FreePool(pHandleBuffer);
    }
  }

  /** Uninstall protocols installed in the driver entry point **/
  TempReturnCode = gBS->UninstallMultipleProtocolInterfaces(ImageHandle,
      &gEfiDriverBindingProtocolGuid, &gNvmDimmDriverDriverBinding,
      &gEfiComponentNameProtocolGuid, &gNvmDimmDriverComponentName,
      &gEfiDriverDiagnosticsProtocolGuid, &gNvmDimmDriverDriverDiagnostics,
      NULL);
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_DBG("Failed to uninstall driver entry protocols, error = " FORMAT_EFI_STATUS ".", TempReturnCode);
  }

  /**  remove these separately as a workaround for systems not supporting the correct version of UEFI **/
  TempReturnCode = gBS->UninstallMultipleProtocolInterfaces(ImageHandle,
      &gEfiComponentName2ProtocolGuid, &gNvmDimmDriverComponentName2,
      &gEfiDriverDiagnostics2ProtocolGuid, &gNvmDimmDriverDriverDiagnostics2,
      NULL);
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_DBG("Failed to uninstall driver entry protocols 2, error = " FORMAT_EFI_STATUS ".", TempReturnCode);
  }

  /** Uninstall Driver Supported EFI Version Protocol **/
  TempReturnCode = gBS->UninstallMultipleProtocolInterfaces(ImageHandle,
      &gEfiDriverSupportedEfiVersionProtocolGuid,
      &gNvmDimmDriverDriverSupportedEfiVersion, NULL);
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_DBG("Failed to uninstall the DriverSupportedEfiVersion protocol, error = " FORMAT_EFI_STATUS ".", TempReturnCode);
  }

  /** Uninstall Driver Health Protocol **/
  TempReturnCode = gBS->UninstallMultipleProtocolInterfaces(ImageHandle,
      &gEfiDriverHealthProtocolGuid, &gNvmDimmDriverHealth, NULL);
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_DBG("Failed to uninstall the DriverHealth protocol, error = 0x%llx.", TempReturnCode);
  }

  /**
    clean up data struct
  **/
  FREE_POOL_SAFE(gNvmDimmData);

#if !defined(MDEPKG_NDEBUG) && !defined(_MSC_VER)
  /** Disable recording AllocatePool and FreePool occurrences, print list and clear it **/
  FlushPointerTrace((CHAR16 *)__WFUNCTION__);
#endif

  if (EFI_ERROR(ReturnCode) && DriverAlreadyUnloaded) {
    NVDIMM_WARN("The driver was not properly initialized or was unloaded before, error = " FORMAT_EFI_STATUS ".", TempReturnCode);
    NVDIMM_DBG("Overriding the return code to SUCCESS");
    ReturnCode = EFI_SUCCESS;
  }
#endif
  NVDIMM_DBG("Exiting DriverUnload, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function reads the driver workarounds flags from the
  shell variable and sets the proper flags or values in the driver.

  This function exists only in the debug version of the driver.
**/
#if defined(DYNAMIC_WA_ENABLE)
STATIC
VOID
InitWorkarounds(
  )
{
  CHAR16 *pShellVar = NULL;
  CHAR16 **ppShellVarSplit = NULL;
  UINT32 ShellTokensCount = 0;
  UINT32 Index = 0;

  pShellVar = GetEnvVariable(DYNAMIC_WA_ENV_VARIABLE_NAME);

  if (pShellVar != NULL) {
    if (NULL == (ppShellVarSplit = StrSplit(pShellVar, L',', &ShellTokensCount))) {
      return;
    }

    for (Index = 0; Index < ShellTokensCount; Index++) {
      if (CompareMem(ppShellVarSplit[Index], WA_FLAG_ALWAYS_LOAD, sizeof(WA_FLAG_ALWAYS_LOAD)) == 0) {
        Print(L"INFO: 'Always load' workaround enabled in the driver.\n");
        gNvmDimmData->AlwaysLoadDriver = TRUE;
        continue;
      }

      if (CompareMem(ppShellVarSplit[Index], WA_FLAG_SIMICS, sizeof(WA_FLAG_SIMICS)) == 0) {
        Print(L"INFO: 'Simics' workaround enabled in the driver.\n");
        gNvmDimmData->SimicsWorkarounds = TRUE;
        continue;
      }

      if (CompareMem(ppShellVarSplit[Index],
          WA_FLAG_UNLOAD_OTHER_DRIVERS, sizeof(WA_FLAG_UNLOAD_OTHER_DRIVERS)) == 0) {
        Print(L"INFO: 'Unload loaded drivers before load' workaround enabled in the driver.\n");
        gNvmDimmData->UnloadExistingDrivers = TRUE;
        continue;
      }

      if (CompareMem(ppShellVarSplit[Index], WA_FLAG_IGNORE_UID_NUMS, sizeof(WA_FLAG_IGNORE_UID_NUMS)) == 0) {
        Print(L"INFO: Ignoring the same NVDIMM UID numbers workaround enabled in the driver.\n");
        gNvmDimmData->IgnoreTheSameUIDNumbers = TRUE;
        continue;
      }

      if (CompareMem(ppShellVarSplit[Index], WA_FLAG_NO_PCD_INIT, sizeof(WA_FLAG_NO_PCD_INIT)) == 0) {
        Print(L"INFO: Disable PCD read during initialization workaround enabled in the driver.\n");
        gNvmDimmData->PcdUsageDisabledOnInit = TRUE;
        continue;
      }
    }
    FreeStringArray(ppShellVarSplit, ShellTokensCount);
  }
}

/**
  Finds a driver handle for the given driver name keywords.

  The driver is identified by the ConponentNameProtocol->GetDriverName, so it is required that the requested
  driver implements this protocol.

  The input driver name should be a comma separated list of the words that exist in the name. The function assumes
  that the driver loaded in the system has one additional word in the name which is the driver version etc.
  The function is case sensitive but it does not care about the order of the words in the driver.

  The driver name in the ComponentNameProtocol should be delimited with spaces.

  Example usage:
  pDriverName = L"XXX,YYY,Driver"

  Will return a handle if in the system there is a driver loaded with names like:
  "XXX YYY 1.0.0.0 Driver", "YYY XXX 1.0.0.0 Driver", "XXX YYY Test123 Driver",
  "Driver XXX YYY Abc Driver", "YYY Driver Apa XXX", "Test123 XXX Driver YYY"

  In case of more than one match in the system, the first handle will be returned.
**/
STATIC
VOID
FindDriverByComponentName(
  IN     CONST CHAR16 *pDriverName,
     OUT EFI_HANDLE *pDriverHandle
  )
{
#ifndef OS_BUILD
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN HandleCount = 0;
  EFI_HANDLE *pHandleBuffer = NULL;
  UINT32 Index = 0;
  EFI_COMPONENT_NAME_PROTOCOL *pComponentName = NULL;
  CHAR16 *pCurrentDriverName = NULL;
  CHAR16 **ppSourceNameTokens = NULL;
  CHAR16 **ppCurrentNameTokens = NULL;
  UINT32 SourceNameTokenCount = 0;
  UINT32 CurrentNameTokenCount = 0;
  UINT32 TokensFound = 0;
  UINT32 Index2 = 0;
  UINT32 Index3 = 0;
  UINTN StringLength = 0; // The StrLen returns the length in UINTN

  NVDIMM_ENTRY();

  if (pDriverHandle == NULL || pDriverHandle == NULL) {
    NVDIMM_DBG("Error in the provided parameters.");
    goto Finish;
  }

  *pDriverHandle = NULL;

  /**
    Find the driver handle by searching for the component name protocol
  **/
  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, &gEfiComponentNameProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(ReturnCode) || HandleCount < 1) {
    NVDIMM_WARN("Failed to find any drivers with the component name installed, error = " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

  ppSourceNameTokens = StrSplit((CHAR16 *)pDriverName, L',', &SourceNameTokenCount);

  if (ppSourceNameTokens == NULL) {
    NVDIMM_DBG("Failed to parse the driver name.");
    goto Finish;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    ReturnCode = gBS->OpenProtocol(
      pHandleBuffer[Index],
      &gEfiComponentNameProtocolGuid,
      (VOID **)&pComponentName,
      NULL,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to open the Component Name protocol, error = " FORMAT_EFI_STATUS "", ReturnCode);
      continue;
    }

    ReturnCode = pComponentName->GetDriverName(pComponentName, "eng", &pCurrentDriverName);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not get the driver name, error = " FORMAT_EFI_STATUS "", ReturnCode);
      continue;
    }

    ppCurrentNameTokens = StrSplit(pCurrentDriverName, L' ', &CurrentNameTokenCount);

    if (ppCurrentNameTokens == NULL) {
      NVDIMM_DBG("Could not parse the driver name.");
      continue;
    }

    if ((SourceNameTokenCount + 1) != CurrentNameTokenCount) {
      /**
        The token number should be the source +1 (the additional one is the version number
        that will be different, so we skip it in the search but must consider in the numbers
      **/
      FreeStringArray(ppCurrentNameTokens, CurrentNameTokenCount);
      continue; // lets skip to the next driver
    }
    TokensFound = 0;

    for (Index2 = 0; Index2 < CurrentNameTokenCount; Index2++) {
      for (Index3 = 0; Index3 < SourceNameTokenCount; Index3++) {
        StringLength = StrLen(ppCurrentNameTokens[Index2]);
        if (StringLength > StrLen(ppSourceNameTokens[Index3])) {
          StringLength = StrLen(ppSourceNameTokens[Index3]);
        }
        if (CompareMem(ppCurrentNameTokens[Index2], ppSourceNameTokens[Index3], StringLength) == 0) {
          TokensFound++;
        }
      }
    }

    if (TokensFound == SourceNameTokenCount) {
      // Success, we assign the return value, clean the memory and leave the loop
      *pDriverHandle = pHandleBuffer[Index];
      FreeStringArray(ppCurrentNameTokens, CurrentNameTokenCount);
      break;
    }
    FreeStringArray(ppCurrentNameTokens, CurrentNameTokenCount);
  }
  FreeStringArray(ppSourceNameTokens, SourceNameTokenCount);

Finish:
  FREE_POOL_SAFE(pHandleBuffer);
  NVDIMM_EXIT();
#endif
}

/**
  Searches for currently loaded base and HII drivers

  If it finds them, they get unloaded.

  @param None
  @retval None
**/
STATIC
VOID
UnloadDcpmmDriversIfAny(
  )
{
  CONST CHAR16 *pIpmctlDriverWordsToFind = PMEM_MODULE_NAME_SEARCH L",Driver";
  CONST CHAR16 *pIpmctlHiiDriverWordsToFind = PMEM_MODULE_NAME_SEARCH L",HII,Driver";
  EFI_HANDLE DriverHandle = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  FindDriverByComponentName(pIpmctlDriverWordsToFind, &DriverHandle);

  if (DriverHandle != NULL) {
    ReturnCode = gBS->UnloadImage(DriverHandle);
    if (!EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Successfully unloaded the base driver.\n");
    } else {
      NVDIMM_WARN("Error while unloading the base driver, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
    }
  } else {
    NVDIMM_DBG("Base driver not detected in the system.\n");
  }

  FindDriverByComponentName(pIpmctlHiiDriverWordsToFind, &DriverHandle);

  if (DriverHandle != NULL) {
    ReturnCode = gBS->UnloadImage(DriverHandle);
    if (!EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Successfully unloaded the HII driver.\n");
    } else {
      NVDIMM_WARN("Error while unloading the HII driver, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
    }
  } else {
    NVDIMM_DBG("HII driver not detected in the system.\n");
  }

}

#endif /** DYNAMIC_WA_ENABLE **/

/**
  Driver entry point
**/
EFI_STATUS
EFIAPI
NvmDimmDriverDriverEntryPoint(
  IN     EFI_HANDLE ImageHandle,
  IN     EFI_SYSTEM_TABLE *pSystemTable
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_LOADED_IMAGE_PROTOCOL *pLoadedImage = NULL;
  EFI_HANDLE ExistingDriver = NULL;
  DRIVER_PREFERENCES DriverPreferences;

  NVDIMM_ENTRY();

  /**
    This is the sample usage of the OutputCheckpoint function.
    The minor and major codes are custom. The BIOS scratchpad must be set to this value before the code gets there.
    Then the code will freeze until the value in the BIOS scratchpad will be changed.
  **/
  //OutputCheckpoint(0x7d,0x00);

  /** Print runtime function address to ease calculation of GDB symbol loading offset. **/
  NVDIMM_DBG_CLEAN("NvmDimmDriverDriverEntryPoint=0x%x\n", (UINT64)&NvmDimmDriverDriverEntryPoint);

  ZeroMem(&DriverPreferences, sizeof(DriverPreferences));

  gSystemTable = pSystemTable;

  /**
    We need to set the Driver Binding Image handle.
    The other handle is set by EfiLibInstallAllDriverProtocols2 function.
  **/

  gNvmDimmDriverDriverBinding.ImageHandle = ImageHandle;

  /**
    Set up the driver data
  **/
  gNvmDimmData = AllocateZeroPool(sizeof(NVMDIMMDRIVER_DATA));
  if (gNvmDimmData == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    NVDIMM_ERR("Unable to allocate NvmDimmData, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
    goto Finish;
  }

  gNvmDimmData->DriverHandle = ImageHandle;

  ReturnCode = ReadRunTimeDriverPreferences(&DriverPreferences);
  if (EFI_ERROR(ReturnCode)) {
    gNvmDimmData->Alignments.RegionPartitionAlignment = SIZE_32GB;
  } else {
    gNvmDimmData->Alignments.RegionPartitionAlignment = ConvertAppDirectGranularityPreference(DriverPreferences.AppDirectGranularity);
  }
  gNvmDimmData->Alignments.RegionVolatileAlignment = REGION_VOLATILE_SIZE_ALIGNMENT_B;
  gNvmDimmData->Alignments.RegionPersistentAlignment = REGION_PERSISTENT_SIZE_ALIGNMENT_B;

  gNvmDimmData->Alignments.PmNamespaceMinSize = PM_NAMESPACE_MIN_SIZE;
  gNvmDimmData->Alignments.BlockNamespaceMinSize = BLOCK_NAMESPACE_MIN_SIZE;

#if defined(DYNAMIC_WA_ENABLE)
#ifndef OS_BUILD
  InitWorkarounds();
  if (gNvmDimmData->UnloadExistingDrivers) {
    // Search for the driver being loaded. If it is loaded but not started, the only protocol that can find
    UnloadDcpmmDriversIfAny();
  }
#endif
#endif

  ReturnCode = GetDriverHandle(&gNvmDimmConfigProtocolGuid, &ExistingDriver);

  if (!EFI_ERROR(ReturnCode)) {
#ifndef OS_BUILD
    NVDIMM_WARN("NvmDimmConfigProtocol already installed, please unload the driver before loading again.");
    ReturnCode = EFI_ALREADY_STARTED;
    goto Finish;
#endif
  }

  /**
    Initialize list heads contained in MEMDev.
  **/
  PMEMDEV_INITIALIZER(&gNvmDimmData->PMEMDev);
#ifndef OS_BUILD
  /**
    Install UEFI Driver Model protocol(s).
  **/
  ReturnCode = EfiLibInstallAllDriverProtocols2(
      ImageHandle,
      pSystemTable,
      &gNvmDimmDriverDriverBinding,
      ImageHandle,
      &gNvmDimmDriverComponentName,
      &gNvmDimmDriverComponentName2,
      NULL,
      NULL,
      &gNvmDimmDriverDriverDiagnostics,
      &gNvmDimmDriverDriverDiagnostics2);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to install the driver protocols, error = 0x%llx.", ReturnCode);
    goto Finish;
  }

  /**
    Install Driver Supported EFI Version Protocol onto ImageHandle.
  **/
  ReturnCode = gBS->InstallMultipleProtocolInterfaces(&ImageHandle,
      &gEfiDriverSupportedEfiVersionProtocolGuid,
      &gNvmDimmDriverDriverSupportedEfiVersion, NULL);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to install the EfiVersionProtocol, error = 0x%llx.", ReturnCode);
    goto Finish;
  }

  /**
    Install Driver Health Protocol onto ImageHandle
  **/
  ReturnCode = gBS->InstallMultipleProtocolInterfaces(
      &ImageHandle,
      &gEfiDriverHealthProtocolGuid, &gNvmDimmDriverHealth,
      NULL);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to install the EfiDriverHealthProtocol, error = 0x%llx.", ReturnCode);
    goto Finish;
  }
#endif // UEFI
Finish:
#ifndef OS_BUILD
  /**
    Install the unload function on the loaded image protocol
  **/
  if (!EFI_ERROR(ReturnCode)) {
    ReturnCode = gBS->OpenProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid,
        (VOID **)&pLoadedImage,
        ImageHandle, ImageHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (!EFI_ERROR(ReturnCode)) {
      pLoadedImage->Unload = NvmDimmDriverUnload;
    }
  } else {  /** clean - call unload manually if we failed to initialize the driver **/
    NvmDimmDriverUnload(ImageHandle);
  }
#endif
  NVDIMM_DBG("Exiting DriverEntryPoint, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function creates a handle with the NFIT Binding Protocol and then calls our binding start function on it.
  It allows us to always load the driver, even if this handle is not properly populated in the system.
**/
#if !defined(MDEPKG_NDEBUG)
#ifndef OS_BUILD
STATIC
#endif // UEFI
EFI_STATUS
ForceStartTheDriver(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_HANDLE FakeBindHandle = NULL;
  /**
    Create a new handle to bind to, so that we are not depended on any existing device handle.
  **/
  ReturnCode = gBS->InstallMultipleProtocolInterfaces(
    &FakeBindHandle,
    &gNfitBindingProtocolGuid,
    NULL,
    NULL);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Could not install the emulated device for binding: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  } else {
    ReturnCode = NvmDimmDriverDriverBindingStart(&gNvmDimmDriverDriverBinding, FakeBindHandle, NULL);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Could not start the driver on the emulated handle: " FORMAT_EFI_STATUS "", ReturnCode);
      goto Finish;
    }
  }

Finish:
  // Set this flag, even if we fail to load -> we just won't load if we failed at the first time
  gNvmDimmData->HandleCreated = TRUE;
  return ReturnCode;
}
#endif

/**
  Tests to see if this driver supports a given controller. If a child device is provided,
  it further tests to see if this driver supports creating a
  handle for the specified child device.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverDriverBindingSupported(
  IN     EFI_DRIVER_BINDING_PROTOCOL *pThis,
  IN     EFI_HANDLE ControllerHandle,
  IN     EFI_DEVICE_PATH_PROTOCOL *pRemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_UNSUPPORTED;
#ifndef OS_BUILD
  VOID *pDummy = NULL;

  //NVDIMM_ENTRY();   //disabled because of flooding

#if !defined(MDEPKG_NDEBUG)
  if (gNvmDimmData->AlwaysLoadDriver) {
    if (!gNvmDimmData->HandleCreated) {
      ReturnCode = ForceStartTheDriver();
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to force start the driver: " FORMAT_EFI_STATUS "", ReturnCode);
      }
    } else {
      // If we made our own handle, we are already bound to it - we should not use it at all
      goto Finish;
    }
  }
#endif

  /**
    Try to open NFIT protocol on this controller handle.
  **/
  ReturnCode = gBS->OpenProtocol(ControllerHandle, &gNfitBindingProtocolGuid,
    (VOID **)&pDummy, pThis->DriverBindingHandle, ControllerHandle,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL);

  if (!EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Detected the Intel NVM Dimm device");

    /**
      If the device path protocol has been already opened - we had already started.
    **/
    ReturnCode = EfiTestManagedDevice(ControllerHandle,
      gNvmDimmDriverDriverBinding.DriverBindingHandle,
      &gNfitBindingProtocolGuid);

    if (!EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_ALREADY_STARTED;
      goto Finish;
    } else {
      ReturnCode = EFI_SUCCESS;
    }
  } else {
    /**
      One way or another - if we had an error, we need to return
      EFI_UNSUPPORTED
    **/
    ReturnCode = EFI_UNSUPPORTED;
  }
  Finish:
#endif
  //NVDIMM_EXIT_I64(ReturnCode);  //disabled because of flooding
  return ReturnCode;
};

/**
  AddStringToUnicodeTable
  Adds the provided string in the "en/eng" language to the provided UNICODE STRING TABLE.

  @param[in] pStringToAdd pointer to the Unicode string to be added.
  @param[in,out] ppTableToAddTo pointer to a pointer to the UNICODE STRING TABLE that the
    string needs to be added.

  @retval EFI_SUCCESS - everything went fine.
  @retval EFI_INVALID_PARAMETER - at least one of the input parameters equals NULL.
  Other return values from the AddUnicodeString2 function.
**/
EFI_STATUS
AddStringToUnicodeTable(
  IN     CHAR16 *pStringToAdd,
  IN OUT EFI_UNICODE_STRING_TABLE **ppTableToAddTo
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  if (pStringToAdd == NULL || ppTableToAddTo == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = AddUnicodeString2(
    "eng",
    gNvmDimmDriverComponentName.SupportedLanguages,
    ppTableToAddTo,
    pStringToAdd,
    TRUE
    );

  if (EFI_ERROR(ReturnCode)) {
    /**
      This is not a critical error - the driver will still work properly.
      We report a warning.
    **/
    NVDIMM_DBG("Failed to register Unicode name, error = " FORMAT_EFI_STATUS "\n", ReturnCode);
  }

  ReturnCode = AddUnicodeString2(
    "en",
    gNvmDimmDriverComponentName2.SupportedLanguages,
    ppTableToAddTo,
    pStringToAdd,
    FALSE
    );

  if (EFI_ERROR(ReturnCode)) {
    /**
      This is not a critical error - the driver will still work properly.
      We report a warning.
    **/
    NVDIMM_DBG("Failed to register DIMM name, error = " FORMAT_EFI_STATUS "\n", ReturnCode);
  }
Finish:
#endif
  return ReturnCode;
}

/**
  RegisterDimmName
  Adds the DIMM name for the use of the ComponentName protocols.

  @param[in] DimmIndex, the index of the DIMM that the caller wants to register the name for.

  @retval EFI_SUCCESS if the name was added successfully
  Other return values from the function AddStringToUnicodeTable.
**/
STATIC
EFI_STATUS
RegisterDimmName(
  IN     UINT32 DimmIndex
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pDimmNameString = NULL;

  NVDIMM_ENTRY();

  pDimmNameString = CatSPrint(NULL, L"Intel Persistent Memory DIMM %d Controller", gDimmsUefiData[DimmIndex].pDimm->DimmID);

  if (pDimmNameString != NULL) {
    ReturnCode = AddStringToUnicodeTable(pDimmNameString, &gDimmsUefiData[DimmIndex].pDimmName);
  } else {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    NVDIMM_WARN("Failed to allocate memory for DIMM name.\n");
  }

  FREE_POOL_SAFE(pDimmNameString);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function makes calls to the dimms required to initialize the driver.

  @retval EFI_SUCCESS if no errors.
  @retval EFI_xxxx depending on error encountered.
**/
EFI_STATUS
InitializeDimms()
{
   EFI_STATUS ReturnCode = EFI_SUCCESS;
   EFI_STATUS ReturnCodeNonBlocking = EFI_SUCCESS;
   UINT32 Index = 0;
   EFI_DEVICE_PATH_PROTOCOL *pTempDevicePathInterface = NULL;
   DIMM *pDimm = NULL;
   DIMM *pDimm2 = NULL;
   LIST_ENTRY *pDimmNode = NULL;
   LIST_ENTRY *pDimmNode2 = NULL;
   BOOLEAN PcdUsage = TRUE;
   CHAR16 Dimm1Uid[MAX_DIMM_UID_LENGTH];
   CHAR16 Dimm2Uid[MAX_DIMM_UID_LENGTH];
   /**
    Init container keeping operation statuses with wanring, error and info level messages.
   **/
   InitErrorAndWarningNvmStatusCodes();

   /**
    enumerate DCPMMs
   **/

   ReturnCodeNonBlocking = FillDimmList();
   if (EFI_ERROR(ReturnCodeNonBlocking)) {
    NVDIMM_WARN("Failed to initialize Dimms, error = " FORMAT_EFI_STATUS ".", ReturnCodeNonBlocking);
   }
#ifndef OS_BUILD
   ReturnCodeNonBlocking = SmbusInit();
   if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed on Smbus init, error = " FORMAT_EFI_STATUS ".", ReturnCodeNonBlocking);
   }

   // For right now, this only fills in additional smbus information for
   // uninitialized dimms listed in the NFIT *only* (not any that aren't listed
   // in the NFIT)
   ReturnCodeNonBlocking = FillUninitializedDimmList();
   if (EFI_ERROR(ReturnCodeNonBlocking)) {
    NVDIMM_WARN("Failed on Smbus dimm list init, error = " FORMAT_EFI_STATUS ".", ReturnCodeNonBlocking);
   }
#endif
   /**
    Verify that all manageable NVM-DIMMs have unique identifier. Otherwise, print a critical error and
    break further initialization.
   **/
   LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pDimmNode);
    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    LIST_FOR_EACH(pDimmNode2, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm2 = DIMM_FROM_NODE(pDimmNode2);
      if (IsDimmManageable(pDimm2)) {
        ZeroMem(Dimm1Uid, sizeof(Dimm1Uid));
        ZeroMem(Dimm2Uid, sizeof(Dimm2Uid));
        GetDimmUid(pDimm, Dimm1Uid, MAX_DIMM_UID_LENGTH);
        GetDimmUid(pDimm2, Dimm2Uid, MAX_DIMM_UID_LENGTH);
        if (pDimm != pDimm2 && (StrICmp(Dimm1Uid,Dimm2Uid) == 0)) {
          NVDIMM_ERR("NVM-DIMMs with the same NVDIMM UID have been detected.");

#if defined(DYNAMIC_WA_ENABLE)
          if (gNvmDimmData->IgnoreTheSameUIDNumbers) {
            NVDIMM_DBG("Ignoring same NVDIMM UIDs among dimms");
          } else {
#endif
            ReturnCode = EFI_DEVICE_ERROR;
            goto Finish;
#if defined(DYNAMIC_WA_ENABLE)
          }
#endif
        }
      }
    }
   }
#if defined(DYNAMIC_WA_ENABLE)
   PcdUsage = !gNvmDimmData->PcdUsageDisabledOnInit;
#endif
#ifndef OS_BUILD
   if (PcdUsage) {
    /**
      Initialize Interleave Sets
      We try to initialize all Regions, but if something goes wrong with a specific Region, then we just don't
      create the Region or add a proper error state to it. So even then we continue the driver initialization.
    **/
    ReturnCode = InitializeISs(gNvmDimmData->PMEMDev.pFitHead,
      &gNvmDimmData->PMEMDev.Dimms, &gNvmDimmData->PMEMDev.ISs);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to retrieve the REGION/IS list, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    }

    /**
      Initialize Namespaces
    **/
    ReturnCode = InitializeNamespaces();
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to initialize Namespaces, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    }
   }
#endif
   Index = 0;
   for (pDimmNode = GetFirstNode(&gNvmDimmData->PMEMDev.Dimms);
      !IsNull(&gNvmDimmData->PMEMDev.Dimms, pDimmNode);
      pDimmNode = GetNextNode(&gNvmDimmData->PMEMDev.Dimms, pDimmNode)) {
    pDimm = DIMM_FROM_NODE(pDimmNode);

    /**
      Technically this should be NULL as it is in a global array,
      but we NULL it here just to be sure that the handle will be created
      in the first call to the "InstallMultipleProtocolInterfaces"
    **/
    gDimmsUefiData[Index].DeviceHandle = NULL;
    gDimmsUefiData[Index].pDevicePath = NULL;
    gDimmsUefiData[Index].pDimm = pDimm;

    gNvmDimmDevicePathNode.NFITDeviceHandle = pDimm->DeviceHandle.AsUint32;
    gDimmsUefiData[Index].pDevicePath = AppendDevicePathNode(
        gNvmDimmData->pControllerDevicePathInstance,
        (CONST EFI_DEVICE_PATH_PROTOCOL *) &gNvmDimmDevicePathNode);

    if (gDimmsUefiData[Index].pDevicePath == NULL) {
      NVDIMM_WARN("Failed to create DIMM logic unit device path, not enough resources.");
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
#ifndef OS_BUILD
    ReturnCode = gBS->InstallMultipleProtocolInterfaces(
            &gDimmsUefiData[Index].DeviceHandle,
            &gEfiDevicePathProtocolGuid,
            gDimmsUefiData[Index].pDevicePath,
            NULL);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to install Device Path protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
    }

    ReturnCode = gBS->OpenProtocol(
        gNvmDimmData->ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        (VOID **)&pTempDevicePathInterface,
        gNvmDimmDriverDriverBinding.DriverBindingHandle,
        gDimmsUefiData[Index].DeviceHandle,
        EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
      );

    if (EFI_ERROR(ReturnCode)) {
      /**
        This is not a critical error - the driver will still work properly.
        We report a warning.
      **/
      NVDIMM_WARN("Failed to open parent Device Path protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    }

    /**
      Register Dimm name
    **/
    ReturnCode = RegisterDimmName(Index);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to add the DIMM name, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
    }
    /**
      Check if we support this DIMMs Firmware API.
    **/
    if (!IsDimmManageable(pDimm)) {
      /**
        Install only firmware update, security and label protocols on a manageable DIMM.
      **/
      Index++;
      continue;
    }

    /**
      This assignment copies the global instance content to each local protocol instance.
    **/
    gDimmsUefiData[Index].FirmwareManagementInstance = gNvmDimmFirmwareManagementProtocol;

    ReturnCode = gBS->InstallMultipleProtocolInterfaces(
                   &gDimmsUefiData[Index].DeviceHandle,
                   &gNvmDimmFirmwareManagementProtocolGuid,
                   &gDimmsUefiData[Index].FirmwareManagementInstance,
                   NULL);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to install Firmware Management Protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
    }

    gDimmsUefiData[Index].StorageSecurityCommandInstance = gNvmDimmDriverStorageSecurityCommand;
    ReturnCode = gBS->InstallMultipleProtocolInterfaces(
        &gDimmsUefiData[Index].DeviceHandle,
        &gEfiStorageSecurityCommandProtocolGuid,
        &gDimmsUefiData[Index].StorageSecurityCommandInstance,
        NULL);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to install Storage Security Command protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
    }

    gDimmsUefiData[Index].NvdimmLabelProtocolInstance = gNvdimmLabelProtocol;

    ReturnCode = gBS->InstallMultipleProtocolInterfaces(
        &gDimmsUefiData[Index].DeviceHandle,
        &gEfiNvdimmLabelProtocolGuid,
        &gDimmsUefiData[Index].NvdimmLabelProtocolInstance,
        NULL);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to install NVDIMM label protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
    }

    Index++;
#endif //OS_BUILD
   }
#ifndef OS_BUILD
   ReturnCode = InstallProtocolsOnNamespaces();

   if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to install Block Namespace devices, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    goto Finish;
   }
#endif
Finish:

   return ReturnCode;

}

#ifdef OS_BUILD
/**
  Starts a device controller or a bus controller.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverDriverBindingStart(
   IN     EFI_DRIVER_BINDING_PROTOCOL *pThis,
   IN     EFI_HANDLE ControllerHandle,
   IN     EFI_DEVICE_PATH_PROTOCOL *pRemainingDevicePath OPTIONAL
)
{
   EFI_STATUS ReturnCode = EFI_SUCCESS;
   UINT32 Index = 0;
   EFI_DEVICE_PATH_PROTOCOL *pTempDevicePathInterface = NULL;
   DIMM *pDimm = NULL;
   DIMM *pDimm2 = NULL;
   LIST_ENTRY *pDimmNode = NULL;
   LIST_ENTRY *pDimmNode2 = NULL;
   VOID *pDummy = 0;
   BOOLEAN PcdUsage = TRUE;
   CHAR16 Dimm1Uid[MAX_DIMM_UID_LENGTH];
   CHAR16 Dimm2Uid[MAX_DIMM_UID_LENGTH];

   NVDIMM_ENTRY();

#if !defined(MDEPKG_NDEBUG) && !defined(_MSC_VER)
   /**
   Enable recording AllocatePool and FreePool occurences
   **/
   EnableTracing();
#endif

   /**
   Init container keeping operation statuses with wanring, error and info level messages.
   **/
   InitErrorAndWarningNvmStatusCodes();

   /**
   Remember the Controller handle that we were started with.
   **/
   gNvmDimmData->ControllerHandle = ControllerHandle;
   gNvmDimmData->NvmDimmConfig = gNvmDimmDriverNvmDimmConfig;

   /**
   load the ACPI Tables (NFIT, PCAT, PMTT)
   **/
   ReturnCode = initAcpiTables();
   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to initialize the ACPI tables, error = " FORMAT_EFI_STATUS ".", ReturnCode);
   }

   /**
   check the NFIT SPA range map against the memory map
   **/
   ReturnCode = CheckMemoryMap();
   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_ERR("Failed while checking memory map, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
   }

   /**
   enumerate DCPMMs
   **/
   ReturnCode = FillDimmList();
   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to initialize Dimms, error = " FORMAT_EFI_STATUS ".", ReturnCode);
   }

   /**
   Verify that all manageable NVM-DIMMs have unique identifier. Otherwise, print a critical error and
   break further initialization.
   **/
   LIST_FOR_EACH(pDimmNode, &gNvmDimmData->PMEMDev.Dimms) {
      pDimm = DIMM_FROM_NODE(pDimmNode);
      if (!IsDimmManageable(pDimm)) {
         continue;
      }

      LIST_FOR_EACH(pDimmNode2, &gNvmDimmData->PMEMDev.Dimms) {
         pDimm2 = DIMM_FROM_NODE(pDimmNode2);
         if (IsDimmManageable(pDimm2)) {
            ZeroMem(Dimm1Uid, sizeof(Dimm1Uid));
            ZeroMem(Dimm2Uid, sizeof(Dimm2Uid));
            GetDimmUid(pDimm, Dimm1Uid, MAX_DIMM_UID_LENGTH);
            GetDimmUid(pDimm2, Dimm2Uid, MAX_DIMM_UID_LENGTH);
            if (pDimm != pDimm2 && (StrICmp(Dimm1Uid, Dimm2Uid) == 0)) {
               NVDIMM_ERR("NVM-DIMMs with the same NVDIMM UID have been detected.");

#if defined(DYNAMIC_WA_ENABLE)
               if (gNvmDimmData->IgnoreTheSameUIDNumbers) {
                  NVDIMM_DBG("Ignoring same NVDIMM UIDs among dimms");
               } else {
#endif
                  NvmDimmDriverDriverBindingStop(pThis, ControllerHandle, 0, NULL);
                  NVDIMM_DBG("Stopping driver");
                  ReturnCode = EFI_DEVICE_ERROR;
                  goto Finish;
#if defined(DYNAMIC_WA_ENABLE)
               }
#endif
            }
         }
      }
   }
#if defined(DYNAMIC_WA_ENABLE)
   PcdUsage = !gNvmDimmData->PcdUsageDisabledOnInit;
#endif

   Index = 0;
   for (pDimmNode = GetFirstNode(&gNvmDimmData->PMEMDev.Dimms);
      !IsNull(&gNvmDimmData->PMEMDev.Dimms, pDimmNode);
      pDimmNode = GetNextNode(&gNvmDimmData->PMEMDev.Dimms, pDimmNode)) {
      pDimm = DIMM_FROM_NODE(pDimmNode);

      /**
      Technically this should be NULL as it is in a global array,
      but we NULL it here just to be sure that the handle will be created
      in the first call to the "InstallMultipleProtocolInterfaces"
      **/
      gDimmsUefiData[Index].DeviceHandle = NULL;
      gDimmsUefiData[Index].pDevicePath = NULL;
      gDimmsUefiData[Index].pDimm = pDimm;

      gNvmDimmDevicePathNode.NFITDeviceHandle = pDimm->DeviceHandle.AsUint32;
      gDimmsUefiData[Index].pDevicePath = AppendDevicePathNode(
         gNvmDimmData->pControllerDevicePathInstance,
         (CONST EFI_DEVICE_PATH_PROTOCOL *) &gNvmDimmDevicePathNode);

      if (gDimmsUefiData[Index].pDevicePath == NULL) {
         NVDIMM_WARN("Failed to create DIMM logic unit device path, not enough resources.");
         ReturnCode = EFI_OUT_OF_RESOURCES;
         goto Finish;
      }
   }

Finish:
  //ReturnCode = 0;
   NVDIMM_DBG("Exiting DriverBindingStart, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
   NVDIMM_EXIT_I64(ReturnCode);
   return ReturnCode;
}
#else
/**
Starts a device controller or a bus controller.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverDriverBindingStart(
  IN     EFI_DRIVER_BINDING_PROTOCOL *pThis,
  IN     EFI_HANDLE ControllerHandle,
  IN     EFI_DEVICE_PATH_PROTOCOL *pRemainingDevicePath OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  VOID *pDummy = 0;
  INTEL_DIMM_CONFIG *pIntelDIMMConfig = NULL;

  NVDIMM_ENTRY();

#if !defined(MDEPKG_NDEBUG) && !defined(_MSC_VER)
   /**
   Enable recording AllocatePool and FreePool occurences
   **/
   EnableTracing();
#endif

   /**
   Remember the Controller handle that we were started with.
   **/
   gNvmDimmData->ControllerHandle = ControllerHandle;

   /**
   Install device path protocol
   **/
   ReturnCode = gBS->InstallMultipleProtocolInterfaces(
      &ControllerHandle,
      &gEfiDevicePathProtocolGuid,
      &gNvmDimmDriverDevicePath,
      NULL);
   if (EFI_ERROR(ReturnCode)) {
      /**
      We might get this error if we already have the device path protocol installed.
      Lets try to open it.
      **/
      if (ReturnCode == EFI_INVALID_PARAMETER) {
         ReturnCode = gBS->OpenProtocol(
            ControllerHandle,
            &gEfiDevicePathProtocolGuid,
            (VOID **)&gNvmDimmData->pControllerDevicePathInstance,
            pThis->DriverBindingHandle,
            NULL,
            EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
         );

         if (EFI_ERROR(ReturnCode)) {
            NVDIMM_WARN("Failed to install Device Path protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
            goto Finish;
         } else {
            gNvmDimmData->UninstallDevicePath = FALSE; //!< The device path was already installed - do not uninstall it
         }
      } else {
         NVDIMM_WARN("Failed to install Device Path protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      }
   } else {
      gNvmDimmData->UninstallDevicePath = TRUE; //!< We installed the device path - need to uninstall it on Stop()
   }

   /**
   Install EFI_DCPMM_CONFIG_PROTOCOL on the driver handle
   **/
   ReturnCode = gBS->InstallMultipleProtocolInterfaces(&gNvmDimmData->DriverHandle,
      &gNvmDimmConfigProtocolGuid, &gNvmDimmDriverNvmDimmConfig,
      NULL);
   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to install the NvmDimmConfigProtocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
   }
   gNvmDimmData->NvmDimmConfig = gNvmDimmDriverNvmDimmConfig;

   /**
   Open the device path protocol to prepare for appending DIMM nodes.
   **/
   ReturnCode = gBS->OpenProtocol(
      ControllerHandle,
      &gEfiDevicePathProtocolGuid,
      (VOID **)&gNvmDimmData->pControllerDevicePathInstance,
      pThis->DriverBindingHandle,
      ControllerHandle,
      EFI_OPEN_PROTOCOL_BY_DRIVER
   );

   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to open Device Path protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto FinishSkipClose;
   }

   ReturnCode = gBS->OpenProtocol(
      ControllerHandle,
      &gNfitBindingProtocolGuid,
      &pDummy,
      pThis->DriverBindingHandle,
      ControllerHandle,
      EFI_OPEN_PROTOCOL_BY_DRIVER
   );

   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to open NFIT Binding protocol, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto FinishSkipClose;
   }

   gNvmDimmData->HiiHandle = HiiAddPackages(&gNvmDimmNgnvmGuid, gNvmDimmData->DriverHandle, IntelDCPersistentMemoryDriverStrings, NULL);
   if (gNvmDimmData->HiiHandle == NULL) {
      NVDIMM_WARN("Unable to add string package to Hii");
      goto Finish;
   }

   /**
   load the ACPI Tables (NFIT and PCAT)
   **/
   ReturnCode = initAcpiTables();
   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to initialize the ACPI tables, error = " FORMAT_EFI_STATUS ".", ReturnCode);
   }

   /**
   check the NFIT SPA range map against the memory map
   **/
   ReturnCode = CheckMemoryMap();
   if (EFI_ERROR(ReturnCode)) {
      NVDIMM_ERR("Failed while checking memory map, error = " FORMAT_EFI_STATUS ".", ReturnCode);
      goto Finish;
   }
  /**
    Initialize DIMMs, ISets and namespaces
  **/
  ReturnCode = InitializeDimms();
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed while checking memory map, error = " FORMAT_EFI_STATUS ".", ReturnCode);
    goto Finish;
  }

  /**
    Check Intel DIMM Config EFI variables whether to perform automatic provisioning
  **/
  ReturnCode = RetrieveIntelDIMMConfig(&pIntelDIMMConfig);

  if (EFI_ERROR(ReturnCode)) {
    // Does not affect BindingStart status, just skip auto provision
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  if (pIntelDIMMConfig->ProvisionCapacityMode == PROVISION_CAPACITY_MODE_AUTO) {
    NVDIMM_DBG("Entering automatic capacity provisioning flow.");
    AutomaticProvisionCapacity(pIntelDIMMConfig);
  }

  if (pIntelDIMMConfig->ProvisionNamespaceMode == PROVISION_CAPACITY_MODE_AUTO) {
    NVDIMM_DBG("Entering automatic namespace provisioning flow.");
    AutomaticProvisionNamespace(pIntelDIMMConfig);
  }

Finish:
  FREE_POOL_SAFE(pIntelDIMMConfig);
  if (EFI_ERROR(ReturnCode)) {
    gBS->CloseProtocol(
      ControllerHandle,
      &gEfiDevicePathProtocolGuid,
      pThis->DriverBindingHandle,
      ControllerHandle
    );

    if (gNvmDimmData->HiiHandle != NULL) {
      HiiRemovePackages(gNvmDimmData->HiiHandle);
    }
  }

FinishSkipClose:
	NVDIMM_DBG("Exiting DriverBindingStart, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
	NVDIMM_EXIT_I64(ReturnCode);
	return ReturnCode;
}
#endif //OS_BUILD
/**
  Stops a device controller or a bus controller.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverDriverBindingStop(
  IN     EFI_DRIVER_BINDING_PROTOCOL *pThis,
  IN     EFI_HANDLE ControllerHandle,
  IN     UINTN NumberOfChildren,
  IN     EFI_HANDLE *pChildHandleBuffer OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  EFI_STATUS TempReturnCode = EFI_SUCCESS;
  DIMM *pDimm = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (gNvmDimmData == NULL) {
    NVDIMM_WARN("Driver data structure not initialized!\n");
    ReturnCode = EFI_DEVICE_ERROR;
    goto Finish;
  }

  if (gNvmDimmData->ControllerHandle == NULL) {
    NVDIMM_WARN("Driver already stopped.\n");
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  if (ControllerHandle == NULL) {
    NVDIMM_WARN("The stop controller handle is NULL.\n");
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  if (ControllerHandle != gNvmDimmData->ControllerHandle) {
    NVDIMM_WARN("The stop controller handle differs from the Start controller handle.\n");
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  ReturnCode = CleanNamespacesAndISs();

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to clean the Namespaces Block Devices, error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
  }

  Index = 0;
  for (pDimmNode = GetFirstNode(&gNvmDimmData->PMEMDev.Dimms);
      !IsNull(&gNvmDimmData->PMEMDev.Dimms, pDimmNode);
      pDimmNode = GetNextNode(&gNvmDimmData->PMEMDev.Dimms, pDimmNode)) {
    pDimm = DIMM_FROM_NODE(pDimmNode);
    ReturnCode = EFI_SUCCESS;
    /**
      Disconnect the device handles from the parent controller.
      (close the protocol opened with the BY_CHILD attribute)

      We are checking if both handles exist, because this function may be
      called more than once.
    **/
    if (gDimmsUefiData[Index].DeviceHandle != NULL) {
      ReturnCode = gBS->CloseProtocol(
        ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        pThis->DriverBindingHandle,
        gDimmsUefiData[Index].DeviceHandle
      );

      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to disconnect the child device, error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
      }

      /**
        Uninstall all protocols from the child
      **/
      ReturnCode = gBS->UninstallMultipleProtocolInterfaces(
        gDimmsUefiData[Index].DeviceHandle,
        &gEfiDevicePathProtocolGuid, gDimmsUefiData[Index].pDevicePath,
        NULL
      );

      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to uninstall the child device device path protocol, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
      }

      if (IsDimmManageable(pDimm)) {
        ReturnCode = gBS->UninstallMultipleProtocolInterfaces(
          gDimmsUefiData[Index].DeviceHandle,
          &gNvmDimmFirmwareManagementProtocolGuid, &gDimmsUefiData[Index].FirmwareManagementInstance,
          NULL
        );

        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_WARN("Failed to uninstall the child device firmware management protocol, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
        }

        ReturnCode = gBS->UninstallMultipleProtocolInterfaces(
          gDimmsUefiData[Index].DeviceHandle,
          &gEfiStorageSecurityCommandProtocolGuid, &gDimmsUefiData[Index].StorageSecurityCommandInstance,
          NULL
        );

        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_WARN("Failed to uninstall the child device security command protocol, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
        }
      }
      gDimmsUefiData[Index].DeviceHandle = NULL;

      if (gDimmsUefiData[Index].pDevicePath != NULL) {
        gBS->FreePool(gDimmsUefiData[Index].pDevicePath);
        gDimmsUefiData[Index].pDevicePath = NULL;
      }

      if (gDimmsUefiData[Index].pDimmName != NULL) {
        FreeUnicodeStringTable(gDimmsUefiData[Index].pDimmName);
        gDimmsUefiData[Index].pDimmName = NULL;
      }
    }
    Index++;
  }

  ReturnCode = gBS->CloseProtocol(
    ControllerHandle,
    &gEfiDevicePathProtocolGuid,
    pThis->DriverBindingHandle,
    ControllerHandle
  );

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to close the controller path protocol, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
  }

  ReturnCode = gBS->CloseProtocol(
    ControllerHandle,
    &gNfitBindingProtocolGuid,
    pThis->DriverBindingHandle,
    ControllerHandle
  );

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to close the nfit binding protocol, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
  }

  /** Uninstall NvmDimm Config Protocol **/
  TempReturnCode = gBS->UninstallMultipleProtocolInterfaces(gNvmDimmData->DriverHandle,
      &gNvmDimmConfigProtocolGuid, &gNvmDimmDriverNvmDimmConfig, NULL);
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_WARN("Failed to uninstall the NvmDimmConfig protocol, error = " FORMAT_EFI_STATUS ".\n", TempReturnCode);
  }

  /**
    Uninstall device path
  **/
  if (gNvmDimmData->UninstallDevicePath) {
    TempReturnCode = gBS->UninstallMultipleProtocolInterfaces(
        ControllerHandle,
        &gEfiDevicePathProtocolGuid, &gNvmDimmDriverDevicePath,
        NULL);
    if (EFI_ERROR(TempReturnCode)) {
      FIRST_ERR(ReturnCode, TempReturnCode);
      NVDIMM_DBG("Failed to uninstall the device path protocol, error = " FORMAT_EFI_STATUS ".\n", TempReturnCode);
    } else {
      gNvmDimmData->UninstallDevicePath = FALSE; //!< Remember that we had already uninstalled the device path
    }
  }
  TempReturnCode = SmbusDeinit();
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_DBG("Failed to Smbus deinit, error = " FORMAT_EFI_STATUS ".\n", TempReturnCode);
  }
  /**
    Remove the DIMM from memory
  **/
  TempReturnCode = FreeDimmList();
  if (EFI_ERROR(TempReturnCode)) {
    FIRST_ERR(ReturnCode, TempReturnCode);
    NVDIMM_DBG("Failed to free dimm list, error = " FORMAT_EFI_STATUS ".\n", TempReturnCode);
  }

  /** Free PCAT tables memory **/
  FreeParsedPcat(gNvmDimmData->PMEMDev.pPcatHead);

  /** Free NFIT tables memory **/
  FreeParsedNfit(gNvmDimmData->PMEMDev.pFitHead);

  if (gNvmDimmData->HiiHandle != NULL) {
    HiiRemovePackages(gNvmDimmData->HiiHandle);
    gNvmDimmData->HiiHandle = NULL;
  }

  /** Clear the controller that we were started with **/
  gNvmDimmData->ControllerHandle = NULL;

Finish:
#else //not OS_BUILD
  uninitAcpiTables();
#endif //not OS_BUILD
#if _BullseyeCoverage
  cov_dumpData();
#endif
  NVDIMM_DBG("Exiting DriverBindingStop, error = " FORMAT_EFI_STATUS ".\n", ReturnCode);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Returns the EFI_UNICODE_STRING_TABLE containing the Namespace Name.

  @param[in] NamespaceHandle the handle that we want to search for.
  @param[out] ppNamespaceName - pointer to where the function should place
  pointer to the Namespace EFI_UNICODE_STRING_TABLE.
  The value will be NULL if we will not find the Namespace.

  @retval EFI_SUCCESS if we have found the Namespace.
  @retval EFI_INVALID_PARAMETER if at least one of the input parameters equals NULL.
  @retval EFI_NOT_FOUND if we could not locate the Namespace.
**/
EFI_STATUS
GetNamespaceName(
IN     EFI_HANDLE NamespaceHandle,
   OUT EFI_UNICODE_STRING_TABLE **ppNamespaceName
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
#ifndef OS_BUILD
  LIST_ENTRY *pNamespaceNode = NULL;
  NAMESPACE *pNamespace = NULL;

  if (NamespaceHandle == NULL || ppNamespaceName == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *ppNamespaceName = NULL;

  LIST_FOR_EACH(pNamespaceNode, &gNvmDimmData->PMEMDev.Namespaces) {
    pNamespace = NAMESPACE_FROM_NODE(pNamespaceNode, NamespaceNode);

    if (pNamespace->BlockIoHandle == NamespaceHandle) {
      *ppNamespaceName = pNamespace->pNamespaceName;
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }
Finish:
#endif
  return ReturnCode;
}

/**
  This function performs all necessary checks to determine what device type
  are the input parameters pointing to.
  This function is meant to be used by the ComponentName and DriverHealth protocols.

  @param[in] ControllerHandle EFI_HANDLE passed from the UEFI Libs, trying to resolve the device type for.
  @param[in] ChildHandle EFI_HANDLE passed from the UEFI Libs, trying to resolve the device type for.
  @param[out] pDimmPid is a pointer to the PID of the DIMM.
  @param[out] pResultDevice
    All if the ControllerHandle equals NULL.
    Controller if the ControllerHandle is our controller and the ChildHandle equals NULL.
    Dimm if the Controller equals to our controller and the ChildHandle equals to one of our DIMM handles.
    Namespace if the Controller equals to our controller and the ChildHandle equals to one of our
      block namespaces OR if the Controller equals to one of our DIMM handles and the ChildHandle equals to the
      block namespace that resides on the specific DIMM.
    Unknown all other cases.
  @retval EFI_SUCCESS - Matching manged device type for given handles
  @retval EFI_INVALID_PARAMETER - if at least one of the input parameters equals NULL.
  @retval EFI_UNSUPPORTED - Handles provided are not managed by driver
**/
EFI_STATUS
ResolveDeviceType(
  IN     EFI_HANDLE ControllerHandle,
  IN     EFI_HANDLE ChildHandle,
     OUT UINT16 *pDimmPid OPTIONAL,
     OUT NvmDeviceType *pResultDevice
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  NAMESPACE *pNamespace = NULL;
  UINT16 DimmPid = 0;


  NVDIMM_ENTRY();

  if (ControllerHandle == NULL || pResultDevice == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pResultDevice = All;

  ReturnCode = EfiTestManagedDevice(ControllerHandle,
      gNvmDimmDriverDriverBinding.DriverBindingHandle,
      &gEfiDevicePathProtocolGuid);

  if (EFI_ERROR(ReturnCode)) {
    *pResultDevice = Unknown;

    // This is not our main controller but it can be our DIMM requesting the Namespace name.
    ReturnCode = EfiTestChildHandle(gNvmDimmData->ControllerHandle, ControllerHandle,
      &gEfiDevicePathProtocolGuid);

    if (EFI_ERROR(ReturnCode)) {
      // It is not the DIMM nor the Controller - unknown device
      goto Finish;
    }

    // The controller is one of our DIMMs, check if the given child is its child
    ReturnCode = EfiTestChildHandle(ControllerHandle, ChildHandle,
      &gEfiDevicePathProtocolGuid);

    if (EFI_ERROR(ReturnCode)) {
      // Again invalid set of handles
      goto Finish;
    }

    // This should be our Namespace - check it to be sure
    pNamespace = HandleToNamespace(ChildHandle);

    if (pNamespace == NULL) {
      // It was not our namespace, exit
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    } else {
      // It is our namespace
      *pResultDevice = Namespace;
      ReturnCode = EFI_SUCCESS;
      goto Finish;
    }
  }

  if (ChildHandle == NULL) {
    /**
      Lookup name of controller specified by ControllerHandle
    **/
    *pResultDevice = Controller;
    goto Finish;
  }

  /**
    If ChildHandle is not NULL, then make sure this driver produced ChildHandle
  **/
  ReturnCode = EfiTestChildHandle(ControllerHandle, ChildHandle,
      &gEfiDevicePathProtocolGuid);
  if (EFI_ERROR(ReturnCode)) {
    // The child handle is not our DIMM... but it can be our namespace.
    if (HandleToNamespace(ChildHandle) != NULL) {
      *pResultDevice = Namespace;
      ReturnCode = EFI_SUCCESS;
    } else {
      *pResultDevice = Unknown;
    }

    goto Finish;
  }

  // It is the DIMM
  *pResultDevice = Dimm;
  ReturnCode = HandleToPID(ChildHandle, NULL, &DimmPid);
  if (EFI_ERROR(ReturnCode)) {
    // We couldn't find the DIMM, apparently it was some other kind of child handle
    *pResultDevice = Unknown;
    ReturnCode = EFI_UNSUPPORTED;
  }

  if (pDimmPid != NULL) {
    *pDimmPid = DimmPid;
  }
Finish:
#endif
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function checks if we are managing the ChildHandle.
  If yes - it returns the DIMMs PID and the optional pointer to its EFI_UNICODE_STRING_TABLE

  @param[in] ChildHandle the handle that we want to search for.
  @param[out] Optional pointer to where the function should place
              pointer to the child EFI_UNICODE_STRING_TABLE.
              The value will be NULL if we will not find the DIMM
  @param[out] pDimmPid is a pointer to the PID of the DIMM.

  @retval EFI_SUCCESS if we have found the ChildHandle.
  @retval EFI_INVALID_PARAMETER if at least one of the input parameters equals NULL.
  @retval EFI_NOT_FOUND if we could not locate the ChildHandle.
**/
EFI_STATUS
HandleToPID(
  IN     EFI_HANDLE ChildHandle,
     OUT EFI_UNICODE_STRING_TABLE **ppChildDeviceName OPTIONAL,
     OUT UINT16 *pDimmPid
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
#ifndef OS_BUILD
  UINT32 Index = 0;
  NVDIMM_ENTRY();
  if (pDimmPid == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
  } else if (ChildHandle != NULL) {
    *pDimmPid = 0;
    for (Index = 0; Index < MAX_DIMMS; Index++) {
      if (gDimmsUefiData[Index].DeviceHandle == ChildHandle) {
        if (ppChildDeviceName != NULL) {
          *ppChildDeviceName = gDimmsUefiData[Index].pDimmName;
        }
        *pDimmPid = gDimmsUefiData[Index].pDimm->DimmID;
        ReturnCode = EFI_SUCCESS;
        goto Finish;
      }
    }
  }
  if (ppChildDeviceName != NULL) {
    *ppChildDeviceName = NULL;
  }
Finish:
#endif
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Returns the index in the global DIMMs EFI Data array for the desired DIMM.
  The caller must provide one of the input commands, if the caller provides more
  than one parameter, only the last one will be taken under consideration.
  (the priority is: DimmHandle, pDimm, DimmPid)

  @param[in] DimmPid optional Dimm ID of the DIMM we want to get the index to.
  @param[in] pDimm optional pointer to the desired DIMM structure.
  @param[in] DimmHandle optional handle to the desired DIMM device.

  @retval DIMM_PID_INVALID is returned if no such DIMM has been found.
  @retval 0-based index of the desired DIMM in the global structure.
**/
UINT16
GetDimmEfiDataIndex(
  IN     UINT16 DimmPid, OPTIONAL
  IN     DIMM *pDimm, OPTIONAL
  IN     EFI_HANDLE DimmHandle OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
#ifndef OS_BUILD
  UINT16 Index = 0;
  if (DimmHandle != NULL) {
    ReturnCode = HandleToPID(DimmHandle, NULL, &DimmPid);
    if EFI_ERROR(ReturnCode) {
      goto Finish;
    }
  } else if (pDimm != NULL) {
    DimmPid = pDimm->DimmID;
  }

  for (Index = 0; Index < MAX_DIMMS; Index++) {
    if (gDimmsUefiData[Index].pDimm->DimmID == DimmPid) {
      return Index;
    }
  }
Finish:
#endif
  return DIMM_PID_INVALID;
}
