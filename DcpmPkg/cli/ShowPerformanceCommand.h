/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_PERFORMANCE_COMMAND_H_
#define _SHOW_PERFORMANCE_COMMAND_H_

#include <Uefi.h>

/**
Execute the Show Performance command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowPerformance(
	IN     struct Command *pCmd
);

/*
* Register the show dimms command
*/
EFI_STATUS
RegisterShowPerformanceCommand(
);

#endif //_SHOW_PERFORMANCE_COMMAND_H_
