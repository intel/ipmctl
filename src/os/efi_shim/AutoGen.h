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

// Definition of PCDs used in this module

#define _PCD_TOKEN_PcdDcpmmCliDefaultCapacityUnit  1U
#define _PCD_VALUE_PcdDcpmmCliDefaultCapacityUnit  0U
extern const  UINT32  _gPcd_FixedAtBuild_PcdDcpmmCliDefaultCapacityUnit;
#define _PCD_GET_MODE_32_PcdDcpmmCliDefaultCapacityUnit  _gPcd_FixedAtBuild_PcdDcpmmCliDefaultCapacityUnit
//#define _PCD_SET_MODE_32_PcdDcpmmCliDefaultCapacityUnit  ASSERT(FALSE)  // It is not allowed to set value for a FIXED_AT_BUILD PCD

#define _PCD_TOKEN_PcdShellSupportLevel  2U
#define _PCD_VALUE_PcdShellSupportLevel  3U
extern const  UINT8  _gPcd_FixedAtBuild_PcdShellSupportLevel;
#define _PCD_GET_MODE_8_PcdShellSupportLevel  _gPcd_FixedAtBuild_PcdShellSupportLevel

#define _PCD_TOKEN_PcdVerifyNodeInList  6U
#define _PCD_VALUE_PcdVerifyNodeInList  ((BOOLEAN)0U)
//GLOBAL_REMOVE_IF_UNREFERENCED const BOOLEAN _gPcd_FixedAtBuild_PcdVerifyNodeInList = _PCD_VALUE_PcdVerifyNodeInList;
extern const  BOOLEAN  _gPcd_FixedAtBuild_PcdVerifyNodeInList;
#define _PCD_GET_MODE_BOOL_PcdVerifyNodeInList  _gPcd_FixedAtBuild_PcdVerifyNodeInList

#define _PCD_TOKEN_PcdMaximumLinkedListLength  7U
#define _PCD_VALUE_PcdMaximumLinkedListLength  1000000U
//GLOBAL_REMOVE_IF_UNREFERENCED const UINT32 _gPcd_FixedAtBuild_PcdMaximumLinkedListLength = _PCD_VALUE_PcdMaximumLinkedListLength;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumLinkedListLength;
#define _PCD_GET_MODE_32_PcdMaximumLinkedListLength  _gPcd_FixedAtBuild_PcdMaximumLinkedListLength
//#define _PCD_SET_MODE_8_PcdShellSupportLevel  ASSERT(FALSE)  // It is not allowed to set value for a FIXED_AT_BUILD PCD

#define _PCD_TOKEN_PcdMaximumDevicePathNodeCount  10U
#define _PCD_VALUE_PcdMaximumDevicePathNodeCount  0U
//GLOBAL_REMOVE_IF_UNREFERENCED const UINT32 _gPcd_FixedAtBuild_PcdMaximumDevicePathNodeCount = _PCD_VALUE_PcdMaximumDevicePathNodeCount;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumDevicePathNodeCount;
#define _PCD_GET_MODE_32_PcdMaximumDevicePathNodeCount  _gPcd_FixedAtBuild_PcdMaximumDevicePathNodeCount

#define _PCD_TOKEN_PcdMaximumUnicodeStringLength  9U
#define _PCD_VALUE_PcdMaximumUnicodeStringLength  1000000U
//GLOBAL_REMOVE_IF_UNREFERENCED const UINT32 _gPcd_FixedAtBuild_PcdMaximumUnicodeStringLength = _PCD_VALUE_PcdMaximumUnicodeStringLength;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumUnicodeStringLength;
#define _PCD_GET_MODE_32_PcdMaximumUnicodeStringLength  _gPcd_FixedAtBuild_PcdMaximumUnicodeStringLength

#define _PCD_TOKEN_PcdMaximumAsciiStringLength  8U
#define _PCD_VALUE_PcdMaximumAsciiStringLength  1000000U
//GLOBAL_REMOVE_IF_UNREFERENCED const UINT32 _gPcd_FixedAtBuild_PcdMaximumAsciiStringLength = _PCD_VALUE_PcdMaximumAsciiStringLength;
extern const  UINT32  _gPcd_FixedAtBuild_PcdMaximumAsciiStringLength;
#define _PCD_GET_MODE_32_PcdMaximumAsciiStringLength  _gPcd_FixedAtBuild_PcdMaximumAsciiStringLength
// Definition of PCDs used in libraries is in AutoGen.c

// Definition of PCDs used in this module
#define _PCD_PATCHABLE_VALUE_PcdPciExpressBaseAddress  ((UINT64)0x80000000ULL)
extern UINT64 _gPcd_BinaryPatch_PcdPciExpressBaseAddress;
//GLOBAL_REMOVE_IF_UNREFERENCED UINTN _gPcd_BinaryPatch_Size_PcdPciExpressBaseAddress = 8;


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
