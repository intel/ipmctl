/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "DataSet.h"
#include <Library/BaseMemoryLib.h>

#define BOOL_TRUE_STR L"True"
#define BOOL_FALSE_STR L"False"

typedef struct _KEY_VAL {
  LIST_ENTRY Link;
  KEY_VAL_INFO KeyValInfo;
  VOID *Value;
  CHAR16 *ValueToString;
}KEY_VAL;

typedef struct _DATA_SET {
  LIST_ENTRY Link;
  LIST_ENTRY KeyValueList;
  LIST_ENTRY DataSetList;
  VOID *DataSetParent;
  CHAR16 *Name;
  BOOLEAN Dirty;
  VOID *UserData;
}DATA_SET;

typedef struct _DS_NAME_INFO {
  CHAR16 *Name;
  UINT32 InstanceNum;
}DS_NAME_INFO;

#define DATA_SET_LIST_FOR_EACH_SAFE(Entry, NextEntry, ListHead) \
  for(Entry = (ListHead)->ForwardLink, NextEntry = Entry->ForwardLink; \
      Entry != (ListHead); \
      Entry = NextEntry, NextEntry = Entry->ForwardLink \
     )

#define SET_KEY_VALUE(DataSetCtx, RetVal, Key, Val, ValType, ValTypeEnum, Base) \
do { \
  KEY_VAL * KeyVal; \
  if(!DataSetCtx || !Key) { \
    *RetVal = EFI_INVALID_PARAMETER; \
    break; \
  } \
  if(NULL == (KeyVal = SetKeyValue(DataSetCtx, Key, Val, sizeof(ValType)))) { \
    *RetVal = EFI_OUT_OF_RESOURCES; \
    break; \
  } \
  KeyVal->ValueToString = CatSPrint(NULL, FormatString(ValTypeEnum, Base), *((ValType*)Val)); \
  KeyVal->KeyValInfo.Type = ValTypeEnum; \
  *RetVal = EFI_SUCCESS; \
}while(0)

VOID FreeAllKeyValuePairs(DATA_SET *DataSet);

/*
* Set all data sets in the ancestry path to dirty.
*/
VOID SetAncestorsDirty(DATA_SET_CONTEXT *DataSetCtx) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  while (DataSet) {
    DataSet->Dirty = TRUE;
    DataSet = DataSet->DataSetParent;
  }
}

/*
* Helper to locate a child node with a particular name
*/
DATA_SET * FindChildDataSetByIndex(DATA_SET *Parent, CHAR16 *Name, UINT32 Index) {
  LIST_ENTRY  *Entry;
  LIST_ENTRY  *NextEntry;
  DATA_SET    *DataSet;

  DATA_SET_LIST_FOR_EACH_SAFE(Entry, NextEntry, &Parent->DataSetList) {
    DataSet = BASE_CR(Entry, DATA_SET, Link);
    if(0 == StrCmp(Name, DataSet->Name)) {
      if (0 == Index) {
        return DataSet;
      }
      --Index;
    }
  }
  return NULL;
}

/*
* Free a DataSet Struct
*/
VOID FreeDataSetMem(DATA_SET *DataSet) {

    if (NULL == DataSet) {
      return;
    }

    FreeAllKeyValuePairs(DataSet);

    if(DataSet->Name) {
      FreePool(DataSet->Name);
    }

    FreePool(DataSet);
  }

/*
* Recursively free all data sets
*/
VOID FreeAllDataSets(DATA_SET *DataSet) {
  LIST_ENTRY  *Entry;
  LIST_ENTRY  *NextEntry;
  DATA_SET *ChildDataSet;

  if (NULL == DataSet) {
    return;
  }

  DATA_SET_LIST_FOR_EACH_SAFE(Entry, NextEntry, &DataSet->DataSetList) {
    ChildDataSet = BASE_CR(Entry, DATA_SET, Link);
    FreeAllDataSets(ChildDataSet);
  }
  if(!IsListEmpty(&DataSet->Link)) {
    RemoveEntryList(&DataSet->Link);
  }
  FreeDataSetMem(DataSet);
}

