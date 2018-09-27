/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_REGIONS_COMMAND_H_
#define _SHOW_REGIONS_COMMAND_H_

#include "CommandParser.h"

#define REGION_ID_STR                   L"RegionID"
#define ISET_ID_STR                     L"ISetID"
#define PERSISTENT_MEM_TYPE_STR         L"PersistentMemoryType"
#define TOTAL_CAPACITY_STR              L"Capacity"
#define FREE_CAPACITY_STR               L"FreeCapacity"
#define REGION_HEALTH_STATE_STR         L"HealthState"

/** Region Health States */
#define HEALTHY_STATE                   L"Healthy"
#define ERROR_STATE                     L"Error"
#define PENDING_STATE                   L"Pending"
#define LOCKED_STATE                    L"Locked"
#define UNKNOWN_STATE                   L"Unknown"

#define DIMM_ID_STR_DELIM               L", "

/**
  Register the show regions command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowRegionsCommand(
  );

/**
  Execute the show regions command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval EFI_ABORTED invoking CONFIG_PROTOCOL function failure
**/
EFI_STATUS
ShowRegions(
  IN     struct Command *pCmd
  );

#endif /* _SHOW_REGIONS_COMMAND_H_ */
