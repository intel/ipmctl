/** @file
  Header file for DCPMM NVDIMM Firmware Interface protocol.

  @copyright
  INTEL CONFIDENTIAL
  Copyright 2018 Intel Corporation. <BR>

  The source code contained or described herein and all documents related to the
  source code ("Material") are owned by Intel Corporation or its suppliers or
  licensors. Title to the Material remains with Intel Corporation or its suppliers
  and licensors. The Material may contain trade secrets and proprietary    and
  confidential information of Intel Corporation and its suppliers and licensors,
  and is protected by worldwide copyright and trade secret laws and treaty
  provisions. No part of the Material may be used, copied, reproduced, modified,
  published, uploaded, posted, transmitted, distributed, or disclosed in any way
  without Intel's prior express written permission.

  No license under any patent, copyright, trade secret or other intellectual
  property right is granted to or conferred upon you by disclosure or delivery
  of the Materials, either expressly, by implication, inducement, estoppel or
  otherwise. Any license under such intellectual property rights must be
  express and approved by Intel in writing.

  Unless otherwise agreed by Intel in writing, you may not remove or alter
  this notice or any other notice embedded in Materials by Intel or
  Intel's suppliers or licensors in any way.
**/

#ifndef _PROTOCOL_DCPMM_TYPES_H_
#define _PROTOCOL_DCPMM_TYPES_H_

//
// Interface to use for communication with NVDIMM firmware.
// It can be SMBUS or DDRT mailbox. There are two mailboxes on DDRT,
// OS and SMM, but this protocol always use OS mailbox. The user must be
// aware that commands restricted to SMM mailbox are not available here.
//
typedef enum {
  NvdimmStateFunctional,    ///< Media and control registers mapped.
  NvdimmStateMediaDisabled, ///< Media not mapped, but DDRT communication available.
  NvdimmStateNotMapped,     ///< Media and DDRT communication not available, try SMBUS.
  NvdimmStateMax
} DCPMM_NVDIMM_STATE;

//
// Interface to use for communication with NVDIMM firmware.
// It can be SMBUS or DDRT mailbox. There are two mailboxes on DDRT,
// OS and SMM, but this protocol always use OS mailbox. The user must be
// aware that commands restricted to SMM mailbox are not available here.
// The user must also be aware of limitations of SMBUS interface when using it.
//
typedef enum {
  FisOverDdrt,   ///< Default communication channel
  FisOverSmbus,  ///< Use if NVDIMM is not mapped to DDRT
  FisIfaceMax
} DCPMM_FIS_INTERFACE;

//
// Parameters for _DSM function Pass-Through Command DSM_FN_VENDOR_COMMAND.
// This DSM can send commands to the NVDIMM, but there are also emulated commands
// to read or write large payload.
//
#pragma pack(1)
typedef struct {
  struct {
    UINT16   FisCmd;           ///< FisCmd[15:8] -- Mailbox Sub-opcode
                               ///< FisCmd[7:0]  -- Mailbox Opcode
    UINT16   Reserved;
    UINT32   DataSize;         ///< Size of command specific data following Head structure
  } Head;
  union {
    struct {                   ///< This structure is used with FW commands
      UINT32 Payload[0];       ///< FIS request payload to copy to Small Payload input registers
    } Fis;
    struct {                   ///< This structure is used with FIS_CMD_READ_LP_OUTPUT_MB
      UINT32 Size;             ///< Number of bytes to read from Large Payload output
      UINT32 Offset;           ///< Offset in Larege Payload where to start reading
    } LpRead;
    struct {                   ///< This structure is used with FIS_CMD_WRITE_LP_INPUT_MB
      UINT32 Size;             ///< Number of bytes to write to Large Payload input
      UINT32 Offset;           ///< Offset in Large Payload input where to start writing
      UINT32 Payload[0];       ///< Data to write follow the LpWrite structure
    } LpWrite;
  } Data;
} DCPMM_FIS_INPUT;

typedef struct {
  struct {
    UINT32   DataSize;         ///< On input the size of buffer following Head, on exit size of data stored
  } Head;
  union {
    struct {                   ///< This structure is used with FW commands
      UINT32 Payload[0];       ///< Small Payload output from FIS request
    } Fis;
    struct {                   ///< This structure is used with FIS_CMD_GET_LP_MB_INFO
      UINT32 InpPayloadSize;
      UINT32 OutPayloadSize;
      UINT32 DataChunkSize;
    } LpInfo;
    struct {                   ///< This structure is used with FIS_CMD_READ_LP_OUTPUT_MB
      UINT32 Payload[0];       ///< Large Payload output from FIS requst
    } LpData;
    UINT64   Bsr;              ///< Boot Status Register value if it was FIS_CMD_GET_BOOT_STATUS
  } Data;
} DCPMM_FIS_OUTPUT;
#pragma pack()

//
// Error record that can be reported by DCPMM_ARS_STATUS() function.
//
typedef struct {
  UINT32    NfitHandle;       // NFIT handle of the NVDIMM that is part of the error record
  UINT32    Reserved;
  UINT64    SpaOfErrLoc;      // Start SPA of the error location
  UINT64    Length;           // Length of the error location region
} DCPMM_ARS_ERROR_RECORD;

#endif