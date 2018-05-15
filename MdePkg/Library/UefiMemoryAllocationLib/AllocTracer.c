/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MDEPKG_NDEBUG
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>

#define MAX_PATH_DEPTH 10
#define BAD_POINTER 0xAFAFAFAFAFAFAFAF

BOOLEAN gMemoryTracingEnabled = FALSE;
LIST_ENTRY *gpPointerList = NULL;
LIST_ENTRY *gpTraceList = NULL;

/**
  Enables RegisterAllocation and Unregisterallocation.
**/
VOID
EnableTracing (
  )
{
  gMemoryTracingEnabled = TRUE;
}

/**
  Disables RegisterAllocation and Unregisterallocation.
**/
VOID
DisableTracing (
  )
{
  gMemoryTracingEnabled = FALSE;
}

/**
  Counts nodes in LIST_ENTRY List
**/
UINT64
NumberOfNodesInList (
  IN     LIST_ENTRY *pList
  )
{
  UINT64 Index = 0;
  LIST_ENTRY *pCurNode = NULL;

  if (pList == NULL) {
    return 0;
  }

  DisableTracing();
  for (pCurNode = GetFirstNode(pList);
      !IsNull(pList, pCurNode);
      pCurNode = GetNextNode(pList, pCurNode)) {
    Index++;
  }
  EnableTracing();
  return Index;
}

/**
  Removes list entries correlated with given pointer.
**/
EFI_STATUS
UnregisterAllocation (
  IN     VOID *pMemory
  )
{
  LIST_ENTRY *pCurNode = NULL;
  LIST_ENTRY *pTempNode = NULL;
  PointerEntry *pPointerListEntry = NULL;

  if (gpPointerList == NULL || IsListEmpty(gpPointerList)) {
    return EFI_NOT_FOUND;
  }

  DisableTracing();
  for (pCurNode = GetFirstNode (gpPointerList);
      !IsNull (gpPointerList, pCurNode);
      pCurNode = pTempNode) {
    pTempNode = GetNextNode(gpPointerList, pCurNode);
    pPointerListEntry = POINTER_ENTRY_FROM_NODE(pCurNode);
    if (pMemory == pPointerListEntry->pPointer ||
        /**
          @todo: Investigate emulator issue with RemoveEntryList
          (this is a workaround and evrything works fine).
        **/
        pPointerListEntry->pPointer == (VOID*)BAD_POINTER) {
      RemoveEntryList(pCurNode);
      gBS->FreePool ((VOID*)pPointerListEntry);
    }
  }
  EnableTracing();
  return EFI_SUCCESS;
}

/**
  Adds list entry containing information about allocated pointer.
  If File/Function path is too long only last 9 entries will be stored.
**/
EFI_STATUS
RegisterAllocation (
  IN     VOID *pMemory
  )
{
  PointerEntry *pPointerListEntry = NULL;
  TraceEntry *pTraceListEntry = NULL;
  LIST_ENTRY *pCurNode = NULL;
  EFI_STATUS Rc = EFI_SUCCESS;
  UINT64 Index = 0;
  UINT64 NodesInList = 0;

  if (gpTraceList == NULL) {
    Rc = EFI_NOT_FOUND;
    goto Finish;
  }

  DisableTracing();
  if (gpPointerList == NULL) {
    Rc = gBS->AllocatePool (EfiBootServicesData, sizeof(LIST_ENTRY), (VOID**)&gpPointerList);
    if (EFI_ERROR(Rc)) {
      goto Finish;
    }
    InitializeListHead(gpPointerList);
  }
  gBS->AllocatePool (EfiBootServicesData, sizeof(PointerEntry), (VOID**)&pPointerListEntry);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }
  NodesInList = NumberOfNodesInList(gpTraceList);
  if (NodesInList < MAX_PATH_DEPTH) {

    StrnCpy (pPointerListEntry->File, L"\0", StrLen(L"\0") + 1);
    StrnCpy (pPointerListEntry->Function, L"\0", StrLen(L"\0") + 1);
  } else {
    StrnCpy (pPointerListEntry->File, L"Path too long...\\\0", StrLen(L"Path too long...\\\0") + 1);
    StrnCpy (pPointerListEntry->Function, L"Path too long...\\\0", StrLen(L"Path too long...\\\0") + 1);
  }

  for (pCurNode = GetFirstNode (gpTraceList);
      !IsNull (gpTraceList, pCurNode);
      pCurNode = GetNextNode(gpTraceList, pCurNode), Index++) {
    if (NodesInList - Index < MAX_PATH_DEPTH) {
      pTraceListEntry = TRACE_ENTRY_FROM_NODE(pCurNode);
      pPointerListEntry->pPointer = pMemory;
      if (StrLen (pPointerListEntry->File) + StrLen (pTraceListEntry->File) < FILE_PATH_MAXLEN) {
        StrnCat (pPointerListEntry->File, pTraceListEntry->File, FILE_NAME_MAXLEN);
      } else {
        StrnCpy (pPointerListEntry->File, pTraceListEntry->File, FILE_NAME_MAXLEN);
      }
      if(StrLen (pPointerListEntry->Function) + StrLen (pTraceListEntry->Function) < FUNCTION_PATH_MAXLEN) {
        StrnCat (pPointerListEntry->Function, pTraceListEntry->Function, FUNCTION_NAME_MAXLEN);
      } else {
        StrnCpy (pPointerListEntry->Function, pTraceListEntry->Function, FUNCTION_NAME_MAXLEN);
      }
    }
  }
  InsertTailList (gpPointerList, &pPointerListEntry->PointerEntryNode);