/*
* Create a new data set structure
*/
DATA_SET_CONTEXT* CreateDataSet(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *Name, VOID *UserData) {
  DATA_SET *NewDataSet = NULL;
  DATA_SET *ParentCtx = (DATA_SET *)DataSetCtx;

  if (NULL == Name) {
    return NULL;
  }
  //check that parent doesn't already have child with the same name
  /*if (ParentCtx && FindChildDataSet(ParentCtx, Name)) {
    return NULL;
  }*/

  if (NULL == (NewDataSet = (DATA_SET*)AllocateZeroPool(sizeof(DATA_SET)))) {
    return NULL;
  }

  if (NULL == (NewDataSet->Name = CatSPrint(NULL, Name))) {
    FreePool(NewDataSet);
    return NULL;
  }

  NewDataSet->UserData = UserData;
  InitializeListHead(&NewDataSet->KeyValueList);
  InitializeListHead(&NewDataSet->DataSetList);

  NewDataSet->Dirty = FALSE;

  if (DataSetCtx) {
    InsertTailList(&ParentCtx->DataSetList, &NewDataSet->Link);
    NewDataSet->DataSetParent = (VOID*)ParentCtx;
  }
  else {
    InitializeListHead(&NewDataSet->Link);
  }

  return NewDataSet;
}

/*
* Helper for GetDataSet
*/
VOID GetDataSetNameInfo(CHAR16 *Name, DS_NAME_INFO *NameInfo) {
  CHAR16 **Toks = NULL;
  CHAR16 **ToksSecondHalf;
  UINT32 NumToks = 0;
  UINT32 NumToksSecondHalf = 0;

  if (NULL == (Toks = StrSplit(Name, L'[', &NumToks))) {
    return;
  }

  if (1 == NumToks) {
    NameInfo->Name = CatSPrint(NULL, Name);
    NameInfo->InstanceNum = 0;
  }
  else {
    NameInfo->Name = CatSPrint(NULL, Toks[0]);
    if (NULL != (ToksSecondHalf = StrSplit(Toks[1], L']', &NumToksSecondHalf))) {
      NameInfo->InstanceNum = (UINT32)StrDecimalToUint64(ToksSecondHalf[0]);
      FreeStringArray(ToksSecondHalf, NumToksSecondHalf);
    }
  }
  FreeStringArray(Toks, NumToks);
}

/*
* Helper for GetDataSet.  Frees memory allocated by GetDataSetNameInfo.
*/
void FreeDataSetNameInfo(DS_NAME_INFO *NameInfo) {
  if (NULL != NameInfo->Name) {
    FreePool(NameInfo->Name);
  }
}

/*
* Retrieve a data set by specifying a path in the form of /sensorlist/dimm[0]/sensor[1]
*/
DATA_SET_CONTEXT *
EFIAPI
GetDataSet(DATA_SET_CONTEXT *Root, CHAR16 *NamePath, ...) {
  CHAR16 **DataSetToks = NULL;
  UINT32 NumDataSetToks = 0;
  UINT32 Index = 0;
  DATA_SET *TempDataSet = (DATA_SET*)Root;
  DATA_SET *TempCreateNewDataSet = NULL;
  DS_NAME_INFO NameInfo;
  CHAR16 *FormattedNamePath;
  VA_LIST Args;
  UINT32 CreateIndex = 0;

  ++NamePath;
  VA_START(Args, NamePath);
  FormattedNamePath = CatVSPrint(NULL, NamePath, Args);
  VA_END(Args);

  if (NULL == FormattedNamePath) {
    return NULL;
  }

  //split path, result toks are data set names
  if (NULL == (DataSetToks = StrSplit(FormattedNamePath, L'/', &NumDataSetToks))) {
    FreePool(FormattedNamePath);
    return NULL;
  }
  FreePool(FormattedNamePath);

  //Root data set must match first tok
  //All other toks that don't exist will be created
  GetDataSetNameInfo(DataSetToks[0], &NameInfo);
  if (StrCmp(NameInfo.Name, GetDataSetName(Root))) {
    TempDataSet = NULL;
    FreeDataSetNameInfo(&NameInfo);
    goto Finish;
  }
  FreeDataSetNameInfo(&NameInfo);
  //iterate through all data set names under the root
  //path: /sensorlist/dimm/sensor
  //iterated toks: dimm, sensor
  //create data sets that don't exist
  for (Index = 1; Index < NumDataSetToks; ++Index) {
    //get info about current data set
    GetDataSetNameInfo(DataSetToks[Index], &NameInfo);
    for (CreateIndex = 0; CreateIndex <= NameInfo.InstanceNum; ++CreateIndex) {
      if (NULL == (TempCreateNewDataSet = FindChildDataSetByIndex(TempDataSet, NameInfo.Name, NameInfo.InstanceNum))) {
        //create a new data set and add it to the end
        if (NULL == (TempCreateNewDataSet = CreateDataSet(TempDataSet, NameInfo.Name, NULL))) {
          TempDataSet = NULL;
          goto Finish;
        }
      }
    }
    FreeDataSetNameInfo(&NameInfo);
    TempDataSet = TempCreateNewDataSet;
  }
Finish:
  FreeStringArray(DataSetToks, NumDataSetToks);
  return TempDataSet;
}

