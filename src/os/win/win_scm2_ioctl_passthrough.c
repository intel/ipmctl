/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "win_scm2_ioctl_passthrough.h"
#include "win_scm2_ioctl.h"
#include <winioctl.h>

#define SCM_BUILD_DSM_OPCODE(opcode, subop_code) (unsigned int)(subop_code << 8 | opcode)

#define CR_DSM_PASS_THROUGH_IOCTL WIN_SCM2_IOCTL_REQUEST
#define DSM_MAILBOX_ERROR_SHIFT (16)

// The SCM_PD_PASSTHROUGH_* structs assume a dummy byte (ANYSIZE_ARRAY) for the
// input and output payloads. It ends up getting padded to 4 bytes, and we
// need to subtract those 4 bytes back if the payload has non-zero size
// when we're computing the size of the payload. If it's a zero-size payload, then
// include the 4 bytes.
#define TRUE_SIZEOF_PASSTHROUGH_STRUCT(s) (sizeof(s) - sizeof(DWORD))

// Thanks https://stackoverflow.com/a/3553321
#define member_size(type, member) sizeof(((type *)0)->member)

// Make sure the struct padding is as we expect, with DWORD bytes of padding
// on the end of the struct. Otherwise need to tweak TRUE_SIZEOF_PASSTHROUGH_STRUCT
// above.
C_ASSERT(sizeof(SCM_PD_PASSTHROUGH_INPUT) -
    (member_size(SCM_PD_PASSTHROUGH_INPUT, Version) +
    member_size(SCM_PD_PASSTHROUGH_INPUT, Size) +
    member_size(SCM_PD_PASSTHROUGH_INPUT, ProtocolGuid) +
    member_size(SCM_PD_PASSTHROUGH_INPUT, DataSize)) == sizeof(DWORD));

C_ASSERT(sizeof(SCM_PD_PASSTHROUGH_OUTPUT) -
   (member_size(SCM_PD_PASSTHROUGH_OUTPUT, Version) +
    member_size(SCM_PD_PASSTHROUGH_OUTPUT, Size) +
    member_size(SCM_PD_PASSTHROUGH_OUTPUT, ProtocolGuid) +
    member_size(SCM_PD_PASSTHROUGH_OUTPUT, DataSize)) == sizeof(DWORD));

C_ASSERT(sizeof(SCM_PD_PASSTHROUGH_INVDIMM_INPUT) -
   (member_size(SCM_PD_PASSTHROUGH_INVDIMM_INPUT, Opcode) +
    member_size(SCM_PD_PASSTHROUGH_INVDIMM_INPUT, OpcodeParametersLength)) == sizeof(DWORD));

C_ASSERT(sizeof(SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT) -
   (member_size(SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT, GeneralStatus) +
    member_size(SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT, ExtendedStatus) +
    member_size(SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT, OutputDataLength)) == sizeof(DWORD));

