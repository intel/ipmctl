/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Library/ShellLib.h>
#include <Library/BaseMemoryLib.h>
#include "ShowSessionCommand.h"
#include <Debug.h>
#include <Types.h>
#include <NvmInterface.h>
#include <NvmLimits.h>
#include <Convert.h>
#include "Common.h"
#include <Utility.h>
#include <PbrDcpmm.h>

#define DS_ROOT_PATH                        L"/Session"
#define DS_TAG_PATH                         L"/Session/Tag"
#define DS_TAG_INDEX_PATH                   L"/Session/Tag[%d]"

#define TAG_ID_FORMAT                       L"0x%x"
#define TAG_ID_SELECTED_FORMAT              L"0x%x*"

EFI_STATUS
MapTagtoCurrentSessionState(
  IN  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol,
  OUT UINT32 *pTag
);

 /*
  *  PRINT LIST ATTRIBUTES
  *  ---TagId=0x0001---
  *     CliArgs=
  */
PRINTER_LIST_ATTRIB ShowSessionListAttributes =
{
 {
    {
      TAG_STR,                                            //GROUP LEVEL TYPE
      L"---" TAG_ID_STR L"=$(" TAG_ID_STR L")---",        //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT L"%ls=%ls",                         //NULL or KEY VAL FORMAT STR
      TAG_ID_STR                                          //NULL or IGNORE KEY LIST (K1;K2)
    }
  }
};

 /*
 *  PRINTER TABLE ATTRIBUTES (3 columns)
 *   TagID  | ExitCode | CliArgs
 *   ========================================================================
 *   0x0001 | X        | X      
 *   ...
 */
PRINTER_TABLE_ATTRIB ShowSessionTableAttributes =
{
  {
    {
      TAG_ID_STR,                                 //COLUMN HEADER
      DEFAULT_MAX_STR_WIDTH,                      //COLUMN MAX STR WIDTH
      DS_TAG_PATH PATH_KEY_DELIM TAG_ID_STR       //COLUMN DATA PATH
    },
    {
      CLI_ARGS_STR,                               //COLUMN HEADER
      DEFAULT_MAX_STR_WIDTH,                      //COLUMN MAX STR WIDTH
      DS_TAG_PATH PATH_KEY_DELIM CLI_ARGS_STR     //COLUMN DATA PATH
    }
  }
};

PRINTER_DATA_SET_ATTRIBS ShowSessionDataSetAttribs =
{
  &ShowSessionListAttributes,
  &ShowSessionTableAttributes
};

/**
  Command syntax definition
**/
struct Command ShowSessionCommand = {
  SHOW_VERB,                                                                                    //!< verb
  {
#ifdef OS_BUILD
    { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired },
#endif
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", L"", L"", L"",FALSE, ValueOptional}
  },                                                                                            //!< options
  {{SESSION_TARGET, L"", L"", TRUE, ValueEmpty}},                                               //!< targets
  {{L"", L"", L"", FALSE, ValueOptional}},                                                      //!< properties
  L"Show basic information about session pbr file",                                             //!< help
  ShowSession,
  TRUE,
  TRUE //exclude from PBR
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
ShowSession(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol = NULL;
  DISPLAY_PREFERENCES DisplayPreferences;
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;
  UINT32 TagCount = 0;
  UINT32 Index = 0;
  CHAR16 *pName = NULL;
  CHAR16 *pDescription = NULL;
  CHAR16 *pTagId = NULL;
  UINT32 TagId = INVALID_TAG_ID;
  UINT32 Signature;

  NVDIMM_ENTRY();

  ZeroMem(&DisplayPreferences, sizeof(DisplayPreferences));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  pPrinterCtx = pCmd->pPrintCtx;

  ReturnCode = ReadRunTimePreferences(&DisplayPreferences, DISPLAY_CLI_INFO);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_DISPLAY_PREFERENCES_RETRIEVE);
    goto Finish;
  }

  /**
    Make sure we can access the config protocol
  **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmPbrProtocolGuid, (VOID **)&pNvmDimmPbrProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  //Retreive the current TagID (CLI's job to track/increment/reset the tag id).
  PbrDcpmmDeserializeTagId(&TagId, 0);

  ReturnCode = pNvmDimmPbrProtocol->PbrGetTagCount(&TagCount);
  if (EFI_ERROR(ReturnCode)) {
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_FAILED_TO_GET_SESSION_TAG_COUNT);
    goto Finish;
  }

  for (Index = 0; Index < TagCount; ++Index) {
    if (Index == TagId) {
      pTagId = CatSPrintClean(NULL, TAG_ID_SELECTED_FORMAT, Index);
    }
    else {
      pTagId = CatSPrintClean(NULL, TAG_ID_FORMAT, Index);
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_TAG_INDEX_PATH, Index);

    ReturnCode = pNvmDimmPbrProtocol->PbrGetTag(Index, &Signature, &pName, &pDescription, NULL, NULL);
    if (ReturnCode == EFI_SUCCESS) {

      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, TAG_ID_STR, pTagId);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, CLI_ARGS_STR, pName);
    }
    FREE_POOL_SAFE(pTagId);
    FREE_POOL_SAFE(pName);
    FREE_POOL_SAFE(pDescription);
  }

  //Specify table attributes
  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowSessionDataSetAttribs);
  PRINTER_ENABLE_TEXT_TABLE_FORMAT(pPrinterCtx);

Finish:
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  NVDIMM_EXIT_I64(ReturnCode);
  FREE_POOL_SAFE(pPath);
  FREE_POOL_SAFE(pTagId);
  FREE_POOL_SAFE(pName);
  FREE_POOL_SAFE(pDescription);
  return  ReturnCode;
}

/**
  Register the show session command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowSessionCommand(
  EFI_DCPMM_PBR_PROTOCOL *pNvmDimmPbrProtocol
  )
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowSessionCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}