/*
* Get the next child dataset in the data set.
*/
DATA_SET_CONTEXT *GetNextChildDataSet(DATA_SET_CONTEXT *DataSetCtx, DATA_SET_CONTEXT *CurrentChildDataSetCtx) {
  LIST_ENTRY  *Entry;
  DATA_SET *DataSet = (DATA_SET *)DataSetCtx;
  DATA_SET *ChildDataSet = (DATA_SET *)CurrentChildDataSetCtx;
  DATA_SET *NextChildDataSet;

  if (NULL == CurrentChildDataSetCtx) {
    //GetFirstNode returns original list is empty
    if (&DataSet->DataSetList == (Entry = GetFirstNode(&DataSet->DataSetList))) {
      return NULL;
    }
    NextChildDataSet = BASE_CR(Entry, DATA_SET, Link);
    return (DATA_SET_CONTEXT*)NextChildDataSet;
  }

  if (NULL != (Entry = GetNextNode(&DataSet->DataSetList, &ChildDataSet->Link))) {
    //GetNextNode returns original list when Link is the last node in list.
    if (Entry != &DataSet->DataSetList) {
      NextChildDataSet = BASE_CR(Entry, DATA_SET, Link);
      return (DATA_SET_CONTEXT*)NextChildDataSet;
    }
  }
  return NULL;
}

/*
* Does the data set contain children data sets?
*/
BOOLEAN IsLeaf(DATA_SET_CONTEXT *DataSetCtx) {
  LIST_ENTRY  *Entry;
  DATA_SET *DataSet = (DATA_SET *)DataSetCtx;

  if (NULL == DataSetCtx) {
    return TRUE;
  }

  if (&DataSet->DataSetList == (Entry = GetFirstNode(&DataSet->DataSetList))) {
    return TRUE;
  }

  return FALSE;
}

/*
* Helper to walk a data set hierarchy
*/
VOID RecurseDataSetInternal(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, DataSetCallBack CallBackRoutine, DataSetAllChildrenDoneCallBack ChildrenDoneCallBackRoutine, VOID *UserData, VOID *CallbackData, BOOLEAN Sparse) {
  LIST_ENTRY  *Entry;
  LIST_ENTRY  *NextEntry;
  DATA_SET *ChildDataSet;
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  CHAR16 *NewPath = NULL;
  VOID * TempCallBackRetVal;

  if (NULL == DataSet || (Sparse && !DataSet->Dirty)) {
    return;
  }

  NewPath = CatSPrint(CurPath, L"/" FORMAT_STR, GetDataSetName(DataSetCtx));
  TempCallBackRetVal = CallBackRoutine(DataSetCtx, NewPath, UserData, CallbackData);

  DATA_SET_LIST_FOR_EACH_SAFE(Entry, NextEntry, &DataSet->DataSetList) {
    ChildDataSet = BASE_CR(Entry, DATA_SET, Link);
    RecurseDataSetInternal(ChildDataSet, NewPath, CallBackRoutine, ChildrenDoneCallBackRoutine, UserData, TempCallBackRetVal, Sparse);
  }

  if (ChildrenDoneCallBackRoutine) {
    ChildrenDoneCallBackRoutine(DataSetCtx, NewPath, UserData);
  }

  if (NewPath) {
    FreePool(NewPath);
  }

  if (TempCallBackRetVal) {
    FreePool(TempCallBackRetVal);
  }
}

