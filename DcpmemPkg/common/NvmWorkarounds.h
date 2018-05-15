/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
  Dynamic workarounds flag.
  Only in the debug version, we enable the runtime workarounds that can be set by an enviromental
  variable in the shell (before loading the driver).
**/
#if !defined(MDEPKG_NDEBUG)
#define DYNAMIC_WA_ENABLE
#endif

/**
  While the BIOS image has still older UEFI driver integrated, there is a mismatch between the checksum that it is
  initializing and the current driver is. In this case we need to skip this check to allow our driver
  properly handle the LSA regions on the DIMM.
**/
#define WA_SKIP_LSA_CHECKSUM_FAIL

/**
  SMBios table doesn't contain valid serial/model numbers
**/
#define WA_GARBAGE_IN_SMBIOS_NVM_DIMM_ENTRIES

/**
  No NGNVM memory type in the SMBIOS specification yet.
**/
#define WA_SMBIOS_SPEC_DOES_NOT_CONTAIN_NGNVM_MEM_TYPE_YET

/**
  Clearing buffer shouldn't be needed. In normal situations it is excessive.
  In case of problems uncomment this define to enforce clearing large payload in every passthru command.
**/
//#define WA_CLEAR_LARGE_PAYLOAD_IN_PASSTHRU

/**
  Duplicate DMA commands sent to channel require adding a delay
  before any writes to large payload register. Value in microseconds.
**/
#define WA_MEDIA_WRITES_DELAY   5000

/**
  Ignore partition size restriction for Nanocore environment
**/
//#define WA_IGNORE_PARTITION_SIZE_RESTRICTION

/**
  Increase the mailbox timeout timers
**/
//#define WA_APPLY_LARGE_MAILBOX_TIMEOUTS

/**
  Run "Update Firmware" via small payloads
**/
//#define WA_UPDATE_FIRMWARE_VIA_SMALL_PAYLOAD

/**
  This makes any reads on BlockIO to be made twice.
**/
#define WA_BLOCK_IO_READ_TWICE

/**
  Make Mailbox writes post in the absence of WPQFlush
**/
//#define WA_NO_WPQFLUSH
