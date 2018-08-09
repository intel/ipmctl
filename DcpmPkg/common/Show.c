/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "Show.h"
#include <Debug.h>
#include <Library/BaseMemoryLib.h>

#define EXPAND_STR_MAX                    1024
#define ESX_XML_FILE_BEGIN                L"<?xml version=\"1.0\"?><output xmlns=\"http://www.vmware.com/Products/ESX/5.0/esxcli/\">"
#define ESX_XML_FILE_END                  L"</output>\n"
#define ESX_XML_STRUCT_LIST_TAG_BEGIN     L"<list type = \"structure\">\n"
#define ESX_XML_LIST_TAG_END              L"</list>\n"

#define NVM_XML_DATA_SET_TAG_START        L"<" FORMAT_STR L">\n"
#define NVM_XML_KEY_VAL_TAG               L"<" FORMAT_STR L">" FORMAT_STR L"</" FORMAT_STR L">\n"
#define NVM_XML_DATA_SET_TAG_END          L"</" FORMAT_STR L">\n"
#define NVM_XML_WHITESPACE_IDENT          L" "

#define TEXT_TABLE_DEFAULT_DELIM          L'|'
#define TEXT_NEW_LINE                     L"\n"
#define TEXT_TABLE_HEADER_SEP             L"="

#define TEXT_LIST_WHITESPACE_IDENT        L"   "
#define TEXT_LIST_IGNORE_LIST_DELIM       L';'
#define TEXT_LIST_HEADER                  L"---"
#define TEXT_LIST_KEY_VAL_DELIM           L"="

#define CHAR_NULL_TERM                    L'\0'
#define CHAR_PATH_DELIM                   L'/'
#define CHAR_WHITE_SPACE                  L' '


typedef struct _PRV_TABLE_INFO {
  SHOW_TABLE_ATTRIB *AllTableAttribs;
  CHAR16 *CurrentPath;
  CHAR16 *PrinterNode;
}PRV_TABLE_INFO;


/**
Helper for finding the list attribute associated with a particular data set

@param[in] LevelType: Name of the data set that represents a particular level in the hierarchy
@param[in] Attribs: Contains a LIST_LEVEL_ATTRIB for each level in the hierarchy

@retval LIST_LEVEL_ATTRIB* that represents the LevelType
@retval NULL if not found
**/
LIST_LEVEL_ATTRIB *ShowTextListFindAttrib(IN CHAR16 *LevelType, IN SHOW_LIST_ATTRIB *Attribs) {
  UINT32 Index;
  if (!LevelType || !Attribs) {
    return NULL;
  }

  for (Index = 0; Index  < MAX_LIST_LEVELS; Index ++) {
    if (Attribs->LevelAttribs[Index].LevelType && (0 == StrCmp(LevelType, Attribs->LevelAttribs[Index].LevelType))) {
      return &Attribs->LevelAttribs[Index];
    }
  }
  return NULL;
}

