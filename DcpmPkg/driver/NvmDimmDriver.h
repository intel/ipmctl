/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVMDIMM_DRIVER_H_
#define _NVMDIMM_DRIVER_H_

#include <Uefi.h>
#include <NvmDimmDriverData.h>
#include <Dimm.h>
#include <Dcpmm.h>

#if defined(DYNAMIC_WA_ENABLE)

#define DYNAMIC_WA_ENV_VARIABLE_NAME  L"PMMD_FLAGS"
#define WA_FLAG_ALWAYS_LOAD           L"ALWAYS_LOAD"
#define WA_FLAG_SIMICS                L"SIMICS"
#define WA_FLAG_UNLOAD_OTHER_DRIVERS  L"UNLOAD"
#define WA_FLAG_IGNORE_UID_NUMS       L"IGNORE_UID_NUMBERS"
#define WA_FLAG_NO_PCD_INIT           L"NO_PCD_INIT"

#endif

/**
  This is the generated IFR binary data for each formset defined in VFR.
  This data array is ready to be used as input of HiiAddPackages() to
  create a packagelist (which contains Form packages, String packages, etc).
**/
extern UINT8  NvmDimmDriverFormsBin[];

/**
  This is the generated String package data for all .UNI files.
  This data array is ready to be used as input of HiiAddPackages() to
  create a packagelist (which contains Form packages, String packages, etc).
**/
extern UINT8  IntelDCPersistentMemoryDriverStrings[];

/**
  Libraries
**/
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>

#include <Library/UefiRuntimeServicesTableLib.h>

/**
  UEFI Driver Model Protocols
**/
#include <Protocol/DriverBinding.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/ComponentName.h>
#include <Protocol/DriverDiagnostics2.h>
#include <Protocol/DriverDiagnostics.h>
#include <Protocol/DriverHealth.h>
#include <Protocol/LoadedImage.h>
#include <Library/DevicePathLib.h>
#include <Library/DevicePathLib.h>

/**
  Consumed Protocols
**/
#include <Protocol/PciIo.h>

/**
  Produced Protocols
**/
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include "NvmDimmConfig.h"
#include <NvmDimmPassThru.h>
#include <NvmFirmwareManagement.h>
#include <Protocol/StorageSecurityCommand.h>
#include <Protocol/NvdimmLabel.h>

/**
  Protocol instances
**/
extern EFI_DRIVER_BINDING_PROTOCOL gNvmDimmDriverDriverBinding;
extern EFI_COMPONENT_NAME2_PROTOCOL gNvmDimmDriverComponentName2;
extern EFI_COMPONENT_NAME_PROTOCOL gNvmDimmDriverComponentName;
extern EFI_DRIVER_DIAGNOSTICS2_PROTOCOL gNvmDimmDriverDriverDiagnostics2;
extern EFI_DRIVER_DIAGNOSTICS_PROTOCOL gNvmDimmDriverDriverDiagnostics;
extern EFI_DRIVER_HEALTH_PROTOCOL gNvmDimmDriverHealth;
extern EFI_DCPMM_CONFIG2_PROTOCOL gNvmDimmDriverNvmDimmConfig;
extern EFI_FIRMWARE_MANAGEMENT_PROTOCOL gNvmDimmFirmwareManagementProtocol;
extern EFI_STORAGE_SECURITY_COMMAND_PROTOCOL gNvmDimmDriverStorageSecurityCommand;
extern EFI_BLOCK_IO_PROTOCOL gNvmDimmDriverBlockIo;
extern EFI_NVDIMM_LABEL_PROTOCOL gNvdimmLabelProtocol;
extern EFI_DCPMM_PBR_PROTOCOL gNvmDimmDriverNvmDimmPbr;

