/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the Linux implementation of the os_adapter.h
 * system call wrappers.
 */


#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdio.h>
#include <libgen.h>
#include <cpuid.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <Base.h>
#include <lnx_adapter.h>
#include <string.h>
#include <os.h>
#include <nvm_management.h>

#define	LOCALE_DIR	"/usr/share/locale"
#define FTOK_PROJ_ID 'R' // Today proj_id is an int, but still only 8 bits are used.
                        // Typical usage has an ASCII character proj_id,
                        // that is why the behavior is said to be undefined when proj_id is zero.
#define SHM_PERM_FLG 0666 // permissions granted to the owner, group, and others.

#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO)
#endif

/*
 * Return the base path for the language catalog
 */
void os_get_locale_dir(OS_PATH locale_dir)
{
	snprintf(locale_dir, OS_PATH_LEN, "%s", LOCALE_DIR);
}

char *os_get_cwd(OS_PATH buffer, size_t size)
{
	return getcwd(buffer, size);
}

int os_get_filesize(const char *filename, size_t *filesize)
{
    struct stat file_stat;

    if (NULL == filesize)
    {
        return -1;
    }

    if (0 != stat(filename, &file_stat))
    {
        return -1;
    }

    *filesize = file_stat.st_size;
    return 0;
}

/*
 * Initializes a mutex.
 */
OS_MUTEX * os_mutex_init(const char *name)
{
	int rc = 0;
	pthread_mutex_t *mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	if (mutex)
	{
		// set attributes to make mutex reentrant (like windows implementation)
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

		// if named make it cross-process safe
		if (name)
		{
			pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
			// create a shared memory id
			int shmid = shmget(ftok(name, FTOK_PROJ_ID), sizeof (pthread_mutex_t), IPC_CREAT | SHM_PERM_FLG);
			if (shmid != -1)
			{
				// attach to the shared memory
				pthread_mutex_t *p_tmp = (pthread_mutex_t *)shmat(shmid, NULL, 0);
				if (p_tmp)
				{
					memmove(mutex, p_tmp, sizeof (pthread_mutex_t));
				}
			}
		}

		// failure when pthread_mutex_init(..) != 0
		rc = pthread_mutex_init((pthread_mutex_t *)mutex, &attr);
		if (rc != 0) {
			free(mutex);
			mutex = NULL;
		}
		// No longer need attr in success and failure case
		pthread_mutexattr_destroy(&attr);
	}
	return mutex;
}

/*
 * Locks the given mutex.
 */
int os_mutex_lock(OS_MUTEX *p_mutex)
{
	int rc = 0;
	if (p_mutex)
	{
		// failure when pthread_mutex_lock(..) != 0
		rc = (pthread_mutex_lock((pthread_mutex_t *)p_mutex) == 0);
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
		// failure when pthread_mutex_unlock(..) != 0
		rc = (pthread_mutex_unlock((pthread_mutex_t *)p_mutex) == 0);
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
    // failure when pthread_mutex_destroy(..) != 0
    rc = (pthread_mutex_destroy((pthread_mutex_t *)p_mutex) == 0);

    // detach and remove the shared memory
    if (name)
    {
      shmdt(p_mutex);
      // remove the shared memory
      int shmid = shmget(ftok(name, FTOK_PROJ_ID), sizeof(pthread_mutex_t), SHM_PERM_FLG);
      if (shmid != -1)
      {
        shmctl(shmid, IPC_RMID, NULL);
      }
    }
    free(p_mutex);
  }
  return rc;
}

/*
 * Initializes a rwlock
 */
int os_rwlock_init(OS_RWLOCK *p_rwlock)
{
	pthread_rwlock_t *p_handle = (pthread_rwlock_t *)p_rwlock;

	// failure when pthread_rwlock_init(..) != 0
	return ((pthread_rwlock_init(p_handle, NULL)) == 0);
}

/*
 * Applies a shared read-lock to the rwlock
 */
int os_rwlock_r_lock(OS_RWLOCK *p_rwlock)
{
	pthread_rwlock_t *p_handle = (pthread_rwlock_t *)p_rwlock;

	// failure when pthread_rwlock_rdlock(..) != 0
	return (pthread_rwlock_rdlock(p_handle) == 0);
}

/*
 * Unlocks an shared-read lock
 */
int os_rwlock_r_unlock(OS_RWLOCK *p_rwlock)
{
	// pthreads implements a single unlock function for
	// shared-read and exclusive-write locks
	return os_rwlock_w_unlock(p_rwlock);
}

/*
 * Applies an exclusive write-lock to the rwlock
 */
int os_rwlock_w_lock(OS_RWLOCK *p_rwlock)
{
	pthread_rwlock_t *p_handle = (pthread_rwlock_t *)p_rwlock;

	// failure when pthread_rwlock_wrlock(..) != 0
	return (pthread_rwlock_wrlock(p_handle) == 0);
}

/*
 * Unlocks an exclusive-write lock
 */
int os_rwlock_w_unlock(OS_RWLOCK *p_rwlock)
{
	pthread_rwlock_t *p_handle = (pthread_rwlock_t *)p_rwlock;

	// failure when pthread_rwlock_unlock(..) != 0
	return (pthread_rwlock_unlock(p_handle) == 0);
}

/*
 * Deletes the rwlock
 */
