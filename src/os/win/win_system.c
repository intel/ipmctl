/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the Windows implementation of the os_adapter.h
 * system call wrappers.
 */

#include <os.h>
#include <sys/stat.h>
#include <windows.h>
#include <winnt.h>
#include <stdio.h>
#include <nvm_management.h>
#include <tchar.h> // todo: remove this header and replace associated functions
#include <direct.h> // for _getcwd
#include <s_str.h>

#pragma comment(lib,"Version.lib")
#pragma comment(lib, "Ws2_32.lib")

#define	MGMTSW_REG_KEY "SOFTWARE\\INTEL\\INTEL DC PMM"
#define	INSTALLDIR_REG_SUBKEY "InstallDir"
#define	EVENT_SOURCE	"IntelASM"

// used by get_os_name
typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
typedef int (WINAPI *PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

EXTERN_C IMAGE_DOS_HEADER __ImageBase;


const char *WIN_VISTA = TEXT("Microsoft Windows Vista");
const char *WIN_SERVER_2008 = TEXT("Microsoft Windows Server 2008");
const char *WIN_7 = TEXT("Microsoft Windows 7");
const char *WIN_SERVER_2008_R2 = TEXT("Microsoft Windows Server 2008 R2");
const char *WIN_8 = TEXT("Microsoft Windows 8");
const char *WIN_SERVER_2012 = TEXT("Microsoft Windows Server 2012");
const char *WIN_8_1 = TEXT("Microsoft Windows 8.1");
const char *WIN_SERVER_2012_R2 = TEXT("Microsoft Windows Server 2012 R2");
const char *WIN_10 = TEXT("Microsoft Windows 10");
const char *WIN_SERVER_2016 = TEXT("Microsoft Windows Server 2016");
const char *WIN_UNKNOWN = TEXT("Unrecognized Microsoft Windows operating system");

void get_install_dir(OS_PATH install_dir);

/*
 * Get current working directory
 */
char *os_get_cwd(OS_PATH buffer, size_t size)
{
	return _getcwd(buffer, (int)size);
}

int utf8_to_wchar(wchar_t *dst, size_t dst_wchars, const char *src, int src_bytes)
{
	int ret = -1;

	// src must exist, as well as a valid value indicating the number of bytes to process
	// dst must exist if we wish to write data to it
	if ((src != NULL) && ((src_bytes == -1) || (src_bytes > 0)) &&
		((dst_wchars == 0) || (dst != NULL)))
	{
		ret = MultiByteToWideChar(
			(UINT)CP_UTF8,		// convert from UTF-8
			0,					// use default conversion type (fastest)
			(LPCSTR)src,		// source UTF-8 encoded char array
			src_bytes,			// number of src bytes to convert
			(LPWSTR)dst,		// destination UTF-16 encoded wchar_t array
			(int)dst_wchars);	// size (in wchar_t characters) of the destination array
	}

	return ret;
}

/*
 * Start a process and get its PID
 */
int os_start_process(const char *process_name, unsigned int *p_process_id)
{
	int rc = -1;

	OS_WPATH w_process_name;
	utf8_to_wchar(w_process_name, (size_t)OS_PATH_LEN, process_name, (int)OS_PATH_LEN);

	STARTUPINFOW start_up_info;
	memset(&start_up_info, 0, sizeof (STARTUPINFOW));
	start_up_info.cb = sizeof (STARTUPINFOW);

	PROCESS_INFORMATION process_info;
	memset(&process_info, 0, sizeof (PROCESS_INFORMATION));

	int success = CreateProcessW(w_process_name, NULL, NULL, NULL, FALSE, 0, NULL, NULL,
			&start_up_info, &process_info);
	if (success)
	{
		*p_process_id = process_info.dwProcessId;
		rc = 0;
		// close handles
		CloseHandle(process_info.hProcess);
		CloseHandle(process_info.hThread);
	}

	return rc;
}

/*
 * Stop a process given the process handle
 */
int os_stop_process(unsigned int process_id)
{
	int rc = -1;

	// get the process handle from the process id
	unsigned long access = PROCESS_TERMINATE;
	HANDLE process_handle = OpenProcess(access, 0, process_id);
	if (process_handle != NULL)
	{
		unsigned int exitCode = 0;
		int success = TerminateProcess(process_handle, exitCode);
		if (success)
		{
			rc = 0;
		}
		CloseHandle(process_handle);
	}

	return rc;
}

/*
 * Blocks for the specified number of msecs.
 */
void os_sleep(unsigned long time)
{
	Sleep(time);
}

/*
 * Create a thread on the current process
 */
void os_create_thread(unsigned long long *p_thread_id, void *(*callback)(void *), void * callback_arg)
{
	CreateThread(
			NULL, // default security
			0,  // default stack size
			(LPTHREAD_START_ROUTINE)callback,
			(LPVOID)callback_arg,
			0, // Immediately run thread
			(LPDWORD)p_thread_id);
}

/*
 * Retrieve the id of the current thread
 */
unsigned long long os_get_thread_id()
{
	return GetCurrentThreadId();
}

/*
 * Creates & Initializes a mutex.
 */
OS_MUTEX * os_mutex_init(const char *name)
{
	return (OS_MUTEX*)CreateMutex(NULL, FALSE, name);
}

/*
 * Locks the given mutex.
 */
int os_mutex_lock(OS_MUTEX *p_mutex)
{
	// default return to failure state
	int rc = 0;
	if (p_mutex)
	{
		HANDLE handle = (HANDLE)p_mutex;
		DWORD f_rc = WaitForSingleObject(handle, INFINITE);
		switch (f_rc)
		{
			// this is the only case of success
			case WAIT_OBJECT_0:
				rc = 1;
				break;

			// all others are failure
			case WAIT_ABANDONED:
			case WAIT_TIMEOUT:
			case WAIT_FAILED:
			default:
				rc = 0;
				break;
		}
	}
	return rc;
}

/*
 * Unlocks a locked mutex
 */
int os_mutex_unlock(OS_MUTEX *p_mutex)
{
	int rc = 0;
	if (p_mutex)
	{
		HANDLE handle = (HANDLE)p_mutex;

		// failure when ReleaseMutex(..) == 0
		rc = (ReleaseMutex(handle) != 0);
	}
	return rc;
}

/*
 * Deletes the mutex
 */
int os_mutex_delete(OS_MUTEX *p_mutex, const char *name)
{
	int rc = 1;
	if (p_mutex)
	{
		HANDLE p_handle = (HANDLE)p_mutex;

		// failure when CloseHandle(..) == 0
		rc = (CloseHandle(p_handle) != 0);

	}
	return rc;
}

/*
 * Initializes a rwlock
 */
int os_rwlock_init(OS_RWLOCK *p_rwlock)
{
	SRWLOCK *p_handle = (SRWLOCK *)p_rwlock;

	// Win32 API provides no indication of success for this function
	InitializeSRWLock(p_handle);
	return 1;
}

/*
 * Applies a shared read-lock to the rwlock
 */
int os_rwlock_r_lock(OS_RWLOCK *p_rwlock)
{
	SRWLOCK *p_handle = (SRWLOCK *)p_rwlock;

	// Win32 API provides no indication of success for this function
	AcquireSRWLockShared(p_handle);
	return 1;
}

/*
 * Unlocks an shared-read lock
 */
int os_rwlock_r_unlock(OS_RWLOCK *p_rwlock)
{
	SRWLOCK *p_handle = (SRWLOCK *)p_rwlock;

	// Win32 API provides no indication of success for this function
	ReleaseSRWLockShared(p_handle);
	return 1;
}

/*
 * Applies an exclusive write-lock to the rwlock
 */
int os_rwlock_w_lock(OS_RWLOCK *p_rwlock)
{
	SRWLOCK *p_handle = (SRWLOCK *)p_rwlock;

	// Win32 API provides no indication of success for this function
	AcquireSRWLockExclusive(p_handle);
	return 1;
}

/*
 * Unlocks an exclusive-write lock
 */
int os_rwlock_w_unlock(OS_RWLOCK *p_rwlock)
{
	SRWLOCK *p_handle = (SRWLOCK *)p_rwlock;

	// Win32 API provides no indication of success for this function
	ReleaseSRWLockExclusive(p_handle);
	return 1;
}

/*
 * Deletes the rwlock
 */
int os_rwlock_delete(OS_RWLOCK *p_rwlock)
{
	// SRW Locks do not need to be explicitly destroyed
	// see: http:// msdn.microsoft.com/en-us/library/windows/desktop/ms683483%28v=vs.85%29.aspx
	return 1;
}

/*
 * Retrieve the name of the host server.
 */
int os_get_host_name(char *name, const unsigned int name_len)
{
	int rc = 0;

	// check input parameters
	if (name == NULL || name_len == 0)
	{
		rc = -1;
	}
	else
	{
		// have to initialize the networking stuff to retrieve host name
		WSADATA wsaData;
		WORD wVersionRequested;
		wVersionRequested = MAKEWORD(2, 2);
		int err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
		{
			rc = -1;
		}
		else
		{
			err = gethostname(name, name_len);
			if (err != 0)
			{
				return -1;
			}
			WSACleanup();
		}
	}
	return rc;
}

/*
int todigit(char c)
{
	return c - '0';
}
*/



int parse_revision(unsigned short int **pp_parts, int parts_count,
	const char * const revision, size_t revision_len)
{
	// no checking of input parameters for this helper function

	// initialize end pointer to the first char in revision
	const char *p_end = &(revision[0]);

	// iterate through the revision string, grabbing the number of parts specified,
	// or until the defined end of the revision string is reached
	int i = 0;
	size_t revision_len_left = revision_len;
	for (; (i < parts_count) && (revision_len_left > 0) && (p_end != NULL) && (*p_end != '\0'); i++)
	{
		revision_len_left -= s_strtous(p_end, revision_len_left, &p_end, pp_parts[i]);
	}

	// returns true only if we grabbed the expected number of parts
	return (i == parts_count);
}

int get_os_version_parts(unsigned short *p_major, unsigned short *p_minor,
		unsigned short *p_build)
{
	char version_str[128];
	int rc = os_get_os_version(version_str, sizeof (version_str));
	if (rc == 0)
	{
		unsigned short *pp_parts[] = { p_major, p_minor, p_build };
		parse_revision(pp_parts, 3, version_str, sizeof (version_str));
	}

	return rc;
}

const char *get_server_release_for_version(const unsigned short major_version,
		const unsigned short minor_version)
{
	const char *release_name = WIN_UNKNOWN;

	// Modern versions of Windows Server
	// Older versions unsupported
	if (major_version == 6)
	{
		switch (minor_version)
		{
		case 0:
			release_name = WIN_SERVER_2008;
			break;
		case 1:
			release_name = WIN_SERVER_2008_R2;
			break;
		case 2:
			release_name = WIN_SERVER_2012;
			break;
		case 3:
			release_name = WIN_SERVER_2012_R2;
			break;
		default: // Unrecognized
			break;
		}
	}
	else if (major_version == 10)
	{
		switch (minor_version)
		{
		case 0:
			release_name = WIN_SERVER_2016;
			break;
		default:
			break;
		}
	}

	return release_name;
}

const char *get_workstation_release_for_version(const unsigned short major_version,
		const unsigned short minor_version)
{
	const char *release_name = WIN_UNKNOWN;

	// Modern versions of Windows
	// Older versions unsupported
	if (major_version == 6)
	{
		switch (minor_version)
		{
		case 0:
			release_name = WIN_VISTA;
			break;
		case 1:
			release_name = WIN_7;
			break;
		case 2:
			release_name = WIN_8;
			break;
		case 3:
			release_name = WIN_8_1;
			break;
		default: // Unrecognized
			break;
		}
	}
	else if (major_version == 10)
	{
		switch (minor_version)
		{
		case 0:
			release_name = WIN_10;
			break;
		default:
			break;
		}
	}

	return release_name;
}

BOOL is_windows_server()
{
	OSVERSIONINFOEX version_info;
	ZeroMemory(&version_info, sizeof (version_info));
	version_info.dwOSVersionInfoSize = sizeof (version_info);

	// Anything that's not a workstation counts as a server
	version_info.wProductType = VER_NT_WORKSTATION;

	DWORDLONG conditionMask = 0;
	VER_SET_CONDITION(conditionMask, VER_PRODUCT_TYPE, VER_EQUAL);

	return !VerifyVersionInfo(&version_info, VER_PRODUCT_TYPE, conditionMask);
}

const char *get_windows_release_name_for_version(const unsigned short major_version,
		const unsigned short minor_version)
{
	const char *win_release;
	if (is_windows_server())
	{
		win_release = get_server_release_for_version(major_version, minor_version);
	}
	else
	{
		win_release = get_workstation_release_for_version(major_version, minor_version);
	}

	return win_release;
}

void get_system_info(SYSTEM_INFO *p_system_info)
{
	ZeroMemory(p_system_info, sizeof (SYSTEM_INFO));

	// Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.
	PGNSI p_gnsi = (PGNSI) GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
		"GetNativeSystemInfo");
	if (NULL != p_gnsi)
	{
		p_gnsi(p_system_info);
	}
	else
	{
		GetSystemInfo(p_system_info);
	}
}

