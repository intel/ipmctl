/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <Debug.h>
#include <Library/BaseMemoryLib.h>
#include <Convert.h>
#include "CommandParser.h"
#include "Common.h"
#include "ShowPcdCommand.h"
#include <PcdCommon.h>
#include <LbaCommon.h>

/**
  Command syntax definition
**/
struct Command ShowPcdCommand =
{
  SHOW_VERB,                                                          //!< verb
  {                                                                   //!< options
  {VERBOSE_OPTION_SHORT, VERBOSE_OPTION, L"", L"", HELP_VERBOSE_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_DDRT, L"", L"",HELP_DDRT_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", PROTOCOL_OPTION_SMBUS, L"", L"",HELP_SMBUS_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", LARGE_PAYLOAD_OPTION, L"", L"", HELP_LPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
    {L"", SMALL_PAYLOAD_OPTION, L"", L"", HELP_SPAYLOAD_DETAILS_TEXT, FALSE, ValueEmpty},
#ifdef OS_BUILD
  { OUTPUT_OPTION_SHORT, OUTPUT_OPTION, L"", OUTPUT_OPTION_HELP, HELP_OPTIONS_DETAILS_TEXT, FALSE, ValueRequired }
#else
  {L"", L"", L"", L"",L"", FALSE, ValueOptional}
#endif
  },
  {                                                                   //!< targets
    {DIMM_TARGET, L"", HELP_TEXT_DIMM_IDS, FALSE, ValueOptional},
    {PCD_TARGET, L"", PCD_CONFIG_TARGET_VALUE L"|" PCD_LSA_TARGET_VALUE, TRUE, ValueOptional}
  },
  {{L"", L"", L"", FALSE, ValueOptional}},                                    //!< properties
  L"Show platform configuration data (PCD) stored on one or more " PMEM_MODULES_STR L".",    //!< help
  ShowPcd,
  TRUE
};

#define DS_ROOT_PATH                      L"/PcdList"
#define DS_DIMM_PATH                      L"/PcdList/Dimm"
#define DS_DIMM_INDEX_PATH                L"/PcdList/Dimm[%d]"
#define DS_PCD_PATH                       L"/PcdList/Dimm/Pcd"
#define DS_PCD_INDEX_PATH                 L"/PcdList/Dimm[%d]/Pcd[%d]"
#define DS_TABLE_PATH                     L"/PcdList/Dimm/Pcd/Table"
#define DS_TABLE_INDEX_PATH               L"/PcdList/Dimm[%d]/Pcd[%d]/Table[%d]"
#define DS_PCD_PCAT_TABLE_INDEX_PATH      L"/PcdList/Dimm[%d]/Pcd[%d]/Table[%d]/PcatTable[%d]"
#define DS_IDENTIFICATION_INFO_INDEX_PATH L"/PcdList/Dimm[%d]/Pcd[%d]/Table[%d]/PcatTable[%d]/IdentificationInfoTable[%d]"

#define PCD_TARGET_CONFIG_STR             L"Target cfg "
#define PCD_TARGET_NAMESPACE_STR          L"Target namespace "
#define PCD_TABLE_STR                     L"Table"
#define PCD_STR                           L"Pcd"
#define PCD_PCAT_TABLE_STR                L"PcatTable"
#define PCD_IDENTIFICATION_INFO_STR       L"IdentificationInfoTable"

#define COLUMN_IN_HEX_DUMP      16

CHAR16 *gpPathLba = NULL;
PRINT_CONTEXT *gpPrinterCtxLbaCommon = NULL;

UINT16 gDimmIndex = 0;
UINT8 ConfigIndex = 0;

PRINTER_LIST_ATTRIB ShowPcdListAttributes =
{
 {
    {
      DIMM_NODE_STR,                                                                                                           //GROUP LEVEL TYPE
      L"--" DIMM_ID_STR L":$(" DIMM_ID_STR L")--",                                                                             //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                                                                             //NULL or KEY VAL FORMAT STR
      DIMM_ID_STR                                                                                                              //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      PCD_STR,                                                                                                                 //GROUP LEVEL TYPE
      L"--" PCD_STR L":$(" PCD_STR L")--",                                                                                     //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                                                                             //NULL or KEY VAL FORMAT STR
      PCD_STR                                                                                                                  //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      PCD_TABLE_STR,                                                                                                           //GROUP LEVEL TYPE
      SHOW_LIST_IDENT L"---" PCD_TABLE_STR L": $(" PCD_TABLE_STR L")---",                                                      //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                                                             //NULL or KEY VAL FORMAT STR
      PCD_TABLE_STR                                                                                                            //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      PCD_PCAT_TABLE_STR,                                                                                             //GROUP LEVEL TYPE
      SHOW_LIST_IDENT SHOW_LIST_IDENT PCD_PCAT_TABLE_STR SHOW_LIST_IDENT L": $(" PCD_PCAT_TABLE_STR L")",    //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                                                             //NULL or KEY VAL FORMAT STR
      PCD_PCAT_TABLE_STR                                                                                              //NULL or IGNORE KEY LIST (K1;K2)
    },
    {
      PCD_IDENTIFICATION_INFO_STR,                                                                                             //GROUP LEVEL TYPE
      SHOW_LIST_IDENT SHOW_LIST_IDENT PCD_IDENTIFICATION_INFO_STR SHOW_LIST_IDENT L": $(" PCD_IDENTIFICATION_INFO_STR L")",    //NULL or GROUP LEVEL HEADER
      SHOW_LIST_IDENT SHOW_LIST_IDENT FORMAT_STR L": " FORMAT_STR,                                                             //NULL or KEY VAL FORMAT STR
      PCD_IDENTIFICATION_INFO_STR                                                                                              //NULL or IGNORE KEY LIST (K1;K2)
    }
 }
};

PRINTER_DATA_SET_ATTRIBS ShowPcdDataSetAttribs =
{
  &ShowPcdListAttributes,
  NULL
};

/**
  Register the Show PCD command

  @retval EFI_SUCCESS success
  @retval EFI_ABORTED registering failure
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RegisterShowPcdCommand(
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  NVDIMM_ENTRY();

  ReturnCode = RegisterCommand(&ShowPcdCommand);

  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

STATIC
EFI_STATUS
GetPcdTarget(
  IN     CHAR16 *pTargetValue,
  OUT UINT8 *pPcdTarget
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  NVDIMM_ENTRY();

  if (pTargetValue == NULL || pPcdTarget == NULL) {
    goto Finish;
  }

  if (StrLen(pTargetValue) == 0) {
    *pPcdTarget = PCD_TARGET_ALL;
  }
  else if (StrICmp(pTargetValue, PCD_CONFIG_TARGET_VALUE) == 0) {
    *pPcdTarget = PCD_TARGET_CONFIG;
  }
  else if (StrICmp(pTargetValue, PCD_LSA_TARGET_VALUE) == 0) {
    *pPcdTarget = PCD_TARGET_NAMESPACES;
  }
  else {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Execute the Show PCD command

  @param[in] pCmd command from CLI

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pCmd is NULL or invalid command line parameters
  @retval EFI_NOT_FOUND couldn't open Config Protocol
  @retval EFI_ABORTED internal
**/
EFI_STATUS
ShowPcd(
  IN     struct Command *pCmd
  )
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  COMMAND_STATUS *pCommandStatus = NULL;
  EFI_DCPMM_CONFIG2_PROTOCOL *pNvmDimmConfigProtocol = NULL;
  DIMM_PCD_INFO *pDimmPcdInfo = NULL;
  UINT32 DimmPcdInfoCount = 0;
  UINT16 *pDimmIds = NULL;
  UINT32 DimmIdsCount = 0;
  CHAR16 *pTargetValue = NULL;
  UINT8 PcdTarget = PCD_TARGET_ALL;
  UINT32 Index = 0;
  DIMM_INFO *pDimms = NULL;
  UINT32 DimmCount = 0;
  CHAR16 DimmStr[MAX_DIMM_UID_LENGTH];
  PRINT_CONTEXT *pPrinterCtx = NULL;
  CHAR16 *pPath = NULL;

