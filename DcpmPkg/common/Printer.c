/*
* Copyright (c) 2018, Intel Corporation.
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "Printer.h"
#include <Debug.h>
#include <NvmDimmCli.h>
#include <Library/BaseMemoryLib.h>
#include <Common.h>

#define EXPAND_STR_MAX                    1024
#define ESX_XML_FILE_BEGIN                L"<?xml version=\"1.0\"?><output xmlns=\"http://www.vmware.com/Products/ESX/5.0/esxcli/\">"
#define ESX_XML_FILE_END                  L"</output>\n"
#define ESX_XML_STRUCT_LIST_TAG_BEGIN     L"<list type = \"structure\">\n"
#define ESX_XML_SIMPLE_STR_TAG_END        L"]]></string>"
#define ESX_XML_SIMPLE_STR_TAG_BEGIN      L"<string><![CDATA["
#define ESX_XML_LIST_TAG_END              L"</list>\n"
#define ESX_XML_LIST_STRING_BEGIN         L"<list type=\"string\">"

#define NVM_XML_DATA_SET_TAG_START        L"<" FORMAT_STR L">\n"
#define NVM_XML_KEY_VAL_TAG               L"<" FORMAT_STR L">" FORMAT_STR L"</" FORMAT_STR L">\n"
#define NVM_XML_DATA_SET_TAG_END          L"</" FORMAT_STR L">\n"
#define NVM_XML_WHITESPACE_IDENT          L" "
#define NVM_XML_RESULT_BEGIN              L"<Results>\n<Result>\n"
#define MVM_XML_RESULT_END                L"</Result>\n</Results>\n"

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
#define CELL_EXTRA_CHARS                  2 //1 for leading whitespace and 1 for terminating pipe

typedef enum {
  PRINT_TEXT,
  PRINT_BASIC_XML,
  PRINT_XML
}PRINT_MODE;

typedef struct _PRV_TABLE_INFO {
  PRINTER_TABLE_ATTRIB *AllTableAttribs;
  PRINTER_TABLE_ATTRIB *ModifiedTableAttribs;
  CHAR16 *CurrentPath;
  CHAR16 *PrinterNode;
}PRV_TABLE_INFO;

/**
Helper for iterating through buffered object list
**/
#define BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, ListHead) \
  for(Entry = (ListHead)->ForwardLink, NextEntry = Entry->ForwardLink; \
      Entry != (ListHead); \
      Entry = NextEntry, NextEntry = Entry->ForwardLink \
     )

#define PRINT_ONLY_DATA_SETS(pPrintCtx) \
  (XML == pPrintCtx->FormatType && 1 == pPrintCtx->BufferedDataSetCnt && EFI_SUCCESS == pPrintCtx->BufferedObjectLastError) \


