/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains the entry point for the NvmMonitor service on Windows.
 */


//#include <cr_i18n.h>
#include "win_service.h"

#define TR
#define SERVICE_NAME "ixpdimm-monitor"
#define SERVICE_DISPLAY_NAME "Intel ixpdimm-monitor"
#define SUCCESS TR("Success")
#define FAIL TR("Fail")

void usage(int argc, char **argv);

int main(int argc, char **argv)
{
	bool success = true;
	if (argc == 2)
	{
		std::string arg = argv[1];

		if (arg == "install")
		{
			success = serviceInstall(SERVICE_NAME, SERVICE_DISPLAY_NAME);
			printf(TR("Service Install: %s"), (success ? SUCCESS : FAIL));
		}
		else if (arg == "uninstall")
		{
			success = serviceUninstall(SERVICE_NAME) == 0;
			printf(TR("Service Uninstall: %s"), (success ? SUCCESS : FAIL));
		}
		else
		{
			usage(argc, argv);
		}
	}
	else if (argc == 1)
	{
		// Being called by SCM so do service work
		success = serviceInit(SERVICE_NAME);
		usage(argc, argv); // print usage in case attempting from command line
	}
	else
	{
		usage(argc, argv);
	}

	return success ? 0 : -1;
}

void usage(int argc, char **argv)
{
	printf(TR("Usage: %s {install|uninstall}\n"), argv[0]);
}