  NVDIMM_ENTRY();

  SetDisplayInfo(L"ShowPcd", ResultsView, NULL);

  ZeroMem(DimmStr, sizeof(DimmStr));

  if (pCmd == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_NO_COMMAND);
    goto Finish;
  }

  /**
    Printing will still work via compatibility mode if NULL so no need to check for NULL.
  **/
  pPrinterCtx = pCmd->pPrintCtx;

  /** Get config protocol **/
  ReturnCode = OpenNvmDimmProtocol(gNvmDimmConfigProtocolGuid, (VOID **)&pNvmDimmConfigProtocol, NULL);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_NOT_FOUND;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, CLI_ERR_OPENING_CONFIG_PROTOCOL);
    goto Finish;
  }

  // Populate the list of DIMM_INFO structures with relevant information
  ReturnCode = GetAllDimmList(pNvmDimmConfigProtocol, pCmd, DIMM_INFO_CATEGORY_NONE, &pDimms, &DimmCount);
  if (EFI_ERROR(ReturnCode)) {
    if (ReturnCode == EFI_NOT_FOUND) {
      PRINTER_SET_MSG(pCmd->pPrintCtx, ReturnCode, CLI_INFO_NO_DIMMS);
    }
    goto Finish;
  }

  ReturnCode = InitializeCommandStatus(&pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  if (ContainTarget(pCmd, DIMM_TARGET)) {
    pTargetValue = GetTargetValue(pCmd, DIMM_TARGET);
    ReturnCode = GetDimmIdsFromString(pCmd, pTargetValue, pDimms, DimmCount, &pDimmIds, &DimmIdsCount);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }
    if (!AllDimmsInListAreManageable(pDimms, DimmCount, pDimmIds, DimmIdsCount)) {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_UNMANAGEABLE_DIMM);
      goto Finish;
    }
  }

  pTargetValue = GetTargetValue(pCmd, PCD_TARGET);
  ReturnCode = GetPcdTarget(pTargetValue, &PcdTarget);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, FORMAT_STR_NL, CLI_ERR_INCORRECT_VALUE_TARGET_PCD);
    goto Finish;
  }

  ReturnCode = pNvmDimmConfigProtocol->GetPcd(pNvmDimmConfigProtocol, PcdTarget, pDimmIds, DimmIdsCount,
    &pDimmPcdInfo, &DimmPcdInfoCount, pCommandStatus);
  if (EFI_ERROR(ReturnCode)) {
    ReturnCode = MatchCliReturnCode(pCommandStatus->GeneralStatus);
    goto Finish;
  }

  for (Index = 0; Index < DimmPcdInfoCount; Index++) {
    ReturnCode = GetPreferredDimmIdAsString(pDimmPcdInfo[Index].DimmId, pDimmPcdInfo[Index].DimmUid,
      DimmStr, MAX_DIMM_UID_LENGTH);
    if (EFI_ERROR(ReturnCode)) {
      goto Finish;
    }

    PRINTER_BUILD_KEY_PATH(pPath, DS_DIMM_INDEX_PATH, Index);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, DIMM_ID_STR, DimmStr);

    if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_NAMESPACES) {
      PRINTER_BUILD_KEY_PATH(pPath, DS_PCD_INDEX_PATH, Index, 0);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PCD_STR, L"LSA");
      PrintLabelStorageArea(pDimmPcdInfo[Index].pLabelStorageArea, pPrinterCtx, pPath);
    }

    if (PcdTarget == PCD_TARGET_ALL || PcdTarget == PCD_TARGET_CONFIG) {
      PRINTER_BUILD_KEY_PATH(pPath, DS_PCD_INDEX_PATH, Index, 1);
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PCD_STR, L"Config");
      PrintPcdConfigurationHeader(pDimmPcdInfo[Index].pConfHeader, pPrinterCtx, pPath);
    }
    gDimmIndex++;
  }

  ReturnCode = EFI_SUCCESS;

  PRINTER_CONFIGURE_DATA_ATTRIBUTES(pPrinterCtx, DS_ROOT_PATH, &ShowPcdDataSetAttribs);
  PRINTER_ENABLE_LIST_TABLE_FORMAT(pPrinterCtx);
  pPrinterCtx->DoNotPrintGeneralStatusSuccessCode = TRUE;