/**
Expands a string by replacing $(key) identifiers with the
associated value if the key/value pair exist in the data set.
Example: ---Dimm=$(DimmId)--- Expands to: ---Dimm=0x0001---

@param[in] DataSetCtx: Data set contains key/val pairs
@param[in] Str: May contain $(key) identifiers that need to be replaced with corresponding values

@retval CHAR16* Expanded version of the input Str.
**/
CHAR16 *ShowTextListExpandStr(IN DATA_SET_CONTEXT *DataSetCtx, IN const CHAR16 *Str) {
  CHAR16 *Val = NULL;
  CHAR16 *ExpandedStr = AllocateZeroPool(EXPAND_STR_MAX);
  const CHAR16 *T1 = Str;
  const CHAR16 *T2 = T1;
  CHAR16 *T3 = ExpandedStr;
  CHAR16 *TempKey = NULL;
  UINTN AppendSize = 0;
  UINTN FreeSize = EXPAND_STR_MAX - sizeof(CHAR16); //minus one for NULL terminator

  if (NULL == ExpandedStr || NULL == Str) {
    goto Finish;
  }

  while (*T1 != CHAR_NULL_TERM) {
    AppendSize = 0;
    //Found begining of $(key) identifier
    if (T1[0] == L'$' && T1[1] == L'(') {
      //Find end of $(key) identifier
      do {
        ++T2;
      }
      //Find end of $(key) identifier or end of str, which ever comes first
      while(*T2 != CHAR_NULL_TERM && *T2 != L')');
      //T1 points to begining of $(key) identifier, T2 points to the end
      //allocate enough memory to copy the string that sits between pointers
      //plus 1 char worth of mem for NULL term to make it a string
      TempKey = AllocateZeroPool(((UINTN)T2 - (UINTN)T1) + sizeof(CHAR16));
      CopyMem(TempKey, T1, ((UINTN)T2 - (UINTN)T1));
      //Get the value associated with the key identifier
      //TempKey[2] - skips past "$(" prefix
      GetKeyValueWideStr(DataSetCtx, (CHAR16*)&TempKey[2], &Val, NULL);
      if (NULL != Val) {
        //don't copy NULL terminator
        AppendSize = StrSize(Val) - sizeof(CHAR16);
        if(0 > (INTN)(FreeSize - AppendSize))
          goto Finish;
        //don't use str copy APIs as Val may contain format specifiers
        CopyMem(T3, Val, AppendSize);
        T3 += StrLen(Val);
      }
      else {
        //don't copy NULL terminator
        AppendSize = StrSize(TempKey) - sizeof(CHAR16);
        if (0 > (INTN)(FreeSize - AppendSize)) {
          goto Finish;
        }
        //don't use str copy APIs as Val may contain format specifiers
        CopyMem(T3, TempKey, AppendSize);
        T3 += StrLen(TempKey);
      }
      FREE_POOL_SAFE(TempKey);
      //Advance T2 past the last part of the $(key) identifier
      if(*T2 == L')') {
        ++T2;
      }
      //Reset T1
      T1 = T2;
    }
    else {
      AppendSize = sizeof(*T1);
      if (0 > (INTN)(FreeSize - AppendSize)) {
        goto Finish;
      }
      *T3 = *T1;
      ++T3;
      ++T1;
      ++T2;
    }

    FreeSize -= AppendSize;
  }
 Finish:
  FREE_POOL_SAFE(TempKey);
  return ExpandedStr;
}

/**
Helper to create text list indentions when printing based on depth of CurPath
**/
INT32 ShowTextListGetListLevel(IN CHAR16 *CurPath) {
  INT32 GroupLevel = 0;

  if (!CurPath) {
    return GroupLevel;
  }

  while (*CurPath != CHAR_NULL_TERM) {
    if (*CurPath++ == CHAR_PATH_DELIM) {
      ++GroupLevel;
    }
  }

  return --GroupLevel;
}

/**
Helper to create text list indentions when printing based on depth of CurPath

@param[in] CurPath: Represents a hierarchical path of data sets in the form of /sensorlist/dimm/sensor

@retval UINT32 Whitespace multiplier
**/
UINT32 ShowTextListGetPrintIdentMultiplier(IN CHAR16 *CurPath) {
  INT32 SkipCnt = 2;
  INT32 Multiplier = 0;

  if(!CurPath)
    return Multiplier;

  while (*CurPath != CHAR_NULL_TERM) {
    if (*CurPath++ == CHAR_PATH_DELIM) {
      if(0 >= SkipCnt--) {
        ++Multiplier;
      }
    }
  }

  return Multiplier;
}

/**
Helper to create text list indentions when printing based on a multiplier value

@param[in] Multiplier: Affects how many whitespaces to print

**/
VOID ShowTextListPrintIdent(IN UINT32 Multiplier) {
  while (Multiplier--) {
    Print(TEXT_LIST_WHITESPACE_IDENT);
  }
}