typedef struct _PMEM_DEV {
  LIST_ENTRY Dimms;
  LIST_ENTRY UninitializedDimms;
  LIST_ENTRY ISs;
  LIST_ENTRY Namespaces;

  BOOLEAN DimmSkuConsistency;
  BOOLEAN RegionsAndNsInitialized;
  BOOLEAN NamespacesInitialized;
  BOOLEAN IsMemModeAllowedByBios;

  ParsedFitHeader *pFitHead;
  ParsedPcatHeader *pPcatHead;
} PMEM_DEV;

/**
  Specifies the alignments used by the driver.
  All of the values are in bytes.
**/
typedef struct {
  UINT64 RegionPartitionAlignment;
  UINT64 RegionVolatileAlignment;
  UINT64 RegionPersistentAlignment;

  UINT64 PmNamespaceMinSize;
  UINT64 BlockNamespaceMinSize;
} ALIGNMENT_SETTINGS;

/** NvmDimmDriver Data structure **/
typedef struct {
  EFI_HANDLE Handle;
  EFI_HANDLE DriverHandle;
  EFI_HANDLE ControllerHandle;
  EFI_HANDLE HiiHandle;
  EFI_DEVICE_PATH_PROTOCOL *pControllerDevicePathInstance;

#ifndef OS_BUILD
  /**
  BIOS Dcpmm protocol
  **/
  EFI_DCPMM_PROTOCOL *pDcpmmProtocol;
#endif // !OS_BUILD

  PMEM_DEV PMEMDev;
  /**
    NvmDimm data
  **/
  EFI_DCPMM_CONFIG2_PROTOCOL NvmDimmConfig;

  /**
    This flag informs us if we installed the Device Path
    on the controller and should uninstall it or not.
  **/
  BOOLEAN UninstallDevicePath;

  /**
    All of the minimal sizes and alignment values for the DIMM
    partitioning and Namespaces creation.
  **/
  ALIGNMENT_SETTINGS Alignments;

#if defined(DYNAMIC_WA_ENABLE)
  /**
    In case we need to try to load the driver when the binding handle is not present in the system,
    we can use this workaround to force-load the driver.

    Warning: the driver still requires NFIT to work properly.
  **/
  BOOLEAN AlwaysLoadDriver;

  /**
    If we have set the 'AlwaysLoadDriver' flag, this indicates if we had already created ourselves a handle to bind.
  **/
  BOOLEAN HandleCreated;

  /**
    If this driver and the HII driver are already loaded, unload them before loading
  **/
  BOOLEAN UnloadExistingDrivers;

  /**
    Special set of workarounds to be applied in SIMICS environment
  **/
  BOOLEAN SimicsWorkarounds;

  /**
    Disable checks of the same NVDIMM UID among dimms.
    Used when same SPD data is flashed without on many dimms.
  **/
  BOOLEAN IgnoreTheSameUIDNumbers;

  /**
    During Initialization of the driver, avoid using any commands that touch the PCD data.
  **/
  BOOLEAN PcdUsageDisabledOnInit;
#endif /** DYNAMIC_WA_ENABLE **/
} NVMDIMMDRIVER_DATA;

#define NVMDIMDRIVER_DATA_FROM_THIS(a)  CR(a, NVMDIMMDRIVER_DATA, ConfigAccess, NVMDIMM_DATA_SIGNATURE)

/**
  Structure to store EFI-related dimms data
**/
typedef struct {
  EFI_HANDLE DeviceHandle;               //!< The UEFI device handle that has the protocols installed
  EFI_DEVICE_PATH_PROTOCOL *pDevicePath; //!< The device path we need to release when unloading the driver
  DIMM *pDimm;                           //!< The DIMM shared information
  EFI_UNICODE_STRING_TABLE *pDimmName;   //!< The localized dimm name table for the ComponentName2 Protocol
  /**
    We create an instance per DIMM so we are able to identify on which
    DIMM handle the protocol procedure was called.
  **/
  EFI_FIRMWARE_MANAGEMENT_PROTOCOL FirmwareManagementInstance;
  EFI_STORAGE_SECURITY_COMMAND_PROTOCOL StorageSecurityCommandInstance;
  EFI_DRIVER_HEALTH_PROTOCOL DriverHealthProtocolInstance;
  EFI_BLOCK_IO_PROTOCOL BlockIoProtocolInstance;
  EFI_NVDIMM_LABEL_PROTOCOL NvdimmLabelProtocolInstance;
} EFI_DIMMS_DATA;

