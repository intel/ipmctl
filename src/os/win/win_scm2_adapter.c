/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "win_scm2_adapter.h"
#include <windows.h>
#include <stdio.h>
#include <Setupapi.h>
#include <GuidDef.h>
#include <Devpkey.h>
#include <devpropdef.h>
#include <objbase.h>


#pragma comment(lib, "Setupapi.lib")

#define INITGUID
#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#ifdef INITGUID
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
#else
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY name
#endif // INITGUID


DEFINE_DEVPROPKEY(DEVPKEY_Device_DriverVersion, 0xa8b865dd, 0x2e3d, 0x4094, 0xad, 0x97, 0xe5, 0x93, 0xa7, 0xc, 0x75, 0xd6, 3);
#define SCM_INBOX_CLASS_GUID	L"{5099944a-f6b9-4057-a056-8c550228544c}"
#define VERS_BUFFER_SZ	256

int get_vendor_driver_revision(char * version_str, const int str_len)
{
	GUID guid;
	HANDLE enum_handle = NULL;
	DWORD index = 0;
	DWORD required_sz = 0;
	WCHAR vers_buffer[VERS_BUFFER_SZ] = { 0 };
	DEVPROPTYPE prop_type = 0;
	SP_DEVINFO_DATA dev_info = { sizeof(SP_DEVINFO_DATA) };
	int status = -1;

	HRESULT hr = CLSIDFromString(SCM_INBOX_CLASS_GUID, (LPCLSID)&guid);

	if (INVALID_HANDLE_VALUE == (enum_handle = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES)))
	{
		printf("SetupDiGetClassDevs failed trying to get the driver version, GetLastError: %d ",
			GetLastError());
		return -1;
	}

	while (SetupDiEnumDeviceInfo(enum_handle, index++, &dev_info)) {

		if (IsEqualGUID(&guid, &dev_info.ClassGuid))
		{
			if (TRUE == SetupDiGetDevicePropertyW(enum_handle, &dev_info, &DEVPKEY_Device_DriverVersion,
							&prop_type, (PBYTE)vers_buffer, sizeof(vers_buffer), &required_sz, 0))
			{
				size_t chars_converted = 0;
				if (0 == wcstombs_s(&chars_converted, version_str, str_len, vers_buffer, required_sz))
				{
					status = 0;
					break;
				}
				else
				{
					printf("wcstombs_s failed converting the driver version string, GetLastError: %d ",
						GetLastError());
					status = -1;
					break;
				}
			}
			else
			{
				printf("SetupDiGetDevicePropertyW failed trying to get the driver version, GetLastError: %d ",
					GetLastError());
				status = -1;
				break;
			}
		}
	}

	SetupDiDestroyDeviceInfoList(enum_handle);

	return status;
}