Finish:
  PRINTER_SET_COMMAND_STATUS(pPrinterCtx, ReturnCode, CLI_INFO_SHOW_PCD, CLI_INFO_ON, pCommandStatus);
  PRINTER_PROCESS_SET_BUFFER(pPrinterCtx);
  FreeCommandStatus(&pCommandStatus);
  FreeDimmPcdInfoArray(pDimmPcdInfo, DimmPcdInfoCount);
  pDimmPcdInfo = NULL;
  FREE_POOL_SAFE(pDimmIds);
  FREE_POOL_SAFE(pDimms);
  FREE_POOL_SAFE(pPath);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
  Print Platform Config Data table header

  @param[in] pHeader table header
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdTableHeader(
  IN     TABLE_HEADER *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx,
    pPath,
    L"Signature                 ",
    L"%c%c%c%c",
    pHeader->Signature & 0xFF,
    (pHeader->Signature >> 8) & 0xFF,
    (pHeader->Signature >> 16) & 0xFF,
    (pHeader->Signature >> 24) & 0xFF);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Length                    ", FORMAT_HEX_NOWIDTH, pHeader->Length);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Revision                  ", FORMAT_HEX_NOWIDTH, pHeader->Revision);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Checksum                  ", FORMAT_HEX_NOWIDTH, pHeader->Checksum);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"OemId                     ", L"%c%c%c%c%c%c",
    pHeader->OemId[0],
    pHeader->OemId[1],
    pHeader->OemId[2],
    pHeader->OemId[3],
    pHeader->OemId[4],
    pHeader->OemId[5]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"OemTableId                ", L"%c%c%c%c%c%c",
    ((UINT8 *)&pHeader->OemTableId)[0],
    ((UINT8 *)&pHeader->OemTableId)[1],
    ((UINT8 *)&pHeader->OemTableId)[2],
    ((UINT8 *)&pHeader->OemTableId)[3],
    ((UINT8 *)&pHeader->OemTableId)[4],
    ((UINT8 *)&pHeader->OemTableId)[5]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"OemRevision               ", FORMAT_HEX_NOWIDTH, pHeader->OemRevision);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"CreatorId                 ", L"%c%c%c%c",
    ((UINT8 *)&pHeader->CreatorId)[0],
    ((UINT8 *)&pHeader->CreatorId)[1],
    ((UINT8 *)&pHeader->CreatorId)[2],
    ((UINT8 *)&pHeader->CreatorId)[3]);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"CreatorRevision           ", FORMAT_HEX_NOWIDTH, pHeader->CreatorRevision);
}

