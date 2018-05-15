/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SRC_CLI_SHOWSYSTEMCAPABILITIESCOMMAND_H_
#define _SRC_CLI_SHOWSYSTEMCAPABILITIESCOMMAND_H_

#include "CommandParser.h"
#include <NvmTables.h>

/**
  Current Memory Modes definitions as for the PCD/PCAT spec R086
**/
#define VOLATILE_MODE_1LM     0x00 // 00b - 1LM Mode
#define VOLATILE_MODE_MEMORY  0x01 // 01b - Memory Mode (2LM)
#define PERSISTENT_MODE_DISABLED          0x00 // 00b - Disabled
#define PERSISTENT_MODE_APP_DIRECT        0x01 // 01b - AppDirect PM Mode
#define PERSISTENT_MODE_APP_DIRECT_CACHE  0x02 // 10b - AppDirect Cached PM Mode

/** Display options for this command **/
#define PLATFORM_CONFIG_SUPPORT_STR              L"PlatformConfigSupported"
#define MEMORY_ALIGNMENT_STR                     L"Alignment"
#define VOLATILE_MODE_ALLOWED_STR                L"AllowedVolatileMode"
#define VOLATILE_MODE_CURRENT_STR                L"CurrentVolatileMode"
#define APPDIRECT_MODE_ALLOWED_STR               L"AllowedAppDirectMode"
#define OPERATING_MODE_SUPPORT_STR               L"ModesSupported"
#define APPDIRECT_SETTINGS_SUPPORTED_STR         L"SupportedAppDirectSettings"
#define APPDIRECT_SETTINGS_RECCOMENDED_STR       L"RecommendedAppDirectSettings"
#define MIN_NAMESPACE_SIZE_STR                   L"MinNamespaceSize"
#define APPDIRECT_MIRROR_SUPPORTED_STR           L"AppDirectMirrorSupported"
#define DIMM_SPARE_SUPPORTED_STR                 L"DimmSpareSupported"
#define APPDIRECT_MIGRATION_SUPPORTED_STR        L"AppDirectMigrationSupported"
#define RENAME_NAMESPACE_SUPPORTED_STR           L"RenameNamespaceSupported"
#define GROW_APPDIRECT_NAMESPACE_SUPPORTED_STR   L"GrowAppDirectNamespaceSupported"
#define SHRINK_APPDIRECT_NAMESPACE_SUPPORTED_STR L"ShrinkAppDirectNamespaceSupported"
#define INITIATE_SCRUB_SUPPORTED                 L"InitiateScrubSupported"
#define ASYNCHRONOUS_DRAM_REFRESH_SUPPORTED_STR  L"AdrSupported"
#define ERASE_DEVICE_DATA_SUPPORTED_STR          L"EraseDeviceDataSupported"
#define ENABLE_DEVICE_SECURITY_SUPPORTED_STR     L"EnableDeviceSecuritySupported"
#define DISABLE_DEVICE_SECURITY_SUPPORTED_STR    L"DisableDeviceSecuritySupported"
#define UNLOCK_DEVICE_SECURITY_SUPPORTED_STR     L"UnlockDeviceSecuritySupported"
#define FREEZE_DEVICE_SECURITY_SUPPORTED_STR     L"FreezeDeviceSecuritySupported"
#define CHANGE_DEVICE_PASSPHRASE_SUPPORTED_STR   L"ChangeDevicePassphraseSupported"
#define APPDIRECT_STR                            L"App Direct"
#define MEMORY_STR                               L"Memory Mode"
#define ONE_LM_STR                               L"1LM"
#define DISABLED_STR                             L"Disabled"
#define UNKNOWN_STR                              L"Unknown"

EFI_STATUS
ShowSystemCapabilities(
  IN     struct Command *pCmd
  );

EFI_STATUS
RegisterShowSystemCapabilitiesCommand(
  );

#endif /* _SRC_CLI_SHOWSYSTEMCAPABILITIESCOMMAND_H_ */