int win_scm2_ioctl_passthrough_cmd(unsigned short nfit_handle,
    unsigned short op_code, unsigned short sub_op_code,
    void *input_payload, unsigned long input_payload_size,
    void *output_payload, unsigned long output_payload_size,
    unsigned int *p_dsm_status)
{

  int rc = 0;
  *p_dsm_status = 0;
  CR_DSM_PASS_THROUGH_IOCTL ioctl_data;

  ioctl_data.InputDataSize = TRUE_SIZEOF_PASSTHROUGH_STRUCT(SCM_PD_PASSTHROUGH_INPUT) +
    TRUE_SIZEOF_PASSTHROUGH_STRUCT(SCM_PD_PASSTHROUGH_INVDIMM_INPUT) + input_payload_size;
  if (input_payload_size == 0) {
    ioctl_data.InputDataSize += sizeof(DWORD); // Microsoft driver expects 12 bytes
  }
  ioctl_data.OutputDataSize = TRUE_SIZEOF_PASSTHROUGH_STRUCT(SCM_PD_PASSTHROUGH_OUTPUT) +
    TRUE_SIZEOF_PASSTHROUGH_STRUCT(SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT) + output_payload_size;
  if (output_payload_size == 0) {
    ioctl_data.OutputDataSize += sizeof(DWORD); // Microsoft driver expects 12 bytes
  }

  // Allocate the passthrough input and output buffers
  ioctl_data.pInputData = calloc(1, ioctl_data.InputDataSize);
  ioctl_data.pOutputData = calloc(1, ioctl_data.OutputDataSize);
  SCM_PD_PASSTHROUGH_INPUT *p_input_data = (SCM_PD_PASSTHROUGH_INPUT *) ioctl_data.pInputData;
  SCM_PD_PASSTHROUGH_OUTPUT *p_output_data = (SCM_PD_PASSTHROUGH_OUTPUT *) ioctl_data.pOutputData;

  if ((p_input_data) && (p_output_data)) {
    p_input_data->Version = sizeof(SCM_PD_PASSTHROUGH_INPUT);
    p_input_data->Size = (ULONG) ioctl_data.InputDataSize;
    p_input_data->ProtocolGuid = GUID_SCM_PD_PASSTHROUGH_INVDIMM;

    ((SCM_PD_PASSTHROUGH_INVDIMM_INPUT *)(p_input_data->Data))->OpcodeParametersLength = input_payload_size;
    ((SCM_PD_PASSTHROUGH_INVDIMM_INPUT *)(p_input_data->Data))->Opcode = SCM_BUILD_DSM_OPCODE(op_code, sub_op_code);

    p_input_data->DataSize = TRUE_SIZEOF_PASSTHROUGH_STRUCT(SCM_PD_PASSTHROUGH_INVDIMM_INPUT)
      + input_payload_size;

    // prepare the input data buffer
    if (input_payload_size > 0) {
      memmove(((SCM_PD_PASSTHROUGH_INVDIMM_INPUT *)(p_input_data->Data))->OpcodeParameters,
        input_payload,
        input_payload_size);
    } else {
      p_input_data->DataSize += sizeof(DWORD); // Microsoft driver expects 12 bytes
    }

    enum WIN_SCM2_IOCTL_RETURN_CODES ioctl_rc = win_scm2_ioctl_execute(nfit_handle,
            &ioctl_data, IOCTL_SCM_PD_PASSTHROUGH);
    if (!WIN_SCM2_IS_SUCCESS(ioctl_rc)) {
      rc = (int)ioctl_rc;
    } else if (ioctl_data.ReturnCode > 0) {
      rc = (int)ioctl_data.ReturnCode;
      SCM_LOG_ERROR_F("Error with passthrough command (%xh, %xh). IOCTL Return Code: 0x%x",
        op_code, sub_op_code, (int)ioctl_data.ReturnCode);
    } else if (((SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT *)(p_output_data->Data))->GeneralStatus != 0) {
      unsigned int status = ((SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT *)(p_output_data->Data))->GeneralStatus |
        (((SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT *)(p_output_data->Data))->ExtendedStatus << DSM_MAILBOX_ERROR_SHIFT);
      *p_dsm_status = status;
      SCM_LOG_ERROR_F("Error with FW Command (%xh, %xh). DSM Status: 0x%x",
        op_code, sub_op_code, status);
    } else {
      if (output_payload_size > 0) {
        size_t bytes_to_copy = output_payload_size;
        if (((SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT *)(p_output_data->Data))->OutputDataLength < output_payload_size) {
          // User expected more data than the command returned
          // We can safely copy it, but there could be a developer error
          bytes_to_copy = ((SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT *)(p_output_data->Data))->OutputDataLength;
        }
        memmove(output_payload, ((SCM_PD_PASSTHROUGH_INVDIMM_OUTPUT *)(p_output_data->Data))->OutputData,
            bytes_to_copy);
      }
    }
  } else {
    rc = -1;
  }

  // Free everything possibly allocated
  if (p_output_data) {
    free(p_output_data);
  }
  if (p_input_data) {
    free(p_input_data);
  }

  SCM_LOG_EXIT_RETURN_I(rc);

  return rc;
}
