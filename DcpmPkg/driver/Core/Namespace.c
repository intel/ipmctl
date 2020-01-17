/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <Uefi.h>
#include <Library/RngLib.h>
#include <Debug.h>
#include <Types.h>
#include "Namespace.h"
#ifndef OS_BUILD
#include <Dcpmm.h>
#endif
#include <AcpiParsing.h>
#include "Region.h"
#include "NvmSecurity.h"
#include "NvmDimmBlockIo.h"
#include "Btt.h"
#include "Pfn.h"
#include <Convert.h>
#include "AsmCommands.h"
#include <Version.h>


extern EFI_SYSTEM_TABLE *gSystemTable;
extern EFI_DIMMS_DATA gDimmsUefiData[MAX_DIMMS];
extern NVMDIMMDRIVER_DATA *gNvmDimmData;
extern EFI_BLOCK_IO_MEDIA gNvmDimmDriverBlockIoMedia;
extern CONST UINT64 gSupportedBlockSizes[SUPPORTED_BLOCK_SIZES_COUNT];

#ifndef OS_BUILD
extern DCPMM_ARS_ERROR_RECORD * gArsBadRecords;
extern INT32 gArsBadRecordsCount;
#endif

extern VOID
(*gClFlush)(
  VOID *pLinearAddress
  );

/** From EDK Network Controller Driver source code **/
#define GET_RANDOM_UINT32(Seed) ((UINT32) (((Seed) * 1103515245L + 12345) % 4294967295L))

/**
  Vendor specific namespaces device path
  For each namespace the UUID should be modified to distinct them.
**/
VENDOR_DEVICE_PATH gVenHwNamespaceDevicePathNode = {
  {
    HARDWARE_DEVICE_PATH,
    HW_VENDOR_DP, {
      (UINT8)(sizeof(VENDOR_DEVICE_PATH)),
      (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
    }
  },
    NVMDIMM_DRIVER_NGNVM_GUID //!< This GUID will be overwritten by the namespace UUID
};

/**
  UEFI spec defined namespace device path. Per UEFI 2.8
  For each namespace the UUID should be modified to distinct them.
**/
NVDIMM_NAMESPACE_DEVICE_PATH gNvdimmNamespaceDevicePathNode = {
  {
    MESSAGING_DEVICE_PATH,
    MSG_NVDIMM_NAMESPACE_DP, {
      (UINT8)(sizeof(NVDIMM_NAMESPACE_DEVICE_PATH)),
      (UINT8)((sizeof(NVDIMM_NAMESPACE_DEVICE_PATH)) >> 8)
    }
  },
    NVMDIMM_DRIVER_NGNVM_GUID //!< This GUID will be overwritten by the namespace UUID
};

EFI_STATUS
IsNamespaceLocked(
  IN     NAMESPACE *pNamespace,
     OUT BOOLEAN *pIsLocked
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 SecurityState = 0;
  DIMM **ppDimms = NULL;
  UINT32 DimmsNum = 0;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pIsLocked == NULL) {
    goto Finish;
  }

  *pIsLocked = FALSE;

  if (pNamespace->pParentIS == NULL) {
   goto Finish;
  }

  DimmsNum = pNamespace->RangesCount;
  ppDimms = AllocateZeroPool(sizeof(*ppDimms) * DimmsNum);
  if (ppDimms == NULL) {
   ReturnCode = EFI_OUT_OF_RESOURCES;
   goto Finish;
  }

  Index = 0;
  for (Index = 0; Index < DimmsNum; Index++) {
    ppDimms[Index] = pNamespace->Range[Index].pDimm;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
   ReturnCode = GetDimmSecurityState(ppDimms[Index], PT_TIMEOUT_INTERVAL, &SecurityState);
   if (EFI_ERROR(ReturnCode)) {
     NVDIMM_DBG("Failed to get DIMM security state.");
     goto Finish;
   }

   if (!IsConfiguringAllowed(SecurityState)) {
     NVDIMM_DBG("Locked namespace discovered %g",pNamespace->NamespaceGuid);
     *pIsLocked = TRUE;
     goto Finish;
   }
  }

Finish:
  FREE_POOL_SAFE(ppDimms);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  RegisterNamespaceName
  Adds the Namespace name for the use of the ComponentName protocols.

  @param[in] pNamespace, the pointer of the Namespace structure
    that the caller wants to register the name for.

  @retval EFI_SUCCESS if the name was added successfully
    Other return values from the function AddStringToUnicodeTable.
**/
STATIC
EFI_STATUS
RegisterNamespaceName(
  IN     NAMESPACE *pNamespace
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pNamespaceName = NULL;
  EFI_UNICODE_STRING_TABLE *pNamespaceNameTable = NULL;

  NVDIMM_ENTRY();

  pNamespaceName = CatSPrint(NULL, (PRODUCT_NAME L" Namespace Id %d"), pNamespace->NamespaceId);

  if (pNamespaceName != NULL) {
    ReturnCode = AddStringToUnicodeTable(pNamespaceName, &pNamespaceNameTable);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to create the unicode namespace name.\n");
      goto Finish;
    }
    pNamespace->pNamespaceName = pNamespaceNameTable;
  } else {
    /**
      This is not a critical error - the driver will still work properly.
      We report a warning.
    **/
    NVDIMM_DBG("Failed to allocate memory for Namespace name.\n");
  }

Finish:
  FREE_POOL_SAFE(pNamespaceName);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function tries to match the given EFI_HANDLE in the list of existing
  block namespace handles.

  @param[in] Handle - the EFI_HANDLE that the caller needs the NAMESPACE structure pointer to.

  @retval NULL - if the handle did not match any installed handles.
  @retval pointer to the result NAMESPACE structure.
**/
NAMESPACE *
HandleToNamespace(
  IN     EFI_HANDLE Handle
  )
{
  LIST_ENTRY *pNamespaceNode = NULL;
  NAMESPACE *pNamespace = NULL;

  for (pNamespaceNode = GetFirstNode(&gNvmDimmData->PMEMDev.Namespaces);
      !IsNull(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode);
      pNamespaceNode = GetNextNode(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode)) {
    pNamespace = NAMESPACE_FROM_NODE(pNamespaceNode, NamespaceNode);

    if (pNamespace->BlockIoHandle == Handle) {
      return pNamespace;
    }
  }

  return NULL;
}

/**
  Iterates over all of the existing namespaces and installs block io protocols

  This function will install two protocols: block and device path on the namespace handle.
  Also it will allocate memory for the block device (child) device path instance so it can
  be attached to the Controller (provided by the parameter).
  At last, the function will open the Controllers device path instance as a child device.

  Before unloading the driver, CleanNamespaces should be run to close, uninstall those protocols
  and to free the memory.

  @retval EFI_SUCCESS the iteration and installation were successful.
  Other errors from InstallMultipleProtocolInterfaces and OpenProtocol functions.

  Warning! This function does not exit at the first error. It will be logged and then it continues
  to next namespace, trying to create as much working block devices as it is possible.
**/
EFI_STATUS
EFIAPI
InstallProtocolsOnNamespaces(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pNamespaceNode = NULL;
  NAMESPACE *pNamespace = NULL;

  for (pNamespaceNode = GetFirstNode(&gNvmDimmData->PMEMDev.Namespaces);
      !IsNull(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode);
      pNamespaceNode = GetNextNode(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode)) {
    pNamespace = NAMESPACE_FROM_NODE(pNamespaceNode, NamespaceNode);

    // We do not install the protocol on disabled block namespaces
    if (!pNamespace->Enabled) {
      continue;
    }

    ReturnCode = InstallNamespaceProtocols(pNamespace);

    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_NOT_READY || ReturnCode == EFI_ACCESS_DENIED) {
        NVDIMM_WARN("Namespace not enabled or invalid security state! Skipping the protocols installation.");
        ReturnCode = EFI_SUCCESS;
      }
    }
  }

  return ReturnCode;
}

