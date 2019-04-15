/**
  @file
  @internal
  @copyright
  Copyright 2019 Intel Corporation All Rights Reserved.

  INTEL CONFIDENTIAL

  The source code contained or described herein and all documents related to
  the source code ("Material") are owned by Intel Corporation or its suppliers
  or licensors. Title to the Material remains with Intel Corporation or its
  suppliers and licensors. The Material may contain trade secrets and
  proprietary and confidential information of Intel Corporation and its
  suppliers and licensors, and is protected by worldwide copyright and trade
  secret laws and treaty provisions. No part of the Material may be used,
  copied, reproduced, modified, published, uploaded, posted, transmitted,
  distributed, or disclosed in any way without Intel's prior express written
  permission.

  No license under any patent, copyright, trade secret or other intellectual
  property right is granted to or conferred upon you by disclosure or delivery
  of the Materials, either expressly, by implication, inducement, estoppel or
  otherwise. Any license under such intellectual property rights must be
  express and approved by Intel in writing.

  Unless otherwise agreed by Intel in writing, you may not remove or alter
  this notice or any other notice embedded in Materials by Intel or Intel's
  suppliers or licensors in any way.
  @endinternal
**/

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