Finish:
  EnableTracing();
  return Rc;
}

/**
  Removes tail element of gpTraceList which stores current location in program.
  If there is no list returns EFI_NOT_FOUND.
**/
EFI_STATUS
PopStackTrace (
  )
{
  TraceEntry *pTraceListEntry = NULL;

  if (gpTraceList == NULL) {
    return EFI_NOT_FOUND;
  }

  DisableTracing();
  if (!IsNull(gpTraceList, gpTraceList->BackLink)) {
    pTraceListEntry = TRACE_ENTRY_FROM_NODE(gpTraceList->BackLink);
    RemoveEntryList(gpTraceList->BackLink);
    gBS->FreePool ((VOID*)pTraceListEntry);
  }
  EnableTracing();
  return EFI_SUCCESS;
}

/**
  Adds element on top of gpTraceList which stores current location in program.
  If there is no list creates it.
**/
EFI_STATUS
RegisterStackTrace (
  IN     CHAR16 File[],
  IN     CHAR16 Function[]
  )
{
  TraceEntry *pTraceListEntry = NULL;
  EFI_STATUS Rc = EFI_SUCCESS;

  if (File == NULL || Function == NULL) {
    Rc = EFI_INVALID_PARAMETER;
    goto Finish;
  }

  DisableTracing();
  if (gpTraceList == NULL) {
    Rc = gBS->AllocatePool (EfiBootServicesData, sizeof(LIST_ENTRY), (VOID**)&gpTraceList);
    if (EFI_ERROR(Rc)) {
      goto Finish;
    }
    InitializeListHead(gpTraceList);
  }
  Rc = gBS->AllocatePool (EfiBootServicesData, sizeof(TraceEntry), (VOID**)&pTraceListEntry);
  if (EFI_ERROR(Rc)) {
    goto Finish;
  }
  InsertTailList (gpTraceList, &pTraceListEntry->TraceEntryNode);
  StrnCpy (pTraceListEntry->File, File, FILE_NAME_MAXLEN);
  StrnCat (pTraceListEntry->File, L"\\\0", StrLen(L"\\\0") + 1);
  StrnCpy (pTraceListEntry->Function, Function, FUNCTION_NAME_MAXLEN);
  StrnCat (pTraceListEntry->Function, L"\\\0", StrLen(L"\\\0") + 1);

Finish:
  EnableTracing();
  return Rc;
}

/**
  Prints current location in program.
  If there is no list returns EFI_NOT_FOUND.
**/

EFI_STATUS
PrintStackTrace (
  )
{
  TraceEntry *pTraceListEntry = NULL;
  LIST_ENTRY *pCurNode = NULL;

  if (gpTraceList == NULL) {
    return EFI_NOT_FOUND;
  }

  DisableTracing();
  DebugPrint (EFI_D_ERROR, "\nFileTrace:\n");
  for (pCurNode = GetFirstNode (gpTraceList);
      !IsNull (gpTraceList, pCurNode);
      pCurNode = GetNextNode(gpTraceList, pCurNode)) {
    pTraceListEntry = TRACE_ENTRY_FROM_NODE(pCurNode);
    DebugPrint (EFI_D_ERROR, "%s", pTraceListEntry->File);
  }

  DebugPrint (EFI_D_ERROR, "\nFunctionTrace:\n");
  for (pCurNode = GetFirstNode (gpTraceList);
      !IsNull (gpTraceList, pCurNode);
      pCurNode = GetNextNode(gpTraceList, pCurNode)) {
    pTraceListEntry = TRACE_ENTRY_FROM_NODE(pCurNode);
    DebugPrint (EFI_D_ERROR, "%s", pTraceListEntry->Function);
  }
  DebugPrint (EFI_D_ERROR, "\n\n");
  EnableTracing();
  return EFI_SUCCESS;
}