/**
Helper ShowTextListCb to determine if a key should be displayed or not
**/
BOOLEAN ShowTextListIgnoreKey(IN CHAR16 *IgnoreKeyList[], IN UINT32 IgnoreListSz, IN CHAR16 *Key) {
  UINT32 Index;

  if(!IgnoreKeyList || !Key) {
    return FALSE;
  }

  for (Index = 0; Index < IgnoreListSz; ++Index) {
    if (0 == StrCmp((const CHAR16*)IgnoreKeyList[Index], (const CHAR16*)Key)) {
      return TRUE;
    }
  }
  return FALSE;
}
/**
The callback routine for displaying a hierarchical list

@param[in] DataSetCtx: Represents a data set that contains key/val pairs
@param[in] CurPath: Represents a hierarchical path of data sets in the form of /sensorlist/dimm/sensor
@param[in] UserData: Represents a SHOW_LIST_ATTRIB struct
@param[in] ParentUserData: Not used in this context

@retval VOID * Always returns NULL
**/
VOID * ShowTextListCb(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN VOID *UserData, IN VOID *ParentUserData) {
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;
  SHOW_LIST_ATTRIB *Attribs = (SHOW_LIST_ATTRIB *)UserData;
  LIST_LEVEL_ATTRIB *LevelAttribs = NULL;
  CHAR16 *Header = NULL;
  CHAR16 **Toks = NULL;
  UINT32 NumToks = 0;
  BOOLEAN ListHeaderPrinted = FALSE;
  UINT32 IdentMultiplier = ShowTextListGetPrintIdentMultiplier(CurPath);
  //Example:CurPath == /sensorlist/dimm
  //Group level is base 0, there value will be 1
  INT32 ListGroupLevel = ShowTextListGetListLevel(CurPath);

  //if CMD is overriding default text list attributes
  if(Attribs) {
    //extract the list attribute associated with the current data set node
    LevelAttribs = ShowTextListFindAttrib(GetDataSetName(DataSetCtx), Attribs);
    //if a header attribute is specified
    if (LevelAttribs && LevelAttribs->LevelHeader) {
      //overriden header may contain key/val "macros" in the form of L"---DimmId=$(DimmId)---"
      //where $(DimmId) is the name of a key within a particular data set.
      //ShowTextListExpandStr will replace macros with associated key value.
      if (NULL != (Header = ShowTextListExpandStr(DataSetCtx, LevelAttribs->LevelHeader))) {
        //print whitespace indentation based on depth of data set node
        ShowTextListPrintIdent(IdentMultiplier);
        Print(FORMAT_STR_NL, Header);
        //free the expanded header
        FreePool(Header);
        ListHeaderPrinted = TRUE;
      }
    }
  }

  //CMD can specify keys to ignore (don't display)
  if(LevelAttribs && LevelAttribs->IgnoreKeyValList) {
    Toks = StrSplit((CHAR16*)LevelAttribs->IgnoreKeyValList, TEXT_LIST_IGNORE_LIST_DELIM, &NumToks);
  }

  //Iterate through current data set's key/values and display them
  while (NULL != (KvInfo = GetNextKey(DataSetCtx, KvInfo))) {
    //If key is not on ignore list, print it
    if (!ShowTextListIgnoreKey(Toks, NumToks, KvInfo->Key)) {
      //retrieve the value in wide string form
      GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
      //print a whitespace indentation
      ShowTextListPrintIdent(IdentMultiplier);
      //by default 1). the data set's group header is the first key/val pair found
      //2). the first group in the list does not have a header
      if (!ListHeaderPrinted && ListGroupLevel > 0) {
        Print(TEXT_LIST_HEADER);
      }
      else {
        ShowTextListPrintIdent(IdentMultiplier);
      }

      //default is to dislay key/val pair as Key=Value unless overriden
      if (LevelAttribs && LevelAttribs->LevelKeyValFormatStr && StrLen(LevelAttribs->LevelKeyValFormatStr) > 0) {
        Print(LevelAttribs->LevelKeyValFormatStr, KvInfo->Key, Val);
      }
      else {
        Print(FORMAT_STR TEXT_LIST_KEY_VAL_DELIM FORMAT_STR, KvInfo->Key, Val);
      }
      //by default only group level 2 and greater will include the ---
      if (!ListHeaderPrinted && ListGroupLevel > 1) {
        Print(TEXT_LIST_HEADER);
      }
      Print(TEXT_NEW_LINE);
      ListHeaderPrinted = TRUE;
    }
  }

  FreeStringArray(Toks, NumToks);
  return NULL;
}

