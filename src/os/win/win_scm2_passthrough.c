/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "win_scm2_passthrough.h"
#include "win_scm2_ioctl_passthrough.h"
//#include "win_scm2_adapter.h"

static int do_passthrough(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    unsigned char opcode, unsigned char sub_op_code,
    void *input_payload, unsigned int input_payload_size,
    void *output_payload, unsigned int output_payload_size);
static int do_passthrough_fix_output(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    unsigned char opcode, unsigned char sub_op_code,
    void *input_payload, unsigned int input_payload_size,
    void *output_payload, unsigned int output_payload_size);
static int get_large_payload_sizes(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    struct pt_bios_get_size *p_size);
static int read_large_ouptut_payload(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    void *large_output_payload, unsigned int large_output_payload_size);
static int write_large_input_payload(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    void *large_input_payload,
    unsigned int large_input_payload_size);

#define	NO_ERRORS(scm_err, p_dsm_status) ((scm_err == 0) && ((*p_dsm_status) == 0))
#define	IS_BIOS_EMULATED_COMMAND(opcode) (opcode) == 0xfd

int win_scm2_passthrough(struct fw_cmd *p_cmd, unsigned int *p_dsm_status)
{
  int scm_err = 0;
  *p_dsm_status = 0;
  scm_err = write_large_input_payload(scm_err, p_dsm_status,
      p_cmd->DimmID, p_cmd->LargeInputPayload, p_cmd->LargeInputPayloadSize);

  scm_err = do_passthrough_fix_output(scm_err, p_dsm_status, p_cmd->DimmID,
      p_cmd->Opcode, p_cmd->SubOpcode,
      p_cmd->InputPayload, p_cmd->InputPayloadSize,
      p_cmd->OutPayload, p_cmd->OutputPayloadSize);

  scm_err = read_large_ouptut_payload(scm_err, p_dsm_status, p_cmd->DimmID,
      p_cmd->LargeOutputPayload, p_cmd->LargeOutputPayloadSize);

  return scm_err;
}

/*
 * For non-large_payload commands, the Windows driver always expects an output payload of
 * size 128, even if the FIS doesn't indicate there is one, or even if it is smaller than
 * 128 bytes. It should never be greater than 128 bytes.
 */
#define PAYLOAD_SIZE 128

static int do_passthrough_fix_output(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    unsigned char opcode, unsigned char sub_op_code,
    void *input_payload, unsigned int input_payload_size,
    void *output_payload, unsigned int output_payload_size)
{
  unsigned char tmp_output_payload[PAYLOAD_SIZE];
  memset(tmp_output_payload, 0, PAYLOAD_SIZE);

  scm_err = do_passthrough(scm_err, p_dsm_status,
      handle,
      opcode, sub_op_code,
      input_payload, input_payload_size,
      tmp_output_payload, PAYLOAD_SIZE);

  if (NO_ERRORS(scm_err, p_dsm_status))
  {
    unsigned int transfer_size = output_payload_size;
    if (transfer_size > PAYLOAD_SIZE)
    {
      transfer_size = PAYLOAD_SIZE;
    }

    memmove(output_payload, tmp_output_payload, transfer_size);
  }
  return scm_err;
}

static int do_passthrough(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    unsigned char opcode, unsigned char sub_op_code,
    void *input_payload, unsigned int input_payload_size,
    void *output_payload, unsigned int output_payload_size)
{
  if (NO_ERRORS(scm_err, p_dsm_status))
  {
      scm_err = win_scm2_ioctl_passthrough_cmd(handle,
          opcode, sub_op_code,
          input_payload, input_payload_size,
          output_payload, output_payload_size,
          p_dsm_status);


  }
  return scm_err;
}

static int get_large_payload_sizes(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    struct pt_bios_get_size *p_size)
{

  if (NO_ERRORS(scm_err, p_dsm_status))
  {
    unsigned input_buffer[128];
    unsigned output_buffer[128];

    scm_err = do_passthrough(scm_err, p_dsm_status, handle,
        BIOS_EMULATED_COMMAND, SUBOP_GET_PAYLOAD_SIZE,
        input_buffer, 128,
        output_buffer, 128);
    memmove(p_size, output_buffer, sizeof (*p_size));
  }
  return scm_err;
}


