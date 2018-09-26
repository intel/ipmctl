/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowHostServerCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include "Common.h"
#include <Utility.h>

#define DS_ROOT_PATH                        L"/HostServer"

/**
  Command syntax definition
**/
struct Command ShowHostServerCommand = {
  SHOW_VERB,                                                                                    //!< verb
  { {ALL_OPTION_SHORT, ALL_OPTION, L"", L"", FALSE, ValueEmpty },
    {DISPLAY_OPTION_SHORT, DISPLAY_OPTION, L"", HELP_TEXT_ATTRIBUTES, FALSE, ValueRequired }
#ifdef OS_BUILD
    ,{ OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, FALSE, ValueRequired }
#endif
  },   //!< options
  {{SYSTEM_TARGET, L"", L"", TRUE, ValueEmpty},
   {HOST_TARGET, L"", L"", TRUE, ValueEmpty }},                                                //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                                                      //!< properties
  L"Show basic information about the host server.",                                             //!< help
  ShowHostServer,
  TRUE
};

CHAR16 *mppAllowedShowHostServerDisplayValues[] = {
  DISPLAYED_NAME_STR,
  DISPLAYED_OS_NAME_STR,
  DISPLAYED_OS_VERSION_STR,
  DISPLAYED_MIXED_SKU_STR,
  DISPLAYED_SKU_VIOLATION_STR
};

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
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  HOST_SERVER_INFO HostServerinfo;
  BOOLEAN IsMixedSku;
  BOOLEAN IsSkuViolation;
  BOOLEAN AllOptionSet = FALSE;
  BOOLEAN DisplayOptionSet = FALSE;
  BOOLEAN ShowAll = FALSE;
  CHAR16 *pDisplayValues = NULL;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  /** if the all option was specified **/
  if (containsOption(pCmd, ALL_OPTION) || containsOption(pCmd, ALL_OPTION_SHORT)) {
    AllOptionSet = TRUE;
  }
  /** if the display option was specified **/
  pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION);
  if (pDisplayValues) {
    DisplayOptionSet = TRUE;
  }
  else {
    pDisplayValues = getOptionValue(pCmd, DISPLAY_OPTION_SHORT);
    if (pDisplayValues) {
      DisplayOptionSet = TRUE;
    }
  }

  /** make sure they didn't specify both the all and display options **/
  if (AllOptionSet && DisplayOptionSet) {
    ReturnCode = EFI_INVALID_PARAMETER;
    NVDIMM_WARN("Options used together");
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPTIONS_ALL_DISPLAY_USED_TOGETHER);
    goto Finish;
  }

  /** check that the display parameters are correct (if display option is set) **/
  if (DisplayOptionSet) {
    ReturnCode = CheckDisplayList(pDisplayValues, mppAllowedShowHostServerDisplayValues,
        ALLOWED_DISP_VALUES_COUNT(mppAllowedShowHostServerDisplayValues));
    if (EFI_ERROR(ReturnCode)) {
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INCORRECT_VALUE_OPTION_DISPLAY);
      goto Finish;
    }
  }

  ShowAll = (!AllOptionSet && !DisplayOptionSet) || AllOptionSet;

  ReturnCode = ReadRunTimeCliDisplayPreferences(&DisplayPreferences);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  ReturnCode = GetHostServerInfo(&HostServerinfo);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: GetHostServerInfo Failed\n");
    goto Finish;
  }


  ReturnCode = IsDimmsMixedSkuCfg(pPrinterCtx, pNvmDimmConfigProtocol, &IsMixedSku, &IsSkuViolation);
  if (EFI_ERROR(ReturnCode)) {
     goto Finish;
  }

  PRINTER_BUILD_KEY_PATH(&pPath, DS_ROOT_PATH);

  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_NAME_STR))) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DISPLAYED_NAME_STR, HostServerinfo.Name);
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_OS_NAME_STR))) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DISPLAYED_OS_NAME_STR, HostServerinfo.OsName);
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_OS_VERSION_STR))) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DISPLAYED_OS_VERSION_STR, HostServerinfo.OsVersion);
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_MIXED_SKU_STR))) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DISPLAYED_MIXED_SKU_STR, (IsMixedSku == TRUE) ? L"1" : L"0");
  }
  if (ShowAll || (DisplayOptionSet && ContainsValue(pDisplayValues, DISPLAYED_SKU_VIOLATION_STR))) {
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DISPLAYED_SKU_VIOLATION_STR, (IsSkuViolation == TRUE) ? L"1" : L"0");
  }

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pDisplayValues);
  FREE_POOL_SAFE(pPath);
  return  ReturnCode;
}

/**
  Register the show memory resources command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowHostServerCommand(
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowHostServerCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STATUS IsDimmsMixedSkuCfg(PRINT_CONTEXT *pPrinterCtx,
  EFI_DCPMM_CONFIG_PROTOCOL *pNvmDimmConfigProtocol,
  BOOLEAN *pIsMixedSku,
  BOOLEAN *pIsSkuViolation)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT32 DimmCount = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 i;

  ReturnCode = pNvmDimmConfigProtocol->GetDimmCount(pNvmDimmConfigProtocol, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    return ReturnCode;
  }

  pDimms = AllocateZeroPool(sizeof(*pDimms) * DimmCount);

  if (pDimms == NULL) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OUT_OF_MEMORY);
    return ReturnCode;
  }
  /** retrieve the DIMM list **/
  ReturnCode = pNvmDimmConfigProtocol->GetDimms(pNvmDimmConfigProtocol, DimmCount,
    DIMM_INFO_CATEGORY_PACKAGE_SPARING, pDimms);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_ABORTED;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_INTERNAL_ERROR);
    NVDIMM_WARN("Failed to retrieve the DIMM inventory found in NFIT");
    goto Finish;
  }

  *pIsMixedSku = FALSE;
  *pIsSkuViolation = FALSE;
  for (i = 0; i < DimmCount; ++i)
  {
    if (FALSE == IsDimmManageableByDimmInfo(&pDimms[i]))
    {
      continue;
    }

    if (pDimms[i].SKUViolation)
    {
      *pIsSkuViolation = TRUE;
    }

    if (NVM_SUCCESS != SkuComparison(pDimms[0].SkuInformation,
      pDimms[i].SkuInformation))
    {
      *pIsMixedSku = TRUE;
    }
  }

Finish:
  FreePool(pDimms);
  return ReturnCode;
}



/**
Get manageability state for Dimm

@param[in] pDimm the DIMM_INFO struct

@retval BOOLEAN whether or not dimm is manageable
**/
BOOLEAN
IsDimmManageableByDimmInfo(
  IN  DIMM_INFO *pDimm
)
{
  if (pDimm == NULL)
  {
    return FALSE;
  }

  return IsDimmManageableByValues(pDimm->SubsystemVendorId,
    pDimm->InterfaceFormatCodeNum,
    pDimm->InterfaceFormatCode,
    pDimm->SubsystemDeviceId,
    pDimm->FwVer.FwApiMajor,
    pDimm->FwVer.FwApiMinor);
}
