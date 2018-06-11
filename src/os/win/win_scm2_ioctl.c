/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <windows.h>
#include <WinBase.h>
#include "win_scm2_ioctl.h"

#ifdef _WIN32
#ifndef WINBOOL
#define WINBOOL BOOL
#endif
#endif

static enum WIN_SCM2_IOCTL_RETURN_CODES scm2_open_ioctl_target(PHANDLE p_handle,
		unsigned short dimm_handle)
{
	enum WIN_SCM2_IOCTL_RETURN_CODES rc = WIN_SCM2_IOCTL_RC_SUCCESS;

	char ioctl_target[256];
	//	\\.\PhysicalNvdimm{NFIT handle in hex}
	sprintf_s(ioctl_target, 256, "\\\\.\\PhysicalNvdimm%x", dimm_handle);

	*p_handle = CreateFile(ioctl_target,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, 0, NULL);

	if (*p_handle == INVALID_HANDLE_VALUE)
	{
		SCM_LOG_ERROR_F("CreateFile Failed for target: %s, Verify if driver installed. Error: %d",
				ioctl_target,
				(int)GetLastError());
		rc = WIN_SCM2_IOCTL_RC_ERR;
	}
	return rc;
}

static enum WIN_SCM2_IOCTL_RETURN_CODES scm2_send_ioctl_command(HANDLE handle,
		unsigned long io_controlcode, WIN_SCM2_IOCTL_REQUEST *p_ioctl_data)
{
	SCM_LOG_ENTRY();

	enum WIN_SCM2_IOCTL_RETURN_CODES rc = WIN_SCM2_IOCTL_RC_SUCCESS;
	WINBOOL io_result;
	DWORD bytes_returned = 0;
	p_ioctl_data->ReturnCode = 0;

	SCM_LOG_ERROR_F("IOCTL send request: io_ctl %lu, in_size %lu, out_size %lu", 
						(unsigned long)io_controlcode, (unsigned long)p_ioctl_data->InputDataSize, (unsigned long)p_ioctl_data->OutputDataSize);
	// DeviceIoControl documentation:
	// (msdn.microsoft.com/en-us/library/windows/desktop/aa363216(v=vs.85).aspx)
	// If the operation completes successfully, the return value is nonzero.
	// If the operation fails or is pending, the return value is zero.
	// To get extended error information, call GetLastError.
	io_result = (unsigned char)DeviceIoControl(handle, io_controlcode, p_ioctl_data->pInputData, (DWORD)p_ioctl_data->InputDataSize,
			p_ioctl_data->pOutputData, (DWORD)p_ioctl_data->OutputDataSize, &bytes_returned, NULL);

	if (!io_result)
	{
        p_ioctl_data->ReturnCode = GetLastError ();
		SCM_LOG_ERROR_F("IOCTL send failed with Windows Error: %d", (int)GetLastError());
		rc = WIN_SCM2_IOCTL_RC_ERR;
	}
	else if (p_ioctl_data->OutputDataSize < bytes_returned)
	{
		SCM_LOG_ERROR_F("IOCTL succeeded, but the OutputData buffer is too small, bytes_returned: %lu.", bytes_returned);
		rc = WIN_SCM2_IOCTL_RC_ERR;
	}

	SCM_LOG_EXIT_RETURN_I(rc);
	return rc;
}

enum WIN_SCM2_IOCTL_RETURN_CODES win_scm2_ioctl_execute(unsigned short nfit_handle,
        WIN_SCM2_IOCTL_REQUEST *p_ioctl_data, int io_controlcode)
{
	SCM_LOG_ENTRY();
	HANDLE handle;

	SCM_LOG_ERROR_F("IOCTL execute request: nfit_hdl %lu",
						(unsigned long)nfit_handle);

	enum WIN_SCM2_IOCTL_RETURN_CODES rc = scm2_open_ioctl_target(&handle, nfit_handle);
	if (WIN_SCM2_IOCTL_SUCCESS(rc))
	{
		rc = scm2_send_ioctl_command(handle, (unsigned long)io_controlcode,
				p_ioctl_data);
		CloseHandle(handle);
	}

	SCM_LOG_EXIT_RETURN_I(rc);

	return rc;
}

UINT32 win_scm2_ioctl_get_system_table(
    _In_  DWORD FirmwareTableProviderSignature,
    _In_  DWORD FirmwareTableID,
    _Out_ PVOID pFirmwareTableBuffer,
    _In_  DWORD BufferSize
)
{
    return GetSystemFirmwareTable(FirmwareTableProviderSignature, FirmwareTableID, pFirmwareTableBuffer, BufferSize);
}

UINT32 win_scm2_ioctl_get_last_error()
{
	return GetLastError();
}
