/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PBR_OS_H_
#define _PBR_OS_H_

#include <Types.h>
#include <PbrTypes.h>

EFI_STATUS PbrSerializeCtx(PbrContext *ctx, BOOLEAN Force);
EFI_STATUS PbrDeserializeCtx(PbrContext * ctx);

#endif //_PBR_OS_H_
