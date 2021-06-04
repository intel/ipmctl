#include <stdlib.h>
#include <stdio.h>
#include <Base.h>
#include <Utility.h>
#include <assert.h>
#include <PrintLib.h>
#include <Common.h>
#include <PbrTypes.h>
#include <PbrDcpmm.h>
#include <os_efi_api.h>
#include <NvmDimmConfigInt.h>
#include <ShellParameters.h>
#include <NvmDimmDriver.h>
#include <ProcessorBind.h>
#ifdef _MSC_VER
#include <io.h>
#include <conio.h>
#include <time.h>
#include <string.h>
#else
#include <unistd.h>
#define _read read
#define _getch getchar
#endif
#include <fcntl.h>


#ifdef OS_BUILD
#include <os_efi_preferences.h>
#include <os_str.h>
#endif

extern NVMDIMMDRIVER_DATA *gNvmDimmData;

extern int get_vendor_driver_revision(char * version_str, const int str_len);

extern BOOLEAN is_verbose_debug_print_enabled();

#define INI_PREFERENCES_LOG_LEVEL L"DBG_LOG_LEVEL"
#define INI_PREFERENCES_LOG_STDOUT_ENABLED L"DBG_LOG_STDOUT_ENABLED"

#ifdef NDEBUG
void (*rel_assert) (void) = NULL;
#endif // NDEBUG

/*
* Debug logger context structure.
*/


UINT8 *gSmbiosTable = NULL;
size_t gSmbiosTableSize = 0;
UINT8 gSmbiosMinorVersion = 0;
UINT8 gSmbiosMajorVersion = 0;

#define SMBIOS_SIZE     0x2800
typedef struct _smbios_table_recording
{
  size_t size;
  UINT8 minor;
  UINT8 major;
  UINT8 table[];
}smbios_table_recording;

struct debug_logger_config
{
  UINT8 initialized : 1;
  CHAR8 stdout_enabled;
  CHAR8 level;
};
enum
{
  LOGGER_OFF = 0,
  LOG_ERROR = 1,
  LOG_WARNING = 2,
  LOG_INFO = 3,
  LOG_VERBOSE = 4,
} LOG_LEVEL_LIST;

static struct debug_logger_config g_log_config = { 0 };



/*
* Function get the ini configuration only on the first call
*/
static void get_logger_config(struct debug_logger_config *p_log_config)
{
  EFI_STATUS efi_status;
  EFI_GUID guid = { 0 };
  UINTN size;

  if (p_log_config->initialized)
    return;

  size = sizeof(p_log_config->level);
  efi_status = GET_VARIABLE(INI_PREFERENCES_LOG_LEVEL, guid, &size, &p_log_config->level);
  if (EFI_SUCCESS != efi_status)
    return;
  size = sizeof(p_log_config->stdout_enabled);
  efi_status = GET_VARIABLE(INI_PREFERENCES_LOG_STDOUT_ENABLED, guid, &size, &p_log_config->stdout_enabled);
  if (EFI_SUCCESS != efi_status)
    return;
  if (is_verbose_debug_print_enabled())
  {
    p_log_config->stdout_enabled = TRUE;
    p_log_config->level = LOG_VERBOSE;
  }

  p_log_config->initialized = TRUE;
}



/*
* Function enables disables the debug logger
*/
int
EFIAPI
DebugLoggerEnable(
  IN  BOOLEAN EnableDbgLogger
)
{
  if (FALSE == g_log_config.initialized)
  {
    return -1;
  }

  if (EnableDbgLogger)
  {
    if (FALSE == g_log_config.stdout_enabled)
      g_log_config.stdout_enabled = TRUE;
    if (LOGGER_OFF == g_log_config.level)
      g_log_config.level = LOG_WARNING;
  }
  else
  {
    if (TRUE == g_log_config.stdout_enabled)
      g_log_config.stdout_enabled = FALSE;
  }

  return 0;
}

/*
* Function returns the current state of the debug logger
*/
BOOLEAN
EFIAPI
IsDebugLoggerEnabled()
{
  if (FALSE == g_log_config.initialized) {
    return FALSE;
  }
  if ((LOGGER_OFF != g_log_config.level) && (TRUE == g_log_config.stdout_enabled)) {
    return TRUE;
  }
  return FALSE;
}

