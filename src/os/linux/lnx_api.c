/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the Windows implementation of the os_adapter.h
 * system call wrappers.
 */

#include <stdio.h>
#include <syslog.h>
#include <os_types.h>
#include <unistd.h>

/**
Sleeps for a given number of microseconds.
**/
int bs_sleep(int microseconds) {
  usleep(microseconds);
  return 0;
}