DWORD get_current_os_edition(const unsigned short major, const unsigned short minor)
{
	PGPI p_gpi;
	DWORD edition = 0;
	p_gpi = (PGPI) GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),
		"GetProductInfo");
	p_gpi(major, minor, 0, 0, &edition);

	return edition;
}

const char *get_current_os_edition_name(const unsigned short major_version,
		const unsigned short minor_version)
{
	const char *edition_name = "";

	DWORD edition_type = get_current_os_edition(major_version, minor_version);
	switch (edition_type)
	{
		case PRODUCT_ULTIMATE:
			edition_name = TEXT("Ultimate Edition");
			break;
		case PRODUCT_PROFESSIONAL:
			edition_name = TEXT("Professional");
			break;
		case PRODUCT_HOME_PREMIUM:
			edition_name = TEXT("Home Premium Edition");
			break;
		case PRODUCT_HOME_BASIC:
			edition_name = TEXT("Home Basic Edition");
			break;
		case PRODUCT_ENTERPRISE:
			edition_name = TEXT("Enterprise Edition");
			break;
		case PRODUCT_BUSINESS:
			edition_name = TEXT("Business Edition");
			break;
		case PRODUCT_STARTER:
			edition_name = TEXT("Starter Edition");
			break;
		case PRODUCT_CLUSTER_SERVER:
			edition_name = TEXT("Cluster Server Edition");
			break;
		case PRODUCT_DATACENTER_SERVER:
			edition_name = TEXT("Datacenter Edition");
			break;
		case PRODUCT_DATACENTER_SERVER_CORE:
			edition_name = TEXT("Datacenter Edition (core installation)");
			break;
		case PRODUCT_ENTERPRISE_SERVER:
			edition_name = TEXT("Enterprise Edition");
			break;
		case PRODUCT_ENTERPRISE_SERVER_CORE:
			edition_name = TEXT("Enterprise Edition (core installation)");
			break;
		case PRODUCT_ENTERPRISE_SERVER_IA64:
			edition_name = TEXT("Enterprise Edition for Itanium-based Systems");
			break;
		case PRODUCT_SMALLBUSINESS_SERVER:
			edition_name = TEXT("Small Business Server");
			break;
		case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
			edition_name = TEXT("Small Business Server Premium Edition");
			break;
		case PRODUCT_STANDARD_SERVER:
			edition_name = TEXT("Standard Edition");
			break;
		case PRODUCT_STANDARD_SERVER_CORE:
			edition_name = TEXT("Standard Edition (core installation)");
			break;
		case PRODUCT_WEB_SERVER:
			edition_name = TEXT("Web Server Edition");
			break;
	}

	return edition_name;
}


