/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _OS_TYPES_H_
#define _OS_TYPES_H_


#define DSM_VENDOR_ERROR_SHIFT (0)
#define DSM_MAILBOX_ERROR_SHIFT (16)
#define DSM_BACKGROUND_OP_STATE_SHIFT (24)
#define DSM_VENDOR_ERROR(status) ((status & 0xFFFF) >> DSM_VENDOR_ERROR_SHIFT)
#define DSM_EXTENDED_ERROR(status) ((status & 0xFFFF0000) >> DSM_MAILBOX_ERROR_SHIFT)
#define DSM_MAX_RETRIES 5

#define BUILD_DSM_OPCODE(Opcode, SubOpcode) (UINT32)(SubOpcode << 8 | Opcode)

#define COMMON_LOG_ENTRY()
#define COMMON_LOG_ERROR(error) \
	//printf(error "\n")

#define COMMON_LOG_EXIT_RETURN_I(rc)
#define COMMON_LOG_DEBUG_F(fmt, ...) \
	//printf(fmt "\n", __VA_ARGS__)

#define COMMON_LOG_ERROR_F(fmt, ...)\
	//printf(fmt "\n", __VA_ARGS__)

#define COMMON_LOG_DEBUG(str)
#define COMMON_LOG_EXIT()
#define COMMON_LOG_HANDOFF_F(fmt, ...)

enum bios_emulated_opcode {
	BIOS_EMULATED_COMMAND = 0xFD,
};

enum bios_emulated_command_subop {
	SUBOP_GET_PAYLOAD_SIZE = 0x00,
	SUBOP_WRITE_LARGE_PAYLOAD_INPUT = 0x01,
	SUBOP_READ_LARGE_PAYLOAD_OUTPUT = 0x02,
	SUBOP_GET_BOOT_STATUS = 0x03,
};

/*
* Error codes for the vendor specific DSM command
*/
enum dsm_vendor_error {
   DSM_VENDOR_SUCCESS = 0x0000,
   DSM_VENDOR_ERR_NOT_SUPPORTED = 0x0001,
   DSM_VENDOR_ERR_NONEXISTING = 0x0002,
   DSM_VENDOR_INVALID_INPUT = 0x0003,
   DSM_VENDOR_HW_ERR = 0x0004,
   DSM_VENDOR_RETRY_SUGGESTED = 0x0005,
   DSM_VENDOR_UNKNOWN = 0x0006,
   DSM_VENDOR_SPECIFIC_ERR = 0x0007,
};

#define DSM_ERROR(A)        (A != DSM_VENDOR_SUCCESS)
#define IN_MB_SIZE          (1 << 20)   //!< Size of the OS mailbox large input payload
#define OUT_MB_SIZE         (1 << 20)   //!< Size of the OS mailbox large output payload
#define IN_PAYLOAD_SIZE     (128)       //!< Total size of the input payload registers
#define OUT_PAYLOAD_SIZE    (128)       //!< Total size of the output payload registers

#define MB_COMPLETE 0x1
#define STATUS_MASK 0xFF

#pragma pack(push)
#pragma pack(1)
struct fw_cmd {
   unsigned int InputPayloadSize;
   unsigned int LargeInputPayloadSize;
   unsigned int OutputPayloadSize;
   unsigned int LargeOutputPayloadSize;
   unsigned char InputPayload[IN_PAYLOAD_SIZE];
   unsigned char LargeInputPayload[IN_MB_SIZE];
   unsigned char OutPayload[OUT_PAYLOAD_SIZE];
   unsigned char LargeOutputPayload[OUT_MB_SIZE];
   unsigned int DimmID;
   unsigned char Opcode;
   unsigned char SubOpcode;
   unsigned char Status;
   unsigned char DsmStatus;
};
#pragma pack(pop)

#define DSM_ERROR(A)                  (A != DSM_VENDOR_SUCCESS)

int fw_mb_err_to_nvm_lib_err(int status);
int dsm_err_to_nvm_lib_err(unsigned int status);
#endif // _OS_TYPES_H_
