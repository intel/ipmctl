/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PFN_LAYOUT_H_
#define _PFN_LAYOUT_H_

#define PFN_INFO_SIG_LEN 16              //!< Signature length
#define PFN_INFO_BLOCK_OFFSET SIZE_4KB  //!< PFN Info Block offset

typedef struct _PFN_INFO {
  UINT8 Sig[PFN_INFO_SIG_LEN];       //!< must be "NVDIMM_PFN_INFO\0"
  GUID Uuid;                        //!< PFN UUID
  GUID ParentUuid;                  //!< UUID of container
  UINT32 Flags;                     //!< see flag bits below
  UINT16 Major;                     //!< major version
  UINT16 Minor;                     //!< minor version
  UINT64 DataOff;                   //!< data offset relative to namespace_base + start pad
  UINT64 NPfns;                     //!< number of page frames hosted by this info block
  UINT32 Mode;                      //!< memmap array storage location, see mode bits below
  /* minor-version-1 additions for section alignment */
  UINT32 StartPad;                  //!< padding to align the capacity to a Linux "section" boundary (128MB)
  UINT32 EndTrunc;                  //!< reserved capacity to align to a Linux "section" boundary (128MB)
  /* minor-version-2 record the base alignment of the mapping */
  UINT32 Align;                     //!< base alignment of the mapping
  UINT8 Unused[4000];               //!< alignment to PFN_ALIGNMENT 4096
  UINT64 Checksum;                  //!< Fletcher64 checksum of all the fields
} PFN_INFO;

#endif // _PFN_LAYOUT_H_

