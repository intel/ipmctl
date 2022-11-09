/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OS_EFI_API_H_
#define	OS_EFI_API_H_

#include <Uefi.h>
#include <stdio.h>
#include <Dimm.h>
#include <UefiBaseType.h>
#include <FwUtility.h>
#include <SmbiosUtility.h>
#include <time.h>

typedef enum {
  RtSmbios = 1,
  RtAcpiNfit = 2,
  RtAcpiPcat = 3,
  RtAcpiPmtt = 4,
  RtPassThru = 5,
} RecordType;

typedef struct _pass_thru_record_req
{
  UINT64 TotalMilliseconds;
  UINT32 DimmId;
  UINT8 Opcode;
  UINT8 SubOpcode;
  UINT32 InputPayloadSize;
  UINT32 InputLargePayloadSize;
  UINT8 Input[];
}pass_thru_record_req;


typedef struct _pass_thru_record_resp
{
  UINT64 TotalMilliseconds;
  EFI_STATUS PassthruReturnCode;
  UINT32 DimmId;
  UINT32 OutputPayloadSize;
  UINT32 OutputLargePayloadSize;
  UINT8 Status;
  UINT8 Output[];
}pass_thru_record_resp;

EFI_STATUS ConvertAsciiStrToUnicode(const CHAR8 * AsciiStr, CHAR16 * UnicodeStr, UINTN UnicodeStrMaxLength);

/**
Gets the current timestamp in terms of milliseconds
**/
UINT64 GetCurrentMilliseconds();

VOID
EFIAPI
GetVendorDriverVersion(CHAR16 * pVersion, UINTN VersionStrSize);

/**
Saves a copy of a table to a file
@param[in] type - the type of recording entry
@param[in] destFile - the file to save the passed table to
@param[in] table - the table to save

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
save_table_to_file(
  RecordType type,
  char* destFile,
  EFI_ACPI_DESCRIPTION_HEADER *table
);

/**
Loads a copy of a table from a file

@param[in] sourceFile - the file to save the passed table to
@param[out] size - the size of the table loaded
@param[out] table - EFI_ACPI_DESCRIPTION_HEADER the table

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
load_table_from_file(
  RecordType type,
  char* sourceFile,
  EFI_ACPI_DESCRIPTION_HEADER ** table
);

/**
Obtains a copy of the NFIT table

@param[out] bufferSize the size of the returned buffer
@param[out] table - EFI_ACPI_DESCRIPTION_HEADER the table

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
get_nfit_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER** pTable,
  OUT UINT32 *tablesize
);

/**
Obtains a copy of the PCAT table

@param[out] bufferSize the size of the returned buffer
@param[out] table - EFI_ACPI_DESCRIPTION_HEADER the table

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
get_pcat_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
);

/**
Obtains a copy of the PMTT table

@param[out] bufferSize the size of the returned buffer
@param[out] table - EFI_ACPI_DESCRIPTION_HEADER the table

@retval EFI_SUCCESS  The count was returned properly
@retval Other errors failure of io
**/
EFI_STATUS
get_pmtt_table(
  OUT EFI_ACPI_DESCRIPTION_HEADER ** table,
  OUT UINT32 *tablesize
);

/**
Obtains a copy of the SMBIOS table

@retval NVM_SUCCESS or error code
**/
UINT32
get_smbios_table(
);

/**

provides os-specific passthru functionality which also applies
recording/playback as specified

@param[in]  pDimm    pointer to current Dimm
@param[in, out]  pCmd    pointer to command data
@param[in]  Timeout    the command timeout
**/
EFI_STATUS
passthru_os(
  IN     struct _DIMM *pDimm,
  IN OUT NVM_FW_CMD *pCmd,
  IN     long Timeout
);

/**
provides playback functionality

@param[in]  pDimm    pointer to current Dimm
**/
EFI_STATUS
passthru_playback(
  IN OUT NVM_FW_CMD *pCmd
);

/**
provides the start of record functionality
(prepares the files to record the transactions)

@param[in]  pDimm    pointer to current Dimm
**/

EFI_STATUS
passthru_record_setup(
  FILE **f_passthru_ptr,
  IN OUT NVM_FW_CMD *pCmd
);


/**
completes recording functionality started in passthru_record_setup

@param[in]  file    pointer to the file
@param[in]  pDimm    pointer to current Dimm
@param[in]  DimmID    the dimm id in context
**/
EFI_STATUS
passthru_record_finalize(
  FILE * file,
  IN OUT NVM_FW_CMD *pCmd,
  UINT32 DimmID,
  EFI_STATUS PassthruReturnCode
);

/**
initializes a recording file

@param[in]  recording_file_path    path to file
**/
EFI_STATUS init_record_file(
  char * recording_file_path
);

/**
Produces a Null-terminated Unicode string in an output buffer based on a Null-terminated
Unicode format string and variable argument list.

Produces a Null-terminated Unicode string in the output buffer specified by StartOfBuffer
and BufferSize.
The Unicode string is produced by parsing the format string specified by FormatString.
Arguments are pulled from the variable argument list based on the contents of the format string.
The number of Unicode characters in the produced output buffer is returned, not including
the Null-terminator.
If BufferSize is 0 or 1, then no output buffer is produced and 0 is returned.

If BufferSize > 1 and StartOfBuffer is NULL, then ASSERT().
If BufferSize > 1 and StartOfBuffer is not aligned on a 16-bit boundary, then ASSERT().
If BufferSize > 1 and FormatString is NULL, then ASSERT().
If BufferSize > 1 and FormatString is not aligned on a 16-bit boundary, then ASSERT().
If PcdMaximumUnicodeStringLength is not zero, and FormatString contains more than
PcdMaximumUnicodeStringLength Unicode characters not including the Null-terminator, then
ASSERT().
If PcdMaximumUnicodeStringLength is not zero, and produced Null-terminated Unicode string
contains more than PcdMaximumUnicodeStringLength Unicode characters not including the
Null-terminator, then ASSERT().

@param  StartOfBuffer   A pointer to the output buffer for the produced Null-terminated
Unicode string.
@param  BufferSize      The size, in bytes, of the output buffer specified by StartOfBuffer.
@param  FormatString    A null-terminated Unicode format string.
@param  ...             The variable argument list whose contents are accessed based on the
format string specified by FormatString.

@return The number of Unicode characters in the produced output buffer, not including the
Null-terminator.

**/
UINTN
EFIAPI
UnicodeSPrint(
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  ...
);

/**
  Function returns value of the first argument form the VA_LIST casted as 32bit
  unsigned int

  The function is used by the event logger to get the DO_NOT_PARSE_ARGS magic number

  @param[in]  args    VA_LIST arguments
**/
UINT32
get_first_arg_from_va_list(VA_LIST args);


#endif //OS_EFI_API_H_
