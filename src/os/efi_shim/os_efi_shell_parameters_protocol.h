/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _OS_SHELL_PARAM_PROTOCOL_H_
#define _OS_SHELL_PARAM_PROTOCOL_H_

#include <Types.h>
#include <NvmTypes.h>
#include <Uefi.h>
#include <Debug.h>

extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;

EFI_STATUS init_protocol_shell_parameters_protocol(int argc, char *argv[]);
int uninit_protocol_shell_parameters_protocol();


#endif //_OS_SHELL_PARAM_PROTOCOL_H_
