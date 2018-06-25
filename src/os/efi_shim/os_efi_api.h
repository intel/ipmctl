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

/**
Makes Bios emulated pass thru call and returns the values

@param[in]  pDimm    pointer to current Dimm
@param[out] pBsrValue   Value from passthru

@retval EFI_SUCCESS  The count was returned properly
@retval EFI_INVALID_PARAMETER One or more parameters are NULL
@retval Other errors failure of FW commands
**/
EFI_STATUS
EFIAPI
FwCmdGetBsr(DIMM *pDimm, UINT64 *pBsrValue);
#endif //OS_EFI_API_H_
