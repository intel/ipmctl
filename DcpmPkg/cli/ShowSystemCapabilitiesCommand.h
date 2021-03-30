/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SRC_CLI_SHOW_SYSTEM_CAPABILITIES_COMMAND_H_
#define _SRC_CLI_SHOW_SYSTEM_CAPABILITIES_COMMAND_H_

#include "CommandParser.h"
#include <NvmTables.h>

/** Display options for this command **/
#define PLATFORM_CONFIG_SUPPORT_STR              L"PlatformConfigSupported"
#define MEMORY_ALIGNMENT_STR                     L"Alignment"
#define VOLATILE_MODE_ALLOWED_STR                L"AllowedVolatileMode"
#define VOLATILE_MODE_CURRENT_STR                L"CurrentVolatileMode"
#define APPDIRECT_MODE_ALLOWED_STR               L"AllowedAppDirectMode"
#define OPERATING_MODE_SUPPORT_STR               L"ModesSupported"
#define APPDIRECT_SETTINGS_SUPPORTED_STR         L"SupportedAppDirectSettings"
#define APPDIRECT_SETTINGS_RECOMMENDED_STR       L"RecommendedAppDirectSettings"
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
#define CHANGE_MASTER_PASSPHRASE_SUPPORTED_STR   L"ChangeMasterPassphraseSupported"
#define MASTER_ERASE_DEVICE_DATA_SUPPORTED_STR   L"MasterEraseDeviceDataSupported"

#define APPDIRECT_STR                            L"App Direct"
#define TWO_LM_STR                               L"2LM"
#define ONE_LM_STR                               L"1LM"
#define ONE_LM_OR_TWO_LM_STR                     L"1LM or 2LM"
#define ONE_LM_PLUS_TWO_LM_STR                   L"1LM+2LM"
#define DISABLED_STR                             L"Disabled"
#define UNKNOWN_STR                              L"Unknown"

EFI_STATUS
ShowSystemCapabilities(
  IN     struct Command *pCmd
  );

EFI_STATUS
RegisterShowSystemCapabilitiesCommand(
  );

#endif /* _SRC_CLI_SHOW_SYSTEM_CAPABILITIES_COMMAND_H_ */