/**
Main entry point for displaying a hierarchical list

@param[in] DataSetCtx: Represents a data set that contains key/val pairs
@param[in] ShowListAttribs: User specified attributes that defines how a list should be printed
**/
VOID ShowTextList(IN DATA_SET_CONTEXT *DataSetCtx, IN SHOW_LIST_ATTRIB *ShowListAttribs) {
  RecurseDataSet(DataSetCtx, ShowTextListCb, NULL, (VOID*)ShowListAttribs, TRUE);
}

/**
The node that represents the last cell in a row is responsible for printing the entire row.
This is a helper to determine if a particular node is the "printer node".

@param[in] DataSetCtx: Represents a data set that contains key/val pairs
@param[in] CurPath: Represents a hierarchical path of data sets in the form of /sensorlist/dimm/sensor
@param[in] PrvTableInfo: Contains information related to a table

@retval BOOLEAN TRUE if CurPath points to the designated "printer node" else FALSE
**/
BOOLEAN ShowTextTableIsPrinterNode(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN PRV_TABLE_INFO *PrvTableInfo) {
  CHAR16 *TmpStr = CatSPrint(CurPath,L".");
  if(!PrvTableInfo) {
    return FALSE;
  }
  BOOLEAN IsPrinter = (0 == StrnCmp(TmpStr, PrvTableInfo->PrinterNode, StrLen(TmpStr)));
  FreePool(TmpStr);
  return IsPrinter;
}

/**
This routine takes a Path and returns a pointer to the keyname.

@param[in] Path: In the form of /sensorlist/dimm/sensor.keyname.

@retval CHAR16 * Pointer to keyname else NULL terminator if keyname not found.
**/
const CHAR16 *ShowTextTableFindKeyInPath(IN const CHAR16 *Path) {
  const CHAR16 *Key = Path;
  if(!Key) {
    return Path;
  }

  while (*Key != CHAR_NULL_TERM && *Key != L'.') {
    ++Key;
  }
  if(*Key == L'.') {
    ++Key;
  }
  return Key;
}

/**
Helper to dermine how many columns to print in a table.

@param[in] Attribs: list of table attributes, each representing a column

@retval UINTN Number of columns to print
**/
UINTN NumTableColumns(IN SHOW_TABLE_ATTRIB *Attribs) {
  UINTN Index = 0;
  if(!Attribs) {
    return Index;
  }

  for (Index = 0; Index < MAX_TABLE_COLUMNS; ++Index) {
    if(!Attribs->ColumnAttribs[Index].ColumnHeader) {
      break;
    }
  }
  return Index;
}

