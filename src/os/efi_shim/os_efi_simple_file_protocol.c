/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <Uefi.h>
#include <Guid/FileInfo.h>
#include <stdlib.h>
#include <errno.h>
#ifdef _MSC_VER
#include <io.h>
#include <conio.h>
#else
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#include <sys/types.h>
#include <safe_str_lib.h>
#define _open open
#define _close close
#define _lseek lseek
#define _write write
#define _read read
#define _getch getchar
#define FILE_MODE 666
#endif

#include <sys/stat.h> 
#include <fcntl.h>
#include "os_efi_simple_file_protocol.h"

EFI_FILE_PROTOCOL gFileProtocol;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL gSimpleFileProtocol;

EFI_STATUS
get_last_error()
{
	switch (errno)
	{
	case EACCES:
		return EFI_ACCESS_DENIED;
	case EEXIST:
		return EFI_INVALID_PARAMETER;
	case EINVAL:
		return EFI_INVALID_PARAMETER;
	case ENOENT:
		return EFI_NOT_FOUND;
	default:
		return EFI_LOAD_ERROR;
	}
}

EFI_STATUS
volume_open(
	IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *This,
	OUT EFI_FILE_PROTOCOL                 **Root
)
{
	*Root = &gFileProtocol;
	return EFI_SUCCESS;
}

#define MAX_FILE_NAME_SIZE 2048
#define MAX_W_FILE_NAME_SIZE	4096

typedef struct _FILE_CONTEXT {
	int fd;
	char filename_ascii[MAX_FILE_NAME_SIZE];
	CHAR16 filename[MAX_W_FILE_NAME_SIZE];
}FILE_CONTEXT;

EFI_STATUS
file_open(
	IN EFI_FILE_PROTOCOL        *This,
	OUT EFI_FILE_PROTOCOL       **NewHandle,
	IN CHAR16                   *FileName,
	IN UINT64                   OpenMode,
	IN UINT64                   Attributes
)
{
	FILE_CONTEXT *pFc;
	EFI_FILE_PROTOCOL *pFp;
	int flags = 0;

	pFp = AllocateZeroPool(sizeof(EFI_FILE_PROTOCOL));
	if (NULL == pFp)
	{
		return EFI_OUT_OF_RESOURCES;
	}

	*pFp = gFileProtocol;
	pFp->Revision = (UINT64)AllocateZeroPool(sizeof(FILE_CONTEXT));
	if (0 == pFp->Revision)
	{
		FreePool(pFp);
		return EFI_OUT_OF_RESOURCES;
	}

	pFc = (FILE_CONTEXT*)(pFp->Revision);
	wcstombs(pFc->filename_ascii, FileName, MAX_FILE_NAME_SIZE);
	swprintf_s(pFc->filename, MAX_W_FILE_NAME_SIZE, FORMAT_STR, FileName);

	if ((OpenMode & EFI_FILE_MODE_READ) &&
		(OpenMode & EFI_FILE_MODE_WRITE))
	{
		flags |= O_RDWR;
	}
	else if (OpenMode & EFI_FILE_MODE_READ)
	{
		flags |= O_RDONLY;
	}
	else if (OpenMode & EFI_FILE_MODE_WRITE)
	{
		flags |= O_WRONLY;
	}

	if (OpenMode & EFI_FILE_MODE_CREATE)
	{
		flags |= O_CREAT;
	}

#ifndef _MSC_VER
	pFc->fd = _open(pFc->filename_ascii, flags, FILE_MODE);
#else
  pFc->fd = _open(pFc->filename_ascii, flags);
#endif

	if (-1 == pFc->fd)
	{
    FreePool(pFp);
		return get_last_error();
	}
	*NewHandle = pFp;
	return EFI_SUCCESS;
}

