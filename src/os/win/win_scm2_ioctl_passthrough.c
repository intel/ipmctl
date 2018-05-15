/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "win_scm2_ioctl_passthrough.h"
#include "win_scm2_ioctl.h"
//#include "win_scm2_adapter.h"

#define	IOCTL_CR_PASS_THROUGH CTL_CODE(NVDIMM_IOCTL, SCM_PHYSICAL_DEVICE_FUNCTION(0x14), METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define	SCM_BUILD_DSM_OPCODE(opcode, subop_code) (unsigned int)(subop_code << 8 | opcode)

#define CR_DSM_PASS_THROUGH_IOCTL WIN_SCM2_IOCTL_REQUEST
#define DSM_MAILBOX_ERROR_SHIFT (16)

int win_scm2_ioctl_passthrough_cmd(unsigned short nfit_handle,
		unsigned short op_code, unsigned short sub_op_code,
		void *input_payload, unsigned long input_payload_size,
		void *output_payload, unsigned long output_payload_size,
		unsigned int *p_dsm_status)
{

	int rc = 0;
	*p_dsm_status = 0;
	CR_DSM_PASS_THROUGH_IOCTL ioctl_data;

    // Because the CR_DSM_PASS_THROUGH_IOCTL struct has a byte for the input and output payloads
	// already, we need to subtract those bytes from the total buffer size
	ioctl_data.InputDataSize = sizeof (NVDIMM_PASSTHROUGH_IN) +
			input_payload_size - 1; // minus 1 byte no padding cause the struct is packed
	if(input_payload_size == 0)
		ioctl_data.InputDataSize += 4; // Microsoft driver expects 12 bytes
    ioctl_data.OutputDataSize = sizeof (NVDIMM_PASSTHROUGH_OUT) +
			output_payload_size - 1; // minus 1 byte no padding cause the struct is packed
	if(output_payload_size == 0)
		ioctl_data.OutputDataSize += 4; // Microsoft driver expects 12 bytes

    // Allocate the passthrough input and output buffers
	ioctl_data.pInputData = calloc(1, ioctl_data.InputDataSize);
	ioctl_data.pOutputData = calloc(1, ioctl_data.OutputDataSize);
	NVDIMM_PASSTHROUGH_IN *p_input_data = (NVDIMM_PASSTHROUGH_IN *) ioctl_data.pInputData;
    NVDIMM_PASSTHROUGH_OUT *p_output_data = (NVDIMM_PASSTHROUGH_OUT *) ioctl_data.pOutputData;

	if ((p_input_data) && (p_output_data))
	{
		p_input_data->Version = NVDIMM_PASSTHROUGH_IN_V1;
		p_input_data->Size = (ULONG) ioctl_data.InputDataSize;
		p_input_data->ProtocolGuid = GUID_NVDIMM_PASSTHROUGH_INVDIMM;

		p_input_data->Data.Arg3OpCodeParameterDataLength = input_payload_size;
		p_input_data->Data.Arg3OpCode = SCM_BUILD_DSM_OPCODE(op_code, sub_op_code);

		// minus 1 byte no padding cause the struct is packed
		p_input_data->DataSize = sizeof (DSM_VENDOR_SPECIFIC_COMMAND_INPUT_PAYLOAD)
										 + input_payload_size - 1;
		if (p_input_data->DataSize < 12)
			p_input_data->DataSize = 12; // Microsoft driver expects 12 byte

        // prepare the input data buffer
		if (input_payload_size > 0)
		{
			memmove(p_input_data->Data.Arg3OpCodeParameterDataBuffer,
					input_payload,
					input_payload_size);
		}
		else
		{
			p_input_data->DataSize += 4; // Microsoft driver expects 12 bytes
		}

		enum WIN_SCM2_IOCTL_RETURN_CODES ioctl_rc = win_scm2_ioctl_execute(nfit_handle,
						&ioctl_data, IOCTL_CR_PASS_THROUGH);
		if (!WIN_SCM2_IS_SUCCESS(ioctl_rc))
		{
			rc = (int)ioctl_rc;
		}
		else if (ioctl_data.ReturnCode > 0)
		{
			rc = (int)ioctl_data.ReturnCode;
			SCM_LOG_ERROR_F("Error with passthrough command (%xh, %xh). IOCTL Return Code: 0x%x",
					op_code, sub_op_code, (int)ioctl_data.ReturnCode);
		}
		else if (p_output_data->Data.Arg3Status.DsmStatus != 0)
		{
			unsigned int status = p_output_data->Data.Arg3Status.DsmStatus |
					(p_output_data->Data.Arg3Status.MailboxStatusCode << DSM_MAILBOX_ERROR_SHIFT);
			*p_dsm_status = status;

			SCM_LOG_ERROR_F("Error with FW Command (%xh, %xh). DSM Status: 0x%x",
					op_code, sub_op_code, status);
		}
		else
		{
			if (output_payload_size > 0)
			{
				size_t bytes_to_copy = output_payload_size;
				if (p_output_data->Data.Arg3OutputBufferLength < output_payload_size)
				{
					// User expected more data than the command returned
					// We can safely copy it, but there could be a developer error
					bytes_to_copy = p_output_data->Data.Arg3OutputBufferLength;
				}
				memmove(output_payload, p_output_data->Data.Arg3OutputBuffer, bytes_to_copy);
			}
		}
	}
	else
	{
		rc = -1;
	}

	// Free everything possibly allocated
	if (p_output_data)
		free(p_output_data);
	if (p_input_data)
		free(p_input_data);

	SCM_LOG_EXIT_RETURN_I(rc);

	return rc;
}
