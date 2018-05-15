/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _OS_BS_PROTOCOL_H_
#define _OS_BS_PROTOCOL_H_

#include <Types.h>
#include <NvmTypes.h>
#include <Uefi.h>
#include <Debug.h>

extern EFI_BOOT_SERVICES gBootServices;
int init_protocol_bs();

#endif //_OS_BS_PROTOCOL_H_
