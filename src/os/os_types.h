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
	printf(error "\n")

#define COMMON_LOG_EXIT_RETURN_I(rc)
#define COMMON_LOG_DEBUG_F(fmt, ...) \
	printf(fmt "\n", __VA_ARGS__)

#define COMMON_LOG_ERROR_F(fmt, ...)\
	printf(fmt "\n", __VA_ARGS__)

#define COMMON_LOG_DEBUG(str)
#define COMMON_LOG_EXIT()
#define COMMON_LOG_HANDOFF_F(fmt, ...)

#define	COMMON_LOG_WARN_F(fmt, ...)

#define SYSTEM_EVENT_NOT_APPLICABLE 0

#define SYSTEM_EVENT_TO_MASK(x) (1<<x)

#define SYSTEM_EVENT_INFO_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_INFO)
#define SYSTEM_EVENT_WARNING_MASK	SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_WARNING)
#define SYSTEM_EVENT_ERROR_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_ERROR)
#define SYSTEM_EVENT_DEBUG_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_TYPE_DEBUG)
#define SYSTEM_EVENT_ALL_MASK       (SYSTEM_EVENT_INFO_MASK | SYSTEM_EVENT_WARNING_MASK | SYSTEM_EVENT_ERROR_MASK | SYSTEM_EVENT_DEBUG_MASK)

#define SYSTEM_EVENT_CAT_DIAG_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_DIAG)
#define SYSTEM_EVENT_CAT_FW_MASK	    SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_FW)
#define SYSTEM_EVENT_CAT_CONFIG_MASK	SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_CONFIG)
#define SYSTEM_EVENT_CAT_PM_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_PM)
#define SYSTEM_EVENT_CAT_QUICK_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_QUICK)
#define SYSTEM_EVENT_CAT_SECURITY_MASK	SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_SECURITY)
#define SYSTEM_EVENT_CAT_HEALTH_MASK	SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_HEALTH)
#define SYSTEM_EVENT_CAT_MGMT_MASK		SYSTEM_EVENT_TO_MASK(SYSTEM_EVENT_CAT_MGMT)
#define SYSTEM_EVENT_CAT_ALL_MASK       (SYSTEM_EVENT_CAT_DIAG_MASK | SYSTEM_EVENT_CAT_FW_MASK | SYSTEM_EVENT_CAT_CONFIG_MASK | SYSTEM_EVENT_CAT_PM_MASK | \
                                        SYSTEM_EVENT_CAT_QUICK_MASK | SYSTEM_EVENT_CAT_SECURITY_MASK | SYSTEM_EVENT_CAT_HEALTH_MASK | SYSTEM_EVENT_CAT_MGMT_MASK)

#define MAX_TIMESTAMP_LEN       23 + 1   // mm/dd/yyyy hh:mm:ss.mmm

#define SYSTEM_LOG_MAX_INI_ENTRY_SIZE 40
#define SYSTEM_LOG_EVENT_FILE_NAME "EVENT_LOG_FILE_NAME"
#define SYSTEM_LOG_DEBUG_FILE_NAME "DBG_LOG_FILE_NAME"
#define SYSTEM_LOG_EVENT_LIMIT "EVENT_LOG_MAX"
#define SYSTEM_LOG_DEBUG_LIMIT "DBG_LOG_MAX"
#define SYSTEM_LOG_FILE_NAME_MAX_LEN 256
#define SYSTEM_LOG_CODE_STRING_SIZE 4
#define ENVIRONMENT_VARIABLE_MAX_LEN 64

/*!
* An enumeration set describing system event types
* Don't extand without changing the Event_Type structure - see event.h
*/
enum system_event_type
{
	SYSTEM_EVENT_TYPE_INFO = 0,	//!< Informational event
	SYSTEM_EVENT_TYPE_WARNING = 1,	//!< Warning event
	SYSTEM_EVENT_TYPE_ERROR = 2,	//!< Error event
	SYSTEM_EVENT_TYPE_DEBUG = 3 //!< Debug event
};

/*!
* An enumeration set describing system event categories
* Don't extand without changing the Event_Type structure - see event.h
*/
enum system_event_category
{
    SYSTEM_EVENT_CAT_DIAG = 0,  //!< Diagnostic test events
    SYSTEM_EVENT_CAT_FW = 1,    //!< FW consistency diagnostic test events
    SYSTEM_EVENT_CAT_CONFIG = 2, //!< Platform config diagnostic test events
    SYSTEM_EVENT_CAT_PM = 3, //!< PM metadata diagnostic test events
    SYSTEM_EVENT_CAT_QUICK = 4, //!< Quick diagnostic test events
    SYSTEM_EVENT_CAT_SECURITY = 5, //!< Secuirty diagnostic test events
    SYSTEM_EVENT_CAT_HEALTH = 6, //!< Device health events
    SYSTEM_EVENT_CAT_MGMT = 7 //!< Management software generated events
};

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
};
#pragma pack(pop)

int fw_mb_err_to_nvm_lib_err(int status, struct fw_cmd *p_fw_cmd);
int dsm_err_to_nvm_lib_err(unsigned int status, struct fw_cmd *p_fw_cmd);
#endif // _OS_TYPES_H_