/**
Callback routine for printing out text tables.

@param[in] DataSetCtx: current data set node
@param[in] CurPath: path to current data set node in the form of: /sensorlist/dimm/sensor
@param[in] UserData: pointer to PRV_TABLE_INFO (defines user defined table/column attributes)
@param[in] ParentUserData: a string that represents the current table row (note, last node in branch
           is responsible for printing the row)

@retval CHAR16* that represents the table row.
@retval NULL on error
**/
VOID * ShowTextTableCb(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN VOID *UserData, IN VOID *ParentUserData) {
  PRV_TABLE_INFO *PrvTableInfo = (PRV_TABLE_INFO *)UserData;
  SHOW_TABLE_ATTRIB *Attribs;
  CHAR16 *TempCurPath = CatSPrint(CurPath, L".");
  UINT32 Index = 0;
  UINT32 Index2 = 0;
  CHAR16 *KeyVal;
  CHAR16 *T1 = CatSPrint(ParentUserData, L"");
  CHAR16 *T2;
  UINTN RowSizeInBytes;
  UINTN NumColumns = 0;

  if(NULL == UserData || NULL == T1 || NULL == TempCurPath) {
    return NULL;
  }

  Attribs = (SHOW_TABLE_ATTRIB *)PrvTableInfo->AllTableAttribs;
  NumColumns = NumTableColumns(Attribs);

  //Loop through all column attributes defined by the CLI cmd handler
  for (Index = 0; Index < NumColumns; ++Index) {
    //column attributes defines which key/value pairs to display in each column.
    //This is done by specifying a path to a key in the form of: /sensorlist/dimm/sensor.keyname.
    //TempCurPath represents the current node in path form with a '.' at the end, i.e. /sensorlist/dimm/sensor.
    if (0 == StrnCmp(TempCurPath, Attribs->ColumnAttribs[Index].ColumnDataSetPath, StrLen(TempCurPath))) {
      //Found a path in column attributes that points to our current node, try to retrieve the
      //associated value.
      GetKeyValueWideStr(DataSetCtx, ShowTextTableFindKeyInPath(Attribs->ColumnAttribs[Index].ColumnDataSetPath), &KeyVal, NULL);
      if(NULL == KeyVal) {
        continue;
      }
      //T1 contains cells in a row already processed, now add more memory for the next cell in the row
      RowSizeInBytes = StrSize(T1) + ((Attribs->ColumnAttribs[Index].ColumnWidth)*sizeof(CHAR16));
      if (NULL == (T1 = ReallocatePool(StrSize(T1), RowSizeInBytes, T1))) {
        return NULL;
      }
      //T2 points to end of the already processed row cells
      T2 = &T1[StrLen(T1)];
      //Always start cell with a space
      *T2++ = CHAR_WHITE_SPACE;
      //Add data to the row string (new column value)
      for (Index2 = 0; Index2 < Attribs->ColumnAttribs[Index].ColumnWidth - 2; ++Index2) {
        if (Index2 < StrLen(KeyVal)) {
          T2[Index2] = KeyVal[Index2];
        }
        else {
          T2[Index2] = CHAR_WHITE_SPACE;
        }
      }
      if((Index + 1) != NumColumns){
        T2[Index2] = TEXT_TABLE_DEFAULT_DELIM;
        ++Index2;
      }
      T2[Index2] = CHAR_NULL_TERM;
    }
  }

  //determine if the current node is the "printer node" (last node in branch)
  if (ShowTextTableIsPrinterNode(DataSetCtx, CurPath, PrvTableInfo)) {
    T2 = T1;
    while (*T2 != CHAR_NULL_TERM) {
      //print individual characters as string may contain format specifiers.
      Print(FORMAT_CHAR, *T2++);
    }
    Print(TEXT_NEW_LINE);
  }
  //return a string that represents the current row.
  return T1;
}

/**
The node that represents the last cell in a row is responsible for printing the entire row.
This is a helper to create a path in the form of /sensorlsit/dimm/sensor
that points to a particular "printer ndoe".

**/
CHAR16 * ShowTextTableGetPrinterNodePath(SHOW_TABLE_ATTRIB * TableAttribs) {
  UINT32 Index = 0;
  CHAR16 *PrinterNodePath = NULL;
  UINT32 DeepestLevel = 0;
  UINT32 TempLevelCount = 0;
  CHAR16 *TempPath;

  for (Index = 0; Index < MAX_TABLE_COLUMNS; ++Index) {
    if(!TableAttribs->ColumnAttribs[Index].ColumnDataSetPath) {
      break;
    }
    TempPath = (CHAR16*)TableAttribs->ColumnAttribs[Index].ColumnDataSetPath;
    TempLevelCount = 0;
    while (*TempPath != CHAR_NULL_TERM) {
      if (*TempPath == CHAR_PATH_DELIM) {
        ++TempLevelCount;
      }
      ++TempPath;
    }
    if (TempLevelCount > DeepestLevel) {
      PrinterNodePath = (CHAR16*)TableAttribs->ColumnAttribs[Index].ColumnDataSetPath;
    }
  }

  return PrinterNodePath;
}

