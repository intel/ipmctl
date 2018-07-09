/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file implements the Linux driver adapter interface for issuing IOCTL
 * passthrough commands.
 */

//#include "device_adapter.h"
#include "lnx_adapter_logging.h"
#include "lnx_adapter.h"
#include "lnx_adapter_passthrough.h"
//#include <os/os_adapter.h>
#include <string.h>
#include <stdlib.h>
#include <os_types.h>
#define DEV_SMALL_PAYLOAD_SIZE	128 /* 128B - Size for a passthrough command small payload */


#define	BIOS_INPUT(NAME, IN_LEN)		\
struct NAME {								\
	UINT32 size;						\
	UINT32 offset;						\
	UINT8  buffer[IN_LEN];				\
}

/*
* Helper function to translate block driver errors to NVM Lib errors.
*/
int linux_err_to_nvm_lib_err(int crbd_err)
{
	COMMON_LOG_ENTRY();
	int ret = NVM_SUCCESS;
	if (crbd_err < 0)
	{
		switch (crbd_err)
		{
		case -EACCES:
			ret = NVM_ERR_INVALID_PERMISSIONS;
			break;
		case -EBADF:
			ret = NVM_ERR_BAD_DEVICE;
			break;
		case -EBUSY:
			ret = NVM_ERR_BUSY_DEVICE;
			break;
		case -EFAULT:
			ret = NVM_ERR_UNKNOWN;
			break;
		case -EINVAL:
			ret = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
			break;
		case -ENODEV:
			ret = NVM_ERR_BAD_DEVICE;
			break;
		case -ENOMEM:
			ret = NVM_ERR_NO_MEM;
			break;
		case -ENOSPC:
			ret = NVM_ERR_BAD_SIZE;
			break;
		case -ENOTTY:
			ret = NVM_ERR_UNKNOWN;
			break;
		case -EPERM:
			ret = NVM_ERR_INVALID_SECURITY_STATE;
			break;
		default:
			ret = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
		}
		COMMON_LOG_ERROR_F("Linux driver error converted to lib error = %d", ret);
	}
	COMMON_LOG_EXIT_RETURN_I(ret);
	return (ret);
}

/*
 * Execute an emulated BIOS ioctl to retrieve information about the bios large mailboxes
 */
int bios_get_payload_size(struct ndctl_dimm *p_dimm, struct pt_bios_get_size *p_bios_mb_size,
		struct fw_cmd *p_fw_cmd)
{
	COMMON_LOG_ENTRY();
	int rc = NVM_SUCCESS;
	int lnx_err_status = 0;
	unsigned int dsm_vendor_err_status = 0;

