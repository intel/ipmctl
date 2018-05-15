/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <windows.h>
#include <WinBase.h>
#include "win_common.h"


#ifdef _WIN32
#ifndef WINBOOL
#define WINBOOL BOOL
#endif
#endif

int wait_for_sec(unsigned int seconds)
{
	Sleep(1000 * seconds);
	return 0;
}