/*
* Recurses through data set tree, invoking the callback at each node (data set) in the tree
*/
VOID RecurseDataSet(DATA_SET_CONTEXT *DataSetCtx, DataSetCallBack CallBackRoutine, DataSetAllChildrenDoneCallBack ChildrenDoneCallBackRoutine, VOID *UserData, BOOLEAN Sparse) {
  RecurseDataSetInternal(DataSetCtx, NULL, CallBackRoutine, ChildrenDoneCallBackRoutine, UserData, NULL, Sparse);
}

/*
* Free a data set structure
*/
VOID FreeDataSet(DATA_SET_CONTEXT *DataSetCtx) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  FreeAllDataSets(DataSet);
}

/*
* Append a child data set to a root data set
*/
VOID AddChildDataSet(DATA_SET_CONTEXT *Root, DATA_SET_CONTEXT *Child) {
  DATA_SET *RootDataSet = (DATA_SET*)Root;
  DATA_SET *ChildDataSet = (DATA_SET*)Child;
  if (NULL != Root && NULL != Child) {
    InsertTailList(&RootDataSet->DataSetList, &ChildDataSet->Link);
  }
}

/*
* Set the name of a data set
*/
VOID SetDataSetName(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *Name) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  if (DataSet) {
    if (DataSet->Name) {
      FreePool(DataSet->Name);
    }
    DataSet->Name = CatSPrint(NULL, Name);
  }
}

/*
* Get the name of a data set
*/
CHAR16 * GetDataSetName(DATA_SET_CONTEXT *DataSetCtx) {
  DATA_SET *DataSet = (DATA_SET *)DataSetCtx;
  return DataSet->Name;
}

/*
* set the user's data to the corresponding data set (UserData must be allocated by AllocatePool)
*/
VOID SetDataSetUserData(DATA_SET_CONTEXT *DataSetCtx, VOID *UserData) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  if(DataSet->UserData) {
    FreePool(DataSet->UserData);
  }
  DataSet->UserData = UserData;
}

/*
* Return the user's data from the corresponding data set
*/
VOID * GetDataSetUserData(DATA_SET_CONTEXT *DataSetCtx) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  return DataSet->UserData;
}

/*
* Callback routine for squash operation
*/
VOID * SquashDataSetCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
  DATA_SET_CONTEXT *RootDataSet = (DATA_SET_CONTEXT*)UserData;
  DATA_SET_CONTEXT *NewDataSet = NULL;
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;

  if (NULL == UserData) {
    return NULL;
  }

  if (IsLeaf(DataSetCtx)) {
    NewDataSet = CreateDataSet(RootDataSet, CatSPrint(NULL, GetDataSetName(DataSetCtx)), NULL);
    while (NULL != (KvInfo = GetNextKey(DataSetCtx, KvInfo))) {
      GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
      if (Val) {
        SetKeyValueWideStr(NewDataSet, KvInfo->Key, Val);
      }
    }
    KvInfo = NULL;
    while (NULL != (KvInfo = GetNextKey(RootDataSet, KvInfo))) {
      GetKeyValueWideStr(RootDataSet, KvInfo->Key, &Val, NULL);
      if (Val) {
        SetKeyValueWideStr(NewDataSet, KvInfo->Key, Val);
      }
    }
  }
  else {
    while (NULL != (KvInfo = GetNextKey(DataSetCtx, KvInfo))) {
      GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
      if (Val) {
        SetKeyValueWideStr(RootDataSet, KvInfo->Key, Val);
      }
    }
  }
  return NULL;
}

