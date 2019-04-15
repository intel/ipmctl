/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _AUTOGENH_55e38c3b_e667_44e5_9ad3_6c92dce2a03a
#define _AUTOGENH_55e38c3b_e667_44e5_9ad3_6c92dce2a03a

#ifdef __cplusplus
extern "C" {
#endif

#include <Base.h>
#include <Uefi.h>
#include <Library/PcdLib.h>

extern GUID  gEfiCallerIdGuid;
extern CHAR8 *gEfiCallerBaseName;

#define EFI_CALLER_ID_GUID \
  {0x55e38c3b, 0xe667, 0x44e5, {0x9a, 0xd3, 0x6c, 0x92, 0xdc, 0xe2, 0xa0, 0x3a}}

// Not sure why I can't put this in Autogen.c
// Windows at minimum doesn't like it
#define _PCD_TOKEN_PcdDcpmmCliDefaultCapacityUnit  1U
#define _PCD_SIZE_PcdDcpmmCliDefaultCapacityUnit 4
#define _PCD_GET_MODE_SIZE_PcdDcpmmCliDefaultCapacityUnit  _PCD_SIZE_PcdDcpmmCliDefaultCapacityUnit
#define _PCD_VALUE_PcdDcpmmCliDefaultCapacityUnit  0U

#define _PCD_TOKEN_PcdDcpmmHiiDefaultCapacityUnit  1U
#define _PCD_SIZE_PcdDcpmmHiiDefaultCapacityUnit 4
#define _PCD_GET_MODE_SIZE_PcdDcpmmHiiDefaultCapacityUnit  _PCD_SIZE_PcdDcpmmHiiDefaultCapacityUnit
#define _PCD_VALUE_PcdDcpmmHiiDefaultCapacityUnit  0U

// Defined in Autogen.c
// These are the values exposed by PcdLib.h
extern const  BOOLEAN  _gPcd_FixedAtBuild_PcdVerifyNodeInList;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumLinkedListLength;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumAsciiStringLength;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumUnicodeStringLength;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumDevicePathNodeCount;
#define _PCD_GET_MODE_BOOL_PcdVerifyNodeInList  _gPcd_FixedAtBuild_PcdVerifyNodeInList
#define _PCD_GET_MODE_32_PcdMaximumLinkedListLength  _gPcd_FixedAtBuild_PcdMaximumLinkedListLength
#define _PCD_GET_MODE_32_PcdMaximumAsciiStringLength  _gPcd_FixedAtBuild_PcdMaximumAsciiStringLength
#define _PCD_GET_MODE_32_PcdMaximumUnicodeStringLength  _gPcd_FixedAtBuild_PcdMaximumUnicodeStringLength
#define _PCD_GET_MODE_32_PcdMaximumDevicePathNodeCount  _gPcd_FixedAtBuild_PcdMaximumDevicePathNodeCount


// Definition of PCDs used in this module
#define _PCD_PATCHABLE_VALUE_PcdPciExpressBaseAddress  ((UINT64)0x80000000ULL)
extern UINT64 _gPcd_BinaryPatch_PcdPciExpressBaseAddress;


EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );



#include "os_efi_hii_auto_gen_defs.h"


#ifdef __cplusplus
}
#endif

#endif
