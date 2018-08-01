/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FIRMWAREMANAGEMENT_H_
#define _FIRMWAREMANAGEMENT_H_

#include <Uefi.h>
#include <Protocol/FirmwareManagement.h>

extern EFI_GUID gNvmDimmFirmwareManagementProtocolGuid;
extern EFI_GUID mNvmDimmFirmwareImageTypeGuid;

/* {c376fe51-aad4-4b40-9ed2-f775cbbdc6a1} */
#define EFI_DCPMM_FIRMWARE_IMAGE_TYPE_GUID \
{0xc376fe51, 0xaad4, 0x4b40, {0x9e, 0xd2, 0xf7, 0x75, 0xcb, 0xbd, 0xc6, 0xa1}}

#define GET_DIMM_FROM_INSTANCE(InstanceAddress) BASE_CR(InstanceAddress, EFI_DIMMS_DATA, FirmwareManagementInstance)

#define PACKAGE_VERSION_DEFINED_BY_PACKAGE_NAME 0xFFFFFFFE

// For HII, the max string length is 255 characters + null char
// Just in case this needs to be cross platform, make it a sizeof(CHAR16)
#define HII_STRING_MAX_STR_LEN  255
#define HII_MAX_STRING_BUFFER_LENGTH  (HII_STRING_MAX_STR_LEN + 1) * sizeof(CHAR16)

#define NVDIMM_IMAGE_ID_NAME L"Intel persistent memory"
#define NVDIMM_IMAGE_ID_NAME_LEN 24 // including null char

#pragma pack(push)
#pragma pack(1)
typedef struct {
  UINT8 FwProduct : 6;
  UINT8 FwRevision : 6;
  UINT8 FwSecurityVersion : 6;
  UINT16 FwBuild : 14;
} FW_VERSION_INFORMATION_PACKED;
#pragma pack(pop)

typedef union {
  FW_VERSION_INFORMATION_PACKED PackedVersion;
  UINT32 AsUint32;
} FW_VERSION_UNION;

#endif /** _FIRMWAREMANAGEMENT_H_ **/
