/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file implements the Linux driver adapter interface for issuing IOCTL
 * passthrough commands.
 */


#include "lnx_common.h"
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

int wait_for_sec(unsigned int seconds)
{
	struct timeval tv = { seconds, 0 };
	select(0, NULL, NULL, NULL, &tv);
	return 0;
}