/*
* Main entry point for displaying a hierarchical data set as a table
*/
VOID ShowTextTable(DATA_SET_CONTEXT *DataSetCtx, VOID * ShowAttribs) {

  UINT32 Index = 0;
  UINT32 Index2 = 0;
  SHOW_TABLE_ATTRIB *Attribs = (SHOW_TABLE_ATTRIB *)ShowAttribs;
  PRV_TABLE_INFO PrvTableInfo;
  CHAR16 *T1 = CatSPrint(NULL, L"");
  CHAR16 *T2 = NULL;
  UINTN RowSizeInBytes = 0;
  UINTN RowSizeInChars = 0;
  UINTN NumColumns = NumTableColumns(Attribs);

  if (NULL == T1) {
    return;
  }

  for (Index = 0; Index < NumColumns; ++Index) {
    //T1 contains cells in a row already processed, now add more memory for the next cell in the row
    RowSizeInBytes = StrSize(T1) + ((Attribs->ColumnAttribs[Index].ColumnWidth) * sizeof(CHAR16));
    if (NULL == (T1 = ReallocatePool(StrSize(T1), RowSizeInBytes, T1))) {
      return;
    }
    //T2 points to end of the already processed row cells
    T2 = &T1[StrLen(T1)];
    //Always start cell with a space
    *T2++ = CHAR_WHITE_SPACE;

    for (Index2 = 0; Index2 < Attribs->ColumnAttribs[Index].ColumnWidth - 2; ++Index2) {
      if (Index2 < StrLen(Attribs->ColumnAttribs[Index].ColumnHeader)) {
        T2[Index2] = Attribs->ColumnAttribs[Index].ColumnHeader[Index2];
      }
      else {
        T2[Index2] = CHAR_WHITE_SPACE;
      }
    }
    if ((Index + 1) != NumColumns) {
      T2[Index2] = TEXT_TABLE_DEFAULT_DELIM;
      ++Index2;
    }
    T2[Index2] = CHAR_NULL_TERM;
  }

  while (T1[RowSizeInChars] != CHAR_NULL_TERM) {
    Print(FORMAT_CHAR, T1[RowSizeInChars]);
    ++RowSizeInChars;
  }

  FreePool(T1);
  Print(TEXT_NEW_LINE);

  for (Index = 0; Index < RowSizeInChars; ++Index) {
    Print(TEXT_TABLE_HEADER_SEP);
  }
  Print(TEXT_NEW_LINE);

  PrvTableInfo.AllTableAttribs = Attribs;
  PrvTableInfo.PrinterNode = ShowTextTableGetPrinterNodePath(Attribs);
  RecurseDataSet(DataSetCtx, ShowTextTableCb, NULL, (VOID*)&PrvTableInfo, TRUE);
}

/*
* Helper to create xml indentions when printing based on depth of CurPath
*/
UINT32 ShowNvmXmlGetNvmXmlIdent(CHAR16 *CurPath) {
  UINT32 NumSpaces = 0;
  while (*CurPath != CHAR_NULL_TERM) {
    if(*CurPath++ == CHAR_PATH_DELIM) {
      ++NumSpaces;
    }
  }
  return NumSpaces;
}

/*
* Callback routine for printing out NVM XML.
* -Start by printing indentation whitespace based on depth of node in tree.
* -Print begining tag: <DataSetName>
* -ForEach KeyValue Pair:
* -  Print indentation whitespace
* -  Print <KeyName>KeyVal</KeyName>
* Note, closing tag for <DataSetName> happens in the ShowNvmlXmlChildrenDoneCb callback routine.
*/
VOID * ShowNvmlXmlCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;
  UINT32 Index = 0;
  UINT32 Ident = 0;

  Ident = ShowNvmXmlGetNvmXmlIdent(CurPath);
  for(Index = 0; Index < Ident; ++Index) {
    Print(NVM_XML_WHITESPACE_IDENT);
  }
  Print(NVM_XML_DATA_SET_TAG_START, GetDataSetName(DataSetCtx));

  while (NULL != (KvInfo = GetNextKey(DataSetCtx, KvInfo))) {
    GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
    for (Index = 0; Index < Ident+1; ++Index) {
      Print(NVM_XML_WHITESPACE_IDENT);
    }
    Print(NVM_XML_KEY_VAL_TAG, KvInfo->Key, Val, KvInfo->Key);
  }
  return NULL;
}