void append_edition_name(const unsigned short major_version,
	const unsigned short minor_version,
	char *os_name, const unsigned int os_name_len)
{
	s_strcat(os_name, os_name_len, " ");
	s_strcat(os_name, os_name_len,
			get_current_os_edition_name(major_version, minor_version));
}

void append_build_number(const unsigned short build_version,
		char *os_name, const unsigned int os_name_len)
{
	TCHAR buf[80];
	snprintf(buf, 80, " (build %u)", build_version);
	s_strcat(os_name, os_name_len, buf);
}

void append_processor_architecture(char *os_name, const unsigned int os_name_len)
{
	SYSTEM_INFO si;
	get_system_info(&si);

	switch (si.wProcessorArchitecture)
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		s_strcat(os_name, os_name_len, TEXT(", 64-bit"));
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		s_strcat(os_name, os_name_len, TEXT(", 32-bit"));
		break;
	default:
		break;
	}
}


int get_os_name_from_version(const unsigned short major_version,
	const unsigned short minor_version, const unsigned short build_version,
	char *os_name, const unsigned int os_name_len)
{
	int rc = 0;

	s_strcpy(os_name, get_windows_release_name_for_version(major_version,
			minor_version), os_name_len);
	if (major_version >= 6) // Vista or later
	{
		append_edition_name(major_version, minor_version, os_name, os_name_len);
		append_build_number(build_version, os_name, os_name_len);
		append_processor_architecture(os_name, os_name_len);
	}

	return rc;
}