EFI_STATUS ConvertAsciiStrToUnicode(const CHAR8 * AsciiStr, CHAR16 * UnicodeStr, UINTN UnicodeStrMaxLength) {
  EFI_STATUS ReturnCode;
  if ((NULL == AsciiStr) || (NULL == UnicodeStr)) {
    return EFI_INVALID_PARAMETER;
  }
  ReturnCode = AsciiStrToUnicodeStrS(AsciiStr, UnicodeStr, UnicodeStrMaxLength);
  if (ReturnCode != EFI_SUCCESS) {
    Print(L"Failed to convert Ascii string to Unicode string. Return code = %d.\n", ReturnCode);
  }
  return ReturnCode;
}

/*
* Sends system event entry to standard output.
*/
static void write_system_event_to_stdout(const char* source, const char* message)
{
  RETURN_STATUS ReturnCode = EFI_SUCCESS;
  NVM_EVENT_MSG ascii_event_message = { 0 };
  CHAR16 w_event_message[sizeof(ascii_event_message)] = { 0 };

  // Prepare string
  os_strcat(ascii_event_message, sizeof(ascii_event_message), source);
  os_strcat(ascii_event_message, sizeof(ascii_event_message), " ");
  os_strcat(ascii_event_message, sizeof(ascii_event_message), message);
  os_strcat(ascii_event_message, sizeof(ascii_event_message), "\n");
  // Convert to the unicode  --  length of array is sizeof(ascii_event_message)
  CHECK_RESULT(ConvertAsciiStrToUnicode(ascii_event_message, w_event_message, sizeof(ascii_event_message)), Finish);

  // Send it to standard output
  Print(FORMAT_STR, w_event_message);

Finish:
  return;
}

extern EFI_SHELL_PARAMETERS_PROTOCOL gOsShellParametersProtocol;

/**
Prints a formatted Unicode string to the console output device specified by
ConOut defined in the EFI_SYSTEM_TABLE.

This function prints a formatted Unicode string to the console output device
specified by ConOut in EFI_SYSTEM_TABLE and returns the number of Unicode
characters that printed to ConOut.  If the length of the formatted Unicode
string is greater than PcdUefiLibMaxPrintBufferSize, then only the first
PcdUefiLibMaxPrintBufferSize characters are sent to ConOut.
If Format is NULL, then ASSERT().
If Format is not aligned on a 16-bit boundary, then ASSERT().
If gST->ConOut is NULL, then ASSERT().

@param Format   A null-terminated Unicode format string.
@param ...      The variable argument list whose contents are accessed based
on the format string specified by Format.

@return Number of Unicode characters printed to ConOut.

**/
UINTN
EFIAPI
Print(
  IN CONST CHAR16  *Format,
  ...
)
{
  va_list argptr;
  va_start(argptr, Format);
  vfwprintf(gOsShellParametersProtocol.StdOut, Format, argptr);
  va_end(argptr);
  fflush(gOsShellParametersProtocol.StdOut);
  return 0;
}

UINTN
EFIAPI
PrintNoBuffer(CHAR16* Format, ...)
{
  va_list argptr;
  va_start(argptr, Format);
  vfwprintf(stdout, Format, argptr);
  va_end(argptr);
  fflush(stdout);
  return 0;
}

/**
Prints a debug message to the debug output device if the specified error level is enabled.

If any bit in ErrorLevel is also set in DebugPrintErrorLevelLib function
GetDebugPrintErrorLevel (), then print the message specified by Format and the
associated variable argument list to the debug output device.

If Format is NULL, then ASSERT().

@param  ErrorLevel  The error level of the debug message.
@param  Format      Format string for the debug message to print.
@param  ...         Variable argument list whose contents are accessed
based on the format string specified by Format.

**/
VOID
EFIAPI
DebugPrint(
  IN  UINTN        ErrorLevel,
  IN  CONST CHAR8  *Format,
  ...
)
{
  VA_LIST args;
  NVM_EVENT_MSG event_message;
  UINT32 size = sizeof(event_message);

  if (FALSE == g_log_config.initialized)
  {
    get_logger_config(&g_log_config);
  }

  if (ErrorLevel == OS_DEBUG_CRIT) {
    // Send the debug entry to the logger
    VA_START(args, Format);
    AsciiVSPrint(event_message, size, Format, args);
    VA_END(args);
    write_system_event_to_stdout(NVM_DEBUG_LOGGER_SOURCE, event_message);
#ifdef NDEBUG
    rel_assert ();
#else // NDEBUG
    assert(FALSE);
#endif // NDEBUG
  }
  else if (LOGGER_OFF == g_log_config.level || g_log_config.stdout_enabled == FALSE)
    return;

  if (((LOG_ERROR == g_log_config.level) & (ErrorLevel == OS_DEBUG_ERROR)) ||
    ((LOG_WARNING == g_log_config.level) & ((ErrorLevel == OS_DEBUG_ERROR) || (ErrorLevel == OS_DEBUG_WARN))) ||
    ((LOG_INFO == g_log_config.level) & ((ErrorLevel == OS_DEBUG_ERROR) || (ErrorLevel == OS_DEBUG_WARN) || (ErrorLevel == OS_DEBUG_INFO))) ||
    (LOG_VERBOSE == g_log_config.level))
  {
    // Send the debug entry to the logger
    VA_START(args, Format);
    AsciiVSPrint(event_message, size, Format, args);
    VA_END(args);
    write_system_event_to_stdout(NVM_DEBUG_LOGGER_SOURCE, event_message);
  }
}

