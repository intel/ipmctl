/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OS_H_
#define OS_H_

#include <stdbool.h>

#ifdef	_MSC_VER
#include <stdlib.h>
#include <limits.h>
#include <intrin.h>
#define PATH_MAX _MAX_PATH
#define	OS_PATH_SEP	"\\"
#else
#include <linux/limits.h>
#define	OS_PATH_SEP	"/"
#endif

#define ROOT_USER_ID 0
#define	BYTES_PER_KIB	1024
#define	BYTES_PER_MIB	(unsigned long long)(1 << 20) // 1024^2
#define	BYTES_PER_GIB	(BYTES_PER_MIB * BYTES_PER_KIB) // 1024^3
#define	OS_PATH_LEN	PATH_MAX
#define OS_NAME_MAX 100
#define OS_VERSION_MAX 100

typedef wchar_t OS_WPATH[OS_PATH_LEN];
typedef char OS_PATH[OS_PATH_LEN];
typedef void OS_MUTEX;
typedef void OS_RWLOCK;



#define	MAX_NUMBER_OF_BLOCK_SIZES 16
struct driver_feature_flags
{
	unsigned int get_platform_capabilities : 1;
	unsigned int get_topology : 1;
	unsigned int get_interleave : 1;
	unsigned int get_dimm_detail : 1;
	unsigned int get_namespaces : 1;
	unsigned int get_namespace_detail : 1;
	unsigned int get_address_scrub_data : 1;
	unsigned int get_platform_config_data : 1;
	unsigned int get_boot_status : 1;
	unsigned int get_power_data : 1;
	unsigned int get_security_state : 1;
	unsigned int get_log_page : 1;
	unsigned int get_features : 1;
	unsigned int set_features : 1;
	unsigned int create_namespace : 1;
	unsigned int delete_namespace : 1;
	unsigned int enable_namespace : 1;
	unsigned int disable_namespace : 1;
	unsigned int set_security_state : 1;
	unsigned int enable_logging : 1;
	unsigned int run_diagnostic : 1;
	unsigned int set_platform_config : 1;
	unsigned int passthrough : 1;
	unsigned int start_address_scrub : 1;
	unsigned int app_direct_mode : 1;
};

struct nvm_driver_capabilities
{
	unsigned long long min_namespace_size; // in bytes
	unsigned int namespace_memory_page_allocation_capable;
	struct driver_feature_flags features;
};
extern void os_get_locale_dir(OS_PATH locale_dir);
extern char * os_get_cwd(OS_PATH buffer, size_t size);
extern int os_mkdir(char *path);

extern OS_MUTEX *os_mutex_init(const char *name);
extern int os_mutex_lock(OS_MUTEX *p_mutex);
extern int os_mutex_unlock(OS_MUTEX *p_mutex);
extern int os_mutex_delete(OS_MUTEX *p_mutex, const char *name);

extern int os_rwlock_init(OS_RWLOCK *p_rwlock);
extern int os_rwlock_r_lock(OS_RWLOCK *p_rwlock);
extern int os_rwlock_r_unlock(OS_RWLOCK *p_rwlock);
extern int os_rwlock_w_lock(OS_RWLOCK *p_rwlock);
extern int os_rwlock_w_unlock(OS_RWLOCK *p_rwlock);
extern int os_rwlock_delete(OS_RWLOCK *p_rwlock);

extern int os_get_host_name(char *name, const unsigned int name_len);
extern int os_get_os_name(char *os_name, const unsigned int os_name_len);
extern int os_get_os_version(char *os_version, const unsigned int os_version_len);
extern int os_get_os_type();
extern int os_get_driver_capabilities(struct nvm_driver_capabilities *p_capabilities);
extern int os_check_admin_permissions();

/*
 Get CPUID info for different OSs. Depending on the inputRequestType,
  regs[0...3] will be populated with register values eax....edx
*/
extern int getCPUID(unsigned int *regs, int registerCount, int inputRequestType);

int wait_for_sec(unsigned int seconds);

bool is_shortcut(const char *path);

#endif