/*
 * Retrieve the operating system name.
 */
int os_get_os_name(char *os_name, const unsigned int os_name_len)
{
	int rc = 0;

	// check input parameters
	if (os_name == NULL || os_name_len == 0)
	{
		rc = -1;
	}
	else
	{
		unsigned short major = 0;
		unsigned short minor = 0;
		unsigned short build = 0;
		get_os_version_parts(&major, &minor, &build);

		rc = get_os_name_from_version(major, minor, build,
				os_name, os_name_len);
	}

	return rc;
}

int get_file_version_info_for_system(LPVOID *pp_version_info)
{
	int rc = 0;

	LPCSTR system_filename = TEXT("kernel32.dll");
	DWORD version_data_size = GetFileVersionInfoSize(system_filename, 0);
	if (version_data_size > 0)
	{
		*pp_version_info = calloc(1, version_data_size);
		if (*pp_version_info == NULL)
		{
			rc = -1;
		}
		else
		{
			if (!GetFileVersionInfo(system_filename, 0, version_data_size, *pp_version_info))
			{
				free(*pp_version_info);
				*pp_version_info = NULL;

				rc = -1;
			}
		}
	}
	else
	{
		rc = -1;
	}

	return rc;
}

// The language and code page numbers for Windows FileVersionInfo.
// Used to come up with paths for version strings in FileVersionInfo.
struct LANGANDCODEPAGE
{
	WORD wLanguage;
	WORD wCodePage;
};