/**
  Print Platform Config Data PCAT table header

  @param[in] pHeader PCAT table header
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdPcatTableHeader(
  IN     PCAT_TABLE_HEADER *pHeader,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Type                      ", FORMAT_HEX_NOWIDTH, pHeader->Type);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Length                    ", FORMAT_HEX_NOWIDTH, pHeader->Length);
}

/**
  Print Platform Config Data Partition Size Change table

  @param[in] pPartitionSizeChange Partition Size Change table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdPartitionSizeChange(
  IN     NVDIMM_PARTITION_SIZE_CHANGE *pPartitionSizeChange,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PCD_PCAT_TABLE_STR, L"PCD Partition Size Change");
  PrintPcdPcatTableHeader(&pPartitionSizeChange->Header, pPrinterCtx, pPath);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PartitionSizeChangeStatus ", FORMAT_HEX_NOWIDTH, pPartitionSizeChange->PartitionSizeChangeStatus);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PartitionSize             ", FORMAT_UINT64_HEX_NOWIDTH, pPartitionSizeChange->PmPartitionSize);
  PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"PartitionSize             ", L"\n");
}

/**
  Print  Platform Config Data Identification Information table

  @param[in] pIdentificationInfo Identification Information table
  @param[in] PcdConfigTableRevision Revision of the PCD Config tables
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdIdentificationInformation(
  IN     VOID *pIdentificationInfoTable,
  IN     ACPI_REVISION PcdConfigTableRevision,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  CHAR16 PartNumber[PART_NUMBER_SIZE + 1];
  CHAR16 *pTmpDimmUid = NULL;

  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, PCD_IDENTIFICATION_INFO_STR, L"PCD Identification Info");

  if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdConfigTableRevision)) {
    NVDIMM_IDENTIFICATION_INFORMATION *pIdentificationInfo = (NVDIMM_IDENTIFICATION_INFORMATION *)pIdentificationInfoTable;
    ZeroMem(PartNumber, sizeof(PartNumber));
    AsciiStrToUnicodeStrS(pIdentificationInfo->DimmIdentification.Version1.DimmPartNumber, PartNumber, PART_NUMBER_SIZE + 1);
    if (IS_ACPI_REV_MAJ_0_MIN_1(PcdConfigTableRevision)) {
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"DimmManufacturerId        ", FORMAT_HEX_NOWIDTH,
        EndianSwapUint16(pIdentificationInfo->DimmIdentification.Version1.DimmManufacturerId));
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"DimmSerialNumber          ", FORMAT_HEX_NOWIDTH,
        EndianSwapUint32(pIdentificationInfo->DimmIdentification.Version1.DimmSerialNumber));
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"DimmPartNumber            ", FORMAT_STR, PartNumber);
    }
    else {
      pTmpDimmUid = CatSPrint(NULL, L"%04x-%02x-%04x-%08x", EndianSwapUint16(pIdentificationInfo->DimmIdentification.Version2.Uid.ManufacturerId),
        pIdentificationInfo->DimmIdentification.Version2.Uid.ManufacturingLocation,
        EndianSwapUint16(pIdentificationInfo->DimmIdentification.Version2.Uid.ManufacturingDate),
        EndianSwapUint32(pIdentificationInfo->DimmIdentification.Version2.Uid.SerialNumber));
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"DimmUniqueIdentifer       ", FORMAT_STR, pTmpDimmUid);
    }
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PartitionOffset           ", FORMAT_UINT64_HEX_NOWIDTH, pIdentificationInfo->PartitionOffset);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PmPartitionSize           ", FORMAT_UINT64_HEX_NOWIDTH L"\n", pIdentificationInfo->PmPartitionSize);
  }
  else if (IS_ACPI_REV_MAJ_1_OR_MAJ_3(PcdConfigTableRevision)) {
    NVDIMM_IDENTIFICATION_INFORMATION3 *pIdentificationInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *)pIdentificationInfoTable;
    pTmpDimmUid = CatSPrint(NULL, L"%04x-%02x-%04x-%08x", EndianSwapUint16(pIdentificationInfo->DimmIdentification.ManufacturerId),
      pIdentificationInfo->DimmIdentification.ManufacturingLocation,
      EndianSwapUint16(pIdentificationInfo->DimmIdentification.ManufacturingDate),
      EndianSwapUint32(pIdentificationInfo->DimmIdentification.SerialNumber));
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"DimmUniqueIdentifer       ", FORMAT_STR, pTmpDimmUid);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"DimmLocation              ", FORMAT_UINT64_HEX_NOWIDTH, pIdentificationInfo->DimmLocation.AsUint64);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PartitionOffset           ", FORMAT_UINT64_HEX_NOWIDTH, pIdentificationInfo->PartitionOffset);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PmPartitionSize           ", FORMAT_UINT64_HEX_NOWIDTH L"\n", pIdentificationInfo->PmPartitionSize);
  }

  if (pTmpDimmUid != NULL) {
    FREE_POOL_SAFE(pTmpDimmUid);
  }
}

/**
  Print Platform Config Data Interleave Information table and its extension tables

  @param[in] pInterleaveInfo Interleave Information table
  @param[in] PcdConfigTableRevision Revision of the PCD Config tables
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdInterleaveInformation(
  IN     PCAT_TABLE_HEADER *pInterleaveInfoTable,
  IN     ACPI_REVISION PcdConfigTableRevision,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  UINT32 Index = 0;
  UINT32 IdentificationInfoIndex = 0;
  CHAR16 *pPathPcdIdentificationInfo = NULL;

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PCD_PCAT_TABLE_STR, L"PCD Interleave Info");
  if (IS_ACPI_REV_MAJ_0_MIN_VALID(PcdConfigTableRevision)) {
    NVDIMM_INTERLEAVE_INFORMATION *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION *)pInterleaveInfoTable;
    PrintPcdPcatTableHeader(&pInterleaveInfo->Header, pPrinterCtx, pPath);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveSetIndex        ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveSetIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"NumOfDimmsInInterleaveSet ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->NumOfDimmsInInterleaveSet);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveMemoryType      ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveMemoryType);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveFormatChannel   ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveFormatChannel);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveFormatImc       ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveFormatImc);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveFormatWays      ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveFormatWays);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveChangeStatus    ", FORMAT_HEX_NOWIDTH L"\n", pInterleaveInfo->InterleaveChangeStatus);

    NVDIMM_IDENTIFICATION_INFORMATION *pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION *)&pInterleaveInfo->pIdentificationInfoList;
    for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
      PRINTER_BUILD_KEY_PATH(pPathPcdIdentificationInfo, DS_IDENTIFICATION_INFO_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 1, Index);
      IdentificationInfoIndex++;
      PrintPcdIdentificationInformation(pCurrentIdentInfo, PcdConfigTableRevision, pPrinterCtx, pPathPcdIdentificationInfo);
      pCurrentIdentInfo++;
    }
  }
  else if (IS_ACPI_REV_MAJ_1_OR_MAJ_3(PcdConfigTableRevision)) {
    NVDIMM_INTERLEAVE_INFORMATION3 *pInterleaveInfo = (NVDIMM_INTERLEAVE_INFORMATION3 *)pInterleaveInfoTable;
    PrintPcdPcatTableHeader(&pInterleaveInfo->Header, pPrinterCtx, pPath);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveSetIndex        ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveSetIndex);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"NumOfDimmsInInterleaveSet ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->NumOfDimmsInInterleaveSet);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveMemoryType      ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveMemoryType);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveSizeChannel     ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveFormatChannel);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveSizeImc         ", FORMAT_HEX_NOWIDTH, pInterleaveInfo->InterleaveFormatImc);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"InterleaveChangeStatus    ", FORMAT_HEX_NOWIDTH , pInterleaveInfo->InterleaveChangeStatus);

    NVDIMM_IDENTIFICATION_INFORMATION3 *pCurrentIdentInfo = (NVDIMM_IDENTIFICATION_INFORMATION3 *)&pInterleaveInfo->pIdentificationInfoList;
    for (Index = 0; Index < pInterleaveInfo->NumOfDimmsInInterleaveSet; Index++) {
      PRINTER_BUILD_KEY_PATH(pPathPcdIdentificationInfo, DS_IDENTIFICATION_INFO_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 1, Index);
      IdentificationInfoIndex++;
      PrintPcdIdentificationInformation(pCurrentIdentInfo, PcdConfigTableRevision, pPrinterCtx, pPathPcdIdentificationInfo);
      pCurrentIdentInfo++;
    }
  }
  FREE_POOL_SAFE(pPathPcdIdentificationInfo);
}

VOID
PrintPcdConfigManagementAttributesInformation(
  IN    CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  INTEL_DIMM_CONFIG *pIntelDIMMConfig = NULL;
  EFI_GUID IntelDimmConfigVariableGuid = INTEL_DIMM_CONFIG_VARIABLE_GUID;
  CHAR16 *pGuidStr = NULL;

  pGuidStr = GuidToStr(&pConfigManagementAttributesInfo->Guid);

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Mngmt Table            ", L"PCD Cfg Mngmt Attr Extension");
  PrintPcdPcatTableHeader(&pConfigManagementAttributesInfo->Header, pPrinterCtx, pPath);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"VendorID               ", FORMAT_HEX_NOWIDTH, pConfigManagementAttributesInfo->VendorId);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"GUID                   ", FORMAT_STR_NL, pGuidStr);

  if (CompareGuid(&pConfigManagementAttributesInfo->Guid, &IntelDimmConfigVariableGuid)) {
    pIntelDIMMConfig = (INTEL_DIMM_CONFIG *)pConfigManagementAttributesInfo->pGuidData;
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Revision                ", FORMAT_INT32, pIntelDIMMConfig->Revision);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ProvisionCapacityMode   ", FORMAT_INT32, pIntelDIMMConfig->ProvisionCapacityMode);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"MemorySize              ", FORMAT_INT32, pIntelDIMMConfig->MemorySize);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PMType                  ", FORMAT_INT32, pIntelDIMMConfig->PMType);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ProvisionNamespaceMode  ", FORMAT_INT32, pIntelDIMMConfig->ProvisionNamespaceMode);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"NamespaceFlags          ", FORMAT_INT32, pIntelDIMMConfig->NamespaceFlags);
    PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"NamespaceLabelVersion   ", FORMAT_INT32, pIntelDIMMConfig->NamespaceLabelVersion);
  }

  FREE_POOL_SAFE(pGuidStr);
}

/**
  Print Platform Config Data Current Config table and its PCAT tables

  @param[in] pCurrentConfig Current Config table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdCurrentConfig(
  IN     NVDIMM_CURRENT_CONFIG *pCurrentConfig,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pPathPcdPcatTable = NULL;

  if (IS_ACPI_HEADER_REV_INVALID(pCurrentConfig)){
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: Invalid revision value %d for PCD current config table.", pCurrentConfig->Header.Revision);
    return;
  }

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PCD_TABLE_STR, L"PCD Current Config");
  PrintPcdTableHeader(&pCurrentConfig->Header, pPrinterCtx, pPath);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ConfigStatus              ", FORMAT_HEX_NOWIDTH, pCurrentConfig->ConfigStatus);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"VolatileMemSizeIntoSpa    ", FORMAT_UINT64_HEX_NOWIDTH, pCurrentConfig->VolatileMemSizeIntoSpa);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"PersistentMemSizeIntoSpa  ", FORMAT_UINT64_HEX_NOWIDTH, pCurrentConfig->PersistentMemSizeIntoSpa);
  PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"PersistentMemSizeIntoSpa  ", L"\n");

  /**
    Check if there is at least one PCAT table
  **/
  if (pCurrentConfig->Header.Length <= sizeof(*pCurrentConfig)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *)&pCurrentConfig->pPcatTables;
  SizeOfPcatTables = pCurrentConfig->Header.Length - (UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)pCurrentConfig);

  /**
    Example of the use of the while loop condition
    PCAT table #1   offset:  0   size: 10
    PCAT table #2   offset: 10   size:  5
    Size of PCAT tables: 15 (10 + 5)

    Iteration #1:   offset: 0
    Iteration #2:   offset: 10
    Iteration #3:   offset: 15   stop the loop: offset isn't less than size
  **/
  while ((UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)&pCurrentConfig->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 0);

      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *)pCurPcatTable;

      PrintPcdPartitionSizeChange(pPartSizeChange, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 1);

      PrintPcdInterleaveInformation(pCurPcatTable, pCurrentConfig->Header.Revision, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pCurPcatTable->Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
      pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *)pCurPcatTable;

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
    }
    else {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: wrong PCAT table type");
      break;
    }
    FREE_POOL_SAFE(pPathPcdPcatTable);
  }
}

