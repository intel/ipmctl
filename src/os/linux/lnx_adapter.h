/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file declares common definitions and helper functions used throughout the Linux
 * device adapter.
 */

#ifndef LNX_ADAPTER_H_
#define	LNX_ADAPTER_H_

#include <stddef.h>
#include <sys/user.h>
#include <linux/ndctl.h>
#include <ndctl/libndctl.h>

#define	SYSFS_ATTR_SIZE 1024

int linux_err_to_nvm_lib_err(int);
int open_ioctl_target(int *p_target, const char *dev_name);
int send_ioctl_command(int fd, unsigned long request, void* parg);
int get_dimm_by_handle(struct ndctl_ctx *ctx, unsigned int handle, struct ndctl_dimm **dimm);
int get_unconfigured_namespace(struct ndctl_namespace **unconfigured_namespace,
	struct ndctl_region *region);
int get_vendor_driver_revision(char * version_str, const int str_len);

#endif /* LNX_ADAPTER_H_ */