// Returns a pointer into existing memory p_version_info
struct LANGANDCODEPAGE *get_first_lang_and_code_page(LPVOID p_version_info)
{
	struct LANGANDCODEPAGE *p_first = NULL;
	UINT bytes = 0;
	if (VerQueryValue(p_version_info, TEXT("\\VarFileInfo\\Translation"),
			(LPVOID *)&p_first, &bytes))
	{
		UINT language_count = bytes / sizeof (struct LANGANDCODEPAGE);
		if (language_count == 0) // inconsistent data
		{
			p_first = NULL;
		}
	}

	return p_first;
}

int get_sub_block_path_for_file_version_info(LPVOID p_file_version_info,
		const char *sub_block_name,
		char *p_sub_block_path, const unsigned int sub_block_path_len)
{
	int rc = 0;

	struct LANGANDCODEPAGE *p_sub_block_lang =
			get_first_lang_and_code_page(p_file_version_info);
	if (p_sub_block_lang)
	{
		snprintf(p_sub_block_path, sub_block_path_len,
				"\\StringFileInfo\\%04x%04x\\%s",
				p_sub_block_lang->wLanguage, p_sub_block_lang->wCodePage,
				sub_block_name);
	}
	else
	{
		// Something is wrong with the version info
		rc = -1;
	}

	return rc;
}

int get_version_string_from_system(char *os_version, const unsigned int os_version_len)
{
	LPVOID p_version_info = NULL;
	int rc = get_file_version_info_for_system(&p_version_info);
	if (rc == 0)
	{
		char product_version_sub_block[256];
		rc = get_sub_block_path_for_file_version_info(p_version_info,
				"ProductVersion",
				product_version_sub_block, sizeof (product_version_sub_block));
		if (rc == 0)
		{
			LPVOID p_version = NULL;
			UINT version_size = 0;
			if (VerQueryValue(p_version_info, TEXT(product_version_sub_block),
					&p_version, &version_size))
			{
				// The sub-block is formatted as a string
				snprintf(os_version, os_version_len, "%s", (char*)p_version);
			}
		}
	}
	free(p_version_info);

	return rc;
}

/*
 * Retrieve the operating system version as a string.
 */
int os_get_os_version(char *os_version, const unsigned int os_version_len)
{
	int rc = 0;

	// check input parameters
	if (os_version == NULL || os_version_len == 0)
	{
		rc = -1;
	}
	else
	{
		rc = get_version_string_from_system(os_version, os_version_len);
	}
	return rc;
}

/*
 * Determine if the caller has permission to make changes to the system
  https://msdn.microsoft.com/en-us/library/windows/desktop/aa379649(v=vs.85).aspx
 */
int os_check_admin_permissions()
{
	int rc = NVM_SUCCESS;
 /*  The SECURITY_NT_AUTHORITY (S-1-5) predefined identifier authority produces SIDs that are not universal but are meaningful
   only on Windows installation*/
	SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
	PSID group;
	BOOL is_member = FALSE;
	DWORD user_type = DOMAIN_ALIAS_RID_ADMINS;

	if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
			user_type, 0, 0, 0, 0, 0, 0, &group))
	{
		if (!CheckTokenMembership(NULL, group, &is_member) || !is_member)
		{
			rc = NVM_ERR_INVALID_PERMISSIONS;
		}
		FreeSid(group);
	}

	return rc;
}

