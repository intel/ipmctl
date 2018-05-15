/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OS_EFI_API_H_
#define	OS_EFI_API_H_

#include <Uefi.h>

VOID
EFIAPI
GetVendorDriverVersion(CHAR16 * pVersion, UINTN VersionStrSize);


#endif //OS_EFI_API_H_
