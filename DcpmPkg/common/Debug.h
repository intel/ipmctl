/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/PciLib.h>
#include "Utility.h"

#ifdef OS_BUILD
#include <os_common.h>
#endif

#define G_FN_NAME_SIZE 1024
extern EFI_BOOT_SERVICES *gBS;
extern CHAR16 gFnName[G_FN_NAME_SIZE];
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFUNCTION__ \
  ((AsciiStrToUnicodeStrS(__FUNCTION__, gFnName, G_FN_NAME_SIZE) == RETURN_SUCCESS) ? gFnName : L"" )

#ifdef OS_BUILD
extern UINTN EFIAPI PrintNoBuffer(CHAR16* fmt, ...);
#endif
#ifndef OS_BUILD
#define NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, Format, ...) \
   Print(Format,  ## __VA_ARGS__)
#else
#define NVDIMM_BUFFER_CONTROLLED_MSG(Buffered, Format, ...) \
  do \
  { \
     if (!Buffered) \
        PrintNoBuffer(Format,  ## __VA_ARGS__); \
     else Print(Format,  ## __VA_ARGS__); \
  } while (0)
#endif

#ifdef _MSC_VER
#define SUB_DIR_CHAR '\\'
#else // MSVC
#define SUB_DIR_CHAR '/'
#endif // MSVC
#ifdef OS_BUILD
static INLINE CHAR8 *FileFromPath(CHAR8 *path)
{
    int i = 0;
    int index = 0;
    while (path[i] != '\0')
    {
        if (path[i] == SUB_DIR_CHAR)
        {
            index = i;
        }
        i++;
    }
    return path + index + 1;
}
#else // OS_BUILD
static INLINE CHAR16 *FileFromPath(CHAR16 *path)
{
  int i = 0;
  int index = 0;
  while (path[i] != L'\0')
  {
    if (path[i] == SUB_DIR_CHAR)
    {
      index = i;
    }
    i++;
  }
  return path + index + 1;
}
#endif // OS_BUILD
#define __WFILE__ WIDEN(__FILE__)

#ifndef MDEPKG_NDEBUG

#define SR_BIOS_SERIAL_DEBUG_CSR_OFF  0xb8
#define SR_BIOS_SERIAL_DEBUG_CSR_BUS  0x0
#define SR_BIOS_SERIAL_DEBUG_CSR_DEV  0x8
#define SR_BIOS_SERIAL_DEBUG_CSR_FUN  0x2

#ifdef OS_BUILD
//FIXME: Added for GCC build on linux
#ifdef DEBUG_BUILD
#if defined(_MSC_VER) || defined(__GNUC__)
#define NVDIMM_ENTRY() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Entering %s::%s()\n", \
           FileFromPath(__FILE__), __FUNCTION__)
#define NVDIMM_EXIT() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s()\n", \
           FileFromPath(__FILE__), __FUNCTION__)
#define NVDIMM_EXIT_I(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__FILE__), __FUNCTION__, rc)
#define NVDIMM_EXIT_I64(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__FILE__), __FUNCTION__, rc)
#define NVDIMM_EXIT_CHECK_I64(rc) \
if(rc) { \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__FILE__), __FUNCTION__, rc); \
}
#else
#define NVDIMM_ENTRY() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Entering %s::%s()\n", \
  FileFromPath(__FILE__), __FUNCTION__); \
  RegisterStackTrace((FileFromPath(__FILE__)), (__FUNCTION__))
#define NVDIMM_EXIT() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s()\n", \
  FileFromPath(__FILE__), __FUNCTION__); \
  PopStackTrace()
#define NVDIMM_EXIT_I(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
  FileFromPath(__FILE__), __FUNCTION__, rc); \
  PopStackTrace()
#define NVDIMM_EXIT_I64(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
  FileFromPath(__FILE__), __FUNCTION__, rc); \
  PopStackTrace()
#define NVDIMM_EXIT_CHECK_I64(rc) \
if(rc) { \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__FILE__), __FUNCTION__, rc); \
} \
  PopStackTrace()
#endif
#else // DEBUG_BUILD
#define NVDIMM_ENTRY()
#define NVDIMM_EXIT()
#define NVDIMM_EXIT_I(rc)
#define NVDIMM_EXIT_I64(rc)
#define NVDIMM_EXIT_CHECK_I64(rc)
#endif // DEBUG_BUILD