/**
  Print Platform Config Data Config Input table and its PCAT tables

  @param[in] pConfigInput Config Input table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdConfInput(
  IN     NVDIMM_PLATFORM_CONFIG_INPUT *pConfigInput,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pPathPcdPcatTable = NULL;

  if (IS_ACPI_HEADER_REV_INVALID(pConfigInput)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: Invalid revision value %d for PCD config input table.", pConfigInput->Header.Revision);
    return;
  }

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PCD_TABLE_STR, L"Platform Config Data Conf Input table");
  PrintPcdTableHeader(&pConfigInput->Header, pPrinterCtx, pPath);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"SequenceNumber             ", FORMAT_HEX_NOWIDTH, pConfigInput->SequenceNumber);
  PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"SequenceNumber             ", L"\n");

  /**
    Check if there is at least one PCAT table
  **/
  if (pConfigInput->Header.Length <= sizeof(*pConfigInput)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *)&pConfigInput->pPcatTables;
  SizeOfPcatTables = pConfigInput->Header.Length - (UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)pConfigInput);

  /**
    Example of the use of the while loop condition
    PCAT table #1   offset:  0   size: 10
    PCAT table #2   offset: 10   size:  5
    Size of PCAT tables: 15 (10 + 5)

    Iteration #1:   offset: 0
    Iteration #2:   offset: 10
    Iteration #3:   offset: 15   stop the loop: offset isn't less than size
  **/
  while ((UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)&pConfigInput->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 0);

      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *)pCurPcatTable;

      PrintPcdPartitionSizeChange(pPartSizeChange, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 1);

      PrintPcdInterleaveInformation(pCurPcatTable, pConfigInput->Header.Revision, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pCurPcatTable->Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 2);

      pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *)pCurPcatTable;

      PrintPcdConfigManagementAttributesInformation(pConfigManagementAttributesInfo, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
    }
    else {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: wrong PCAT table type");
      break;
    }
    FREE_POOL_SAFE(pPathPcdPcatTable);
  }
}

