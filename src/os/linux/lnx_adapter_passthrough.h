/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file implements the Linux driver adapter interface for issuing IOCTL
 * passthrough commands.
 */

//#include "device_adapter.h"
//#include "lnx_adapter.h"
//#include <os/os_adapter.h>
#include <string.h>
#include <stdlib.h>
#include <os_types.h>



#pragma pack(push)
#pragma pack(1)
struct pt_bios_get_size {
	unsigned int large_input_payload_size;
	unsigned int large_output_payload_size;
	unsigned int rw_size;
};
#pragma pack(pop)



/*
 * Execute a passthrough IOCTL
 */
int ioctl_passthrough_fw_cmd(struct fw_cmd *p_fw_cmd);