/**
Error level messages are messages that tell the user what hapened in the case
of a critical failure.  They should be general enough for the end user and
only show if the CLI command fails (not all failures have these debug messages).
These are usually something the user can fix or the message they will get
just before the system shuts down.

Warning level debug messages are shown when something fails that may or may
not cause issues to the user.  Messages should be understood without knowledge
of the codebase. Something like NFIT table issues, BIOS issues,
no DIMMs, failed to initialize smbus, failed to retrieve pools,
EFI return codes, invalid CLI parameters.

Debug level messages are messages that may be helpful to debug an issue, such
as the value of variables or status, or something specific failed.  These
are more specific to the codebase.

Verbose/info messages typically show where we are in the code, such as method
entry/exit.
**/

#define NVDIMM_VERB(fmt, ...)  \
  OS_NVDIMM_VERB(fmt, ## __VA_ARGS__)
/*
  DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:%s::%s:%d: " fmt "\n", \
    FileFromPath(__FILE__), __FUNCTION__, __LINE__, ## __VA_ARGS__)*/
#define NVDIMM_DBG(fmt, ...)  \
  OS_NVDIMM_DBG(fmt, ## __VA_ARGS__)

#define NVDIMM_DBG_CLEAN(fmt, ...)  \
  OS_NVDIMM_DBG_CLEAN(fmt, ## __VA_ARGS__)

#define NVDIMM_WARN(fmt, ...) \
  OS_NVDIMM_WARN(fmt, ## __VA_ARGS__)

#define NVDIMM_ERR(fmt, ...)  \
  OS_NVDIMM_ERR(fmt, ## __VA_ARGS__)

#define NVDIMM_ERR_W(fmt, ...)
/*\
DebugPrint(EFI_D_ERROR, "NVDIMM-ERR:%s::%s:%d: " fmt "\n", \
FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)*/

#define NVDIMM_CRIT(fmt, ...) \
  OS_NVDIMM_CRIT(fmt, ## __VA_ARGS__)

#define NVDIMM_HEXDUMP(data, size) \
    HexDebugPrint(data, size)
#else // OS_BUILD
//FIXME: Added for GCC build on linux
#if defined(_MSC_VER) || defined(__GNUC__)
#define NVDIMM_ENTRY() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Entering %s::%s()\n", \
           FileFromPath(__WFILE__), __WFUNCTION__)
#define NVDIMM_EXIT() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s()\n", \
           FileFromPath(__WFILE__), __WFUNCTION__)
#define NVDIMM_EXIT_I(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__WFILE__), __WFUNCTION__, rc)
#define NVDIMM_EXIT_I64(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): " FORMAT_EFI_STATUS "\n", \
           FileFromPath(__WFILE__), __WFUNCTION__, rc)
#define NVDIMM_EXIT_CHECK_I64(rc) \
if(rc) { \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__WFILE__), __WFUNCTION__, rc); \
}
#else
#define NVDIMM_ENTRY() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Entering %s::%s()\n", \
  FileFromPath(__WFILE__), __WFUNCTION__); \
  RegisterStackTrace((FileFromPath(__WFILE__)), (__WFUNCTION__))
#define NVDIMM_EXIT() \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s()\n", \
  FileFromPath(__WFILE__), __WFUNCTION__); \
  PopStackTrace()
#define NVDIMM_EXIT_I(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
  FileFromPath(__WFILE__), __WFUNCTION__, rc); \
  PopStackTrace()
#define NVDIMM_EXIT_I64(rc) \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): " FORMAT_EFI_STATUS "\n", \
  FileFromPath(__WFILE__), __WFUNCTION__, rc); \
  PopStackTrace()
