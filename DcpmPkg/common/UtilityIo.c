/*
This file contains functions that were in Utility.c that aren't
self-contained or useful for all of our unit tested code. It allows us
to limit the scope of the implementation that is needed to get unit
tests running and happens to contain some API calls that we'll probably
want to stub in the future as well.
*/


#include <Utility.h>
#include <Debug.h>
#include <Library/BaseMemoryLib.h>
#include <Uefi/UefiBaseType.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Guid/FileInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Convert.h>
#ifdef OS_BUILD
#include <os.h>
#include <os_efi_preferences.h>
#else
#include <Library/SerialPortLib.h>
extern EFI_RUNTIME_SERVICES  *gRT;
#endif


extern EFI_GUID gNvmDimmConfigProtocolGuid;

/**
  Return a first found handle for specified protocol.

  @param[in] pProtocolGuid protocol that EFI handle will be found for.
  @param[out] pDriverHandle is the pointer to the result handle.

  @retval EFI_INVALID_PARAMETER if one or more input parameters are NULL.
  @retval all of the LocateHandleBuffer return values.
**/
EFI_STATUS
GetDriverHandle(
  IN     EFI_GUID *pProtocolGuid,
     OUT EFI_HANDLE *pDriverHandle
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINTN HandleCount = 0;
  EFI_HANDLE *pHandleBuffer = NULL;

  NVDIMM_ENTRY();

  if (pProtocolGuid == NULL || pDriverHandle == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pDriverHandle = NULL;

  /**
    Find the driver handle by searching for our custom NvmDimmConfig protocol
  **/
  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, pProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(ReturnCode) || HandleCount != 1) {
    ReturnCode = EFI_NOT_FOUND;
  } else {
    *pDriverHandle = pHandleBuffer[0];
  }

Finish:
  FREE_POOL_SAFE(pHandleBuffer);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Return the NvmDimmController handle.

  @param[out] pControllerHandle is the pointer to the result handle.

  @retval EFI_INVALID_PARAMETER if the pControllerHandle is NULL.
  @retval all of the LocateHandleBuffer return values.
**/
EFI_STATUS
GetControllerHandle(
     OUT EFI_HANDLE *pControllerHandle
  )
{
  EFI_STATUS ReturnCode = EFI_NOT_FOUND;
  EFI_HANDLE DriverHandle = NULL;
  UINT32 Index = 0;
  EFI_HANDLE *pHandleBuffer = NULL;
  UINT64 HandleCount = 0;

  NVDIMM_ENTRY();

  if (pControllerHandle == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pControllerHandle = NULL;

  ReturnCode = GetDriverHandle(&gNvmDimmConfigProtocolGuid, &DriverHandle);

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  /**
    Retrieve array of all handles in the handle database
  **/
  ReturnCode = gBS->LocateHandleBuffer(ByProtocol, &gEfiDevicePathProtocolGuid, NULL, &HandleCount, &pHandleBuffer);
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Could not find the handles set.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  /**
    Search all of the existing device path protocol instances for a device controlled by our driver.
  **/
  for (Index = 0; Index < HandleCount; Index++) {
    ReturnCode =
    EfiTestManagedDevice(pHandleBuffer[Index],
        DriverHandle, // Our driver handle equals the driver binding handle so this call is valid
        &gEfiDevicePathProtocolGuid);

    // If the handle is managed - this is our controller.
    if (!EFI_ERROR(ReturnCode)) {
      *pControllerHandle = pHandleBuffer[Index];
      break;
    }
  }

Finish:
  if (pHandleBuffer != NULL) {
    FreePool(pHandleBuffer);
  }

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}



/**
  Open the specified protocol.
  If the user does not provide a handle, the function will try
  to match the driver or the controller handle basing on the
  provided protocol GUID.
  No need to call close protocol because of the way it is opened.

  @param[in] Guid is the EFI GUID of the protocol we want to open.
  @param[out] ppProtocol is the pointer to a pointer where the opened
    protocol instance address will be returned.
  @param[in] pHandle a handle that we want to open the protocol on. OPTIONAL

  @retval EFI_SUCCESS if everything went successful.
  @retval EFI_INVALID_ARGUMENT if ppProtocol is NULL.

  Other return values from functions:
    getControllerHandle
    getDriverHandle
    gBS->OpenProtocol
**/
EFI_STATUS
OpenNvmDimmProtocol(
  IN     EFI_GUID Guid,
     OUT VOID **ppProtocol,
  IN     EFI_HANDLE pHandle OPTIONAL
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_HANDLE DeviceHandle = NULL;

  NVDIMM_ENTRY();

  if (pHandle == NULL) {
    if (CompareGuid(&Guid, &gEfiDevicePathProtocolGuid)) {
      ReturnCode = GetControllerHandle(&DeviceHandle);
    } else {
      ReturnCode = GetDriverHandle(&gNvmDimmConfigProtocolGuid, &DeviceHandle);
    }
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not determine the target device type, error = " FORMAT_EFI_STATUS "", ReturnCode);
      goto Finish;
    }
  } else {
    DeviceHandle = pHandle;
  }

  ReturnCode = gBS->OpenProtocol(
    DeviceHandle,
    &Guid,
    ppProtocol,
    NULL,
    NULL,
    EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_ALREADY_STARTED) {
      ReturnCode = EFI_SUCCESS;
    } else {
      NVDIMM_WARN("Failed to open NvmDimmProtocol, error = " FORMAT_EFI_STATUS "", ReturnCode);
      goto Finish;
    }
  }

  if (CompareGuid(&Guid, &gNvmDimmConfigProtocolGuid)) {
    ReturnCode = CheckConfigProtocolVersion((EFI_DCPMM_CONFIG2_PROTOCOL *) *ppProtocol);
    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Failed to get the proper config protocol.");
      ppProtocol = NULL;
    }
  }


Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Open file or create new file.

  @param[in] pArgFilePath path to a file that will be opened
  @param[out] pFileHandle output handler
  @param[in, optional] pCurrentDirectory is the current directory path to where
    we should start to search for the file.
  @param[in] CreateFileFlag - TRUE to create new file or FALSE to open
    existing file

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER pFilePath is NULL or empty or pFileHandle is NULL
  @retval EFI_PROTOCOL_ERROR if there is no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