EFI_STATUS
file_close(
	IN EFI_FILE_PROTOCOL  *This
)
{
	FILE_CONTEXT *pFc;

	if (This != &gFileProtocol)
	{
		pFc = (FILE_CONTEXT *)This->Revision;
		if (NULL != pFc && 0 != pFc->fd)
		{
			_close(pFc->fd);
			pFc->fd = 0;
		}

		if (NULL != pFc)
		{
			FreePool(pFc);
			This->Revision = 0x0;
		}
	}
	return EFI_SUCCESS;
}

EFI_STATUS
file_get_info(
	IN EFI_FILE_PROTOCOL        *This,
	IN EFI_GUID                 *InformationType,
	IN OUT UINTN                *BufferSize,
	OUT VOID                    *Buffer
)
{
	FILE_CONTEXT *pFc = (FILE_CONTEXT*)This->Revision;
	EFI_FILE_INFO *pFi = (EFI_FILE_INFO *)Buffer;
	UINT64 size = (sizeof(EFI_FILE_INFO) + StrSize(pFc->filename));

	if (NULL == Buffer || size > *BufferSize)
	{
		*BufferSize = sizeof(EFI_FILE_INFO) + StrSize(pFc->filename);
		return EFI_BUFFER_TOO_SMALL;
	}
	else
	{
		pFi->Size = size;
		struct stat buf;
		fstat(pFc->fd, &buf);
		pFi->FileSize = buf.st_size;
		pFi->PhysicalSize = pFi->FileSize;
		return EFI_SUCCESS;
	}
}

EFI_STATUS
file_read(
	IN EFI_FILE_PROTOCOL        *This,
	IN OUT UINTN                *BufferSize,
	OUT VOID                    *Buffer
)
{
	FILE_CONTEXT *pFc = (FILE_CONTEXT*)This->Revision;
	if (NULL == pFc)
	{
		return EFI_OUT_OF_RESOURCES;
	}

	if (-1 == _read(pFc->fd, Buffer, (unsigned int) *BufferSize))
	{
		return EFI_DEVICE_ERROR;
	}
	return EFI_SUCCESS;
}

EFI_STATUS
file_write(
	IN EFI_FILE_PROTOCOL        *This,
	IN OUT UINTN                *BufferSize,
	IN VOID                     *Buffer
)
{
	FILE_CONTEXT *pFc = (FILE_CONTEXT*)This->Revision;
	if (NULL == pFc)
	{
		return EFI_OUT_OF_RESOURCES;
	}

	if (-1 == _write(pFc->fd, Buffer, (unsigned int) *BufferSize))
	{
		return EFI_DEVICE_ERROR;
	}
	return EFI_SUCCESS;
}

EFI_STATUS
file_get_pos(
	IN EFI_FILE_PROTOCOL        *This,
	OUT UINT64                  *Position
)
{
	FILE_CONTEXT *pFc = (FILE_CONTEXT*)This->Revision;
	if (NULL == pFc)
	{
		return EFI_OUT_OF_RESOURCES;
	}

	*Position = _lseek(pFc->fd, 0, SEEK_CUR);
	return EFI_SUCCESS;
}

EFI_STATUS
file_set_pos(
	IN EFI_FILE_PROTOCOL        *This,
	IN UINT64                   Position
)
{
	FILE_CONTEXT *pFc = (FILE_CONTEXT*)This->Revision;
	if (NULL == pFc)
	{
		return EFI_OUT_OF_RESOURCES;
	}

	_lseek(pFc->fd, (long) Position, SEEK_SET);
	return EFI_SUCCESS;
}

int init_protocol_simple_file_system_protocol()
{
	gSimpleFileProtocol.OpenVolume = volume_open;
	gFileProtocol.Open = file_open;
	gFileProtocol.Close = file_close;
	gFileProtocol.GetInfo = file_get_info;
	gFileProtocol.Read = file_read;
	gFileProtocol.Write = file_write;
	gFileProtocol.GetPosition = file_get_pos;
	gFileProtocol.SetPosition = file_set_pos;
	return 0;
}