/**
  This function closes all protocols opened by the block devices handles,
  it will also uninstall the block and device path protocols. At the end
  it deallocated the memory taken for the device path protocol instance.

  @retval EFI_SUCCESS the cleanup completed successfully.
  Other errors returned from UninstallMultipleProtocolInterfaces function.

  Warning! In case of an error, the cleanup does not break, it tries to
  clean the rest of the DIMMs.
**/
EFI_STATUS
EFIAPI
CleanNamespaces(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pNamespaceNode = NULL;
  NAMESPACE *pNamespace = NULL;

  NVDIMM_ENTRY();

  for (pNamespaceNode = GetFirstNode(&gNvmDimmData->PMEMDev.Namespaces);
      !IsNull(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode);
      pNamespaceNode = GetNextNode(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode)) {
    pNamespace = NAMESPACE_FROM_NODE(pNamespaceNode, NamespaceNode);

    ReturnCode = UninstallNamespaceProtocols(pNamespace);

    if (pNamespace->IsBttEnabled && pNamespace->pBtt != NULL) {
      BttRelease(pNamespace->pBtt);
      pNamespace->pBtt = NULL;
    }
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Attaches the block device to the DIMM and installs the Block IO Protocol to the Namespace.

  @param[in,out] pNamespace - is the pointer to the NAMESPACE structure that is supposed to be
    exposed in the system.

  @retval EFI_INVALID_PARAMETER if the pNamespace equals NULL.
  @retval EFI_ABORTED if the parent DIMM block window was not properly initialized or the DIMM security state
    could not be determined.
  @retval EFI_NOT_READY if the Namespace was not Enabled.
  @retval EFI_ACCESS_DENIED if the DIMM is locked
  @retval EFI_SUCCESS if the operation completed successfully. Or the Namespace is not Block.
  Other return values from OpenProtocol and InstallMultipleProtocolInterfaces function.
**/
EFI_STATUS
InstallNamespaceProtocols(
  IN OUT NAMESPACE *pNamespace
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_DEVICE_PATH_PROTOCOL *pTempDevicePathInterface = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pParentDevicePath = NULL;
  UINT32 MediaBlockSize = 0;
  VENDOR_DEVICE_PATH *pVenHwNamespaceDevicePath = NULL;
  NVDIMM_NAMESPACE_DEVICE_PATH *pNvdimmNamespaceDevicePath = NULL;

  NVDIMM_ENTRY();

  if (pNamespace == NULL) {
    goto Finish;
  }

  if (pNamespace->ProtocolsInstalled) {
    /** Already installed **/
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  if (!pNamespace->Enabled) {
    ReturnCode = EFI_NOT_READY;
    goto Finish;
  }

  if (pNamespace->pParentIS == NULL) {
    goto Finish;
  }

  pParentDevicePath = gNvmDimmData->pControllerDevicePathInstance;

  // Use standarized path if namespace is bootable
  if ((pNamespace->IsBttEnabled == TRUE ) ||
      (pNamespace->IsPfnEnabled == TRUE) ||
      (pNamespace->IsRawNamespace == TRUE)) {

    NVDIMM_DBG("Using NVDIMM Namespace Device Path");
    pNvdimmNamespaceDevicePath = AllocateZeroPool(sizeof(NVDIMM_NAMESPACE_DEVICE_PATH));
    if (NULL == pNvdimmNamespaceDevicePath) {
      NVDIMM_DBG("Failed to initialize the pNvdimmNamespaceDevicePath.\n");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    pNvdimmNamespaceDevicePath->Header.Type = MESSAGING_DEVICE_PATH;
    pNvdimmNamespaceDevicePath->Header.SubType = MSG_NVDIMM_NAMESPACE_DP;

    pNvdimmNamespaceDevicePath->Header.Length[0] = (UINT8)(sizeof(NVDIMM_NAMESPACE_DEVICE_PATH));
    pNvdimmNamespaceDevicePath->Header.Length[1] = (UINT8)((sizeof(NVDIMM_NAMESPACE_DEVICE_PATH)) >> 8);

    CopyMem_S(&pNvdimmNamespaceDevicePath->Guid, sizeof(pNvdimmNamespaceDevicePath->Guid), pNamespace->NamespaceGuid,
      sizeof(pNvdimmNamespaceDevicePath->Guid));

    pNamespace->pBlockDevicePath = AppendDevicePathNode(
      NULL,
      (CONST EFI_DEVICE_PATH_PROTOCOL *) pNvdimmNamespaceDevicePath);
  } else {
    NVDIMM_DBG("Using VenHw Namespace Device Path");
    pVenHwNamespaceDevicePath = AllocateZeroPool(sizeof(VENDOR_DEVICE_PATH));
    if (NULL == pVenHwNamespaceDevicePath) {
      NVDIMM_DBG("Failed to initialize the pVenHwNamespaceDevicePath.\n");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
    pVenHwNamespaceDevicePath->Header.Type = HARDWARE_DEVICE_PATH;
    pVenHwNamespaceDevicePath->Header.SubType = HW_VENDOR_DP;

    pVenHwNamespaceDevicePath->Header.Length[0] = (UINT8)(sizeof(VENDOR_DEVICE_PATH));
    pVenHwNamespaceDevicePath->Header.Length[1] = (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8);

    CopyMem_S(&pVenHwNamespaceDevicePath->Guid, sizeof(pVenHwNamespaceDevicePath->Guid), pNamespace->NamespaceGuid,
      sizeof(pVenHwNamespaceDevicePath->Guid));

    pNamespace->pBlockDevicePath = AppendDevicePathNode(
      pParentDevicePath,
      (CONST EFI_DEVICE_PATH_PROTOCOL *) pVenHwNamespaceDevicePath);
  }

  pNamespace->BlockIoInstance = gNvmDimmDriverBlockIo;

  // Get the logical block size before the Block IO Media copy
  MediaBlockSize = (UINT32)GetBlockDeviceBlockSize(pNamespace);
  if (MediaBlockSize == 0) {
    // Don't install the protocol if the size is 0.
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  CopyMem_S(&pNamespace->Media, sizeof(pNamespace->Media), &gNvmDimmDriverBlockIoMedia, sizeof(pNamespace->Media));

  // Overwrite with actual namespace size
  pNamespace->Media.BlockSize = MediaBlockSize;
  pNamespace->Media.OptimalTransferLengthGranularity = pNamespace->Media.BlockSize;
  pNamespace->Media.LastBlock = GetAccessibleCapacity(pNamespace) / pNamespace->Media.BlockSize - 1;

  if (pNamespace->IsBttEnabled) {
    if (pNamespace->pBtt == NULL) {
      NVDIMM_DBG("Failed to initialize the BTT.\n");
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
    // In case of Btt enabled we need to calculate LastBlock based on BTT LBA count
    pNamespace->Media.LastBlock = pNamespace->pBtt->NLbas - 1;
  }

  pNamespace->BlockIoInstance.Media = &pNamespace->Media;

  ReturnCode = RegisterNamespaceName(pNamespace);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not initialize the namespace name.");
  }

  ReturnCode = gBS->InstallProtocolInterface(
    &pNamespace->BlockIoHandle, &gEfiDevicePathProtocolGuid, EFI_NATIVE_INTERFACE, pNamespace->pBlockDevicePath);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to install the device path protocol, error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
  }

  ReturnCode = gBS->InstallMultipleProtocolInterfaces(
    &pNamespace->BlockIoHandle,
    &gEfiBlockIoProtocolGuid, &pNamespace->BlockIoInstance,
    // We might also need the BlockIo2Protocol while we are not sure, lets leave this as commented.
    //&gEfiBlockIo2ProtocolGuid, &gTestDeviceBlockIo2Protocol,
    NULL);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to install the block io protocol, error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
  } else {
    ReturnCode = gBS->OpenProtocol(
      gNvmDimmData->ControllerHandle,
      &gEfiDevicePathProtocolGuid,
      (VOID **)&pTempDevicePathInterface,
      gNvmDimmData->DriverHandle,
      pNamespace->BlockIoHandle,
      EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_WARN("Failed to initialize the Block Device, error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
    }
  }

  pNamespace->ProtocolsInstalled = TRUE;

Finish:
  FREE_POOL_SAFE(pNvdimmNamespaceDevicePath);
  FREE_POOL_SAFE(pVenHwNamespaceDevicePath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Detaches the block device from the DIMM and uninstalls the Block IO Protocol from the Namespace.
  If the Namespace is not enabled, no actions are taken.

  @param[in,out] pNamespace - is the pointer to the NAMESPACE structure that is supposed to be
    removed from the system.

  @retval EFI_INVALID_PARAMETER if the pNamespace equals NULL.
  @retval EFI_ABORTED if the Block IO instance handle equals NULL and no actions can be performed.
  @retval EFI_SUCCESS if the operation completed successfully. Or the Namespace is not Block.
  Other return values from UninstallMultipleProtocolInterfaces function.
**/
EFI_STATUS
UninstallNamespaceProtocols(
  IN OUT NAMESPACE *pNamespace
  )
{
  EFI_STATUS ReturnCode = EFI_ABORTED;
  EFI_DEVICE_PATH *pBlockDevicePath = NULL;
  UINT16 DimmDataIndex = DIMM_PID_INVALID;
  EFI_HANDLE DimmHandle = NULL;

  NVDIMM_ENTRY();

  if (pNamespace == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (!pNamespace->Enabled) {
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  pBlockDevicePath = pNamespace->pBlockDevicePath;

  if (pNamespace->BlockIoHandle == NULL) {
    NVDIMM_DBG("The DIMM Block IO handle is NULL, nothing to uninstall.");
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  DimmDataIndex = GetDimmEfiDataIndex(0, pNamespace->pParentDimm, NULL);
  if (DimmDataIndex != DIMM_PID_INVALID) {
    DimmHandle = gDimmsUefiData[DimmDataIndex].DeviceHandle;
    NVDIMM_DBG("DimmDataIndex -- gDimmsUefiData[%d].DeviceHandle = %d", DimmDataIndex, DimmHandle);
  } else {
    NVDIMM_DBG("DimmDataIndex (%d) = DIMM_PID_INVALID... DimmHandle was not set", DimmDataIndex);
  }

  NVDIMM_DBG("DimmHandle = %d", DimmHandle);
  NVDIMM_DBG("gNvmDimmData->ControllerHandle = %d", gNvmDimmData->ControllerHandle);
  NVDIMM_DBG("gNvmDimmData->DriverHandle = %d", gNvmDimmData->DriverHandle);
  NVDIMM_DBG("pNamespace->BlockIoHandle = %d", pNamespace->BlockIoHandle);

  ReturnCode = gBS->CloseProtocol(
    (pNamespace->Flags.Values.Local) ? DimmHandle : gNvmDimmData->ControllerHandle,
    &gEfiDevicePathProtocolGuid,
    gNvmDimmData->DriverHandle,
    pNamespace->BlockIoHandle
  );

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to detach the block device from parent device.");
    NVDIMM_WARN("Error = " FORMAT_EFI_STATUS "\n.", ReturnCode);

    if (pNamespace->Flags.Values.Local) {
      NVDIMM_DBG("CloseProtocol failed using DimmHandle.");
      NVDIMM_DBG("Attempting CloseProtocol with ControllerHandle");
      ReturnCode = gBS->CloseProtocol(
        gNvmDimmData->ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        gNvmDimmData->DriverHandle,
        pNamespace->BlockIoHandle
      );

      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to detach the block device from parent device.");
        NVDIMM_WARN("Error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
        goto Finish;
      }
    } else {
      NVDIMM_DBG("CloseProtocol failed using ControllerHandle.");
      NVDIMM_DBG("Attempting CloseProtocol with DimmHandle");
      ReturnCode = gBS->CloseProtocol(
        DimmHandle,
        &gEfiDevicePathProtocolGuid,
        gNvmDimmData->DriverHandle,
        pNamespace->BlockIoHandle
      );

      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to detach the block device from parent device.");
        NVDIMM_WARN("Error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
        goto Finish;
      }
    }
  }

  ReturnCode = gBS->UninstallMultipleProtocolInterfaces(
    pNamespace->BlockIoHandle,
    &gEfiDevicePathProtocolGuid, pNamespace->pBlockDevicePath,
    &gEfiBlockIoProtocolGuid, &pNamespace->BlockIoInstance,
    // We might also need the BlockIo2Protocol while we are not sure, lets leave this as commented.
    //&gEfiBlockIo2ProtocolGuid, &pNamespace->BlockIoInstance,
    NULL
    );

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_WARN("Failed to uninstall the block device protocols. Error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
    NVDIMM_WARN("The device may be still visible in the system and accessing it may cause unpredicted behaviour.");
  } else {
    // Free the instance only if the protocol was uninstalled successfully.
    FREE_POOL_SAFE(pBlockDevicePath);
    pNamespace->pBlockDevicePath = NULL;
    pNamespace->BlockIoHandle = NULL;
    ReturnCode = FreeUnicodeStringTable(pNamespace->pNamespaceName);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to free the namespace unicode name. Error = " FORMAT_EFI_STATUS "\n.", ReturnCode);
    } else {
      // The memory is cleared in the FreeUnicodeStringTable function
      pNamespace->pNamespaceName = NULL;
    }
  }

  pNamespace->ProtocolsInstalled = FALSE;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  GetNamespace
  Looks through the list of namespaces, searching for a namespace with a particular GUID.

  @param[in] pNamespacesList pointer to the list where the function is supposed to search for the Namespace.
  @param[in] Uuid the target UUID of the Namespace that the function is searching for.
  @param[out] ppNamespace pointer to a pointer of where the Namespace pointer will be stored.

  @retval FALSE At least one of the input pointers is NULL or there is no Namespace with
    the specified UUID on the list.
  @retval TRUE the Namespace has been found and is stored under the ppNamespace.
**/
STATIC
BOOLEAN
GetNamespace(
  IN     LIST_ENTRY *pNamespacesList,
  IN     GUID Uuid,
     OUT NAMESPACE **ppNamespace
  );

/**
  Initialize a random seed using current time.

  Get current time first. Then initialize a random seed based on some basic
  mathematics operation on the hour, day, minute, second, nanosecond and year
  of the current time.

  @return The random seed initialized with current time.
  @return 0 if there was an error while getting the current time.
**/
UINT32
EFIAPI
GenerateCurrentTimeSeed(
  VOID
  )
{
  EFI_TIME Time;
  UINT32 Seed = 0;

  SetMem(&Time, sizeof(Time), 0x0);

  if (gSystemTable->RuntimeServices != NULL && !EFI_ERROR(gSystemTable->RuntimeServices->GetTime(&Time, NULL))) {
    Seed = (~Time.Hour << 24 | Time.Day << 16 | Time.Minute << 8 | Time.Second);
    Seed ^= Time.Nanosecond;
    Seed ^= Time.Year << 7;
  }

  return Seed;
}

/**
  Generate random numbers in a buffer.

  @param[in, out]  Rand       The buffer to contain random numbers.
  @param[in]       RandLength The length of the Rand buffer.
**/
VOID
RandomizeBuffer(
  IN OUT UINT8  *Rand,
  IN     UINT64  RandLength
  )
{
  STATIC UINT32 Next = 0;

  if (Next == 0) {
    Next = GenerateCurrentTimeSeed();
  }

  while (RandLength > 0) {
    Next  = GET_RANDOM_UINT32(Next);
    *Rand = (UINT8) Next;
    Rand++;
    RandLength--;
  }
}

/**
  Generate a NamespaceId value
  Namespace Id is a 16bit value and consists of the InterleaveSetIndex/RegionId
  (upper 8bits) and slot index (lower 8 bits).
  Neigher InterleaveSetIndex nor slot index can equal zero. The lowest namespace
  Id value is 0x0101.

  @retval The generated ID
**/
UINT16
EFIAPI
GenerateNamespaceId(UINT16 RequestedRegionId)
{
  NAMESPACE *pNamespace = NULL;
  LIST_ENTRY *pNode = NULL;
  UINT16 NamespaceId = CREATE_NAMESPACE_ID(RequestedRegionId, 0);

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Namespaces) {
    pNamespace = NAMESPACE_FROM_NODE(pNode, NamespaceNode);
    if (pNamespace->pParentIS->RegionId != RequestedRegionId) {
      continue; // Find the namespace with requested Region ID
    }
    if (pNamespace->NamespaceId == NamespaceId + 1) {
      NamespaceId = pNamespace->NamespaceId;
    }
    else if (pNamespace->NamespaceId > NamespaceId) {
      break;
    }
  }
  NamespaceId++;
  return NamespaceId;
}


/**
  Generate a random (version 4) GUID

  @retval The generated GUID
  @retval Zeroed buffer, if there was a problem while getting the time seed
**/
VOID
EFIAPI
GenerateRandomGuid(
     OUT GUID *pResultGuid
  )
{
#ifndef OS_BUILD
  GUID GeneratedGuid;

  SetMem(&GeneratedGuid, sizeof(GeneratedGuid), 0x0);
  GetRandomNumber128((UINT64*)&GeneratedGuid);
  // Set the version of the GUID to the 4th version (Random GUID)
  GeneratedGuid.Data3 |= 0x4000; // 0b0100000000000000 Make sure that the 4 bits are set
  GeneratedGuid.Data3 &= 0x4FFF; // 0b0100111111111111 Zero other bits from the highest hex

  // Set two highest bits from Data4 to 10 (accordingly to the spec)
  GeneratedGuid.Data4[0] |= 0x80; // 0b10000000 Set the highest bit
  GeneratedGuid.Data4[0] &= 0xBF; // 0b10111111 Clear the 7th bit

  CopyMem_S(pResultGuid, sizeof(*pResultGuid), &GeneratedGuid, sizeof(*pResultGuid));
#endif
}


/**
  Function changes Namespace slot status to a required state.
  Slot status can be free or occupied. Appropriate bit is set or
  cleared in specified index block free bitmap.

  @param[in] pIndex Index Block in which to update free status
  @param[in] SlotNumber Number of a slot on which to update status
  @param[in] NewStatus Predefined value representing new status. This
    can be SLOT_FREE or SLOT_USED.

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
ChangeSlotStatus(
  IN     NAMESPACE_INDEX *pIndex,
  IN     UINT16 SlotNumber,
  IN     UINT16 NewStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  CONST UINT16 BitsInBlock = sizeof(UINT8) * 8;         // how many bits in a block
  CONST UINT16 BlockNumber = SlotNumber / BitsInBlock;  // subsequent block number in the bitmap
  CONST UINT8 BitNumber = (CONST UINT8)(SlotNumber % BitsInBlock);     // subsequent bit number in a block

  NVDIMM_ENTRY();
  if (pIndex == NULL) {
    goto Finish;
  }

  if (NewStatus == SLOT_FREE) {
    pIndex->pFree[BlockNumber] |= (1 << BitNumber);    // free slot marked with set bit
  } else if (NewStatus == SLOT_USED) {
    pIndex->pFree[BlockNumber] &= ~(1 << BitNumber);   // used slot marked with cleared bit
  } else {
    NVDIMM_DBG("Invalid slot status provided");
    goto Finish;
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Get Namespace label from of specified DIMM by it's UUID.

  Function allocates memory for the label data. It's callee responsibility
  to free it after it's no longer needed. If more than one label is detected
  then return error.

  @param[in] pDimm Target DIMM
  @param[in] UUID Namespace UUID
  @param[out] ppNamespaceLabel Pointer to a location where store the label

  @retval EFI_INVALID_PARAMETER NULL pointer parameter provided
  @retval EFI_OUT_OF_RESOURCES No memory to allocate label structure
  @retval EFI_DEVICE_ERROR More than one label found
  @retval EFI_SUCCESS Current Index position found
**/
EFI_STATUS
GetAppDirectLabelByUUID(
  IN     DIMM *pDimm,
  IN     GUID UUID,
     OUT NAMESPACE_LABEL **ppNamespaceLabel
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LABEL_STORAGE_AREA *pLsa = NULL;
  NAMESPACE_LABEL *pLabel = NULL;
  UINT16 CurrentIndex = 0;
  UINT16 Index = 0;
  BOOLEAN Found = FALSE;
  UINT16 SlotStatus = SLOT_UNKNOWN;
  BOOLEAN Use_Namespace1_1 = FALSE;
  BOOLEAN lsaIsLocal = FALSE;


  NVDIMM_ENTRY();

  if (pDimm == NULL || ppNamespaceLabel == NULL) {
    goto Finish;
  }

  if (pDimm->pLsa == NULL) {
    NVDIMM_DBG("Loading the actual cache");
    lsaIsLocal = TRUE;
    ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
    if (EFI_ERROR(ReturnCode) || pLsa == NULL) {
      goto Finish;
    }
  } else {
    NVDIMM_DBG("Using LSA cache pointer on DIMM");
    pLsa = pDimm->pLsa;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = EFI_NOT_FOUND;

  for (Index = 0; Index < pLsa->Index[CurrentIndex].NumberOfLabels; Index++) {
    CheckSlotStatus(&pLsa->Index[CurrentIndex], Index, &SlotStatus);
    if (SlotStatus == SLOT_FREE) {
      continue;
    }

    if (pLsa->Index[CurrentIndex].Major == NSINDEX_MAJOR &&
        pLsa->Index[CurrentIndex].Minor == NSINDEX_MINOR_1) {
      Use_Namespace1_1 = TRUE;
    }


    if (!IsNameSpaceTypeAppDirect(&pLsa->pLabels[Index], Use_Namespace1_1)) {
      continue;
    }

    if (CompareMem(&UUID, &pLsa->pLabels[Index].Uuid, sizeof(UUID)) != 0) {
      continue;
    }
    if (Found == TRUE) {
      NVDIMM_DBG("More than one AppDirect label with the same UUID found (DIMM=%d, pos=%d)",
          pDimm->DimmID, Index);
      ReturnCode = EFI_DEVICE_ERROR;
      goto Finish;
    }

    pLabel = AllocateZeroPool(sizeof(*pLabel));
    if (pLabel == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    CopyMem_S(pLabel, sizeof(*pLabel), &pLsa->pLabels[Index], sizeof(*pLabel));

    *ppNamespaceLabel = pLabel;
    Found = TRUE;
  }

  /** Return EFI_SUCCESS if a label was found **/
  if (Found == TRUE) {
    ReturnCode = EFI_SUCCESS;
  }

Finish:
  if (lsaIsLocal == TRUE && pLsa != NULL) {
    FreeLsaSafe(&pLsa);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Sanity checks of a Namespace Label

  @param[in]    pNamespaceLabel     Pointer to a Namespace label struct to be checked
  @param[in]    pLabelStorageArea   Pointer to a LSA to which NamespaceLabel belongs
  @retval   EFI_INVALID_PARAMETER Null pointer provided
  @retval   EFI_VOLUME_CORRUPTED The label is not valid
  @retval   EFI_SUCCESS The label is valid
**/
EFI_STATUS
ValidateNamespaceLabel(
  IN     NAMESPACE_LABEL *pNamespaceLabel, IN BOOLEAN Use_Namespace1_1
  )
{
  EFI_STATUS ReturnCode = EFI_VOLUME_CORRUPTED;
  NVDIMM_ENTRY();

  if (pNamespaceLabel == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  // Common sanity checks
  if (pNamespaceLabel->RawSize == 0) {
    goto Finish;
  }

  // Block namespace checks
  if (!IsNameSpaceTypeAppDirect(pNamespaceLabel, Use_Namespace1_1) &&
      (pNamespaceLabel->LbaSize == 0 ||
       pNamespaceLabel->Position != 0 ||
       pNamespaceLabel->NumberOfLabels != 0)) {
    goto Finish;
  }

  // AppDirect namespace checks
  if (IsNameSpaceTypeAppDirect(pNamespaceLabel, Use_Namespace1_1) &&
    (pNamespaceLabel->Position > pNamespaceLabel->NumberOfLabels)) {
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Checks provided Label Storage Area for consistency.

  Function runs a number of checks to determine whether LSA contains
  valid data.

  @param[in] pLabelStorageArea Pointer to a LSA structure

  @retval EFI_INVALID_PARAMETER Provided structure is NULL or contains errors
  @retval EFI_OUT_OF_RESOURCES If a memory allocation operation failed.
  @retval EFI_VOLUME_CORRUPTED There are no valid indexes in the LSA buffer
  @retval EFI_NOT_FOUND The LSA buffer is empty (filled with zeroes)
  @retval EFI_SUCCESS Provided structure contains valid data
**/
EFI_STATUS
ValidateLsaData(
  IN     LABEL_STORAGE_AREA *pLabelStorageArea
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT128 NamespaceSignature;
  BOOLEAN IndexesValid[2];
  BOOLEAN ChecksumMatched = FALSE;
  UINT32 Index = 0;
  LABEL_STORAGE_AREA *pEmptyLsa = NULL;
  UINT8 *pRawData = NULL;
  UINT64 IndexLength = 0;
  UINT32 ChecksumOffset = 0;

  NVDIMM_ENTRY();

  SetMem(&NamespaceSignature, sizeof(NamespaceSignature), 0x0);
  SetMem(IndexesValid, sizeof(IndexesValid), TRUE);

  if (pLabelStorageArea == NULL) {
    goto Finish;
  }

  pEmptyLsa = AllocateZeroPool(sizeof(*pEmptyLsa));

  if (pEmptyLsa == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  if (CompareMem(pLabelStorageArea, pEmptyLsa, sizeof(*pLabelStorageArea)) == 0) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ChecksumOffset = OFFSET_OF(NAMESPACE_INDEX, Checksum);

  /** Index Blocks tests **/
  for (Index = 0; Index < NAMESPACE_INDEXES; ++Index) {
    NVDIMM_DBG("Signature of the NAMESPACE INDEX %d", Index);
    // We are copying the signature in two steps so we need to copy twice the half of the NSINDEX_SIG_LEN length.
    CopyMem_S(&NamespaceSignature.Uint64, sizeof(NamespaceSignature.Uint64), &pLabelStorageArea->Index[Index].Signature, NSINDEX_SIG_LEN / 2);
    CopyMem_S(&NamespaceSignature.Uint64_1,
      sizeof(NamespaceSignature.Uint64_1),
      &pLabelStorageArea->Index[Index].Signature[NSINDEX_SIG_LEN / 2], NSINDEX_SIG_LEN / 2);
    NVDIMM_HEXDUMP(&NamespaceSignature, sizeof(NamespaceSignature));
    if (NamespaceSignature.Uint64 != LSA_NAMESPACE_INDEX_SIG_L ||
      NamespaceSignature.Uint64_1 != LSA_NAMESPACE_INDEX_SIG_H) {
      NVDIMM_DBG("The signature of the NAMESPACE INDEX %d is incorrect.", Index);
      IndexesValid[Index] = FALSE;
      continue;
    }

    ReturnCode = LabelIndexAreaToRawData(pLabelStorageArea, Index, &pRawData);

    if (EFI_ERROR(ReturnCode) || (pRawData == NULL)) {
      NVDIMM_DBG("Failed to convert label area index to raw data");
      FREE_POOL_SAFE(pRawData);
      goto Finish;
    }

    ReturnCode = EFI_INVALID_PARAMETER;

    IndexLength = pLabelStorageArea->Index[Index].MySize;
    ChecksumMatched = ChecksumOperations(pRawData, IndexLength, (UINT64 *)(pRawData + ChecksumOffset), FALSE);

    FREE_POOL_SAFE(pRawData);
    if (!ChecksumMatched) {
      NVDIMM_DBG("Incorrect checksum of the NAMESPACE INDEX %d.", Index);
#ifndef WA_SKIP_LSA_CHECKSUM_FAIL
      IndexesValid[Index] = FALSE;
      continue;
#endif
    }

    if (pLabelStorageArea->Index[Index].MyOffset != (Index * IndexLength) ||
        pLabelStorageArea->Index[Index].OtherOffset != (Index == 0 ? IndexLength : 0)) {
      NVDIMM_DBG("Size and/or offsets are incorrect in NAMESPACE INDEX %d.", Index);
      NVDIMM_DBG("MyOffset: %d, MySize: %d, OtherOffset: %d", pLabelStorageArea->Index[Index].MyOffset,
        pLabelStorageArea->Index[Index].MySize, pLabelStorageArea->Index[Index].OtherOffset);
      IndexesValid[Index] = FALSE;
      continue;
    }

    if (pLabelStorageArea->Index[Index].Sequence == 0 ||
        pLabelStorageArea->Index[Index].Sequence > 3) {
      NVDIMM_DBG("Invalid sequence number in the NAMESPACE INDEX %d.", Index);
      IndexesValid[Index] = FALSE;
      continue;
    }

    if (pLabelStorageArea->Index[Index].Major != NSINDEX_MAJOR) {
      NVDIMM_DBG("Index Major Version %d not supported NAMESPACE INDEX %d.", pLabelStorageArea->Index[Index].Major, Index);
      IndexesValid[Index] = FALSE;
      continue;
    }

    if (pLabelStorageArea->Index[Index].Minor != NSINDEX_MINOR_1 &&
      pLabelStorageArea->Index[Index].Minor != NSINDEX_MINOR_2) {
      NVDIMM_DBG("Index Minor Version %d not supported NAMESPACE INDEX %d.", pLabelStorageArea->Index[Index].Minor, Index);
      IndexesValid[Index] = FALSE;
      continue;
    }

    if (pLabelStorageArea->Index[Index].Minor >= NSINDEX_MINOR_2 &&
      pLabelStorageArea->Index[Index].LabelSize == 0) {
      NVDIMM_DBG("Invalid label size, Index %d: %d", Index, INDEX_LABEL_SIZE_TO_BYTE(pLabelStorageArea->Index[Index].LabelSize));
      IndexesValid[Index] = FALSE;
      continue;
    }

    if (pLabelStorageArea->Index[Index].Minor >= NSINDEX_MINOR_2 &&
      pLabelStorageArea->Index[Index].LabelSize != BYTE_TO_INDEX_LABEL_SIZE(sizeof(*(pLabelStorageArea->pLabels)))) {
      NVDIMM_DBG("Invalid label size, Index %d: %d", Index, INDEX_LABEL_SIZE_TO_BYTE(pLabelStorageArea->Index[Index].LabelSize));
      IndexesValid[Index] = FALSE;
      continue;
    }
  }

  if (!IndexesValid[FIRST_INDEX_BLOCK] && !IndexesValid[SECOND_INDEX_BLOCK]) {
    NVDIMM_DBG("No valid Index Blocks found.");
    ReturnCode = EFI_VOLUME_CORRUPTED;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pEmptyLsa);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Copies the Raw LSA Index data into proper data structures

  It's the caller's responsibility to free the memory.

  @param[in] pRawData Raw data from the PCD
  @param[in] PcdLsaPartitionSize for checking Index size
  @param[out] pLsa The Label Storage Area structure

  @retval EFI_INVALID_PARAMETER Invalid parameter passed
  @retval EFI_SUCCESS Data was copied successfully
**/
EFI_STATUS
RawDataToLabelIndexArea(
  IN     UINT8 *pRawData,
  IN     UINT32 PcdLsaPartitionSize,
     OUT LABEL_STORAGE_AREA *pLsa
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  NAMESPACE_INDEX *pNamespaceIndex = NULL;
  NAMESPACE_INDEX *pRawNamespaceIndex = NULL;
  UINT8 *pBuffer;
  UINT32 Index = 0;
  UINT64 FreeOffset = 0;
  UINT64 NumFreeBytes = 0;
  UINT64 IndexSize = 0;
  UINT64 CalculatedIndexSize = 0;
  UINT64 LabelSize = 0;
  UINT64 TotalSize = 0;

  if ((pRawData == NULL) || pLsa == NULL) {
    goto Finish;
  }

  pBuffer = pRawData;
  pRawNamespaceIndex = (NAMESPACE_INDEX *)pRawData;

  IndexSize = pRawNamespaceIndex->MySize;
  if (IndexSize == 0) {
    // Returning success as it could mean there is no LSA initialized
    // and that is fine, as this function is called while reading LSA
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }

  LabelSize = INDEX_LABEL_SIZE_TO_BYTE(pRawNamespaceIndex->LabelSize);

  // Total label space we can use is size of 2 Index blocks + size of max number of labels
  TotalSize = (IndexSize * 2) + (LabelSize * pRawNamespaceIndex->NumberOfLabels);

  FreeOffset = OFFSET_OF(NAMESPACE_INDEX, pFree);
  NumFreeBytes = LABELS_TO_FREE_BYTES(ROUNDUP(pRawNamespaceIndex->NumberOfLabels, NSINDEX_FREE_ALIGN));

  CalculatedIndexSize = ROUNDUP(FreeOffset + NumFreeBytes, NSINDEX_ALIGN);
  // Sanity checks
  // Check size in the label matches static struct size + free + align
  // Check if Index + labels based on passed in NSlot would actually fit in LSA
  if (IndexSize != CalculatedIndexSize) {
    NVDIMM_WARN("Invalid index size. Label size: %d Calculated size: %d",
      IndexSize, CalculatedIndexSize);
    ReturnCode = EFI_VOLUME_CORRUPTED;
    goto Finish;
  } else if (TotalSize > PcdLsaPartitionSize) {
    NVDIMM_WARN("Invalid index size or NSlot.  Label size: %d NSlot: %d",
      IndexSize, pRawNamespaceIndex->NumberOfLabels);
    ReturnCode = EFI_VOLUME_CORRUPTED;
    goto Finish;
  }

  for (Index = 0; Index < NAMESPACE_INDEXES; Index++) {
    pNamespaceIndex = &pLsa->Index[Index];

    CopyMem_S(pNamespaceIndex, sizeof(*pNamespaceIndex), pBuffer, FreeOffset);

    pNamespaceIndex->pFree = AllocateZeroPool(NumFreeBytes);
    if (pNamespaceIndex->pFree == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    CopyMem_S(pNamespaceIndex->pFree, NumFreeBytes, pBuffer + FreeOffset, NumFreeBytes);

    pBuffer += IndexSize;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}

/**
  Reads Label Storage Area of a specified DIMM.

  Function reads Platform Config Data partition 3. of a DIMM and invokes
  validation subroutine to check for data consistency. Required memory
  will be allocated, it's caller responsibility to free it after it's
  no longer needed.

  @param[in] DimmPid Dimm ID of DIMM from which to read the data
  @param[out] ppLsa Pointer with address at which memory
    LSA data will be stored.

  @retval EFI_INVALID_PARAMETER NULL pointer provided as a parameter
  @retval EFI_DEVICE_ERROR Unable to retrieve data from DIMM or retrieved
    data is not valid
  @retval EFI_SUCCESS Valid LSA retrieved
**/
EFI_STATUS
ReadLabelStorageArea(
  IN     UINT16 DimmPid,
     OUT LABEL_STORAGE_AREA **ppLsa
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  UINT8 *pRawData = NULL;
  UINT16 CurrentIndex = 0;
  UINT64 LabelIndexSize = 0;
  UINT64 LabelSize = 0;
  BOOLEAN UseNamespace1_1 = FALSE;
  UINT8 *pTo = NULL;
  UINT8 *pFrom = NULL;
  UINT32 Index = 0;
  UINT32 IndexSize = 0;
  UINT32 Offset = 0;
  UINT32 AlignPageIndex = 0;
  UINT32 PageSize = 0;
  UINT8 PageIndexMask = 0;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS pAttribs;
  BOOLEAN LargePayloadAvailable = FALSE;

  NVDIMM_ENTRY();


  if (ppLsa == NULL) {
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);

  if (pDimm == NULL || !IsDimmManageable(pDimm)) {

    goto Finish;
  }

  NVDIMM_DBG("Reading LSA for DIMM %x ...", pDimm->DeviceHandle.AsUint32);

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetFisTransportAttributes(pNvmDimmConfigProtocol, &pAttribs);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  CHECK_RESULT(IsLargePayloadAvailable(pDimm, &LargePayloadAvailable), Finish);
  if (!LargePayloadAvailable) {
    // At first read the Index size only form the beginning of the LSA
    IndexSize = sizeof((*ppLsa)->Index);
    ReturnCode = FwGetPCDFromOffsetSmallPayload(pDimm, PCD_LSA_PARTITION_ID, Offset, IndexSize, &pRawData);
    if (EFI_SUCCESS == ReturnCode) {
      // Read the IndexSize again plus 2 times siez of the Free Mask starting at the end of the previoues read
      Offset = IndexSize;
      IndexSize += 2 * LABELS_TO_FREE_BYTES(ROUNDUP(((LABEL_STORAGE_AREA *)pRawData)->Index[0].NumberOfLabels, NSINDEX_FREE_ALIGN));
      ReturnCode = FwGetPCDFromOffsetSmallPayload(pDimm, PCD_LSA_PARTITION_ID, Offset, IndexSize, &pRawData);
    }
  }
  else {
    ReturnCode = FwCmdGetPlatformConfigData(pDimm, PCD_LSA_PARTITION_ID, &pRawData);
  }
  if (ReturnCode == EFI_NO_MEDIA) {
    goto Finish;
  }

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("FwCmdGetPlatformConfigData returned: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

  *ppLsa = AllocateZeroPool(sizeof(**ppLsa));
  if (*ppLsa == NULL) {
    NVDIMM_WARN("Can't allocate memory Label Storage Area");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Copy the Label Index area
  ReturnCode = RawDataToLabelIndexArea(pRawData, pDimm->PcdLsaPartitionSize, *ppLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishError;
  }

  // Validate the index area
  ReturnCode = ValidateLsaData(*ppLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishError;
  }

  ReturnCode = GetLsaIndexes(*ppLsa, &CurrentIndex, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishError;
  }

  if ((*ppLsa)->Index[CurrentIndex].Major == NSINDEX_MAJOR &&
      (*ppLsa)->Index[CurrentIndex].Minor == NSINDEX_MINOR_1) {
    UseNamespace1_1 = TRUE;
  }

  LabelIndexSize = NAMESPACE_INDEXES * (*ppLsa)->Index[CurrentIndex].MySize;
  LabelSize = sizeof(*((*ppLsa)->pLabels)) * (*ppLsa)->Index[CurrentIndex].NumberOfLabels;

  NVDIMM_DBG("Read from the LSA for DIMM %x: Current index size %d :: No of labels %d", pDimm->DeviceHandle.AsUint32,
                (*ppLsa)->Index[CurrentIndex].MySize, (*ppLsa)->Index[CurrentIndex].NumberOfLabels);

  (*ppLsa)->pLabels = AllocateZeroPool(LabelSize);
  if ((*ppLsa)->pLabels == NULL) {
    FREE_POOL_SAFE(*ppLsa);
    goto FinishError;
  }

  // Copy the Label area
  if (!LargePayloadAvailable) {
    // Copy the Label area
    if (UseNamespace1_1) {
      PageSize = sizeof(NAMESPACE_LABEL_1_1);
    }
    else {
      PageSize = sizeof(NAMESPACE_LABEL);
    }

    for (AlignPageIndex = 0; AlignPageIndex < (*ppLsa)->Index[CurrentIndex].NumberOfLabels; AlignPageIndex += NSINDEX_FREE_ALIGN) {
      // Check if we have any namespaces defined for these slots
      if ((*ppLsa)->Index[CurrentIndex].pFree[LABELS_TO_FREE_BYTES(AlignPageIndex)] != FREE_BLOCKS_MASK_ALL_SET) {
        // Find the label to read
        for(PageIndexMask = (*ppLsa)->Index[CurrentIndex].pFree[LABELS_TO_FREE_BYTES(AlignPageIndex)], Index = 0;
          (Index < NSINDEX_FREE_ALIGN) && ((AlignPageIndex + Index) < (*ppLsa)->Index[CurrentIndex].NumberOfLabels);
          PageIndexMask >>= 1, Index++) {
          if (BIT0 != (PageIndexMask & BIT0)) {
            // Calculate the offest to read, one label per read only
            Offset = (UINT32)(LabelIndexSize + (PageSize * (AlignPageIndex + Index)));
            // Read data
            ReturnCode = FwGetPCDFromOffsetSmallPayload(pDimm, PCD_LSA_PARTITION_ID, Offset, PageSize, &pRawData);
            // Copy data to the LSA struct
            pFrom = pRawData + Offset;
            pTo = ((UINT8 *)(*ppLsa)->pLabels) + (sizeof(NAMESPACE_LABEL) * (AlignPageIndex + Index));
            CopyMem_S(pTo, PageSize, pFrom, PageSize);
          }
        }
      }
    }
  }
  else {
    if (UseNamespace1_1) {
      pTo = (UINT8 *)(*ppLsa)->pLabels;
      pFrom = pRawData + LabelIndexSize;

      for (Index = 0; Index < (*ppLsa)->Index[CurrentIndex].NumberOfLabels; Index++) {
        CopyMem_S(pTo, sizeof(NAMESPACE_LABEL_1_1), pFrom, sizeof(NAMESPACE_LABEL_1_1));
        pTo += sizeof(*((*ppLsa)->pLabels));
        pFrom += sizeof(NAMESPACE_LABEL_1_1);
      }
        }
    else {
      CopyMem_S((*ppLsa)->pLabels, LabelSize, pRawData + LabelIndexSize, LabelSize);
    }
  }
  ReturnCode = EFI_SUCCESS;

  goto Finish;

FinishError:
  FreeLsaSafe(ppLsa);

Finish:
  FREE_POOL_SAFE(pRawData);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Writes Label Storage Area to a specified DIMM.

  Function invokes validation subroutine to check provided data consistency
  and then stores LSA on Platform Config Data partition 3. of a DIMM

  @param[in] DimmPid Dimm ID of DIMM on which to write LSA
  @param[in] pLsa Pointer with LSA structure

  @retval EFI_INVALID_PARAMETER No data provided or the data is invalid
  @retval EFI_DEVICE_ERROR Unable to store data on a DIMM
  @retval EFI_SUCCESS LSA written correctly
**/
EFI_STATUS
WriteLabelStorageArea(
  IN     UINT16 DimmPid,
  IN     LABEL_STORAGE_AREA *pLsa
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  UINT8 *pRawData = NULL;
  UINT64 TotalPcdSize = 0;
  UINT8 *pTo = NULL;
  UINT32 Index = 0;
  UINT64 LabelSize = 0;
  UINT16 CurrentIndex = 0;
  UINT64 LabelIndexSize = 0;
  UINT8 *pIndexArea = NULL;
  BOOLEAN UseNamespace_1_1 = FALSE;
  UINT8 *pFrom = NULL;
  UINT32 AlignPageIndex = 0;
  UINT32 PageSize = 0;
  UINT8 PageIndexMask = 0;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  EFI_DCPMM_CONFIG_TRANSPORT_ATTRIBS pAttribs;
  BOOLEAN LargePayloadAvailable = FALSE;

  NVDIMM_ENTRY();

  if (pLsa == NULL) {
    goto Finish;
  }

  ReturnCode = ValidateLsaData(pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL || !IsDimmManageable(pDimm)) {
    goto Finish;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, NULL);

  LabelIndexSize = NAMESPACE_INDEXES * pLsa->Index[CurrentIndex].MySize;
  LabelSize = sizeof(*(pLsa->pLabels)) * pLsa->Index[CurrentIndex].NumberOfLabels;
  TotalPcdSize = LabelIndexSize + LabelSize;

  if ((pLsa->Index[CurrentIndex].Major == NSINDEX_MAJOR) &&
      (pLsa->Index[CurrentIndex].Minor == NSINDEX_MINOR_1)) {
     UseNamespace_1_1 = TRUE;
  }

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetFisTransportAttributes(pNvmDimmConfigProtocol, &pAttribs);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  CHECK_RESULT(IsLargePayloadAvailable(pDimm, &LargePayloadAvailable), Finish);
  if (LargePayloadAvailable) {
    pRawData = AllocateZeroPool(TotalPcdSize);
    if (pRawData == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
  }

  ReturnCode = LabelIndexAreaToRawData(pLsa, ALL_INDEX_BLOCKS, &pIndexArea);

  if (EFI_ERROR(ReturnCode) || (pIndexArea == NULL)) {
    NVDIMM_DBG("Failed to convert label area index to raw data");
    goto Finish;
  }

  if (!LargePayloadAvailable) {
    // Copy the Label index area
    ReturnCode = FwSetPCDFromOffsetSmallPayload(pDimm, PCD_LSA_PARTITION_ID, pIndexArea, 0, (UINT32)LabelIndexSize);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    // Copy the Label area
    if (UseNamespace_1_1) {
      PageSize = sizeof(NAMESPACE_LABEL_1_1);
    }
    else {
      PageSize = sizeof(NAMESPACE_LABEL);
    }

    for (AlignPageIndex = 0; AlignPageIndex < pLsa->Index[CurrentIndex].NumberOfLabels; AlignPageIndex += NSINDEX_FREE_ALIGN) {
      // Check if we have at least one namespace to copy
      if (pLsa->Index[CurrentIndex].pFree[LABELS_TO_FREE_BYTES(AlignPageIndex)] != FREE_BLOCKS_MASK_ALL_SET) {
        // Find the label to write
        for (PageIndexMask = pLsa->Index[CurrentIndex].pFree[LABELS_TO_FREE_BYTES(AlignPageIndex)], Index = 0;
          (Index < NSINDEX_FREE_ALIGN) && ((AlignPageIndex + Index) < pLsa->Index[CurrentIndex].NumberOfLabels);
          PageIndexMask >>= 1, Index++) {
          if (BIT0 != (PageIndexMask & BIT0)) {
            // Calculate the offset to write, one label per write only
            pFrom = ((UINT8 *)(pLsa->pLabels) + (sizeof(NAMESPACE_LABEL) * (AlignPageIndex + Index)));
            ReturnCode = FwSetPCDFromOffsetSmallPayload(pDimm, PCD_LSA_PARTITION_ID, pFrom, (UINT32)(LabelIndexSize + (PageSize * (AlignPageIndex + Index))), PageSize);
            if (EFI_ERROR(ReturnCode)) {
              goto Finish;
            }
          }
        }
      }
    }
  }
  else {
    // Copy the Label index area, but check for NULL first
    CHECK_NOT_TRUE((NULL != pRawData && NULL != pIndexArea), Finish);
    CopyMem_S(pRawData, TotalPcdSize, pIndexArea, LabelIndexSize);

    // Copy the label area
    if (UseNamespace_1_1) {
      pFrom = (UINT8 *)pLsa->pLabels;
      pTo = pRawData + LabelIndexSize;

      for (Index = 0; Index < pLsa->Index[CurrentIndex].NumberOfLabels; Index++) {
        CopyMem_S(pTo, sizeof(NAMESPACE_LABEL_1_1), pFrom, sizeof(NAMESPACE_LABEL_1_1));
        pFrom += sizeof(*(pLsa->pLabels));
        pTo += sizeof(NAMESPACE_LABEL_1_1);
      }
    }
    else {
      CopyMem_S(pRawData + LabelIndexSize, TotalPcdSize - LabelIndexSize, pLsa->pLabels, LabelSize);
    }

    NVDIMM_DBG("Writing LSA to DIMM %x ...", pDimm->DeviceHandle.AsUint32);
    ReturnCode = FwCmdSetPlatformConfigData(pDimm, PCD_LSA_PARTITION_ID,
      pRawData, pDimm->PcdLsaPartitionSize);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("FwCmdSetPlatformConfigData returned: " FORMAT_EFI_STATUS "", ReturnCode);
      goto Finish;
    }
  }

Finish:
  FREE_POOL_SAFE(pIndexArea);
  FREE_POOL_SAFE(pRawData);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Zero the Label Storage Area on the specified DIMM.

  @param[in] DimmPid Dimm ID of DIMM on which to write LSA

  @retval EFI_INVALID_PARAMETER No data provided or the data is invalid
  @retval EFI_OUT_RESOURCES Unable to allocate resources
  @retval EFI_SUCCESS LSA written correctly
**/
EFI_STATUS
ZeroLabelStorageArea(
  IN     UINT16 DimmPid
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  UINT8 *pZeroRawLsa = NULL;

  NVDIMM_ENTRY();

  pDimm = GetDimmByPid(DimmPid, &gNvmDimmData->PMEMDev.Dimms);
  if (pDimm == NULL || !IsDimmManageable(pDimm)) {
    goto Finish;
  }

  pZeroRawLsa = AllocateZeroPool(pDimm->PcdLsaPartitionSize);
  if (pZeroRawLsa == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  NVDIMM_DBG("Zero-ing the LSA on DIMM 0x%x ...", pDimm->DeviceHandle.AsUint32);
  ReturnCode = FwCmdSetPlatformConfigData(pDimm, PCD_LSA_PARTITION_ID,
    pZeroRawLsa, pDimm->PcdLsaPartitionSize);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("FwCmdSetPlatformConfigData returned: " FORMAT_EFI_STATUS "", ReturnCode);
    goto Finish;
  }

Finish:
  FREE_POOL_SAFE(pZeroRawLsa);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  GetNamespace
  Looks through the list of namespaces, searching for a namespace with a particular GUID.

  @param[in] pNamespacesList pointer to the list where the function is supposed to search for the Namespace.
  @param[in] Uuid the target UUID of the Namespace that the function is searching for.
  @param[out] ppNamespace pointer to a pointer of where the Namespace pointer will be stored.

  @retval FALSE At least one of the input pointers is NULL or there is no Namespace with
    the specified UUID on the list.
  @retval TRUE the Namespace has been found and is stored under the ppNamespace.
**/
STATIC
BOOLEAN
GetNamespace(
 IN     LIST_ENTRY *pNamespacesList,
 IN     GUID Uuid,
    OUT NAMESPACE **ppNamespace
  )
{
  LIST_ENTRY *pNamespaceNode = NULL;
  NAMESPACE *pNamespace = NULL;

  if (pNamespacesList == NULL || ppNamespace == NULL) {
    return FALSE;
  }

  for (pNamespaceNode = GetFirstNode(&gNvmDimmData->PMEMDev.Namespaces);
        !IsNull(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode);
        pNamespaceNode = GetNextNode(&gNvmDimmData->PMEMDev.Namespaces, pNamespaceNode)) {
    pNamespace = NAMESPACE_FROM_NODE(pNamespaceNode, NamespaceNode);
    if (CompareMem(pNamespace->NamespaceGuid, &Uuid, sizeof(pNamespace->NamespaceGuid)) == 0) {
      // We have a match!
      *ppNamespace = pNamespace;
      return TRUE;
    }
  }

  return FALSE;
}

/**
  GetNamespaceByName
  Looks through namespaces list searching for a namespace with a particular name.

  @param[in] pName Target Name of the Namespace that the function is searching for.

  @retval NAMESPACE structure pointer if Namespace has been found
  @retval NULL pointer if not found
**/
NAMESPACE*
GetNamespaceByName(
  IN     CHAR8 *pName
  )
{
  LIST_ENTRY *pNode = NULL;
  NAMESPACE *pNamespace = NULL;
  NAMESPACE *pTargetNamespace = NULL;

  if (pName == NULL) {
    goto Finish;
  }

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Namespaces) {
    pNamespace = NAMESPACE_FROM_NODE(pNode, NamespaceNode);
    if (CompareMem(pNamespace->Name, pName, sizeof(pNamespace->Name)) == 0) {
      pTargetNamespace = pNamespace;
      goto Finish;
    }
  }

Finish:
  return pTargetNamespace;
}

/**
  Looks through namespaces list searching for a namespace with a particular id.

  @param[in] NamespaceId Target ID of the Namespace that the function is searching for.

  @retval NAMESPACE structure pointer if Namespace has been found
  @retval NULL pointer if not found
**/
NAMESPACE*
GetNamespaceById(
 IN     UINT16 NamespaceId
  )
{
  LIST_ENTRY *pNode = NULL;
  NAMESPACE *pNamespace = NULL;
  NAMESPACE *pTargetNamespace = NULL;

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Namespaces) {
    pNamespace = NAMESPACE_FROM_NODE(pNode, NamespaceNode);
    if (pNamespace->NamespaceId == NamespaceId) {
      pTargetNamespace = pNamespace;
      goto Finish;
    }
  }

Finish:
  return pTargetNamespace;
}

/**
  Function checks if first data of a Namespace is a BTT Arena Info Block

  @param[in]    pNamespace  Namespace to be verified
  @param[out]   pBttFound   BTT existence flag
  @retval   EFI_SUCCESS Verification passed
  @retval   EFI_OUT_OF_RESOURCES Memory allocation failed
  @retval   EFI_VOLUME_CORRUPTED BTT found but is inconsistent
  @retval   EFI_INVALID_PARAMETER One of input parameters is NULL
**/
EFI_STATUS
CheckBttExistence(
  IN     NAMESPACE *pNamespace,
     OUT BOOLEAN *pBttFound
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BTT_INFO *pBttInfo = NULL;
  UINT64 BttInfoOffset = BTT_PRIMARY_INFO_BLOCK_OFFSET;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pBttFound == NULL) {
    goto Finish;
  }

  if ((pNamespace->Major == NSINDEX_MAJOR) &&
      (pNamespace->Minor == NSINDEX_MINOR_1)) {
    BttInfoOffset = BTT_PRIMARY_INFO_BLOCK_OFFSET_1_1;
  }

  *pBttFound = FALSE;

  pBttInfo = AllocateZeroPool(sizeof(BTT_INFO));
  if (pBttInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Check primary BTT info block of first arena
  ReturnCode = ReadNamespaceBytes(pNamespace, BttInfoOffset, pBttInfo, sizeof(BTT_INFO));
  if(EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to read namespace bytes");
    goto Finish;
  }
  ReturnCode = BttReadInfo(pBttInfo, pNamespace->pBtt);
  if(!EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Valid primary BTT_INFO block found.");
    *pBttFound = TRUE;
  }

  // Check backup BTT info block of first arena if primary info block wasn't found
  // Currently no recovery occurs, we just detect the backup info block here.
  if (*pBttFound == FALSE) {
    if (GetAccessibleCapacity(pNamespace) > BTT_MAX_ARENA_SIZE) {
      BttInfoOffset = BTT_MAX_ARENA_SIZE - sizeof(BTT_INFO);
    } else {
      BttInfoOffset = GetAccessibleCapacity(pNamespace) - sizeof(BTT_INFO);
    }
    ReturnCode = ReadNamespaceBytes(pNamespace, BttInfoOffset, pBttInfo, sizeof(BTT_INFO));
    if(EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to read namespace bytes");
      goto Finish;
    }
    ReturnCode = BttReadInfo(pBttInfo, pNamespace->pBtt);
    if(!EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Valid backup BTT_INFO block found but recovery is not supported.");
      *pBttFound = TRUE;
    }
  }

  // BTT GUID must match parent (Namespace) GUID
  if (*pBttFound == TRUE && CompareMem(&pNamespace->NamespaceGuid, &pBttInfo->ParentUuid, sizeof(GUID)) != 0) {
    NVDIMM_DBG("BTT and Namespace GUID don't match:");
    NVDIMM_DBG("BTT Parent GUID: %g", pBttInfo->ParentUuid);
    NVDIMM_DBG("NS  GUID: %g", pNamespace->NamespaceGuid);
    *pBttFound = FALSE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pBttInfo);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Function checks if first data of a Namespace is a PFN info block

  @param[in]    pNamespace  Namespace to be verified
  @param[out]   pPfnFound   PFN existence flag
  @retval   EFI_SUCCESS Verification passed
  @retval   EFI_OUT_OF_RESOURCES Memory allocation failed
  @retval   EFI_VOLUME_CORRUPTED PFN found but is inconsistent
  @retval   EFI_INVALID_PARAMETER One of input parameters is NULL
**/
EFI_STATUS
CheckPfnExistence(
  IN     NAMESPACE *pNamespace,
     OUT BOOLEAN *pPfnFound
  )
{
  PFN_INFO *pPfnInfo = NULL;
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pPfnFound == NULL) {
    goto Finish;
  }

  *pPfnFound = FALSE;

  pPfnInfo = AllocateZeroPool(sizeof(PFN_INFO));
  if (pPfnInfo == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = ReadNamespaceBytes(pNamespace, PFN_INFO_BLOCK_OFFSET, pPfnInfo, sizeof(PFN_INFO));
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to read namespace bytes");
    goto Finish;
  }

  ReturnCode = PfnValidateInfo(pPfnInfo, NULL);
  if (!EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Valid PFN_INFO block found.");
    *pPfnFound = TRUE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPfnInfo);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

#if 0
// This flow may not work properly under certain scenarios.
/**
  Recover a partially updated namespace label set
  Clear the updating bit and use the name from label in pos 0

  @param[in] pUuid of the label to perfrom recovery on
  @param[in out] pNamespace - namespace struct that needs to be updated after recovery
  @param[in out] pNamespaceLabelStale - namespace label struct that needs to be updated after recovery
**/
STATIC
EFI_STATUS
RecoverLabelSet(
  IN     GUID *pUuid,
  IN OUT NAMESPACE *pNamespace,
  IN OUT NAMESPACE_LABEL *pNamespaceLabelStale
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  DIMM *pDimm = NULL;
  NAMESPACE_LABEL *pNamespaceLabel = NULL;
  LIST_ENTRY *pNode = NULL;
  CHAR8 *pNamespaceLabelName = NULL;
  LABEL_FLAGS Flags;
  UINT32 LabelsFound = 0;
  BOOLEAN Pos0Found = FALSE;


  NVDIMM_ENTRY();

  ZeroMem(&Flags, sizeof(Flags));

  if (pNamespace == NULL || pNamespaceLabelStale == NULL) {
    goto Finish;
  }

  pNamespaceLabelName = AllocateZeroPool(NLABEL_NAME_LEN_WITH_TERMINATOR);

  if (pNamespaceLabelName == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  // Find position 0 and store the name
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    ReturnCode = GetAppDirectLabelByUUID(pDimm, *pUuid, &pNamespaceLabel);
    if (ReturnCode == EFI_NOT_FOUND || ReturnCode ==  EFI_NO_MEDIA) {
      continue;
    }

    if (EFI_ERROR(ReturnCode)) {
      FREE_POOL_SAFE(pNamespaceLabel);
      goto Finish;
    }

    LabelsFound++;

    if (pNamespaceLabel->Position == 0) {
      CopyMem_S(pNamespaceLabelName, NLABEL_NAME_LEN_WITH_TERMINATOR, &pNamespaceLabel->Name, NSLABEL_NAME_LEN);
      Pos0Found = TRUE;
    }
    FREE_POOL_SAFE(pNamespaceLabel);
  }

  // Check if the set is complete
  if (pNamespaceLabelStale->NumberOfLabels != LabelsFound) {
    NVDIMM_DBG("Invalid number of labels found! Expected %d, got %d",
        pNamespaceLabelStale->NumberOfLabels, LabelsFound);
    ReturnCode =  EFI_ABORTED;
    goto Finish;
  }

  // Check if position 0 was found
  if (!Pos0Found) {
    NVDIMM_DBG("Could not find Position 0 label.");
    ReturnCode = EFI_ABORTED;
    goto Finish;
  }

  // Write each label label in the set with pos 0 name and UPDATING clear
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    ReturnCode = GetAppDirectLabelByUUID(pDimm, *pUuid, &pNamespaceLabel);
    if (ReturnCode == EFI_NOT_FOUND) {
      continue;
    }

    if (EFI_ERROR(ReturnCode)) {
      FREE_POOL_SAFE(pNamespaceLabel);
      goto Finish;
    }

    // Clear the UPDATING bit
    Flags.AsUint32 = pNamespaceLabel->Flags.AsUint32;
    Flags.Values.Updating = 0;

    ReturnCode = ModifyNamespaceLabels(pDimm, pUuid, &Flags.AsUint32, pNamespaceLabelName, 0);
    if (EFI_ERROR(ReturnCode)) {
      FREE_POOL_SAFE(pNamespaceLabel);
      goto Finish;
    }
    FREE_POOL_SAFE(pNamespaceLabel);
  }

  // Our structs for pNamespace and pNamespaceLabel in RetrieveNamespacesFromLsa are now out of date
  // Update the UPDATING bit and name here saving another LSA Storage read
  pNamespace->Flags.Values.Updating = 0;
  CopyMem_S(&pNamespace->Name, sizeof(pNamespace->Name), pNamespaceLabelName, NSLABEL_NAME_LEN);

  pNamespaceLabelStale->Flags.Values.Updating = 0;
  CopyMem_S(&pNamespaceLabelStale->Name, sizeof(pNamespaceLabelStale->Name), pNamespaceLabelName, NSLABEL_NAME_LEN);

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pNamespaceLabelName);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif

/**
  Retrieve Namespaces information from provided LSA structure.

  Function scans Namespaces Index of LSA and reads Namespace slots
  for Namespace data. Memory for required structures is allocated
  and it's callee responsibility to free it when it's no longer needed.

  @param[in] pDimm Pointer to a DIMM structure to which LSA relates
  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[out] pNamespacesList Pointer to a list to which to add Namespace structures

  @retval EFI_INVALID_PARAMETER No data provided or the data is invalid
  @retval EFI_OUT_OF_RESOURCES Memory allocation failure
  @retval EFI_SUCCESS Namespaces correctly retrieved
**/
EFI_STATUS
RetrieveNamespacesFromLsa(
  IN     DIMM *pDimm,
  IN     ParsedFitHeader *pFitHead,
     OUT LIST_ENTRY *pNamespacesList
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_STATUS ReturnCode2 = EFI_INVALID_PARAMETER;
  NAMESPACE_LABEL *pNamespaceLabel = NULL;
  NAMESPACE_LABEL *pNamespaceLabel2 = NULL;
  NAMESPACE *pNamespace = NULL;
  LIST_ENTRY *pNode = NULL;
  BOOLEAN ChecksumMatch = FALSE;
  UINT16 CurrentIndex = 0;
  UINT16 SlotStatus = SLOT_UNKNOWN;
  UINT32 Index = 0;
  UINT32 LabelsFound = 0;
  BOOLEAN BttFound = FALSE;
  BOOLEAN PfnFound = FALSE;
  UINT64 RawCapacity = 0;
  BOOLEAN Use_Namespace1_1 = FALSE;
  EFI_GUID ZeroGuid;
  LABEL_STORAGE_AREA *pLsa = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&ZeroGuid, sizeof(EFI_GUID));

  if (pNamespacesList == NULL) {
    NVDIMM_DBG("pNamespacesList is NULL");
    goto Finish;
  }

  if (pDimm == NULL) {
    NVDIMM_DBG("pDimm is NULL");
    goto Finish;
  }

  pLsa = pDimm->pLsa;
  if (pLsa == NULL) {
    NVDIMM_DBG("pLsa is NULL");
    goto Finish;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (pLsa->Index[CurrentIndex].Major == NSINDEX_MAJOR &&
      pLsa->Index[CurrentIndex].Minor == NSINDEX_MINOR_1) {
    Use_Namespace1_1 = TRUE;
  }

  for (Index = 0; Index < pLsa->Index[CurrentIndex].NumberOfLabels; Index++) {
    RawCapacity = 0;
    CheckSlotStatus(&pLsa->Index[CurrentIndex], (UINT16)Index, &SlotStatus);
    if (SlotStatus == SLOT_FREE) {
      continue;
    }
    NVDIMM_DBG("Label found at slot %d", Index);
    pNamespaceLabel = &pLsa->pLabels[Index];

    if (GetNamespace(pNamespacesList, pNamespaceLabel->Uuid, &pNamespace)) {
      NVDIMM_DBG("Namespace for this label already initialized");
      continue;
    }

    ReturnCode = ValidateNamespaceLabel(pNamespaceLabel, Use_Namespace1_1);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Label invalid or check failed. Skipping");
      continue;
    }

    pNamespace = (NAMESPACE *) AllocateZeroPool(sizeof(*pNamespace));
    if (pNamespace == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    pNamespace->Flags.AsUint32 = pNamespaceLabel->Flags.AsUint32;
    pNamespace->Signature = NAMESPACE_SIGNATURE;
    pNamespace->Enabled = FALSE;
    pNamespace->HealthState = NAMESPACE_HEALTH_OK;
    CopyMem_S(&pNamespace->Name, sizeof(pNamespace->Name), &pNamespaceLabel->Name, NSLABEL_NAME_LEN);
    CopyMem_S(&pNamespace->NamespaceGuid, sizeof(pNamespace->NamespaceGuid), &pNamespaceLabel->Uuid, NSGUID_LEN);
    pNamespace->InterleaveSetCookie = pNamespaceLabel->InterleaveSetCookie;
    pNamespace->Major = NSINDEX_MAJOR;
    if (Use_Namespace1_1) {
      pNamespace->Minor = NSINDEX_MINOR_1;
    } else {
      pNamespace->Minor = NSINDEX_MINOR_2;
    }

    LabelsFound = 0;
    if (IsNameSpaceTypeAppDirect(pNamespaceLabel, Use_Namespace1_1)) {
      if (!Use_Namespace1_1 &&
        !(CompareGuid(&gSpaRangeIsoPmRegionGuid, &pNamespaceLabel->TypeGuid) ||
          CompareGuid(&gSpaRangeVolatileRegionGuid, &pNamespaceLabel->TypeGuid) ||
          CompareGuid(&gSpaRangeIsoVolatileRegionGuid, &pNamespaceLabel->TypeGuid) ||
          CompareGuid(&gSpaRangePmRegionGuid, &pNamespaceLabel->TypeGuid) ||
          CompareGuid(&gSpaRangeRawPmRegionGuid, &pNamespaceLabel->TypeGuid) ||
          CompareGuid(&gSpaRangeRawVolatileRegionGuid, &pNamespaceLabel->TypeGuid))) {
        NVDIMM_DBG("Unexpected TypeGuid for AppDirect NS");
        continue;
      }

#if 0
      // This flow is incomplete and may result in corrupt LSA
      // OSV's advise preference for UEFI to skip any automated recovery
      // Leave recovery to the OS
      // Logic after still validates the labels are consistent

      // Iterate over DIMMs to check for partial update
      LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
        pDimm = DIMM_FROM_NODE(pNode);

        if (!IsDimmManageable(pDimm)) {
          continue;
        }

        ReturnCode = GetAppDirectLabelByUUID(pDimm, pNamespaceLabel->Uuid, &pNamespaceLabel2);
        if (ReturnCode == EFI_NOT_FOUND) {
          continue;
        }

        if (EFI_ERROR(ReturnCode)) {
          FREE_POOL_SAFE(pNamespaceLabel2);
          break;
        }

        if (pNamespaceLabel2->Flags.Values.Updating) {
          NVDIMM_WARN("Partial update of namespace labels detected. Performing recovery.");
          ReturnCode = RecoverLabelSet(&pNamespaceLabel->Uuid, pNamespace, pNamespaceLabel);
          if (EFI_ERROR(ReturnCode)) {
            FREE_POOL_SAFE(pNamespaceLabel2);
            NVDIMM_WARN("Failed to recover namespace labels.");
            pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
            break;
          }
        }
        FREE_POOL_SAFE(pNamespaceLabel2);
      }
#endif

      // Iterate over DIMMs to collect labels to assemble AppDirect NS
      LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
        pDimm = DIMM_FROM_NODE(pNode);

        if (!IsDimmManageable(pDimm)) {
          continue;
        }

        ReturnCode = GetAppDirectLabelByUUID(pDimm, pNamespaceLabel->Uuid, &pNamespaceLabel2);
        if (ReturnCode == EFI_NOT_FOUND || ReturnCode == EFI_NO_MEDIA) {
          continue;
        }

        if (EFI_ERROR(ReturnCode)) {
          FREE_POOL_SAFE(pNamespaceLabel2);
          goto Finish;
        }

        // Check labels consistency
        if (pNamespaceLabel->Flags.AsUint32 != pNamespaceLabel2->Flags.AsUint32 ||
            pNamespaceLabel->LbaSize != pNamespaceLabel2->LbaSize ||
            CompareMem(pNamespaceLabel->Name, pNamespaceLabel2->Name, sizeof(pNamespaceLabel->Name)) != 0 ||
            pNamespace->InterleaveSetCookie != pNamespaceLabel2->InterleaveSetCookie) {
          FREE_POOL_SAFE(pNamespaceLabel2);
          NVDIMM_DBG("AppDirect Namespace labels are not consistent. Skipping the current label.");
          continue;
        }

        // Only support 512 or 4k LbaSize
        // Linux treats 0 as 512
        if (pNamespaceLabel2->LbaSize == 0 ||
            pNamespaceLabel2->LbaSize == AD_NAMESPACE_LABEL_LBA_SIZE_512) {
          pNamespace->Media.BlockSize = AD_NAMESPACE_LABEL_LBA_SIZE_512;
        } else if (pNamespaceLabel2->LbaSize == AD_NAMESPACE_LABEL_LBA_SIZE_4K) {
            pNamespace->Media.BlockSize = AD_NAMESPACE_LABEL_LBA_SIZE_4K;
        } else {
          NVDIMM_WARN("Unsupported LbaSize %lld", pNamespaceLabel2->LbaSize);
          pNamespace->Media.BlockSize = AD_NAMESPACE_LABEL_LBA_SIZE_512;
          pNamespace->HealthState = NAMESPACE_HEALTH_WARNING;
        }

        ReturnCode = ValidateNamespaceLabel(pNamespaceLabel2, Use_Namespace1_1);
        if (EFI_ERROR(ReturnCode)) {
          FREE_POOL_SAFE(pNamespaceLabel2);
          NVDIMM_DBG("Label invalid or check failed. Skipping");
          continue;
        }

        ChecksumMatch =
          ChecksumOperations(pNamespaceLabel2, sizeof(*pNamespaceLabel2), &pNamespaceLabel2->Checksum, FALSE);
        if (!Use_Namespace1_1 && !ChecksumMatch) {
          FREE_POOL_SAFE(pNamespaceLabel2);
          pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
          continue;
        }

        if (!Use_Namespace1_1 &&
           !(CompareGuid(&gSpaRangeIsoPmRegionGuid, &pNamespaceLabel2->TypeGuid) ||
             CompareGuid(&gSpaRangeVolatileRegionGuid, &pNamespaceLabel2->TypeGuid) ||
             CompareGuid(&gSpaRangeIsoVolatileRegionGuid, &pNamespaceLabel2->TypeGuid) ||
             CompareGuid(&gSpaRangePmRegionGuid, &pNamespaceLabel2->TypeGuid) ||
             CompareGuid(&gSpaRangeRawPmRegionGuid, &pNamespaceLabel2->TypeGuid) ||
             CompareGuid(&gSpaRangeRawVolatileRegionGuid, &pNamespaceLabel2->TypeGuid))) {
          FREE_POOL_SAFE(pNamespaceLabel2);
          NVDIMM_DBG("Unexpected TypeGuid for AppDirect NS");
          continue;
        }

        if (!Use_Namespace1_1) {
          NVDIMM_DBG("Check Abstaction GUID: %g", pNamespaceLabel2->AddressAbstractionGuid);
          if (CompareGuid(&gBttAbstractionGuid, &pNamespaceLabel2->AddressAbstractionGuid)) {
            pNamespace->IsBttEnabled = TRUE;
          } else if (CompareGuid(&gPfnAbstractionGuid, &pNamespaceLabel2->AddressAbstractionGuid)) {
            pNamespace->IsPfnEnabled = TRUE;
          } else if (CompareGuid(&ZeroGuid, &pNamespaceLabel2->AddressAbstractionGuid)) {
            pNamespace->IsRawNamespace = TRUE;
          }
        }

// Save an extra FIS call in OS. OS just needs to size for region capacity used
#ifndef OS_BUILD
        UINT32 SecurityState = 0;
        ReturnCode = GetDimmSecurityState(pDimm, PT_TIMEOUT_INTERVAL, &SecurityState);
        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_DBG("Failed to get DIMM security state.");
          goto Finish;
        }

        if (!IsConfiguringAllowed(SecurityState)) {
          NVDIMM_DBG("Namespace contains a locked DIMM 0x%X", pDimm->DeviceHandle.AsUint32);
          pNamespace->HealthState = NAMESPACE_HEALTH_LOCKED;
        }
#endif

        LabelsFound++;
        NVDIMM_DBG("Label (%d/%d) of App Direct Namespace %g foundDPA = 0x%lx",
          LabelsFound, pNamespaceLabel->NumberOfLabels, pNamespace->NamespaceGuid, pNamespaceLabel2->Dpa);
        pNamespace->Range[pNamespace->RangesCount].Dpa = pNamespaceLabel2->Dpa;
        pNamespace->Range[pNamespace->RangesCount].Size = pNamespaceLabel2->RawSize;
        pNamespace->Range[pNamespace->RangesCount].pDimm = pDimm;
        pNamespace->RangesCount += 1;
        RawCapacity += pNamespaceLabel2->RawSize;
        FREE_POOL_SAFE(pNamespaceLabel2);
      }

      pNamespace->BlockSize = AD_NAMESPACE_BLOCK_SIZE;
      pNamespace->BlockCount = RawCapacity;
      pNamespace->UsableSize = RawCapacity;
      pNamespace->NamespaceType = APPDIRECT_NAMESPACE;

      // Find parent Interleave Set
      ReturnCode = FindAndAssignISForNamespace(pNamespace);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Interleave Set has not been found for App Direct Namespace.");
        pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
      }

      if (pNamespace->pParentIS != NULL && pNamespace->HealthState != NAMESPACE_HEALTH_OK) {
        if (pNamespace->pParentIS->State != IS_STATE_HEALTHY) {
          switch (pNamespace->pParentIS->State) {
          case IS_STATE_INIT_FAILURE:
          case IS_STATE_DIMM_MISSING:
            if (pNamespace->pParentIS->MirrorEnable) {
              pNamespace->HealthState = NAMESPACE_HEALTH_WARNING;
            }
            else {
              pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
            }
            break;
          case IS_STATE_CONFIG_INACTIVE:
          case IS_STATE_SPA_MISSING:
          default:
            pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
            break;
          }
        }
      }
      else if (pNamespace->pParentIS == NULL) {
        // We will hit this case if there are no IS in the system or if we couldn't find any IS
        // with a valid cookie to match the labels.
        FREE_POOL_SAFE(pNamespace);
        continue;
      }

      // Sanity check
      if (pNamespaceLabel->NumberOfLabels != LabelsFound) {
        NVDIMM_DBG("Invalid number of labels found! Expected %d, got %d",
            pNamespaceLabel->NumberOfLabels, LabelsFound);
        pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
        pNamespace->Enabled = FALSE;
      }
    }
    // Get the Index of the last namespace Id for the previous interleave set
    pNamespace->NamespaceId = CREATE_NAMESPACE_ID(pNamespace->pParentIS->RegionId, Index);
    InsertTailList(pNamespacesList, &pNamespace->NamespaceNode);

    if (pNamespace->HealthState != NAMESPACE_HEALTH_CRITICAL &&
        pNamespace->HealthState != NAMESPACE_HEALTH_UNSUPPORTED &&
        pNamespace->HealthState != NAMESPACE_HEALTH_LOCKED) {
      if (Use_Namespace1_1 || pNamespace->IsBttEnabled || pNamespace->IsPfnEnabled) {
        ReturnCode = EFI_INVALID_PARAMETER;
        ReturnCode2 = EFI_INVALID_PARAMETER;

#ifndef OS_BUILD
        BttFound = FALSE;
        PfnFound = FALSE;
        if (Use_Namespace1_1 || pNamespace->IsBttEnabled) {
          ReturnCode = CheckBttExistence(pNamespace, &BttFound);
        }

        if (Use_Namespace1_1 || pNamespace->IsPfnEnabled) {
          ReturnCode2 = CheckPfnExistence(pNamespace, &PfnFound);
        }
#endif

        if ((EFI_ERROR(ReturnCode) && EFI_ERROR(ReturnCode2))  || (BttFound && PfnFound)) {
          NVDIMM_DBG("Failed to check address abstraction existence");
          pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
        }
        if (BttFound) {
          NVDIMM_DBG("BTT detected");
          pNamespace->IsBttEnabled = TRUE;
        } else if (PfnFound) {
          NVDIMM_DBG("PFN detected");
          pNamespace->IsPfnEnabled = TRUE;
        } else {
          NVDIMM_DBG("Address abstraction not detected");
          pNamespace->IsBttEnabled = FALSE;
          pNamespace->IsPfnEnabled = FALSE;
          pNamespace->IsRawNamespace = TRUE;
          if (!Use_Namespace1_1) {
            // Set health state critical only for v1.2
            pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
          }
        }
        if (pNamespace->IsBttEnabled) {
          pNamespace->pBtt = BttInit(
            GetAccessibleCapacity(pNamespace),
            (UINT32)GetBlockDeviceBlockSize(pNamespace),
            (GUID *) pNamespace->NamespaceGuid,
            pNamespace
            );
          if (pNamespace->pBtt == NULL) {
            NVDIMM_DBG("Failed to initialize the BTT. Namespace GUID: %g", (GUID *)pNamespace->NamespaceGuid);
            pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
          }
        } else if (pNamespace->IsPfnEnabled) {
          ReturnCode = PfnInit(
            GetAccessibleCapacity(pNamespace),
            (UINT32)GetBlockDeviceBlockSize(pNamespace),
            (GUID *) pNamespace->NamespaceGuid,
            pNamespace
            );

          if (EFI_ERROR(ReturnCode)) {
            NVDIMM_DBG("Failed to initialize the PFN. Namespace GUID: %g", (GUID *)pNamespace->NamespaceGuid);
            pNamespace->HealthState = NAMESPACE_HEALTH_CRITICAL;
          }
        }
      }
    }

    if (pNamespace->NamespaceType == APPDIRECT_NAMESPACE &&
          (
          pNamespace->HealthState == NAMESPACE_HEALTH_OK ||
          pNamespace->HealthState == NAMESPACE_HEALTH_WARNING
          )
        ) {
      pNamespace->Enabled = TRUE;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
/**
Checks whether NamespaceType is AppDirect

Function reads the label version and NameSpaceLabel and
determines whether App Direct Type or not

@retval TRUE if AppDirect Type
@retval FALSE if not AppDirect Type
**/
BOOLEAN
IsNameSpaceTypeAppDirect(IN NAMESPACE_LABEL *pNamespaceLabel, IN BOOLEAN Is_Namespace1_1
)
{
  if (!Is_Namespace1_1)
    return CompareGuid(&gAppDirectPmTypeGuid, &pNamespaceLabel->TypeGuid);
  else
    return !(pNamespaceLabel->Flags.Values.Local & 0x01);
}
/*
  Checks if Lsa status of Dimms is not initalized
  for all manageable dimms

  @retval TRUE - if all manageable dimms have
                 lsaStatus set to LSA_NOT_INIT
*/
BOOLEAN IsLSANotInitializedOnDimms()
{
  LIST_ENTRY *pNode = NULL;
  DIMM *pDimm = NULL;
  BOOLEAN returncode = TRUE;
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pNode);

    if (!IsDimmManageable(pDimm)) {
      continue;
    }
    returncode &= (LSA_NOT_INIT == pDimm->LsaStatus);
  }
  return returncode;
}

/**
  Initializes Namespaces inventory

  Function reads LSA data from all DIMMs, then scans for Namespaces
  data in it. All found Namespaces are stored in a list in global
  gNvmDimmData->PMEMDev structure.

  If any DCPMMs fail to initialize, continue to initialize the rest of
  them, but return an error.

  @retval EFI_DEVICE_ERROR Reading LSA data failed
  @retval EFI_ABORTED Reading Namespaces data from LSA failed
  @retval EFI_SUCCESS Namespaces inventory correctly initialized
**/
EFI_STATUS
InitializeNamespaces(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_STATUS TempReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pNode = NULL;
  DIMM *pDimm = NULL;
  LABEL_STORAGE_AREA *pLsa = NULL;

  NVDIMM_ENTRY();

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pNode);
    if (pDimm->pLsa != NULL) {
      FreeLsaSafe(&pDimm->pLsa);
      pDimm->pLsa = NULL;
    }

    if (!IsDimmManageable(pDimm)) {
      continue;
    }

    TempReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
    if (TempReturnCode == EFI_NOT_FOUND) {
      /**
        Return code is purposefully not set here. EFI_NOT_FOUND is
        returned due to LSA not being initialized. In this
        case success code should be returned
      **/
      NVDIMM_DBG("LSA not found on DIMM 0x%x", pDimm->DeviceHandle.AsUint32);
      pDimm->LsaStatus = LSA_NOT_INIT;
      continue;
    }
    else if (EFI_ERROR(TempReturnCode)) {
      ReturnCode = TempReturnCode;
      pDimm->LsaStatus = LSA_CORRUPTED;
      /**
        If the LSA is corrupted, we do nothing - it may be a driver mismach between UEFI and the OS,
        so we don't want to "kill" a valid configuration
      **/
      NVDIMM_DBG("LSA corrupted on DIMM 0x%x", pDimm->DeviceHandle.AsUint32);
      continue;
    }

    pDimm->pLsa = pLsa;
  }

  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pNode);
    if (!IsDimmManageable(pDimm)) {
      continue;
    }
    if (pDimm->LsaStatus == LSA_NOT_INIT || pDimm->LsaStatus == LSA_CORRUPTED) {
      continue;
    }

    TempReturnCode = RetrieveNamespacesFromLsa(pDimm, gNvmDimmData->PMEMDev.pFitHead,
      &gNvmDimmData->PMEMDev.Namespaces);
    if (EFI_ERROR(TempReturnCode)) {
      ReturnCode = TempReturnCode;
      NVDIMM_DBG("Failed to retrieve Namespaces from LSA");
      pDimm->LsaStatus = LSA_COULD_NOT_READ_NAMESPACES;
      continue;
    }

    pDimm->LsaStatus = LSA_OK;
  }

  //cleanup cache
  LIST_FOR_EACH(pNode, &gNvmDimmData->PMEMDev.Dimms) {
    pDimm = DIMM_FROM_NODE(pNode);
    if (pDimm->pLsa != NULL) {
      FreeLsaSafe(&pDimm->pLsa);
      pDimm->pLsa = NULL;
    }
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Read data from an Intel NVM Dimm Namespace.
  Transform LBA into RDPA and call the Intel NVM Dimm read function.
  The function reads the block size, so the input buffer size needs
  to be at least the namespace block size.

  @param[in] pNamespace The Intel NVM Dimm Namespace to read data from
  @param[in] Lba LBA of the start of the data region to read
  @param[out] pBuffer Buffer to place read data into

  @retval EFI_SUCCESS on a successful read
  @retval Error return values from IoNamespaceBlock function
**/
EFI_STATUS
ReadNamespaceBlock(
  IN     NAMESPACE *pNamespace,
  IN     UINT64 Lba,
     OUT CHAR8 *pBuffer
  )
{
  return IoNamespaceBlock(pNamespace, Lba, pBuffer, (UINT32)pNamespace->Media.BlockSize, TRUE);
}

/**
  Write data to an Intel NVM Dimm Namespace.
  Transform LBA into RDPA and call the Intel NVM Dimm DIMM write function.
  The function writes the block size, so the input buffer size needs
  to be at least the namespace block size.

  @param[in] pNamespace The Intel NVM Dimm Namespace to write data to
  @param[in] Lba LBA of the start of the data region to write
  @param[in] pBuffer Buffer with data to write

  @retval EFI_SUCCESS on a successful write
  @retval Error return values from IoNamespaceBlock function
**/
EFI_STATUS
WriteNamespaceBlock(
  IN     NAMESPACE *pNamespace,
  IN     UINT64 Lba,
  IN     CHAR8 *pBuffer
  )
{
  return IoNamespaceBlock(pNamespace, Lba, pBuffer, (UINT32)pNamespace->Media.BlockSize, FALSE);
}

/**
  Read data from an Intel NVM Dimm Namespace.
  Transform buffer offset into RDPA.
  Call the Intel NVM Dimm read function.
  The buffer length and the offset need to be aligned to the cache line size.

  @param[in] pNamespace The Intel NVM Dimm Namespace to read data from
  @param[in] Offset bytes offset of the start of the data region to read
  @param[in] Length the length of the buffer to read
  @param[out] pBuffer Buffer to place read data into

  @retval EFI_SUCCESS on a successful read
  @retval Error return values from IoNamespaceBlock function
**/
EFI_STATUS
ReadNamespaceBytes(
  IN     NAMESPACE *pNamespace,
  IN     CONST UINT64 Offset,
     OUT VOID *pBuffer,
  IN     CONST UINT64 Length
  )
{
  return IoNamespaceBytes(pNamespace, Offset, (CHAR8 *)pBuffer, (UINT32)Length, TRUE);
}

/**
  Write data to an Intel NVM Dimm Namespace.
  Transform buffer offset into RDPA.
  Call the Intel NVM Dimm write function.
  The buffer length and the offset need to be aligned to the cache line size.

  @param[in] pNamespace The Intel NVM Dimm Namespace to write data to
  @param[in] Offset bytes offset of the start of the data region to write
  @param[in] Length the length of the buffer to write
  @param[in] pBuffer Buffer with data to write

  @retval EFI_SUCCESS on a successful write
  @retval Error return values from IoNamespaceBlock function
**/
EFI_STATUS
WriteNamespaceBytes(
  IN     NAMESPACE *pNamespace,
  IN     CONST UINT64 Offset,
  IN     VOID *pBuffer,
  IN     CONST UINT64 Length
  )
{
  return IoNamespaceBytes(pNamespace, Offset, (CHAR8 *)pBuffer, (UINT32)Length, FALSE);
}

/**
  Performs a block read or write to the Namespace.
  The function calculates the proper DPA DIMM offset and issues
  the proper read or write operation on the destination DIMM.

  @param[in] pNamespace The Intel NVM Dimm Namespace to perform the IO Block operation.
  @param[in] Lba the Logica Block Addressing block offset to perform the IO on.
  @param[out] pBuffer the destination/source buffer where or from the data will be copied.
  @param[in] BlockLength the length of the buffer - should equal to the namespace block size.
  @param[in] ReadOperation boolean value indicating what type of IO is requested.
    TRUE means a read operation, and FALSE results in write operation.

  @retval EFI_SUCCESS if the IO operation was performed without errors.
  @retval Other return codes from functions:
    DimmRead, DimmWrite, AppDirectIo
**/
EFI_STATUS
IoNamespaceBlock(
  IN     NAMESPACE *pNamespace,
  IN     CONST UINT64 Lba,
     OUT CHAR8 *pBuffer,
  IN     CONST UINT32 BlockLength,
  IN     CONST BOOLEAN ReadOperation
  )
{
  EFI_STATUS ReturnCode = EFI_ABORTED;
  UINT64 Offset = 0;

  NVDIMM_ENTRY();

  Offset = Lba * pNamespace->Media.BlockSize;
  ReturnCode = AppDirectIo(pNamespace, Offset, pBuffer, BlockLength, ReadOperation);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Performs a read or write to the AppDirect Namespace.
  The data is read/written from/to Interlave Set mapped in system memory.

  @param[in] pNamespace Intel NVM Dimm Namespace to perform the IO operation.
  @param[in] Offset Offset of AppDirect Namespace
  @param[in, out] pBuffer Destination/source buffer where or from the data will be copied.
  @param[in] Nbytes Number of bytes to read/write
  @param[in] ReadOperation boolean value indicating what type of IO is requested.

  @retval EFI_SUCCESS If the IO operation was performed without errors.
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
AppDirectIo(
  IN     NAMESPACE *pNamespace,
  IN     UINT64 Offset,
  IN OUT CHAR8 *pBuffer,
  IN     UINT64 Nbytes,
  IN     BOOLEAN ReadOperation
  )
{
#ifndef OS_BUILD
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pNode = NULL;
  DIMM_REGION *pRegion = NULL;
  UINT8 *pSpaStart = NULL;
  UINT32 Index = 0;
  UINT8 *pAddress = 0;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pNamespace->pParentIS == NULL || pBuffer == NULL) {
    goto Finish;
  }

  pSpaStart = (UINT8 *) pNamespace->SpaNamespaceBase;
  pAddress = pSpaStart + Offset;

  if (gArsBadRecordsCount != 0) {
    ReturnCode = IsAddressRangeInArsList((UINT64)pAddress, Nbytes);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
  }

  if (ReadOperation) {
    CopyMem(pBuffer, pAddress, Nbytes);
  } else {
    CopyMem(pAddress, pBuffer, Nbytes);

    for (Index = 0; Index < Nbytes / CACHE_LINE_SIZE; Index++) {
      gClFlush(pAddress + (Index * CACHE_LINE_SIZE));
    }

    LIST_FOR_EACH(pNode, &pNamespace->pParentIS->DimmRegionList) {
      pRegion = DIMM_REGION_FROM_NODE(pNode);
      DimmWPQFlush(pRegion->pDimm);
    }
    AsmSfence();
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
#else
  return EFI_UNSUPPORTED;
#endif
}

/**
  Performs a buffer read or write to the namespace.
  The function calculates the proper DPA DIMM offset and issues
  the proper read or write operation on the destination DIMM.

  @param[in] pNamespace The Intel NVM Dimm Namespace to perform the IO buffer operation.
  @param[in] Offset - byte offset on the Namespace to perform the IO on - should be a multiple of cache line size.
  @param[out] pBuffer the destination/source buffer where or from the data will be copied.
  @param[in] BufferLength the length of the buffer - should be a multiple of cache line size.
  @param[in] ReadOperation boolean value indicating what type of IO is requested.
    TRUE means a read operation, and FALSE results in write operation.

  @retval EFI_SUCCESS if the IO operation was performed without errors.
  @retval EFI_INVALID_PARAMETER if pNamespace and/or pBuffer equals NULL.
  @retval EFI_BAD_BUFFER_SIZE if Offset and/or BufferLength are not aligned to the cache line size.
  @retval Other return codes from functions:
    DimmRead, DimmWrite, AppDirectIo
**/
EFI_STATUS
IoNamespaceBytes(
  IN     NAMESPACE *pNamespace,
  IN     UINT64 Offset,
     OUT CHAR8 *pBuffer,
  IN     UINT32 BufferLength,
  IN     BOOLEAN ReadOperation
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pBuffer == NULL) {
    goto Finish;
  }

  ReturnCode = AppDirectIo(pNamespace, Offset, pBuffer, BufferLength, ReadOperation);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Read block from the namespace
  The function reads only one block from the device, the buffer length needs to
  be at least the Namespace block size.

  @param[in] pNamespace Namespace that data will be read from
  @param[in] Lba LBA to retrieve read from
  @param[out] pBuffer pointer to the memory where the result should be stored

  @retval EFI_SUCCESS on a successful read
  @retval Error return values from BttRead or ReadNamespaceBlock function
**/
EFI_STATUS
ReadBlockDevice(
  IN     NAMESPACE *pNamespace,
  IN     UINT64 Lba,
     OUT CHAR8 *pBuffer
  )
{
  if (pNamespace->IsBttEnabled) {
    if (pNamespace->pBtt == NULL) {
      return EFI_NOT_READY;
    }
    return BttRead(pNamespace->pBtt, Lba, pBuffer);
  } else if (pNamespace->IsPfnEnabled) {
    if (pNamespace->pPfn == NULL) {
      return EFI_NOT_READY;
    }
    return PfnRead(pNamespace->pPfn, Lba, pBuffer);
  } else if (pNamespace->IsRawNamespace) {
    return ReadNamespaceBlock(pNamespace, Lba, pBuffer);
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  Write to the namespace through the BTT mechanism.
  The function writes only one block from the device, the buffer length needs to
  be at least the Namespace block size.

  @param[in] pNamespace Namespace containing the BTT
  @param[in] Lba LBA to store the write to
  @param[out] pBuffer pointer to the memory where the destination data resides

  @retval EFI_SUCCESS on a successful write
  @retval Error return values from BttWrite or WriteNamespaceBlock function
**/
EFI_STATUS
WriteBlockDevice(
  IN     NAMESPACE *pNamespace,
  IN     UINT64 Lba,
     OUT CHAR8 *pBuffer
  )
{
  if (pNamespace->IsBttEnabled) {
    if (pNamespace->pBtt == NULL) {
      return EFI_NOT_READY;
    }
    return BttWrite(pNamespace->pBtt, Lba, pBuffer);
  } else if (pNamespace->IsPfnEnabled) {
    if (pNamespace->pPfn == NULL) {
      return EFI_NOT_READY;
    }
    return PfnWrite(pNamespace->pPfn, Lba, pBuffer);
  } else if (pNamespace->IsRawNamespace) {
    return WriteNamespaceBlock(pNamespace, Lba, pBuffer);
  } else {
    return EFI_UNSUPPORTED;
  }
}

/**
  Compare region length field in MemoryMapRange Struct

  @param[in] pFirst First item to compare
  @param[in] pSecond Second item to compare

  @retval -1 if first is less than second
  @retval  0 if first is equal to second
  @retval  1 if first is greater than second
**/
STATIC
INT32
CompareRegionLengthInMemoryRange(
  IN     VOID *pFirst,
  IN     VOID *pSecond
  )
{
  MEMMAP_RANGE *pRange  = NULL;
  MEMMAP_RANGE *pRange2 = NULL;

  if (pFirst == NULL || pSecond == NULL) {
      NVDIMM_DBG("NULL pointer found.");
      return 0;
  }

  pRange = MEMMAP_RANGE_FROM_NODE(pFirst);
  pRange2 = MEMMAP_RANGE_FROM_NODE(pSecond);

  if (pRange->RangeLength < pRange2->RangeLength) {
    return -1;
  } else if (pRange->RangeLength > pRange2->RangeLength) {
    return 1;
  } else {
    return 0;
  }
}

/**
  Compare region DPA Start field in MemoryMapRange Struct

  @param[in] pFirst First item to compare
  @param[in] pSecond Second item to compare

  @retval -1 if first is less than second
  @retval  0 if first is equal to second
  @retval  1 if first is greater than second
**/
STATIC
INT32
CompareRegionDpaStartInMemoryRange(
  IN     VOID *pFirst,
  IN     VOID *pSecond
  )
{
  MEMMAP_RANGE *pRange  = NULL;
  MEMMAP_RANGE *pRange2 = NULL;

  if (pFirst == NULL || pSecond == NULL) {
      NVDIMM_DBG("NULL pointer found.");
      return 0;
  }

  pRange = MEMMAP_RANGE_FROM_NODE(pFirst);
  pRange2 = MEMMAP_RANGE_FROM_NODE(pSecond);

  if (pRange->RangeStartDpa < pRange2->RangeStartDpa) {
    return -1;
  } else if (pRange->RangeStartDpa > pRange2->RangeStartDpa) {
    return 1;
  } else {
    return 0;
  }
}

/**
  Find the Memory map range with the lowest DPA that contains a given region size in a given IS.

  IS that is passed in is of the type the user wishes to create NS on
  Size has already been aligned for NS requirements

  If size is MAX_UINT64 ignore lowest DPA and instead find the largest free memory region for that exists on
  all dimms in the interleave set

  @param[in] pIS pointer to Interleave Set that the memory range is contained in.
  @param[in] Size Minimum size the memory map range must be, if MAX_UINT64 then find the largest possible.
  @param[out] pFoundRange pointer memory range structure range will be copied into

  @retval EFI_SUCCESS everything went fine
  @retval EFI_OUT_OF_RESOURCES when memory allocation fails
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.
  @retval EFI_NOT_FOUND could not find a region of given size in IS
  Other return codes from functions:
    GetDimmFreemap
    GetListSize
**/

EFI_STATUS
FindADMemmapRangeInIS(
  IN      NVM_IS *pIS,
  IN      UINT64 Size,
      OUT MEMMAP_RANGE *pFoundRange
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY **ppFreemapList = NULL;
  LIST_ENTRY *pNode = NULL;
  LIST_ENTRY *pNode2 = NULL;
  LIST_ENTRY *pNode3 = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  MEMMAP_RANGE *pRange = NULL;
  MEMMAP_RANGE *pRange2 = NULL;
  UINT64 ISStartDpa = 0;
  UINT64 ISEndDpa = 0;
  UINT32 DimmNum = 0;
  UINT32 Index = 0;
  BOOLEAN Found = FALSE;

  if (pIS == NULL  || IsListEmpty(&pIS->DimmRegionList) || pFoundRange == NULL) {
    goto Finish;
  }

  ZeroMem(pFoundRange, sizeof(*pFoundRange));

  ReturnCode = GetListSize(&pIS->DimmRegionList, &DimmNum);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ppFreemapList = (LIST_ENTRY **) AllocateZeroPool(sizeof(*ppFreemapList) * DimmNum);

  if (ppFreemapList == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {

    ppFreemapList[Index] = (LIST_ENTRY *) AllocateZeroPool(sizeof(*ppFreemapList[Index]));

    if (ppFreemapList[Index] == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    InitializeListHead(ppFreemapList[Index]);

    pDimmRegion = DIMM_REGION_FROM_NODE(pNode);

    ReturnCode = GetDimmFreemap(pDimmRegion->pDimm, FreeCapacityForADMode, ppFreemapList[Index]);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to get free memory range");
      goto Finish;
    }

    if (IsListEmpty(ppFreemapList[Index])) {
      ReturnCode = EFI_NOT_FOUND;
      goto Finish;
    }

    Index++;
  }

  pNode = GetFirstNode(&pIS->DimmRegionList);
  pDimmRegion = DIMM_REGION_FROM_NODE(pNode);

  /** We can assume the IS on the first dimm starts at the same location on all dimms in the IS **/
  ISStartDpa = pDimmRegion->pDimm->PmStart + pDimmRegion->PartitionOffset;
  ISEndDpa = ISStartDpa + pDimmRegion->PartitionSize;

  if (Size == MAX_UINT64_VALUE) {
    ReturnCode = BubbleSortLinkedList(ppFreemapList[0], CompareRegionLengthInMemoryRange);
  } else {
    ReturnCode = BubbleSortLinkedList(ppFreemapList[0], CompareRegionDpaStartInMemoryRange);
  }

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to bubble sort free memory ranges");
    goto Finish;
  }

  if (Size == MAX_UINT64_VALUE) {
    LIST_FOR_EACH_REVERSE(pNode2, ppFreemapList[0]) {
      pRange = MEMMAP_RANGE_FROM_NODE(pNode2);
      /** We must find range that exists inside IS **/
      if (pRange->RangeStartDpa >= ISStartDpa && ((pRange->RangeStartDpa + pRange->RangeLength) <= ISEndDpa)) {

        /**
          Free memory range candidate exists on first dimm, now validate the same candidate range we are requesting exists on other dimms.
          If candidate range is not discovered on a single dimm it is not a valid candidate range.
        **/

        Found = TRUE;

        for (Index=1; Index < DimmNum; Index++) {
          Found = FALSE;
          LIST_FOR_EACH(pNode3, ppFreemapList[Index]) {
            pRange2 = MEMMAP_RANGE_FROM_NODE(pNode3);
            if (pRange2->RangeStartDpa >= ISStartDpa && pRange2->RangeStartDpa + pRange2->RangeLength <= ISEndDpa && /** Range exists in the InterleaveSet **/
              pRange->RangeStartDpa >= pRange2->RangeStartDpa && /** Start of the candidate is not before the start of the test range **/
              pRange->RangeStartDpa + pRange->RangeLength <= pRange2->RangeStartDpa + pRange2->RangeLength) { /**end of the candidate is before or at the end of the test range **/
              Found = TRUE;
              break;
            }
          }

          /** Unable to find region on more than one dimm, invalid canidate range, try next canidate range **/
          if (!Found) {
            break;
          }
        }

        /** Since candidate list is sorted by smallest range first and searched in reverse order, the first match will have the largest memory range **/
        if (Found) {
          break;
        }
      }
    }
  } else {
    LIST_FOR_EACH(pNode2, ppFreemapList[0]) {
      pRange = MEMMAP_RANGE_FROM_NODE(pNode2);
      /** We must find range that exists inside IS && is larger than the requested capacity **/
      if (pRange->RangeLength >= Size &&
        pRange->RangeStartDpa >= ISStartDpa && (pRange->RangeStartDpa + pRange->RangeLength <= ISEndDpa)) {

        /**
          Free memory range candidate exists on first dimm, now validate the same candidate range we are requesting exists on other dimms.
          If candidate range is not discovered on a single dimm it is not a valid candidate range.
        **/

        Found = TRUE;

        for (Index=1; Index < DimmNum; Index++) {
          Found = FALSE;
          LIST_FOR_EACH(pNode3, ppFreemapList[Index]) {
            pRange2 = MEMMAP_RANGE_FROM_NODE(pNode3);
            if (pRange2->RangeLength >= Size && /** Large enough for request **/
              pRange2->RangeStartDpa >= ISStartDpa && pRange2->RangeStartDpa + pRange2->RangeLength <= ISEndDpa && /** Range exists in the InterleaveSet **/
              pRange->RangeStartDpa >= pRange2->RangeStartDpa && /** Start of the candidate is not before the start of the test range **/
              pRange->RangeStartDpa + pRange->RangeLength <= pRange2->RangeStartDpa + pRange2->RangeLength) { /**end of the candidate is before or at the end of the test range **/
              Found = TRUE;
              break;
            }
          }

          /** Unable to find region on more than one dimm, invalid canidate range, try next canidate range **/
          if (!Found) {
            break;
          }
        }

        /** Since candidate list is sorted by DPA, the first match will have the lowest DPA **/
        if (Found) {
          break;
        }
      }
    }
  }

  if (!Found) {
    ReturnCode = EFI_NOT_FOUND;
  } else {
    CopyMem_S(pFoundRange, sizeof(*pFoundRange), pRange, sizeof(*pFoundRange));
  }

Finish:
  if (ppFreemapList != NULL) {
    for(Index = 0; Index < DimmNum; Index++) {
      FreeMemmapItems(ppFreemapList[Index]);
    }
    FREE_POOL_SAFE(ppFreemapList);
  }
  return ReturnCode;
}

/**
  Find the Starting address of an Appdirect Namespace in the SPA

  @param[in] pNamespace pointer to Namespace to calculate the base SPA for
  @param[in] pIS pointer to Interleave Set that namespace is contained in.

  @retval EFI_SUCCESS everything went fine
  @retval EFI_INVALID_PARAMETER if any of the parameters is a NULL.

**/

STATIC
EFI_STATUS
CalculateAppDirectNamespaceBaseSpa(
  IN      NAMESPACE *pNamespace,
  IN      NVM_IS *pIS
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 Index = 0;
  NvDimmRegionMappingStructure *pDimmRegionTable = NULL;
  InterleaveStruct *pInterleaveTable = NULL;
  UINT64 NamespaceRdpa = 0;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pIS == NULL) {
    goto Finish;
  }

  for (Index = 0; Index < pNamespace->RangesCount; Index++) {
    ReturnCode = GetNvDimmRegionMappingStructureForPid(
      gNvmDimmData->PMEMDev.pFitHead,
      pNamespace->Range[Index].pDimm->DimmID,
      NULL,
      TRUE,
      pIS->pSpaTbl->SpaRangeDescriptionTableIndex,
      &pDimmRegionTable);

    if (EFI_ERROR(ReturnCode) || pDimmRegionTable == NULL) {
      NVDIMM_DBG("Unable to locate NvDimmRegion table in NFIT for dimm");
      goto Finish;
    }

    // First Dimm in the interleave set
    if (pDimmRegionTable->RegionOffset == 0) {

      // Interleave tables only exist for interleaved dimms
      if (pDimmRegionTable->InterleaveStructureIndex != 0) {
        ReturnCode = GetInterleaveTable(
          gNvmDimmData->PMEMDev.pFitHead,
          pDimmRegionTable->InterleaveStructureIndex,
          &pInterleaveTable);

        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_DBG("Unable to locate Interleave table in NFIT for dimm");
          goto Finish;
        }
      } else {
        pInterleaveTable = NULL;
      }

      if (pNamespace->Range[Index].Dpa < pDimmRegionTable->NvDimmPhysicalAddressRegionBase) {
        NVDIMM_DBG("DPA exists before RegionBase");
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }

      NamespaceRdpa = pNamespace->Range[Index].Dpa - pDimmRegionTable->NvDimmPhysicalAddressRegionBase;

      ReturnCode = RdpaToSpa(
        NamespaceRdpa,
        pDimmRegionTable,
        pIS->pSpaTbl,
        pInterleaveTable,
        &pNamespace->SpaNamespaceBase);

      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to calculate SPA base for namespace from RDPA");
        goto Finish;
      }
      break;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
/**
 Namespace Capacity Alignment

 Aligning the Namespace Capacity  Helper Function
 @param [in] NamespaceCapacity Namespace Capacity provided
 @param [in] DimmCount
 @param [out] *pAlignedNamespaceCapacity Total Namespace Capacity after alignment

 @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
 @retval EFI_SUCCESS Namespace capacity aligned
**/
EFI_STATUS
AlignNamespaceCapacity(
  IN  UINT64 NamespaceCapacity,
  IN  UINT64 DimmCount,
  OUT UINT64* pAlignedNamespaceCapacity
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 NamespaceCapacityPerDimm = 0;

  if (pAlignedNamespaceCapacity == NULL) {
    goto Finish;
  }
  if (NamespaceCapacity < GIB_TO_BYTES(32)) {
    NVDIMM_DBG("Capacity  length is < 32 bit then it is at 4K Alignment ");
    NamespaceCapacityPerDimm = ROUNDUP((NamespaceCapacity/DimmCount), NAMESPACE_4KB_ALIGNMENT_SIZE);
  }
  else if (NamespaceCapacity < TIB_TO_BYTES(1)) {
    NVDIMM_DBG("Capacity  length is < 40 bit then it is at 4K Alignment ");
    NamespaceCapacityPerDimm = ROUNDUP((NamespaceCapacity / DimmCount), NAMESPACE_4KB_ALIGNMENT_SIZE);
  }
  else if (NamespaceCapacity < TIB_TO_BYTES(256)) {
    NVDIMM_DBG("Capacity  length is < 48 bit then it is at 64K Alignment ");
    NamespaceCapacityPerDimm = ROUNDUP((NamespaceCapacity / DimmCount), NAMESPACE_64KB_ALIGNMENT_SIZE);
  }
  else {
    NVDIMM_DBG("Capacity length is < 60 bit then it is at 32G Alignment ");
    NamespaceCapacityPerDimm = ROUNDUP((NamespaceCapacity / DimmCount), NAMESPACE_32GB_ALIGNMENT_SIZE);
  }

  *pAlignedNamespaceCapacity = NamespaceCapacityPerDimm * DimmCount;

  NVDIMM_DBG("AlignmentCapacity: %u \n", *pAlignedNamespaceCapacity);
  ReturnCode = EFI_SUCCESS;

  Finish:
  return ReturnCode;
}
/**
  Provision Namespace capacity on a DIMM or Interleave Set.

  Only one of target pointers must be provided, other one must be NULL.
  Function analyses possibility of allocation of NamespaceCapacity bytes
  on a DIMM or on an Interleave Set. If there is enough free space
  function assigns appropriate Namespace regions.

  @param[in] pDimm DIMM structure pointer in case of Block Mode Namespaces
  @param[in] pIS Interleave Set structure pointer in case of Persistent Memory Namespaces
  @param[in out] NamespaceCapacity Required capacity in bytes
  @param[out] pNamespace Namespace which regions will be updated if allocation
    is successful

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_OUT_OF_RESOURCES Not enough free space on target
  @retval EFI_SUCCESS Namespace capacity allocated
**/
EFI_STATUS
AllocateNamespaceCapacity(
  IN     DIMM *pDimm, OPTIONAL
  IN     NVM_IS *pIS, OPTIONAL
  IN OUT UINT64 *pNamespaceCapacity,
     OUT NAMESPACE *pNamespace
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pFreemapList = NULL;
  LIST_ENTRY *pNode = NULL;
  DIMM_REGION *pRegion = NULL;
  MEMMAP_RANGE AppDirectRange;
  UINT32 DimmCount = 0;
  UINT64 AlignedNamespaceCapacitySize = 0;
  UINT64 RegionSize = 0;
  UINT16 Index = 0;

  NVDIMM_ENTRY();

  ZeroMem(&AppDirectRange, sizeof(AppDirectRange));

  if ((pDimm == NULL && pIS == NULL) ||
      (pDimm != NULL && pIS != NULL) ||
      (pNamespaceCapacity == NULL) ||
      (pIS == NULL && pNamespace->NamespaceType == APPDIRECT_NAMESPACE)) {
    goto Finish;
  }

  if (pNamespace->NamespaceType == APPDIRECT_NAMESPACE) {
    if (pIS->State != IS_STATE_HEALTHY) {
      NVDIMM_DBG("Interleave Set %d is not active", pIS->InterleaveSetIndex);
      ReturnCode = EFI_NOT_READY;
      goto Finish;
    }

    // Provision capacity equally on all DIMMs of the Interleave Set
    ReturnCode = GetListSize(&pIS->DimmRegionList, &DimmCount);
    if (EFI_ERROR(ReturnCode) || DimmCount == 0) {
      goto Finish;
    }
    ReturnCode = AlignNamespaceCapacity(*pNamespaceCapacity, DimmCount, &AlignedNamespaceCapacitySize);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Namespace Capacity is an invalid parameter");
      ReturnCode = EFI_INVALID_PARAMETER;
    }
    if (*pNamespaceCapacity % DimmCount != 0) {
      NVDIMM_WARN("Unable to equally distribute required capacity among %d regions. "
          "Allocated Capacity will be increased to %lld", DimmCount, AlignedNamespaceCapacitySize);
    }

    RegionSize = AlignedNamespaceCapacitySize / DimmCount;

    ReturnCode = FindADMemmapRangeInIS(pIS, RegionSize, &AppDirectRange);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Did not find free area of requested size %llx", *pNamespaceCapacity);
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    *pNamespaceCapacity = AlignedNamespaceCapacitySize;

    Index = 0;
    LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {
      pRegion = DIMM_REGION_FROM_NODE(pNode);
      pNamespace->Range[Index].pDimm = pRegion->pDimm;
      pNamespace->Range[Index].Dpa = AppDirectRange.RangeStartDpa;
      pNamespace->Range[Index].Size = RegionSize;
      Index++;
    }
    pNamespace->RangesCount = Index;
    ReturnCode = CalculateAppDirectNamespaceBaseSpa(pNamespace, pIS);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to locate base SPA address of Namespace");
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  if (pFreemapList != NULL) {
    FreeMemmapItems(pFreemapList);
    FREE_POOL_SAFE(pFreemapList);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check for free slot status of Label Storage Area. Function analyses
  Label Storage Area and returns number of free slots and first slot
  number whichever is required.

  @param[in] pLsa Label Area Storage structure pointer
  @param[out] pFreeSlotsCount Number of free slots found
  @param[out] pFirstFreeSlotId First free slot number

  @retval EFI_INVALID_PARAMETER LSA pointer not provided
  @retval EFI_SUCCESS Free slots information determined correctly
**/
#define UNDEFINED_SLOT_ID  0xFFFF

EFI_STATUS
GetLsaFreeSlots(
  IN     LABEL_STORAGE_AREA *pLsa,
     OUT UINT16 *pFreeSlotsCount, OPTIONAL
     OUT UINT16 *pFirstFreeSlotId OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 CurrentIndex = 0;
  UINT16 Index = 0;
  UINT16 SlotStatus = SLOT_UNKNOWN;

  NVDIMM_ENTRY();
  if (pFreeSlotsCount != NULL) {
    *pFreeSlotsCount = 0;
  }
  if (pFirstFreeSlotId != NULL) {
    *pFirstFreeSlotId = UNDEFINED_SLOT_ID;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  for (Index = 0; Index < pLsa->Index[CurrentIndex].NumberOfLabels; Index++) {
    CheckSlotStatus(&pLsa->Index[CurrentIndex], Index, &SlotStatus);
    if (SlotStatus == SLOT_FREE) {
      if (pFreeSlotsCount != NULL) {
        *pFreeSlotsCount += 1;
      }
      if (pFirstFreeSlotId != NULL &&  *pFirstFreeSlotId == UNDEFINED_SLOT_ID) {
        *pFirstFreeSlotId = Index;
        NVDIMM_DBG("First free slot: %d", *pFirstFreeSlotId);
      }
    }
  }
  ReturnCode = EFI_SUCCESS;

Finish:
  if (pFreeSlotsCount != NULL) {
    NVDIMM_DBG("Free slots count: %d", *pFreeSlotsCount);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Aligns the size of the Label Index area and calculates the number of
  free blocks, padding the driver can support.

  @param[in] PcdSize Size of the PCD Partition area
  @param[in] UseLatestLabelVersion Use latest version of Labels
  @param[out] pFreeBlocks The size of free array
  @param[out] pPadding The padding required to complete the index area

  @retval EFI_INVALID_PARAMETER null parameter provided
  @retval EFI_SUCCESS Alignment went well
**/
EFI_STATUS
AlignLabelStorageArea(
  IN     UINT32 PcdSize,
  IN     BOOLEAN UseLatestLabelVersion,
     OUT UINT32 *pFreeBlocks,
     OUT UINT32 *pPadding
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 TotalLabels = 0;
  UINT32 FreeOffset = 0;
  UINT32 IndexSize = MIN_NAMESPACE_LABEL_INDEX_SIZE;
  UINT32 LabelSize = 0;

  if ((pFreeBlocks == NULL) || (pPadding == NULL)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  LabelSize = (UseLatestLabelVersion) ? sizeof(NAMESPACE_LABEL) : sizeof(NAMESPACE_LABEL_1_1);

  FreeOffset = OFFSET_OF(NAMESPACE_INDEX, pFree);
  TotalLabels = PcdSize / LabelSize;
  *pFreeBlocks = LABELS_TO_FREE_BYTES(TotalLabels);

  if ((FreeOffset + *pFreeBlocks) > MIN_NAMESPACE_LABEL_INDEX_SIZE) {
    IndexSize = ROUNDUP(FreeOffset + *pFreeBlocks, NSINDEX_ALIGN);
  }

  *pPadding = IndexSize - (FreeOffset + *pFreeBlocks);

Finish:
  return ReturnCode;
}

/**
  Gets the Label Index Data in a contiguous memory

  It's the caller's responsibility to free the memory.

  @param[in] pLsa The Label Storage Area
  @param[in] LabelIndex The index of the LSA

  @param[out] pRawData Pointer to the contigous memory region

  @retval EFI_INVALID_PARAMETER NULL pointer provided
  @retval EFI_OUT_OF_RESOURCES could not allocate memory
  @retval EFI_SUCCESS Successfully transferred memory
**/
EFI_STATUS
LabelIndexAreaToRawData(
  IN     LABEL_STORAGE_AREA *pLsa,
  IN     UINT32  LabelIndex,
     OUT UINT8 **ppRawData
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT8 *pBuffer = NULL;
  UINT64 IndexSize = 0;
  UINT64 FreeOffset = 0;
  UINT64 NumFreeBytes = 0;
  UINT64 TotalIndexSize = 0;
  NAMESPACE_INDEX *pNamespaceIndex = NULL;
  UINT32 Index = 0;

  if (pLsa == NULL) {
    goto Finish;
  }

  Index = (LabelIndex == ALL_INDEX_BLOCKS) ? 0 : LabelIndex;

  pNamespaceIndex = &pLsa->Index[Index];
  if (pNamespaceIndex == NULL) {
    goto Finish;
  }

  IndexSize = pNamespaceIndex->MySize;
  if (IndexSize == 0) {
    goto Finish;
  }

  TotalIndexSize = (LabelIndex == ALL_INDEX_BLOCKS) ? (ALL_INDEX_BLOCKS * IndexSize) : IndexSize;

  *ppRawData = AllocateZeroPool(TotalIndexSize);
  if (*ppRawData == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  pBuffer = *ppRawData;

  FreeOffset = OFFSET_OF(NAMESPACE_INDEX, pFree);
  NumFreeBytes = LABELS_TO_FREE_BYTES(ROUNDUP(pNamespaceIndex->NumberOfLabels, 8));

  for (; Index < ALL_INDEX_BLOCKS; Index++) {

    pNamespaceIndex = &pLsa->Index[Index];
    if (pNamespaceIndex == NULL) {
      goto Finish;
    }

    CopyMem_S(pBuffer, TotalIndexSize, pNamespaceIndex, FreeOffset);

    if (TotalIndexSize < FreeOffset) {
      NVDIMM_ERR("TotalIndexSize is smaller than FreeOffset. Leads to negative destination buffer size.");
      ReturnCode = EFI_BAD_BUFFER_SIZE;
      goto Finish;
    }
      CopyMem_S(pBuffer + FreeOffset, TotalIndexSize - FreeOffset, pNamespaceIndex->pFree, NumFreeBytes);

      pBuffer += pNamespaceIndex->MySize;
      TotalIndexSize -= pNamespaceIndex->MySize;

    if (LabelIndex != ALL_INDEX_BLOCKS) {
      break;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}

/**
  Check if LSA of a specified DIMM is initialized.
  If empty LSA is detected then it is initialized.
  If non empty LSA is detected then it's validated
  for data correctness.

  @param[in] pDimm Target DIMM
  @param[in] LabelVersionMajor Major version of label to init
  @param[in] LabelVersionMinor Minor version of label to init

  @retval EFI_INVALID_PARAMETER NULL pointer provided
  @retval EFI_VOLUME_CORRUPTED LSA data is broken
  @retval EFI_SUCCESS Valid LSA detected or initialized correctly
**/
EFI_STATUS
InitializeLabelStorageArea(
  IN     DIMM *pDimm,
  IN     UINT16 LabelVersionMajor,
  IN     UINT16 LabelVersionMinor
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LABEL_STORAGE_AREA *pLsa = NULL;
  NAMESPACE_INDEX *pNewIndex = NULL;
  UINT128 IndexSignature;
  UINT16 Index = 0;
  BOOLEAN ChecksumInserted = FALSE;
  UINT32 FreeBlocks = 0;
  UINT32 Padding = 0;
  UINT8 *pRawData = NULL;
  UINT32 ChecksumOffset = 0;
  UINT32 LabelSize = 0;
  BOOLEAN UseLatestLabelVersion = FALSE;

  NVDIMM_ENTRY();

  SetMem(&IndexSignature, sizeof(IndexSignature), 0x0);

  if (pDimm == NULL) {
    goto Finish;
  }

#ifndef OS_BUILD
  ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
  if (ReturnCode != EFI_NOT_FOUND) {
    // Here we have a validated LSA or a corrupted LSA, just pass the result up
    goto Finish;
  }
#endif // OS_BUILD

  /**
    If ReadLabelStorageArea fails to validate the DIMM LSA, it frees the buffer
    and assigns NULL to it. Then we need to allocate it here to initialize.
  **/
  if (pLsa == NULL) {
    pLsa = AllocateZeroPool(sizeof(*pLsa));
    if (pLsa == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
  }

  // Proceed with initialization
  NVDIMM_DBG("Initializing LSA on DIMM: 0x%x", pDimm->DeviceHandle.AsUint32);
  IndexSignature.Uint64 = LSA_NAMESPACE_INDEX_SIG_L;
  IndexSignature.Uint64_1 = LSA_NAMESPACE_INDEX_SIG_H;

  if ((LabelVersionMajor == NSINDEX_MAJOR) && (LabelVersionMinor == NSINDEX_MINOR_2)) {
    UseLatestLabelVersion = TRUE;
  }

  AlignLabelStorageArea(pDimm->PcdLsaPartitionSize, UseLatestLabelVersion, &FreeBlocks, &Padding);
  if ((FreeBlocks == 0) && (Padding == 0)) {
    goto Finish;
  }

  ChecksumOffset = OFFSET_OF(NAMESPACE_INDEX, Checksum);
  LabelSize = (UseLatestLabelVersion) ? sizeof(NAMESPACE_LABEL) : sizeof(NAMESPACE_LABEL_1_1);

  // Some fields are the same for both Indexes
  for (Index = 0; Index < NAMESPACE_INDEXES; Index++) {
    pNewIndex = &pLsa->Index[Index];
    pNewIndex->MySize = OFFSET_OF(NAMESPACE_INDEX, pFree) + FreeBlocks + Padding;
    pNewIndex->LabelOffset = 2 * pNewIndex->MySize;
    pNewIndex->NumberOfLabels = FREE_BLOCKS_TO_LABELS(FreeBlocks) -
                                    ((UINT32)(pNewIndex->MySize / LabelSize) * 2);
    pNewIndex->Major = LabelVersionMajor;
    pNewIndex->Minor = LabelVersionMinor;
    pNewIndex->pFree = (UINT8 *) AllocateZeroPool( FreeBlocks);
    if (pNewIndex->pFree == NULL) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    SetMem(pNewIndex->pFree, FreeBlocks, 0xFF);
    CopyMem_S(&pNewIndex->Signature, sizeof(pNewIndex->Signature), &IndexSignature, NSINDEX_SIG_LEN);

    pNewIndex->LabelSize = (UINT8) BYTE_TO_INDEX_LABEL_SIZE(LabelSize);

    NVDIMM_DBG("LSA Index info %d: Index size: %d :: No of labels: %d", Index, pNewIndex->MySize,
                pNewIndex->NumberOfLabels);
  }

  // Rest of fields need to be initialized individually
  pLsa->Index[FIRST_INDEX_BLOCK].Sequence = 1;
  pLsa->Index[FIRST_INDEX_BLOCK].MyOffset = 0;
  pLsa->Index[FIRST_INDEX_BLOCK].OtherOffset =  pLsa->Index[FIRST_INDEX_BLOCK].MySize;

  // Gets the Label Index data into a contiguous memory region
  ReturnCode = LabelIndexAreaToRawData(pLsa, FIRST_INDEX_BLOCK, &pRawData);

  if (EFI_ERROR(ReturnCode) || (pRawData == NULL)) {
    NVDIMM_DBG("Failed to convert label area index to raw data");
    FREE_POOL_SAFE(pRawData);
    goto Finish;
  }

  ChecksumInserted = ChecksumOperations(pRawData, pLsa->Index[FIRST_INDEX_BLOCK].MySize,
                                         (UINT64 *)(pRawData + ChecksumOffset), TRUE);
  if (!ChecksumInserted) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  pLsa->Index[FIRST_INDEX_BLOCK].Checksum = *(UINT64 *)(pRawData + ChecksumOffset);
  FREE_POOL_SAFE(pRawData);

  pLsa->Index[SECOND_INDEX_BLOCK].Sequence = 2;
  pLsa->Index[SECOND_INDEX_BLOCK].MyOffset = pLsa->Index[SECOND_INDEX_BLOCK].MySize;
  pLsa->Index[SECOND_INDEX_BLOCK].OtherOffset = 0;

  ReturnCode = LabelIndexAreaToRawData(pLsa, SECOND_INDEX_BLOCK, &pRawData);

  if (EFI_ERROR(ReturnCode) || (pRawData == NULL)) {
    NVDIMM_DBG("Failed to convert label area index to raw data");
    FREE_POOL_SAFE(pRawData);
    goto Finish;
  }

  ChecksumInserted = ChecksumOperations(pRawData, pLsa->Index[SECOND_INDEX_BLOCK].MySize,
                                         (UINT64 *)(pRawData + ChecksumOffset), TRUE);
  if (!ChecksumInserted) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  pLsa->Index[SECOND_INDEX_BLOCK].Checksum = *(UINT64 *)(pRawData + ChecksumOffset);
  FREE_POOL_SAFE(pRawData);

  pLsa->pLabels = AllocateZeroPool(sizeof(*(pLsa->pLabels)) *
                                      (pLsa->Index[FIRST_INDEX_BLOCK].NumberOfLabels));
  if (pLsa->pLabels == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = WriteLabelStorageArea(pDimm->DimmID, pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  if (!ChecksumInserted) {
    NVDIMM_DBG("Could not calculate the checksum.");
  }
  NVDIMM_EXIT_I64(ReturnCode);
  FreeLsaSafe(&pLsa);
  return ReturnCode;
}

/**
  Initialize all label storage areas

  @param[in] ppDimms Array of DIMMs
  @param[in] DimmsNum Number of DIMMs
  @param[in] LabelVersionMajor Major version of label to init
  @param[in] LabelVersionMinor Minor version of label to init
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pDimmList is NULL
  @retval return codes from SendConfigInputToDimm
**/

EFI_STATUS
InitializeAllLabelStorageAreas(
  IN     DIMM **ppDimms,
  IN     UINT32 DimmsNum,
  IN     UINT16 LabelVersionMajor,
  IN     UINT16 LabelVersionMinor,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LABEL_STORAGE_AREA *pLsa = NULL;
  UINT32 Index = 0;

  NVDIMM_ENTRY();

  if (ppDimms == NULL || pCommandStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  for (Index = 0; Index < DimmsNum; Index++) {
    if (!IsDimmManageable(ppDimms[Index])) {
      continue;
    }

#ifndef OS_BUILD
    if (ppDimms[Index]->LsaStatus != LSA_NOT_INIT) {
      ReturnCode = ZeroLabelStorageArea(ppDimms[Index]->DimmID);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to clear LSA data on a DIMM 0x%x", ppDimms[Index]->DeviceHandle.AsUint32);
        ppDimms[Index]->LsaStatus = LSA_COULD_NOT_INIT;
        SetObjStatusForDimm(pCommandStatus, ppDimms[Index], NVM_ERR_FAILED_TO_INIT_NS_LABELS);
        goto Finish;
      }
    }
#endif // OS_BUILD

    ReturnCode = InitializeLabelStorageArea(ppDimms[Index], LabelVersionMajor, LabelVersionMinor);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to initialize LSA data on a DIMM 0x%x", ppDimms[Index]->DeviceHandle.AsUint32);
      ppDimms[Index]->LsaStatus = LSA_COULD_NOT_INIT;
      SetObjStatusForDimm(pCommandStatus, ppDimms[Index], NVM_ERR_FAILED_TO_INIT_NS_LABELS);
      goto Finish;
    }

    // Check if the LSA was written properly and is not corrupted
    ReturnCode = ReadLabelStorageArea(ppDimms[Index]->DimmID, &pLsa);
    if (EFI_ERROR(ReturnCode)) {
      ppDimms[Index]->LsaStatus = LSA_CORRUPTED_AFTER_INIT;
      NVDIMM_DBG("LSA corrupted after initialization on DIMM 0x%x", ppDimms[Index]->DeviceHandle.AsUint32);
      SetObjStatusForDimm(pCommandStatus, ppDimms[Index], NVM_ERR_FAILED_TO_INIT_NS_LABELS);
      goto Finish;
    }
    FREE_POOL_SAFE(pLsa);
    ppDimms[Index]->LsaStatus = LSA_OK;
    SetObjStatusForDimm(pCommandStatus, ppDimms[Index], NVM_SUCCESS);
  }
  SetCmdStatus(pCommandStatus, NVM_SUCCESS);

Finish:
  return ReturnCode;
}

/**
  Check if the label index version on the DIMM is v1.2

  @param[in] pDimm The target DIMM
  @param[out] pHasLatestIndexVersion Pointer to boolean if the index is v1.2

  @retval EFI_INVALID_PARAMETER Null parameter is passed
  @retval EFI_NOT_FOUND If no index blocks found on the DIMM
  @retval EFI_SUCCESS If found the label version successfully
**/
EFI_STATUS
CheckDimmNsLabelVersion(
  IN     DIMM *pDimm,
     OUT BOOLEAN *pHasLatestIndexVersion
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 CurrentIndex = 0;
  BOOLEAN HasLatestVersion = FALSE;
  LABEL_STORAGE_AREA *pLsa = NULL;

  NVDIMM_ENTRY();

  if (pDimm == NULL || pHasLatestIndexVersion == NULL) {
    goto Finish;
  }

  if (pDimm->LsaStatus == LSA_NOT_INIT) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  } else {
    ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, NULL);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    if ((pLsa->Index[CurrentIndex].Major == NSINDEX_MAJOR) &&
        (pLsa->Index[CurrentIndex].Minor == NSINDEX_MINOR_2)) {
      HasLatestVersion = TRUE;
    }

    FreeLsaSafe(&pLsa);
  }

  *pHasLatestIndexVersion = HasLatestVersion;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  FreeLsaSafe(&pLsa);
  return ReturnCode;
}

/**
  Check if we can use the latest index block version

  @param[in] pNamespace The target namespace
  @param[in out] pUseLatestVersion Returns if the LSA uses new version
                 TRUE if all DIMMs in the Namespace have same Major/Minor
                      version of index blocks. No index blocks is treated
                      as version 1.2
                 FALSE if the DIMMs in the Namespace have a mismatch of the
                       Major/Minor versions in the index block
  @retval EFI_INVALID_PARAMETER Null parameter is passed
  @retval EFI_DEVICE_ERROR If the versions don't match on the DIMMs
  @retval EFI_OUT_OF_RESOURCES Memory allocation failed
  @retval EFI_SUCCESS Retrieved versions successfully
**/
EFI_STATUS
UseLatestNsLabelVersion(
  IN     NVM_IS *pIS,
  IN     DIMM *pParentDimm,
     OUT BOOLEAN *pUseLatestVersion
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pNode = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  UINT16 Index = 0;
  BOOLEAN DimmLabelIsLatest = FALSE;
  BOOLEAN FirstDimmLabelIsLatest = FALSE;
  DIMM *pDimm = NULL;

  NVDIMM_ENTRY();

  if (pUseLatestVersion == NULL) {
    goto Finish;
  }

  if (pParentDimm != NULL) {
    // Storage namespace
    pDimm = pParentDimm;

    ReturnCode = CheckDimmNsLabelVersion(pDimm, &DimmLabelIsLatest);
    if (EFI_ERROR(ReturnCode)) {
      if (ReturnCode == EFI_NOT_FOUND) {
        // If no index block found, use v1.2
        DimmLabelIsLatest = TRUE;
      } else {
        goto Finish;
      }
    }
  } else {
    // Appdirect namespace
    if (pIS == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {
      pDimmRegion = DIMM_REGION_FROM_NODE(pNode);
      pDimm = pDimmRegion->pDimm;

      ReturnCode = CheckDimmNsLabelVersion(pDimm, &DimmLabelIsLatest);
      if (ReturnCode == EFI_NOT_FOUND) {
        // Did not find any index block on the DIMM, no issues, check next DIMM
        continue;
      }

      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }

      if (Index == 0) {
        // Store info of first DIMM
        FirstDimmLabelIsLatest = DimmLabelIsLatest;
      }

      if (Index != 0 && FirstDimmLabelIsLatest != DimmLabelIsLatest) {
        /** Mismatch for index block versions on the DIMMs **/
        ReturnCode = EFI_DEVICE_ERROR;
        goto Finish;
      }

      Index++;
    }

    // If none of the DIMMs in the IS had an index block, we will use v1.2
    if (Index == 0) {
      DimmLabelIsLatest = TRUE;
    }
  }

  *pUseLatestVersion = DimmLabelIsLatest;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Creates the namespace labels based on the version of the labels

  @param[in] pNamespace The target namespace
  @param[in] Index The index of the label in the namespace
  @param[in] UseLatestVersion Use latest version of labels
  @param[in out] pLabel The output label

  @retval EFI_INVALID_PARAMETER Null parameter passed
  @retval EFI_SUCCESS Created new label successfully
**/
EFI_STATUS
CreateNamespaceLabels(
  IN     NAMESPACE *pNamespace,
  IN     UINT16 Index,
  IN     BOOLEAN UseLatestVersion,
  IN OUT NAMESPACE_LABEL *pLabel
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  BOOLEAN ChecksumInserted = FALSE;
  NVM_IS *pIS = NULL;

  NVDIMM_ENTRY();

  if (pNamespace == NULL || pLabel == NULL) {
    goto Finish;
  }

  pLabel->Dpa = pNamespace->Range[Index].Dpa;
  pLabel->RawSize = pNamespace->Range[Index].Size;
  pLabel->Flags = pNamespace->Flags;
  CopyMem_S(&pLabel->Uuid, sizeof(pLabel->Uuid), pNamespace->NamespaceGuid, sizeof(pLabel->Uuid));
  CopyMem_S(&pLabel->Name, sizeof(pLabel->Name), pNamespace->Name, sizeof(pLabel->Name));
  pIS = pNamespace->pParentIS;
  if (pIS == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }
  pLabel->Position = (UINT16)Index;
  pLabel->NumberOfLabels = (UINT16)pNamespace->RangesCount;
  pLabel->InterleaveSetCookie = (UseLatestVersion) ? pIS->InterleaveSetCookie : pIS->InterleaveSetCookie_1_1;
  pLabel->LbaSize = (UseLatestVersion) ? AD_NAMESPACE_LABEL_LBA_SIZE_4K : 0;
  if (UseLatestVersion) {
    CopyGuid(&pLabel->TypeGuid, &gSpaRangePmRegionGuid);
  }

  if (UseLatestVersion) {
    if (pNamespace->IsBttEnabled) {
      CopyGuid(&pLabel->AddressAbstractionGuid, &gBttAbstractionGuid);
    }

    ChecksumInserted =
      ChecksumOperations(pLabel, sizeof(*pLabel), &pLabel->Checksum, TRUE);
    if (!ChecksumInserted) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Insert Namespace labels into a Label Storage Area of a DIMM.
  Function updates all required fields, index bitmap, sequence
  numbers, etc. Finally Label Storage Area is written to the DIMM.

  @param[in] pDimm DIMM structure pointer to which insert the labels
  @param[in] ppLabel Array of label structures to be inserted
  @param[in] LabelCount Number of labels in the array
  @param[in] LabelVersionMajor Major version of label to init
  @param[in] LabelVersionMinor Minor version of label to init

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_OUT_OF_RESOURCES No more label free slots in LSA
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
InsertNamespaceLabels(
  IN     DIMM *pDimm,
  IN     NAMESPACE_LABEL **ppLabel,
  IN     UINT16 LabelCount,
  IN     UINT16 LabelVersionMajor,
  IN     UINT16 LabelVersionMinor
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LABEL_STORAGE_AREA *pLsa = NULL;
  UINT16 Index = 0;
  UINT16 SlotNumber = 0;
  UINT16 FreeSlots = 0;
  UINT16 CurrentIndex = 0;
  UINT16 NextIndex = 0;
  UINT32 FreeBlocks = 0;

  NVDIMM_ENTRY();
  if (pDimm == NULL || ppLabel == NULL || LabelCount > MAX_NAMESPACE_RANGES) {
    goto Finish;
  }

  if (pDimm->LsaStatus == LSA_NOT_INIT) {
    ReturnCode = InitializeLabelStorageArea(pDimm, LabelVersionMajor, LabelVersionMinor);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Unable to initialize LSA data on a DIMM 0x%x", pDimm->DeviceHandle.AsUint32);
      pDimm->LsaStatus = LSA_COULD_NOT_INIT;
      goto Finish;
    }

    // Check if the LSA was written properly and is not corrupted
    ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
    if (EFI_ERROR(ReturnCode)) {
      pDimm->LsaStatus = LSA_CORRUPTED_AFTER_INIT;
      NVDIMM_DBG("LSA corrupted after initialization on DIMM 0x%x", pDimm->DeviceHandle.AsUint32);
      goto Finish;
    }
    FREE_POOL_SAFE(pLsa);
    pDimm->LsaStatus = LSA_OK;
  }

  ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
  if (EFI_ERROR(ReturnCode) || pLsa == NULL) {
    goto Finish;
  }

  GetLsaFreeSlots(pLsa, &FreeSlots, NULL);
  if (FreeSlots < LabelCount + 1) {
    // After adding the Labels at least one free slot must remain
    NVDIMM_DBG("Too few empty label slots.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, &NextIndex);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  FreeBlocks = LABELS_TO_FREE_BYTES(ROUNDUP(pLsa->Index[CurrentIndex].NumberOfLabels, 8));
  // Copy free bitmap to next index before updating it
  CopyMem_S(pLsa->Index[NextIndex].pFree, FreeBlocks, pLsa->Index[CurrentIndex].pFree, FreeBlocks);

  // Insert labels & update next index free map
  for (Index = 0; Index < LabelCount; Index++) {
    ReturnCode = GetLsaFreeSlots(pLsa, NULL, &SlotNumber);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    ppLabel[Index]->Slot = SlotNumber;
    CopyMem_S(&pLsa->pLabels[SlotNumber], sizeof(pLsa->pLabels[SlotNumber]), ppLabel[Index], sizeof(pLsa->pLabels[SlotNumber]));
    ChangeSlotStatus(&pLsa->Index[NextIndex], SlotNumber, SLOT_USED);
  }
  ReturnCode = UpdateLsaIndex(pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = WriteLabelStorageArea(pDimm->DimmID, pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  FreeLsaSafe(&pLsa);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Modify attributes of a Label located at specified slot.
  Function updates all required fields, index bitmap, sequence
  numbers, etc. Finally Label Storage Area is written to the DIMM.

  @param[in] pDimm DIMM structure pointer to which insert the labels
  @param[in] SlotNumber Target LSA slot number
  @param[in] pFlags New value of Flags field OPTIONAL
  @param[in] pName New value of name OPTIONAL
  @param[in] TotalLabelsNum the new amount of the namespace labels OPTIONAL
  @param[in] pNewPosition pointer the new index position for this label OPTIONAL
  @param[in] NewRawSize new size for the label (support for modify) OPTIONAL

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
ModifyLabelAtSlot(
  IN     DIMM *pDimm,
  IN     UINT16 SlotNumber,
  IN     UINT32 *pFlags, OPTIONAL
  IN     CHAR8 *pName, OPTIONAL
  IN     UINT16 TotalLabelsNum, OPTIONAL
  IN     UINT16 *pNewPosition, OPTIONAL
  IN     UINT64 NewRawSize OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LABEL_STORAGE_AREA *pLsa = NULL;
  UINT16 NewSlot = UNDEFINED_SLOT_ID;
  UINT16 FreeSlots = 0;
  UINT16 CurrentIndex = 0;
  UINT16 NextIndex = 0;
  BOOLEAN ChecksumInserted = FALSE;
  UINT32 FreeBlocks = 0;
  BOOLEAN UseLatestVersion = FALSE;

  if (pDimm == NULL) {
    goto Finish;
  }

  ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
  if (EFI_ERROR(ReturnCode) || pLsa == NULL) {
    goto Finish;
  }

  GetLsaFreeSlots(pLsa, &FreeSlots, &NewSlot);
  if (FreeSlots < 1) {
    // Label modification requires at least one free slot
    NVDIMM_DBG("Too few empty label slots.");
    goto Finish;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, &NextIndex);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if ((pLsa->Index[CurrentIndex].Major == NSINDEX_MAJOR) &&
      (pLsa->Index[CurrentIndex].Minor == NSINDEX_MINOR_2)) {
    UseLatestVersion = TRUE;
  }

  FreeBlocks = LABELS_TO_FREE_BYTES(ROUNDUP(pLsa->Index[CurrentIndex].NumberOfLabels, 8));
  // Copy free bitmap to next index before updating it
  CopyMem_S(pLsa->Index[NextIndex].pFree, FreeBlocks, pLsa->Index[CurrentIndex].pFree, FreeBlocks);

  // Copy Label to a new position
  CopyMem_S(&pLsa->pLabels[NewSlot], sizeof(pLsa->pLabels[NewSlot]), &pLsa->pLabels[SlotNumber], sizeof(pLsa->pLabels[NewSlot]));
  pLsa->pLabels[NewSlot].Slot = NewSlot;

    // Update Label fields
  if (pFlags != NULL) {
    pLsa->pLabels[NewSlot].Flags.AsUint32 = *pFlags;
  }
  if (pName != NULL) {
    CopyMem_S(&pLsa->pLabels[NewSlot].Name, sizeof(pLsa->pLabels[NewSlot].Name), pName, NSLABEL_NAME_LEN);
  }
  if (TotalLabelsNum != 0) {
    pLsa->pLabels[NewSlot].NumberOfLabels = TotalLabelsNum;
  }
  if (pNewPosition != NULL) {
    pLsa->pLabels[NewSlot].Position = *pNewPosition;
  }
  if (NewRawSize != 0) {
    pLsa->pLabels[NewSlot].RawSize = NewRawSize;
  }

  if (UseLatestVersion) {
    ChecksumInserted =
        ChecksumOperations(&pLsa->pLabels[NewSlot], sizeof(pLsa->pLabels[NewSlot]), &pLsa->pLabels[NewSlot].Checksum, TRUE);
    if (!ChecksumInserted) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
  }

  ChangeSlotStatus(&pLsa->Index[NextIndex], NewSlot, SLOT_USED);
  ChangeSlotStatus(&pLsa->Index[NextIndex], SlotNumber, SLOT_FREE);
  ZeroMem(&pLsa->pLabels[SlotNumber], sizeof(pLsa->pLabels[SlotNumber]));

  ReturnCode = UpdateLsaIndex(pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = WriteLabelStorageArea(pDimm->DimmID, pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  FreeLsaSafe(&pLsa);
  return ReturnCode;
}

/**
  Modify attributes of all Namespace labels identified by UUID on a DIMM.
  Function updates all required fields, index bitmap, sequence
  numbers, etc. Finally Label Storage Area is written to the DIMM.

  @param[in] pDimm DIMM structure pointer to which insert the labels
  @param[in] pUuid Target Namespace identifier
  @param[in] pFlags New value of Flags field
  @param[in] pName New value of name
  @param[in] LabelsNum the new value for the total labels in the block namespace labels set

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_ABORTED Failed to modify the labels
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
ModifyNamespaceLabels(
  IN     DIMM *pDimm,
  IN     GUID *pUuid,
  IN     UINT32 *pFlags, OPTIONAL
  IN     CHAR8 *pName, OPTIONAL
  IN     UINT16 LabelsNum OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LABEL_STORAGE_AREA *pLsa = NULL;
  UINT16 Index = 0;
  UINT16 FreeSlots = 0;
  UINT16 FirstIndex = 0;
  UINT16 SlotsToModify[MAX_NAMESPACE_RANGES];
  UINT16 SlotsToModifyCount = 0;

  ZeroMem(SlotsToModify, sizeof(SlotsToModify));

  if (pDimm == NULL || pUuid == NULL) {
    goto Finish;
  }

  ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
  if (EFI_ERROR(ReturnCode) || pLsa == NULL) {
    goto Finish;
  }

  GetLsaFreeSlots(pLsa, &FreeSlots, NULL);
  if (FreeSlots < 1) {
    // Label modification requires at least one free slot
    NVDIMM_DBG("Too few empty label slots.");
    goto Finish;
  }

  // Collect slot numbers with specified UUID first
  for (Index = 0; Index < pLsa->Index[FirstIndex].NumberOfLabels; Index++) {
    if (CompareMem(&pLsa->pLabels[Index].Uuid, pUuid, sizeof(GUID)) == 0) {
      NVDIMM_DBG("Slot %d marked for update", Index);
      SlotsToModify[SlotsToModifyCount] = Index;
      SlotsToModifyCount++;
    }
  }
  // Now proceed with update one by one
  for (Index = 0; Index < SlotsToModifyCount; Index++) {
    ReturnCode = ModifyLabelAtSlot(pDimm, SlotsToModify[Index], pFlags, pName, LabelsNum, NULL, 0);
    if (EFI_ERROR(ReturnCode)) {
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }
  }
  NVDIMM_DBG("Modified %d labels", SlotsToModifyCount);

Finish:
  FreeLsaSafe(&pLsa);
  return ReturnCode;
}

/**
  Remove all Namespace labels with specified UUID from a DIMM.
  Function updates all required fields, index bitmap, sequence
  numbers, etc. Finally Label Storage Area is written to the DIMM.

  @param[in] pDimm DIMM structure pointer to which insert the labels
  @param[in] pUuid Target Namespace identifier
  @param[in] Dpa Limit removal to only one label corresponding to this DPA

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
RemoveNamespaceLabels(
  IN     DIMM *pDimm,
  IN     GUID *pUuid,
  IN     UINT64 Dpa OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LABEL_STORAGE_AREA *pLsa = NULL;
  UINT16 Index = 0;
  UINT16 CurrentIndex = 0;
  UINT16 NextIndex = 0;
  NAMESPACE_LABEL *pLabel = NULL;
  UINT32 FreeBlocks = 0;

  NVDIMM_ENTRY();
  if (pDimm == NULL) {
    goto Finish;
  }
  ReturnCode = ReadLabelStorageArea(pDimm->DimmID, &pLsa);
  if (EFI_ERROR(ReturnCode) || pLsa == NULL) {
    goto Finish;
  }

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, &NextIndex);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  FreeBlocks = LABELS_TO_FREE_BYTES(ROUNDUP(pLsa->Index[CurrentIndex].NumberOfLabels, 8));
  // Copy free bitmap to next index before modifying it
  CopyMem_S(pLsa->Index[NextIndex].pFree, FreeBlocks, pLsa->Index[CurrentIndex].pFree, FreeBlocks);

  for (Index = 0; Index < pLsa->Index[NextIndex].NumberOfLabels; Index++) {
    pLabel = &pLsa->pLabels[Index];
    if (CompareMem(&pLabel->Uuid, pUuid, sizeof(GUID)) != 0) {
      continue;
    }
    if (Dpa != 0 && Dpa != pLabel->Dpa) {
      continue;
    }
    ReturnCode = ChangeSlotStatus(&pLsa->Index[NextIndex], Index, SLOT_FREE);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to change slot %d status to %d", Index, SLOT_FREE);
      goto Finish;
    }
    ZeroMem(pLabel, sizeof(*pLabel));
    NVDIMM_DBG("Removing label from slot %d", Index);
  }

  ReturnCode = UpdateLsaIndex(pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = WriteLabelStorageArea(pDimm->DimmID, pLsa);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FreeLsaSafe(&pLsa);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  This function calculates the real block and raw size that needs to be allocated on the DIMM in
  order to allow the caller of his desired block size and raw size.

  The function checks the Raw size to block size alignment and the block size to cache line size alignment,
  doing appropriate increasing or decreasing of the Raw Size.

  At least one of the optional parameters is required to be specified.

  One of: BlockCount or DesiredRawSize must be specified, the other needs to equal 0.

  @param[in] DesiredRawSize the raw size of the device (should be multiply of DesiredBlockSize, but if not,
    the function will decrease it accordingly).
  @param[in] DesiredBlockSize the required block size that will be aligned to the cache line size.
  @param[in] BlockCount the input block amount - specified by the caller.
  @param[out] pRealDimmRawSize pointer to where the real DIMM raw size needed will be stored.
  @param[out] pRealDimmBlockSize pointer to where the real DIMM block size will be stored.
  @param[out] pBlockCount pointer to where the calculated block count will be stored.

  @retval EFI_SUCCESS the parameters are OK and the values were calculated properly.
  @retval EFI_INVALID_PARAMETER the provided parameters are not passed accordingly to the function header.
**/
EFI_STATUS
GetRealRawSizeAndRealBlockSize(
  IN     UINT64 DesiredRawSize OPTIONAL,
  IN     UINT32 DesiredBlockSize,
  IN     UINT64 BlockCount OPTIONAL,
     OUT UINT64 *pRealDimmRawSize OPTIONAL,
     OUT UINT32 *pRealDimmBlockSize OPTIONAL,
     OUT UINT64 *pBlockCount OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 TempRealBlockSize = 0;

  if ((pRealDimmRawSize == NULL && pRealDimmBlockSize == NULL && pBlockCount == NULL) ||
      (((DesiredRawSize != 0 && BlockCount != 0) ||
      (DesiredRawSize == 0 && BlockCount == 0)) && DesiredBlockSize == 0) ||
      (DesiredBlockSize == 0)) {
    goto Finish;
  }

  // If the desired size is not aligned to the desired block size - reduce the size until it is aligned.
  if (DesiredRawSize != 0) {
    // Slightly decrease the Raw size if it is not aligned with the desired block size.
    DesiredRawSize -= (DesiredRawSize % DesiredBlockSize);
    BlockCount = DesiredRawSize / DesiredBlockSize;
  } else {
    DesiredRawSize = BlockCount * DesiredBlockSize;
  }

  // If the block size is aligned to our Cache line size, it will stay the same.
  TempRealBlockSize = (UINT32) GetPhysicalBlockSize(DesiredBlockSize);

  if (pRealDimmRawSize != NULL) {
    *pRealDimmRawSize = BlockCount * TempRealBlockSize;
  }

  if (pRealDimmBlockSize != NULL) {
    *pRealDimmBlockSize = TempRealBlockSize;
  }

  if (pBlockCount != NULL) {
    *pBlockCount = BlockCount;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  return ReturnCode;
}

/**
  Function updates sequence number and checksum of a next Index Block
  of the specified Label Storage Area.

  @param[in] pLsa Target Label Storage Area structure

  @retval EFI_INVALID_PARAMETER Invalid set of parameters provided
  @retval EFI_OUT_OF_RESOURCES Unsuccessful checksum calculation
  @retval EFI_SUCCESS Operation successful
**/
EFI_STATUS
UpdateLsaIndex(
  IN     LABEL_STORAGE_AREA *pLsa
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT16 CurrentIndex = 0;
  UINT16 NextIndex = 0;
  BOOLEAN ChecksumInserted = FALSE;
  UINT8 *pRawData = NULL;
  UINT32 ChecksumOffset = 0;

  NVDIMM_ENTRY();

  ReturnCode = GetLsaIndexes(pLsa, &CurrentIndex, &NextIndex);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ChecksumOffset = OFFSET_OF(NAMESPACE_INDEX, Checksum);

  // Update sequence number and checksum
  pLsa->Index[NextIndex].Sequence = pLsa->Index[CurrentIndex].Sequence % 3 + 1;

  ReturnCode = LabelIndexAreaToRawData(pLsa, NextIndex, &pRawData);

  if (EFI_ERROR(ReturnCode) || (pRawData == NULL)) {
    NVDIMM_DBG("Failed to convert label area index to raw data");
    goto Finish;
  }

  ChecksumInserted = ChecksumOperations(pRawData, pLsa->Index[CurrentIndex].MySize,
                                         (UINT64 *)(pRawData + ChecksumOffset), TRUE);
  pLsa->Index[NextIndex].Checksum = *(UINT64 *)(pRawData + ChecksumOffset);
  if (!ChecksumInserted) {
    NVDIMM_DBG("Could not insert the checksum after LSA update.");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pRawData);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Clean the namespaces list

  @param[in,out] pList: the list to clean
**/
VOID
CleanNamespacesList(
  IN OUT LIST_ENTRY *pList
  )
{
  NAMESPACE *pNamespace = NULL;
  LIST_ENTRY *pNamespaceNode = NULL;
  LIST_ENTRY *pNamespaceNext = NULL;

  NVDIMM_ENTRY();

  if (pList == NULL) {
    goto Finish;
  }

  /** Free namespaces and remove them from the namespaces list **/
  LIST_FOR_EACH_SAFE(pNamespaceNode, pNamespaceNext, pList) {
    pNamespace = NAMESPACE_FROM_NODE(pNamespaceNode, NamespaceNode);
    RemoveEntryList(pNamespaceNode);
    FREE_POOL_SAFE(pNamespace);
  }

Finish:
  NVDIMM_EXIT();
}

/**
  Compare region SPA offset field in interleave set cookie structure

  @param[in] pFirst First item to compare
  @param[in] pSecond Second item to compare

  @retval -1 if first is less than second
  @retval  0 if first is equal to second
  @retval  1 if first is greater than second
**/
STATIC
INT32
CompareRegionSpaOffsetInISet(
  IN     VOID *pFirst,
  IN     VOID *pSecond
  )
{
  NVM_COOKIE_DATA *pOffsetFirst = NULL;
  NVM_COOKIE_DATA *pOffsetSecond = NULL;

  if (pFirst == NULL || pSecond == NULL) {
    NVDIMM_DBG("NULL pointer found.");
    return 0;
  }

  pOffsetFirst = (NVM_COOKIE_DATA *) pFirst;
  pOffsetSecond = (NVM_COOKIE_DATA *) pSecond;

  if (pOffsetFirst->RegionSpaOffset < pOffsetSecond->RegionSpaOffset) {
    return -1;
  } else if (pOffsetFirst->RegionSpaOffset > pOffsetSecond->RegionSpaOffset) {
    return 1;
  } else {
    return 0;
  }
}

/**
  Calculate checksum (called Cookie) from Namespace ranges v1.2

  @param[in]     pFitHead Fully populated NVM Firmware Interface Table
  @param[in]     pIS  InterleaveSet that interleave set cookie will be calculated for

  @retval EFI_SUCCESS           Success
  @retval EFI_LOAD_ERROR        When calculated cookie didn't match
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
CalculateISetCookie(
  IN     ParsedFitHeader *pFitHead,
  IN OUT NVM_IS *pIS
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT64 Checksum = 0;
  NVM_COOKIE_DATA ISetCookieData[MAX_NAMESPACE_RANGES];
  NVM_COOKIE_DATA TmpCookieData;
  BOOLEAN ChecksumInserted = FALSE;
  NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure = NULL;
  ControlRegionTbl *pControlRegionTable = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 RegionCount = 0;

  NVDIMM_ENTRY();

  ZeroMem(ISetCookieData, sizeof(ISetCookieData));
  ZeroMem(&TmpCookieData, sizeof(TmpCookieData));

  if (pFitHead == NULL || pIS == NULL || pIS->pSpaTbl == NULL) {
    goto Finish;
  }

  ReturnCode = GetListSize(&pIS->DimmRegionList, &RegionCount);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  LIST_FOR_EACH(pDimmNode, &pIS->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pDimmNode);
    if (pDimmRegion->pDimm == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    ReturnCode = GetNvDimmRegionMappingStructureForPid(pFitHead, pDimmRegion->pDimm->DimmID,
            NULL, TRUE, pIS->pSpaTbl->SpaRangeDescriptionTableIndex, &pNvDimmRegionMappingStructure);
    if (EFI_ERROR(ReturnCode) || pNvDimmRegionMappingStructure == NULL) {
      goto Finish;
    }

    ReturnCode = GetControlRegionTableForNvDimmRegionTable(pFitHead, pNvDimmRegionMappingStructure, &pControlRegionTable);
    if (EFI_ERROR(ReturnCode) || pNvDimmRegionMappingStructure == NULL) {
      goto Finish;
    }

    ISetCookieData[Index].RegionSpaOffset = pNvDimmRegionMappingStructure->RegionOffset;
    ISetCookieData[Index].SerialNum = pControlRegionTable->SerialNumber;
    ISetCookieData[Index].VendorId = pControlRegionTable->VendorId;
    ISetCookieData[Index].ManufacturingDate = pControlRegionTable->ManufacturingDate;
    ISetCookieData[Index].ManufacturingLocation = pControlRegionTable->ManufacturingLocation;
    Index++;
  }

  ReturnCode = BubbleSort(ISetCookieData, RegionCount,
      sizeof(NVM_COOKIE_DATA), CompareRegionSpaOffsetInISet);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ChecksumInserted =
    ChecksumOperations(&ISetCookieData, sizeof(NVM_COOKIE_DATA) * RegionCount, &Checksum, TRUE);

  if (!ChecksumInserted) {
    NVDIMM_DBG("Could not calculate the checksum.");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  pIS->InterleaveSetCookie = Checksum;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate checksum (called Cookie) from Namespace ranges for Namespace Labels V1.1

  @param[in]     pFitHead Fully populated NVM Firmware Interface Table
  @param[in]     pIS  InterleaveSet that interleave set cookie will be calculated for

  @retval EFI_SUCCESS           Success
  @retval EFI_LOAD_ERROR        When calculated cookie didn't match
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
CalculateISetCookieVer1_1(
  IN     ParsedFitHeader *pFitHead,
  IN OUT NVM_IS *pIS
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT64 Checksum = 0;
  NVM_COOKIE_DATA_1_1 ISetCookieData[MAX_NAMESPACE_RANGES];
  NVM_COOKIE_DATA_1_1 TmpCookieData;
  BOOLEAN ChecksumInserted = FALSE;
  NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure = NULL;
  ControlRegionTbl *pControlRegionTable = NULL;
  DIMM_REGION *pDimmRegion = NULL;
  LIST_ENTRY *pDimmNode = NULL;
  UINT32 RegionCount = 0;

  NVDIMM_ENTRY();

  ZeroMem(ISetCookieData, sizeof(ISetCookieData));
  ZeroMem(&TmpCookieData, sizeof(TmpCookieData));

  if (pFitHead == NULL || pIS == NULL ||
      pIS->pSpaTbl == NULL) {
    goto Finish;
  }

  ReturnCode = GetListSize(&pIS->DimmRegionList, &RegionCount);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  LIST_FOR_EACH(pDimmNode, &pIS->DimmRegionList) {
    pDimmRegion = DIMM_REGION_FROM_NODE(pDimmNode);
    if (pDimmRegion->pDimm == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    ReturnCode = GetNvDimmRegionMappingStructureForPid(pFitHead, pDimmRegion->pDimm->DimmID,
            NULL, TRUE, pIS->pSpaTbl->SpaRangeDescriptionTableIndex, &pNvDimmRegionMappingStructure);
    if (EFI_ERROR(ReturnCode) || pNvDimmRegionMappingStructure == NULL) {
      goto Finish;
    }

    ReturnCode = GetControlRegionTableForNvDimmRegionTable(pFitHead, pNvDimmRegionMappingStructure, &pControlRegionTable);
    if (EFI_ERROR(ReturnCode) || pNvDimmRegionMappingStructure == NULL) {
      goto Finish;
    }

    ISetCookieData[Index].RegionSpaOffset = pNvDimmRegionMappingStructure->RegionOffset;
    ISetCookieData[Index].SerialNum = pControlRegionTable->SerialNumber;
    Index++;
  }

  ReturnCode = BubbleSort(ISetCookieData, RegionCount,
      sizeof(NVM_COOKIE_DATA_1_1), CompareRegionSpaOffsetInISet);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ChecksumInserted =
    ChecksumOperations(&ISetCookieData, sizeof(NVM_COOKIE_DATA_1_1) * RegionCount, &Checksum, TRUE);

  if (!ChecksumInserted) {
    NVDIMM_DBG("Could not calculate the checksum.");
    ReturnCode = EFI_LOAD_ERROR;
    goto Finish;
  }

  pIS->InterleaveSetCookie_1_1 = Checksum;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Find and assign parent Interleave Set for AppDirect Namespace

  @param[in,out] pNamespace AppDirect Namespace that Interleave Set will be found for

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_NOT_FOUND         Interleave Set has not been found
**/
EFI_STATUS
FindAndAssignISForNamespace(
  IN OUT NAMESPACE *pNamespace
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  LIST_ENTRY *pIsNode = NULL;
  NVM_IS *pIs = NULL;
  LIST_ENTRY *pRegionNode = NULL;
  LIST_ENTRY *pFreeMapNode = NULL;
  DIMM_REGION *pRegion = NULL;
  BOOLEAN ISetFound = FALSE;
  BOOLEAN RegionFound = FALSE;
  UINT32 Index = 0;
  UINT32 DimmRegionListSize = 0;
  LIST_ENTRY *pFreemapList = NULL;
  MEMMAP_RANGE *pRange = NULL;
  UINT64 ISCookie = 0;

  NVDIMM_ENTRY();

  if (pNamespace == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  LIST_FOR_EACH(pIsNode, &gNvmDimmData->PMEMDev.ISs) {
    pIs = IS_FROM_NODE(pIsNode);
    ISetFound = FALSE;

    ISCookie = (pNamespace->Major == NSINDEX_MAJOR && pNamespace->Minor == NSINDEX_MINOR_1) ?
                 pIs->InterleaveSetCookie_1_1 : pIs->InterleaveSetCookie;

    /** Cookie from label must match the cookie calculated for the interleave set **/
    /** Bail out if did not find a match for old labels or new labels **/
    if (pNamespace->InterleaveSetCookie != ISCookie) {
      NVDIMM_DBG("Cookie from label: %llx Cookie from IS: %llx", pNamespace->InterleaveSetCookie, ISCookie);
      continue;
    }

    // Assume that we will find a proper Interleave Set
    // Let's assign the parent IS to the namespace right now. If the size of the namespace region count does not
    // match the number of regions on the IS, that means we found broken labels (partial namespace), and we do
    // want to surface such a namespace with health state as critical.

    ISetFound = TRUE;
    pNamespace->pParentIS = pIs;
    InsertTailList(&pIs->AppDirectNamespaceList, &pNamespace->IsNode);

    ReturnCode = GetListSize(&pIs->DimmRegionList, &DimmRegionListSize);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    /** Interleave Set and AppDirect Namespace have to have the same number of regions/ranges **/
    if (DimmRegionListSize != pNamespace->RangesCount) {
      continue;
    }

    LIST_FOR_EACH(pRegionNode, &pIs->DimmRegionList) {
      pRegion = DIMM_REGION_FROM_NODE(pRegionNode);

      pFreemapList = (LIST_ENTRY *) AllocateZeroPool(sizeof(*pFreemapList));

      if (pFreemapList == NULL) {
        ReturnCode = EFI_OUT_OF_RESOURCES;
        goto Finish;
      }

      InitializeListHead(pFreemapList);

      ReturnCode = GetDimmFreemap(pRegion->pDimm, FreeCapacityForADMode, pFreemapList);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed to get free memory range");
        FreeMemmapItems(pFreemapList);
        goto Finish;
      }

      RegionFound = FALSE;
      for (Index = 0; Index < pNamespace->RangesCount && !RegionFound; Index++) {
        LIST_FOR_EACH(pFreeMapNode, pFreemapList) {
          pRange = MEMMAP_RANGE_FROM_NODE(pFreeMapNode);
          /** NS DPA range from label must exist inside a free AD capacity range **/
          if (pNamespace->Range[Index].pDimm == pRegion->pDimm
            && pNamespace->Range[Index].Dpa >= pRange->RangeStartDpa
            && ((pNamespace->Range[Index].Dpa + pNamespace->Range[Index].Size) <= (pRange->RangeStartDpa + pRange->RangeLength))) {
            RegionFound = TRUE;
            break;
          }
        }
      }

      FreeMemmapItems(pFreemapList);

      if (!RegionFound) {
        ISetFound = FALSE;
        break;
      }
    }

    if (ISetFound) {
      // If the IS is found and mapped correctly, we will get a SPA address for the Namespace
      ReturnCode = CalculateAppDirectNamespaceBaseSpa(pNamespace, pIs);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Unable to locate base SPA address of Namespace");
        goto Finish;
      }

      break;
    }
  }

  if (!ISetFound) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Calculate actual Namespace size on Dimm

  @param[in] NamespaceType Type of the namespace to be created (Block or PM).
  @param[in] BlockSize the size of each of the block in the device.
  @param[in] Capacity usable capacity
  @param[in] Mode -  boolean value to decide when the block namespace should have the BTT arena included
  @param[out] pActualBlockCount actual block count that needs to be occupied on Dimm
  @param[out] pActualCapacity actual capacity that needs to be occupied on Dimm
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS           Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
ConvertUsableSizeToActualSize(
  IN     UINT32 BlockSize,
  IN     UINT64 Capacity,
  IN     BOOLEAN Mode,
     OUT UINT64 *pActualBlockCount,
     OUT UINT64 *pActualCapacity,
     OUT COMMAND_STATUS *pCommandStatus
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT64 ActualBlockCount = 0;
  UINT64 ActualCapacity = Capacity;

  NVDIMM_ENTRY();

  if (pActualBlockCount == NULL || pActualCapacity == NULL || pCommandStatus == NULL) {
    goto Finish;
  }

  // In case we have a non pow(2) block size, we might need to make a proper block alignment
  if (ActualCapacity % BlockSize != 0) {
    ActualCapacity = ROUNDUP(ActualCapacity, BlockSize);
  }

  ActualBlockCount = ActualCapacity / BlockSize;

  if (ActualCapacity == 0) {
    ResetCmdStatus(pCommandStatus, NVM_ERR_INVALID_NAMESPACE_CAPACITY);
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pActualBlockCount = ActualBlockCount;
  *pActualCapacity = ActualCapacity;
  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Check if there is at least one Namespace on specified Dimms.

  @param[in] pDimms Array of Dimm pointers
  @param[in] DimmIdsCount Number of items in array
  @param[out] pFound Output variable saying if there is Namespace or not

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
IsNamespaceOnDimms(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT BOOLEAN *pFound
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  UINT32 NamespacesNum = 0;

  NVDIMM_ENTRY();

  if (pDimms == NULL || pFound == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pFound = FALSE;

  for (Index = 0; Index < DimmsNum; Index++) {
    if (pDimms[Index] == NULL) {
      ReturnCode = EFI_ABORTED;
      goto Finish;
    }

    for (Index2 = 0; Index2 < pDimms[Index]->ISsNum; Index2++) {
      ReturnCode = GetListSize(&pDimms[Index]->pISs[Index2]->AppDirectNamespaceList, &NamespacesNum);
      if (EFI_ERROR(ReturnCode)) {
        goto Finish;
      }
      if (NamespacesNum > 0) {
        *pFound = TRUE;
        break;
      }
    }
    if (*pFound) {
      break;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Retrieve mapped AppDirect memory from NFIT.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in, out] pIS Interleave Set to retrieve AppDirect I/O structures for

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_NOT_FOUND SPA table has not been found
**/
EFI_STATUS
RetrieveAppDirectMappingFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN OUT NVM_IS *pIS
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  SpaRangeTbl *pSpa = NULL;
  NvDimmRegionMappingStructure *pNvDimmRegion = NULL;
  LIST_ENTRY *pNode = NULL;
  DIMM_REGION *pRegion = NULL;
  BOOLEAN SameRegionOffset = FALSE;
  BOOLEAN SameRegionSize = FALSE;
  BOOLEAN MatchedDimmFound = FALSE;
  UINT32 MatchedDimmsNum = 0;
  BOOLEAN MatchedSpaFound = FALSE;
  UINT32 DimmsNumInIS = 0;

  NVDIMM_ENTRY();

  if (pFitHead == NULL || pIS == NULL) {
    goto Finish;
  }

  if (pIS->pSpaTbl != NULL) {
    goto Finish;
  }

  ReturnCode = GetListSize(&pIS->DimmRegionList, &DimmsNumInIS);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /** Match Interleave Set with SPA table **/
  for (Index = 0; Index < pFitHead->SpaRangeTblesNum; Index++) {
    pSpa = pFitHead->ppSpaRangeTbles[Index];

    /** Start with TRUE. If everything will be matched and valid, it will stay as TRUE. **/
    MatchedSpaFound = TRUE;
    MatchedDimmsNum = 0;

    for (Index2 = 0; Index2 < pFitHead->NvDimmRegionMappingStructuresNum; Index2++) {
      pNvDimmRegion = pFitHead->ppNvDimmRegionMappingStructures[Index2];

      if (pSpa->SpaRangeDescriptionTableIndex == pNvDimmRegion->SpaRangeDescriptionTableIndex) {
        MatchedDimmFound = FALSE;

        LIST_FOR_EACH(pNode, &pIS->DimmRegionList) {
          pRegion = DIMM_REGION_FROM_NODE(pNode);

          if (pRegion->pDimm->DimmID == pNvDimmRegion->NvDimmPhysicalId) {
            SameRegionOffset = pRegion->pDimm->PmStart + pRegion->PartitionOffset ==
                pNvDimmRegion->NvDimmPhysicalAddressRegionBase;
            SameRegionSize = pRegion->PartitionSize == pNvDimmRegion->NvDimmRegionSize;

            if (SameRegionOffset && SameRegionSize) {
              MatchedDimmFound = TRUE;
              MatchedDimmsNum += 1;
              pRegion->SpaRegionOffset = pNvDimmRegion->RegionOffset;
            }
            break;
          }
        }

        if (!MatchedDimmFound) {
          MatchedSpaFound = FALSE;
          break;
        }
      }
    }

    if (MatchedSpaFound && MatchedDimmsNum == DimmsNumInIS) {
      /** The matched SPA has been found **/
      pIS->pSpaTbl = pSpa;
      break;
    }
  }

  if (pIS->pSpaTbl != NULL) {
    ReturnCode = EFI_SUCCESS;
  } else {
    ReturnCode = EFI_NOT_FOUND;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get block size of block device for Namesapce

  @param[in] pNamespace Namespace that block size of block device will be retrieved for

  @retval Block size
**/
UINT64
GetBlockDeviceBlockSize(
  IN     NAMESPACE *pNamespace
  )
{
  UINT64 BlockSize = 0;

  NVDIMM_ENTRY();

  if (pNamespace == NULL) {
    goto Finish;
  }

  BlockSize = pNamespace->Media.BlockSize;

Finish:
  NVDIMM_EXIT();
  return BlockSize;
}

/**
  Get persistent memory type for Namespace

  @param[in] pNamespace Namespace that persistent memory type will be determined for
  @param[out] pPersistentMemType Persistent memory type

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
**/
EFI_STATUS
GetPersistentMemoryType(
  IN     NAMESPACE *pNamespace,
     OUT UINT8 *pPersistentMemType
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  LIST_ENTRY *pMemmapList = NULL;
  BOOLEAN Interleaved = FALSE;
  UINT32 RegionsNum = 0;

  if (pNamespace == NULL || pPersistentMemType == NULL) {
    goto Finish;
  }

  if (pNamespace->NamespaceType == APPDIRECT_NAMESPACE) {
    /** No interleave set found for the AppDirect Namespace **/
    if (pNamespace->pParentIS == NULL) {
      ReturnCode = EFI_INVALID_PARAMETER;
      goto Finish;
    }
    ReturnCode = GetListSize(&pNamespace->pParentIS->DimmRegionList, &RegionsNum);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    Interleaved = RegionsNum > 1;

    *pPersistentMemType = (Interleaved) ? PM_TYPE_AD : PM_TYPE_AD_NI;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  if (pMemmapList != NULL) {
    FreeMemmapItems(pMemmapList);
    FREE_POOL_SAFE(pMemmapList);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Get accessible capacity in bytes

  @param[in] pNamespace Namespace that accessible capacity will be calculated for

  @retval Accessible capacity
**/
UINT64
GetAccessibleCapacity(
  IN     NAMESPACE *pNamespace
  )
{
  return pNamespace->BlockSize * pNamespace->BlockCount;
}

/**
  Get raw capacity in bytes

  @param[in] pNamespace Namespace that raw capacity will be calculated for

  @retval Raw capacity
**/
UINT64
GetRawCapacity(
  IN     NAMESPACE *pNamespace
  )
{
  return GetPhysicalBlockSize(pNamespace->BlockSize) * pNamespace->BlockCount;
}

#ifndef OS_BUILD
/**
  Checks to see if a given address block collides with one or more of the addresses BIOS has marked as bad

  @param[in] Address an array of bad addresses as returned from BIOS
  @param[in] Length the number of bad addresses

  @retval EFI_SUCCESS If the range does not collide
  @retval EFI_DEVICE_ERROR If the range is found to collide with one or more addresses from BIOS
**/
EFI_STATUS
IsAddressRangeInArsList(
  IN     UINT64  Address,
  IN     UINT64  Length
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  INT32 Index = 0;
  UINT64 BadBlockStart = 0;
  UINT64 BadBlockEnd = 0;
  UINT64 CheckBlockStart = Address;
  UINT64 CheckBlockEnd = Address + Length - 1;

  if (gArsBadRecordsCount == 0) {
      return ReturnCode;
  }

  NVDIMM_ENTRY();

  if (gArsBadRecordsCount < 0) {
    ReturnCode = LoadArsList();
    if (EFI_PROTOCOL_ERROR == ReturnCode)
    {
      ReturnCode = EFI_SUCCESS;
      NVDIMM_DBG("Failed to load the ARS list (protocol missing?) Cannot check for collisions.\n");
      goto Finish;
    }
  }

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to load the ARS list. Cannot check for collisions.\n");
    goto Finish;
  }

  if (gArsBadRecordsCount == 0) {
    NVDIMM_DBG("There are no bad addresses in the ARS list.\n");
    goto Finish;
  }

  if (NULL != gArsBadRecords && gArsBadRecordsCount > 0) {
    //Check that address range does not contain one of the bad addresses identified by BIOS
    for (Index = 0; Index < gArsBadRecordsCount; Index++) {
      BadBlockStart = gArsBadRecords[Index].SpaOfErrLoc;
      BadBlockEnd = gArsBadRecords[Index].SpaOfErrLoc + gArsBadRecords[Index].Length - 1;

      NVDIMM_DBG("Checking address 0x%llx, len 0x%llx against bad ARS address 0x%llx, len 0x%llx", Address, Length, gArsBadRecords[Index].SpaOfErrLoc, gArsBadRecords[Index].Length);

      if ((CheckBlockStart >= BadBlockStart   && CheckBlockStart <= BadBlockEnd)   || //the request starts at an address in the bad range
          (CheckBlockStart >= BadBlockStart   && CheckBlockEnd   <= BadBlockEnd)   || //the request fits entirly in a bad range
          (BadBlockStart   >= CheckBlockStart && BadBlockEnd     <= CheckBlockEnd) || //the bad range fits entirely in the check range
          (CheckBlockEnd   >= BadBlockStart   && CheckBlockEnd   <= BadBlockEnd))     //the request ends at an address in the bad range
      {
        ReturnCode = EFI_DEVICE_ERROR;
        NVDIMM_DBG("Collision detected with the ARS list");
        goto Finish;
      }
    }
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}
#endif