#define MAX_PROMT_INPUT_SZ 1024
#define RETURN_KEY  0xD
#define LINE_FEED 0xA

/**
Prompted input request

@param[in] pPrompt - information about expected input
@param[in] ShowInput - Show characters written by user
@param[in] OnlyAlphanumeric - Allow only for alphanumeric characters
@param[out] ppReturnValue - is a pointer to a pointer to the 16-bit character string
that will contain the return value

@retval - Appropriate CLI return code
**/
EFI_STATUS
PromptedInput(
  IN     CHAR16 *pPrompt,
  IN     BOOLEAN ShowInput,
  IN     BOOLEAN OnlyAlphanumeric,
  OUT CHAR16 **ppReturnValue
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  int PromptIndex;
  char ThrowAway;
  VOID * ptr;
  BOOLEAN NoReturn = TRUE;

  NVDIMM_ENTRY();

  if (pPrompt == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  Print(L"%ls", pPrompt);
  char buff[MAX_PROMT_INPUT_SZ];
  memset(buff, 0, MAX_PROMT_INPUT_SZ);

  for (PromptIndex = 0; PromptIndex < (MAX_PROMT_INPUT_SZ - 1); ++PromptIndex) {
    buff[PromptIndex] = _getch();
    if (RETURN_KEY == buff[PromptIndex] || LINE_FEED == buff[PromptIndex]) {
      //terminate string, advance index to indicate size
      buff[PromptIndex++] = '\0';
      NoReturn = FALSE;
      break;
    }
  }

  *ppReturnValue = NULL;
  while (NoReturn) {
    //we ran out of buffer before user pressed Enter
    //consume stdin until Enter
    ThrowAway = _getch();

    if (RETURN_KEY == ThrowAway || LINE_FEED == ThrowAway) {
      ReturnCode = EFI_BUFFER_TOO_SMALL;
      goto Finish;
    }
  }

  ptr = AllocateZeroPool(PromptIndex * (sizeof(CHAR16)));
  if (NULL == ptr) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }

  ReturnCode = ConvertAsciiStrToUnicode(buff, ptr, PromptIndex);
  if (!EFI_ERROR(ReturnCode)) {
    *ppReturnValue = ptr;
  }

Finish:
  Print(L"\n");
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

/**
Display "yes/no" question and retrieve reply using prompt mechanism

@param[out] pConfirmation Confirmation from prompt

@retval EFI_INVALID_PARAMETER One or more parameters are invalid
@retval EFI_SUCCESS All Ok
**/
EFI_STATUS
PromptYesNo(
  OUT BOOLEAN *pConfirmation
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *pPromptReply = NULL;
  BOOLEAN ValidInput = FALSE;
  char buf[10];
  int readSize = 0;

  NVDIMM_ENTRY();

  if (pConfirmation == NULL) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  PrintNoBuffer(L"%ls", PROMPT_CONTINUE_QUESTION);
  if (0 >= (readSize = _read(0, buf, sizeof(buf))))
  {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  ValidInput = readSize == 2 &&
    (buf[0] == 'y' || buf[0] == 'n');
  if (!ValidInput) {
    ReturnCode = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  if (buf[0] == 'y') {
    *pConfirmation = TRUE;
  }
  else {
    *pConfirmation = FALSE;
  }

  ReturnCode = EFI_SUCCESS;

Finish:
  FREE_POOL_SAFE(pPromptReply);
  NVDIMM_EXIT_I64(ReturnCode);
  return ReturnCode;
}

EFI_STATUS
EFIAPI
DefaultPassThru(
  IN     struct _DIMM *pDimm,
  IN OUT NVM_FW_CMD *pCmd,
  IN     UINT64 Timeout
)
{
  EFI_STATUS Rc = EFI_SUCCESS;
  EFI_STATUS PbrRc = EFI_SUCCESS;
  UINT32 DimmID;
  PbrContext *pContext = PBR_CTX();

  if (!pDimm || !pCmd)
    return EFI_INVALID_PARAMETER;

  if (PBR_PLAYBACK_MODE == PBR_GET_MODE(pContext))
  {
    Rc = PbrGetPassThruRecord(pContext, pCmd, &PbrRc);
    if (EFI_SUCCESS == Rc) {
      Rc = PbrRc;
    }
    return Rc;
  }

  DimmID = pCmd->DimmID;
  pCmd->DimmID = pDimm->DeviceHandle.AsUint32;
  Rc = passthru_os(pDimm, pCmd, (long)Timeout);

  if (PBR_RECORD_MODE == PBR_GET_MODE(pContext))
  {
      PbrRc = PbrSetPassThruRecord(pContext, pCmd, Rc);

      // If PBR fails, show error but don't abort
      if (EFI_SUCCESS != PbrRc) {
        NVDIMM_ERR("PBR failed to record transaction. RC: 0x%x", PbrRc);
      }
  }
  pCmd->DimmID = DimmID;

  return Rc;
}


EFI_STATUS
initAcpiTables()
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  EFI_ACPI_DESCRIPTION_HEADER * PtrNfitTable = NULL;
  EFI_ACPI_DESCRIPTION_HEADER * PtrPcatTable = NULL;
  EFI_ACPI_DESCRIPTION_HEADER * PtrPMTTTable = NULL;
  UINT32 failures = 0;
  PbrContext *pContext = PBR_CTX();
  UINT32 Size = 0;

  if (PBR_PLAYBACK_MODE == PBR_GET_MODE(pContext))
  {
    ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_NFIT, (VOID**)&PtrNfitTable, (UINT32*)&Size);
    if (EFI_ERROR(ReturnCode)) {
      Print(L"Failed to record NFIT");
      failures++;
    }

    ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_PCAT, (VOID**)&PtrPcatTable, (UINT32*)&Size);
    if (EFI_ERROR(ReturnCode)) {
      Print(L"Failed to record PCAT");
      failures++;
    }

    ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_PMTT, (VOID**)&PtrPMTTTable, (UINT32*)&Size);
    if (EFI_ERROR(ReturnCode)) {
      Print(L"Failed to record PMTT");
      //failures++; allowed to not be there
    }
  }
  else
  {
    if (EFI_ERROR(get_nfit_table(&PtrNfitTable, &Size)))
    {
      NVDIMM_WARN("Failed to get the NFIT table.\n");
      failures++;
    }

    if (PBR_RECORD_MODE == PBR_GET_MODE(pContext))
    {
      ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_NFIT, PtrNfitTable, Size);
      if (EFI_ERROR(ReturnCode)) {
        Print(L"Failed to record NFIT");
        failures++;
      }
    }

    if (EFI_ERROR(get_pcat_table(&PtrPcatTable, &Size)))
    {
      NVDIMM_WARN("Failed to get the PCAT table.\n");
      failures++;
    }

    if (PBR_RECORD_MODE == PBR_GET_MODE(pContext))
    {
      ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_PCAT, PtrPcatTable, Size);
      if (EFI_ERROR(ReturnCode)) {
        Print(L"Failed to record PCAT");
        failures++;
      }
    }