	if (p_dimm == NULL)
	{
		COMMON_LOG_ERROR("Invalid parameter, Dimm is null");
		rc = NVM_ERR_INVALID_PARAMETER;
	}
	else if (p_bios_mb_size == NULL)
	{
		COMMON_LOG_ERROR("Invalid parameter, size struct is null");
		rc = NVM_ERR_INVALID_PARAMETER;
	}
	else
	{
		memset(p_bios_mb_size, 0, sizeof (*p_bios_mb_size));
		struct ndctl_cmd *p_vendor_cmd = NULL;
		if ((p_vendor_cmd = ndctl_dimm_cmd_new_vendor_specific(p_dimm,
			BUILD_DSM_OPCODE(BIOS_EMULATED_COMMAND, SUBOP_GET_PAYLOAD_SIZE), 128,
			sizeof (struct pt_bios_get_size))) == NULL)
		{
			rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
			COMMON_LOG_ERROR("Failed to get vendor command from driver");
		}
		else
		{
			if ((lnx_err_status = ndctl_cmd_submit(p_vendor_cmd)) == 0)
			{
				if ((dsm_vendor_err_status = ndctl_cmd_get_firmware_status(p_vendor_cmd))
						!= DSM_VENDOR_SUCCESS)
				{
					rc = dsm_err_to_nvm_lib_err(dsm_vendor_err_status, p_fw_cmd);
					COMMON_LOG_ERROR_F("BIOS get failed:DSM returned error %d for command with "
							"Opcode - 0x%x SubOpcode - 0x%x ", dsm_vendor_err_status,
							p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
				}
				else
				{
					size_t return_size = ndctl_cmd_vendor_get_output(p_vendor_cmd,
						p_bios_mb_size, sizeof (struct pt_bios_get_size));
					if (return_size != sizeof (struct pt_bios_get_size))
					{
						rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
						COMMON_LOG_ERROR("Small Payload returned less data than requested");
					}
				}
			}
			else
			{
				rc = linux_err_to_nvm_lib_err(lnx_err_status);
				COMMON_LOG_ERROR_F("BIOS get failed: "
						"Linux driver returned error %d for command with "
								"Opcode- 0x%x SubOpcode- 0x%x ", lnx_err_status,
										p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
			}
			ndctl_cmd_unref(p_vendor_cmd);
		}
	}
	COMMON_LOG_EXIT_RETURN_I(rc);
	return rc;
}

/*
 * Populate the emulated bios large input mailbox
 */
int bios_write_large_payload(struct ndctl_dimm *p_dimm, struct fw_cmd *p_fw_cmd)
{
	COMMON_LOG_ENTRY();
	int rc = NVM_SUCCESS;
	struct pt_bios_get_size mb_size;

	if (!p_dimm)
	{
		COMMON_LOG_ERROR("Invalid parameter, Dimm is null");
		rc = NVM_ERR_INVALID_PARAMETER;
	}
	else if ((rc = bios_get_payload_size(p_dimm, &mb_size, p_fw_cmd)) == NVM_SUCCESS)
	{
		if (mb_size.large_input_payload_size < p_fw_cmd->LargeInputPayloadSize)
		{
			rc = NVM_ERR_BAD_SIZE;
		}
		else
		{
			unsigned int transfer_size = mb_size.rw_size;
			unsigned int current_offset = 0;

			while (current_offset < p_fw_cmd->LargeInputPayloadSize &&
					rc == NVM_SUCCESS)
			{
				if ((current_offset + mb_size.rw_size) > p_fw_cmd->LargeInputPayloadSize)
				{
					transfer_size = p_fw_cmd->LargeInputPayloadSize - current_offset;
				}

				BIOS_INPUT(bios_InputPayload, transfer_size);
				struct bios_InputPayload *p_dsm_input = calloc(1,
					sizeof (struct bios_InputPayload));
				if (p_dsm_input)
				{
					struct ndctl_cmd *p_vendor_cmd = NULL;
					if ((p_vendor_cmd = ndctl_dimm_cmd_new_vendor_specific(
							p_dimm, BUILD_DSM_OPCODE(BIOS_EMULATED_COMMAND,
							SUBOP_WRITE_LARGE_PAYLOAD_INPUT),
							sizeof (*p_dsm_input), 0)) == NULL)
					{
						COMMON_LOG_ERROR("Failed to get vendor command from driver");
						rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
					}
					else
					{
						int lnx_err_status = 0;
						unsigned int dsm_vendor_err_status = 0;

						p_dsm_input->size = transfer_size;
						p_dsm_input->offset = current_offset;

						memmove(p_dsm_input->buffer,
							p_fw_cmd->LargeInputPayload + current_offset,
							sizeof (p_dsm_input->buffer));

						size_t bytes_written = ndctl_cmd_vendor_set_input(
							p_vendor_cmd, p_dsm_input, sizeof (*p_dsm_input));

						if (bytes_written != sizeof (*p_dsm_input))
						{
							COMMON_LOG_ERROR("Failed to write input payload");
							rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
						}
						else
						{
							if ((lnx_err_status = ndctl_cmd_submit(p_vendor_cmd)) == 0)
							{
								if ((dsm_vendor_err_status =
										ndctl_cmd_get_firmware_status(p_vendor_cmd))
												!= DSM_VENDOR_SUCCESS)
								{
									rc = dsm_err_to_nvm_lib_err(dsm_vendor_err_status, p_fw_cmd);
									COMMON_LOG_ERROR_F("BIOS write failed: "
											"DSM returned error %d for command with "
											"Opcode- 0x%x SubOpcode- 0x%x ", dsm_vendor_err_status,
											p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
								}
								else
								{
										current_offset += transfer_size;
								}

							}
							else
							{
								rc = linux_err_to_nvm_lib_err(lnx_err_status);
								COMMON_LOG_ERROR_F("BIOS write failed: "
										"Linux driver returned error %d for command with "
											"Opcode- 0x%x SubOpcode- 0x%x ", lnx_err_status,
												p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);

							}
						}

						ndctl_cmd_unref(p_vendor_cmd);
					}
					free(p_dsm_input);
				}
				else
				{
					COMMON_LOG_ERROR("Failed to allocate memory for BIOS input payload");
					rc = NVM_ERR_NO_MEM;
				}
			} // end while

			if (current_offset != p_fw_cmd->LargeInputPayloadSize)
			{
				COMMON_LOG_ERROR("Failed to write large payload");
				rc = NVM_ERR_UNKNOWN;
			}
		}
	}

	COMMON_LOG_EXIT_RETURN_I(rc);
	return rc;
}

/*
 * Read the emulated bios large output mailbox
 */
int bios_read_large_payload(struct ndctl_dimm *p_dimm, struct fw_cmd *p_fw_cmd)
{
	COMMON_LOG_ENTRY();
	int rc = NVM_SUCCESS;
	struct pt_bios_get_size mb_size;

	if (!p_dimm)
	{
		COMMON_LOG_ERROR("Invalid parameter, Dimm is null");
		rc = NVM_ERR_INVALID_PARAMETER;
	}
	else if ((rc = bios_get_payload_size(p_dimm, &mb_size, p_fw_cmd)) == NVM_SUCCESS)
	{
		if (mb_size.large_input_payload_size < p_fw_cmd->LargeOutputPayloadSize)
		{
			rc = NVM_ERR_BAD_SIZE;
		}
		else
		{
			unsigned int transfer_size = mb_size.rw_size;
			unsigned int current_offset = 0;
			int lnx_err_status = 0;
			unsigned int dsm_vendor_err_status = 0;

			rc = NVM_SUCCESS;
			while (current_offset < p_fw_cmd->LargeOutputPayloadSize &&
					rc == NVM_SUCCESS)
			{
				if ((current_offset + mb_size.rw_size) > p_fw_cmd->LargeOutputPayloadSize)
				{
					transfer_size = p_fw_cmd->LargeOutputPayloadSize - current_offset;
				}

				BIOS_INPUT(bios_InputPayload, 0);
				struct bios_InputPayload *p_dsm_input =
					calloc(1, sizeof (struct bios_InputPayload));
				if (p_dsm_input)
				{
					struct ndctl_cmd *p_vendor_cmd = NULL;
					if ((p_vendor_cmd = ndctl_dimm_cmd_new_vendor_specific(p_dimm,
						BUILD_DSM_OPCODE(BIOS_EMULATED_COMMAND, SUBOP_READ_LARGE_PAYLOAD_OUTPUT),
						sizeof (*p_dsm_input), transfer_size)) == NULL)
					{
						rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
						COMMON_LOG_ERROR("Failed to get vendor command from driver");
					}
					else
					{
						p_dsm_input->size = transfer_size;
						p_dsm_input->offset = current_offset;

						size_t bytes_written = ndctl_cmd_vendor_set_input(
							p_vendor_cmd, p_dsm_input, sizeof (*p_dsm_input));
						if (bytes_written != sizeof (*p_dsm_input))
						{
							COMMON_LOG_ERROR("Failed to write input payload");
							rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
						}

						if ((lnx_err_status = ndctl_cmd_submit(p_vendor_cmd)) == 0)
						{
							if ((dsm_vendor_err_status =
									ndctl_cmd_get_firmware_status(p_vendor_cmd)) !=
											DSM_VENDOR_SUCCESS)
							{
								rc = dsm_err_to_nvm_lib_err(dsm_vendor_err_status, p_fw_cmd);
								COMMON_LOG_ERROR_F("BIOS read failed: "
										"DSM returned error %d for command with "
										"Opcode - 0x%x SubOpcode - 0x%x ", dsm_vendor_err_status,
										p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
							}
							else
							{
								size_t return_size = ndctl_cmd_vendor_get_output(p_vendor_cmd,
										p_fw_cmd->LargeOutputPayload +
										current_offset, transfer_size);
								if (return_size != transfer_size)
								{
									rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
									COMMON_LOG_ERROR("Large Payload returned "
											"less data than requested");
								}
								else
								{
									current_offset += transfer_size;
								}
							}
						}
						else
						{
							rc = linux_err_to_nvm_lib_err(lnx_err_status);
							COMMON_LOG_ERROR_F("BIOS read failed: "
									"Linux driver returned error %d for command with "
									"Opcode - 0x%x SubOpcode - 0x%x ", lnx_err_status,
									p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
						}
						ndctl_cmd_unref(p_vendor_cmd);
					}
					free(p_dsm_input);
				}
				else
				{
					COMMON_LOG_ERROR("Failed to allocate memory for BIOS payload");
					rc = NVM_ERR_NO_MEM;
				}
			}  // end while

			if (current_offset != p_fw_cmd->LargeOutputPayloadSize)
			{
				COMMON_LOG_ERROR("Failed to read large payload");
				rc = NVM_ERR_UNKNOWN;
			}
		}
	}

	COMMON_LOG_EXIT_RETURN_I(rc);
	return rc;
}

int get_dimm_by_handle(struct ndctl_ctx *ctx, unsigned int handle, struct ndctl_dimm **dimm)
{
	COMMON_LOG_ENTRY();
	int rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
	struct ndctl_dimm *target_dimm;
	*dimm = NULL;

	struct ndctl_bus *bus;
	ndctl_bus_foreach(ctx, bus)
	{
		target_dimm = ndctl_dimm_get_by_handle(bus, handle);

		if (target_dimm)
		{
			*dimm = target_dimm;
			rc = NVM_SUCCESS;
			break;
		}
	}

	if (*dimm == NULL)
	{
		COMMON_LOG_ERROR("Failed to get DIMM from driver");
	}

	COMMON_LOG_EXIT_RETURN_I(rc);
	return rc;
}

/*
 * Execute a passthrough IOCTL
 */
int ioctl_passthrough_fw_cmd(struct fw_cmd *p_fw_cmd)
{
	COMMON_LOG_ENTRY();
	int rc = NVM_SUCCESS;
	struct ndctl_ctx *ctx;
	int retry = 0;

	// check input parameters
	if (p_fw_cmd == NULL)
	{
		COMMON_LOG_ERROR("Invalid parameter, cmd struct is null");
		rc = NVM_ERR_UNKNOWN;
	}
	else if ((p_fw_cmd->InputPayloadSize > 0 && p_fw_cmd->InputPayload == NULL) ||
			(p_fw_cmd->OutputPayloadSize > 0 && p_fw_cmd->OutPayload == NULL) ||
			(p_fw_cmd->LargeInputPayloadSize > 0 && p_fw_cmd->LargeInputPayload == NULL) ||
			(p_fw_cmd->LargeOutputPayloadSize > 0 && p_fw_cmd->LargeOutputPayload == NULL))
	{
		COMMON_LOG_ERROR("Invalid input or output payloads specified");
		rc = NVM_ERR_UNKNOWN;
	}
#if __LARGE_PAYLOAD_NOT_SUPPORTED__
	else if ((p_fw_cmd->Opcode == 0x08 && p_fw_cmd->SubOpcode == 0x02) || // get fw debug log
				(p_fw_cmd->Opcode == 0x0A)) // inject error
	{
		COMMON_LOG_ERROR_F("Intel DC PMM FW command OpCode: 0x%x SubOpCode: "
				"0x%x is not supported",
				p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
		rc = NVM_LIB_ERR_NOTSUPPORTED;
	}
#endif
	else if ((rc = ndctl_new(&ctx)) < 0)
	{
		COMMON_LOG_ERROR("Failed to retrieve ctx");
		rc = linux_err_to_nvm_lib_err(rc);
	}
	else
	{
		struct ndctl_dimm *p_dimm = NULL;
		if ((rc = get_dimm_by_handle(ctx, p_fw_cmd->DimmID, &p_dimm)) == NVM_SUCCESS)
		{
			unsigned int Opcode = BUILD_DSM_OPCODE(p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
			struct ndctl_cmd *p_vendor_cmd = NULL;
			if ((p_vendor_cmd = ndctl_dimm_cmd_new_vendor_specific(
					p_dimm, Opcode, DEV_SMALL_PAYLOAD_SIZE,
					DEV_SMALL_PAYLOAD_SIZE)) == NULL)
			{
				rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
				COMMON_LOG_ERROR("Failed to get vendor command from driver");
			}
			else
			{
				while (retry < DSM_MAX_RETRIES)
				{
					int lnx_err_status = 0;
					unsigned int dsm_vendor_err_status = 0;

					if (p_fw_cmd->InputPayloadSize > 0)
					{
						size_t bytes_written = ndctl_cmd_vendor_set_input(p_vendor_cmd,
							p_fw_cmd->InputPayload, p_fw_cmd->InputPayloadSize);

						if (bytes_written != p_fw_cmd->InputPayloadSize)
						{
							COMMON_LOG_ERROR("Failed to write input payload");
							rc = NVM_ERR_GENERAL_OS_DRIVER_FAILURE;
							break;
						}
					}

					if (p_fw_cmd->LargeInputPayloadSize > 0)
					{
						rc = bios_write_large_payload(p_dimm, p_fw_cmd);
						if (rc != NVM_SUCCESS)
						{
							break;
						}
					}

					COMMON_LOG_HANDOFF_F("Passthrough IOCTL. Opcode: 0x%x, SubOpcode: 0x%x",
						p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
					if (p_fw_cmd->InputPayloadSize)
					{
						// Print one DWORD at a time starting from LSB of InputPayload
						for (int i = 0; i < p_fw_cmd->InputPayloadSize / sizeof(UINT32); i++)
						{
							// Make sure entire DWORD gets printed
							COMMON_LOG_HANDOFF_F("Input[%d]: 0x%.8x",
								i, ((UINT32 *) (p_fw_cmd->InputPayload))[i]);
						}
					}

					if ((lnx_err_status = ndctl_cmd_submit(p_vendor_cmd)) >= 0)
					{
						// BSR returns 0x78, but everything else seems to indicate the
						// command was a success. Going
						// to ignore the result for now. If there was a real error,
						// the fw_status should have it.
						dsm_vendor_err_status =	ndctl_cmd_get_firmware_status(p_vendor_cmd);

						if (dsm_vendor_err_status == DSM_VENDOR_RETRY_SUGGESTED)
						{
							rc = dsm_err_to_nvm_lib_err(dsm_vendor_err_status, p_fw_cmd);
							COMMON_LOG_ERROR_F("RETRY %i IOCTL passthrough failed: "
								"DSM returned error %d for command with "
										"Opcode - 0x%x SubOpcode - 0x%x \n", retry, dsm_vendor_err_status,
											p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
							retry++;
							continue;
						}
						else if (dsm_vendor_err_status != DSM_VENDOR_SUCCESS)
						{
							rc = dsm_err_to_nvm_lib_err(dsm_vendor_err_status, p_fw_cmd);
							COMMON_LOG_ERROR_F("IOCTL passthrough failed: "
								"DSM returned error %d for command with "
										"Opcode - 0x%x SubOpcode - 0x%x \n", dsm_vendor_err_status,
											p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
							break;
						}
						else
						{
							if (p_fw_cmd->OutputPayloadSize > 0)
							{
								ndctl_cmd_vendor_get_output(p_vendor_cmd,
											p_fw_cmd->OutPayload,
												p_fw_cmd->OutputPayloadSize);
							}

							if (p_fw_cmd->LargeOutputPayloadSize > 0)
							{

								rc = bios_read_large_payload(p_dimm, p_fw_cmd);
							}
							break;
						}
					}
					else
					{
						rc = linux_err_to_nvm_lib_err(lnx_err_status);
						COMMON_LOG_ERROR_F("IOCTL passthrough failed "
								"Linux driver returned error %d for command with "
								"Opcode- 0x%x SubOpcode- 0x%x ", lnx_err_status,
								p_fw_cmd->Opcode, p_fw_cmd->SubOpcode);
						break;
					}
				}
				ndctl_cmd_unref(p_vendor_cmd);
			}
		}
		ndctl_unref(ctx);
	}

	memset(&p_fw_cmd, 0, sizeof(p_fw_cmd));
	COMMON_LOG_EXIT_RETURN_I(rc);
	return rc;
}