/*
* Squash a data set hierarchy
* Before Squash:
* -SensorList
* --Dimm
* --Dimm.DimmId = 0x0001 (key/val pair under Dimm)
* ---Sensor
* ---Sensor.Value = 32C
* After Squash:
* -SensorList
* --Sensor
* --Sensor.DimmId = 0x0001
* --Sensor.Value = 32C
*/
DATA_SET_CONTEXT *SquashDataSet(DATA_SET_CONTEXT *DataSetCtx) {
  DATA_SET_CONTEXT *ChildDataSet = NULL;
  DATA_SET_CONTEXT *NewRootDataSet = NULL;

  if (IsLeaf(DataSetCtx)) {
    return DataSetCtx;
  }

  if (NULL == (NewRootDataSet = CreateDataSet(NULL, CatSPrint(NULL, GetDataSetName(DataSetCtx)), NULL))) {
    return NewRootDataSet;
  }

  while (NULL != (ChildDataSet = GetNextChildDataSet(DataSetCtx, ChildDataSet))) {
    RecurseDataSet(ChildDataSet, SquashDataSetCb, NULL, NewRootDataSet, TRUE);
    FreeAllKeyValuePairs(NewRootDataSet);
  }
  return NewRootDataSet;
}

/*
* Helper to obtain a format string for the specific type
*/
CHAR16 * FormatString(KEY_TYPE KeyType, TO_STRING_BASE Base) {
  switch (KeyType) {
  case KEY_UINT64:
    return (Base == HEX) ? L"0x" FORMAT_UINT64_HEX : FORMAT_UINT64;
  case KEY_INT64:
    return (Base == HEX) ? L"0x" FORMAT_UINT64_HEX : FORMAT_INT64;
  case KEY_UINT32:
    return (Base == HEX) ? L"0x" FORMAT_UINT32_HEX : FORMAT_UINT32;
  case KEY_INT32:
    return (Base == HEX) ? L"0x" FORMAT_UINT32_HEX : FORMAT_INT32;
  case KEY_UINT16:
    return (Base == HEX) ? L"0x" FORMAT_UINT16_HEX : FORMAT_UINT16;
  case KEY_INT16:
    return (Base == HEX) ? L"0x" FORMAT_UINT16_HEX : FORMAT_INT16;
  case KEY_UINT8:
    return (Base == HEX) ? L"0x" FORMAT_UINT8_HEX : FORMAT_UINT8;
  case KEY_INT8:
    return (Base == HEX) ? L"0x" FORMAT_UINT8_HEX : FORMAT_INT8;
  default:
    return NULL;
  }
}

/*
* Helper to locate a child node with a particular name
*/
KEY_VAL * FindKeyValuePair(DATA_SET *DataSet, const CHAR16 *Key) {
  LIST_ENTRY  *Entry;
  LIST_ENTRY  *NextEntry;
  KEY_VAL *KeyVal;

  DATA_SET_LIST_FOR_EACH_SAFE(Entry, NextEntry, &DataSet->KeyValueList) {
    KeyVal = BASE_CR(Entry, KEY_VAL, Link);
    if (0 == StrCmp(Key, KeyVal->KeyValInfo.Key)) {
      return KeyVal;
    }
  }
  return NULL;
}

/*
* Free a single KeyVal Struct
*/
VOID FreeKeyValMem(KEY_VAL *KeyVal) {
  if (NULL == KeyVal) {
    return;
  }
  if (KeyVal->KeyValInfo.Key) {
    FreePool(KeyVal->KeyValInfo.Key);
  }
  if ((KeyVal->ValueToString) && (KeyVal->ValueToString != KeyVal->Value)) {
    FreePool(KeyVal->ValueToString);
  }
  if (KeyVal->Value) {
    FreePool(KeyVal->Value);
  }
  if (KeyVal->KeyValInfo.UserData) {
    FreePool(KeyVal->KeyValInfo.UserData);
  }
  FreePool(KeyVal);
}

/*
* Free all key val pairs in the set
*/
VOID FreeAllKeyValuePairs(DATA_SET *DataSet) {
  LIST_ENTRY  *Entry;
  LIST_ENTRY  *NextEntry;
  KEY_VAL *KeyVal;

  DATA_SET_LIST_FOR_EACH_SAFE(Entry, NextEntry, &DataSet->KeyValueList) {
    KeyVal = BASE_CR(Entry, KEY_VAL, Link);
    RemoveEntryList(&KeyVal->Link);
    FreeKeyValMem(KeyVal);
  }
}

