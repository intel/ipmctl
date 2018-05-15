/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CR_MGMT_SCM2_IOCTL_C_H
#define	CR_MGMT_SCM2_IOCTL_C_H

#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>

#define FILE_DEVICE_PERSISTENT_MEMORY   0x00000059 // defined in devioctl.h

#define	NVDIMM_IOCTL FILE_DEVICE_PERSISTENT_MEMORY //!< NVDIMM IOCTLs codes base value

//
// Functions 0 to 0x2FF are reserved for the bus device.
// Functions 0x300 to 0x5FF are reserved for the logical disk device.
// Functions from 0x600 are reserved for the physical NVDIMM device.
//
#define IOCTL_SCM_PHYSICAL_DEVICE_FUNCTION_BASE     0x600

#define SCM_PHYSICAL_DEVICE_FUNCTION(x) (IOCTL_SCM_PHYSICAL_DEVICE_FUNCTION_BASE + x)

typedef struct _WIN_SCM2_IOCTL_REQUEST {
    ULONG ReturnCode;
    size_t InputDataSize;
    size_t OutputDataSize;
    void * pInputData;
    void * pOutputData;
} WIN_SCM2_IOCTL_REQUEST;

// defined in Crystal Ridge RS2+ SCM Based Windows Driver SAS
typedef enum _CR_RETURN_CODES
{
	CR_RETURN_CODE_SUCCESS = 0,
	CR_RETURN_CODE_NOTSUPPORTED = 1,
	CR_RETURN_CODE_NOTALLOWED = 2,
	CR_RETURN_CODE_INVALIDPARAMETER = 3,
	CR_RETURN_CODE_BUFFER_OVERRUN = 4,
	CR_RETURN_CODE_BUFFER_UNDERRUN = 5,
	CR_RETURN_CODE_NOMEMORY = 6,
	CR_RETURN_CODE_NAMESPACE_CANT_BE_MODIFIED = 7,
	CR_RETURN_CODE_NAMESPACE_CANT_BE_REMOVED = 8,
	CR_RETURN_CODE_LOCKED_DIMM = 9,
	CR_RETURN_CODE_MAXIMUM_NAMESPACES_REACHED = 10,
	CR_RETURN_CODE_UNKNOWN = 11
} CR_RETURN_CODES;

#define	WIN_SCM2_IOCTL_SUCCESS(rc) (rc) == WIN_SCM2_IOCTL_RC_SUCCESS
enum WIN_SCM2_IOCTL_RETURN_CODES
{
	WIN_SCM2_IOCTL_RC_SUCCESS = 0,
	WIN_SCM2_IOCTL_RC_ERR = -1
};

enum WIN_SCM2_IOCTL_RETURN_CODES win_scm2_ioctl_execute(unsigned short nfit_handle,
		WIN_SCM2_IOCTL_REQUEST *p_ioctl_data,
		int io_controlcode);

#define	SCM_LOG_ENTRY()
#define	SCM_LOG_EXIT_RETURN_I(i)

#ifdef WIN_SCM2_DEBUG
#define	SCM_LOG_INFO(str) \
	printf("%s:%d(%s)> "str"\n", __FILE__, __LINE__, __FUNCTION__)
#define	SCM_LOG_INFO_F(fmt, ...) \
	printf("%s:%d(%s)> ", __FILE__, __LINE__, __FUNCTION__);  \
	printf(fmt"\n", __VA_ARGS__)
#define	SCM_LOG_ERROR(str) \
	printf("%s:%d(%s)> "str"\n", __FILE__, __LINE__, __FUNCTION__)
#define	SCM_LOG_ERROR_F(fmt, ...) \
	printf("%s:%d(%s)> ", __FILE__, __LINE__, __FUNCTION__);  \
	printf(fmt"\n", __VA_ARGS__)
#else
#define	SCM_LOG_INFO(str)
#define	SCM_LOG_INFO_F(fmt, ...)
#define	SCM_LOG_ERROR(str)
#define	SCM_LOG_ERROR_F(fmt, ...)
#endif

#define	WIN_SCM2_IS_SUCCESS(rc) (rc) == WIN_SCM2_SUCCESS
enum WIN_SCM2_RETURN_CODES
{
	WIN_SCM2_SUCCESS = 0,
	WIN_SCM2_ERR_UNKNOWN = -1,
	WIN_SCM2_ERR_DRIVERFAILED = -2,
	WIN_SCM2_ERR_NOMEMORY = -3,
};
#endif // CR_MGMT_SCM2_IOCTL_C_H
