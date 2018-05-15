/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CR_MGMT_WIN_SCM2_PASSTHROUGH_H
#define	CR_MGMT_WIN_SCM2_PASSTHROUGH_H

#include <os_types.h>
#ifdef __cplusplus
extern "C"
{
#endif




#pragma pack(push)
#pragma pack(1)
	struct pt_bios_get_size {
	unsigned int large_input_payload_size;
	unsigned int large_output_payload_size;
	unsigned int rw_size;
	};
#pragma pack(pop)



int win_scm2_passthrough(struct fw_cmd *p_cmd, unsigned int *p_dsm_status);

#ifdef __cplusplus
}
#endif

#endif // CR_MGMT_WIN_SCM2_PASSTHROUGH_H