/*
* Children done callback routine for printing out NVM XML.
* -Start by printing indentation whitespace based on depth of node in tree.
* -Print closing tag: </DataSetName>
*/
VOID * ShowNvmlXmlChildrenDoneCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData) {

  UINT32 Index = 0;
  UINT32 Ident = 0;
  Ident = ShowNvmXmlGetNvmXmlIdent(CurPath);
  for (Index = 0; Index < Ident; ++Index) {
    Print(NVM_XML_WHITESPACE_IDENT);
  }
  Print(NVM_XML_DATA_SET_TAG_END, GetDataSetName(DataSetCtx));
  return NULL;
}

/*
* Main entry point for displaying a hierarchical data set as a table
*/
VOID ShowNvmXml(DATA_SET_CONTEXT *DataSetCtx, VOID * ShowAttribs) {
  RecurseDataSet(DataSetCtx, ShowNvmlXmlCb, ShowNvmlXmlChildrenDoneCb, NULL, TRUE);
}

/*
* Callback routine for printing out ESX XML.
* -Start by printing indentation whitespace based on depth of node in tree.
* -Print begining tag: <structure typeName="DataSetName">
* -ForEach KeyValue Pair:
* -   <field name = "Key"><string>"Value"</string></field>
* -Print closing tag: </structure>
*/
VOID * ShowEsxXmlCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;

  if (0 == GetKeyCount(DataSetCtx)) {
    return NULL;
  }

  Print(L"<structure typeName=\"" FORMAT_STR L"\">\n", GetDataSetName(DataSetCtx));
  while (NULL != (KvInfo = GetNextKey(DataSetCtx, KvInfo))) {
    GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
    Print(L"  <field name=\"" FORMAT_STR L"\"><string>" FORMAT_STR L"</string></field>\n", KvInfo->Key, Val);// KvInfo->Key);
  }
  Print(L"</structure>\n");
  return NULL;
}

/*
* Main entry point for displaying a hierarchical data set as ESX XML.
*/
VOID ShowEsxXml(DATA_SET_CONTEXT *DataSetCtx, VOID * ShowAttribs) {
  Print(ESX_XML_FILE_BEGIN);
  Print(ESX_XML_STRUCT_LIST_TAG_BEGIN);
  RecurseDataSet(DataSetCtx, ShowEsxXmlCb, NULL, NULL, TRUE);
  Print(ESX_XML_LIST_TAG_END);
  Print(ESX_XML_FILE_END);
}

/*
* Callback routine for printing out ESX XML (keyval pair struct list).
* -ForEach KeyValue Pair:
* -   <structure typeName="KeyValue">
* -   <field name = "Attribute Name"><string>MemoryCapacity</string></field><field name = "Value"><string>0 B</string></field>
* -   </structure>
*/
VOID * ShowEsxKeyValXmlCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;

  if (0 == GetKeyCount(DataSetCtx)) {
    return NULL;
  }

  while (NULL != (KvInfo = GetNextKey(DataSetCtx, KvInfo))) {
    GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
    Print(L"<structure typeName=\"KeyValue\">\n");
    Print(L"  <field name=\"Attribute Name\"><string>" FORMAT_STR L"</string></field><field name=\"Value\"><string>" FORMAT_STR L"</string></field>\n", KvInfo->Key, Val);
    Print(L"</structure>\n");
  }
  return NULL;
}

/*
* Main entry point for displaying a hierarchical data set as ESX XML (keyval pair struct list).
*/
VOID ShowEsxKeyValXml(DATA_SET_CONTEXT *DataSetCtx, VOID * ShowAttribs) {
  Print(ESX_XML_FILE_BEGIN);
  Print(ESX_XML_STRUCT_LIST_TAG_BEGIN);
  RecurseDataSet(DataSetCtx, ShowEsxKeyValXmlCb, NULL, NULL, TRUE);
  Print(ESX_XML_LIST_TAG_END);
  Print(ESX_XML_FILE_END);
}

