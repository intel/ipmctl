/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _READ_RUN_TIME_DISPLAY_PREFERENCES_H_
#define _READ_RUN_TIME_DISPLAY_PREFERENCES_H_

#include <Base.h>
#include <Types.h>

extern EFI_GUID gNvmDimmVariableGuid;

enum DISPLAY_TYPE {DISPLAY_HII_INFO, DISPLAY_CLI_INFO};

/**
  Retrieve the User Preferences from RunTime Services.

  @param[in, out] pDisplayPreferences pointer to the current driver preferences.
  @param[in] DisplayRequest enum of whether information from hii or cli will be displayed.

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All ok
**/
EFI_STATUS
ReadRunTimePreferences(
  IN  OUT DISPLAY_PREFERENCES *pDisplayPreferences,
  IN BOOLEAN DisplayRequest
);

#endif /** _READ_RUN_TIME_DISPLAY_PREFERENCES_H_ **/