/**
  Print Platform Config Data Config Output table and its PCAT tables

  @param[in] pConfigOutput Config Output table
  @param[in] pPrinterCtx pointer for printer
  @param[in] pPath
**/
VOID
PrintPcdConfOutput(
  IN     NVDIMM_PLATFORM_CONFIG_OUTPUT *pConfigOutput,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  NVDIMM_PARTITION_SIZE_CHANGE *pPartSizeChange = NULL;
  CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *pConfigManagementAttributesInfo = NULL;
  PCAT_TABLE_HEADER *pCurPcatTable = NULL;
  UINT32 SizeOfPcatTables = 0;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *pPathPcdPcatTable = NULL;

  if (IS_ACPI_HEADER_REV_INVALID(pConfigOutput)) {
    ReturnCode = EFI_INVALID_PARAMETER;
    PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: Invalid revision value %d for PCD config output table.", pConfigOutput->Header.Revision);
    return;
  }

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, PCD_TABLE_STR, L"Platform Config Data Conf Output table");
  PrintPcdTableHeader(&pConfigOutput->Header, pPrinterCtx, pPath);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"SequenceNumber             ", FORMAT_HEX_NOWIDTH, pConfigOutput->SequenceNumber);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ValidationStatus           ", FORMAT_HEX_NOWIDTH, pConfigOutput->ValidationStatus);
  PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"ValidationStatus           ", L"\n");

  /** Check if there is at least one PCAT table **/
  if (pConfigOutput->Header.Length <= sizeof(*pConfigOutput)) {
    return;
  }

  pCurPcatTable = (PCAT_TABLE_HEADER *)&pConfigOutput->pPcatTables;
  SizeOfPcatTables = pConfigOutput->Header.Length - (UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)pConfigOutput);

  /**
    Example of the use of the while loop condition
    PCAT table #1   offset:  0   size: 10
    PCAT table #2   offset: 10   size:  5
    Size of PCAT tables: 15 (10 + 5)

    Iteration #1:   offset: 0
    Iteration #2:   offset: 10
    Iteration #3:   offset: 15   stop the loop: offset isn't less than size
  **/
  while ((UINT32)((UINT8 *)pCurPcatTable - (UINT8 *)&pConfigOutput->pPcatTables) < SizeOfPcatTables) {
    if (pCurPcatTable->Type == PCAT_TYPE_PARTITION_SIZE_CHANGE_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 0);

      pPartSizeChange = (NVDIMM_PARTITION_SIZE_CHANGE *)pCurPcatTable;

      PrintPcdPartitionSizeChange(pPartSizeChange, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pPartSizeChange->Header.Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_INTERLEAVE_INFORMATION_TABLE) {
      PRINTER_BUILD_KEY_PATH(pPathPcdPcatTable, DS_PCD_PCAT_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex, 1);

      PrintPcdInterleaveInformation(pCurPcatTable, pConfigOutput->Header.Revision, pPrinterCtx, pPathPcdPcatTable);

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pCurPcatTable->Length);
    }
    else if (pCurPcatTable->Type == PCAT_TYPE_CONFIG_MANAGEMENT_ATTRIBUTES_TABLE) {
      pConfigManagementAttributesInfo = (CONFIG_MANAGEMENT_ATTRIBUTES_EXTENSION_TABLE *)pCurPcatTable;

      pCurPcatTable = GET_VOID_PTR_OFFSET(pCurPcatTable, pConfigManagementAttributesInfo->Header.Length);
    }
    else {
      ReturnCode = EFI_INVALID_PARAMETER;
      PRINTER_SET_MSG(pPrinterCtx, ReturnCode, L"Error: wrong PCAT table type");
      break;
    }
    FREE_POOL_SAFE(pPathPcdPcatTable);
  }
}