#define NVDIMM_EXIT_CHECK_I64(rc) \
if(rc) { \
DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:Exiting %s::%s(): 0x%x\n", \
           FileFromPath(__WFILE__), __WFUNCTION__, rc); \
}
  PopStackTrace()
#endif


/**
  Error level messages are messages that tell the user what hapened in the case
  of a critical failure.  They should be general enough for the end user and
  only show if the CLI command fails (not all failures have these debug messages).
  These are usually something the user can fix or the message they will get
  just before the system shuts down.

  Warning level debug messages are shown when something fails that may or may
  not cause issues to the user.  Messages should be understood without knowledge
  of the codebase. Something like NFIT table issues, BIOS issues,
  no DIMMs, failed to initialize smbus, failed to retrieve pools,
  EFI return codes, invalid CLI parameters.

  Debug level messages are messages that may be helpful to debug an issue, such
  as the value of variables or status, or something specific failed.  These
  are more specific to the codebase.

  Verbose/info messages typically show where we are in the code, such as method
  entry/exit.
 **/

#define NVDIMM_VERB(fmt, ...)  \
  DebugPrint(EFI_D_VERBOSE, "NVDIMM-VERB:%s::%s:%d: " fmt "\n", \
    FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)
#define NVDIMM_DBG(fmt, ...)  \
  DebugPrint(EFI_D_INFO, "NVDIMM-DBG:%s::%s:%d: " fmt "\n", \
    FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)
#define NVDIMM_DBG_CLEAN(fmt, ...)  \
  DebugPrint(EFI_D_INFO, fmt, ## __VA_ARGS__)
#define NVDIMM_WARN(fmt, ...) \
  DebugPrint(EFI_D_WARN, "NVDIMM-WARN:%s::%s:%d: " fmt "\n", \
    FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)
#define NVDIMM_ERR(fmt, ...)  \
  DebugPrint(EFI_D_ERROR, "NVDIMM-ERR:%s::%s:%d: " fmt "\n", \
    FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)
#define NVDIMM_ERR_W(fmt, ...)
/*\
  DebugPrint(EFI_D_ERROR, "NVDIMM-ERR:%s::%s:%d: " fmt "\n", \
    FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)*/
#define NVDIMM_CRIT(fmt, ...) \
  DebugPrint(EFI_D_ERROR, "NVDIMM-ERR:%s::%s:%d: " fmt "\n", \
    FileFromPath(__WFILE__), __WFUNCTION__, __LINE__, ## __VA_ARGS__)
#define NVDIMM_HEXDUMP(data, size) \
    HexDebugPrint(data, size)
#endif //OS_BUILD

#define CREATE_COMPARE_CODE(MajorCode, MinorCode) (((MajorCode) << 8 | (MinorCode)) << 16)
STATIC
INLINE
VOID
OutputCheckpoint(
  IN    UINT8 MajorCode,
  IN    UINT8 MinorCode
  )
{
  CONST UINT32 CompareCode = CREATE_COMPARE_CODE(MajorCode, MinorCode);
  CONST UINT32 ScratchPadRegAddr = PCI_LIB_ADDRESS(SR_BIOS_SERIAL_DEBUG_CSR_BUS,
      SR_BIOS_SERIAL_DEBUG_CSR_DEV, SR_BIOS_SERIAL_DEBUG_CSR_FUN, SR_BIOS_SERIAL_DEBUG_CSR_OFF);
  UINT32 ScratchPadReg = 0;

  ScratchPadReg = PciRead32(ScratchPadRegAddr);
  do {
    NVDIMM_DBG("Match checkpoint = %x, MajorCode = %x, MinorCode = %x", ScratchPadReg, MajorCode, MinorCode);
    gBS->Stall(1000);
    ScratchPadReg = PciRead32(ScratchPadRegAddr);
  } while ((ScratchPadReg & 0xFFFF0000) == CompareCode);
}

#else

#define NVDIMM_ENTRY()
#define NVDIMM_EXIT()
#define NVDIMM_EXIT_I(rc)
#define NVDIMM_EXIT_I64(rc)
#define NVDIMM_EXIT_CHECK_I64(rc)
#define NVDIMM_VERB(fmt, ...)
#define NVDIMM_DBG(fmt, ...)
#define NVDIMM_DBG_CLEAN(fmt, ...)
#define NVDIMM_WARN(fmt, ...)
#define NVDIMM_ERR(fmt, ...)
#define NVDIMM_CRIT(fmt, ...)
#define NVDIMM_HEXDUMP(data, size)

STATIC
INLINE
VOID
OutputCheckpoint(
  IN    UINT8 MajorCode,
  IN    UINT8 MinorCode
  )
{
  /** In release this function will be cut by compiler **/
}
#endif /** MDEPKG_NDEBUG **/

/**
  Compile time assert:
  used for ensuring that dependencies between constant symbols values
  (enums, defines) meet certain condition.
  If assertion fail file won't compile and will throw gcc error:
  error: size of array __C_ASSERT__ is negative
**/
#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]

#endif /** C_ASSERT **/

#endif /** _DEBUG_H_ **/
