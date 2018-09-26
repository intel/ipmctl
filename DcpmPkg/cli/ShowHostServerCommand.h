/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SRC_CLI_SHOWHOSTSERVERCOMMAND_H_
#define _SRC_CLI_SHOWHOSTSERVERCOMMAND_H_

#include "CommandParser.h"

#define DISPLAYED_NAME_STR                      L"Name"
#define DISPLAYED_OS_NAME_STR                   L"OsName"
#define DISPLAYED_OS_VERSION_STR                L"OsVersion"
#define DISPLAYED_MIXED_SKU_STR                 L"MixedSKU"
#define DISPLAYED_SKU_VIOLATION_STR             L"SKUViolation"

/**
Execute the show host server command

@param[in] pCmd command from CLI

@retval EFI_SUCCESS success
@retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
@retval EFI_OUT_OF_RESOURCES memory allocation failure
@retval EFI_ABORTED invoking CONFIG_PROTOGOL function failure
**/
EFI_STATUS
ShowHostServer(
   IN     struct Command *pCmd
);

/**
  Register the show host server command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowHostServerCommand(
  );


EFI_STATUS IsDimmsMixedSkuCfg(
   PRINT_CONTEXT *pPrinterCtx,
   EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
   BOOLEAN *pIsMixedSku,
   BOOLEAN *pIsSkuViolation);



/**
Get manageability state for Dimm

@param[in] pDimm the DIMM_INFO struct

@retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmManageableByDimmInfo(
  IN  DIMM_INFO *pDimm
);


#endif /* _SRC_CLI_SHOWHOSTSERVERCOMMAND_H_ */