/*
* Create a new keyval struct
*/
KEY_VAL * CreateKeyVal(DATA_SET_CONTEXT *DataSetCtx) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = (KEY_VAL*)AllocateZeroPool(sizeof(KEY_VAL));
  if(NULL == KeyVal) {
    return NULL;
  }
  InsertTailList(&DataSet->KeyValueList, &KeyVal->Link);
  return KeyVal;
}

/*
* Set a unicode string value
*/
EFI_STATUS EFIAPI SetKeyValueWideStrFormat(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, const CHAR16 *Val, ...) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  CHAR16 *TempMsg = NULL;
  VA_LIST Marker;
  EFI_STATUS RetCode = EFI_SUCCESS;

  if (NULL == Key || NULL == Val || NULL == DataSet) {
    return EFI_INVALID_PARAMETER;
  }

  VA_START(Marker, Val);
  TempMsg = CatVSPrint(NULL, Val, Marker);
  VA_END(Marker);

  RetCode = SetKeyValueWideStr(DataSetCtx, Key, TempMsg);

  FREE_POOL_SAFE(TempMsg);
  return RetCode;
}

/*
* Set a unicode string value
*/
EFI_STATUS SetKeyValueWideStr(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, const CHAR16 *Val) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;


  if (NULL == Key || NULL == Val || NULL == DataSet) {
    return EFI_INVALID_PARAMETER;
  }

  //first try to find the key, but if not found create a new key/value entry
  //and set the name of the key
  if (NULL == (KeyVal = FindKeyValuePair(DataSet, Key))) {
    if (NULL == (KeyVal = CreateKeyVal(DataSet))) {
      return EFI_OUT_OF_RESOURCES;
    }
    KeyVal->KeyValInfo.Key = CatSPrint(NULL, Key);
  }
  //found the key, now free previous values (tostring and actual value)
  else {
    if ((KeyVal->ValueToString) && (KeyVal->ValueToString != KeyVal->Value)) {
      FreePool(KeyVal->ValueToString);
    }

    if (KeyVal->Value) {
      FreePool(KeyVal->Value);
    }
  }

  if (NULL == (KeyVal->Value = AllocatePool(StrSize(Val)))) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem(KeyVal->Value, (VOID*)Val, StrSize(Val));
  KeyVal->ValueToString = KeyVal->Value;
  SetAncestorsDirty(DataSetCtx);
  return EFI_SUCCESS;
}
/*
* Retrieve a unicode string from the data set.
*/
EFI_STATUS GetKeyValueWideStr(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, CHAR16 **Val, CHAR16 *DefaultVal) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;

  if (NULL == Key || NULL == Val || NULL == DataSet) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == (KeyVal = FindKeyValuePair(DataSet, Key))) {
    *Val = DefaultVal;
  }
  else {
    *Val = KeyVal->ValueToString;
  }
  return EFI_SUCCESS;
}

/*
* Helper to set all primitive types
*/
KEY_VAL * SetKeyValue(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, VOID * Val, UINTN ValSize) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;

  if (NULL == Key || NULL == Val || NULL == DataSet) {
    return NULL;
  }

  if (NULL == (KeyVal = FindKeyValuePair(DataSet, Key))) {
    if (NULL == (KeyVal = CreateKeyVal(DataSet))) {
      return NULL;
    }
    KeyVal->KeyValInfo.Key = CatSPrint(NULL, Key);
  }
  else {
    if ((KeyVal->ValueToString) && (KeyVal->ValueToString != KeyVal->Value)) {
      FreePool(KeyVal->ValueToString);
    }

    if (KeyVal->Value) {
      FreePool(KeyVal->Value);
    }
  }
  KeyVal->Value = AllocatePool(ValSize);
  if(KeyVal->Value) {
    CopyMem(KeyVal->Value, Val, ValSize);
  }
  KeyVal->KeyValInfo.ValueSize = (UINT32)ValSize;

  SetAncestorsDirty(DataSetCtx);
  return KeyVal;
}