/**
  Clears gpPointerList.
  If there is no list or it is empty returns EFI_NOT_FOUND.
**/
EFI_STATUS
ResetPointerTrace (
  )
{
  LIST_ENTRY *pCurNode = NULL;
  LIST_ENTRY *pTempNode = NULL;
  PointerEntry *pPointerListEntry = NULL;

  if (gpPointerList == NULL || IsListEmpty(gpPointerList)) {
    return EFI_NOT_FOUND;
  }

  DisableTracing();
  for (pCurNode = GetFirstNode (gpPointerList);
      !IsNull (gpPointerList, pCurNode);
      pCurNode = pTempNode) {
    pTempNode = GetNextNode(gpPointerList, pCurNode);
    pPointerListEntry = POINTER_ENTRY_FROM_NODE(pCurNode);
      RemoveEntryList(pCurNode);
      gBS->FreePool ((VOID*)pPointerListEntry);
  }
  EnableTracing();
  return EFI_SUCCESS;
}

/**
  Prints information about tracked memory allocations.
  If there is no list returns EFI_NOT_FOUND.
**/
EFI_STATUS
PrintPointerTrace (
  )
{
  PointerEntry *pPointerListEntry = NULL;
  LIST_ENTRY *pCurNode = NULL;
  UINT64 Index = 0;

  if (gpPointerList == NULL) {
    DebugPrint (EFI_D_ERROR, "No recorded pointer(s) need FreePool.\n");
    return EFI_NOT_FOUND;
  }

  DisableTracing();
  DebugPrint (EFI_D_ERROR, "\n");
  for (pCurNode = GetFirstNode (gpPointerList);
      !IsNull (gpPointerList, pCurNode);
      pCurNode = GetNextNode(gpPointerList, pCurNode), Index++) {
    pPointerListEntry = POINTER_ENTRY_FROM_NODE(pCurNode);

    DebugPrint (EFI_D_ERROR, "Pointer address: %ld\n", pPointerListEntry->pPointer);
    DebugPrint (EFI_D_ERROR, "File:%s\n", pPointerListEntry->File);
    DebugPrint (EFI_D_ERROR, "Function:%s\n", pPointerListEntry->Function);
  }
  if (Index == 0) {
    DebugPrint (EFI_D_ERROR, "No recorded pointer(s) need FreePool.\n\n");
  } else {
    DebugPrint (EFI_D_ERROR, "%ld recorded pointer(s) need FreePool.\n\n", Index);
  }
  EnableTracing();
  return EFI_SUCCESS;
}

/**
  Prints information about tracked memory allocations, then Disables tracing and clears list.
  If there is no list returns EFI_NOT_FOUND.
**/
EFI_STATUS
FlushPointerTrace(
  IN     CHAR16 *pCurrentLocation
  )
{
  PointerEntry *pPointerListEntry = NULL;
  LIST_ENTRY *pCurNode = NULL;
  UINT64 Index = 0;

  if (pCurrentLocation == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DebugPrint(EFI_D_ERROR, "\nIn %s ", pCurrentLocation);
  if (gpPointerList == NULL) {
    DebugPrint (EFI_D_ERROR, "No recorded pointer(s) need FreePool.\n");
    return EFI_NOT_FOUND;
  }

  DisableTracing();
  DebugPrint (EFI_D_ERROR, "\n");
  for (pCurNode = GetFirstNode (gpPointerList);
      !IsNull (gpPointerList, pCurNode);
      pCurNode = GetNextNode(gpPointerList, pCurNode), Index++) {
    pPointerListEntry = POINTER_ENTRY_FROM_NODE(pCurNode);

    DebugPrint (EFI_D_ERROR, "Pointer address: %ld\n", pPointerListEntry->pPointer);
    DebugPrint (EFI_D_ERROR, "File:%s\n", pPointerListEntry->File);
    DebugPrint (EFI_D_ERROR, "Function:%s\n", pPointerListEntry->Function);
  }
  if (Index == 0) {
    DebugPrint (EFI_D_ERROR, "No recorded pointer(s) need FreePool.\n\n");
  } else {
    DebugPrint (EFI_D_ERROR, "%ld recorded pointer(s) need FreePool.\n\n", Index);
  }
  ResetPointerTrace();

  return EFI_SUCCESS;
}
#endif
