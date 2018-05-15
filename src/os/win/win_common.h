/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CR_MGMT_WIN_COMMON_H
#define	CR_MGMT_WIN_COMMON_H

#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>

int wait_for_sec(unsigned int seconds);

#endif // CR_MGMT_WIN_COMMON_H