/**
   Print Platform Config Data Configuration Header table and all subtables

   @param[in] pConfHeader Configuration Header table
   @param[in] pPrinterCtx pointer for printer
   @param[in] pPath
**/
VOID
PrintPcdConfigurationHeader(
  IN     NVDIMM_CONFIGURATION_HEADER *pConfHeader,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  CHAR16 *pPathPcdTable = NULL;

  ConfigIndex = 0;
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Table                     ", L"PCD Config Header");
  PrintPcdTableHeader(&pConfHeader->Header, pPrinterCtx, pPath);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"CurrentConfDataSize       ", FORMAT_HEX_NOWIDTH, pConfHeader->CurrentConfDataSize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"CurrentConfStartOffset    ", FORMAT_HEX_NOWIDTH, pConfHeader->CurrentConfStartOffset);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ConfInputDataSize         ", FORMAT_HEX_NOWIDTH, pConfHeader->ConfInputDataSize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ConfInputDataOffset       ", FORMAT_HEX_NOWIDTH, pConfHeader->ConfInputStartOffset);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ConfOutputDataSize        ", FORMAT_HEX_NOWIDTH, pConfHeader->ConfOutputDataSize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ConfOutputDataOffset      ", FORMAT_HEX_NOWIDTH, pConfHeader->ConfOutputStartOffset);
  PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"ConfOutputDataOffset      ", L"\n");

  if (pConfHeader->CurrentConfStartOffset != 0 && pConfHeader->CurrentConfDataSize != 0) {
    PRINTER_BUILD_KEY_PATH(pPathPcdTable, DS_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex);
    PrintPcdCurrentConfig(GET_NVDIMM_CURRENT_CONFIG(pConfHeader), pPrinterCtx, pPathPcdTable);
    ConfigIndex++;
  }
  if (pConfHeader->ConfInputStartOffset != 0 && pConfHeader->ConfInputDataSize != 0) {
    PRINTER_BUILD_KEY_PATH(pPathPcdTable, DS_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex);
    PrintPcdConfInput(GET_NVDIMM_PLATFORM_CONFIG_INPUT(pConfHeader), pPrinterCtx, pPathPcdTable);
    ConfigIndex++;
  }
  if (pConfHeader->ConfOutputStartOffset != 0 && pConfHeader->ConfOutputDataSize != 0) {
    PRINTER_BUILD_KEY_PATH(pPathPcdTable, DS_TABLE_INDEX_PATH, gDimmIndex, 1, ConfigIndex);
    PrintPcdConfOutput(GET_NVDIMM_PLATFORM_CONFIG_OUTPUT(pConfHeader), pPrinterCtx, pPathPcdTable);
    ConfigIndex++;
  }
  FREE_POOL_SAFE(pPathPcdTable);
}

/**
   Print Namespace Index

   @param[in] pNamespaceIndex Namespace Index
**/
VOID
PrintNamespaceIndex(
  IN     NAMESPACE_INDEX *pNamespaceIndex
)
{
  CHAR16 Buffer[NSINDEX_SIG_LEN + 1];

  if (pNamespaceIndex == NULL) {
    return;
  }

  ZeroMem(Buffer, sizeof(Buffer));
  AsciiStrToUnicodeStrS((CHAR8 *)&pNamespaceIndex->Signature, Buffer, NSINDEX_SIG_LEN + 1);

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Signature          ", FORMAT_STR, Buffer);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Flags              ", FORMAT_HEX_NOWIDTH, *pNamespaceIndex->Flags);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"LabelSize          ", FORMAT_HEX_NOWIDTH, pNamespaceIndex->LabelSize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Sequence           ", FORMAT_HEX_NOWIDTH, pNamespaceIndex->Sequence);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"MyOffset           ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceIndex->MyOffset);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"MySize             ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceIndex->MySize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"OtherOffset        ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceIndex->OtherOffset);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"LabelOffset        ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceIndex->LabelOffset);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"NumOfLabel         ", FORMAT_HEX_NOWIDTH, pNamespaceIndex->NumberOfLabels);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Major              ", FORMAT_HEX_NOWIDTH, pNamespaceIndex->Major);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Minor              ", FORMAT_HEX_NOWIDTH, pNamespaceIndex->Minor);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Checksum           ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceIndex->Checksum);

  PrintLsaHex(pNamespaceIndex->pFree,
    LABELS_TO_FREE_BYTES(ROUNDUP(pNamespaceIndex->NumberOfLabels, 8)));
}

/**
   Print Namespace Label

   @param[in] pNamespaceLabel Namespace Label
   @param[in] pPrinterCtx pointer for printer
   @param[in] pPath
**/
VOID
PrintNamespaceLabel(
  IN     NAMESPACE_LABEL *pNamespaceLabel,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  CHAR16 Buffer[NLABEL_NAME_LEN_WITH_TERMINATOR];
  CHAR16 *pUuidStr = NULL;
  CHAR16 *pTypeGuidStr = NULL;
  CHAR16 *pAddrAbstrGuidStr = NULL;

  if (pNamespaceLabel == NULL) {
    return;
  }

  ZeroMem(Buffer, sizeof(Buffer));
  AsciiStrToUnicodeStrS((CHAR8 *)&pNamespaceLabel->Name, Buffer, NLABEL_NAME_LEN_WITH_TERMINATOR);

  pUuidStr = GuidToStr(&pNamespaceLabel->Uuid);
  pTypeGuidStr = GuidToStr(&pNamespaceLabel->TypeGuid);
  pAddrAbstrGuidStr = GuidToStr(&pNamespaceLabel->AddressAbstractionGuid);

  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Uuid               ", FORMAT_STR, pUuidStr);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Name               ", FORMAT_STR, Buffer);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"LabelFlags         ", FORMAT_HEX_NOWIDTH, pNamespaceLabel->Flags);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"NumOfLabels        ", FORMAT_HEX_NOWIDTH, pNamespaceLabel->NumberOfLabels);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Position           ", FORMAT_HEX_NOWIDTH, pNamespaceLabel->Position);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"ISetCookie         ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceLabel->InterleaveSetCookie);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"LbaSize            ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceLabel->LbaSize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Dpa                ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceLabel->Dpa);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"RawSize            ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceLabel->RawSize);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Slot               ", FORMAT_HEX_NOWIDTH, pNamespaceLabel->Slot);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"Alignment          ", FORMAT_HEX_NOWIDTH, pNamespaceLabel->Alignment);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"TypeGuid           ", FORMAT_STR, pTypeGuidStr);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"AddrAbstrGuid      ", FORMAT_STR, pAddrAbstrGuidStr);
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(pPrinterCtx, pPath, L"LabelChecksum      ", FORMAT_UINT64_HEX_NOWIDTH, pNamespaceLabel->Checksum);
  PRINTER_APPEND_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"LabelChecksum      ", L"\n");


  FREE_POOL_SAFE(pUuidStr);
  FREE_POOL_SAFE(pTypeGuidStr);
  FREE_POOL_SAFE(pAddrAbstrGuidStr);
}

