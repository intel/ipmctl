/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Implementation of Linux device adapter interface for general operations.
 */
#include <stdio.h>
#include "lnx_adapter.h"

/*
 * Retrieve the vendor specific NVDIMM driver version.
 */
int get_vendor_driver_revision(char * version_str, const int str_len)
{
	snprintf(version_str, str_len, "0.0.0.0");
	return 0;
}
