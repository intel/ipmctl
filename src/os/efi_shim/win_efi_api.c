/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Uefi.h>
#include <Dimm.h>
#include <win_scm2_passthrough.h>
#include <NvmDimmDriver.h>

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

extern UINT32 win_scm2_ioctl_get_system_table(
  IN  UINT32 FirmwareTableProviderSignature,
  IN  UINT32 FirmwareTableID,
  OUT VOID * pFirmwareTableBuffer,
  IN  UINT32 BufferSize
);

extern UINT8 *gSmbiosTable;
extern size_t gSmbiosTableSize;
extern UINT8 gSmbiosMinorVersion;
extern UINT8 gSmbiosMajorVersion;

extern UINT32 win_scm2_ioctl_get_last_error();

/*
* Standard Windows structure for SMBIOS data.
* GetSystemFirmwareTable returns a blob in this format.
*/
__pragma(pack(push, 1))
struct RawSMBIOSData
{
  UINT8    Used20CallingMethod;
  UINT8    SMBIOSMajorVersion;
  UINT8    SMBIOSMinorVersion;
  UINT8    DmiRevision;
  UINT32   Length;
  UINT8    SMBIOSTableData[];
};
__pragma(pack(pop))

/**
Loads a table as specified in the args

@param[in]  tableProviderSig - the table signature
@param[in]  tableId - the table id
@param[out] table - EFI_ACPI_DESCRIPTION_HEADER the table

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
get_table(
  IN UINT32 tableProviderSig,
  IN UINT32 tableId,
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table
);

EFI_STATUS
EFIAPI
passthru_os(
  IN     struct _DIMM *pDimm,
  IN OUT FW_CMD *pCmd,
  IN     UINT64 Timeout
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 ReturnCode;
  unsigned int dsm_status;

  ReturnCode = win_scm2_passthrough((struct fw_cmd *)pCmd, &dsm_status);
  if (0 == ReturnCode && 0 == dsm_status)
  {
    Rc = EFI_SUCCESS;
  }
  else
  {
    Rc = EFI_DEVICE_ERROR;
    pCmd->Status = DSM_EXTENDED_ERROR(dsm_status);
  }

  return Rc;
}

EFI_STATUS
get_nfit_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER * table
)
{
  return get_table('ACPI', 'TIFN', table);
}

EFI_STATUS
get_pcat_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER * table
)
{
  return get_table('ACPI', 'TACP', table);
}

EFI_STATUS
get_pmtt_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER * table
)
{
  return get_table('ACPI', 'TTMP', table);
}

EFI_STATUS
get_table(
  IN UINT32 tableProviderSig,
  IN UINT32 tableId,
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table
)
{
  *table = NULL;

  UINT32 buf_size = 0;
  buf_size = win_scm2_ioctl_get_system_table(tableProviderSig, tableId, NULL, buf_size);
  if (buf_size == 0)
  {
    return EFI_END_OF_FILE;
  }

  *table = AllocatePool(buf_size);
  if (NULL == *table)
  {
    return EFI_END_OF_FILE;
  }

  win_scm2_ioctl_get_system_table(tableProviderSig, tableId, *table, buf_size);
 
  return EFI_SUCCESS;
}

UINT32 string_to_dword(const char *str)
{
  union
  {
    UINT32 dword;
    char string[4];
  } fw_table_signature;

  memmove(fw_table_signature.string, str, sizeof(fw_table_signature.string));

  return fw_table_signature.dword;
}

/*
* Get the UINT32-formatted Windows signature for fetching the SMBIOS table
*/
UINT32 get_smbios_table_signature()
{
  // Endian-flipped "RSMB"
  static const char *SMBIOS_TABLE_SIGNATURE = "BMSR";
  return string_to_dword(SMBIOS_TABLE_SIGNATURE);
}

size_t allocate_smbios_table_from_raw_data(const struct RawSMBIOSData *p_data,
  UINT8 **pp_smbios_table)
{
  //COMMON_LOG_ENTRY();
  size_t allocated_size = (size_t)p_data->Length;

  *pp_smbios_table = calloc(1, allocated_size);
  if (*pp_smbios_table)
  {
    memmove(*pp_smbios_table, p_data->SMBIOSTableData, allocated_size);
  }
  else
  {
    //COMMON_LOG_ERROR_F("Failed to allocate memory for SMBIOS table of size %llu",
    //	allocated_size);
    allocated_size = 0;
  }

  //COMMON_LOG_EXIT();
  return allocated_size;
}

/*
* Helper function to get a copy of the SMBIOS table dynamically allocated to the passed-in pointer.
* p_allocated_size returns the size of the allocated data.
* Caller is assumed to have passed non-NULL inputs.
*/
int get_smbios_table_alloc(UINT8 **pp_smbios_table, size_t *p_allocated_size, UINT8 *major_version, UINT8 *minor_version)
{
  //COMMON_LOG_ENTRY();
  int rc = NVM_SUCCESS;

  UINT32 smbios_sig = get_smbios_table_signature();
  UINT32 buf_size = win_scm2_ioctl_get_system_table(smbios_sig, 0, NULL, 0);
  if (buf_size > 0)
  {
    UINT8 *smbios_table_buf = malloc(buf_size * sizeof(UINT8));
    if (NULL != smbios_table_buf) {
      UINT32 size_fetched = win_scm2_ioctl_get_system_table(smbios_sig, 0,
        smbios_table_buf, buf_size);
      if (size_fetched == 0)
      {
        //COMMON_LOG_ERROR_F(
        //	"Windows reported no SMBIOS table after reporting a size (size = %u)",
        //	buf_size);
        rc = NVM_LAST_STATUS_VALUE;
      }
      else
      {
        struct RawSMBIOSData *smbios_table = (struct RawSMBIOSData *)smbios_table_buf;

        *p_allocated_size = allocate_smbios_table_from_raw_data(smbios_table, pp_smbios_table);
        if (*p_allocated_size == 0)
        {
          rc = NVM_LAST_STATUS_VALUE;
        }
        *major_version = smbios_table->SMBIOSMajorVersion;
        *minor_version = smbios_table->SMBIOSMinorVersion;
      }

      free(smbios_table_buf);
    }
    else {
      rc = NVM_LAST_STATUS_VALUE;
    }
  }
  else
  {
    //COMMON_LOG_ERROR("Windows reported no SMBIOS table (size = 0)");
    rc = NVM_LAST_STATUS_VALUE;
  }

  //COMMON_LOG_EXIT_RETURN_I(rc);
  return rc;
}

UINT32
get_smbios_table(
)
{
  return get_smbios_table_alloc(&gSmbiosTable, &gSmbiosTableSize, &gSmbiosMajorVersion, &gSmbiosMinorVersion);
}
