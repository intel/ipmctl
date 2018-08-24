/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/
#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <ShellBase.h>
#include <Guid/FileInfo.h>
#include <EfiShellParameters.h>
#include <LoadedImage.h>
#include <EfiShellInterface.h>
#include <NvmStatus.h>
#include <Debug.h>
#include <stdarg.h>
#include <stdlib.h>
#include <Library/PrintLib.h>
#include <BaseMemoryLib.h>
#include <Convert.h>
#include <Dimm.h>
#include <NvmDimmDriver.h>
#include <Common.h>
#ifdef _MSC_VER
#include <io.h>
#include <conio.h>
#include <time.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#include <safe_str_lib.h>
#include <safe_mem_lib.h>
#include <safe_lib.h>
#define _read read
#define _getch getchar
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include "os_efi_hii_auto_gen_strings.h"
#include "os_efi_simple_file_protocol.h"
#include "os_efi_bs_protocol.h"
#include "os_efi_shell_parameters_protocol.h"
#include "os.h"
#include "os_common.h"
#include <os_efi_api.h>
#include <os_types.h>
#include "event.h"

EFI_SYSTEM_TABLE *gST;
EFI_SHELL_INTERFACE *mEfiShellInterface;
EFI_RUNTIME_SERVICES *gRT;
EFI_HANDLE gImageHandle;

#define CLI_VERSION_MAX 25
#define FILE_DESCRIPTION_MAX 1024
#define FILE_DESCRIPTION "Intel(R) Optane(TM) DC Persistent Memory Recording File."

extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;
extern int get_vendor_driver_revision(char * version_str, const int str_len);
extern int g_record_mode;
extern int g_playback_mode;
extern NVMDIMMDRIVER_DATA *gNvmDimmData;
extern char g_recording_fullpath[PATH_MAX];


UINT8 *gSmbiosTable = NULL;
size_t gSmbiosTableSize = 0;
UINT8 gSmbiosMinorVersion = 0;
UINT8 gSmbiosMajorVersion = 0;

#pragma pack(push)
#pragma pack(1)
typedef struct _record
{
  RecordType  type;
  UINT32 offset;
  UINT32 size;
  UINT32 reserved;
}record;

typedef struct _record_table
{
  record smbios;
  record acpi_nfit;
  record acpi_pcat;
  record acpi_pmtt;
  record pass_thru;
  record reserved1;
  record reserved2;
  record reserved3;
  record reserved4;
  record reserved5;
}record_table;

typedef struct _recording_file
{
  record_table record_table_locations;
  CHAR8 sw_version[CLI_VERSION_MAX];
  CHAR8 os_version[OS_VERSION_MAX];
  CHAR8 os_name[OS_NAME_MAX];
  CHAR8 description[FILE_DESCRIPTION_MAX];
  UINT32 reserved1;
  UINT32 reserved2;
  UINT32 reserved3;
  UINT32 reserved4;
  UINT32 reserved5;
}recording_file;
#pragma pop()

typedef struct _smbios_table_recording
{
  size_t size;
  UINT8 minor;
  UINT8 major;
  UINT8 table[];
}smbios_table_recording;

#define SMBIOS_SIZE     0x2800  //10k
#define ACPI_NFIT_SIZE  0x2800  //10k
#define ACPI_PCAT_SIZE  0x2800  //10k
#define ACPI_PMTT_SIZE  0x2800  //10k

#define SMBIOS_OFFSET    (sizeof(recording_file))
#define ACPI_NFIT_OFFSET (SMBIOS_OFFSET + SMBIOS_SIZE) //10k
#define ACPI_PCAT_OFFSET (ACPI_NFIT_OFFSET + ACPI_NFIT_SIZE) //20k
#define ACPI_PMTT_OFFSET (ACPI_PCAT_OFFSET + ACPI_PCAT_SIZE) //30k
#define PASS_THRU_OFFSET (ACPI_PMTT_OFFSET + ACPI_PMTT_SIZE) //40k

int g_pass_thru_cnt = 0;
size_t g_pass_thru_playback_offset = 0;

#define REC_FILE_PATH g_recording_fullpath
#define PLAYBACK_ENABLED() g_playback_mode
#define RECORD_ENABLED() g_record_mode
#define INC_PASS_THRU_CNT() ++g_pass_thru_cnt
#define APPEND_RECORDING() g_pass_thru_cnt

struct debug_logger_config
{
  CHAR8 initialized : 1;
  CHAR8 stdout_enabled;
  CHAR8 file_enabled;
  CHAR8 level;
};
enum
{
  LOGGER_OFF = 0,
  LOG_ERROR = 1,
  LOG_WARNING = 2,
  LOG_INFO = 3,
  LOG_VERBOSE = 4,
} LOG_LEVEL_LIST;

#define INI_PREFERENCES_LOG_LEVEL L"DBG_LOG_LEVEL"
#define INI_PREFERENCES_LOG_STDOUT_ENABLED L"DBG_LOG_STDOUT_ENABLED"
#define INI_PREFERENCES_LOG_DEBUG_FILE_ENABLED L"DBG_LOG_DEBUG_FILE_ENABLED"

/*
* Debug logger context structure.
*/
static struct debug_logger_config g_log_config = { 0 };

EFI_STATUS init_record_file(char * recording_file_path)
{
  FILE* f_ptr = NULL;
  recording_file rec_file_header;
  ZeroMem(&rec_file_header, sizeof(recording_file));
  rec_file_header.record_table_locations.smbios.type = RtSmbios;
  rec_file_header.record_table_locations.smbios.offset = SMBIOS_OFFSET;
  rec_file_header.record_table_locations.acpi_nfit.type = RtAcpiNfit;
  rec_file_header.record_table_locations.acpi_nfit.offset = ACPI_NFIT_OFFSET;
  rec_file_header.record_table_locations.acpi_pcat.type = RtAcpiPcat;
  rec_file_header.record_table_locations.acpi_pcat.offset = ACPI_PCAT_OFFSET;
  rec_file_header.record_table_locations.acpi_pmtt.type = RtAcpiPmtt;
  rec_file_header.record_table_locations.acpi_pmtt.offset = ACPI_PMTT_OFFSET;
  rec_file_header.record_table_locations.pass_thru.type = RtPassThru;
  rec_file_header.record_table_locations.pass_thru.offset = PASS_THRU_OFFSET;

  os_get_os_name(rec_file_header.os_name, OS_NAME_MAX);
  os_get_os_version(rec_file_header.os_version, OS_VERSION_MAX);
  strcpy_s(rec_file_header.sw_version, CLI_VERSION_MAX, NVMDIMM_VERSION_STRING_A);
  sprintf_s(rec_file_header.description, FILE_DESCRIPTION_MAX, FILE_DESCRIPTION);

  if (0 != fopen_s(&f_ptr, recording_file_path, "wb"))
  {
    NVDIMM_ERR("Failed to open the following recording file: %s\n", recording_file_path);
    return EFI_OUT_OF_RESOURCES;
  }

  size_t bytes_written = 0;
  bytes_written = fwrite(&rec_file_header, sizeof(recording_file), 1, f_ptr);
  if (1 != bytes_written)
  {
    NVDIMM_ERR("Failed to write the recording file headaer\n");
    return EFI_END_OF_FILE;
  }

  fclose(f_ptr);
  return EFI_SUCCESS;
}

EFI_STATUS update_record_size(RecordType type, FILE * file_stream, UINT32 size, BOOLEAN increment)
{
  recording_file rc;
  UINT32 offset;

  //seek to the begining of the file
  if (0 != fseek(file_stream, 0, SEEK_SET))
  {
    NVDIMM_ERR("Failed seeking to the begining of the file\n");
    return EFI_END_OF_FILE;
  }

  if (1 != fread(&rc, sizeof(recording_file), 1, file_stream))
  {
    NVDIMM_ERR("Failed to read the recording file header\n");
    return EFI_END_OF_FILE;
  }

  switch (type)
  {
  case RtSmbios:
    if (increment)
    {
      rc.record_table_locations.smbios.size += size;
    }
    else
    {
      rc.record_table_locations.smbios.size = size;
    }
    break;
  case RtAcpiNfit:
    if (increment)
    {
      rc.record_table_locations.acpi_nfit.size += size;
    }
    else
    {
      rc.record_table_locations.acpi_nfit.size = size;
    }
    break;
  case RtAcpiPcat:
    if (increment)
    {
      rc.record_table_locations.acpi_pcat.size += size;
    }
    else
    {
      rc.record_table_locations.acpi_pcat.size = size;
    }
    break;
  case RtAcpiPmtt:
    if (increment)
    {
      rc.record_table_locations.acpi_pmtt.size += size;
    }
    else
    {
      rc.record_table_locations.acpi_pmtt.size = size;
    }
    break;
  case RtPassThru:
    if (increment)
    {
      rc.record_table_locations.pass_thru.size += size;
    }
    else
    {
      rc.record_table_locations.pass_thru.size = size;
    }
    break;
  default:
    NVDIMM_ERR("Unknown record type\n");
    return EFI_END_OF_FILE;
  }

  //seek to the begining of the file
  if (0 != fseek(file_stream, 0, SEEK_SET))
  {
    NVDIMM_ERR("Failed seeking to the begining of the file\n");
    return EFI_END_OF_FILE;
  }

  size_t bytes_written = 0;
  bytes_written = fwrite(&rc, sizeof(recording_file), 1, file_stream);
  if (1 != bytes_written)
  {
    NVDIMM_ERR("Failed to write the recording file header\n");
    return EFI_END_OF_FILE;
  }

  return EFI_SUCCESS;
}