/*
* Helper to get all primitive types
*/
EFI_STATUS GetKeyValue(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, VOID *Val, UINTN ValSize, VOID *DefaultVal) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;

  if (NULL == Key || NULL == Val || NULL == DataSet) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == (KeyVal = FindKeyValuePair(DataSet, Key))) {
    CopyMem(Val, DefaultVal, ValSize);
  }
  else {
    if(KeyVal->KeyValInfo.ValueSize <= ValSize) {
      CopyMem(Val, KeyVal->Value, KeyVal->KeyValInfo.ValueSize);
    }
    else {
      CopyMem(Val, DefaultVal, ValSize);
    }
  }
  return EFI_SUCCESS;
}

/*
* Set a boolean into the data set.
*/
EFI_STATUS SetKeyValueBool(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, BOOLEAN Val) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;
  const CHAR16 *BoolVal = Val ? BOOL_TRUE_STR : BOOL_FALSE_STR;

  if (NULL == Key || NULL == DataSet) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == (KeyVal = FindKeyValuePair(DataSet, Key))) {
    if (NULL == (KeyVal = CreateKeyVal(DataSet))) {
      return EFI_OUT_OF_RESOURCES;
    }
    KeyVal->KeyValInfo.Key = CatSPrint(NULL, Key);
  }
  else {
    if ((KeyVal->ValueToString) && (KeyVal->ValueToString != KeyVal->Value)) {
      FreePool(KeyVal->ValueToString);
    }

    if (KeyVal->Value) {
      FreePool(KeyVal->Value);
    }
  }
  if (NULL == (KeyVal->Value = AllocatePool(sizeof(BOOLEAN)))) {
    return EFI_OUT_OF_RESOURCES;
  }
  CopyMem(KeyVal->Value, (VOID*)&Val, sizeof(BOOLEAN));
  KeyVal->ValueToString = CatSPrint(NULL, BoolVal);
  SetAncestorsDirty(DataSetCtx);
  return EFI_SUCCESS;
}

/*
* Retrieve a boolean value from the data set.
*/
EFI_STATUS GetKeyValueBool(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, BOOLEAN *Val, BOOLEAN *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(BOOLEAN), (VOID *)DefaultVal);
}

/*
* Set an unsigned 64 bit value into the data set.
*/
EFI_STATUS SetKeyValueUint64(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT64 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, UINT64, KEY_UINT64, Base);
  return RetVal;
}

/*
* Retrieve an unsigned 64 bit value from the data set.
*/
EFI_STATUS GetKeyValueUint64(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT64 *Val, UINT64 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(UINT64), (VOID *)DefaultVal);
}

/*
* Set an signed 64 bit value into the data set.
*/
EFI_STATUS SetKeyValueInt64(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT64 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, INT64, KEY_INT64, Base);
  return RetVal;
}

/*
* Retrieve a signed 64 bit value from the data set.
*/
EFI_STATUS GetKeyValueInt64(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT64 *Val, INT64 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(INT64), (VOID *)DefaultVal);
}

/*
* Set an unsigned 32 bit value into the data set.
*/
EFI_STATUS SetKeyValueUint32(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT32 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, UINT32, KEY_UINT32, Base);
  return RetVal;
}

/*
* Retrieve an unsigned 32 bit value from the data set.
*/
EFI_STATUS GetKeyValueUint32(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT32 *Val, UINT32 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(UINT32), (VOID *)DefaultVal);
}

/*
* Set an signed 32 bit value into the data set.
*/
EFI_STATUS SetKeyValueInt32(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT32 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, INT32, KEY_INT32, Base);
  return RetVal;
}

/*
* Retrieve a signed 32 bit value from the data set.
*/
EFI_STATUS GetKeyValueInt32(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT32 *Val, INT32 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(INT32), (VOID *)DefaultVal);
}

/*
* Set an unsigned 16 bit value into the data set.
*/
EFI_STATUS SetKeyValueUint16(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT16 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, UINT16, KEY_UINT16, Base);
  return RetVal;
}

/*
* Retrieve an unsigned 16 bit value from the data set.
*/
EFI_STATUS GetKeyValueUint16(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT16 *Val, UINT16 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(UINT16), (VOID *)DefaultVal);
}

