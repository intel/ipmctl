/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <nvm_management.h>
#include <os_types.h>
#include "event.h"

extern NVM_API int nvm_run_cli(int argc, char *argv[]);
int main(int argc, char *argv[])
{
	return nvm_run_cli(argc, argv);
}