EFI_STATUS seek_to_record_offset(RecordType type, FILE * file_stream, UINT32 *record_size)
{
  recording_file rc;
  UINT32 offset;

  //seek to the begining of the file
  if (0 != fseek(file_stream, 0, SEEK_SET))
  {
    NVDIMM_ERR("Failed seeking to the begining of the file\n");
    return -1;
  }

  if (1 != fread(&rc, sizeof(recording_file), 1, file_stream))
  {
    NVDIMM_ERR("Failed to read the recording file header\n");
    return -1;
  }

  switch (type)
  {
  case RtSmbios:
    offset = rc.record_table_locations.smbios.offset;
    *record_size = rc.record_table_locations.smbios.size;
    break;
  case RtAcpiNfit:
    offset = rc.record_table_locations.acpi_nfit.offset;
    *record_size = rc.record_table_locations.acpi_nfit.size;
    break;
  case RtAcpiPcat:
    offset = rc.record_table_locations.acpi_pcat.offset;
    *record_size = rc.record_table_locations.acpi_pcat.size;
    break;
  case RtAcpiPmtt:
    offset = rc.record_table_locations.acpi_pmtt.offset;
    *record_size = rc.record_table_locations.acpi_pmtt.size;
    break;
  case RtPassThru:
    offset = rc.record_table_locations.pass_thru.offset;
    *record_size = rc.record_table_locations.pass_thru.size;
    break;
  default:
    NVDIMM_ERR("Unknown record type\n");
    return -1;
  }

  //seek to the begining of the record type partition
  if (0 != fseek(file_stream, offset, SEEK_SET))
  {
    NVDIMM_ERR("Failed seeking to the begining of the file\n");
    return -1;
  }
  return EFI_SUCCESS;
}

EFI_STATUS
passthru_playback(
  IN OUT FW_CMD *pCmd
)
{
  if (!PLAYBACK_ENABLED())
  {
    return EFI_UNSUPPORTED;
  }

  if (NULL == pCmd)
  {
    return EFI_INVALID_PARAMETER;
  }

  FILE *f_passthru_ptr = NULL;

  pass_thru_record_req pt_rec_req;
  pass_thru_record_resp pt_rec_resp;
  UINT32 record_size = 0;

  errno_t open_result = fopen_s(&f_passthru_ptr, REC_FILE_PATH, "rb+");
  if (0 != open_result)
  {
    NVDIMM_ERR("Failed to open the following recording file: %s\n", REC_FILE_PATH);
    return EFI_END_OF_FILE;
  }

  //seek it to pass thru partition
  if (EFI_SUCCESS != seek_to_record_offset(RtPassThru, f_passthru_ptr, &record_size))
  {
    NVDIMM_ERR("Failed seeking to the passthru partition\n");
    return EFI_END_OF_FILE;
  }

  if (0 != fseek(f_passthru_ptr, g_pass_thru_playback_offset, SEEK_CUR))
  {
    NVDIMM_ERR("Failed seeking into playback file\n");
    return EFI_END_OF_FILE;
  }

  if (1 != fread(&pt_rec_req, sizeof(pass_thru_record_req), 1, f_passthru_ptr))
  {
    NVDIMM_ERR("Failed to read the request packet from the recording file\n");
    return EFI_END_OF_FILE;
  }
  g_pass_thru_playback_offset += sizeof(pass_thru_record_req);

  //todo: support large input payload size
  if (0 != fseek(f_passthru_ptr, pt_rec_req.InputPayloadSize, SEEK_CUR))
  {
    NVDIMM_ERR("Failed seeking into playback file\n");
    return EFI_END_OF_FILE;
  }
  g_pass_thru_playback_offset += pt_rec_req.InputPayloadSize;

  if (1 != fread(&pt_rec_resp, sizeof(pass_thru_record_resp), 1, f_passthru_ptr))
  {
    NVDIMM_ERR("Failed to read the response packet from the recording file\n");
    return EFI_END_OF_FILE;
  }
  g_pass_thru_playback_offset += sizeof(pass_thru_record_resp);

  if (0 == pt_rec_resp.OutputPayloadSize)
  {
    NVDIMM_ERR("Payload size is reporting 0 in the recording file\n");
    return EFI_END_OF_FILE;
  }

  if (pt_rec_resp.OutputPayloadSize > IN_PAYLOAD_SIZE)
  {
    pCmd->LargeOutputPayloadSize = pt_rec_resp.OutputPayloadSize;
    if (1 != fread(pCmd->LargeOutputPayload, pt_rec_resp.OutputPayloadSize, 1, f_passthru_ptr))
    {
      NVDIMM_ERR("Failed to read the LargeOutputPayload from the recording file\n");
      return EFI_END_OF_FILE;
    }
    g_pass_thru_playback_offset += pt_rec_resp.OutputPayloadSize;
  }
  else
  {
    pCmd->OutputPayloadSize = pt_rec_resp.OutputPayloadSize;
    if (1 != fread(pCmd->OutPayload, pt_rec_resp.OutputPayloadSize, 1, f_passthru_ptr))
    {
      NVDIMM_ERR("Failed to read the OutputPayload from the recording file\n");
      return EFI_END_OF_FILE;
    }
    g_pass_thru_playback_offset += pt_rec_resp.OutputPayloadSize;
  }
  pCmd->Status = pt_rec_resp.Status;
  fclose(f_passthru_ptr);
  INC_PASS_THRU_CNT();

  return pt_rec_resp.PassthruReturnCode;
}

EFI_STATUS
passthru_record_setup(
  FILE **f_passthru_ptr,
  IN OUT FW_CMD *pCmd
)
{
  UINT32 record_size = 0;

  if (!RECORD_ENABLED())
  {
    NVDIMM_ERR("Recording mode not enabled. \n");
    return EFI_UNSUPPORTED;
  }

  if (NULL == f_passthru_ptr || NULL == pCmd)
  {
    return EFI_INVALID_PARAMETER;
  }

  errno_t open_result = fopen_s(f_passthru_ptr, REC_FILE_PATH, "rb+");
  if (0 != open_result)
  {
    NVDIMM_ERR("Failed to open the following recording file: %s\n", REC_FILE_PATH);
    return EFI_END_OF_FILE;
  }

  //seek it to pass thru partition
  if (EFI_SUCCESS != seek_to_record_offset(RtPassThru, *f_passthru_ptr, &record_size))
  {
    NVDIMM_ERR("Failed seeking to the passthru partition\n");
    return EFI_END_OF_FILE;
  }

  if (0 != fseek(*f_passthru_ptr, record_size, SEEK_CUR))
  {
    NVDIMM_ERR("Failed seeking into playback file\n");
    return EFI_END_OF_FILE;
  }

  /* if (APPEND_RECORDING())
   {
     errno_t open_result = fopen_s(f_passthru_ptr, REC_FILE_PASSTHRU, "ab");
     if (0 != open_result)
     {
       NVDIMM_ERR("Failed to open the following recording file in append mode: %s\n", REC_FILE_PASSTHRU);
       return EFI_END_OF_FILE;
     }
   }
   else
   {
     errno_t open_result = fopen_s(f_passthru_ptr, REC_FILE_PASSTHRU, "wb");
     if (0 != open_result)
     {
       NVDIMM_ERR("Failed to open the following recording file: %s\n", REC_FILE_PASSTHRU);
       return EFI_END_OF_FILE;
     }
   }

   if (0 != fseek(*f_passthru_ptr, 0, SEEK_END))
   {
     NVDIMM_ERR("Failed seeking into playback file\n");
     return EFI_END_OF_FILE;
   }
 */
  return EFI_SUCCESS;
}