/**
   Print Label Storage Area and all subtables

   @param[in] pLba Label Storage Area
   @param[in] pPrinterCtx pointer for printer
   @param[in] pPath
**/
VOID
PrintLabelStorageArea(
  IN     LABEL_STORAGE_AREA *pLba,
  IN     PRINT_CONTEXT *pPrinterCtx,
  IN     CHAR16 *pPath
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  UINT16 Index = 0;
  UINT16 CurrentIndex = 0;
  UINT16 SlotStatus = SLOT_UNKNOWN;
  BOOLEAN First = FALSE;
  PRINT_CONTEXT *pPrinterCtxNsLabel = NULL;
  CHAR16 *pPathNsLabel = NULL;

  pPrinterCtxNsLabel = pPrinterCtx;
  if (pLba == NULL) {
    return;
  }

  ReturnCode = GetLsaIndexes(pLba, &CurrentIndex, NULL);
  if (EFI_ERROR(ReturnCode)) {
    return;
  }

  PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Label Storage Area ", L"Current Index");
  gpPathLba = pPath;
  gpPrinterCtxLbaCommon = pPrinterCtx;
  PrintNamespaceIndex(&pLba->Index[CurrentIndex]);

  for (Index = 0, First = TRUE; Index < pLba->Index[CurrentIndex].NumberOfLabels; Index++) {
    CheckSlotStatus(&pLba->Index[CurrentIndex], Index, &SlotStatus);
    if (SlotStatus == SLOT_FREE) {
      continue;
    }

    if (First) {
      PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtx, pPath, L"Labels", L"Label Storage Area ");
      First = FALSE;
    }
    PRINTER_BUILD_KEY_PATH(pPathNsLabel, DS_TABLE_INDEX_PATH, gDimmIndex, 0, Index);
    PRINTER_SET_KEY_VAL_WIDE_STR(pPrinterCtxNsLabel, pPathNsLabel, PCD_TABLE_STR, L"Namespace Label Info");
    PrintNamespaceLabel(&pLba->pLabels[Index], pPrinterCtxNsLabel, pPathNsLabel);
  }
  FREE_POOL_SAFE(pPathNsLabel);
}

/**
  Function that allows for nicely formatted HEX & ASCII console output.
  It can be used to inspect memory objects without a need for debugger or dumping raw DIMM data.

  @param[in] pBuffer Pointer to an arbitrary object
  @param[in] Bytes Number of bytes to display
**/
VOID
PrintLsaHex(
  IN     VOID *pBuffer,
  IN     UINT32 Bytes
)
{
  UINT8 Byte, AsciiBuffer[COLUMN_IN_HEX_DUMP];
  UINT16 Column, NextColumn, Index, Index2;
  UINT8 *pData;
  CHAR16 *message = NULL;
  CHAR16 *messageId = NULL;

  if (pBuffer == NULL) {
    NVDIMM_DBG("pBuffer is NULL");
    return;
  }
  PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, L"Hexdump", L"For %d bytes", Bytes);
  pData = (UINT8 *)pBuffer;
  for (Index = 0; Index < Bytes; Index++) {
    Column = Index % COLUMN_IN_HEX_DUMP;
    NextColumn = (Index + 1) % COLUMN_IN_HEX_DUMP;
    Byte = *(pData + Index);
    if (Column == 0) {
      messageId = CatSPrintClean(messageId, L"%.3d", Index);
    }

    if (Index % 8 == 0) {
      message = CatSPrintClean(message, L" ");
    }

    message = CatSPrintClean(message, L"%.2x", *(pData + Index));
    AsciiBuffer[Column] = IsAsciiAlnumCharacter(Byte) ? Byte : '.';
    if (NextColumn == 0 && Index != 0) {
      message = CatSPrintClean(message, L" ");
      for (Index2 = 0; Index2 < COLUMN_IN_HEX_DUMP; Index2++) {
        message = CatSPrintClean(message, L"%c", AsciiBuffer[Index2]);
        if (Index2 == COLUMN_IN_HEX_DUMP / 2 - 1) {
          message = CatSPrintClean(message, L" ");
        }
      }
      PRINTER_SET_KEY_VAL_WIDE_STR_FORMAT(gpPrinterCtxLbaCommon, gpPathLba, messageId, message);
      FREE_POOL_SAFE(message);
      FREE_POOL_SAFE(messageId);
    }
  }

  FREE_POOL_SAFE(message);
  FREE_POOL_SAFE(messageId);
}
