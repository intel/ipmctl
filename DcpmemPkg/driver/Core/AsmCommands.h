/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ASM_COMMANDS_H_
#define _ASM_COMMANDS_H_
#include "Utility.h"

/**
  The software query the availability of the new instructions must be set to:
  CPUID.(EAX=07H, ECX=0H)
**/
#define CPUID_NEWMEM_FUNCTIONS_EAX  0x7
#define CPUID_NEWMEM_FUNCTIONS_ECX  0x0

/**
  Sends the assembler CPUID command to the processor.

  This is the extended version, where the caller can also specify the initial value of the ECX register.

  @param[in, out] pCpuInfo array of four 32bit integers, where the result will be stored.
    Their value also takes part in the command parsing.
  @param[in] InfoType the main parameter that defines what "page" of CPU capabilities the caller wants to retrieve.
  @param[in] EcxValue the initial value of the ECX register just before calling the CPUID processor command.

  @retval - nothing, the result data will be stored int the CpuInfo array.

  Warning! The InfoType parameter can go outside of the processor supported pages, resulting with a CPU exception.
  It is the caller responsibility to be sure that the processor supports the requested parameter or that the user
  performs proper checking before issuing the command.
**/
VOID
EFIAPI
AsmCpuidEcx(
  IN     UINT32 RegisterInEax,
  IN     UINT32 RegisterInEcx,
     OUT UINT32 *pRegisterOutEax,
     OUT UINT32 *pRegisterOutEbx,
     OUT UINT32 *pRegisterOutEcx,
     OUT UINT32 *pRegisterOutEdx
  );

/**
  Performs a serializing operation on all store-to-memory
  instructions that were issued prior the SFENCE instruction.

  The function is defined here, because the used GCC does not yet have it defined.
**/
VOID
AsmSfence(
  );

/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  This is one of the faster flushing function available, it is preferred to
  use it if the processor does not support any faster flush functions.

  Flushed the cache line specified by LinearAddress.

  @param[in] LinearAddress The address of the cache line to flush.
**/
VOID
AsmClFlushOpt(
  IN     VOID *pLinearAddress
  );

/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  This is currently the fastest flushing function available, it is preferred to
  use it if the processor supports it.

  Flushed the cache line specified by LinearAddress.

  @param[in] LinearAddress The address of the cache line to flush.
**/
VOID
AsmClWb(
  IN     VOID *pLinearAddress
  );

/**
  Flushes a cache line from all the instruction and data caches within the
  coherency domain of the CPU.

  This is one of the first flushing function available, it is the slowest so it is preferred to
  use only if the processor does not support any other flush functions.

  Flushed the cache line specified by LinearAddress.

  @param[in] LinearAddress The address of the cache line to flush.
**/
VOID
AsmFlushCl(
  IN     VOID *LinearAddress
  );

/**
    Loads the pSrc buffer into XMM register and does a non-temporal copy of 128 bits to the pDest buffer.
    No buffer validation takes place here.

    @param[in] pDest The destination buffer, needs to have at least 16 bytes. Needs to be aligned to 16 bytes.
    @param[in] pSrc The source buffer, needs to have at least 16 bytes. No alignment required.
**/
VOID
AsmNonTemporalStore128(
  IN     VOID *pDest,
     OUT VOID *pSrc
  );

/**
    Loads the pSrc buffer into YMM register and does a non-temporal copy of 256 bits to the pDest buffer.
    No buffer validation takes place here.

    @param [in] pDest The destination buffer, needs to have at least 32 bytes. Needs to be aligned to 32 bytes.
    @param [in] pSrc The source buffer, needs to have at least 32 bytes. No alignment required.
**/
VOID
AsmNonTemporalStore256(
  IN     VOID *pDest,
     OUT VOID *pSrc
  );

/**
    Loads the pSrc buffer into ZMM register and does a non-temporal copy of 512 bits to the pDest buffer.
    No buffer validation takes place here.

    @param [in] pDest The destination buffer, needs to have at least 64 bytes. Needs to be aligned to 64 bytes.
    @param [in] pSrc The source buffer, needs to have at least 64 bytes. No alignment required.
**/
VOID
AsmNonTemporalStore512(
  IN     VOID *pDest,
     OUT VOID *pSrc
  );

#endif