/*
* Set an signed 16 bit value into the data set.
*/
EFI_STATUS SetKeyValueInt16(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT16 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, INT16, KEY_INT16, Base);
  return RetVal;
}

/*
* Retrieve a signed 16 bit value from the data set.
*/
EFI_STATUS GetKeyValueInt16(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT16 *Val, INT16 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(INT16), (VOID *)DefaultVal);
}

/*
* Set an unsigned 8 bit value into the data set.
*/
EFI_STATUS SetKeyValueUint8(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT8 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, UINT8, KEY_UINT8, Base);
  return RetVal;
}

/*
* Retrieve an unsigned 8 bit value from the data set.
*/
EFI_STATUS GetKeyValueUint8(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, UINT8 *Val, UINT8 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(UINT8), (VOID *)DefaultVal);
}

/*
* Set a signed 8 bit value into the data set.
*/
EFI_STATUS SetKeyValueInt8(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT8 Val, TO_STRING_BASE Base) {
  EFI_STATUS RetVal = EFI_SUCCESS;
  SET_KEY_VALUE(DataSetCtx, &RetVal, Key, (VOID*)&Val, INT8, KEY_INT8, Base);
  return RetVal;
}

/*
* Retrieve a signed 8 bit value from the data set.
*/
EFI_STATUS GetKeyValueInt8(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, INT8 *Val, INT8 *DefaultVal) {
  return GetKeyValue(DataSetCtx, Key, (VOID *)Val, sizeof(INT8), (VOID *)DefaultVal);
}

/*
* Get the next key in the data set. KeyInfo == NULL retrieves the first key in the data set.
*/
KEY_VAL_INFO * GetNextKey(DATA_SET_CONTEXT *DataSetCtx, KEY_VAL_INFO *KeyInfo) {
  LIST_ENTRY  *Entry;
  KEY_VAL *KeyVal;
  DATA_SET *DataSet = (DATA_SET *)DataSetCtx;

  if (NULL == KeyInfo) {
    //GetFirstNode returns original list is empty
    if (&DataSet->KeyValueList == (Entry = GetFirstNode(&DataSet->KeyValueList))) {
      return NULL;
    }
    KeyVal = BASE_CR(Entry, KEY_VAL, Link);
    return &KeyVal->KeyValInfo;
  }

  if (NULL != (KeyVal = FindKeyValuePair(DataSet, KeyInfo->Key))) {
    if (NULL != (Entry = GetNextNode(&DataSet->KeyValueList, &KeyVal->Link))) {
      //GetNextNode returns original list when Link is the last node in list.
      if (Entry != &DataSet->KeyValueList) {
        KeyVal = BASE_CR(Entry, KEY_VAL, Link);
        return &KeyVal->KeyValInfo;
      }
    }
  }
  return NULL;
}

/*
* Get the number of key/val pairs in a data set.
*/
UINT32 GetKeyCount(DATA_SET_CONTEXT *DataSetCtx) {
  KEY_VAL_INFO * pKeyInfo = NULL;
  UINT32 Count = 0;

  while (NULL != (pKeyInfo = GetNextKey(DataSetCtx, pKeyInfo))) {
    ++Count;
  }

  return Count;
}

/*
* Associate user data with a particular key
*/
EFI_STATUS SetKeyUserData(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key, VOID *UserData) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;

  if (NULL == Key || NULL == DataSet || NULL == UserData) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL != (KeyVal = FindKeyValuePair(DataSet, Key))) {
    KeyVal->KeyValInfo.UserData = UserData;
  }
  else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/*
* Retrieve user data from a particular key
*/
VOID * GetKeyUserData(DATA_SET_CONTEXT *DataSetCtx, const CHAR16 *Key) {
  DATA_SET *DataSet = (DATA_SET*)DataSetCtx;
  KEY_VAL *KeyVal = NULL;

  if (NULL == Key || NULL == DataSet) {
    return NULL;
  }

  if (NULL != (KeyVal = FindKeyValuePair(DataSet, Key))) {
    return KeyVal->KeyValInfo.UserData;
  }
  else {
    return NULL;
  }
}
