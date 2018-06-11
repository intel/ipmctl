/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CR_MGMT_SCM2_PASSTHROUGH_H
#define CR_MGMT_SCM2_PASSTHROUGH_H

#include <windows.h>
#include <initguid.h>

#ifdef __cplusplus
extern "C"
{
#endif

int win_scm2_ioctl_passthrough_cmd(unsigned short nfit_handle,
		unsigned short op_code, unsigned short sub_op_code,
		void *input_payload, unsigned long input_payload_size,
		void *output_payload, unsigned long output_payload_size,
		unsigned int *p_dsm_status);


#ifdef __cplusplus
}
#endif


#endif // CR_MGMT_SCM2_PASSTHROUGH_H