int os_rwlock_delete(OS_RWLOCK *p_rwlock)
{
	pthread_rwlock_t *p_handle = (pthread_rwlock_t *)p_rwlock;

	// failure when pthread_rwlock_destroy(..) != 0
	return (pthread_rwlock_destroy(p_handle) == 0);
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
		if (gethostname(name, name_len) != 0)
		{
			return -1;
		}
	}
	return rc;
}

/*
 * Retrieve the operating system name.
 */
int os_get_os_name(char *os_name, const unsigned int os_name_len)
{
	int rc = 0;
	if (os_name == NULL || os_name_len == 0)
	{
		rc = -1;
	}
	else
	{
		// get the OS info
		struct utsname name;
		if (uname(&name) == -1)
		{
			rc = -1;
		}
		else
		{
			snprintf(os_name, os_name_len, "%s", name.sysname);
		}
	}
	return rc;
}

/*
 * Retrieve the operating system version as a string.
 */
int os_get_os_version(char *os_version, const unsigned int os_version_len)
{
	int rc = 0;
	if (os_version == NULL || os_version_len == 0)
	{
		rc = -1;
	}
	else
	{
		// get the OS info
		struct utsname name;
		if (uname(&name) == -1)
		{
			rc = -1;
		}
		else
		{
			snprintf(os_version, os_version_len, "%s", name.release);
		}
	}
	return rc;
}

int os_get_os_type()
{
	return OS_TYPE_LINUX;
}

int get_supported_block_sizes(struct nvm_driver_capabilities *p_capabilities)
{
	int rc = NVM_SUCCESS;
	int found = 0;
	struct ndctl_ctx *ctx;

	p_capabilities->num_block_sizes = 0;

	if ((rc = ndctl_new(&ctx)) >= 0)
	{
		struct ndctl_bus *bus;
		ndctl_bus_foreach(ctx, bus)
		{
			struct ndctl_region *region;
			ndctl_region_foreach(bus, region)
			{
				int nstype = ndctl_region_get_nstype(region);
				if (ndctl_region_is_enabled(region) &&
					(nstype == ND_DEVICE_NAMESPACE_BLK))
				{
					struct ndctl_namespace *namespace;
					ndctl_namespace_foreach(region, namespace)
					{
						p_capabilities->num_block_sizes =
							ndctl_namespace_get_num_sector_sizes(namespace);

						for (int i = 0; i < p_capabilities->num_block_sizes; i++)
						{
							p_capabilities->block_sizes[i] =
								ndctl_namespace_get_supported_sector_size(namespace, i);
						}
						found = 1;
						break;
					}
				}
				if (found)
				{
					break;
				}
			}
			if (found)
			{
				break;
			}
		}
		ndctl_unref(ctx);
	}
	else
	{
		rc = linux_err_to_nvm_lib_err(rc);
	}

	return rc;
}

int os_get_driver_capabilities(struct nvm_driver_capabilities *p_capabilities)
{
	p_capabilities->features.get_platform_capabilities = 1;
	p_capabilities->features.get_topology = 1;
	p_capabilities->features.get_interleave = 1;
	p_capabilities->features.get_dimm_detail = 1;
	p_capabilities->features.get_namespaces = 1;
	p_capabilities->features.get_namespace_detail = 1;
	p_capabilities->features.get_address_scrub_data = 1;
	p_capabilities->features.get_platform_config_data = 1;
	p_capabilities->features.get_boot_status = 1;
	p_capabilities->features.get_power_data = 1;
	p_capabilities->features.get_security_state = 0;
	p_capabilities->features.get_log_page = 1;
	p_capabilities->features.get_features = 1;
	p_capabilities->features.set_features = 1;
	p_capabilities->features.create_namespace = 0;
	p_capabilities->features.delete_namespace = 0;
	p_capabilities->features.enable_namespace = 0;
	p_capabilities->features.disable_namespace = 0;
	p_capabilities->features.set_security_state = 0;
	p_capabilities->features.enable_logging = 0;
	p_capabilities->features.run_diagnostic = 0;
	p_capabilities->features.set_platform_config = 1;
	p_capabilities->features.passthrough = 1;
	p_capabilities->features.start_address_scrub = 1;
	p_capabilities->features.app_direct_mode = 1;

	p_capabilities->min_namespace_size = ndctl_min_namespace_size();
	get_supported_block_sizes(p_capabilities);
	p_capabilities->namespace_memory_page_allocation_capable = 1;
	return 0;
}

/*
* Determine if the caller has permission to make changes to the system
*/
int os_check_admin_permissions()
{
  int rc = NVM_ERR_INVALID_PERMISSIONS;
  // root user id will always be 0
  if (ROOT_USER_ID == getuid())
  {
    rc = NVM_SUCCESS;
  }
  return rc;
}

/*
* Recursive mkdir, return 0 on success, -1 on error
*/
int os_mkdir(char *path)
{
  char* p;
  for (p = strchr(path + 1, '/'); p; p = strchr(p + 1, '/'))
  {
    *p = '\0';
    if (mkdir(path, ACCESSPERMS) == -1) {
      if (errno != EEXIST) { *p = '/'; return -1; }
    }
    *p = '/';
  }
  return 0;
}

/*
 Get CPUID info for Linux. Depending on the inputRequestType,
  regs[0...3] will be populated with register values eax....edx
*/
int getCPUID(unsigned int *regs, int registerCount, int inputRequestType) {
  int rc = NVM_ERR_INVALIDPARAMETER;
  if (registerCount < 4) {
    return rc;
  }
  __get_cpuid(inputRequestType, &(regs[0]), &(regs[1]), &(regs[2]), &(regs[3]));
  return NVM_SUCCESS;
}