EFI_STATUS
passthru_record_finalize(
  FILE *f_passthru_ptr,
  IN OUT FW_CMD *pCmd,
  UINT32 DimmID,
  EFI_STATUS PassthruReturnCode
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 total_write_sz = 0;
  if (!RECORD_ENABLED())
  {
    NVDIMM_ERR("Recording mode not enabled. \n");
    return EFI_UNSUPPORTED;
  }

  if (NULL == f_passthru_ptr || NULL == pCmd)
  {
    return EFI_INVALID_PARAMETER;
  }

  pass_thru_record_req pt_rec_req;
  pt_rec_req.DimmId = DimmID;
  pt_rec_req.Opcode = pCmd->Opcode;
  pt_rec_req.SubOpcode = pCmd->SubOpcode;
  pt_rec_req.TotalMilliseconds = GetCurrentMilliseconds();
  pt_rec_req.InputPayloadSize = pCmd->InputPayloadSize + pCmd->LargeInputPayloadSize;

  size_t bytes_written = 0;
  bytes_written = fwrite(&pt_rec_req, sizeof(pass_thru_record_req), 1, f_passthru_ptr);
  if (1 != bytes_written)
  {
    NVDIMM_ERR("Failed to write the request packet to the recording file\n");
    return EFI_END_OF_FILE;
  }
  total_write_sz += sizeof(pass_thru_record_req);

  if (pCmd->InputPayloadSize)
  {
    if (1 != fwrite(pCmd->InputPayload, pCmd->InputPayloadSize, 1, f_passthru_ptr))
    {
      NVDIMM_ERR("Failed to write the input payload to the recording file \n");
      return EFI_END_OF_FILE;
    }
    total_write_sz += pCmd->InputPayloadSize;
  }
  else if (pCmd->LargeInputPayloadSize)
  {
    if (1 != fwrite(pCmd->LargeInputPayload, pCmd->LargeInputPayloadSize, 1, f_passthru_ptr))
    {
      NVDIMM_ERR("Failed to write the large input payload to the recording file \n");
      return EFI_END_OF_FILE;
    }
    total_write_sz += pCmd->LargeInputPayloadSize;
  }

  pass_thru_record_resp pt_rec_resp;
  pt_rec_resp.DimmId = DimmID;
  pt_rec_resp.PassthruReturnCode = PassthruReturnCode;
  pt_rec_resp.Status = pCmd->Status;
  pt_rec_resp.TotalMilliseconds = GetCurrentMilliseconds();
  pt_rec_resp.OutputPayloadSize = pCmd->OutputPayloadSize + pCmd->LargeOutputPayloadSize;
  if (1 != fwrite(&pt_rec_resp, sizeof(pass_thru_record_resp), 1, f_passthru_ptr))
  {
    NVDIMM_ERR("Failed to write the response payload to the recording file \n");
    return EFI_END_OF_FILE;
  }
  total_write_sz += sizeof(pass_thru_record_resp);
  if (pCmd->OutputPayloadSize)
  {
    if (1 != fwrite(pCmd->OutPayload, pCmd->OutputPayloadSize, 1, f_passthru_ptr))
    {
      NVDIMM_ERR("Failed to write the outpayload to the recording file \n");
      return EFI_END_OF_FILE;
    }
    total_write_sz += pCmd->OutputPayloadSize;
  }
  else if (pCmd->LargeOutputPayloadSize)
  {
    if (1 != fwrite(pCmd->LargeOutputPayload, pCmd->LargeOutputPayloadSize, 1, f_passthru_ptr))
    {
      NVDIMM_ERR("Failed to write the large outpayload to the recording file \n");
      return EFI_END_OF_FILE;
    }
    total_write_sz += pCmd->LargeOutputPayloadSize;
  }

  update_record_size(RtPassThru, f_passthru_ptr, total_write_sz, TRUE);
  fflush(f_passthru_ptr);
  fclose(f_passthru_ptr);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
PassThru(
  IN     struct _DIMM *pDimm,
  IN OUT FW_CMD *pCmd,
  IN     UINT64 Timeout
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  EFI_STATUS RecordRc = EFI_SUCCESS;
  UINT32 DimmID;
  FILE *f_passthru_ptr = NULL;

  if (!pDimm || !pCmd)
    return EFI_INVALID_PARAMETER;

  if (PLAYBACK_ENABLED())
  {
    return passthru_playback(pCmd);
  }

  if (RECORD_ENABLED())
  {
    RecordRc = passthru_record_setup(&f_passthru_ptr, pCmd);
    if (EFI_SUCCESS != RecordRc)
    {
      return RecordRc;
    }
  }

  DimmID = pCmd->DimmID;
  pCmd->DimmID = pDimm->DeviceHandle.AsUint32;
  Rc = passthru_os(pDimm, pCmd, Timeout);
  INC_PASS_THRU_CNT();
  pCmd->DimmID = DimmID;

  if (RECORD_ENABLED())
  {
    RecordRc = passthru_record_finalize(f_passthru_ptr, pCmd, DimmID, Rc);
    if (EFI_SUCCESS != RecordRc)
    {
      return RecordRc;
    }
  }

  return Rc;
}

EFI_STATUS
save_table_to_file(
  RecordType type,
  char* destFile,
  EFI_ACPI_DESCRIPTION_HEADER *table
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  FILE* f_ptr;
  UINT32 record_size = 0;

  errno_t open_result = fopen_s(&f_ptr, destFile, "rb+");
  if (0 != open_result)
  {
    return EFI_END_OF_FILE;
  }

  //seek it to pass thru partition
  if (EFI_SUCCESS != (Rc = seek_to_record_offset(type, f_ptr, &record_size)))
  {
    NVDIMM_ERR("Failed seeking to the ACPI partition\n");
    return Rc;
  }

  if (table && 1 != fwrite(table, table->Length, 1, f_ptr))
  {
    Rc = EFI_END_OF_FILE;
  }

  if (table)
  {
    Rc = update_record_size(type, f_ptr, table->Length, FALSE);
  }
  fclose(f_ptr);
  return Rc;
}


EFI_STATUS
load_table_from_file(
  RecordType type,
  char* sourceFile,
  EFI_ACPI_DESCRIPTION_HEADER ** table
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 size;
  FILE* f_ptr;

  *table = NULL;
  errno_t open_result = fopen_s(&f_ptr, sourceFile, "rb+");
  if (0 != open_result)
  {
    Rc = EFI_END_OF_FILE;
    return Rc;
  }

  //seek it to pass thru partition
  if (EFI_SUCCESS != (Rc = seek_to_record_offset(type, f_ptr, &size)))
  {
    NVDIMM_ERR("Failed seeking to the ACPI partition\n");
    return Rc;
  }

  if (!size)
  {
    Rc = EFI_END_OF_FILE;
  }
  else
  {
    *table = AllocatePool(size);
    if (!*table)
    {
      Rc = EFI_OUT_OF_RESOURCES;
    }
    else {
      if (1 != fread(*table, size, 1, f_ptr))
      {
        Rc = EFI_END_OF_FILE;
      }
    }
  }

  fclose(f_ptr);
  return Rc;
}

EFI_STATUS
initAcpiTables()
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_ACPI_DESCRIPTION_HEADER * PtrNfitTable = NULL;
  EFI_ACPI_DESCRIPTION_HEADER * PtrPcatTable = NULL;
  EFI_ACPI_DESCRIPTION_HEADER * PtrPMTTTable = NULL;
  UINT32 failures = 0;

  if (PLAYBACK_ENABLED())
  {
    if (EFI_ERROR(load_table_from_file(RtAcpiNfit, REC_FILE_PATH, &PtrNfitTable)))
    {
      NVDIMM_WARN("Failed to load the NFIT table from the record file.\n");
      failures++;
    }

    if (EFI_ERROR(load_table_from_file(RtAcpiPcat, REC_FILE_PATH, &PtrPcatTable)))
    {
      NVDIMM_WARN("Failed to load the PCAT table from the record file.\n");
      failures++;
    }

    if (EFI_ERROR(load_table_from_file(RtAcpiPmtt, REC_FILE_PATH, &PtrPMTTTable)))
    {
      NVDIMM_WARN("Failed to load the PMTT table from the record file.\n");
      //failures++; //table allowed to be empty. Not a failure
    }
  }
  else
  {
    if (EFI_ERROR(get_nfit_table(&PtrNfitTable)))
    {
      NVDIMM_WARN("Failed to get the NFIT table.\n");
      failures++;
    }
    if (EFI_ERROR(get_pcat_table(&PtrPcatTable)))
    {
      NVDIMM_WARN("Failed to get the PCAT table.\n");
      failures++;
    }
    if (EFI_ERROR(get_pmtt_table(&PtrPMTTTable)))
    {
      NVDIMM_WARN("Failed to get the PMTT table.\n");
      //failures++; //table allowed to be empty. Not a failure
    }

    if (RECORD_ENABLED())
    {
      if (EFI_ERROR(save_table_to_file(RtAcpiNfit, REC_FILE_PATH, PtrNfitTable)))
      {
        NVDIMM_WARN("Failed to save the NFIT table to the record file.\n");
        failures++;
      }
      if (EFI_ERROR(save_table_to_file(RtAcpiPcat, REC_FILE_PATH, PtrPcatTable)))
      {
        NVDIMM_WARN("Failed to save the PCAT table to the record file.\n");
        failures++;
      }
      if (EFI_ERROR(save_table_to_file(RtAcpiPmtt, REC_FILE_PATH, PtrPMTTTable)))
      {
        NVDIMM_WARN("Failed to save the PMTT table to the record file.\n");
        //failures++; //table allowed to be empty. Not a failure
      }
    }
  }

  if (failures > 0)
  {
    NVDIMM_WARN("Encountered %d failures.", failures);
    return EFI_NOT_FOUND;
  }

  if (NULL == PtrNfitTable || NULL == PtrPcatTable)
  {
    NVDIMM_WARN("Failed to obtain NFIT or PCAT table.");
    return EFI_NOT_FOUND;
  }
  else
  {
    ReturnCode = ParseAcpiTables(PtrNfitTable, PtrPcatTable, PtrPMTTTable,
      &gNvmDimmData->PMEMDev.pFitHead, &gNvmDimmData->PMEMDev.pPcatHead, &gNvmDimmData->PMEMDev.IsMemModeAllowedByBios);
    if (EFI_ERROR(ReturnCode))
    {
      NVDIMM_WARN("Failed to parse NFIT or PCAT or PMTT table.");
      return EFI_NOT_FOUND;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
uninitAcpiTables(
)
{
  FREE_POOL_SAFE(gNvmDimmData->PMEMDev.pFitHead);
  FREE_POOL_SAFE(gNvmDimmData->PMEMDev.pPcatHead);
  FREE_POOL_SAFE(gNvmDimmData->PMEMDev.pPMTTTble);
  return EFI_SUCCESS;
}


EFI_STATUS
GetFirstAndBoundSmBiosStructPointer(
  OUT SMBIOS_STRUCTURE_POINTER *pSmBiosStruct,
  OUT SMBIOS_STRUCTURE_POINTER *pLastSmBiosStruct,
  OUT SMBIOS_VERSION *pSmbiosVersion
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;;
  int rc = 0;
  FILE *f_ptr;
  smbios_table_recording recording;
  UINT32 record_size = 0;

  if (pSmBiosStruct == NULL || pLastSmBiosStruct == NULL || pSmbiosVersion == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // One time initialization
  if (NULL == gSmbiosTable && !PLAYBACK_ENABLED())
  {
    get_smbios_table();
  }

  if (RECORD_ENABLED())
  {
    recording.major = gSmbiosMajorVersion;
    recording.minor = gSmbiosMinorVersion;
    recording.size = gSmbiosTableSize;

    errno_t open_result = fopen_s(&f_ptr, REC_FILE_PATH, "rb+");
    if (0 == open_result && NULL != gSmbiosTable)
    {
      //seek it to smbios partition
      if (EFI_SUCCESS != (ReturnCode = seek_to_record_offset(RtSmbios, f_ptr, &record_size)))
      {
        NVDIMM_ERR("Failed seeking to the SMBIOS partition\n");
        ReturnCode = EFI_END_OF_FILE;
      }

      if (1 != fwrite(&recording, sizeof(smbios_table_recording), 1, f_ptr))
      {
        NVDIMM_ERR("Failed to write to recording file: %s\n", REC_FILE_PATH);
        ReturnCode = EFI_END_OF_FILE;
      }
      if (1 != fwrite(gSmbiosTable, gSmbiosTableSize, 1, f_ptr))
      {
        NVDIMM_ERR("Failed to write to recording file: %s\n", REC_FILE_PATH);
        ReturnCode = EFI_END_OF_FILE;
      }
      update_record_size(RtSmbios, f_ptr, sizeof(smbios_table_recording) + gSmbiosTableSize, FALSE);
      fclose(f_ptr);
    }
  }
  else if (PLAYBACK_ENABLED() && NULL == gSmbiosTable)
  {
    errno_t open_result = fopen_s(&f_ptr, REC_FILE_PATH, "rb+");
    if (0 == open_result && NULL == gSmbiosTable)
    {
      //seek it to pass thru partition
      if (EFI_SUCCESS != (rc = seek_to_record_offset(RtSmbios, f_ptr, &record_size)))
      {
        NVDIMM_ERR("Failed seeking to the SMBIOS partition\n");
        ReturnCode = EFI_END_OF_FILE;
      }

      if (1 != fread(&recording, sizeof(smbios_table_recording), 1, f_ptr))
      {
        NVDIMM_ERR("Failed to read from recording file: %s\n", REC_FILE_PATH);
        ReturnCode = EFI_END_OF_FILE;
      }

      if (0 == recording.size)
      {
        NVDIMM_ERR("SMBIOS table in file %s reports size of 0.\n", REC_FILE_PATH);
        ReturnCode = EFI_END_OF_FILE;
      }
      else
      {
        gSmbiosTable = calloc(1, recording.size);
        if (NULL == gSmbiosTable)
        {
          NVDIMM_ERR("Unable to alloc for SMBIOS table\n");
          ReturnCode = EFI_END_OF_FILE;
        }
        else
        {
          size_t bytesRead = fread(gSmbiosTable, recording.size, 1, f_ptr);
          if (bytesRead != 1)
          {
            NVDIMM_ERR("SMBIOS table in file %s - read %lu bytes, expected %lu.\n", REC_FILE_PATH, bytesRead, recording.size);
            ReturnCode = EFI_END_OF_FILE;
          }
        }

        gSmbiosMajorVersion = recording.major;
        gSmbiosMinorVersion = recording.minor;
        gSmbiosTableSize = recording.size;
      }

      fclose(f_ptr);
    }
  }

  if (NULL != gSmbiosTable)
  {
    pSmBiosStruct->Raw = (UINT8 *)gSmbiosTable;
    pLastSmBiosStruct->Raw = pSmBiosStruct->Raw + gSmbiosTableSize;
    pSmbiosVersion->Major = gSmbiosMajorVersion;
    pSmbiosVersion->Minor = gSmbiosMinorVersion;
  }
  else
  {
    NVDIMM_ERR("Failed to retrieve smbios table\n");
    ReturnCode = EFI_END_OF_FILE;
  }

  return ReturnCode;
}

/*
* Function get the ini configuration only on the first call
*/
static void get_logger_config(struct debug_logger_config *p_log_config)
{
  EFI_STATUS efi_status;
  EFI_GUID guid = { 0 };
  UINTN size;

  if (p_log_config->initialized)
    return;

  size = sizeof(p_log_config->level);
  efi_status = GET_VARIABLE(INI_PREFERENCES_LOG_LEVEL, guid, &size, &p_log_config->level);
  if (EFI_SUCCESS != efi_status)
    return;
  size = sizeof(p_log_config->stdout_enabled);
  efi_status = GET_VARIABLE(INI_PREFERENCES_LOG_STDOUT_ENABLED, guid, &size, &p_log_config->stdout_enabled);
  if (EFI_SUCCESS != efi_status)
    return;
  size = sizeof(p_log_config->file_enabled);
  efi_status = GET_VARIABLE(INI_PREFERENCES_LOG_DEBUG_FILE_ENABLED, guid, &size, &p_log_config->file_enabled);
  if (EFI_SUCCESS != efi_status)
    return;

  p_log_config->initialized = TRUE;
}

/*
* Function enables disables the debug logger
*/
int
EFIAPI
DebugLoggerEnable(
  IN  BOOLEAN EnableDbgLogger
)
{
  if (FALSE == g_log_config.initialized)
  {
    return -1;
  }

  if (EnableDbgLogger)
  {
    if (FALSE == g_log_config.file_enabled)
      g_log_config.file_enabled = TRUE;
    if (LOGGER_OFF == g_log_config.level)
      g_log_config.level = LOG_WARNING;
  }
  else
  {
    if (TRUE == g_log_config.file_enabled)
      g_log_config.file_enabled = FALSE;
    if (TRUE == g_log_config.stdout_enabled)
      g_log_config.stdout_enabled = FALSE;
  }

  return 0;
}

/*
* Function returns the current state of the debug logger
*/
BOOLEAN
EFIAPI
IsDebugLoggerEnabled()
{
  if (FALSE == g_log_config.initialized) {
    return FALSE;
  }
  if (LOGGER_OFF != g_log_config.level) {
    if ((TRUE == g_log_config.file_enabled) ||
      (TRUE == g_log_config.stdout_enabled)) {
      return TRUE;
    }
  }
  return FALSE;
}

#ifdef NDEBUG
void (*rel_assert) (void) = NULL;
#endif // NDEBUG

/**
Prints a debug message to the debug output device if the specified error level is enabled.

If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
GetDebugPrintErrorLevel (), then print the message specified by Format and the
associated variable argument list to the debug output device.

If Format is NULL, then ASSERT().

@param  ErrorLevel  The error level of the debug message.
@param  Format      Format string for the debug message to print.
@param  ...         Variable argument list whose contents are accessed
based on the format string specified by Format.

**/
VOID
EFIAPI
DebugPrint(
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  ...
)
{
  VA_LIST args;
  static unsigned int event_type_common = 0;
  NVM_EVENT_MSG event_message;
  UINT32 size = sizeof(event_message);

  if (FALSE == g_log_config.initialized)
  {
    get_logger_config(&g_log_config);
    if (g_log_config.file_enabled)
      event_type_common |= SYSTEM_EVENT_TYPE_SYSLOG_FILE_SET(TRUE);
    if (g_log_config.stdout_enabled)
      event_type_common |= SYSTEM_EVENT_TYPE_SOUT_SET(TRUE);
    event_type_common |= SYSTEM_EVENT_TYPE_SEVERITY_SET(SYSTEM_EVENT_TYPE_DEBUG);
  }
  if (ErrorLevel == OS_DEBUG_CRIT) {
    // Send the debug entry to the logger
    VA_START(args, Format);
    AsciiVSPrint(event_message, size, Format, args);
    VA_END(args);
    nvm_store_system_entry(NVM_DEBUG_LOGGER_SOURCE, event_type_common | SYSTEM_EVENT_TYPE_SOUT_SET(TRUE) | SYSTEM_EVENT_TYPE_SYSLOG_FILE_SET(TRUE), NULL, event_message);
#ifdef NDEBUG
    rel_assert ();
#else // NDEBUG
    assert(FALSE);
#endif // NDEBUG
  }
  else if (LOGGER_OFF == g_log_config.level)
    return;
  if (((LOG_ERROR == g_log_config.level) & (ErrorLevel == OS_DEBUG_ERROR)) ||
    ((LOG_WARNING == g_log_config.level) & ((ErrorLevel == OS_DEBUG_ERROR) || (ErrorLevel == OS_DEBUG_WARN))) ||
    ((LOG_INFO == g_log_config.level) & ((ErrorLevel == OS_DEBUG_ERROR) || (ErrorLevel == OS_DEBUG_WARN) || (ErrorLevel == OS_DEBUG_INFO))) ||
    (LOG_VERBOSE == g_log_config.level))
  {
    // Send the debug entry to the logger
    VA_START(args, Format);
    AsciiVSPrint(event_message, size, Format, args);
    VA_END(args);
    nvm_store_system_entry(NVM_DEBUG_LOGGER_SOURCE, event_type_common, NULL, event_message);
  }
}

/**
Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
ASCII format string and a VA_LIST argument list.

Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
and BufferSize.
The ASCII string is produced by parsing the format string specified by FormatString.
Arguments are pulled from the variable argument list specified by Marker based on
the contents of the format string.
The number of ASCII characters in the produced output buffer is returned not including
the Null-terminator.
If BufferSize is 0, then no output buffer is produced and 0 is returned.

If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT().
If BufferSize > 0 and FormatString is NULL, then ASSERT().
If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator, then
ASSERT().
If PcdMaximumAsciiStringLength is not zero, and produced Null-terminated ASCII string
contains more than PcdMaximumAsciiStringLength ASCII characters not including the
Null-terminator, then ASSERT().

@param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
ASCII string.
@param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
@param  FormatString    A Null-terminated ASCII format string.
@param  Marker          VA_LIST marker for the variable argument list.

@return The number of ASCII characters in the produced output buffer not including the
Null-terminator.

**/
UINTN
EFIAPI
AsciiVSPrint(
  OUT CHAR8         *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR8   *FormatString,
  IN  VA_LIST       Marker
)
{
  if (0 == BufferSize)
    return BufferSize;

  return vsnprintf_s(StartOfBuffer, BufferSize
#ifdef _MSC_VER
    , BufferSize - 1
#endif
    , FormatString, Marker);
}

/**
Prints a formatted Unicode string to the console output device specified by
ConOut defined in the EFI_SYSTEM_TABLE.

This function prints a formatted Unicode string to the console output device
specified by ConOut in EFI_SYSTEM_TABLE and returns the number of Unicode
characters that printed to ConOut.  If the length of the formatted Unicode
string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
If Format is NULL, then ASSERT().
If Format is not aligned on a 16-bit boundary, then ASSERT().
If gST->ConOut is NULL, then ASSERT().

@param Format   A null-terminated Unicode format string.
@param ...      The variable argument list whose contents are accessed based
on the format string specified by Format.

@return Number of Unicode characters printed to ConOut.

**/
UINTN
EFIAPI
Print(
  IN CONST CHAR16  *Format,
  ...
)
{
  va_list argptr;
  va_start(argptr, Format);
  vfwprintf(gOsShellParametersProtocol.StdOut, Format, argptr);
  va_end(argptr);
  fflush(gOsShellParametersProtocol.StdOut);
  return 0;
}

UINTN
EFIAPI
PrintNoBuffer(CHAR16* Format, ...)
{
  va_list argptr;
  va_start(argptr, Format);
  vfwprintf(stdout, Format, argptr);
  va_end(argptr);
  fflush(stdout);
  return 0;
}
/**
Frees a buffer that was previously allocated with one of the pool allocation functions in the
Memory Allocation Library.

Frees the buffer specified by Buffer.  Buffer must have been allocated on a previous call to the
pool allocation services of the Memory Allocation Library.  If it is not possible to free pool
resources, then this function will perform no actions.

If Buffer was not allocated with a pool allocation function in the Memory Allocation Library,
then ASSERT().

@param  Buffer                Pointer to the buffer to free.

**/
VOID
EFIAPI
FreePool(
  IN VOID   *Buffer
)
{
  if (Buffer)free(Buffer);
}

/**
Fills a target buffer with zeros, and returns the target buffer.

This function fills Length bytes of Buffer with zeros, and returns Buffer.

If Length > 0 and Buffer is NULL, then ASSERT().
If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

@param  Buffer      The pointer to the target buffer to fill with zeros.
@param  Length      The number of bytes in Buffer to fill with zeros.

@return Buffer.

**/
VOID *
EFIAPI
ZeroMem(
  OUT VOID  *Buffer,
  IN UINTN  Length
)
{
  memset(Buffer, 0, Length);
  return Buffer;
}

/**
Removes a package list from the HII database.

If HiiHandle is NULL, then ASSERT().
If HiiHandle is not a valid EFI_HII_HANDLE in the HII database, then ASSERT().

@param[in]  HiiHandle   The handle that was previously registered in the HII database

**/
VOID
EFIAPI
HiiRemovePackages(
  IN      EFI_HII_HANDLE      HiiHandle
)
{
}

/**
Copies a source buffer to a destination buffer, and returns the destination buffer.

This function copies Length bytes from SourceBuffer to DestinationBuffer, and returns
DestinationBuffer.  The implementation must be reentrant, and it must handle the case
where SourceBuffer overlaps DestinationBuffer.

If Length is greater than (MAX_ADDRESS - DestinationBuffer + 1), then ASSERT().
If Length is greater than (MAX_ADDRESS - SourceBuffer + 1), then ASSERT().

@param  DestinationBuffer   The pointer to the destination buffer of the memory copy.
@param  SourceBuffer        The pointer to the source buffer of the memory copy.
@param  Length              The number of bytes to copy from SourceBuffer to DestinationBuffer.

@return DestinationBuffer.

**/
VOID *
EFIAPI
CopyMem(
  OUT VOID       *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
)
{
  memcpy_s(DestinationBuffer, Length, SourceBuffer, Length);
  return DestinationBuffer;
}



/**
Compares the contents of two buffers.

This function compares Length bytes of SourceBuffer to Length bytes of DestinationBuffer.
If all Length bytes of the two buffers are identical, then 0 is returned.  Otherwise, the
value returned is the first mismatched byte in SourceBuffer subtracted from the first
mismatched byte in DestinationBuffer.

If Length > 0 and DestinationBuffer is NULL, then ASSERT().
If Length > 0 and SourceBuffer is NULL, then ASSERT().
If Length is greater than (MAX_ADDRESS - DestinationBuffer + 1), then ASSERT().
If Length is greater than (MAX_ADDRESS - SourceBuffer + 1), then ASSERT().

@param  DestinationBuffer The pointer to the destination buffer to compare.
@param  SourceBuffer      The pointer to the source buffer to compare.
@param  Length            The number of bytes to compare.

@return 0                 All Length bytes of the two buffers are identical.
@retval Non-zero          The first mismatched byte in SourceBuffer subtracted from the first
mismatched byte in DestinationBuffer.

**/
INTN
EFIAPI
CompareMem(
  IN CONST VOID  *DestinationBuffer,
  IN CONST VOID  *SourceBuffer,
  IN UINTN       Length
)
{
  return memcmp(DestinationBuffer, SourceBuffer, Length);
}

/**
Compares two GUIDs.

This function compares Guid1 to Guid2.  If the GUIDs are identical then TRUE is returned.
If there are any bit differences in the two GUIDs, then FALSE is returned.

If Guid1 is NULL, then ASSERT().
If Guid2 is NULL, then ASSERT().

@param  Guid1       A pointer to a 128 bit GUID.
@param  Guid2       A pointer to a 128 bit GUID.

@retval TRUE        Guid1 and Guid2 are identical.
@retval FALSE       Guid1 and Guid2 are not identical.

**/
BOOLEAN
EFIAPI
CompareGuid(
  IN CONST GUID  *Guid1,
  IN CONST GUID  *Guid2
)
{
  if (Guid1->Data1 == Guid2->Data1 &&
    Guid1->Data2 == Guid2->Data2 &&
    Guid1->Data3 == Guid2->Data3 &&
    0 == memcmp(Guid1->Data4, Guid2->Data4, 8))
    return TRUE;
  return FALSE;
}

/**
Copies a source GUID to a destination GUID.

This function copies the contents of the 128-bit GUID specified by SourceGuid to
DestinationGuid, and returns DestinationGuid.

If DestinationGuid is NULL, then ASSERT().
If SourceGuid is NULL, then ASSERT().

@param  DestinationGuid   The pointer to the destination GUID.
@param  SourceGuid        The pointer to the source GUID.

@return DestinationGuid.

**/
GUID *
EFIAPI
CopyGuid(
  OUT GUID       *DestinationGuid,
  IN CONST GUID  *SourceGuid
)
{
  WriteUnaligned64(
    (UINT64*)DestinationGuid,
    ReadUnaligned64((CONST UINT64*)SourceGuid)
  );
  WriteUnaligned64(
    (UINT64*)DestinationGuid + 1,
    ReadUnaligned64((CONST UINT64*)SourceGuid + 1)
  );
  return DestinationGuid;
}

/**
Retrieves a string from a string package in a specific language.  If the language
is not specified, then a string from a string package in the current platform
language is retrieved.  If the string cannot be retrieved using the specified
language or the current platform language, then the string is retrieved from
the string package in the first language the string package supports.  The
returned string is allocated using AllocatePool().  The caller is responsible
for freeing the allocated buffer using FreePool().

If HiiHandle is NULL, then ASSERT().
If StringId is 0, then ASSERT().

@param[in]  HiiHandle  A handle that was previously registered in the HII Database.
@param[in]  StringId   The identifier of the string to retrieved from the string
package associated with HiiHandle.
@param[in]  Language   The language of the string to retrieve.  If this parameter
is NULL, then the current platform language is used.  The
format of Language must follow the language format assumed in
the HII Database.

@retval NULL   The string specified by StringId is not present in the string package.
@retval Other  The string was returned.

**/
EFI_STRING
EFIAPI
HiiGetString(
  IN EFI_HII_HANDLE  HiiHandle,
  IN EFI_STRING_ID   StringId,
  IN CONST CHAR8     *Language  OPTIONAL
)
{
  UINTN str_size = StrSize(gHiiStrings[StringId]);
  CHAR16 * str = (CHAR16*)AllocatePool(str_size);
  if (NULL != str) {
    CopyMem_S(str, str_size, gHiiStrings[StringId], str_size);
  }
  return str;
}

/**
Tests whether a controller handle is being managed by a specific driver.

This function tests whether the driver specified by DriverBindingHandle is
currently managing the controller specified by ControllerHandle.  This test
is performed by evaluating if the the protocol specified by ProtocolGuid is
present on ControllerHandle and is was opened by DriverBindingHandle with an
attribute of EFI_OPEN_PROTOCOL_BY_DRIVER.
If ProtocolGuid is NULL, then ASSERT().

@param  ControllerHandle     A handle for a controller to test.
@param  DriverBindingHandle  Specifies the driver binding handle for the
driver.
@param  ProtocolGuid         Specifies the protocol that the driver specified
by DriverBindingHandle opens in its Start()
function.

@retval EFI_SUCCESS          ControllerHandle is managed by the driver
specified by DriverBindingHandle.
@retval EFI_UNSUPPORTED      ControllerHandle is not managed by the driver
specified by DriverBindingHandle.

**/
EFI_STATUS
EFIAPI
EfiTestManagedDevice(
  IN CONST EFI_HANDLE       ControllerHandle,
  IN CONST EFI_HANDLE       DriverBindingHandle,
  IN CONST EFI_GUID         *ProtocolGuid
)
{
  return 0;
}

/**
Appends a formatted Unicode string to a Null-terminated Unicode string

This function appends a formatted Unicode string to the Null-terminated
Unicode string specified by String.   String is optional and may be NULL.
Storage for the formatted Unicode string returned is allocated using
AllocatePool().  The pointer to the appended string is returned.  The caller
is responsible for freeing the returned string.

If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
If FormatString is NULL, then ASSERT().
If FormatString is not aligned on a 16-bit boundary, then ASSERT().

@param[in] String         A Null-terminated Unicode string.
@param[in] FormatString   A Null-terminated Unicode format string.
@param[in]  Marker        VA_LIST marker for the variable argument list.

@retval NULL    There was not enough available memory.
@return         Null-terminated Unicode string is that is the formatted
string appended to String.
**/
CHAR16*
EFIAPI
CatVSPrint(
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  IN  VA_LIST       Marker
)
{
  INT32   CharactersRequired;
  UINTN   SizeRequired;
  CHAR16  *BufferToReturn;
  VA_LIST ExtraMarker;

  VA_COPY(ExtraMarker, Marker);
  static const int nBuffSize = 8192;
  static wchar_t evalBuff[8192];
  CharactersRequired = vswprintf_s(evalBuff, nBuffSize, FormatString, ExtraMarker);
  if (CharactersRequired > nBuffSize)
    return NULL;

  VA_END(ExtraMarker);

  if (String != NULL) {
    SizeRequired = StrSize(String) + (CharactersRequired * sizeof(CHAR16));
  }
  else {
    SizeRequired = sizeof(CHAR16) + (CharactersRequired * sizeof(CHAR16));
  }

  BufferToReturn = AllocateZeroPool(SizeRequired);

  if (BufferToReturn == NULL) {
    return NULL;
  }

  if (String != NULL) {
    wcscpy_s(BufferToReturn, SizeRequired / sizeof(CHAR16), String);
  }
  vswprintf_s(BufferToReturn + StrLen(BufferToReturn), (CharactersRequired + 1), FormatString, Marker);

  ASSERT(StrSize(BufferToReturn) == SizeRequired);

  return (BufferToReturn);
}

/**
Appends a formatted Unicode string to a Null-terminated Unicode string

This function appends a formatted Unicode string to the Null-terminated
Unicode string specified by String.   String is optional and may be NULL.
Storage for the formatted Unicode string returned is allocated using
AllocatePool().  The pointer to the appended string is returned.  The caller
is responsible for freeing the returned string.

If String is not NULL and not aligned on a 16-bit boundary, then ASSERT().
If FormatString is NULL, then ASSERT().
If FormatString is not aligned on a 16-bit boundary, then ASSERT().

@param[in] String         A Null-terminated Unicode string.
@param[in] FormatString   A Null-terminated Unicode format string.
@param[in] ...            The variable argument list whose contents are
accessed based on the format string specified by
FormatString.

@retval NULL    There was not enough available memory.
@return         Null-terminated Unicode string is that is the formatted
string appended to String.
**/
CHAR16 *
EFIAPI
CatSPrint(
  IN  CHAR16  *String, OPTIONAL
  IN  CONST CHAR16  *FormatString,
  ...
)
{
  VA_LIST   Marker;
  CHAR16    *NewString;

  VA_START(Marker, FormatString);
  NewString = CatVSPrint(String, FormatString, Marker);
  VA_END(Marker);
  return NewString;
}

/**
Allocates a buffer of type EfiBootServicesData.

Allocates the number bytes specified by AllocationSize of type EfiBootServicesData and returns a
pointer to the allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is
returned.  If there is not enough memory remaining to satisfy the request, then NULL is returned.

@param  AllocationSize        The number of bytes to allocate.

@return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocatePool(
  IN UINTN  AllocationSize
)
{
  return malloc(AllocationSize);
}

/**
Allocates and zeros a buffer of type EfiBootServicesData.

Allocates the number bytes specified by AllocationSize of type EfiBootServicesData, clears the
buffer with zeros, and returns a pointer to the allocated buffer.  If AllocationSize is 0, then a
valid buffer of 0 size is returned.  If there is not enough memory remaining to satisfy the
request, then NULL is returned.

@param  AllocationSize        The number of bytes to allocate and zero.

@return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateZeroPool(
  IN UINTN  AllocationSize
)
{
  return calloc(AllocationSize, 1);
}

/**
Copies a buffer to an allocated buffer of type EfiBootServicesData.

Allocates the number bytes specified by AllocationSize of type EfiBootServicesData, copies
AllocationSize bytes from Buffer to the newly allocated buffer, and returns a pointer to the
allocated buffer.  If AllocationSize is 0, then a valid buffer of 0 size is returned.  If there
is not enough memory remaining to satisfy the request, then NULL is returned.

If Buffer is NULL, then ASSERT().
If AllocationSize is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

@param  AllocationSize        The number of bytes to allocate and zero.
@param  Buffer                The buffer to copy to the allocated buffer.

@return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
AllocateCopyPool(
  IN UINTN       AllocationSize,
  IN CONST VOID  *Buffer
)
{
  void * ptr = calloc(AllocationSize, 1);
  if (NULL != ptr) {
    memcpy_s(ptr, AllocationSize, Buffer, AllocationSize);
  }
  return ptr;
}

/**
Reallocates a buffer of type EfiBootServicesData.

Allocates and zeros the number bytes specified by NewSize from memory of type
EfiBootServicesData.  If OldBuffer is not NULL, then the smaller of OldSize and
NewSize bytes are copied from OldBuffer to the newly allocated buffer, and
OldBuffer is freed.  A pointer to the newly allocated buffer is returned.
If NewSize is 0, then a valid buffer of 0 size is  returned.  If there is not
enough memory remaining to satisfy the request, then NULL is returned.

If the allocation of the new buffer is successful and the smaller of NewSize and OldSize
is greater than (MAX_ADDRESS - OldBuffer + 1), then ASSERT().

@param  OldSize        The size, in bytes, of OldBuffer.
@param  NewSize        The size, in bytes, of the buffer to reallocate.
@param  OldBuffer      The buffer to copy to the allocated buffer.  This is an optional
parameter that may be NULL.

@return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
EFIAPI
ReallocatePool(
  IN UINTN  OldSize,
  IN UINTN  NewSize,
  IN VOID   *OldBuffer  OPTIONAL
)
{
  return realloc(OldBuffer, NewSize);
}

/**
Safely append with automatic string resizing given length of Destination and
desired length of copy from Source.

Append the first D characters of Source to the end of Destination, where D is
the lesser of Count and the StrLen() of Source. If appending those D characters
will fit within Destination (whose Size is given as CurrentSize) and
still leave room for a NULL terminator, then those characters are appended,
starting at the original terminating NULL of Destination, and a new terminating
NULL is appended.

If appending D characters onto Destination will result in a overflow of the size
given in CurrentSize the string will be grown such that the copy can be performed
and CurrentSize will be updated to the new size.

If Source is NULL, there is nothing to append, so return the current buffer in
Destination.

If Destination is NULL, then ASSERT().
If Destination's current length (including NULL terminator) is already more than
CurrentSize, then ASSERT().

@param[in, out] Destination    The String to append onto.
@param[in, out] CurrentSize    On call, the number of bytes in Destination.  On
return, possibly the new size (still in bytes).  If NULL,
then allocate whatever is needed.
@param[in]      Source         The String to append from.
@param[in]      Count          The maximum number of characters to append.  If 0, then
all are appended.

@return                       The Destination after appending the Source.
**/
CHAR16*
EFIAPI
StrnCatGrow(
  IN OUT CHAR16           **Destination,
  IN OUT UINTN            *CurrentSize,
  IN     CONST CHAR16     *Source,
  IN     UINTN            Count
)
{
  UINTN DestinationStartSize;
  UINTN NewSize;

  //
  // ASSERTs
  //
  ASSERT(Destination != NULL);

  //
  // If there's nothing to do then just return Destination
  //
  if (Source == NULL) {
    return (*Destination);
  }

  //
  // allow for un-initialized pointers, based on size being 0
  //
  if (CurrentSize != NULL && *CurrentSize == 0) {
    *Destination = NULL;
  }

  //
  // allow for NULL pointers address as Destination
  //
  if (*Destination != NULL) {
    ASSERT(CurrentSize != 0);
    DestinationStartSize = StrSize(*Destination);
    ASSERT(DestinationStartSize <= *CurrentSize);
  }
  else {
    DestinationStartSize = 0;
    //    ASSERT(*CurrentSize == 0);
  }

  //
  // Append all of Source?
  //
  if (Count == 0) {
    Count = StrLen(Source);
  }

  //
  // Test and grow if required
  //
  if (CurrentSize != NULL) {
    NewSize = *CurrentSize;
    if (NewSize < DestinationStartSize + (Count * sizeof(CHAR16))) {
      while (NewSize < (DestinationStartSize + (Count * sizeof(CHAR16)))) {
        NewSize += 2 * Count * sizeof(CHAR16);
      }
      *Destination = ReallocatePool(*CurrentSize, NewSize, *Destination);
      *CurrentSize = NewSize;
    }
  }
  else {
    *Destination = AllocateZeroPool((Count + 1) * sizeof(CHAR16));
  }

  //
  // Now use standard StrnCat on a big enough buffer
  //
  if (*Destination == NULL) {
    return (NULL);
  }
  return StrnCat(*Destination, Source, Count);
}

/**
Fills a target buffer with a byte value, and returns the target buffer.

This function fills Length bytes of Buffer with Value, and returns Buffer.

If Length is greater than (MAX_ADDRESS - Buffer + 1), then ASSERT().

@param  Buffer    The memory to set.
@param  Length    The number of bytes to set.
@param  Value     The value with which to fill Length bytes of Buffer.

@return Buffer.

**/
VOID *
EFIAPI
SetMem(
  OUT VOID  *Buffer,
  IN UINTN  Length,
  IN UINT8  Value
)
{
  memset(Buffer, Value, Length);
  return Buffer;
}

/**
Prints an assert message containing a filename, line number, and description.
This may be followed by a breakpoint or a dead loop.

Print a message of the form "ASSERT <FileName>(<LineNumber>): <Description>\n"
to the debug output device.  If DEBUG_PROPERTY_ASSERT_BREAKPOINT_ENABLED bit of
PcdDebugProperyMask is set then CpuBreakpoint() is called. Otherwise, if
DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED bit of PcdDebugProperyMask is set then
CpuDeadLoop() is called.  If neither of these bits are set, then this function
returns immediately after the message is printed to the debug output device.
DebugAssert() must actively prevent recursion.  If DebugAssert() is called while
processing another DebugAssert(), then DebugAssert() must return immediately.

If FileName is NULL, then a <FileName> string of "(NULL) Filename" is printed.
If Description is NULL, then a <Description> string of "(NULL) Description" is printed.

@param  FileName     The pointer to the name of the source file that generated the assert condition.
@param  LineNumber   The line number in the source file that generated the assert condition
@param  Description  The pointer to the description of the assert condition.

**/
VOID
EFIAPI
DebugAssert(
  IN CONST CHAR8  *FileName,
  IN UINTN        LineNumber,
  IN CONST CHAR8  *Description
)
{
}

/**
Returns TRUE if ASSERT() macros are enabled.

This function returns TRUE if the DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of
PcdDebugProperyMask is set.  Otherwise FALSE is returned.

@retval  TRUE    The DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is set.
@retval  FALSE   The DEBUG_PROPERTY_DEBUG_ASSERT_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugAssertEnabled(
  VOID
)
{
  return FALSE;
}

/**
Returns TRUE if DEBUG() macros are enabled.

This function returns TRUE if the DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of
PcdDebugProperyMask is set.  Otherwise FALSE is returned.

@retval  TRUE    The DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of PcdDebugProperyMask is set.
@retval  FALSE   The DEBUG_PROPERTY_DEBUG_PRINT_ENABLED bit of PcdDebugProperyMask is clear.

**/
BOOLEAN
EFIAPI
DebugPrintEnabled(
  VOID
)
{
  return FALSE;
}

/**
The constructor function caches the pointer of Boot Services Table.

The constructor function caches the pointer of Boot Services Table through System Table.
It will ASSERT() if the pointer of System Table is NULL.
It will ASSERT() if the pointer of Boot Services Table is NULL.
It will always return EFI_SUCCESS.

@param  ImageHandle   The firmware allocated handle for the EFI image.
@param  SystemTable   A pointer to the EFI System Table.

@retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiBootServicesTableLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
The constructor function caches the pointer to DevicePathUtilites protocol,
DevicePathToText protocol and DevicePathFromText protocol.

The constructor function locates these three protocols from protocol database.
It will caches the pointer to local protocol instance if that operation fails
and it will always return EFI_SUCCESS.

@param  ImageHandle   The firmware allocated handle for the EFI image.
@param  SystemTable   A pointer to the EFI System Table.

@retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiDevicePathLibOptionalDevicePathProtocolConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
The constructor function caches the pointer of Runtime Services Table.

The constructor function caches the pointer of Runtime Services Table.
It will ASSERT() if the pointer of Runtime Services Table is NULL.
It will always return EFI_SUCCESS.

@param  ImageHandle   The firmware allocated handle for the EFI image.
@param  SystemTable   A pointer to the EFI System Table.

@retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiRuntimeServicesTableLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
The constructor function retrieves pointers to the UEFI HII protocol instances

The constructor function retrieves pointers to the four UEFI HII protocols from the
handle database.  These include the UEFI HII Font Protocol, the UEFI HII String
Protocol, the UEFI HII Image Protocol, the UEFI HII Database Protocol, and the
UEFI HII Config Routing Protocol.  This function always return EFI_SUCCESS.
All of these protocols are optional if the platform does not support configuration
and the UEFI HII Image Protocol and the UEFI HII Font Protocol are optional if
the platform does not support a graphical console.  As a result, the consumers
of this library much check the protocol pointers againt NULL before using them,
or use dependency expressions to guarantee that some of them are present before
assuming they are not NULL.

@param  ImageHandle   The firmware allocated handle for the EFI image.
@param  SystemTable   A pointer to the EFI System Table.

@retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.

**/
EFI_STATUS
EFIAPI
UefiHiiServicesLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Empty constructor function that is required to resolve dependencies between
libraries.

** DO NOT REMOVE **

@param  ImageHandle   The firmware allocated handle for the EFI image.
@param  SystemTable   A pointer to the EFI System Table.

@retval EFI_SUCCESS   The constructor executed correctly.

**/
EFI_STATUS
EFIAPI
UefiLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
This function frees the table of Unicode strings in UnicodeStringTable.

If UnicodeStringTable is NULL, then EFI_SUCCESS is returned.
Otherwise, each language code, and each Unicode string in the Unicode string
table are freed, and EFI_SUCCESS is returned.

@param  UnicodeStringTable  A pointer to the table of Unicode strings.

@retval EFI_SUCCESS         The Unicode string table was freed.

**/
EFI_STATUS
EFIAPI
FreeUnicodeStringTable(
  IN EFI_UNICODE_STRING_TABLE  *UnicodeStringTable
)
{
  return 0;
}

/**
Constructor for the library.

@param[in] ImageHandle    Ignored.
@param[in] SystemTable    Ignored.

@retval EFI_SUCCESS   The operation was successful.
**/
EFI_STATUS
EFIAPI
HandleParsingLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Constructor for the Shell Command library.

Initialize the library and determine if the underlying is a UEFI Shell 2.0 or an EFI shell.

@param ImageHandle    the image handle of the process
@param SystemTable    the EFI System Table pointer

@retval EFI_SUCCESS   the initialization was complete sucessfully
**/
EFI_STATUS
EFIAPI
ShellLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Constructor for the Shell Command library.

Initialize the library and determine if the underlying is a UEFI Shell 2.0 or an EFI shell.

@param ImageHandle    the image handle of the process
@param SystemTable    the EFI System Table pointer

@retval EFI_SUCCESS   the initialization was complete sucessfully
**/
EFI_STATUS
EFIAPI
ShellCommandLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Constructor for the Shell Debug1 Commands library.

@param ImageHandle    the image handle of the process
@param SystemTable    the EFI System Table pointer

@retval EFI_SUCCESS        the shell command handlers were installed sucessfully
@retval EFI_UNSUPPORTED    the shell level required was not found.
**/
EFI_STATUS
EFIAPI
UefiShellDebug1CommandsLibConstructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Destructor for the library.  free any resources.

@param ImageHandle            The image handle of the process.
@param SystemTable            The EFI System Table pointer.
**/
EFI_STATUS
EFIAPI
UefiShellDebug1CommandsLibDestructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Destructor for the library.  free any resources.

@param ImageHandle    the image handle of the process
@param SystemTable    the EFI System Table pointer

@retval RETURN_SUCCESS this function always returns success
**/
EFI_STATUS
EFIAPI
ShellCommandLibDestructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Destructor for the library.  free any resources.

@param[in] ImageHandle  A copy of the ImageHandle.
@param[in] SystemTable  A pointer to the SystemTable for the application.

@retval EFI_SUCCESS   The operation was successful.
@return               An error from the CloseProtocol function.
**/
EFI_STATUS
EFIAPI
ShellLibDestructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}

/**
Destructor for the library.  free any resources.

@param[in] ImageHandle    Ignored.
@param[in] SystemTable    Ignored.

@retval EFI_SUCCESS   The operation was successful.
**/
EFI_STATUS
EFIAPI
HandleParsingLibDestructor(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  return 0;
}
#define MAX_PROMT_INPUT_SZ 1024
#define RETURN_KEY	0xD
#define LINE_FEED 0xA

/**
Prompted input request

@param[in] pPrompt - information about expected input
@param[in] ShowInput - Show characters written by user
@param[in] OnlyAlphanumeric - Allow only for alphanumeric characters
@param[out] ppReturnValue - is a pointer to a pointer to the 16-bit character string
that will contain the return value

@retval - Appropriate CLI return code
**/
EFI_STATUS
PromptedInput(
  IN     CHAR16 *pPrompt,
  IN     BOOLEAN ShowInput,
  IN     BOOLEAN OnlyAlphanumeric,
  OUT CHAR16 **ppReturnValue
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int PromptIndex;
  char ThrowAway;
  BOOLEAN NoReturn = TRUE;

  NVDIMM_ENTRY();

  if (pPrompt == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Print(L"%ls", pPrompt);
  char buff[MAX_PROMT_INPUT_SZ];
  memset(buff, 0, MAX_PROMT_INPUT_SZ);

  for (PromptIndex = 0; PromptIndex < (MAX_PROMT_INPUT_SZ - 1); ++PromptIndex) {
    buff[PromptIndex] = _getch();
    if (RETURN_KEY == buff[PromptIndex] || LINE_FEED == buff[PromptIndex]) {
      //terminate string, advance index to indicate size
      buff[PromptIndex++] = '\0';
      NoReturn = FALSE;
      break;
    }
  }

  *ppReturnValue = NULL;
  while (NoReturn) {
    //we ran out of buffer before user pressed Enter
    //consume stdin until Enter
    ThrowAway = _getch();

    if (RETURN_KEY == ThrowAway || LINE_FEED == ThrowAway) {
      ReturnCode = EFI_BUFFER_TOO_SMALL;
      goto Finish;
    }
  }

  VOID * ptr = AllocateZeroPool((PromptIndex * (sizeof(CHAR16))));
  if (NULL == ptr) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  *ppReturnValue = AsciiStrToUnicodeStr(buff, ptr);

Finish:
  Print(L"\n");
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Display "yes/no" question and retrieve reply using prompt mechanism

@param[out] pConfirmation Confirmation from prompt

@retval EFI_INVALID_PARAMETER One or more parameters are invalid
@retval EFI_SUCCESS All Ok
**/
EFI_STATUS
PromptYesNo(
  OUT BOOLEAN *pConfirmation
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pPromptReply = NULL;
  BOOLEAN ValidInput = FALSE;
  char buf[10];
  int readSize = 0;

  NVDIMM_ENTRY();

  if (pConfirmation == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  PrintNoBuffer(L"%ls", PROMPT_CONTINUE_QUESTION);
  if (0 >= (readSize = _read(0, buf, sizeof(buf))))
  {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ValidInput = readSize == 2 &&
    (buf[0] == 'y' || buf[0] == 'n');
  if (!ValidInput) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (buf[0] == 'y') {
    *pConfirmation = TRUE;
  }
  else {
    *pConfirmation = FALSE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPromptReply);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Produces a Null-terminated Unicode string in an output buffer based on a Null-terminated
Unicode format string and variable argument list.

Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
and BufferSize.
The Unicode string is produced by parsing the format string specified by FormatString.
Arguments are pulled from the variable argument list based on the contents of the format string.
The number of Unicode characters in the produced output buffer is returned, not including
the Null-terminator.
If BufferSize is 0 or 1, then no output buffer is produced and 0 is returned.

If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT().
If BufferSize > 1 and StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().
If BufferSize > 1 and FormatString is NULL, then ASSERT().
If BufferSize > 1 and FormatString is not aligned on a 16-bit boundary, then ASSERT().
If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
ASSERT().
If PcdMaximumUnicodeStringLength is not zero, and produced Null-terminated Unicode string
contains more than PcdMaximumUnicodeStringLength Unicode characters not including the
Null-terminator, then ASSERT().

@param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
Unicode string.
@param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
@param  FormatString    A null-terminated Unicode format string.
@param  ...             The variable argument list whose contents are accessed based on the
format string specified by FormatString.

@return The number of Unicode characters in the produced output buffer, not including the
Null-terminator.

**/
UINTN
EFIAPI
UnicodeSPrint(
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  ...
)
{
  VA_LIST Marker;
  VA_START(Marker, FormatString);
  return vswprintf_s(StartOfBuffer, BufferSize / sizeof(CHAR16), FormatString, Marker);
}

/**
Produces a Null-terminated Unicode string in an output buffer based on
a Null-terminated Unicode format string and a VA_LIST argument list.

Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
and BufferSize.
The Unicode string is produced by parsing the format string specified by FormatString.
Arguments are pulled from the variable argument list specified by Marker based on the
contents of the format string.
The number of Unicode characters in the produced output buffer is returned, not including
the Null-terminator.
If BufferSize is 0 or 1, then no output buffer is produced and 0 is returned.

If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT().
If BufferSize > 1 and StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().
If BufferSize > 1 and FormatString is NULL, then ASSERT().
If BufferSize > 1 and FormatString is not aligned on a 16-bit boundary, then ASSERT().
If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
PcdMaximumUnicodeStringLength Unicode characters, not including the Null-terminator, then
ASSERT().
If PcdMaximumUnicodeStringLength is not zero, and produced Null-terminated Unicode string
contains more than PcdMaximumUnicodeStringLength Unicode characters, not including the
Null-terminator, then ASSERT().

@param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
Unicode string.
@param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
@param  FormatString    Null-terminated Unicode format string.
@param  Marker          VA_LIST marker for the variable argument list.

@return The number of Unicode characters in the produced output buffer, not including the
Null-terminator.

**/
UINTN
EFIAPI
UnicodeVSPrint(
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN          BufferSize,
  IN  CONST CHAR16  *FormatString,
  IN  VA_LIST        Marker
)
{
  return vswprintf_s(StartOfBuffer, BufferSize / sizeof(CHAR16), FormatString, Marker);
}

/**
Produces a Null-terminated ASCII string in an output buffer based on a Null-terminated
ASCII format string and  variable argument list.

Produces a Null-terminated ASCII string in the output buffer specified by StartOfBuffer
and BufferSize.
The ASCII string is produced by parsing the format string specified by FormatString.
Arguments are pulled from the variable argument list based on the contents of the
format string.
The number of ASCII characters in the produced output buffer is returned not including
the Null-terminator.
If BufferSize is 0, then no output buffer is produced and 0 is returned.

If BufferSize > 0 and StartOfBuffer is NULL, then ASSERT().
If BufferSize > 0 and FormatString is NULL, then ASSERT().
If PcdMaximumAsciiStringLength is not zero, and FormatString contains more than
PcdMaximumAsciiStringLength ASCII characters not including the Null-terminator, then
ASSERT().
If PcdMaximumAsciiStringLength is not zero, and produced Null-terminated ASCII string
contains more than PcdMaximumAsciiStringLength ASCII characters not including the
Null-terminator, then ASSERT().

@param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
ASCII string.
@param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
@param  FormatString    A Null-terminated ASCII format string.
@param  ...             Variable argument list whose contents are accessed based on the
format string specified by FormatString.

@return The number of ASCII characters in the produced output buffer not including the
Null-terminator.

**/
UINTN
EFIAPI
AsciiSPrint(
  OUT CHAR8        *StartOfBuffer,
  IN  UINTN        BufferSize,
  IN  CONST CHAR8  *FormatString,
  ...
)
{
  VA_LIST Marker;
  VA_START(Marker, FormatString);
  return vsnprintf_s(StartOfBuffer, BufferSize
#ifdef _MSC_VER
    , BufferSize - 1
#endif
    , FormatString, Marker);
}
/**
Returns the number of characters that would be produced by if the formatted
output were produced not including the Null-terminator.

If Format is NULL, then ASSERT().
If Format is not aligned on a 16-bit boundary, then ASSERT().

@param[in]  FormatString    A Null-terminated Unicode format string.
@param[in]  Marker          VA_LIST marker for the variable argument list.

@return The number of characters that would be produced, not including the
Null-terminator.
**/
UINTN
EFIAPI
SPrintLength(
  IN  CONST CHAR16   *FormatString,
  IN  VA_LIST         Marker
)
{
  static const int nBuffSprintLenSize = 1024;
  static wchar_t evalSprintBuff[1024];
  return vswprintf_s(evalSprintBuff, nBuffSprintLenSize, FormatString, Marker);
}
/**
Makes Bios emulated pass thru call and returns the values

@param[in]  pDimm    pointer to current Dimm
@param[out] pBsrValue   Value from passthru

@retval EFI_SUCCESS  The count was returned properly
@retval EFI_INVALID_PARAMETER One or more parameters are NULL
@retval Other errors failure of FW commands
**/

EFI_STATUS
EFIAPI
FwCmdGetBsr(DIMM *pDimm, UINT64 *pBsrValue)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  FW_CMD *pFwCmd = NULL;
  if (pBsrValue == NULL || pDimm == NULL) {
    goto Finish;
  }
  pFwCmd = AllocateZeroPool(sizeof(*pFwCmd));
  if (pFwCmd == NULL) {
    goto Finish;
  }
  pFwCmd->DimmID = pDimm->DimmID;
  pFwCmd->Opcode = BIOS_EMULATED_COMMAND;
  pFwCmd->SubOpcode = SUBOP_GET_BOOT_STATUS;
  pFwCmd->OutputPayloadSize = sizeof(unsigned long long);
  ReturnCode = PassThru(pDimm, pFwCmd, PT_TIMEOUT_INTERVAL);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }
  CopyMem_S(pBsrValue, sizeof(*pBsrValue), pFwCmd->OutPayload, sizeof(UINT64));

Finish:
  FREE_POOL_SAFE(pFwCmd);
  return ReturnCode;
}

VOID
EFIAPI
GetVendorDriverVersion(CHAR16 * pVersion, UINTN VersionStrSize)
{
  char ascii_buffer[100];

  if (0 == get_vendor_driver_revision(ascii_buffer, sizeof(ascii_buffer)))
  {
    AsciiStrToUnicodeStr(ascii_buffer, pVersion);
  }
  else
  {
    UnicodeSPrint(pVersion, VersionStrSize, L"0.0.0.0");
  }
}

VOID
AsmSfence(
)
{
}

