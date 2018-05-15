/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LNX_ADAPTER_H_
#define	LNX_ADAPTER_H_
#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __MSVC__
#define PACK_STRUCT(_structure_) __pragma(pack(push,1)) _structure_; __pragma(pack(pop))

#else
#define PACK_STRUCT(_structure_) _structure_ __attribute__((packed));
#endif
int wait_for_sec(unsigned int seconds);

#ifdef __cplusplus
}
#endif


#endif
