/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DRIVER_HEALTH_H_
#define _DRIVER_HEALTH_H_

#include <Uefi.h>
#include <Protocol/DriverHealth.h>


/**
  Get Dimm health status

  @param[in] DimmPid Dimm ID
  @param[out] pHealthStatus A pointer to the health status that is returned by this function.

  @retval EFI_SUCCESS Completed without issues
  @retval EFI_INVALID_PARAMETER pHealthStatus is NULL
**/
EFI_STATUS
GetDimmHealthStatus (
  IN     UINT16 DimmPid,
     OUT EFI_DRIVER_HEALTH_STATUS *pHealthStatus
  );

extern EFI_GUID gNvmDimmDriverHealthGuid;

#endif /** _DRIVER_HEALTH_H_ **/
