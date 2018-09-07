/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Utility.h>
#include <NvmDimmPassThru.h>
#include "NvmDimmDriver.h"
#include "NvmSecurity.h"
#include <Protocol/StorageSecurityCommand.h>

/**
  Supported Security Protocols & Protocol Specific parameters
**/
#define SECURITY_PROTOCOL_INFORMATION          0x00      //!< Supported protocols information
#define CMD_SUPPORTED_SECURITY_PROTOCOL_LIST   0x0000    //!< Retrieve supported security protocols
#define SECURITY_PROTOCOL_FW_COMMANDS          0xFA      //!< Vendor specific protocol
#define CMD_OPCODE_MASK                        0xFF00    //!< 2nd byte contains Opcode
#define CMD_SUBOPCODE_MASK                     0x00FF    //!< 1st byte contains SubOpcode
#define SUPPORTED_PROTOCOL_LIST_LENGTH         2         //!< Number of supported protocols

#pragma pack(push)
#pragma pack(1)
typedef struct {
  UINT8 Reserved[6];
  UINT16 Length;
  UINT8 Protocol[SUPPORTED_PROTOCOL_LIST_LENGTH];
} PROTOCOL_INFORMATION;
#pragma pack(pop)

/**
  Instance of StorageSecurityCommandProtocol
**/

