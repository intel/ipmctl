/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PBR_DCPMM_H_
#define _PBR_DCPMM_H_
#include <Types.h>
#include <Pbr.h>
#include <PbrTypes.h>

#define PBR_TMP_DIR               "/tmp/pbr/"
//DCPMM specific PBR defines
#define PBR_RECORD_TYPE_SMBIOS            0x1
#define PBR_RECORD_TYPE_NFIT              0x2
#define PBR_RECORD_TYPE_PCAT              0x3
#define PBR_RECORD_TYPE_PMTT              0x4
#define PBR_RECORD_TYPE_PASSTHRU          0x5

#define PBR_DCPMM_CLI_SIG                 SIGNATURE_32('P', 'B', 'D', 'C')
#define PBR_PASS_THRU_SIG                 SIGNATURE_32('P', 'B', 'C', 'L')
#define PBR_UEFI_DRIVER_INIT_SIG          SIGNATURE_32('P', 'B', 'D', 'I')
#define PBR_SMBIOS_SIG                    SIGNATURE_32('P', 'B', 'S', 'M')
#define PBR_NFIT_SIG                      SIGNATURE_32('P', 'B', 'N', 'F')
#define PBR_PCAT_SIG                      SIGNATURE_32('P', 'B', 'P', 'C')
#define PBR_PMTT_SIG                      SIGNATURE_32('P', 'B', 'P', 'M')

#define PBR_FILE_DESCRIPTION              "Intel(R) Optane(TM) DC Persistent Memory Recording File."
#define PBR_DRIVER_INIT_TAG_DESCRIPTION   L"driver: initialization"

/**passthru data struct that is used within the passthru partition**/
typedef struct _PbrPassThruReq {
  UINT64  TotalMilliseconds;                                  //!< Duration of the PT request
  UINT32  DimmId;                                             //!< Target DIMM ID
  UINT8   Opcode;                                             //!< FIS Opcode
  UINT8   SubOpcode;                                          //!< FIS SubOpcode 
  UINT32  InputPayloadSize;                                   //!< FIS Input payload size (small payload)
  UINT32  InputLargePayloadSize;                              //!< FIS Input large payload size
  UINT8   Input[];                                            //!< Payload
}PbrPassThruReq;

/**passthru data struct that is used within the passthru partition**/
typedef struct _PbrPassThruResp {
  UINT64      TotalMilliseconds;                              //!< Duration of the PT response
  EFI_STATUS  PassthruReturnCode;                             //!< Return value from the PT adapter layer
  UINT32      DimmId;                                         //!< Target DIMM ID
  UINT32      OutputPayloadSize;                              //!< FIS Output payload size (small payload)
  UINT32      OutputLargePayloadSize;                         //!< FIS Output large payload size
  UINT8       Status;                                         //!< FIS Status
  UINT8       Output[];                                       //!< Payload
}PbrPassThruResp;

/**smbios data struct that is used within the smbios partition**/
typedef struct _PbrSmbiosTableRecord
{
  UINT32  Size;                                               //!< Size of the smbios table
  UINT8   Minor;                                              //!< Minor version of table
  UINT8   Major;                                              //!< Major version of table
  UINT8   Table[];                                            //!< SMBIOS table(s)
}PbrSmbiosTableRecord;

/**
  Return the current FW_CMD from the playback buffer

  @param[in] pContext: Pbr context
  @param[in] pCmd: current FW_CMD from the playback buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrGetPassThruRecord(
  IN    PbrContext *pContext,
  OUT   FW_CMD *pCmd,
  OUT   EFI_STATUS *pPassThruRc
);

/**
  Record a FW_CMD into the recording buffer

  @param[in] pContext: Pbr context
  @param[in] pCmd: current FW_CMD from the playback buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrSetPassThruRecord(
  IN    PbrContext *pContext,
  OUT   FW_CMD *pCmd,
  EFI_STATUS PassthruReturnCode
);


/**
  Return the current table from the playback buffer

  @param[in] pContext: Pbr context
  @param[in] TableType: 1-smbios, 2-nfit, 3-pcat, 4-pmtt

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrGetTableRecord(
  IN    PbrContext *pContext,
  IN    UINT32 TableType,
  OUT   VOID **ppTable,
  OUT   UINT32 *pTableSize
);

/**
  Record a table into the recording buffer

  @param[in] pContext: Pbr context
  @param[in] TableType: 1-smbios, 2-nfit, 3-pcat, 4-pmtt

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrSetTableRecord(
  IN    PbrContext *pContext,
  IN    UINT32 TableType,
  IN    VOID *pTable,
  IN    UINT32 TableSize
);

/**
  Return the current FW_CMD from the playback buffer

  @param[in] pContext: Pbr context
  @param[in] pCmd: current FW_CMD from the playback buffer
  
  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrGetPassThruRecord(
  IN    PbrContext *pContext,
  OUT   FW_CMD *pCmd,
  OUT   EFI_STATUS *pPassThruRc
);

/**
  Record a FW_CMD into the recording buffer

  @param[in] pContext: Pbr context
  @param[in] pCmd: current FW_CMD from the playback buffer

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrSetPassThruRecord(
  IN    PbrContext *pContext,
  OUT   FW_CMD *pCmd,
  EFI_STATUS PassthruReturnCode
);


/**
  Return the current table from the playback buffer

  @param[in] pContext: Pbr context
  @param[in] TableType: 1-smbios, 2-nfit, 3-pcat, 4-pmtt

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrGetTableRecord(
  IN    PbrContext *pContext,
  IN    UINT32 TableType,
  OUT   VOID **ppTable,
  OUT   UINT32 *pTableSize
);

/**
  Record a table into the recording buffer

  @param[in] pContext: Pbr context
  @param[in] TableType: 1-smbios, 2-nfit, 3-pcat, 4-pmtt

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
**/
EFI_STATUS
PbrSetTableRecord(
  IN    PbrContext *pContext,
  IN    UINT32 TableType,
  IN    VOID *pTable,
  IN    UINT32 TableSize
);

#endif //_PBR_DCPMM_H_