**/
EFI_STATUS
OpenFile(
  IN     CHAR16 *pArgFilePath,
     OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag
  )
{
  return OpenFileWithFlag(pArgFilePath, pFileHandle, pCurrentDirectory, CreateFileFlag, FALSE);
}

/**
  Open file or create new file in binary mode.

  @param[in] pArgFilePath path to a file that will be opened
  @param[out] pFileHandle output handler
  @param[in, optional] pCurrentDirectory is the current directory path to where
    we should start to search for the file.
  @param[in] CreateFileFlag TRUE to create new file or FALSE to open
    existing file

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER pFilePath is NULL or empty or pFileHandle is NULL
  @retval EFI_PROTOCOL_ERROR if there is no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
**/
EFI_STATUS
OpenFileBinary(
  IN     CHAR16 *pArgFilePath,
  OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag
)
{
#ifdef OS_BUILD
#ifdef _MSC_VER
  return OpenFileWithFlag(pArgFilePath, pFileHandle, pCurrentDirectory, CreateFileFlag, TRUE);
#endif
#endif
  return OpenFileWithFlag(pArgFilePath, pFileHandle, pCurrentDirectory, CreateFileFlag, FALSE);
}

/**
  Open file or create new file with the proper flags.

  @param[in] pArgFilePath path to a file that will be opened
  @param[out] pFileHandle output handler
  @param[in, optional] pCurrentDirectory is the current directory path to where
    we should start to search for the file.
  @param[in] CreateFileFlag - TRUE to create new file or FALSE to open
    existing file
  @param[in] binary - use binary open

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER pFilePath is NULL or empty or pFileHandle is NULL
  @retval EFI_PROTOCOL_ERROR if there is no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
**/
EFI_STATUS
OpenFileWithFlag(
  IN     CHAR16 *pArgFilePath,
  OUT EFI_FILE_HANDLE *pFileHandle,
  IN     CONST CHAR16 *pCurrentDirectory OPTIONAL,
  IN     BOOLEAN CreateFileFlag,
  BOOLEAN binary
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pVolume = NULL;
  CHAR16 *pFullFilePath = NULL;
  CHAR16 *pFilePath = NULL;
  EFI_FILE_HANDLE RootDirHandle = NULL;
  EFI_HANDLE *pHandles = NULL;
  UINTN HandlesSize = 0;
  INT32 Index = 0;
  UINTN CurrentWorkingDirLength = 0;
  NVDIMM_ENTRY();

  /**
     @todo: This function have problem in two scenarios:
     1) There are two or more file systems on platform. At least two of them contains file
        with provided name. There is no way to tell which one would be opened.
        It depends on enumeration order returned by LocateHandleBuffer.
     2) Provided file name contains more than one dot (example: file.1.0.img),
        there is more than one file system on the platform and said file is not on the first one
        enumerated in pHandles returned by LocateHandleBuffer.
        In this case open and read succeed on each file system but data returned by read on those that
        do not contain said file is garbage. This is UDK issue.
  **/

  if (pArgFilePath == NULL || pArgFilePath[0] == '\0' || pFileHandle == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (pCurrentDirectory != NULL && pArgFilePath[0] != '\\') {
    CurrentWorkingDirLength = StrLen(pCurrentDirectory);
    if (CurrentWorkingDirLength != 0 && pCurrentDirectory[CurrentWorkingDirLength - 1] != '\\') {
      pFullFilePath = CatSPrint(NULL, FORMAT_STR L"\\" FORMAT_STR, pCurrentDirectory, pArgFilePath);
    } else {
      pFullFilePath = CatSPrint(NULL, FORMAT_STR FORMAT_STR, pCurrentDirectory, pArgFilePath);
    }

    if (pFullFilePath != NULL) {
      pFilePath = pFullFilePath;
      while (pFilePath[0] != '\\' && pFilePath[0] != '\0') {
        pFilePath++;
      }
    }
  }

  Rc = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandlesSize, &pHandles);
  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Couldn't find EfiSimpleFileSystemProtocol: " FORMAT_EFI_STATUS "", Rc);
    goto Finish;
  }

  if (pFilePath == NULL) {
    pFilePath = pArgFilePath;
  }

  for (Index = 0; Index < HandlesSize; Index++) {
    /**
      Get the file system protocol
    **/
    Rc = gBS->OpenProtocol(
      pHandles[Index],
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID *) &pVolume,
      NULL,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if (EFI_ERROR(Rc)) {
      goto AfterHandles;
    }

    /**
      Open the root file
    **/
    Rc = pVolume->OpenVolume(pVolume, &RootDirHandle);
    if (EFI_ERROR(Rc)) {
      goto AfterHandles;
    }

    if (CreateFileFlag) {
      // if EFI_FILE_MODE_CREATE then also EFI_FILE_MODE_READ and EFI_FILE_MODE_WRITE are needed.
      if (binary) {
        Rc = RootDirHandle->Open(RootDirHandle, pFileHandle, pFilePath,
          EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_BINARY, 0);
      } else {
        Rc = RootDirHandle->Open(RootDirHandle, pFileHandle, pFilePath,
          EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
      }
    } else {
      if (binary) {
        Rc = RootDirHandle->Open(RootDirHandle, pFileHandle, pFilePath, EFI_FILE_MODE_READ | EFI_FILE_MODE_BINARY, 0);
      } else {
        Rc = RootDirHandle->Open(RootDirHandle, pFileHandle, pFilePath, EFI_FILE_MODE_READ, 0);
      }
    }

    RootDirHandle->Close(RootDirHandle);
    if (!EFI_ERROR(Rc)) {
      break;
    }
  }

AfterHandles:
  FreePool(pHandles);
Finish:

  if (pFullFilePath != NULL) {
    FreePool(pFullFilePath);
  }

  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  Open file handle of root directory from given path

  @param[in] pDevicePath - path to file
  @param[out] pFileHandle - root directory file handle

**/
EFI_STATUS
OpenRootFileVolume(
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
     OUT EFI_FILE_HANDLE *pRootDirHandle
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_HANDLE DeviceHandle = NULL;
  EFI_DEVICE_PATH_PROTOCOL *pDevicePathTmp = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pVolume = NULL;

  if (pDevicePath == NULL || pRootDirHandle == NULL) {
    goto Finish;
  }
  // Copy device path pointer, LocateDevicePath modifies it
  pDevicePathTmp = pDevicePath;
  // Locate Handle for Simple File System Protocol on device
  ReturnCode = gBS->LocateDevicePath(&gEfiSimpleFileSystemProtocolGuid, &pDevicePathTmp, &DeviceHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = gBS->OpenProtocol(DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID *) &pVolume, NULL,
        NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  // Open the root file
  ReturnCode = pVolume->OpenVolume(pVolume, pRootDirHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Open file or create new file based on device path protocol.

  @param[in] pArgFilePath Pointer to path to a file that will be opened
  @param[in] pDevicePath Pointer to instance of device path protocol
  @param[in] CreateFileFlag - TRUE to create new file or FALSE to open
    existing file
  @param[out] pFileHandle Output file handler

  @retval EFI_SUCCESS File opened or created
  @retval EFI_INVALID_PARAMETER Input parameter is invalid
  @retval Others From LocateDevicePath, OpenProtocol, OpenVolume and Open
**/
EFI_STATUS
OpenFileByDevice(
  IN     CHAR16 *pArgFilePath,
  IN     EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  IN     BOOLEAN CreateFileFlag,
     OUT EFI_FILE_HANDLE *pFileHandle
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE RootDirHandle = NULL;
  NVDIMM_ENTRY();

  if (pArgFilePath == NULL || pDevicePath == NULL || pFileHandle == NULL) {
    goto Finish;
  }

  ReturnCode = OpenRootFileVolume(pDevicePath, &RootDirHandle);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (CreateFileFlag) {
    // if EFI_FILE_MODE_CREATE then also EFI_FILE_MODE_READ and EFI_FILE_MODE_WRITE are needed.
    ReturnCode = RootDirHandle->Open(RootDirHandle, pFileHandle, pArgFilePath,
        EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,
        0);
  } else {
    ReturnCode = RootDirHandle->Open(RootDirHandle, pFileHandle, pArgFilePath, EFI_FILE_MODE_READ, 0);
  }

  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Returns the size of the specified file.

  @param[in] FileHandle - handle to the opened file that we want to get the size for.
  @param[out] pFileSize - the result file size on bytes.

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one of the input parameters is a NULL.

  Other return values associated with the GetInfo callback.
**/
EFI_STATUS
GetFileSize(
  IN     EFI_FILE_HANDLE FileHandle,
     OUT UINT64 *pFileSize
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT64 BuffSize = 0;
  EFI_FILE_INFO *pFileInfo = NULL;

  if (FileHandle == NULL || pFileSize == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pFileSize = 0;

  ReturnCode = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &BuffSize, pFileInfo);

  if (ReturnCode != EFI_BUFFER_TOO_SMALL) {
    NVDIMM_DBG("pFileHandle->GetInfo returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
    goto Finish;
  }

  pFileInfo = AllocatePool(BuffSize);

  if (pFileInfo == NULL) {
    NVDIMM_DBG("Could not allocate resources.\n");
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &BuffSize, pFileInfo);

  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("pFileHandle->GetInfo returned: " FORMAT_EFI_STATUS ".\n", ReturnCode);
  }

  *pFileSize = pFileInfo->FileSize;

  FreePool(pFileInfo);

Finish:
  return ReturnCode;
}


/**
  Checks if the user-inputted desired ARS status matches with the
  current system-wide ARS status.

  @param[in] DesiredARSStatus Desired value of the ARS status to match against
  @param[out] pARSStatusMatched Pointer to a boolean value which shows if the
              current system ARS status matches the desired one.

  @retval EFI_SUCCESS if there were no problems
  @retval EFI_INVALID_PARAMETER if one of the input parameters is a NULL, or an invalid value.
**/
EFI_STATUS
MatchCurrentARSStatus(
  IN     UINT8 DesiredARSStatus,
     OUT BOOLEAN *pARSStatusMatched
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  UINT8 CurrentARSStatus = LONG_OP_STATUS_NOT_STARTED;

  NVDIMM_ENTRY();

  if ((pARSStatusMatched == NULL)||
      ((DesiredARSStatus != LONG_OP_STATUS_UNKNOWN) &&
       (DesiredARSStatus != LONG_OP_STATUS_NOT_STARTED) &&
       (DesiredARSStatus != LONG_OP_STATUS_IN_PROGRESS) &&
       (DesiredARSStatus != LONG_OP_STATUS_COMPLETED) &&
       (DesiredARSStatus != LONG_OP_STATUS_ABORTED) &&
       (DesiredARSStatus != LONG_OP_STATUS_ERROR))) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  *pARSStatusMatched = FALSE;

  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **) &pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetARSStatus(
      pNvmDimmConfigProtocol,
      &CurrentARSStatus
      );
  if (EFI_ERROR(ReturnCode)) {
    NVDIMM_ERR("Failed to get the current ARS status of the system");
    goto Finish;
  }

  if (CurrentARSStatus == DesiredARSStatus) {
    *pARSStatusMatched = TRUE;
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}


/**
  Poll long operation status

  Polls the status of the background operation on the dimm.

  @param [in] pNvmDimmConfigProtocol Pointer to the EFI_DCPMM_CONFIG2_PROTOCOL instance
  @param [in] DimmId Dimm ID of the dimm to poll status
  @param [in] OpcodeToPoll Specify an opcode to poll, 0 to poll regardless of opcode
  @param [in] SubOpcodeToPoll Specify an opcode to poll
  @param [in] Timeout for the background operation
**/
EFI_STATUS
PollLongOpStatus(
  IN     EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol,
  IN     UINT16 DimmId,
  IN     UINT8 OpcodeToPoll OPTIONAL,
  IN     UINT8 SubOpcodeToPoll OPTIONAL,
  IN     UINT64 Timeout
  )
{
  UINT8 EventCount = 0;
  EFI_STATUS ReturnCode = EFI_DEVICE_ERROR;
  UINT64 WaitIndex = 0;
  EFI_EVENT WaitList[2];
  EFI_STATUS LongOpEfiStatus = EFI_NOT_READY;
  UINT8 CmdOpcode = 0;
  UINT8 CmdSubOpcode = 0;

  ZeroMem(WaitList, sizeof(WaitList));

  gBS->CreateEvent(EVT_TIMER, TPL_NOTIFY, NULL, NULL, &WaitList[LONG_OP_POLL_EVENT_TIMER]);
  gBS->SetTimer(WaitList[LONG_OP_POLL_EVENT_TIMER], TimerPeriodic, LONG_OP_POLL_TIMER_INTERVAL);
  EventCount++;

  if (Timeout > 0) {
    gBS->CreateEvent(EVT_TIMER, TPL_NOTIFY, NULL, NULL, &WaitList[LONG_OP_POLL_EVENT_TIMEOUT]);
    gBS->SetTimer(WaitList[LONG_OP_POLL_EVENT_TIMEOUT], TimerRelative, Timeout);
    EventCount++;
  }

  do {
    ReturnCode = gBS->WaitForEvent(EventCount, WaitList, &WaitIndex);
    if (EFI_ERROR(ReturnCode)) {
      break;
    }

    ReturnCode = pNvmDimmConfigProtocol->GetLongOpStatus(pNvmDimmConfigProtocol, DimmId,
      &CmdOpcode, &CmdSubOpcode, NULL, NULL, &LongOpEfiStatus);

    if (EFI_ERROR(ReturnCode)) {
      NVDIMM_DBG("Could not get long operation status");
      goto Finish;
    }

    // If user passed in an opcode, validate that it matches the long operation opcode
    // If it doesn't match, assume it is < FIS 1.6 and not supported
    if (OpcodeToPoll != 0) {
      if (OpcodeToPoll != CmdOpcode || SubOpcodeToPoll != CmdSubOpcode) {
        ReturnCode = EFI_INCOMPATIBLE_VERSION;
        goto Finish;
      }
    }
    // Report back failure with the long op command
    if (EFI_ERROR(LongOpEfiStatus)) {
      if (LongOpEfiStatus != EFI_NO_RESPONSE) {
        ReturnCode = LongOpEfiStatus;
        goto Finish;
      }
    }

    if (WaitIndex == LONG_OP_POLL_EVENT_TIMEOUT) {
      NVDIMM_DBG("Timed out polling long operation status");
      ReturnCode = EFI_TIMEOUT;
      goto Finish;
    }
  } while (LongOpEfiStatus != EFI_SUCCESS);

  ReturnCode = EFI_SUCCESS;

Finish:
  gBS->CloseEvent(WaitList[LONG_OP_POLL_EVENT_TIMER]);
  if (Timeout > 0) {
    gBS->CloseEvent(WaitList[LONG_OP_POLL_EVENT_TIMEOUT]);
  }

  return ReturnCode;
}


/**
  Read file to given buffer

  * WARNING * caller is responsible for freeing ppFileBuffer

  @param[in] pFilePath - file path
  @param[in] pDevicePath - handle to obtain generic path/location information concerning the physical device
                          or logical device. The device path describes the location of the device the handle is for.
  @param[out] pFileSize - number of bytes written to buffer
  @param[out] ppFileBuffer - output buffer  * WARNING * caller is responsible for freeing

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_NOT_STARTED Test was not executed
  @retval EFI_SUCCESS All Ok
  @retval EFI_OUT_OF_RESOURCES if memory allocation fails.
**/
EFI_STATUS
FileRead(
  IN      CHAR16 *pFilePath,
  IN      EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
  IN      CONST UINT64  MaxFileSize,
     OUT  UINT64 *pFileSize,
     OUT  VOID **ppFileBuffer
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  EFI_FILE_HANDLE pFileHandle = NULL;

  if (pFileSize == NULL || ppFileBuffer == NULL || pFilePath == NULL) {
    goto Finish;
  }
#ifdef OS_BUILD
  ReturnCode = OpenFile(pFilePath, &pFileHandle, NULL, 0);
  if (EFI_ERROR(ReturnCode) || pFileHandle == NULL) {
     NVDIMM_DBG("Failed opening File (" FORMAT_EFI_STATUS ")", ReturnCode);
     goto Finish;
  }
#else
  if (pDevicePath == NULL) {
     goto Finish;
  }
  ReturnCode = OpenFileByDevice(pFilePath, pDevicePath, FALSE, &pFileHandle);
  if (EFI_ERROR(ReturnCode) || pFileHandle == NULL) {
    NVDIMM_DBG("Failed opening File (" FORMAT_EFI_STATUS ")", ReturnCode);
    goto Finish;
  }
#endif
  ReturnCode = GetFileSize(pFileHandle, pFileSize);
  if (EFI_ERROR(ReturnCode) || *pFileSize == 0) {
    goto Finish;
  }

  if (MaxFileSize != 0 && *pFileSize > MaxFileSize) {
    ReturnCode = EFI_BUFFER_TOO_SMALL;
    goto Finish;
  }

  *ppFileBuffer = AllocateZeroPool(*pFileSize);
  if (*ppFileBuffer == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = pFileHandle->Read(pFileHandle, pFileSize, *ppFileBuffer);
  if (EFI_ERROR(ReturnCode)) {
    goto FinishFreeBuffer;
  }

  // Everything went fine, do not free buffer
  goto Finish;

FinishFreeBuffer:
  FREE_POOL_SAFE(*ppFileBuffer);

Finish:
  if (pFileHandle != NULL) {
    pFileHandle->Close(pFileHandle);
  }
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Read ASCII line from a file.

  The function ignores carriage return chars.

  @param FileHandle handle to a file
  @param pLine output buffer that will be filled with read line
  @param LineSize size of pLine buffer
  @param pEndOfFile output variable to report about end of file

  @retval EFI_SUCCESS
  @retval EFI_BUFFER_TOO_SMALL when pLine buffer is too small
  @retval EFI_INVALID_PARAMETER pLine or pEndOfFile is NULL
**/
EFI_STATUS
ReadAsciiLineFromFile(
  IN     EFI_FILE_HANDLE FileHandle,
     OUT CHAR8 *pLine,
  IN     INT32 LineSize,
     OUT BOOLEAN *pEndOfFile
  )
{
  EFI_STATUS Rc = EFI_SUCCESS;
  CHAR8 Buffer = 0;
  UINTN BufferSizeInBytes = sizeof(Buffer);
  INT32 Index = 0;

  NVDIMM_ENTRY();
  if (pLine == NULL || pEndOfFile == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (LineSize == 0) {
    Rc = EFI_BUFFER_TOO_SMALL;
    goto Finish;
  }

  while (!EFI_ERROR(Rc = FileHandle->Read(FileHandle, &BufferSizeInBytes, &Buffer))) {
    // End of file
    if (BufferSizeInBytes == 0) {
      *pEndOfFile = TRUE;
      break;
    }
    // Ignore Carriage Return
    if (Buffer == '\r') {
      continue;
    }
    // End of line
    if (Buffer == '\n') {
      break;
    }
    // We need to have one CHAR8 reserved for the end of string '\0'
    if (Index + 1 >= LineSize) {
      Rc = EFI_BUFFER_TOO_SMALL;
      goto Finish;
    }
    pLine[Index] = Buffer;
    Index++;
  }

  if (EFI_ERROR(Rc)) {
    NVDIMM_DBG("Error reading the file: " FORMAT_EFI_STATUS "", Rc);
  }

  pLine[Index] = '\0';

Finish:
  NVDIMM_EXIT_I64(Rc);
  return Rc;
}

/**
  The Print function is not able to print long strings.
  This function is dividing the input string to safe lengths
  and prints all of the parts.

  @param[in] pString - the Unicode string to be printed
**/
VOID
LongPrint(
  IN     CHAR16 *pString
  )
{
  UINT32 StrOffset = 0;
  UINT32 MaxToPrint = PCD_UEFI_LIB_MAX_PRINT_BUFFER_SIZE;
  CHAR16 TempChar = L'\0';

  if (pString == NULL) {
    return;
  }

  while (pString[0] != L'\0') {
    while (pString[StrOffset] != L'\0' && StrOffset < MaxToPrint) {
      StrOffset++;
    }
    if (StrOffset == MaxToPrint) {
      TempChar = pString[StrOffset]; // Remember it and put a NULL there
      pString[StrOffset] = L'\0';
      Print(FORMAT_STR, pString); // Print the string up to the newline
      pString[StrOffset] = TempChar;// Put back the stored value.
      pString += StrOffset; // Move the pointer over the printed part and the '\n'
      StrOffset = 0;
    } else { // There is a NULL after the newline or there is just a NULL
      Print(FORMAT_STR, pString);
      break;
    }
  }
}


/**
Get basic information about the host server

@param[out] pHostServerInfo pointer to a HOST_SERVER_INFO struct

@retval EFI_SUCCESS Success
@retval EFI_INVALID_PARAMETER One or more parameters are invalid
**/
EFI_STATUS
GetHostServerInfo(
   OUT HOST_SERVER_INFO *pHostServerInfo
)
{
   EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
   NVDIMM_ENTRY();
#ifdef OS_BUILD
   char name[HOST_SERVER_NAME_LEN];
   char osName[HOST_SERVER_OS_NAME_LEN];
   char osVersion[HOST_SERVER_OS_VERSION_LEN];
   if (NULL == pHostServerInfo)
   {
      goto Finish;
   }
   if (0 != os_get_host_name(name, HOST_SERVER_NAME_LEN))
   {
      goto Finish;
   }
   else
   {
      AsciiStrToUnicodeStrS(name, pHostServerInfo->Name, HOST_SERVER_NAME_LEN);
   }

   if (0 != os_get_os_name(osName, HOST_SERVER_OS_NAME_LEN))
   {
      goto Finish;
   }
   else
   {
      AsciiStrToUnicodeStrS(osName, pHostServerInfo->OsName, HOST_SERVER_OS_NAME_LEN);
   }

   if (0 != os_get_os_version(osVersion, HOST_SERVER_OS_VERSION_LEN))
   {
      goto Finish;
   }
   else
   {
      AsciiStrToUnicodeStrS(osVersion, pHostServerInfo->OsVersion, HOST_SERVER_OS_VERSION_LEN);
   }
   ReturnCode = EFI_SUCCESS;
#else
   //2nd arg is size of the destination buffer in bytes so sizeof is appropriate
   UnicodeSPrint(pHostServerInfo->OsName, sizeof(pHostServerInfo->OsName), L"UEFI");
   UnicodeSPrint(pHostServerInfo->Name, sizeof(pHostServerInfo->Name), L"N/A");
   UnicodeSPrint(pHostServerInfo->OsVersion, sizeof(pHostServerInfo->OsVersion), L"N/A");
   ReturnCode = EFI_SUCCESS;
   goto Finish;
#endif

Finish:
   NVDIMM_EXIT_I64(ReturnCode);
   return ReturnCode;
}


/**
  Retrieves Intel Dimm Config EFI vars

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




#ifndef OS_BUILD

#define EFI_ACPI_16550_UART_HID EISA_PNP_ID(0x0501)

extern EFI_GUID gEfiSerialIoProtocolGuid;
/**
  Check whether the device path node is ISA Serial Node.
  @param[in] Acpi           Device path node to be checked
  @retval TRUE          It is ISA Serial Node.
  @retval FALSE         It is NOT ISA Serial Node.
**/
BOOLEAN
IsISASerialNode(
  IN ACPI_HID_DEVICE_PATH *Acpi
)
{
  return (BOOLEAN)(
    (DevicePathType(Acpi) == ACPI_DEVICE_PATH) &&
    (DevicePathSubType(Acpi) == ACPI_DP) &&
    (ReadUnaligned32(&Acpi->HID) == EFI_ACPI_16550_UART_HID)
    );
}

/**
  The initialization routine in DebugLib initializes the
  serial port to a static value defined in module dec file.
  This function find's out the serial port attributes
  from SerialIO protocol and set it on serial port

  @retval EFI_SUCCESS The function complete successfully.
  @retval EFI_UNSUPPORTED No serial ports present.

**/

EFI_STATUS
SetSerialAttributes(
  VOID
)
{
  UINTN                     Index;

  UINTN                     NoHandles;
  EFI_HANDLE                *Handles;
  EFI_STATUS                ReturnCode;
  ACPI_HID_DEVICE_PATH      *Acpi;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_SERIAL_IO_PROTOCOL    *SerialIo;
  EFI_DEVICE_PATH_PROTOCOL  *Node;

  ReturnCode = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiSerialIoProtocolGuid,
    NULL,
    &NoHandles,
    &Handles
  );
  CHECK_RETURN_CODE(ReturnCode,Finish);
  for (Index = 0; Index < NoHandles; Index++) {
    // Check to see whether the handle has DevicePath Protocol installed
    ReturnCode = gBS->HandleProtocol(
      Handles[Index],
      &gEfiDevicePathProtocolGuid,
      (VOID **)&DevicePath
    );
    CHECK_RETURN_CODE(ReturnCode,Finish);
    Acpi = NULL;
    for (Node = DevicePath; !IsDevicePathEnd(Node); Node = NextDevicePathNode(Node)) {
      if ((DevicePathType(Node) == MESSAGING_DEVICE_PATH) && (DevicePathSubType(Node) == MSG_UART_DP)) {
        break;
      }
      // Acpi points to the node before Uart node
      Acpi = (ACPI_HID_DEVICE_PATH *)Node;
    }
    if ((Acpi != NULL) && IsISASerialNode(Acpi)) {
      ReturnCode = gBS->HandleProtocol(
        Handles[Index],
        &gEfiSerialIoProtocolGuid,
        (VOID **)&SerialIo
      );
      CHECK_RETURN_CODE(ReturnCode,Finish);
      EFI_PARITY_TYPE    Parity = (EFI_PARITY_TYPE)SerialIo->Mode->Parity;
      UINT8              DataBits = (UINT8)SerialIo->Mode->DataBits;
      EFI_STOP_BITS_TYPE StopBits = (EFI_STOP_BITS_TYPE)(SerialIo->Mode->StopBits);
      ReturnCode = SerialPortSetAttributes(
        &(SerialIo->Mode->BaudRate),
        &(SerialIo->Mode->ReceiveFifoDepth),
        &(SerialIo->Mode->Timeout),
        &Parity, &DataBits, &StopBits);
      CHECK_RETURN_CODE(ReturnCode,Finish);
      break;
    }
  }
Finish:
  FREE_POOL_SAFE(Handles);
  return ReturnCode;
}
#endif



#if defined(DYNAMIC_WA_ENABLE)
/**
  Local define of the Shell Protocol GetEnv function. The local definition allows the driver to use
  this function without including the Shell headers.
**/
typedef
CONST CHAR16 *
(EFIAPI *EFI_SHELL_GET_ENV_LOCAL) (
  IN CONST CHAR16 *Name OPTIONAL
);

/**
  Local, partial definition of the Shell Protocol, the first Reserved value is a pointer to a different function,
  but we don't need it here so it is masked as reserved. We ignore any functions after the GetEnv one.
**/
typedef struct {
  VOID *Reserved;
  EFI_SHELL_GET_ENV_LOCAL GetEnv;
} EFI_SHELL_PROTOCOL_GET_ENV;

/**
  Returns the value of the environment variable with the given name.

  @param[in] pVarName Unicode name of the variable to retrieve

  @retval NULL if the shell protocol could not be located or if the variable is not defined in the system
  @retval pointer to the Unicode string containing the variable value
**/
CHAR16 *
GetEnvVariable(
  IN     CHAR16 *pVarName
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pReturn = NULL;
  EFI_SHELL_PROTOCOL_GET_ENV *pGetEnvShell = NULL;
  EFI_GUID GetEnvShellProtGuid = EFI_SHELL_PROTOCOL_GUID;

  ReturnCode = gBS->LocateProtocol(&GetEnvShellProtGuid, NULL, (VOID **)&pGetEnvShell);
  if (!EFI_ERROR(ReturnCode)) {
    pReturn = (CHAR16 *)pGetEnvShell->GetEnv(pVarName);
  }

  return pReturn;
}
#endif



//Its ok to keep these routines here, but they should be calling abstracted serialize/deserialize data APIs in the future.
extern EFI_GUID gIntelDimmPbrTagIdVariableguid;

#ifndef OS_BUILD
EFI_STATUS PbrDcpmmSerializeTagId(
  UINT32 Id
)
{
  UINTN VariableSize;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  VariableSize = sizeof(UINT32);
  ReturnCode = SET_VARIABLE(
    PBR_TAG_ID_VAR,
    gIntelDimmPbrTagIdVariableguid,
    VariableSize,
    (VOID*)&Id);
  return ReturnCode;
}

EFI_STATUS PbrDcpmmDeserializeTagId(
  UINT32 *pId,
  UINT32 DefaultId
)
{
  UINTN VariableSize;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 Id = 0;

  VariableSize = sizeof(UINT32);
  ReturnCode = GET_VARIABLE(
    PBR_TAG_ID_VAR,
    gIntelDimmPbrTagIdVariableguid,
    &VariableSize,
    &Id);

  if (ReturnCode == EFI_NOT_FOUND) {
    *pId = DefaultId;
    ReturnCode = EFI_SUCCESS;
    goto Finish;
  }
  else if (EFI_ERROR(ReturnCode)) {
    NVDIMM_DBG("Failed to retrieve PBR TAG ID value");
    goto Finish;
  }
  *pId = Id;
Finish:
  return ReturnCode;
}
#else
#include <PbrDcpmm.h>
#ifdef _MSC_VER
extern int registry_volatile_write(const char *key, unsigned int dword_val);
extern int registry_read(const char *key, unsigned int *dword_val, unsigned int default_val);
#else
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
/**
  Helper that serializes pbr id to a volatile store.  We should not be maintaining
  sessions across system reboots
**/
EFI_STATUS PbrDcpmmSerializeTagId(
  UINT32 id
)
{
  EFI_STATUS ReturnCode = EFI_LOAD_ERROR;
#if _MSC_VER
  registry_volatile_write("pbr_id", id);
#else
  UINT32 ShmId;
  key_t Key;
  UINT32 *pPbrId = NULL;
  Key = ftok(PBR_TMP_DIR, 'i');
  ShmId = shmget(Key, sizeof(*pPbrId), IPC_CREAT | 0666);
  if (-1 == ShmId) {
    NVDIMM_DBG("Failed to shmget\n");
    return ReturnCode;
  }
  pPbrId = (UINT32*)shmat(ShmId, NULL, 0);
  if ((VOID*)pPbrId == (VOID*)-1) {
    NVDIMM_DBG("Failed to shmat\n");
    return ReturnCode;
  }
  else
  {
    *pPbrId = id;
    NVDIMM_DBG("Writing to shared memory: %d\n", *pPbrId);
    shmdt(pPbrId);
    //If id is reset to zero it is ok to mark
    //this share memory to be removed
    if (0 == id)
    {
      shmctl(ShmId, IPC_RMID, NULL);
    }
    ReturnCode = EFI_SUCCESS;
  }
#endif
  return ReturnCode;
}

/**
  Helper that deserializes pbr tag id from a volatile store.  We should not be maintaining
  sessions across system reboots
**/
EFI_STATUS PbrDcpmmDeserializeTagId(
  UINT32 *id,
  UINT32 defaultId
)
{
  EFI_STATUS ReturnCode = EFI_LOAD_ERROR;
#if _MSC_VER
  registry_read("pbr_id", id, defaultId);
#else
  UINT32 ShmId;
  key_t Key;
  UINT32 *pPbrId = NULL;
  Key = ftok(PBR_TMP_DIR, 'i');
  ShmId = shmget(Key, sizeof(*pPbrId), IPC_CREAT | 0666);
  if (-1 == ShmId) {
    NVDIMM_DBG("Failed to shmget\n");
    return ReturnCode;
  }
  ReturnCode = EFI_SUCCESS;
  pPbrId = (UINT32*)shmat(ShmId, NULL, 0);
  if ((VOID*)pPbrId == (VOID*)-1) {
    NVDIMM_DBG("Failed to shmat\n");
    *id = defaultId;
  }
  else
  {
    *id = *pPbrId;
    shmdt(pPbrId);
    //If id is reset to zero it is ok to mark
    //this share memory to be removed
    if (0 == *id)
    {
      shmctl(ShmId, IPC_RMID, NULL);
    }
  }
#endif
  return ReturnCode;
}

#endif




/**
  Function that allows for nicely formatted HEX & ASCII debug output.
  It can be used to inspect memory objects without a need for debugger

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/

#define COLUMN_IN_HEX_DUMP   16

VOID
HexDebugPrint(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
  )
{
  UINT8 Byte, AsciiBuffer[COLUMN_IN_HEX_DUMP];
  UINT16 Column, NextColumn, Index, Index2;
  UINT8 *pData;

  if (pBuffer == NULL) {
    NVDIMM_DBG("pBuffer is NULL");
    return;
  }
  DebugPrint(EFI_D_INFO, "Hexdump starting at: 0x%p\n", pBuffer);
  pData = (UINT8 *) pBuffer;
  for (Index = 0; Index < Bytes; Index++) {
    Column = Index % COLUMN_IN_HEX_DUMP;
    NextColumn = (Index + 1) % COLUMN_IN_HEX_DUMP;
    Byte = *(pData + Index);
    if (Column == 0) {
      DebugPrint(EFI_D_INFO, "%.3d:", Index);
    }
    if (Index % 8 == 0) {
      DebugPrint(EFI_D_INFO, " ");
    }
    DebugPrint(EFI_D_INFO, "%.2x", *(pData + Index));
    AsciiBuffer[Column] = IsAsciiAlnumCharacter(Byte) ? Byte : '.';
    if (NextColumn == 0 && Index != 0) {
      DebugPrint(EFI_D_INFO, " ");
      for (Index2 = 0; Index2 < COLUMN_IN_HEX_DUMP; Index2++) {
        DebugPrint(EFI_D_INFO, "%c", AsciiBuffer[Index2]);
        if (Index2 == COLUMN_IN_HEX_DUMP / 2 - 1) {
          DebugPrint(EFI_D_INFO, " ");
        }
      }
      DebugPrint(EFI_D_INFO, "\n");
    }
  }
}



/**
  Function that allows for nicely formatted HEX & ASCII console output.
  It can be used to inspect memory objects without a need for debugger or dumping raw DIMM data.

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/
VOID
HexPrint(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
  )
{
  UINT8 Byte, AsciiBuffer[COLUMN_IN_HEX_DUMP];
  UINT16 Column, NextColumn, Index, Index2;
  UINT8 *pData;

  if (pBuffer == NULL) {
    NVDIMM_DBG("pBuffer is NULL");
    return;
  }
  Print(L"Hexdump for %d bytes:\n", Bytes);
  pData = (UINT8 *)pBuffer;
  for (Index = 0; Index < Bytes; Index++) {
    Column = Index % COLUMN_IN_HEX_DUMP;
    NextColumn = (Index + 1) % COLUMN_IN_HEX_DUMP;
    Byte = *(pData + Index);
    if (Column == 0) {
      Print(L"%.3d:", Index);
    }
    if (Index % 8 == 0) {
      Print(L" ");
    }
    Print(L"%.2x", *(pData + Index));
    AsciiBuffer[Column] = IsAsciiAlnumCharacter(Byte) ? Byte : '.';
    if (NextColumn == 0 && Index != 0) {
      Print(L" ");
      for (Index2 = 0; Index2 < COLUMN_IN_HEX_DUMP; Index2++) {
        Print(L"%c", AsciiBuffer[Index2]);
        if (Index2 == COLUMN_IN_HEX_DUMP / 2 - 1) {
          Print(L" ");
        }
      }
      Print(L"\n");
    }
  }
}