/*
* Main entry point for displaying a hierarchical data set as text.
* Function determines if format is a list or table.
*/
VOID ShowText(DATA_SET_CONTEXT *DataSetCtx, SHOW_CMD_CONTEXT *ShowCtx) {
  if (ShowCtx->FormatTypeFlags.Flags.List) {
    ShowTextList(DataSetCtx, (SHOW_LIST_ATTRIB *)ShowCtx->FormatTypeAttribs);
  }
  else if (ShowCtx->FormatTypeFlags.Flags.Table) {
    ShowTextTable(DataSetCtx, (SHOW_TABLE_ATTRIB *)ShowCtx->FormatTypeAttribs);
  }
}

/*
* Main entry point for displaying a hierarchical data set as XML.
* Function determines which XML format to display.
*/
VOID ShowXml(DATA_SET_CONTEXT *DataSetCtx, SHOW_CMD_CONTEXT *ShowCtx) {
  DATA_SET_CONTEXT *SquashedDataSet;
  SquashedDataSet = SquashDataSet(DataSetCtx);

  if (ShowCtx->FormatTypeFlags.Flags.EsxCustom) {
    ShowEsxXml(SquashedDataSet, ShowCtx->FormatTypeAttribs);
  }
  else if (ShowCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    ShowEsxKeyValXml(SquashedDataSet, ShowCtx->FormatTypeAttribs);
  }
  else {
    ShowNvmXml(SquashedDataSet, ShowCtx->FormatTypeAttribs);
  }
  //SquashDataSet will return original Data set if it has no children
  //If it doesn't squash, don't free it.
  if(SquashedDataSet != DataSetCtx) {
    FreeDataSet(SquashedDataSet);
  }
}

/*
* Main entry point for displaying a hierarchical data.
* Currently supports: Nested lists as text (SHOW_LIST)
*                     Table as text (SHOW_TABLE)
*                     NVM XML (SHOW_NVM_XML)
*                     ESX XML (SHOW_ESX_XML)
*/
VOID ShowCmdData(DATA_SET_CONTEXT *DataSetCtx, SHOW_CMD_CONTEXT *ShowCtx) {

  if (NULL == ShowCtx) {
    NVDIMM_ERR("Null ShowCtx\n");
    return;
  }

  switch (ShowCtx->FormatType) {
    case TEXT:
      ShowText(DataSetCtx, ShowCtx);
      break;
    case XML:
      ShowXml(DataSetCtx, ShowCtx);
      break;
    default:
      NVDIMM_ERR("Show type not supported\n");
  }
}

/*
* Display error as text
*/
VOID ShowTextError(SHOW_CMD_CONTEXT *ShowCtx, EFI_STATUS CmdExitCode, CHAR16 *FullMsg) {
  Print(FullMsg);
}

/*
* Display error as XML
*/
VOID ShowXmlError(SHOW_CMD_CONTEXT *ShowCtx, EFI_STATUS CmdExitCode, CHAR16 *FullMsg) {
  if (ShowCtx->FormatTypeFlags.Flags.EsxCustom || ShowCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    Print(L"ERROR: %ls\n", FullMsg);
  }
  else {
    Print(L"<Error Type = \"%d\">\n", CmdExitCode);
    Print(L"%ls\n", FullMsg);
    Print(L"<Error/>\n");
  }
}

/*
* Main entry point for displaying an error.
*/
VOID ShowCmdError(SHOW_CMD_CONTEXT *ShowCtx, EFI_STATUS CmdExitCode, CHAR16* Msg, ...) {
  CHAR16 *FullMsg = NULL;
  VA_LIST Marker;

  if (NULL == ShowCtx) {
    Print(L"Null ShowCtx\n");
    return;
  }

  VA_START(Marker, Msg);
  FullMsg = CatVSPrint(NULL, Msg, Marker);
  VA_END(Marker);

  switch (ShowCtx->FormatType) {
  case TEXT:
    ShowTextError(ShowCtx, CmdExitCode, FullMsg);
    break;
  case XML:
    ShowXmlError(ShowCtx, CmdExitCode, FullMsg);
    break;
  default:
    Print(L"Show type not supported\n");
  }

  if (FullMsg) {
    FreePool(FullMsg);
  }
}