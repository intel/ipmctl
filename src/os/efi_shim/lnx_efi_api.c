/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
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

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

extern int get_acpi_table(const char *signature, struct acpi_table *p_table, const unsigned int size);

EFI_STATUS
EFIAPI
PassThru(
	IN     struct _DIMM *pDimm,
	IN OUT FW_CMD *pCmd,
	IN     UINT64 Timeout
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT32 ReturnCode;
  UINT32 DimmID;

  if(!pDimm || !pCmd)
    return EFI_INVALID_PARAMETER;

  DimmID = pCmd->DimmID;

  pCmd->DimmID = pDimm->DeviceHandle.AsUint32;
  ReturnCode = ioctl_passthrough_fw_cmd((struct fw_cmd *)pCmd);
  if (0 == ReturnCode)
  {
      Rc = EFI_SUCCESS;
  }
  else
  {
      Rc = EFI_DEVICE_ERROR;
  }

  pCmd->DimmID = DimmID;
  return Rc;
}


EFI_STATUS
initAcpiTables(
)
{
    EFI_ACPI_DESCRIPTION_HEADER *PtrNfitTable = NULL;
    EFI_ACPI_DESCRIPTION_HEADER *PtrPcatTable = NULL;
    EFI_ACPI_DESCRIPTION_HEADER *PtrPMTTTable = NULL;
    INT32 BuffSize = 0;
    INT32 Result = 0;
    EFI_STATUS ReturnCode;
	UINT32 lastErr;
    BuffSize = get_acpi_table("NFIT", NULL, BuffSize);
    if (BuffSize > 0)
    {
        PtrNfitTable = AllocatePool(BuffSize);
        if (NULL != PtrNfitTable)
        {
            Result = get_acpi_table("NFIT", (struct acpi_table*)PtrNfitTable, BuffSize);
            if (0 > Result)
            {
                return EFI_LOAD_ERROR;
            }
        }
        else
            return EFI_OUT_OF_RESOURCES;
    }
	else if (BuffSize == 0)
	{
		lastErr = errno;
	}
    BuffSize = get_acpi_table("PCAT", NULL, BuffSize);
    if (BuffSize > 0)
    {
        PtrPcatTable = AllocatePool(BuffSize);
        if (NULL != PtrPcatTable)
        {
            Result = get_acpi_table("PCAT", (struct acpi_table*)PtrPcatTable, BuffSize);
            if (0 > Result)
            {
                return EFI_LOAD_ERROR;
            }
        }
        else
            return EFI_OUT_OF_RESOURCES;
    }

    BuffSize = get_acpi_table("PMTT", NULL, BuffSize);
    if (BuffSize > 0)
    {
      PtrPMTTTable = AllocatePool(BuffSize);
      if (NULL != PtrPMTTTable)
      {
        Result = get_acpi_table("PMTT", (struct acpi_table*)PtrPMTTTable, BuffSize);
        if (0 > Result)
        {
          return EFI_LOAD_ERROR;
        }
      }
      else
        return EFI_OUT_OF_RESOURCES;
    }

    /**
    Find the NVDIMM FW Interface Table (NFIT), PCAT & PMTT
    **/
    ReturnCode = ParseAcpiTables(PtrNfitTable, PtrPcatTable, PtrPMTTTable, &gNvmDimmData->PMEMDev.pFitHead,
      &gNvmDimmData->PMEMDev.pPcatHead, &gNvmDimmData->PMEMDev.IsMemModeAllowedByBios);
    if (EFI_ERROR(ReturnCode))
    {
        NVDIMM_WARN("Failed to parse NFIT or PCAT or PMTT table.");
        return EFI_NOT_FOUND;
    }
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

	fclose(dmi_file);

	return rc;
}

UINT8 *gSmbiosTable = NULL;
size_t gSmbiosTableSize = 0;
UINT8 gSmbiosMinorVersion = 0;
UINT8 gSmbiosMajorVersion = 0;

VOID
GetFirstAndBoundSmBiosStructPointer(
	OUT SMBIOS_STRUCTURE_POINTER *pSmBiosStruct,
	OUT SMBIOS_STRUCTURE_POINTER *pLastSmBiosStruct,
	OUT SMBIOS_VERSION *pSmbiosVersion
)
{
	int rc = 0;

	if (pSmBiosStruct == NULL || pLastSmBiosStruct == NULL || pSmbiosVersion == NULL) {
		return;
	}

	// One time initialization
	if (NULL == gSmbiosTable)
	{
		rc = get_smbios_table_alloc(&gSmbiosTable, &gSmbiosTableSize, &gSmbiosMajorVersion, &gSmbiosMinorVersion);
	}

	if (rc == 0)
	{
		pSmBiosStruct->Raw = (UINT8 *)gSmbiosTable;
		pLastSmBiosStruct->Raw = pSmBiosStruct->Raw + gSmbiosTableSize;
		pSmbiosVersion->Major = gSmbiosMajorVersion;
		pSmbiosVersion->Minor = gSmbiosMinorVersion;
	}
}
