/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "os_types.h"
#include "NvmSharedDefs.h"


/*
* Mail Box error codes
*
* These are returned from the device driver when it completes a pass through
* command with an error state from the firmware.
*/
enum mb_error {
   MB_SUCCESS = 0x00, /* Command was successfully completed */
   /* An input parameter was not valid */
   MB_INVALID_CMD_PARAM = 0x01,
   /* There was an error in the data transfer */
   MB_DATA_XFER_ERR = 0x02,
   /* There was an internal device error */
   MB_INTERNAL_DEV_ERR = 0x03,
   /* The command opcode or subopcode was not recognized */
   MB_UNSUPPORTED_CMD = 0x04,
   /* Device is currently busy processing a long operation */
   MB_DEVICE_BUSY = 0x05,
   /* Passphrase or Security Nonce does is not acceptable */
   MB_INVALID_CREDENTIAL = 0x06,
   /* The Security CHeck on the image has failed verification */
   MB_SECURITY_CHK_FAIL = 0x07,
   /* Operation is valid in the current security state */
   MB_INVALID_SECURITY_STATE = 0x08,
   /* The system time has not been set yet */
   MB_SYSTEM_TIME_NOT_SET = 0x09,
   /* Returned if "get data" is called before "set data" */
   MB_DATA_NOT_SET = 0x0A,
   /* Command has been aborted. A long operation command has aborted. */
   MB_ABORTED = 0x0B,
   /* REMOVED in FIS 1.6! Execute FW was called prior to uploading new FW image. */
   RESERVED = 0x0C,
   /* Illegal rollback failure. */
   MB_REVISION_FAILURE = 0x0D,
   /* Error injection is not currently enabled on the device. */
   MB_INJECTION_DISABLED = 0x0E,
   /* During configuration lockdown commands that modify config will return this error */
   MB_CONFIG_LOCKED_COMMAND_INVALID = 0x0F,
   /* Invalid Paramter Alignment */
   MB_INVALID_ALIGNMENT = 0x10,
   /* Command is not legally allowed in this mode of the dimm */
   MB_INCOMPATIBLE_DIMM = 0x11,
   /* FW timed out on HW components returning in a timely manner */
   MB_TIMED_OUT = 0x12,
   /* The media on the dimm has been disabled due to critical or other failure */
   MB_MEDIA_DISABLED = 0x14,
   /* After a successfull FW Update, another FW update is being made */
   MB_FW_UPDATE_ALREADY_OCCURED = 0x15,
   /* The FW could not acquire resources required for the particular command */
   MB_NO_RESOURCES_AVAILABLE = 0x16,
};


int fw_mb_err_to_nvm_lib_err(int status, struct fw_cmd *p_fw_cmd)
{
	COMMON_LOG_ENTRY();
	int ret = NVM_SUCCESS;

   p_fw_cmd->Status = DSM_EXTENDED_ERROR(status);
	switch (DSM_EXTENDED_ERROR(status))
	{
	case MB_SUCCESS:
		// This function is only called if _DSM error code != SUCCESS.
		// Here 0x0 means MB code was not updated due to _DSM timeout.
		ret = NVM_ERR_TIMEOUT;
		break;
	case MB_INVALID_CMD_PARAM:
		ret = NVM_ERR_INVALID_PARAMETER;
		break;
	case MB_DATA_XFER_ERR:
		ret = NVM_ERR_DATA_TRANSFER;
		break;
	case MB_INTERNAL_DEV_ERR:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	case MB_UNSUPPORTED_CMD:
		ret = NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU;
		break;
	case MB_DEVICE_BUSY:
		ret = NVM_ERR_BUSY_DEVICE;
		break;
	case MB_INVALID_CREDENTIAL:
		ret = NVM_ERR_INVALID_PASSPHRASE;
		break;
	case MB_SECURITY_CHK_FAIL:
		ret = NVM_ERR_BAD_FW;
		break;
	case MB_INVALID_SECURITY_STATE:
		ret = NVM_ERR_INVALID_SECURITY_STATE;
		break;
	case MB_SYSTEM_TIME_NOT_SET:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	case MB_DATA_NOT_SET:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	case MB_ABORTED:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	case MB_REVISION_FAILURE:
		ret = NVM_ERR_BAD_FW;
		break;
	case MB_INJECTION_DISABLED:
		ret = NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU;
		break;
	case MB_CONFIG_LOCKED_COMMAND_INVALID:
		ret = NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU;
		break;
	case MB_INVALID_ALIGNMENT:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	case MB_INCOMPATIBLE_DIMM:
		ret = NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU;
		break;
	case MB_TIMED_OUT:
		ret = NVM_ERR_BUSY_DEVICE;
		break;
	case MB_MEDIA_DISABLED:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	case MB_FW_UPDATE_ALREADY_OCCURED:
		ret = NVM_ERR_FIRMWARE_ALREADY_LOADED;
		break;
	case MB_NO_RESOURCES_AVAILABLE:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
		break;
	default:
		ret = NVM_ERR_GENERAL_DEV_FAILURE;
	}
	COMMON_LOG_EXIT_RETURN_I(ret);
	return (ret);
}


int dsm_err_to_nvm_lib_err(unsigned int status, struct fw_cmd *p_fw_cmd)
{
	COMMON_LOG_ENTRY();
	int rc = NVM_SUCCESS;

	if (status)
	{
		switch (DSM_VENDOR_ERROR(status))
		{
		case DSM_VENDOR_ERR_NOT_SUPPORTED:
			rc = NVM_ERR_COMMAND_NOT_SUPPORTED_BY_THIS_SKU;
			break;
		case DSM_VENDOR_ERR_NONEXISTING:
			rc = NVM_ERR_BAD_DEVICE;
			break;
		case DSM_VENDOR_INVALID_INPUT:
			rc = NVM_ERR_INVALID_PARAMETER;
			break;
		case DSM_VENDOR_HW_ERR:
			rc = NVM_ERR_GENERAL_DEV_FAILURE;
			break;
		case DSM_VENDOR_RETRY_SUGGESTED:
			rc = NVM_ERR_RETRY_SUGGESTED;
			break;
		case DSM_VENDOR_UNKNOWN:
			rc = NVM_ERR_UNKNOWN;
			break;
		case DSM_VENDOR_SPECIFIC_ERR:
			rc = fw_mb_err_to_nvm_lib_err(status, p_fw_cmd);
			break;
		default:
			rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
			break;
		}
	}

	COMMON_LOG_EXIT_RETURN_I(rc);
	return rc;
}