EFI_STATUS
EFIAPI
ReceiveData (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *This,
  IN UINT32 MediaId,                                 //!< Ignored in this implementation
  IN UINT64 Timeout,                                 //!< Timeout passed to PassThru protocol
  IN UINT8 SecurityProtocol,                         //!< Specifies which security protocol is being used
  IN UINT16 SecurityProtocolSpecificData,            //!< Command Opcode & SubOpcode in case of FW commands
  IN UINTN PayloadBufferSize,
  OUT VOID *PayloadBuffer,
  OUT UINTN *PayloadTransferSize)
{
  NVDIMM_ENTRY();
  EFI_STATUS ReturnCode = EFI_UNSUPPORTED;

  UINT8 SecurityState = 0;
  UINT8 Opcode, SubOpcode;
  PROTOCOL_INFORMATION SupportedProtocolsData = {
    .Reserved = {0},
    .Length = SUPPORTED_PROTOCOL_LIST_LENGTH,
    .Protocol = {SECURITY_PROTOCOL_INFORMATION, SECURITY_PROTOCOL_FW_COMMANDS}
  };

  EFI_DIMMS_DATA *Dimm = BASE_CR(This, EFI_DIMMS_DATA, StorageSecurityCommandInstance);

  if ((PayloadBuffer == NULL || PayloadTransferSize == NULL) && PayloadBufferSize != 0) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (SecurityProtocol != SECURITY_PROTOCOL_INFORMATION &&
      SecurityProtocol != SECURITY_PROTOCOL_FW_COMMANDS) {
    NVDIMM_WARN("Security protocol id %d not supported by ReceiveData function", SecurityProtocol);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  /**
    Security Protocol Information
  **/
  if (SecurityProtocol == SECURITY_PROTOCOL_INFORMATION) {
    if (SecurityProtocolSpecificData == CMD_SUPPORTED_SECURITY_PROTOCOL_LIST) {
      NVDIMM_DBG("Retrieving supported security protocol list ...");
      if (PayloadBuffer == NULL || PayloadTransferSize == NULL) {
        ReturnCode = EFI_INVALID_PARAMETER;
        goto Finish;
      }
      // check amount of data to be returned
      *PayloadTransferSize = sizeof(SupportedProtocolsData);
      if (*PayloadTransferSize > PayloadBufferSize) {
        NVDIMM_DBG("Buffer too small");
        *PayloadTransferSize = PayloadBufferSize;
        ReturnCode = EFI_WARN_BUFFER_TOO_SMALL;
      } else {
        ReturnCode = EFI_SUCCESS;
      }
      CopyMem_S(PayloadBuffer, PayloadBufferSize, &SupportedProtocolsData, *PayloadTransferSize);
      goto Finish;

    } else {
      NVDIMM_WARN("Command not not supported: 0x%x", SecurityProtocolSpecificData);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }
  }

  /**
    FW Commands
  **/
  if (SecurityProtocol == SECURITY_PROTOCOL_FW_COMMANDS) {
    Opcode = (UINT8) ((SecurityProtocolSpecificData & CMD_OPCODE_MASK) >> 8);
    SubOpcode = (UINT8) (SecurityProtocolSpecificData & CMD_SUBOPCODE_MASK);
    NVDIMM_DBG("FW command: Opcode=%x, SubOpcode=%x", Opcode, SubOpcode);

    if (Opcode == PtGetSecInfo) {
      if (SubOpcode == SubopGetSecState) {
        if (PayloadBuffer == NULL || PayloadTransferSize == NULL) {
          ReturnCode = EFI_INVALID_PARAMETER;
          goto Finish;
        }
        ReturnCode = GetDimmSecurityState(Dimm->pDimm, PT_TIMEOUT_INTERVAL, &SecurityState);
        if (EFI_ERROR(ReturnCode)) {
          NVDIMM_DBG("Failed on GetDimmSecurityState, status=" FORMAT_EFI_STATUS "", ReturnCode);
          goto Finish;
        }

        // check amount of data to be returned
        *PayloadTransferSize = sizeof(SecurityState);
        if (*PayloadTransferSize > PayloadBufferSize) {
          NVDIMM_DBG("Buffer too small");
          *PayloadTransferSize = PayloadBufferSize;
          ReturnCode = EFI_WARN_BUFFER_TOO_SMALL;
        }
        // write data to return buffer
        CopyMem_S(PayloadBuffer, PayloadBufferSize, &SecurityState, *PayloadTransferSize);

      } else {
        //SubOpcode not supported
        NVDIMM_WARN("Command not supported: Opcode=%x, SubOpcode=%x", Opcode, SubOpcode);
        ReturnCode = EFI_UNSUPPORTED;
        goto Finish;
      }

    } else {
      //Opcode not supported
      NVDIMM_WARN("Command not supported: Opcode=%x, SubOpcode=%x", Opcode, SubOpcode);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STATUS
EFIAPI
SendData (
  IN EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *This,
  IN UINT32 MediaId,
  IN UINT64 Timeout,
  IN UINT8 SecurityProtocol,
  IN UINT16 SecurityProtocolSpecificData,
  IN UINTN PayloadBufferSize,
  IN VOID *PayloadBuffer)
{
  NVDIMM_ENTRY();
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  UINT8 Opcode, SubOpcode;

  EFI_DIMMS_DATA *Dimm = BASE_CR(This, EFI_DIMMS_DATA, StorageSecurityCommandInstance);

  if ((PayloadBuffer == NULL) && (PayloadBufferSize != 0)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (SecurityProtocol != SECURITY_PROTOCOL_FW_COMMANDS) {
    NVDIMM_DBG("Security protocol id %d not supported by SendData function", SecurityProtocol);
    ReturnCode = EFI_UNSUPPORTED;
    goto Finish;
  }

  /**
    FW Commands
  **/
  if (SecurityProtocol == SECURITY_PROTOCOL_FW_COMMANDS) {
    Opcode = (UINT8) ((SecurityProtocolSpecificData & CMD_OPCODE_MASK) >> 8);
    SubOpcode = (UINT8) (SecurityProtocolSpecificData & CMD_SUBOPCODE_MASK);
    NVDIMM_DBG("FW command: Opcode=%x, SubOpcode=%x", Opcode, SubOpcode);

    if (Opcode == PtSetSecInfo) {
      ReturnCode =
        SetDimmSecurityState(Dimm->pDimm, Opcode, SubOpcode, (UINT16)PayloadBufferSize, PayloadBuffer, PT_TIMEOUT_INTERVAL);
      if (EFI_ERROR(ReturnCode)) {
        NVDIMM_DBG("Failed on SetDimmSecurityState, status=" FORMAT_EFI_STATUS "", ReturnCode);
        goto Finish;
      }
    } else {
      //Opcode not supported
      NVDIMM_WARN("Command not supported: Opcode=%x, SubOpcode=%x", Opcode, SubOpcode);
      ReturnCode = EFI_UNSUPPORTED;
      goto Finish;
    }
  }

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STORAGE_SECURITY_COMMAND_PROTOCOL gNvmDimmDriverStorageSecurityCommand =
{
  ReceiveData,
  SendData
};
