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
	unsigned int dsm_status;
	UINT32 DimmID = pCmd->DimmID;

	pCmd->DimmID = pDimm->DeviceHandle.AsUint32;
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
    UINT32 BuffSize = 0;
    UINT32 Result = 0;
    EFI_STATUS ReturnCode;
	  UINT32 lastErr;

    BuffSize = win_scm2_ioctl_get_system_table('ACPI', 'TIFN', NULL, BuffSize);
    if (BuffSize > 0)
    {
        PtrNfitTable = AllocatePool(BuffSize);
        if (NULL != PtrNfitTable)
        {
            Result = win_scm2_ioctl_get_system_table('ACPI', 'TIFN', PtrNfitTable, BuffSize);
            if (0 == Result)
            {
                return EFI_LOAD_ERROR;
            }
        }
        else
            return EFI_OUT_OF_RESOURCES;
    }
	else if (BuffSize == 0)
	{
		lastErr = win_scm2_ioctl_get_last_error();
	}
    BuffSize = win_scm2_ioctl_get_system_table('ACPI', 'TACP', NULL, BuffSize);
    if (BuffSize > 0)
    {
        PtrPcatTable = AllocatePool(BuffSize);
        if (NULL != PtrPcatTable)
        {
            Result = win_scm2_ioctl_get_system_table('ACPI', 'TACP', PtrPcatTable, BuffSize);
            if (0 == Result)
            {
                return EFI_LOAD_ERROR;
            }
        }
        else
            return EFI_OUT_OF_RESOURCES;
    }
    BuffSize = win_scm2_ioctl_get_system_table('ACPI', 'TTMP', NULL, BuffSize);
    if (BuffSize > 0)
    {
      PtrPMTTTable = AllocatePool(BuffSize);
      if (NULL != PtrPMTTTable)
      {
        Result = win_scm2_ioctl_get_system_table('ACPI', 'TTMP', PtrPMTTTable, BuffSize);
        if (0 == Result)
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
    if (EFI_ERROR(ReturnCode)) {
        NVDIMM_WARN("Failed to parse NFIT or PCAT or PMTT table.");
        return EFI_NOT_FOUND;
    }
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
	int rc = NVM_SUCCESS;

	if (pSmBiosStruct == NULL || pLastSmBiosStruct == NULL || pSmbiosVersion == NULL) {
		return;
	}

	// One time initialization
	if (NULL == gSmbiosTable)
	{
		rc = get_smbios_table_alloc(&gSmbiosTable, &gSmbiosTableSize, &gSmbiosMajorVersion, &gSmbiosMinorVersion);
	}

	if (NVM_SUCCESS == rc)
	{
		pSmBiosStruct->Raw = (UINT8 *)gSmbiosTable;
		pLastSmBiosStruct->Raw = pSmBiosStruct->Raw + gSmbiosTableSize;
		pSmbiosVersion->Major = gSmbiosMajorVersion;
		pSmbiosVersion->Minor = gSmbiosMinorVersion;
	}
}
