/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CR_MGMT_WIN_SCM2_ADAPTER_H
#define	CR_MGMT_WIN_SCM2_ADAPTER_H

#define	WIN_SCM2_IS_SUCCESS(rc) (rc) == WIN_SCM2_SUCCESS
enum WIN_SCM2_RETURN_CODES
{
	WIN_SCM2_SUCCESS = 0,
	WIN_SCM2_ERR_UNKNOWN = -1,
	WIN_SCM2_ERR_DRIVERFAILED = -2,
	WIN_SCM2_ERR_NOMEMORY = -3,
};

int get_vendor_driver_revision(char * version_str, const int str_len);


#endif // CR_MGMT_WIN_SCM2_ADAPTER_H