/*
 *  Check the given user is an administrator
 */
int is_admin(HANDLE hUserToken)
{
	int rc = 0;
	DWORD i = 0, dwSize = 0, dwResult = 0;
	SID_IDENTIFIER_AUTHORITY ntAuth = {SECURITY_NT_AUTHORITY};
	PSID pAdminSid;
	PTOKEN_GROUPS pGroupInfo;

	// calculate the token buffer for the given user token
	if (!GetTokenInformation(hUserToken, TokenGroups, NULL, dwSize, &dwSize))
	{
		dwResult = GetLastError();
		if (dwResult != ERROR_INSUFFICIENT_BUFFER)
		{
			return rc;
		}
	}

	// get token information for the given user token
	pGroupInfo = (PTOKEN_GROUPS) GlobalAlloc(GPTR, dwSize);
	if ((NULL != pGroupInfo) && (GetTokenInformation(hUserToken, TokenGroups, pGroupInfo, dwSize, &dwSize)))
	{
		// SID for the Administrators group
		if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
				DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminSid))
		{
			// loop and compare through groups in token
			for (i = 0; i < pGroupInfo->GroupCount; i++)
			{
				if (EqualSid(pAdminSid, pGroupInfo->Groups[i].Sid))
				{
					rc = 1;
					break;
				}
			}
			FreeSid(pAdminSid);
		}
	}

	// close handle
	CloseHandle(hUserToken);
	return rc;
}


/*
 * Return the base path for the language catalog
 */
void os_get_locale_dir(OS_PATH locale_dir)
{
	get_install_dir(locale_dir);
}

/*
* Return the path to the installation directory
*/
void get_install_dir(OS_PATH install_dir)
{
	// GetModuleFileName puts full path to DLL into install_dir
	// ex: C:\output\build\windows\debug\libixpdimm.dll
	if (GetModuleFileName((HINSTANCE)&__ImageBase, install_dir, OS_PATH_LEN) > 0)
	{
		// find last '/' or '\'
		int len = (int)strlen(install_dir) - 1;
		while (len >= 0)
		{
			if (install_dir[len] == '\\' ||
				install_dir[len] == '/')
			{
				install_dir[len + 1] = '\0'; // keep the last '\'
				len = 0; // all done
			}
			len--;
		}
	}
}



int os_get_driver_capabilities(struct nvm_driver_capabilities *p_capabilities)
{
	memset(p_capabilities, 0, sizeof(struct nvm_driver_capabilities));

	p_capabilities->min_namespace_size = BYTES_PER_GIB;
	p_capabilities->num_block_sizes = 1;
	p_capabilities->block_sizes[0] = 1;

	p_capabilities->namespace_memory_page_allocation_capable = 0;
	p_capabilities->features.get_platform_capabilities = 1;
	p_capabilities->features.get_topology = 0;
	p_capabilities->features.get_interleave = 1;
	p_capabilities->features.get_dimm_detail = 0;
	p_capabilities->features.get_namespaces = 1;
	p_capabilities->features.get_namespace_detail = 1;
	p_capabilities->features.get_boot_status = 1;
	p_capabilities->features.get_power_data = 0;
	p_capabilities->features.get_security_state = 0;
	p_capabilities->features.get_log_page = 1;
	p_capabilities->features.get_features = 1;
	p_capabilities->features.set_features = 1;
	p_capabilities->features.create_namespace = 0;
	p_capabilities->features.rename_namespace = 0;
	p_capabilities->features.delete_namespace = 0;
	p_capabilities->features.set_security_state = 0;
	p_capabilities->features.enable_logging = 1;
	p_capabilities->features.run_diagnostic = 0;
	p_capabilities->features.passthrough = 1;
	p_capabilities->features.app_direct_mode = 1;
	p_capabilities->features.storage_mode = 0;
	return 0;
}

int os_get_os_type()
{
	return 1;
}

/*
* Recursive mkdir, return 0 on success, -1 on error
*/
int os_mkdir(OS_PATH path)
{
  char* p;
  for (p = strchr(path + 1, '/'); p; p = strchr(p + 1, '/'))
  {
    *p = '\0';
    if (_mkdir(path) == -1) {
      if (errno != EEXIST) { *p = '/'; return -1; }
    }
    *p = '/';
  }
  return 0;
}
