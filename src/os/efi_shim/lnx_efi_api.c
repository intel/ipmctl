/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <Uefi.h>
#include <Dimm.h>
#include <NvmDimmDriver.h>
#include <errno.h>
#include <lnx_acpi.h>
#include <lnx_smbios_types.h>
#include <lnx_adapter_passthrough.h>

#define SMBIOS_ENTRY_POINT_FILE "/sys/firmware/dmi/tables/smbios_entry_point"
#define SMBIOS_DMI_FILE "/sys/firmware/dmi/tables/DMI"

unsigned char SMBIOS_ANCHOR_STR[] = { 0x5f, 0x53, 0x4d, 0x5f };
unsigned char SMBIOS_3_ANCHOR_STR[] = { 0x5f, 0x53, 0x4d, 0x33, 0x5f };


extern UINT8 *gSmbiosTable;
extern size_t gSmbiosTableSize;
extern UINT8 gSmbiosMinorVersion;
extern UINT8 gSmbiosMajorVersion;


extern int get_acpi_table(const char *signature, struct acpi_table *p_table, const unsigned int size);

/**
Gets the current timestamp in terms of milliseconds
**/
UINT64 GetCurrentMilliseconds()
{
  UINT64 retval = 0;

  time_t s;  // Seconds
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);

  s = spec.tv_sec;
  retval = spec.tv_nsec / 1.0e6; // Convert nanoseconds to milliseconds
  if (retval > 999) {
    s++;
    retval = 0;
  }

  retval = (spec.tv_sec * 1000) + retval;

  return retval;
}

/**
Loads a table as specified in the args

@param[in]  currentTableName - the name of the table to load
@param[out] table - EFI_ACPI_DESCRIPTION_HEADER the table

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
get_table(
  IN CHAR8* currentTableName,
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
);

EFI_STATUS
passthru_os(
  IN     struct _DIMM *pDimm,
  IN OUT NVM_FW_CMD *pCmd,
  IN     long Timeout
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 ReturnCode;

  ReturnCode = ioctl_passthrough_fw_cmd((struct fw_cmd *)pCmd);
  if (0 == ReturnCode)
  {
    Rc = EFI_SUCCESS;
  }
  else
  {
    Rc = EFI_DEVICE_ERROR;
  }

  return Rc;
}

EFI_STATUS
get_nfit_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
)
{
  return get_table("NFIT", table, tablesize);
}

EFI_STATUS
get_pcat_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
)
{
  return get_table("PCAT", table, tablesize);
}

EFI_STATUS
get_pmtt_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
)
{
  return get_table("PMTT", table, tablesize);
}

EFI_STATUS
get_table(
  IN CHAR8* currentTableName,
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
)
{
  if (NULL == currentTableName || NULL == tablesize || NULL == table)
  {
    return EFI_INVALID_PARAMETER;
  }

  *table = NULL;

  int buf_size = get_acpi_table(currentTableName, NULL, 0);
  if (buf_size <= 0)
  {
    return EFI_END_OF_FILE;
  }

  *table = AllocatePool(buf_size);
  if (NULL == *table)
  {
    return EFI_END_OF_FILE;
  }
  *tablesize = (UINT32)buf_size;
  get_acpi_table(currentTableName, (struct acpi_table*)*table, buf_size);
  return EFI_SUCCESS;
}

/*
* Harvest the raw SMBIOS table data from memory and allocate a copy
* to parse.
*/
int get_smbios_table_alloc(UINT8 **pp_smbios_table, size_t *p_allocated_size, UINT8 *major_version, UINT8 *minor_version)
{
  int rc = 0;
  size_t entry_size;
  size_t table_length;

  // set buffer to larger of the possible structs
  char entry_point_buffer[sizeof(struct smbios_entry_point)];
  memset(entry_point_buffer, 0, sizeof(struct smbios_entry_point));

  FILE *entry_file = fopen(SMBIOS_ENTRY_POINT_FILE, "r");
  if (entry_file == NULL)
  {
    NVDIMM_ERR("Couldn't open SMBIOS entry point file");
    return -EIO;
  }

  entry_size = fread(entry_point_buffer, 1, sizeof(struct smbios_entry_point), entry_file);

  struct smbios_entry_point *smbios = ((struct smbios_entry_point *) entry_point_buffer);
  struct smbios_3_entry_point *smbios_3 = ((struct smbios_3_entry_point *) entry_point_buffer);
  if ((memcmp(smbios->anchor_str, SMBIOS_ANCHOR_STR, sizeof(SMBIOS_ANCHOR_STR)) == 0) &&
    (entry_size == sizeof(struct smbios_entry_point)))
  {
    table_length = smbios->structure_table_length;
    *major_version = smbios->smbios_major_version;
    *minor_version = smbios->smbios_minor_version;
  }
  else if ((memcmp(smbios_3->anchor_str, SMBIOS_3_ANCHOR_STR, sizeof(SMBIOS_3_ANCHOR_STR)) == 0) &&
    (entry_size == sizeof(struct smbios_3_entry_point)))
  {
    table_length = smbios_3->structure_table_max_length;
    *major_version = smbios_3->smbios_major_version;
    *minor_version = smbios_3->smbios_minor_version;
  }
  else
  {
    NVDIMM_DBG("Couldn't find SMBIOS entry point from sysfs");
    fclose(entry_file);
    return -ENXIO;
  }

  fclose(entry_file);

  FILE *dmi_file = fopen(SMBIOS_DMI_FILE, "r");
  if (dmi_file == NULL)
  {
    NVDIMM_ERR("Couldn't open SMBIOS DMI file");
    return -EIO;
  }

  UINT8 *p_smbios_table = calloc(1, table_length);
  if (NULL != p_smbios_table) {
    if (fread(p_smbios_table, 1, table_length, dmi_file) == table_length)
    {
      *pp_smbios_table = p_smbios_table;
      *p_allocated_size = table_length;
    }
    else
    {
      NVDIMM_ERR("Could not read SMBIOS DMI from sysfs");
      free(p_smbios_table);
      fclose(dmi_file);
      return -ENXIO;
    }
  }
  fclose(dmi_file);

  return rc;
}

UINT32
get_smbios_table(
)
{
  return get_smbios_table_alloc(&gSmbiosTable, &gSmbiosTableSize, &gSmbiosMajorVersion, &gSmbiosMinorVersion);
}

UINT32
get_first_arg_from_va_list(VA_LIST args)
{
  return VA_ARG(args, UINT32);
}
