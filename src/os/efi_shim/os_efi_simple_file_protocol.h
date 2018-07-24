/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _OS_SIMPLE_FILE_PROTOCOL_H_
#define _OS_SIMPLE_FILE_PROTOCOL_H_

#include <Types.h>
#include <NvmTypes.h>
#include <Uefi.h>
#include <Debug.h>

extern EFI_SIMPLE_FILE_SYSTEM_PROTOCOL gSimpleFileProtocol;

int init_protocol_simple_file_system_protocol();

EFI_STATUS
volume_open(
	IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *This,
	OUT EFI_FILE_PROTOCOL                 **Root
);

EFI_STATUS
file_open(
	IN EFI_FILE_PROTOCOL        *This,
	OUT EFI_FILE_PROTOCOL       **NewHandle,
	IN CHAR16                   *FileName,
	IN UINT64                   OpenMode,
	IN UINT64                   Attributes
);

EFI_STATUS
file_close(
	IN EFI_FILE_PROTOCOL  *This
);

EFI_STATUS
file_get_info(
	IN EFI_FILE_PROTOCOL        *This,
	IN EFI_GUID                 *InformationType,
	IN OUT UINTN                *BufferSize,
	OUT VOID                    *Buffer
);

EFI_STATUS
file_read(
	IN EFI_FILE_PROTOCOL        *This,
	IN OUT UINTN                *BufferSize,
	OUT VOID                    *Buffer
);

EFI_STATUS
file_write(
	IN EFI_FILE_PROTOCOL        *This,
	IN OUT UINTN                *BufferSize,
	IN VOID                     *Buffer
);

EFI_STATUS
file_get_pos(
	IN EFI_FILE_PROTOCOL        *This,
	OUT UINT64                  *Position
);

EFI_STATUS
file_set_pos(
	IN EFI_FILE_PROTOCOL        *This,
	IN UINT64                   Position
);

#endif