/**
  Type used to determine what kind of device is the particular set of
  Controller and Child handles aiming to.
**/
typedef enum {
  Controller,
  Dimm,
  Namespace,
  All,
  Unknown
} NvmDeviceType;

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
  );

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
  );

/**
  Function that allows to "refresh" the existing DIMMs.
  If a DIMM is unaccessible, all of the ISs and namespaces
  that it was a part of will be removed.

  @param[in] DoDriverCleanup, if the caller wants namespaces cleaned up including unloading protocols

  @retval EFI_SUCCESS if all DIMMs are working.
  @retval EFI_INVALID_PARAMETER if any of pointer parameters in NULL
  @retval EFI_ABORTED if at least one DIMM is not responding.
  @retval EFI_OUT_OF_RESOURCES if the memory allocation fails.
  @retval EFI_NO_RESPONSE FW busy for one or more dimms
**/
EFI_STATUS
ReenumerateNamespacesAndISs(
  IN BOOLEAN DoDriverCleanup
  );

/**
  Function tries to remove all of the block namespaces protocols, then it
  removes all of the enumerated namespaces from the LSA and also the ISs.

  @retval EFI_SUCCESS the object were cleared successfully
  Error return codes from CleanBlockNamespaces function
**/
EFI_STATUS
CleanNamespacesAndISs(
  );

/**
  Returns the EFI_UNICODE_STRING_TABLE containing the Namespace Name.

  @param[in] NamespaceHandle the handle that we want to search for.
  @param[out] ppNamespaceName - pointer to where the function should place
  pointer to the Namespace EFI_UNICODE_STRING_TABLE.
  The value will be NULL if we will not find the Namespace.

  @retval EFI_SUCCeSS if we have found the Namespace.
  @retval EFI_INVALID_PARAMETER if at least one of the input parameters equals NULL.
  @retval EFI_NOT_FOUND if we could not locate the Namespace.
**/
EFI_STATUS
GetNamespaceName(
  IN     EFI_HANDLE NamespaceHandle,
     OUT EFI_UNICODE_STRING_TABLE **ppNamespaceName
  );

/**
  This function checks if we are managing the ChildHandle.
  If yes - it returns the DIMMs PID. And the pointer to its EFI_UNICODE_STRING_TABLE

  @param[in] ChildHandle the handle that we want to search for.
  @param[out] Optional pointer to where the function should place
              pointer to the child EFI_UNICODE_STRING_TABLE.
              The value will be NULL if we will not find the DIMM

  @retval UINT16 DimmID of the DIMM or 0 of we did not find the device.
**/
EFI_STATUS
HandleToPID(
  IN     EFI_HANDLE ChildHandle,
     OUT EFI_UNICODE_STRING_TABLE **ppChildDeviceName OPTIONAL,
     OUT UINT16 *pDimmPid
  );

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
UINT16
GetDimmEfiDataIndex(
  IN     UINT16 DimmPid, OPTIONAL
  IN     DIMM *pDimm, OPTIONAL
  IN     EFI_HANDLE DimmHandle OPTIONAL
  );

/**
  This function makes calls to the dimms required to initialize the driver.

  @retval EFI_SUCCESS if no errors.
  @retval EFI_xxxx depending on error encountered.
**/
EFI_STATUS
InitializeDimms();


/**
  Include files with function prototypes
**/
#include "DriverBinding.h"
#include "ComponentName.h"
#include "DriverDiagnostics.h"
#include "DriverHealth.h"

#endif /** _NVMDIMM_DRIVER_H_ **/
