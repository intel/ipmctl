/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_GOAL_COMMAND_
#define _SHOW_GOAL_COMMAND_

#define APPDIRECT_1_SIZE_TABLE_HEADER   L"AppDirect1Size"
#define APPDIRECT_2_SIZE_TABLE_HEADER   L"AppDirect2Size"
#define MEMORY_SIZE_TABLE_HEADER        L"MemorySize"
#define SOCKET_ID_TABLE_HEADER          L"SocketID"
#define DIMM_ID_TABLE_HEADER            L"DimmID"

#define STATUS_STR                      L"Status"
#define NOT_APPLICABLE_SHORT_STR        L"N/A"
#define CLI_REBOOT_REQUIRED_MSG         L"A reboot is required to process new memory allocation goals.\n"
#define CLI_NO_GOALS_MSG                L"There are no goal configs defined in the system.\nPlease use 'show -region' to display currently valid persistent memory regions.\n"
#define CLI_GET_REGION_MSG              L"Get region configuration goal"
#define CLI_GET_REGION_ON_MSG           L" on"
#define MIRRORED_STR                    L" Mirrored"
#define CLI_CREATE_SUCCESS_STATUS       L"Created following region configuration goal\n"

#include <Uefi.h>

/**
  Register the Show Goal command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowGoalCommand();

/**
  Print results of show goal according to table view

  @param[in] pCmd command from CLI
  @param[in] pRegionConfigsInfo - Region Config table to be printed
  @param[in] CurrentUnits The requested type of units to convert the capacity into
  @param[in] RegionConfigsCount - Number of elements in array
  @retval EFI_SUCCESS if printing is successful
  @retval EFI_INVALID_PARAMETER if input parameter is incorrect
**/
EFI_STATUS
ShowGoalPrintTableView(
    IN    struct Command *pCmd,
    IN    REGION_GOAL_PER_DIMM_INFO *pRegionConfigsInfo,
    IN    UINT16 CurrentUnits,
    IN    UINT32 RegionConfigsCount,
    IN    BOOLEAN Buffered
);

/**
  Execute the Show Goal command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_READY Invalid device state to perform action
**/
EFI_STATUS
ShowGoal(
  IN     struct Command *pCmd
  );

#endif /** _SHOW_GOAL_COMMAND_ **/