/**
Helper for finding the list attribute associated with a particular data set

@param[in] LevelType: Name of the data set that represents a particular level in the hierarchy
@param[in] Attribs: Contains a LIST_LEVEL_ATTRIB for each level in the hierarchy

@retval LIST_LEVEL_ATTRIB* that represents the LevelType
@retval NULL if not found
**/
static LIST_LEVEL_ATTRIB *TextListFindAttrib(IN CHAR16 *LevelType, IN PRINTER_LIST_ATTRIB *Attribs) {
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
static CHAR16 *TextListExpandStr(IN DATA_SET_CONTEXT *DataSetCtx, IN const CHAR16 *Str) {
  CHAR16 *Val = NULL;
  CHAR16 *ExpandedStr = AllocateZeroPool(EXPAND_STR_MAX);
  const CHAR16 *OriginalStrTmpBegin = Str;
  const CHAR16 *OriginalStrTmpEnd = OriginalStrTmpBegin;
  CHAR16 *ExpandedStrTmp = ExpandedStr;
  CHAR16 *MacroKeyName = NULL;
  UINTN AppendSize = 0;
  UINTN FreeSize = EXPAND_STR_MAX - sizeof(CHAR16); //minus one for NULL terminator

  if (NULL == ExpandedStr || NULL == Str) {
    goto Finish;
  }

  while (*OriginalStrTmpBegin != CHAR_NULL_TERM) {
    AppendSize = 0;
    //Found begining of $(key) identifier
    if (OriginalStrTmpBegin[0] == L'$' && OriginalStrTmpBegin[1] == L'(') {
      //Find end of $(key) identifier
      do {
        ++OriginalStrTmpEnd;
      }
      //Find end of $(key) identifier or end of str, which ever comes first
      while(*OriginalStrTmpEnd != CHAR_NULL_TERM && *OriginalStrTmpEnd != L')');
      //OriginalStrTmpBegin points to begining of $(key) identifier, OriginalStrTmpEnd points to the end
      //allocate enough memory to copy the string that sits between pointers
      //plus 1 char worth of mem for NULL term to make it a string
      MacroKeyName = AllocateZeroPool(((UINTN)OriginalStrTmpEnd - (UINTN)OriginalStrTmpBegin) + sizeof(CHAR16));
      if (NULL == MacroKeyName) {
        NVDIMM_CRIT("AllocateZeroPool returned NULL\n");
        goto Finish;
      }
      CopyMem(MacroKeyName, OriginalStrTmpBegin, ((UINTN)OriginalStrTmpEnd - (UINTN)OriginalStrTmpBegin));
      //Get the value associated with the key identifier
      //TempKey[2] - skips past "$(" prefix
      GetKeyValueWideStr(DataSetCtx, (CHAR16*)&MacroKeyName[2], &Val, NULL);
      if (NULL != Val) {
        //don't copy NULL terminator
        AppendSize = StrSize(Val) - sizeof(CHAR16);
        if(0 > (INTN)(FreeSize - AppendSize))
          goto Finish;
        //don't use str copy APIs as Val may contain format specifiers
        CopyMem(ExpandedStrTmp, Val, AppendSize);
        ExpandedStrTmp += StrLen(Val);
      }
      else {
        //don't copy NULL terminator
        AppendSize = StrSize(MacroKeyName) - sizeof(CHAR16);
        if (0 > (INTN)(FreeSize - AppendSize)) {
          goto Finish;
        }
        //don't use str copy APIs as Val may contain format specifiers
        CopyMem(ExpandedStrTmp, MacroKeyName, AppendSize);
        ExpandedStrTmp += StrLen(MacroKeyName);
      }
      FREE_POOL_SAFE(MacroKeyName);
      //Advance OriginalStrTmpEnd past the last part of the $(key) identifier
      if(*OriginalStrTmpEnd == L')') {
        ++OriginalStrTmpEnd;
      }
      //Reset OriginalStrTmpBegin
      OriginalStrTmpBegin = OriginalStrTmpEnd;
    }
    else {
      AppendSize = sizeof(*OriginalStrTmpBegin);
      if (0 > (INTN)(FreeSize - AppendSize)) {
        goto Finish;
      }
      *ExpandedStrTmp = *OriginalStrTmpBegin;
      ++ExpandedStrTmp;
      ++OriginalStrTmpBegin;
      ++OriginalStrTmpEnd;
    }

    FreeSize -= AppendSize;
  }
 Finish:
  FREE_POOL_SAFE(MacroKeyName);
  return ExpandedStr;
}

/*
* Helper to create xml indentions when printing based on depth of CurPath
*/
static UINT32 NvmXmlGetNvmXmlIdent(CHAR16 *CurPath) {
  UINT32 NumSpaces = 0;
  while (*CurPath != CHAR_NULL_TERM) {
    if (*CurPath++ == CHAR_PATH_DELIM) {
      ++NumSpaces;
    }
  }
  return NumSpaces;
}

/**
Helper to create text list indentions when printing based on depth of CurPath
**/
static INT32 TextListGetListLevel(IN CHAR16 *CurPath) {
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

@retval UINT32 Whitespace count
**/
static UINT32 TextListGetPrintIdentCount(IN CHAR16 *CurPath) {
  return 0; //currently no default indentation
}

/**
Helper to create text list indentions when printing based on a count value

@param[in] Count: Affects how many whitespaces to print

**/
static VOID TextListPrintIdent(IN UINT32 Count) {
  while (Count--) {
    Print(TEXT_LIST_WHITESPACE_IDENT);
  }
}

/**
Helper TextListCb to determine if a key should be displayed or not
**/
static BOOLEAN TextListIgnoreKey(IN CHAR16 *IgnoreKeyList[], IN UINT32 IgnoreListSz, IN CHAR16 *Key) {
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
@param[in] UserData: Represents a PRINTER_LIST_ATTRIB struct
@param[in] ParentUserData: Not used in this context

@retval VOID * Always returns NULL
**/
static VOID * TextListCb(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN VOID *UserData, IN VOID *ParentUserData) {
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;
  PRINTER_LIST_ATTRIB *Attribs = (PRINTER_LIST_ATTRIB *)UserData;
  LIST_LEVEL_ATTRIB *LevelAttribs = NULL;
  CHAR16 *Header = NULL;
  CHAR16 **Toks = NULL;
  UINT32 NumToks = 0;
  BOOLEAN ListHeaderPrinted = FALSE;
  UINT32 IdentCount = TextListGetPrintIdentCount(CurPath);
  //Example:CurPath == /sensorlist/dimm
  //Group level is base 0, there value will be 1
  INT32 ListGroupLevel = TextListGetListLevel(CurPath);

  //if CMD is overriding default text list attributes
  if(Attribs) {
    //extract the list attribute associated with the current data set node
    LevelAttribs = TextListFindAttrib(GetDataSetName(DataSetCtx), Attribs);
    //if a header attribute is specified
    if (LevelAttribs && LevelAttribs->LevelHeader) {
      //overriden header may contain key/val "macros" in the form of L"---DimmId=$(DimmId)---"
      //where $(DimmId) is the name of a key within a particular data set.
      //TextListExpandStr will replace macros with associated key value.
      if (NULL != (Header = TextListExpandStr(DataSetCtx, LevelAttribs->LevelHeader))) {
        //print whitespace indentation based on depth of data set node
        TextListPrintIdent(IdentCount);
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
    if (!TextListIgnoreKey(Toks, NumToks, KvInfo->Key)) {
      //retrieve the value in wide string form
      GetKeyValueWideStr(DataSetCtx, KvInfo->Key, &Val, NULL);
      //print a whitespace indentation
      TextListPrintIdent(IdentCount);
      //by default 1). the data set's group header is the first key/val pair found
      //2). the first group in the list does not have a header
      if (!ListHeaderPrinted && ListGroupLevel > 0) {
        Print(TEXT_LIST_HEADER);
      }
      else {
        TextListPrintIdent(IdentCount);
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
@param[in] PrinterListAttribs: User specified attributes that defines how a list should be printed
**/
static VOID PrintTextList(IN DATA_SET_CONTEXT *DataSetCtx, IN PRINTER_LIST_ATTRIB *PrinterListAttribs) {
  RecurseDataSet(DataSetCtx, TextListCb, NULL, (VOID*)PrinterListAttribs, TRUE);
}

/**
The node that represents the last cell in a row is responsible for printing the entire row.
This is a helper to determine if a particular node is the "printer node".

@param[in] DataSetCtx: Represents a data set that contains key/val pairs
@param[in] CurPath: Represents a hierarchical path of data sets in the form of /sensorlist/dimm/sensor
@param[in] PrvTableInfo: Contains information related to a table

@retval BOOLEAN TRUE if CurPath points to the designated "printer node" else FALSE
**/
static BOOLEAN TextTableIsPrinterNode(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN PRV_TABLE_INFO *PrvTableInfo) {
  CHAR16 *TmpStr = CatSPrint(CurPath,L".");
  BOOLEAN IsPrinter = FALSE;

  if (NULL == TmpStr) {
    NVDIMM_CRIT("CatSPrint returned NULL\n");
    goto Finish;
  }

  if(NULL == PrvTableInfo) {
    goto Finish;
  }

  IsPrinter = (0 == StrnCmp(TmpStr, PrvTableInfo->PrinterNode, StrLen(TmpStr)));

Finish:
  FREE_POOL_SAFE(TmpStr);
  return IsPrinter;
}

/**
This routine takes a Path and returns a pointer to the keyname.

@param[in] Path: In the form of /sensorlist/dimm/sensor.keyname.

@retval CHAR16 * Pointer to keyname else NULL terminator if keyname not found.
**/
static const CHAR16 *TextTableFindKeyInPath(IN const CHAR16 *Path) {
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
static UINTN NumTableColumns(IN PRINTER_TABLE_ATTRIB *Attribs) {
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
static VOID * TextTableCb(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN VOID *UserData, IN VOID *ParentUserData) {
  PRV_TABLE_INFO *PrvTableInfo = (PRV_TABLE_INFO *)UserData;
  PRINTER_TABLE_ATTRIB *Attribs;
  CHAR16 *TempCurPath = CatSPrint(CurPath, L".");
  UINT32 ColumnIndex = 0;
  UINT32 CellValueIndex = 0;
  CHAR16 *KeyVal;
  CHAR16 *CurRowText = CatSPrint(ParentUserData, L"");
  CHAR16 *EndOfRowText;
  UINTN RowSizeInBytes;
  UINTN NumColumns = 0;
  UINTN KeyValSize = 0;
  UINTN MaxCellChars = 0;
  CHAR16 *TempRowText = NULL;
  BOOLEAN LastNodeInBranch = FALSE;
  CHAR16 *EmptyCell = L"X";

  if(NULL == UserData || NULL == CurRowText || NULL == TempCurPath) {
    goto Finish;
  }

  LastNodeInBranch = TextTableIsPrinterNode(DataSetCtx, CurPath, PrvTableInfo);

  Attribs = (PRINTER_TABLE_ATTRIB *)PrvTableInfo->AllTableAttribs;
  NumColumns = NumTableColumns(Attribs);

  //Loop through all column attributes defined by the CLI cmd handler
  for (ColumnIndex = 0; ColumnIndex < NumColumns; ++ColumnIndex) {
    //column attributes defines which key/value pairs to display in each column.
    //This is done by specifying a path to a key in the form of: /sensorlist/dimm/sensor.keyname.
    //TempCurPath represents the current node in path form with a '.' at the end, i.e. /sensorlist/dimm/sensor.
    if (0 == StrnCmp(TempCurPath, Attribs->ColumnAttribs[ColumnIndex].ColumnDataSetPath, StrLen(TempCurPath))) {
      //Found a path in column attributes that points to our current node, try to retrieve the
      //associated value.
      GetKeyValueWideStr(DataSetCtx, TextTableFindKeyInPath(Attribs->ColumnAttribs[ColumnIndex].ColumnDataSetPath), &KeyVal, NULL);
      if(NULL == KeyVal) {
        KeyVal = EmptyCell;
      }
      //if we are at the last column, let the value wrap
      if (LastNodeInBranch && ColumnIndex == (NumColumns-1)) {
        MaxCellChars = StrLen(KeyVal);
        RowSizeInBytes = StrSize(CurRowText) + ((MaxCellChars + CELL_EXTRA_CHARS) * sizeof(CHAR16));
      }
      //CurRowText contains cells in a row already processed, now add more memory for the next cell in the row
      //if we are not at the last row, restrict the size of the cell as specified in table attributes
      else {
        MaxCellChars = Attribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen;
        RowSizeInBytes = StrSize(CurRowText) + ((MaxCellChars + CELL_EXTRA_CHARS) * sizeof(CHAR16));
      }
      
      if (NULL == (CurRowText = ReallocatePool(StrSize(CurRowText), RowSizeInBytes, CurRowText))) {
        goto Finish;
      }
      //Make EndOfRowText point to end of the already processed row
      EndOfRowText = &CurRowText[StrLen(CurRowText)];
      //Always start cell with a space
      *EndOfRowText++ = CHAR_WHITE_SPACE;
      //Number of chars in the new column value
      KeyValSize = StrLen(KeyVal);
      //Add data to the row string (new column value)
      for (CellValueIndex = 0; CellValueIndex < MaxCellChars; ++CellValueIndex) {
        //fill the column cell with the key value
        if (CellValueIndex < KeyValSize) {
          EndOfRowText[CellValueIndex] = KeyVal[CellValueIndex];
        }
        //pad the rest of the cell with whitespaces (up to the specified column max width)
        else {
          EndOfRowText[CellValueIndex] = CHAR_WHITE_SPACE;
        }
      }

      if((ColumnIndex + 1) != NumColumns){
        EndOfRowText[CellValueIndex] = TEXT_TABLE_DEFAULT_DELIM;
        ++CellValueIndex;
      }
      EndOfRowText[CellValueIndex] = CHAR_NULL_TERM;
    }
  }

  //determine if the current node is the "printer node" (last node in branch)
  if (LastNodeInBranch) {
    TempRowText = CurRowText;
    while (*TempRowText != CHAR_NULL_TERM) {
      //print individual characters as string may contain format specifiers.
      Print(FORMAT_CHAR, *TempRowText++);
    }
    Print(TEXT_NEW_LINE);
  }

Finish:
  FREE_POOL_SAFE(TempCurPath);
  //return a string that represents the current row.
  return CurRowText;
}

/**
Callback routine for determining max column widths of a text table

@param[in] DataSetCtx: current data set node
@param[in] CurPath: path to current data set node in the form of: /sensorlist/dimm/sensor
@param[in] UserData: pointer to PRV_TABLE_INFO (defines user defined table/column attributes)
@param[in] ParentUserData: a string that represents the current table row (note, last node in branch
           is responsible for printing the row)

@retval CHAR16* that represents the table row.
@retval NULL on error
**/
static VOID * CalculateTextTableDimensionCb(IN DATA_SET_CONTEXT *DataSetCtx, IN CHAR16 *CurPath, IN VOID *UserData, IN VOID *ParentUserData) {
  PRV_TABLE_INFO *PrvTableInfo = (PRV_TABLE_INFO *)UserData;
  PRINTER_TABLE_ATTRIB *Attribs;
  PRINTER_TABLE_ATTRIB *ModifiedAttribs;
  CHAR16 *TempCurPath = CatSPrint(CurPath, L".");
  UINT32 ColumnIndex = 0;
  UINTN NumColumns = 0;
  UINTN MaxCellChars = 0;
  CHAR16 *EmptyCell = L"X";
  CHAR16 *KeyVal;

  if (NULL == UserData || NULL == TempCurPath) {
    goto Finish;
  }


  Attribs = (PRINTER_TABLE_ATTRIB *)PrvTableInfo->AllTableAttribs;
  ModifiedAttribs = (PRINTER_TABLE_ATTRIB *)PrvTableInfo->ModifiedTableAttribs;
  NumColumns = NumTableColumns(Attribs);

  //Loop through all column attributes defined by the CLI cmd handler
  for (ColumnIndex = 0; ColumnIndex < NumColumns; ++ColumnIndex) {
    //column attributes defines which key/value pairs to display in each column.
    //This is done by specifying a path to a key in the form of: /sensorlist/dimm/sensor.keyname.
    //TempCurPath represents the current node in path form with a '.' at the end, i.e. /sensorlist/dimm/sensor.
    if (0 == StrnCmp(TempCurPath, Attribs->ColumnAttribs[ColumnIndex].ColumnDataSetPath, StrLen(TempCurPath))) {
      //Found a path in column attributes that points to our current node, try to retrieve the
      //associated value.
      GetKeyValueWideStr(DataSetCtx, TextTableFindKeyInPath(Attribs->ColumnAttribs[ColumnIndex].ColumnDataSetPath), &KeyVal, NULL);
      if (NULL == KeyVal) {
        KeyVal = EmptyCell;
      }
      MaxCellChars = StrLen(KeyVal) + 1;

      if (MaxCellChars < Attribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen && MaxCellChars > ModifiedAttribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen) {
        ModifiedAttribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen = (UINT32)MaxCellChars;
      }
      //reset to original max width specified by the attributes table
      else if (MaxCellChars >= Attribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen) {
        ModifiedAttribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen = Attribs->ColumnAttribs[ColumnIndex].ColumnMaxStrLen;
      }
    }
  }

Finish:
  FREE_POOL_SAFE(TempCurPath);
  return NULL;
}


/**
The node that represents the last cell in a row is responsible for printing the entire row.
This is a helper to create a path in the form of /sensorlist/dimm/sensor
that points to a particular "printer node".

**/
static CHAR16 * TextTableGetPrinterNodePath(PRINTER_TABLE_ATTRIB * TableAttribs) {
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
VOID PrintDataSetAsTextTable(DATA_SET_CONTEXT *DataSetCtx, PRINTER_TABLE_ATTRIB * Attribs) {

  UINT32 Index = 0;
  UINT32 Index2 = 0;
  PRV_TABLE_INFO PrvTableInfo;
  CHAR16 *TableHeaderStart = NULL;
  CHAR16 *TableHeaderEnd = NULL;
  UINTN RowSizeInBytes = 0;
  UINTN RowSizeInChars = 0;
  UINTN NumColumns = NumTableColumns(Attribs);

  if (NULL == Attribs) {
    NVDIMM_CRIT("CMDs must specify a PRINTER_TABLE_ATTRIB when displaying text tables\n");
    return;
  }

  TableHeaderStart = CatSPrint(NULL, L"");
  if (NULL == TableHeaderStart) {
    return;
  }

  //Loop generates a string that represents the table's header
  for (Index = 0; Index < NumColumns; ++Index) {
    //TableRowStart contains cells in a row already processed, now add more memory for the next cell in the row
    RowSizeInBytes = StrSize(TableHeaderStart) + ((Attribs->ColumnAttribs[Index].ColumnMaxStrLen + CELL_EXTRA_CHARS) * sizeof(CHAR16)); //+CELL_EXTRA_CHARS on ColumnWidth to accomadate whitespace and pipe
    if (NULL == (TableHeaderStart = ReallocatePool(StrSize(TableHeaderStart), RowSizeInBytes, TableHeaderStart))) {
      return;
    }
    //TableHeaderEnd points to end of the already processed row cells
    TableHeaderEnd = &TableHeaderStart[StrLen(TableHeaderStart)];
    //Always start cell with a space
    *TableHeaderEnd++ = CHAR_WHITE_SPACE;

    for (Index2 = 0; Index2 < Attribs->ColumnAttribs[Index].ColumnMaxStrLen; ++Index2) {
      if (Index2 < StrLen(Attribs->ColumnAttribs[Index].ColumnHeader)) {
        TableHeaderEnd[Index2] = Attribs->ColumnAttribs[Index].ColumnHeader[Index2];
      }
      else {
        TableHeaderEnd[Index2] = CHAR_WHITE_SPACE;
      }
    }
    if ((Index + 1) != NumColumns) {
      TableHeaderEnd[Index2] = TEXT_TABLE_DEFAULT_DELIM;
      ++Index2;
    }
    TableHeaderEnd[Index2] = CHAR_NULL_TERM;
  }

  //Print the header
  while (TableHeaderStart[RowSizeInChars] != CHAR_NULL_TERM) {
    Print(FORMAT_CHAR, TableHeaderStart[RowSizeInChars]);
    ++RowSizeInChars;
  }

  FreePool(TableHeaderStart);
  Print(TEXT_NEW_LINE);

  //Print the header/body seperator
  for (Index = 0; Index < RowSizeInChars; ++Index) {
    Print(TEXT_TABLE_HEADER_SEP);
  }
  Print(TEXT_NEW_LINE);

  PrvTableInfo.AllTableAttribs = Attribs;
  PrvTableInfo.PrinterNode = TextTableGetPrinterNodePath(Attribs);
  //Print the body of the table
  RecurseDataSet(DataSetCtx, TextTableCb, NULL, (VOID*)&PrvTableInfo, TRUE);
}

/*
* Determine column widths of a text table
*/
VOID CalculateTextTableDimensions(DATA_SET_CONTEXT *DataSetCtx, PRINTER_TABLE_ATTRIB * Attribs, PRINTER_TABLE_ATTRIB * ModifiedAttribs) {

  UINT32 Index = 0;
  PRV_TABLE_INFO PrvTableInfo;
  UINTN NumColumns = NumTableColumns(Attribs);
  UINTN ColumnHeaderStrLen = 0;

  if (NULL == Attribs || NULL == ModifiedAttribs) {
    NVDIMM_CRIT("CMDs must specify a PRINTER_TABLE_ATTRIB when displaying text tables\n");
    return;
  }

  //calculate max column header widths
  for (Index = 0; Index < NumColumns; ++Index) {
    ColumnHeaderStrLen = StrLen(Attribs->ColumnAttribs[Index].ColumnHeader) + 1;
    if (ColumnHeaderStrLen < Attribs->ColumnAttribs[Index].ColumnMaxStrLen) {
      ModifiedAttribs->ColumnAttribs[Index].ColumnMaxStrLen = (UINT32)ColumnHeaderStrLen;
    }
  }

  PrvTableInfo.AllTableAttribs = Attribs;
  PrvTableInfo.ModifiedTableAttribs = ModifiedAttribs;
  PrvTableInfo.PrinterNode = TextTableGetPrinterNodePath(Attribs);
  //calculate max column widths based on table content
  RecurseDataSet(DataSetCtx, CalculateTextTableDimensionCb, NULL, (VOID*)&PrvTableInfo, TRUE);
}

/*
* Callback routine for printing out NVM XML.
* -Start by printing indentation whitespace based on depth of node in tree.
* -Print begining tag: <DataSetName>
* -ForEach KeyValue Pair:
* -  Print indentation whitespace
* -  Print <KeyName>KeyVal</KeyName>
* Note, closing tag for <DataSetName> happens in the NvmlXmlChildrenDoneCb callback routine.
*/
static VOID * NvmlXmlCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
  KEY_VAL_INFO *KvInfo = NULL;
  CHAR16 *Val = NULL;
  UINT32 Index = 0;
  UINT32 Ident = 0;

  Ident = NvmXmlGetNvmXmlIdent(CurPath);
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
static VOID * NvmlXmlChildrenDoneCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData) {

  UINT32 Index = 0;
  UINT32 Ident = 0;
  Ident = NvmXmlGetNvmXmlIdent(CurPath);
  for (Index = 0; Index < Ident; ++Index) {
    Print(NVM_XML_WHITESPACE_IDENT);
  }
  Print(NVM_XML_DATA_SET_TAG_END, GetDataSetName(DataSetCtx));
  return NULL;
}

/*
* Main entry point for displaying a hierarchical data set as a table
*/
static VOID PrintNvmXml(DATA_SET_CONTEXT *DataSetCtx) {
  RecurseDataSet(DataSetCtx, NvmlXmlCb, NvmlXmlChildrenDoneCb, NULL, TRUE);
}

/*
* Callback routine for printing out ESX XML.
* -Start by printing indentation whitespace based on depth of node in tree.
* -Print begining tag: <structure typeName="DataSetName">
* -ForEach KeyValue Pair:
* -   <field name = "Key"><string>"Value"</string></field>
* -Print closing tag: </structure>
*/
static VOID * EsxXmlCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
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
static VOID PrintEsxXml(DATA_SET_CONTEXT *DataSetCtx) {
  Print(ESX_XML_FILE_BEGIN);
  Print(ESX_XML_STRUCT_LIST_TAG_BEGIN);
  RecurseDataSet(DataSetCtx, EsxXmlCb, NULL, NULL, TRUE);
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
static VOID * EsxKeyValXmlCb(DATA_SET_CONTEXT *DataSetCtx, CHAR16 *CurPath, VOID *UserData, VOID *ParentUserData) {
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
static VOID PrintEsxKeyValXml(DATA_SET_CONTEXT *DataSetCtx) {
  Print(ESX_XML_FILE_BEGIN);
  Print(ESX_XML_STRUCT_LIST_TAG_BEGIN);
  RecurseDataSet(DataSetCtx, EsxKeyValXmlCb, NULL, NULL, TRUE);
  Print(ESX_XML_LIST_TAG_END);
  Print(ESX_XML_FILE_END);
}

/*
* Main entry point for displaying a hierarchical data set as text.
* Function determines if format is a list or table.
*/
static VOID PrintAsText(DATA_SET_CONTEXT *DataSetCtx, PRINT_CONTEXT *PrintCtx) {
  PRINTER_DATA_SET_ATTRIBS *Attribs = (PRINTER_DATA_SET_ATTRIBS *)GetDataSetUserData(DataSetCtx);
  PRINTER_LIST_ATTRIB *ListAttribs = NULL;
  PRINTER_TABLE_ATTRIB *TableAttribs = NULL;
  PRINTER_TABLE_ATTRIB *ModifiedTableAttribs = (PRINTER_TABLE_ATTRIB *)AllocateZeroPool(sizeof(PRINTER_TABLE_ATTRIB));

  if (NULL == ModifiedTableAttribs) {
    NVDIMM_CRIT("AllocateZeroPool returned NULL\n");
    return;
  }

  if (PrintCtx->FormatTypeFlags.Flags.List) {
    if (Attribs) {
      ListAttribs = Attribs->pListAttribs;
    }
    PrintTextList(DataSetCtx, ListAttribs);
  }
  else if (PrintCtx->FormatTypeFlags.Flags.Table) {
    if (Attribs) {
      TableAttribs = Attribs->pTableAttribs;
      if(TableAttribs) {
        CopyMem_S(ModifiedTableAttribs, sizeof(PRINTER_TABLE_ATTRIB), TableAttribs, sizeof(PRINTER_TABLE_ATTRIB));
      }
    }
    else
    {
      NVDIMM_CRIT("CMDs must specify a PRINTER_TABLE_ATTRIB when displaying text tables\n");
      goto Finish;
    }
    CalculateTextTableDimensions(DataSetCtx, TableAttribs, ModifiedTableAttribs);
    PrintDataSetAsTextTable(DataSetCtx, ModifiedTableAttribs);
  }
Finish:
  FREE_POOL_SAFE(ModifiedTableAttribs);
}

/*
* Main entry point for displaying a hierarchical data set as XML.
* Function determines which XML format to display.
*/
static VOID PrintAsXml(DATA_SET_CONTEXT *DataSetCtx, PRINT_CONTEXT *PrintCtx) {
  DATA_SET_CONTEXT *SquashedDataSet;
  SquashedDataSet = SquashDataSet(DataSetCtx);

  if (PrintCtx->FormatTypeFlags.Flags.EsxCustom) {
    PrintEsxXml(SquashedDataSet);
  }
  else if (PrintCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    PrintEsxKeyValXml(SquashedDataSet);
  }
  else {
    PrintNvmXml(SquashedDataSet);
  }
  //SquashDataSet will return original Data set if it has no children
  //If it doesn't squash, don't free it.
  if(SquashedDataSet != DataSetCtx) {
    FreeDataSet(SquashedDataSet);
  }
}

/*
* Print to stdout and ensure newline
*/
static VOID PrintTextWithNewLine(CHAR16 *Msg) {
  UINTN MsgLen = 0;

  if (NULL == Msg) {
    return;
  }
  MsgLen = StrLen(Msg);
  Print(Msg);
  if (MsgLen && Msg[MsgLen - 1] != L'\n') {
    Print(L"\n");
  }
}

/*
* Display begining of XML error
*/
static VOID PrintXmlStartErrorTag(PRINT_CONTEXT *PrintCtx, EFI_STATUS CmdExitCode) {
  if (PrintCtx->FormatTypeFlags.Flags.EsxCustom || PrintCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    Print(L"ERROR: ");
  }
  else {
#ifdef OS_BUILD
    CmdExitCode = UefiToOsReturnCode(CmdExitCode);
#endif
    Print(L"<Error Type = \"%d\">\n", CmdExitCode);
  }
}

/*
* Display ending of XML error
*/
static VOID PrintXmlEndErrorTag(PRINT_CONTEXT *PrintCtx, EFI_STATUS CmdExitCode) {
  if (!PrintCtx->FormatTypeFlags.Flags.EsxCustom && !PrintCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    Print(L"</Error>\n");
  }
}

/*
* Display begining of XML success msg
*/
static VOID PrintXmlStartSuccessTag(PRINT_CONTEXT *PrintCtx, EFI_STATUS CmdExitCode) {
  if (PrintCtx->FormatTypeFlags.Flags.EsxCustom || PrintCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    Print(ESX_XML_FILE_BEGIN);
    Print(ESX_XML_SIMPLE_STR_TAG_BEGIN);
  }
  else {
    Print(NVM_XML_RESULT_BEGIN);
  }
}

/*
* Display ending of XML success msg
*/
static VOID PrintXmlEndSuccessTag(PRINT_CONTEXT *PrintCtx, EFI_STATUS CmdExitCode) {
  if (PrintCtx->FormatTypeFlags.Flags.EsxCustom || PrintCtx->FormatTypeFlags.Flags.EsxKeyVal) {
    Print(ESX_XML_SIMPLE_STR_TAG_END);
    Print(ESX_XML_FILE_END);
  }
  else {
    Print(MVM_XML_RESULT_END);
  }
}

/*
* Helper that creates a message out of a COMMAND_STATUS object.
*/
static EFI_STATUS CreateCmdStatusMsg(CHAR16 **ppMsg, CHAR16 *pStatusMessage, CHAR16 *pStatusPreposition, COMMAND_STATUS *pCommandStatus) {
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  UINT8 DimmIdentifier = 0;
  BOOLEAN ObjectIdNumberPreferred = FALSE;

  ReturnCode = GetDimmIdentifierPreference(&DimmIdentifier);
  if (EFI_ERROR(ReturnCode)) {
    goto Finish;
  }

  ObjectIdNumberPreferred = DimmIdentifier == DISPLAY_DIMM_ID_HANDLE;

  ReturnCode = CreateCommandStatusString(gNvmDimmCliHiiHandle, pStatusMessage, pStatusPreposition, pCommandStatus,
    ObjectIdNumberPreferred, ppMsg);

Finish:
  return ReturnCode;
}

/*
* Create a printer context
*/
EFI_STATUS PrinterCreateCtx(
  OUT    PRINT_CONTEXT **ppPrintCtx
)
{
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (NULL == ppPrintCtx) {
    return EFI_INVALID_PARAMETER;
  }

  if (NULL == (*ppPrintCtx = (PRINT_CONTEXT*)AllocateZeroPool(sizeof(PRINT_CONTEXT)))) {
    ReturnCode = EFI_OUT_OF_RESOURCES;
    goto Finish;
  }
  InitializeListHead(&((*ppPrintCtx)->BufferedObjectList));
  InitializeListHead(&((*ppPrintCtx)->DataSetLookup));
  InitializeListHead(&((*ppPrintCtx)->DataSetRootLookup));
Finish:
  return ReturnCode;
}

/*
* Helper to free all items in the "lookup list"
*/
static VOID CleanDataSetLookupItems(
  IN    PRINT_CONTEXT *pPrintCtx
)
{
  LIST_ENTRY *Entry;
  LIST_ENTRY *NextEntry;
  DATA_SET_LOOKUP_ITEM *DataSetLookupItem = NULL;

  if (NULL == pPrintCtx) {
    return;
  }

  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->DataSetLookup) {
    DataSetLookupItem = BASE_CR(Entry, DATA_SET_LOOKUP_ITEM, Link);
    RemoveEntryList(&DataSetLookupItem->Link);
    FREE_POOL_SAFE(DataSetLookupItem->DsPath);
    FREE_POOL_SAFE(DataSetLookupItem);
  }

  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->DataSetRootLookup) {
    DataSetLookupItem = BASE_CR(Entry, DATA_SET_LOOKUP_ITEM, Link);
    RemoveEntryList(&DataSetLookupItem->Link);
    FREE_DATASET_RECURSIVE_SAFE(DataSetLookupItem->pDataSet); //recursive free, no need to free children data sets.
    FREE_POOL_SAFE(DataSetLookupItem->DsPath);
    FREE_POOL_SAFE(DataSetLookupItem);
  }
}

/*
* Destroys a printer context
*/
EFI_STATUS PrinterDestroyCtx(
  IN    PRINT_CONTEXT *pPrintCtx
)
{
  LIST_ENTRY *Entry;
  LIST_ENTRY *NextEntry;
  BUFFERED_PRINTER_OBJECT *BufferedObject = NULL;
  EFI_STATUS ReturnCode = EFI_SUCCESS;

  if (NULL == pPrintCtx) {
    return EFI_INVALID_PARAMETER;
  }

  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->BufferedObjectList) {
    BufferedObject = BASE_CR(Entry, BUFFERED_PRINTER_OBJECT, Link);
    RemoveEntryList(&BufferedObject->Link);
    if (BUFF_STR_TYPE == BufferedObject->Type) {
      BUFFERED_STR *pTempBs = (BUFFERED_STR *)BufferedObject->Obj;
      FREE_POOL_SAFE(pTempBs->pStr);
    }
   else if (BUFF_COMMAND_STATUS_TYPE == BufferedObject->Type) {
      BUFFERED_COMMAND_STATUS *pTempCs = (BUFFERED_COMMAND_STATUS *)BufferedObject->Obj;
      FreeCommandStatus(&pTempCs->pCommandStatus);
      FREE_POOL_SAFE(pTempCs->pStatusMessage);
      FREE_POOL_SAFE(pTempCs->pStatusPreposition);
    }
    FREE_POOL_SAFE(BufferedObject->Obj);
    FREE_POOL_SAFE(BufferedObject);
  }

  CleanDataSetLookupItems(pPrintCtx);

  FREE_POOL_SAFE(pPrintCtx);
  return ReturnCode;
}

/*
* Helper to create a string object destined for the "set buffer"
*/
static EFI_STATUS CreateBufferedStrObj(BUFFERED_PRINTER_OBJECT **ppBufferedObj, EFI_STATUS Status, CHAR16 *Str) {
  BUFFERED_STR *pTempBuffStr;
  if (NULL == ppBufferedObj) {
    return EFI_INVALID_PARAMETER;
  }
  pTempBuffStr = (BUFFERED_STR*)AllocateZeroPool(sizeof(BUFFERED_STR));
  *ppBufferedObj = (BUFFERED_PRINTER_OBJECT*)AllocateZeroPool(sizeof(BUFFERED_PRINTER_OBJECT));
  if (*ppBufferedObj == NULL || pTempBuffStr == NULL) {
    FREE_POOL_SAFE(*ppBufferedObj);
    FREE_POOL_SAFE(pTempBuffStr);
    return EFI_OUT_OF_RESOURCES;
  }
  (*ppBufferedObj)->Type = BUFF_STR_TYPE;
  (*ppBufferedObj)->Obj = (VOID*)pTempBuffStr;
  (*ppBufferedObj)->Status = Status;
  pTempBuffStr->StrType = UNICODE_STR;
  pTempBuffStr->pStr = (VOID*)Str;
  return EFI_SUCCESS;
}

/*
* Helper to create a dataset object destined for the "set buffer"
*/
static EFI_STATUS CreateBufferedDataSetObj(BUFFERED_PRINTER_OBJECT **ppBufferedObj, EFI_STATUS Status, DATA_SET_CONTEXT *pDataSetCtx) {
  BUFFERED_DATA_SET *pTempBuffStr;
  if (NULL == ppBufferedObj) {
    return EFI_INVALID_PARAMETER;
  }
  pTempBuffStr = (BUFFERED_DATA_SET*)AllocateZeroPool(sizeof(BUFFERED_DATA_SET));
  *ppBufferedObj = (BUFFERED_PRINTER_OBJECT*)AllocateZeroPool(sizeof(BUFFERED_PRINTER_OBJECT));
  if (*ppBufferedObj == NULL || pTempBuffStr == NULL) {
    FREE_POOL_SAFE(*ppBufferedObj);
    FREE_POOL_SAFE(pTempBuffStr);
    return EFI_OUT_OF_RESOURCES;
  }
  (*ppBufferedObj)->Type = BUFF_DATA_SET_TYPE;
  (*ppBufferedObj)->Obj = (VOID*)pTempBuffStr;
  (*ppBufferedObj)->Status = Status;
  pTempBuffStr->pDataSet = pDataSetCtx;
  return EFI_SUCCESS;
}

/*
* Helper to create a dataset object destined for the "lookup list"
*/
static EFI_STATUS CreateDataSetLookupItem(DATA_SET_LOOKUP_ITEM **ppDataSetLookupItem, CHAR16 *pPath, DATA_SET_CONTEXT *pDataSetCtx) {
  if (NULL == ppDataSetLookupItem) {
    return EFI_INVALID_PARAMETER;
  }
  *ppDataSetLookupItem = (DATA_SET_LOOKUP_ITEM*)AllocateZeroPool(sizeof(DATA_SET_LOOKUP_ITEM));
  if (*ppDataSetLookupItem == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  (*ppDataSetLookupItem)->pDataSet = pDataSetCtx;
  (*ppDataSetLookupItem)->DsPath = CatSPrint(NULL, pPath);
  return EFI_SUCCESS;
}

/*
* Handle string messages
*/
EFI_STATUS EFIAPI PrinterSetMsg(
  IN    PRINT_CONTEXT *pPrintCtx,
  IN    EFI_STATUS Status,
  IN    CHAR16 *pMsg,
  ...
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *FullMsg = NULL;
  VA_LIST Marker;

  if (NULL == pMsg) {
    NVDIMM_ERR("Invalid input parameter\n");
    goto Finish;
  }

  VA_START(Marker, pMsg);
  FullMsg = CatVSPrint(NULL, pMsg, Marker);
  VA_END(Marker);

  //here for backwards compatibility
  if (NULL == pPrintCtx || !pPrintCtx->FormatTypeFlags.Flags.Buffered) {
    PrintTextWithNewLine(FullMsg);
    FREE_POOL_SAFE(FullMsg);
    return EFI_SUCCESS;
  }

  if (pPrintCtx->FormatTypeFlags.Flags.Buffered) {
    BUFFERED_PRINTER_OBJECT *pBufferedObj = NULL;
    if (EFI_SUCCESS != (ReturnCode = CreateBufferedStrObj(&pBufferedObj, Status, FullMsg))) {
      goto Finish;
    }
    InsertTailList(&pPrintCtx->BufferedObjectList, &pBufferedObj->Link);
    if (EFI_ERROR(Status)) {
      pPrintCtx->BufferedObjectLastError = Status;
    }
  }
  pPrintCtx->BufferedMsgCnt++;
  ReturnCode = EFI_SUCCESS;
Finish:
  return ReturnCode;
}

/*
* Handle dataset objects
*/
EFI_STATUS PrinterSetData(
  IN    PRINT_CONTEXT *pPrintCtx,
  IN    EFI_STATUS Status,
  IN    DATA_SET_CONTEXT *pDataSetCtx
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;

  if (NULL == pDataSetCtx && NULL == pPrintCtx) {
    NVDIMM_ERR("Invalid input parameter\n");
    goto Finish;
  }

  if (pPrintCtx->FormatTypeFlags.Flags.Buffered) {
    BUFFERED_PRINTER_OBJECT *pBufferedObj = NULL;
    if (EFI_SUCCESS != (ReturnCode = CreateBufferedDataSetObj(&pBufferedObj, Status, pDataSetCtx))) {
      goto Finish;
    }
    InsertTailList(&pPrintCtx->BufferedObjectList, &pBufferedObj->Link);
    if (EFI_ERROR(Status)) {
      pPrintCtx->BufferedObjectLastError = Status;
    }
  }
  pPrintCtx->BufferedDataSetCnt++;
  ReturnCode = EFI_SUCCESS;
Finish:
  return ReturnCode;
}

/*
* Handle commandstatus objects
*/
EFI_STATUS PrinterSetCommandStatus(
  IN     PRINT_CONTEXT *pPrintCtx,
  IN     EFI_STATUS Status,
  IN     CHAR16 *pStatusMessage,
  IN     CHAR16 *pStatusPreposition,
  IN     COMMAND_STATUS *pCommandStatus
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 *FullMsg = NULL;

  if (NULL == pCommandStatus && NULL == pPrintCtx) {
    NVDIMM_ERR("Invalid input parameter\n");
    goto Finish;
  }

  if (EFI_SUCCESS != (ReturnCode = CreateCmdStatusMsg(&FullMsg, pStatusMessage, pStatusPreposition, pCommandStatus))) {
    goto Finish;
  }

  if (EFI_SUCCESS != (ReturnCode = PrinterSetMsg(pPrintCtx, Status, FullMsg))) {
    goto Finish;
  }
  ReturnCode = EFI_SUCCESS;
Finish:
  FREE_POOL_SAFE(FullMsg);
  return ReturnCode;
}

/*
* Proces mode
*/
static PRINT_MODE PrintMode(
  IN     PRINT_CONTEXT *pPrintCtx
)
{
  if (NULL == pPrintCtx) {
    return PRINT_TEXT;
  }

  if (XML == pPrintCtx->FormatType && 1 == pPrintCtx->BufferedDataSetCnt && EFI_SUCCESS == pPrintCtx->BufferedObjectLastError) {
    return PRINT_XML;
  }
  else if (XML == pPrintCtx->FormatType) {
    return PRINT_BASIC_XML;
  }
  else return PRINT_TEXT;
}

/*
* Process all objects int the "set buffer"
*/
EFI_STATUS PrinterProcessSetBuffer(
  IN     PRINT_CONTEXT *pPrintCtx
)
{
  LIST_ENTRY *Entry;
  LIST_ENTRY *NextEntry;
  BUFFERED_PRINTER_OBJECT *BufferedObject;
  EFI_STATUS ReturnCode = EFI_SUCCESS;
  CHAR16 *FullMsg = NULL;
  PRINT_MODE PrinterMode = PRINT_TEXT;

  if (NULL == pPrintCtx) {
    return EFI_INVALID_PARAMETER;
  }

  PrinterMode = PrintMode(pPrintCtx);

  //if XML mode print the appropriate start tag
  if (PRINT_BASIC_XML == PrinterMode) {
    if (EFI_SUCCESS != pPrintCtx->BufferedObjectLastError) {
      PrintXmlStartErrorTag(pPrintCtx, pPrintCtx->BufferedObjectLastError);
    }
    else {
      PrintXmlStartSuccessTag(pPrintCtx, pPrintCtx->BufferedObjectLastError);
    }
  }

  //iterate through all items in the "set buffer".
  //all items found should be transformed to text and printed directly to stdout
  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->BufferedObjectList) {
    BufferedObject = BASE_CR(Entry, BUFFERED_PRINTER_OBJECT, Link);
    RemoveEntryList(&BufferedObject->Link);
    if (BUFF_STR_TYPE == BufferedObject->Type) {
      BUFFERED_STR *pTempBs = (BUFFERED_STR *)BufferedObject->Obj;
      if (PRINT_XML != PrinterMode) {
        PrintTextWithNewLine(pTempBs->pStr);
      }
      FREE_POOL_SAFE(pTempBs->pStr);
      pPrintCtx->BufferedMsgCnt--;
    }
    else if (BUFF_DATA_SET_TYPE == BufferedObject->Type) {
      BUFFERED_DATA_SET *pTempDs = (BUFFERED_DATA_SET *)BufferedObject->Obj;
      if (PRINT_XML == PrinterMode) {
        PrintAsXml(pTempDs->pDataSet, pPrintCtx);
      }
      else {
        PrintAsText(pTempDs->pDataSet, pPrintCtx);
      }
      pPrintCtx->BufferedDataSetCnt--;
    }
    else if (BUFF_COMMAND_STATUS_TYPE == BufferedObject->Type) {
      BUFFERED_COMMAND_STATUS *pTempCs = (BUFFERED_COMMAND_STATUS *)BufferedObject->Obj;
      CreateCmdStatusMsg(&FullMsg, pTempCs->pStatusMessage, pTempCs->pStatusPreposition, pTempCs->pCommandStatus);
      if (PRINT_XML != PrinterMode) {
        PrintTextWithNewLine(FullMsg);
      }
      FreeCommandStatus(&pTempCs->pCommandStatus);
      FREE_POOL_SAFE(pTempCs->pStatusMessage);
      FREE_POOL_SAFE(pTempCs->pStatusPreposition);
      FREE_POOL_SAFE(FullMsg);
      pPrintCtx->BufferedCmdStatusCnt--;
    }
    FREE_POOL_SAFE(BufferedObject->Obj);
    FREE_POOL_SAFE(BufferedObject);
  }
  //if XML mode print the appropriate end tag
  if (PRINT_BASIC_XML == PrinterMode) {
    if (EFI_SUCCESS != pPrintCtx->BufferedObjectLastError) {
      PrintXmlEndErrorTag(pPrintCtx, pPrintCtx->BufferedObjectLastError);
    }
    else {
      PrintXmlEndSuccessTag(pPrintCtx, pPrintCtx->BufferedObjectLastError);
    }
  }
  
  CleanDataSetLookupItems(pPrintCtx);
  pPrintCtx->BufferedObjectLastError = EFI_SUCCESS;
  return ReturnCode;
}

/*
* Helper that associates a table/list of attributes with a dataset
*/
EFI_STATUS SetDataSetPrinterAttribs(
  IN     PRINT_CONTEXT *pPrintCtx,
  IN     CHAR16 *pKeyPath,
  IN     PRINTER_DATA_SET_ATTRIBS *pAttribs
) 
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 **DataSetToks = NULL;
  UINT32 NumDataSetToks = 0;
  LIST_ENTRY *Entry;
  LIST_ENTRY *NextEntry;
  DATA_SET_LOOKUP_ITEM *DataSetLookupItem = NULL;

  if (NULL == pPrintCtx || NULL == pKeyPath) {
    goto Finish;
  }

  // split path, result toks are data set names
  if (NULL == (DataSetToks = StrSplit(pKeyPath, L'/', &NumDataSetToks))) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
  else if (NumDataSetToks < 2) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  ReturnCode = EFI_NOT_FOUND;
  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->DataSetRootLookup) {
    DataSetLookupItem = BASE_CR(Entry, DATA_SET_LOOKUP_ITEM, Link);
    if (0 == StrCmp(DataSetToks[1], DataSetLookupItem->DsPath)) {
      SetDataSetUserData(DataSetLookupItem->pDataSet, (VOID *)pAttribs);
      ReturnCode = EFI_SUCCESS;
      break;
    }
  }
Finish:
  FREE_POOL_SAFE(DataSetToks);
  return ReturnCode;
}

/*
* Helper to lookup a dataset described by a path
*/
EFI_STATUS LookupDataSet(
  IN     PRINT_CONTEXT *pPrintCtx,
  IN     CHAR16 *pKeyPath,
  OUT    DATA_SET_CONTEXT **ppDataSet
)
{
  EFI_STATUS ReturnCode = EFI_INVALID_PARAMETER;
  CHAR16 **DataSetToks = NULL;
  UINT32 NumDataSetToks = 0;
  LIST_ENTRY *Entry;
  LIST_ENTRY *NextEntry;
  DATA_SET_LOOKUP_ITEM *DataSetLookupItem = NULL;
  DATA_SET_CONTEXT *Root = NULL;

  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->DataSetLookup) {
    DataSetLookupItem = BASE_CR(Entry, DATA_SET_LOOKUP_ITEM, Link);
    if (0 == StrCmp(pKeyPath, DataSetLookupItem->DsPath)) {
      *ppDataSet = DataSetLookupItem->pDataSet;
      return EFI_SUCCESS;
    }
  }

  //split path, result toks are data set names
  if (NULL == (DataSetToks = StrSplit(pKeyPath, L'/', &NumDataSetToks))) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }
  else if (NumDataSetToks < 2) {
    ReturnCode = EFI_NOT_FOUND;
    goto Finish;
  }

  BUFFERED_OBJECT_LIST_FOR_EACH_SAFE(Entry, NextEntry, &pPrintCtx->DataSetRootLookup) {
    DataSetLookupItem = BASE_CR(Entry, DATA_SET_LOOKUP_ITEM, Link);
    if (0 == StrCmp(DataSetToks[1], DataSetLookupItem->DsPath)) {
      Root = DataSetLookupItem->pDataSet;
      break;
    }
  }

  if(NULL == Root) {
    if(NULL == (Root = CreateDataSet(NULL, DataSetToks[1], NULL))) {
      ReturnCode = EFI_OUT_OF_RESOURCES;
      goto Finish;
    }
    DataSetLookupItem = NULL;
    if (EFI_SUCCESS == (ReturnCode = CreateDataSetLookupItem(&DataSetLookupItem, DataSetToks[1], Root))) {
      InsertTailList(&pPrintCtx->DataSetRootLookup, &DataSetLookupItem->Link);
    }

    PRINTER_SET_DATA(pPrintCtx, EFI_SUCCESS, Root);
  }

  *ppDataSet = GetDataSet(Root, pKeyPath);
  DataSetLookupItem = NULL;
  if (EFI_SUCCESS == (ReturnCode = CreateDataSetLookupItem(&DataSetLookupItem, pKeyPath, *ppDataSet))) {
    InsertTailList(&pPrintCtx->DataSetLookup, &DataSetLookupItem->Link);
  }

  ReturnCode = EFI_SUCCESS;
Finish:
  FreeStringArray(DataSetToks, NumDataSetToks);
  return ReturnCode;
}

/*
* Helper that builds a path to a dataset node
*/
CHAR16 *
EFIAPI
BuildPath(CHAR16 *Format, ...) {
  CHAR16 *NewPath = NULL;
  VA_LIST Marker;

  if (NULL == Format) {
    return NULL;
  }

  VA_START(Marker, Format);
  NewPath = CatVSPrint(NULL, Format, Marker);
  VA_END(Marker);
  return NewPath;
}