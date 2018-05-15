/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the definition of Windows system service utility methods.
 */

#include <vector>
#include "NvmMonitorBase.h"
#include <nvm_types.h>

/*
 * Install the service into Windows SCM
 */
bool serviceInstall(std::string serviceName, std::string displayName);

/*
 * Uninstall the service from Windows SCM
 */
bool serviceUninstall(const char *service_name);

/*
 * Initialize the service when it is starting
 */
bool serviceInit(std::string serviceName);