static int write_large_input_payload(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    void *large_input_payload,
    unsigned int large_input_payload_size)
{

  if (NO_ERRORS(scm_err, p_dsm_status) && large_input_payload_size > 0)
  {
    struct pt_bios_get_size size;
    scm_err = get_large_payload_sizes(scm_err, p_dsm_status, handle, &size);

    if (!NO_ERRORS(scm_err, p_dsm_status))
    {

    }
    else if (large_input_payload_size > size.large_input_payload_size)
    {
      scm_err = -1;
    }
    else
    {
      // write to the large input payload
      unsigned int current_offset = 0;
      unsigned int total_transfer_size = large_input_payload_size;
      int i = 0;
      struct bios_input_payload
      {
        unsigned int bytes_to_transfer;
        unsigned int large_input_payload_offset;
        unsigned char buffer[];
      };
      unsigned int bios_input_payload_size = sizeof(struct bios_input_payload) + size.rw_size;
      struct bios_input_payload *p_input_payload = malloc(bios_input_payload_size);

      if (p_input_payload) {
          p_input_payload->bytes_to_transfer = size.rw_size;

          while (current_offset < total_transfer_size && NO_ERRORS(scm_err, p_dsm_status))
          {
              p_input_payload->large_input_payload_offset = current_offset;

              unsigned int transfer_size = size.rw_size;
              if (transfer_size + current_offset > total_transfer_size)
              {
                  transfer_size = total_transfer_size - current_offset;
              }
               memmove(p_input_payload->buffer, (unsigned char *)large_input_payload + current_offset,
                  size.rw_size);

               scm_err = do_passthrough(scm_err, p_dsm_status, handle,
                  BIOS_EMULATED_COMMAND, SUBOP_WRITE_LARGE_PAYLOAD_INPUT,
                  p_input_payload, bios_input_payload_size,
                  NULL, 0);

               current_offset += transfer_size;
               i++;
          }

          free(p_input_payload);
      } else {
               scm_err = -1;
      }
      if (!NO_ERRORS(scm_err, p_dsm_status))
      {
      }
    }
  }
  return scm_err;
}

static int read_large_ouptut_payload(int scm_err, unsigned int *p_dsm_status, unsigned int handle,
    void *large_output_payload, unsigned int large_output_payload_size)
{
  if (NO_ERRORS(scm_err, p_dsm_status) && large_output_payload_size > 0)
  {
    // get the sizes used for writing to the large payload
    struct pt_bios_get_size size;
    scm_err = get_large_payload_sizes(scm_err, p_dsm_status, handle, &size);
    if (!NO_ERRORS(scm_err, p_dsm_status))
    {
    }
    else if (large_output_payload_size > size.large_input_payload_size)
    {
      scm_err = -1;
    }
    else
    {
      struct
      {
        unsigned int bytes_to_transfer;
        unsigned int large_output_payload_offset;
      } input_payload;
      input_payload.bytes_to_transfer = size.rw_size;
      unsigned int offset = 0;
      input_payload.large_output_payload_offset = offset;

      unsigned int total_transfer_size = large_output_payload_size;
      while (input_payload.large_output_payload_offset < total_transfer_size &&
        NO_ERRORS(scm_err, p_dsm_status))
      {
        if (input_payload.bytes_to_transfer + input_payload.large_output_payload_offset
          > total_transfer_size)
        {
          input_payload.bytes_to_transfer =
              total_transfer_size - input_payload.large_output_payload_offset;
        }

        unsigned char *buffer = malloc(input_payload.bytes_to_transfer);
                if (buffer) {
                    scm_err = do_passthrough(scm_err, p_dsm_status, handle,
                        BIOS_EMULATED_COMMAND, SUBOP_READ_LARGE_PAYLOAD_OUTPUT,
                        &input_payload, sizeof(input_payload),
                        buffer,
                        input_payload.bytes_to_transfer);

                    memmove((unsigned char*)large_output_payload + offset, buffer, input_payload.bytes_to_transfer);

                    free(buffer);
                } else {
                    scm_err = -1;
                }

        input_payload.large_output_payload_offset += input_payload.bytes_to_transfer;
        offset += input_payload.bytes_to_transfer;
      }
    }
  }
  return scm_err;
}