/*
    if (EFI_ERROR(get_pmtt_table(&PtrPMTTTable, &Size)))
    {
      NVDIMM_WARN("Failed to get the PMTT table.\n");
      //failures++; //table allowed to be empty. Not a failure
    }
*/
    if (PBR_RECORD_MODE == PBR_GET_MODE(pContext))
    {
      ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_PMTT, PtrPMTTTable, Size);
      if (EFI_ERROR(ReturnCode)) {
        Print(L"Failed to record PMTT");
        //failures++;
      }
    }
  }

  if (failures > 0)
  {
    NVDIMM_WARN("Encountered %d failures.", failures);
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  if (NULL == PtrNfitTable || NULL == PtrPcatTable)
  {
    NVDIMM_WARN("Failed to obtain NFIT or PCAT table.");
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = ParseAcpiTables(PtrNfitTable, PtrPcatTable, PtrPMTTTable,
    &gNvmDimmData->PMEMDev.pFitHead, &gNvmDimmData->PMEMDev.pPcatHead, &gNvmDimmData->PMEMDev.pPmttHead,
    &gNvmDimmData->PMEMDev.IsMemModeAllowedByBios);
  if (EFI_ERROR(ReturnCode))
  {
    NVDIMM_WARN("Failed to parse NFIT or PCAT or PMTT table.");
    goto Finish;
  }

Finish:
  if (PBR_PLAYBACK_MODE != PBR_GET_MODE(pContext)) {
    FREE_POOL_SAFE(PtrNfitTable);
    FREE_POOL_SAFE(PtrPcatTable);
    FREE_POOL_SAFE(PtrPMTTTable);
  }
  return ReturnCode;
}

EFI_STATUS
uninitAcpiTables(
)
{
  FREE_POOL_SAFE(gNvmDimmData->PMEMDev.pFitHead);
  FREE_POOL_SAFE(gNvmDimmData->PMEMDev.pPcatHead);
  return EFI_SUCCESS;
}

EFI_STATUS
GetFirstAndBoundSmBiosStructPointer(
  OUT SMBIOS_STRUCTURE_POINTER *pSmBiosStruct,
  OUT SMBIOS_STRUCTURE_POINTER *pLastSmBiosStruct,
  OUT SMBIOS_VERSION *pSmbiosVersion
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  smbios_table_recording *recording = NULL;
  UINT32 record_size = 0;
  PbrContext *pContext = PBR_CTX();

  if (pSmBiosStruct == NULL || pLastSmBiosStruct == NULL || pSmbiosVersion == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // One time initialization
  if (NULL == gSmbiosTable && PBR_PLAYBACK_MODE != PBR_GET_MODE(pContext))
  {
    get_smbios_table();
  }

  if (PBR_RECORD_MODE == PBR_GET_MODE(pContext))
  {
    recording = malloc(sizeof(smbios_table_recording) + gSmbiosTableSize);
    if (NULL == recording) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }

    recording->major = gSmbiosMajorVersion;
    recording->minor = gSmbiosMinorVersion;
    recording->size = gSmbiosTableSize;
    if (gSmbiosTable) {
      CopyMem(recording->table, gSmbiosTable, gSmbiosTableSize);
    }
    else {
      NVDIMM_ERR("Problems initializing smbios table\n");
    }

    ReturnCode = PbrSetTableRecord(pContext, PBR_RECORD_TYPE_SMBIOS, recording, (UINT32)(sizeof(smbios_table_recording) + gSmbiosTableSize));
    if (EFI_ERROR(ReturnCode)) {
      FREE_POOL_SAFE(recording);
      Print(L"Failed to record SMBIOS2");
      goto Finish;
    }
    FREE_POOL_SAFE(recording);
  }
  else if (PBR_PLAYBACK_MODE == PBR_GET_MODE(pContext) && NULL == gSmbiosTable)
  {
    ReturnCode = PbrGetTableRecord(pContext, PBR_RECORD_TYPE_SMBIOS, (VOID**)&recording, &record_size);
    if (EFI_ERROR(ReturnCode) || record_size == 0) {
      goto Finish;
    }

    if (NULL == gSmbiosTable) {

      if (SMBIOS_SIZE < recording->size || 0 == recording->size)
      {
        //todo: fix error message
        NVDIMM_ERR("Invalid PBR SMBIOS table size - %d.\n", recording->size);
        ReturnCode = EFI_END_OF_FILE;
      }
      else
      {
        gSmbiosTable = calloc(1, recording->size);
        if (NULL == gSmbiosTable)
        {
          NVDIMM_ERR("Unable to alloc for SMBIOS table\n");
          ReturnCode = EFI_END_OF_FILE;
          goto Finish;
        }
        else
        {
          CopyMem(gSmbiosTable, recording->table, recording->size);
        }

        gSmbiosMajorVersion = recording->major;
        gSmbiosMinorVersion = recording->minor;
        gSmbiosTableSize = recording->size;
      }
    }
  }
Finish:
  if (NULL != gSmbiosTable)
  {
    pSmBiosStruct->Raw = (UINT8 *)gSmbiosTable;
    pLastSmBiosStruct->Raw = pSmBiosStruct->Raw + gSmbiosTableSize;
    pSmbiosVersion->Major = gSmbiosMajorVersion;
    pSmbiosVersion->Minor = gSmbiosMinorVersion;
  }
  else
  {
    NVDIMM_ERR("Failed to retrieve smbios table\n");
    ReturnCode = EFI_END_OF_FILE;
  }
  return ReturnCode;
}

VOID
EFIAPI
GetVendorDriverVersion(CHAR16 * pVersion, UINTN VersionStrSize)
{
  char ascii_buffer[100];

  if (0 == get_vendor_driver_revision(ascii_buffer, sizeof(ascii_buffer)))
  {
    ConvertAsciiStrToUnicode(ascii_buffer, pVersion, VersionStrSize);
  }
  else
  {
    UnicodeSPrint(pVersion, VersionStrSize, L"0.0.0.0");
  }
}
