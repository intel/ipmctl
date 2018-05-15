/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <Uefi.h>
#include <BaseMemoryLib.h>
#include <Guid/FileInfo.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <io.h>
#include <conio.h>
#else
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#define _read read
#define _getch getchar
#endif
#include <sys/stat.h> 
#include <fcntl.h>
#include "os.h"
#include "os_efi_bs_protocol.h"
#include "os_efi_simple_file_protocol.h"

EFI_BOOT_SERVICES gOsBootServices;
EFI_BOOT_SERVICES *gBS = &gOsBootServices;

//below externs used by BsLocateHandleBuffer & BsOpenProtocol
extern EFI_NVMDIMM_CONFIG_PROTOCOL gNvmDimmDriverNvmDimmConfig;
extern EFI_GUID gNvmDimmConfigProtocolGuid;
extern EFI_GUID  gEfiSimpleFileSystemProtocolGuid;


typedef struct _TIMER_EVENT_CONTEXT {
	void * notify_context;
	EFI_TIMER_DELAY timer_type;
	UINT64 timeout_sec;
	INT64 timeout_sec_remaining;
}TIMER_EVENT_CONTEXT;



#define PROTOCOL_HANDLE_NVDIMM_CONFIG 0x1
#define PROTOCOL_HANDLE_FILE_IO 0x1

EFI_STATUS BsLocateHandleBuffer(
	IN     EFI_LOCATE_SEARCH_TYPE       SearchType,
	IN     EFI_GUID                     *Protocol, OPTIONAL
	IN     VOID                         *SearchKey, OPTIONAL
	IN OUT UINTN                        *NoHandles,
	OUT    EFI_HANDLE                   **Buffer
)
{
	if (CompareGuid(Protocol, &gNvmDimmConfigProtocolGuid))
	{
		*NoHandles = 1;
		*Buffer = AllocatePool(sizeof(UINTN));
        if (NULL == *Buffer) {
            return EFI_OUT_OF_RESOURCES;
        }
		**Buffer = (EFI_HANDLE)PROTOCOL_HANDLE_NVDIMM_CONFIG;
		return EFI_SUCCESS;
	}
	else if (CompareGuid(Protocol, &gEfiSimpleFileSystemProtocolGuid))
	{
		*NoHandles = 1;
		*Buffer = AllocatePool(sizeof(UINTN));
        if (NULL == *Buffer) {
            return EFI_OUT_OF_RESOURCES;
        }
        **Buffer = (EFI_HANDLE)PROTOCOL_HANDLE_FILE_IO;
		return EFI_SUCCESS;
	}
	return EFI_PROTOCOL_ERROR;
}

EFI_STATUS BsOpenProtocol(
	IN  EFI_HANDLE                Handle,
	IN  EFI_GUID                  *Protocol,
	OUT VOID                      **Interface, OPTIONAL
	IN  EFI_HANDLE                AgentHandle,
	IN  EFI_HANDLE                ControllerHandle,
	IN  UINT32                    Attributes
)
{
	if (CompareGuid(Protocol, &gNvmDimmConfigProtocolGuid))
	{
		*Interface = &gNvmDimmDriverNvmDimmConfig;
	}
	else if (CompareGuid(Protocol, &gEfiSimpleFileSystemProtocolGuid))
	{
		*Interface = &gSimpleFileProtocol;
	}
	else
	{
		return EFI_PROTOCOL_ERROR;
	}
	return EFI_SUCCESS;
}

EFI_STATUS
create_event(
	IN  UINT32                       Type,
	IN  EFI_TPL                      NotifyTpl,
	IN  EFI_EVENT_NOTIFY             NotifyFunction,
	IN  VOID                         *NotifyContext,
	OUT EFI_EVENT                    *Event
)
{
	if (EVT_TIMER == Type)
	{
		TIMER_EVENT_CONTEXT * pEc = (TIMER_EVENT_CONTEXT *)AllocatePool(sizeof(TIMER_EVENT_CONTEXT));
        if (NULL == pEc) {
            return EFI_OUT_OF_RESOURCES;
        }
		pEc->notify_context = NotifyContext;
		pEc->timer_type = Type;
		pEc->timeout_sec = 0;
		pEc->timeout_sec_remaining = 0;
		*Event = (EFI_EVENT)pEc;
		return EFI_SUCCESS;
	}
	else
	{
		return EFI_UNSUPPORTED;
	}
}

EFI_STATUS
set_timer(
	IN  EFI_EVENT                Event,
	IN  EFI_TIMER_DELAY          Type,
	IN  UINT64                   TriggerTime
)
{
	TIMER_EVENT_CONTEXT * pEc = (TIMER_EVENT_CONTEXT *)Event;

	if (NULL == pEc)
	{
		return EFI_INVALID_PARAMETER;
	}
	pEc->timer_type = Type;
	pEc->timeout_sec = pEc->timeout_sec_remaining = TriggerTime/ 10000000; //for now WA for UEFI/OS differences
	if (0 == pEc->timeout_sec)
		pEc->timeout_sec = pEc->timeout_sec_remaining = 1;
	return EFI_SUCCESS;
}

EFI_STATUS
wait_for_event(
	IN  UINTN                    NumberOfEvents,
	IN  EFI_EVENT                *Event,
	OUT UINTN                    *Index
)
{
	UINTN index;
	TIMER_EVENT_CONTEXT ** pEc = (TIMER_EVENT_CONTEXT **)Event;
	UINTN index_that_timed_out = 0;
	BOOLEAN index_timed_out = FALSE;
	INT64 lowest_timeout_val = pEc[0]->timeout_sec_remaining;

	for (index = 0; index < NumberOfEvents; ++index)
	{
		if (pEc[index]->timeout_sec_remaining < lowest_timeout_val)
			lowest_timeout_val = pEc[index]->timeout_sec_remaining;
		if (0 >= pEc[index]->timeout_sec_remaining)
		{
			index_that_timed_out = index;
			goto done;
		}
	}

	wait_for_sec((unsigned int)lowest_timeout_val);
	for (index = 0; index < NumberOfEvents; ++index)
	{
		pEc[index]->timeout_sec_remaining -= lowest_timeout_val;
		if (0 >= pEc[index]->timeout_sec_remaining && FALSE == index_timed_out)
		{
			index_that_timed_out = index;
			index_timed_out = TRUE;
		}
	}
done:
	*Index = index_that_timed_out;
	if (TimerPeriodic == pEc[index_that_timed_out]->timer_type)
	{
		pEc[index_that_timed_out]->timeout_sec_remaining = pEc[index_that_timed_out]->timeout_sec;
	}
	return EFI_SUCCESS;
}

EFI_STATUS
close_event(
	IN EFI_EVENT                Event
)
{
	FreePool(Event);
	return EFI_SUCCESS;
}

int init_protocol_bs()
{
	gOsBootServices.LocateHandleBuffer = BsLocateHandleBuffer;
	gOsBootServices.OpenProtocol = BsOpenProtocol;
	gOsBootServices.CloseEvent = close_event;
	gOsBootServices.CreateEvent = create_event;
	gOsBootServices.WaitForEvent = wait_for_event;
	gOsBootServices.SetTimer = set_timer;
	return 0;
}
