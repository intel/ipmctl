/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <windows.h>
#include <initguid.h>
//
// The protocol GUID for INVDIMM devices. The application and the driver use this value for
// the "ProtocolGuid" field of the NVDIMM_PASSTHROUGH_IN and NVDIMM_PASSTHROUGH_OUT structures.
// (4309AC30-0D11-11E4-9191-0800200C9A66)
//
DEFINE_GUID(GUID_NVDIMM_PASSTHROUGH_INVDIMM, 0x4309AC30, 0x0D11, 0x11E4, 0x91, 0x91, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66);

#ifndef CR_MGMT_SCM2_PASSTHROUGH_H
#define	CR_MGMT_SCM2_PASSTHROUGH_H

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push, 1)

/*!
    Defines an alias representing the dsm status enum.
*/
typedef enum _DSM_STATUS_ENUM {
    STATUS_DSM_SUCCESS = 0,
    STATUS_DSM_NOT_SUPPORTED,
    STATUS_DSM_NON_EXISTING_DEVICE,
    STATUS_DSM_INVALID_INPUT_PARAMETERS,
    STATUS_DSM_HW_ERROR,
    STATUS_DSM_RETRY_SUGGESTED,
    STATUS_DSM_ERROR_UNKNOWN_REASON,
    STATUS_DSM_VENDOR_SPECIFIC_ERROR,
    STATUS_DSM_RESERVED
} DSM_STATUS_ENUM;

/*!
    Defines an alias representing the dsm status.
*/
typedef struct _DSM_STATUS {
    UINT32 DsmStatus : 16;                               //!< DSM_STATUS
#define MAILBOX_STATUS_SUCCESS 0x0                       //!< Status code when success
#define MAILBOX_STATUS_CODE_INVALID_LARGE_PAYLOAD 0x80   //!< Status code when invalid large payload
#define MAILBOX_STATUS_CODE_INVALID_TRANSFER_LENGTH 0x81 //!< Status code when invalid transfer length
    UINT32 MailboxStatusCode : 8;                        //!< Status Code
    UINT32 BackgroundOperationState : 1;                 //!< Operation State
    UINT32 Reserved : 7;                                 //!< Reserved
} DSM_STATUS, *PDSM_STATUS; //!< @see _DSM_STATUS

/*!
    Defines an alias representing the dsm vendor specific command input payload.
*/
typedef struct _DSM_VENDOR_SPECIFIC_COMMAND_INPUT_PAYLOAD {
    ULONG Arg3OpCode;                      //!< Specifies OpCode/SubOpCode
    ULONG Arg3OpCodeParameterDataLength;   //!< Specifies Arg3OpCodeParameterDataBuffer length.
#define ARG3_OPCODE_PARAMETER_DATA_BUFFER_PLACEHOLDER 1 //!< Placeholder size
    UCHAR Arg3OpCodeParameterDataBuffer[ARG3_OPCODE_PARAMETER_DATA_BUFFER_PLACEHOLDER]; //!< Input space for complete input buffer length
} DSM_VENDOR_SPECIFIC_COMMAND_INPUT_PAYLOAD, *PDSM_VENDOR_SPECIFIC_COMMAND_INPUT_PAYLOAD; //!< @see _DSM_VENDOR_SPECIFIC_COMMAND_INPUT_PAYLOAD

/*!
    Defines an alias representing the dsm vendor specific command output payload.
    This struct is populated by bios.
*/
typedef struct _DSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD {
    DSM_STATUS Arg3Status;          //!< The argument 3 status.
    UINT32 Arg3OutputBufferLength;  //!< Length of the argument 3 output buffer.
#define DSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD_PLACEHOLDERS 1   //!< Placeholders size
    UCHAR Arg3OutputBuffer[DSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD_PLACEHOLDERS]; //!< Output space for output buffer of length of OutputBufferLength
} DSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD, *PDSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD; //!< @see _DSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD

typedef struct _NVDIMM_PASSTHROUGH_IN {
    //
    // The size of the structure, including the Data field, in bytes.
    //
    ULONG Size;
    //
    // The version of the structure.
    //
    ULONG Version;
    //
    // This GUID defines which command protocol is being used. The driver will
    // check this field to make sure the application is sending commands for
    // device types that the driver understands.
    //
    GUID ProtocolGuid;
    //
    // The size, in bytes, of the data field.
    //
    ULONG DataSize;
    //
    // The NVDIMM-type specific structure which contains the passthrough command.
    //
    DSM_VENDOR_SPECIFIC_COMMAND_INPUT_PAYLOAD Data;
} NVDIMM_PASSTHROUGH_IN, *PNVDIMM_PASSTHROUGH_IN;

#define NVDIMM_PASSTHROUGH_IN_V1 1UL // V1 of the NVDIMM_PASSTHROUGH_IN structure.

typedef struct _NVDIMM_PASSTHROUGH_OUT {
    //
    // The size of the structure, including the Data field, in bytes.
    //
    ULONG Size;
    //
    // The version of the structure.
    //
    ULONG Version;
    //
    // This GUID defines which command protocol is being used. The application should
    // check this field to make sure the driver is using a protocol that it understands.
    //
    GUID ProtocolGuid;
    //
    // The size, in bytes, of the data field.
    //
    ULONG DataSize;
    //
    // The NVDIMM-type specific structure which contains the output of the passthrough command.
    //
    DSM_VENDOR_SPECIFIC_COMMAND_OUTPUT_PAYLOAD Data;
} NVDIMM_PASSTHROUGH_OUT, *PNVDIMM_PASSTHROUGH_OUT;

#define NVDIMM_PASSTHROUGH_OUT_V1 1UL // V1 of the NVDIMM_PASSTHROUGH_OUT structure.

#pragma pack(pop)

int win_scm2_ioctl_passthrough_cmd(unsigned short nfit_handle,
		unsigned short op_code, unsigned short sub_op_code,
		void *input_payload, unsigned long input_payload_size,
		void *output_payload, unsigned long output_payload_size,
		unsigned int *p_dsm_status);


#ifdef __cplusplus
}
#endif


#endif // CR_MGMT_SCM2_PASSTHROUGH_H
