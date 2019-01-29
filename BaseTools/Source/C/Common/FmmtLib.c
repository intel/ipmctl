/** @file

Copyright (c) 2004 - 2016, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "../FMMT/FMMT.h"
#include "FirmwareVolumeBufferLib.h"

char *tmpname() {
  const char Mask[] = "/tmp/XXXXXX";
  static char toReturn[] = "/tmp/XXXXXX";
  int file = 0;

  memcpy(toReturn, Mask, strlen(Mask));

  file = mkstemp(toReturn);

  if (file > 0) {
    close(file);

    return toReturn + 5;
  }

  return NULL;
}

#define EFI_TEST_FFS_ATTRIBUTES_BIT(FvbAttributes, TestAttributes, Bit) \
    ( \
      (BOOLEAN) ( \
          (FvbAttributes & EFI_FVB2_ERASE_POLARITY) ? (((~TestAttributes) & Bit) == Bit) : ((TestAttributes & Bit) == Bit) \
        ) \
    )
    
CHAR8      mFirmwareFileSystem2Guid[16] = {0x78, 0xE5, 0x8C, 0x8C, 0x3D, 0x8A, 0x1C, 0x4F, 0x99, 0x35, 0x89, 0x61, 0x85, 0xC3, 0x2D, 0xD3};

CHAR8      mFirmwareFileSystem3Guid[16] = {0x7A, 0xC0, 0x73, 0x54, 0xCB, 0x3D, 0xCA, 0x4D, 0xBD, 0x6F, 0x1E, 0x96, 0x89, 0xE7, 0x34, 0x9A };

EFI_GUID   mEfiCrc32GuidedSectionExtractionProtocolGuid = EFI_CRC32_GUIDED_SECTION_EXTRACTION_PROTOCOL_GUID;


STATIC CHAR8 *mSectionTypeName[] = {
  NULL,                                 // 0x00 - reserved
  "EFI_SECTION_COMPRESSION",            // 0x01
  "EFI_SECTION_GUID_DEFINED",           // 0x02
  NULL,                                 // 0x03 - reserved
  NULL,                                 // 0x04 - reserved
  NULL,                                 // 0x05 - reserved
  NULL,                                 // 0x06 - reserved
  NULL,                                 // 0x07 - reserved
  NULL,                                 // 0x08 - reserved
  NULL,                                 // 0x09 - reserved
  NULL,                                 // 0x0A - reserved
  NULL,                                 // 0x0B - reserved
  NULL,                                 // 0x0C - reserved
  NULL,                                 // 0x0D - reserved
  NULL,                                 // 0x0E - reserved
  NULL,                                 // 0x0F - reserved
  "EFI_SECTION_PE32",                   // 0x10
  "EFI_SECTION_PIC",                    // 0x11
  "EFI_SECTION_TE",                     // 0x12
  "EFI_SECTION_DXE_DEPEX",              // 0x13
  "EFI_SECTION_VERSION",                // 0x14
  "EFI_SECTION_USER_INTERFACE",         // 0x15
  "EFI_SECTION_COMPATIBILITY16",        // 0x16
  "EFI_SECTION_FIRMWARE_VOLUME_IMAGE",  // 0x17
  "EFI_SECTION_FREEFORM_SUBTYPE_GUID",  // 0x18
  "EFI_SECTION_RAW",                    // 0x19
  NULL,                                 // 0x1A
  "EFI_SECTION_PEI_DEPEX",              // 0x1B
  "EFI_SECTION_SMM_DEPEX"               // 0x1C
};


STATIC CHAR8 *mFfsFileType[] = {
  NULL,                                   // 0x00
  "EFI_FV_FILETYPE_RAW",                  // 0x01
  "EFI_FV_FILETYPE_FREEFORM",             // 0x02
  "EFI_FV_FILETYPE_SECURITY_CORE",        // 0x03
  "EFI_FV_FILETYPE_PEI_CORE",             // 0x04
  "EFI_FV_FILETYPE_DXE_CORE",             // 0x05
  "EFI_FV_FILETYPE_PEIM",                 // 0x06
  "EFI_FV_FILETYPE_DRIVER",               // 0x07
  "EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER", // 0x08
  "EFI_FV_FILETYPE_APPLICATION",          // 0x09
  "EFI_FV_FILETYPE_SMM",                  // 0x0A
  "EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE",// 0x0B
  "EFI_FV_FILETYPE_COMBINED_SMM_DXE",     // 0x0C
  "EFI_FV_FILETYPE_SMM_CORE"              // 0x0D
 };

STATIC CHAR8 *mGuidSectionAttr[] = {
  "NONE",                                 // 0x00
  "PROCESSING_REQUIRED",                  // 0x01
  "AUTH_STATUS_VALID"                     // 0x02
};

FV_INFORMATION * 
LibInitializeFvStruct (
  FV_INFORMATION *Fv
)
{
  UINT32     Index;
  
  if (Fv == NULL) {
    return NULL;
  }
  
  for (Index = 0; Index < MAX_NUMBER_OF_FILES_IN_FV; Index ++) {
    memset (Fv->FfsAttuibutes[Index].FfsName, '\0', MAX_PATH);
    memset (Fv->FfsAttuibutes[Index].UiName, '\0', sizeof(Fv->FfsAttuibutes[Index].UiName));
    
    Fv->FfsAttuibutes[Index].IsLeaf               = TRUE;
	Fv->FfsAttuibutes[Index].Level                = 0xFF;  
    Fv->FfsAttuibutes[Index].TotalSectionNum      = 0;  
  }
  
  Fv->EncapData = NULL;
  Fv->FvNext = NULL;
  Fv->FvLevel   = 0;
  
  return Fv;
} 


EFI_STATUS
LibFindFvInFd (
  IN     FILE             *InputFile,
  IN OUT FIRMWARE_DEVICE  **FdData
)
{
  FIRMWARE_DEVICE             *LocalFdData;
  UINT16                      Index;
  CHAR8                       Ffs2Guid[16];  
  CHAR8                       SignatureCheck[5] = "";
  CHAR8                       Signature[5] = "_FVH";  
  FV_INFORMATION              *CurrentFv;
  FV_INFORMATION              *NewFoundFv; 
  BOOLEAN                     FirstMatch;
  UINT32                      FdSize;
  UINT16                      FvCount;
  UINT8                       *FdBuffer;
  UINT8                       *FdBufferEnd;
  UINT8                       *FdBufferOri;
  EFI_FIRMWARE_VOLUME_HEADER  *FvHeader;

  CurrentFv      = NULL;
  NewFoundFv     = NULL;
  FdBuffer       = NULL;
  FdBufferOri    = NULL;
  FirstMatch     = TRUE; 
  Index          = 0;
  FdSize         = 0;
  FvCount        = 0;
  LocalFdData    = NULL;
    
  if (InputFile == NULL) {
    Error ("FMMT", 0, 0001, "Error opening the input file", "");
    return EFI_ABORTED;
  }  
    
  //
  // Find each FVs in the FD.
  //
  
  fseek(InputFile,0,SEEK_SET);
  fseek(InputFile,0,SEEK_END);

  FdSize = ftell(InputFile);

  fseek(InputFile,0,SEEK_SET);  
  //
  // Create an FD structure to store useful information.
  // 
  LocalFdData     = (FIRMWARE_DEVICE *) malloc (sizeof (FIRMWARE_DEVICE));
  LocalFdData->Fv = (FV_INFORMATION *)  malloc (sizeof (FV_INFORMATION));  
  
  LibInitializeFvStruct (LocalFdData->Fv);
  
  //
  // Readout the FD file data to buffer.
  //
  FdBuffer = malloc (FdSize);
  
  if (FdBuffer == NULL) {
    Error ("FMMT", 0, 0002, "Error searching FVs in the input fd", "Allocate memory error");
    return EFI_OUT_OF_RESOURCES;    
  }
  
  if (fread (FdBuffer, 1, FdSize, InputFile) != FdSize) {
    Error ("FMMT", 0, 0002, "Error searching FVs in the input fd", "Read FD file error!");
    return EFI_ABORTED;
  }
  
  FdBufferOri = FdBuffer;
  FdBufferEnd = FdBuffer + FdSize;
  
  while (FdBuffer <= FdBufferEnd - sizeof (EFI_FIRMWARE_VOLUME_HEADER)) {
    FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *) FdBuffer;
    //
    // Copy 4 bytes of fd data to check the _FVH signature
    //
    memcpy (SignatureCheck, &FvHeader->Signature, 4);
  
    if (strncmp(SignatureCheck, Signature, 4) == 0){
      // 
      // Still need to determine the FileSystemGuid in EFI_FIRMWARE_VOLUME_HEADER equal to 
      // EFI_FIRMWARE_FILE_SYSTEM2_GUID or EFI_FIRMWARE_FILE_SYSTEM3_GUID.
      // Turn back 28 bytes to find the GUID.
      //
      memcpy (Ffs2Guid, &FvHeader->FileSystemGuid, 16);
      
      //
      // Compare GUID.
      //
      for (Index = 0; Index < 16; Index ++) {
        if (Ffs2Guid[Index] != mFirmwareFileSystem2Guid[Index]) {
          break;
        }
      }
	  if (Index != 16) {
	    for (Index = 0; Index < 16; Index ++) {
          if (Ffs2Guid[Index] != mFirmwareFileSystem3Guid[Index]) {
            break;
          }
        } 
	  }
      
      //
      // Here we found an FV.
      // 
      if ((Index == 16) && ((FdBuffer + FvHeader->FvLength) <= FdBufferEnd)) {
        if (FirstMatch) {
          LocalFdData->Fv->ImageAddress = (UINTN)((UINT8 *)FdBuffer - (UINT8 *)FdBufferOri);
          CurrentFv                     = LocalFdData->Fv;
          CurrentFv->FvNext             = NULL;
          //
          // Store the FV name by found sequence
          // 
          sprintf(CurrentFv->FvName, "FV%d", FvCount);         
          
          FirstMatch = FALSE;
          } else {
            NewFoundFv = (FV_INFORMATION *) malloc (sizeof (FV_INFORMATION));
            
            LibInitializeFvStruct (NewFoundFv);
            
            //
            // Need to turn back 0x2c bytes
            //
            NewFoundFv->ImageAddress = (UINTN)((UINT8 *)FdBuffer - (UINT8 *)FdBufferOri);
            
            //
            // Store the FV name by found sequence
            // 
            sprintf(NewFoundFv->FvName, "FV%d", FvCount);     
                
            //
            // Value it to NULL for found FV usage.
            //
            NewFoundFv->FvNext       = NULL;
            CurrentFv->FvNext        = NewFoundFv; 
         
            //
            // Make the CurrentFv point to next FV.
            //   
            CurrentFv                = CurrentFv->FvNext;
          }    

        FvCount ++;
        FdBuffer = FdBuffer + FvHeader->FvLength;
      } else {
        FdBuffer ++;
      }

    } else {
      FdBuffer ++;
    }
  } 
  
  LocalFdData->Size = FdSize;
  
  *FdData = LocalFdData;
  
  free (FdBufferOri);
  
  return EFI_SUCCESS;
}

/**

  This function determines the size of the FV and the erase polarity.  The 
  erase polarity is the FALSE value for file state.


  @param[in ]   InputFile       The file that contains the FV image.
  @param[out]   FvSize          The size of the FV.
  @param[out]   ErasePolarity   The FV erase polarity.
 
  @return EFI_SUCCESS             Function completed successfully.
  @return EFI_INVALID_PARAMETER   A required parameter was NULL or is out of range.
  @return EFI_ABORTED             The function encountered an error.
  
**/
EFI_STATUS
LibReadFvHeader (
  IN   VOID                       *InputFv,
  IN   BOOLEAN                    ViewFlag,
  IN   UINT8                      FvLevel,
  IN   CHAR8                      *FvName
  )
{
  EFI_FIRMWARE_VOLUME_HEADER     *VolumeHeader;
  CHAR8                          *BlankSpace;
  UINT8                          ParentFvCount;

  BlankSpace = NULL;
  
  //
  // Check input parameters
  //  
  if (InputFv == NULL) {
    Error (__FILE__, __LINE__, 0, "FMMT application error", "invalid parameter to function");
    return EFI_INVALID_PARAMETER;
  }  
  
  //
  // Read the header
  //
  VolumeHeader = (EFI_FIRMWARE_VOLUME_HEADER *) InputFv;

  BlankSpace = LibConstructBlankChar((FvLevel)*2);
  
  ParentFvCount = (UINT8) atoi (FvName+2);
  
  if (ViewFlag) {
    if ((FvLevel -1) == 0) {
      printf ("\n%s :\n", FvName);
    } else {
      printf ("%sChild FV named FV%d of %s\n", BlankSpace, FvLevel+ParentFvCount-1, FvName);
    }
  }
  
  //
  // Print FV header information
  //
  if (ViewFlag) {
    printf ("\n%sAttributes:            %X\n", BlankSpace, (unsigned) VolumeHeader->Attributes);
    printf ("%sTotal Volume Size:     0x%08X\n\n", BlankSpace, (unsigned) VolumeHeader->FvLength);
  }

  return EFI_SUCCESS;
}

/*
  Get size info from FV file.
  
  @param[in]
  @param[out]
  
  @retval
  
*/
EFI_STATUS
LibGetFvSize (
  IN   FILE                       *InputFile,
  OUT  UINT32                     *FvSize
  )
{

  UINTN                          BytesRead;
  UINT32                         Size;
  EFI_FV_BLOCK_MAP_ENTRY         BlockMap;

  BytesRead = 0;
  Size      = 0;  
  
  if (InputFile == NULL || FvSize == NULL) {
    Error (__FILE__, __LINE__, 0, "FMMT application error", "invalid parameter to function");
    return EFI_INVALID_PARAMETER;    
  }
  
  fseek (InputFile, sizeof (EFI_FIRMWARE_VOLUME_HEADER) - sizeof (EFI_FV_BLOCK_MAP_ENTRY), SEEK_CUR);
  do {
    fread (&BlockMap, sizeof (EFI_FV_BLOCK_MAP_ENTRY), 1, InputFile);
    BytesRead += sizeof (EFI_FV_BLOCK_MAP_ENTRY);

    if (BlockMap.NumBlocks != 0) {
      Size += BlockMap.NumBlocks * BlockMap.Length;
    } 
  } while (!(BlockMap.NumBlocks == 0 && BlockMap.Length == 0)); 
  
  
  *FvSize = Size;
  
  return EFI_SUCCESS;
}

/**

  Expands the 3 byte size commonly used in Firmware Volume data structures

  @param[in]    Size - Address of the 3 byte array representing the size
                      
  @return       UINT32

**/
/*UINT32
FvBufExpand3ByteSize (
  IN VOID* Size
  )
{
  return (((UINT8*)Size)[2] << 16) +
         (((UINT8*)Size)[1] << 8) +
         ((UINT8*)Size)[0];
}*/

/**

  Clears out all files from the Fv buffer in memory

  @param[in]    Fv - Address of the Fv in memory
                      
  @return       EFI_STATUS

**/
/*EFI_STATUS
FvBufGetSize (
  IN  VOID   *Fv,
  OUT UINTN  *Size
  )
{
  EFI_FIRMWARE_VOLUME_HEADER *hdr;
  EFI_FV_BLOCK_MAP_ENTRY     *blk;

  *Size = 0;
  hdr   = (EFI_FIRMWARE_VOLUME_HEADER*)Fv;
  blk   = hdr->BlockMap;

  while (blk->Length != 0 || blk->NumBlocks != 0) {
    *Size = *Size + (blk->Length * blk->NumBlocks);
    if (*Size >= 0x40000000) {
	  //
      // If size is greater than 1GB, then assume it is corrupted
      //
      return EFI_VOLUME_CORRUPTED;
    }
    blk++;
  }

  if (*Size == 0) {
  	//
    // If size is 0, then assume the volume is corrupted
    //
    return EFI_VOLUME_CORRUPTED;
  }

  return EFI_SUCCESS;
}*/
/**

  Iterates through the files contained within the firmware volume

  @param[in]    Fv  - Address of the Fv in memory
  @param[in]    Key - Should be 0 to get the first file.  After that, it should be
                      passed back in without modifying it's contents to retrieve
                      subsequent files.
  @param[in]    File- Output file pointer
                      File == NULL - invalid parameter
                      otherwise - *File will be update to the location of the file
                      
  @return       EFI_STATUS
                EFI_NOT_FOUND
                EFI_VOLUME_CORRUPTED

**/
/*EFI_STATUS
FvBufFindNextFile (
  IN     VOID      *Fv,
  IN OUT UINTN     *Key,
  OUT    VOID      **File
  )
{
  EFI_FIRMWARE_VOLUME_HEADER *hdr;
  EFI_FFS_FILE_HEADER        *fhdr;
  EFI_FVB_ATTRIBUTES_2       FvbAttributes;
  UINTN                      fsize;
  EFI_STATUS                 Status;
  UINTN                      fvSize;

  hdr = (EFI_FIRMWARE_VOLUME_HEADER*)Fv;
  fhdr = NULL;
  
  if (Fv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = FvBufGetSize (Fv, &fvSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (*Key == 0) {
    *Key = hdr->HeaderLength;
  }

  FvbAttributes = hdr->Attributes;

  for(
      *Key = (UINTN)ALIGN_POINTER (*Key, 8);
      (*Key + sizeof (*fhdr)) < fvSize;
      *Key = (UINTN)ALIGN_POINTER (*Key, 8)
    ) {
    fhdr = (EFI_FFS_FILE_HEADER*) ((UINT8*)hdr + *Key);
    if (fhdr->Attributes & FFS_ATTRIB_LARGE_FILE) {
	  fsize = ((EFI_FFS_FILE_HEADER2*)fhdr)->ExtendedSize;
    } else {
      fsize = FvBufExpand3ByteSize (fhdr->Size);
    }
    if (!EFI_TEST_FFS_ATTRIBUTES_BIT(
          FvbAttributes,
          fhdr->State,
          EFI_FILE_HEADER_VALID
        ) ||
        EFI_TEST_FFS_ATTRIBUTES_BIT(
          FvbAttributes,
          fhdr->State,
          EFI_FILE_HEADER_INVALID
        )
      ) {
      *Key = *Key + 1; 
      continue;
    } else if(
        EFI_TEST_FFS_ATTRIBUTES_BIT(
          FvbAttributes,
          fhdr->State,
          EFI_FILE_MARKED_FOR_UPDATE
        ) ||
        EFI_TEST_FFS_ATTRIBUTES_BIT(
          FvbAttributes,
          fhdr->State,
          EFI_FILE_DELETED
        )
      ) {
      *Key = *Key + fsize;
      continue;
    } else if (EFI_TEST_FFS_ATTRIBUTES_BIT(
          FvbAttributes,
          fhdr->State,
          EFI_FILE_DATA_VALID
        )
      ) {
      *File = (UINT8*)hdr + *Key;
      *Key = *Key + fsize;
      return EFI_SUCCESS;
    }

    *Key = *Key + 1; 
  }

  return EFI_NOT_FOUND;
}*/
/**

  Get firmware information. Including the FV headers, 

  @param[in]    Fv            - Firmware Volume to get information from

  @return       EFI_STATUS

**/
EFI_STATUS
LibGetFvInfo (
  IN     VOID                         *Fv,
  IN OUT FV_INFORMATION               *CurrentFv,
  IN     CHAR8                        *FvName,
  IN     UINT8                        Level,
  IN     UINT32                       *FfsCount,
  IN     BOOLEAN                      ViewFlag,
  IN     BOOLEAN                      IsChildFv
  )
{
  EFI_STATUS                  Status;
  UINTN                       NumberOfFiles;
  BOOLEAN                     ErasePolarity;
  UINTN                       FvSize;
  EFI_FFS_FILE_HEADER2        *CurrentFile;
  UINTN                       Key;
  ENCAP_INFO_DATA             *LocalEncapData;
  

  NumberOfFiles  = 0;
  Key            = 0;
  LocalEncapData = NULL;
  CurrentFile    = NULL;
  
  
  Level += 1;
  CurrentFv->FvLevel += 1;
  
  Status = FvBufGetSize (Fv, &FvSize);
 
  ErasePolarity = (((EFI_FIRMWARE_VOLUME_HEADER*)Fv)->Attributes & EFI_FVB2_ERASE_POLARITY) ? TRUE : FALSE;  
  
  Status = LibReadFvHeader (Fv, ViewFlag, CurrentFv->FvLevel, CurrentFv->FvName);
  if (EFI_ERROR (Status)) {
    Error (NULL, 0, 0003, "error parsing FV image", "Header is invalid");
    return EFI_ABORTED;
  }
  
  if (!IsChildFv) {
    //
    // Write FV header information into CurrentFv struct.
    //
    CurrentFv->FvHeader = (EFI_FIRMWARE_VOLUME_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_HEADER));

    if (CurrentFv->FvHeader == NULL) {
      Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
      return EFI_ABORTED;
    }      

    //
    // Get the FV Header information
    //  
    memcpy (CurrentFv->FvHeader, Fv, sizeof (EFI_FIRMWARE_VOLUME_HEADER)); 
    CurrentFv->FvExtHeader = NULL;
    
    //
    // Exist Extend FV header.
    //
    if (CurrentFv->FvHeader->ExtHeaderOffset != 0){
      CurrentFv->FvExtHeader = (EFI_FIRMWARE_VOLUME_EXT_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER));

      if (CurrentFv->FvExtHeader == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }   

      //
      // Get the FV extended Header information
      //  
      memcpy (CurrentFv->FvExtHeader, (VOID *)((UINTN)Fv + CurrentFv->FvHeader->ExtHeaderOffset), sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER)); 

    }
  }
   
  //
  // Put encapsulate information into structure.
  // 
  if (CurrentFv->EncapData == NULL && !IsChildFv) {
    //
    // First time in, the root FV
    //
    CurrentFv->EncapData = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));
    CurrentFv->EncapData->FvExtHeader = NULL;

    if (CurrentFv->EncapData == NULL) {
      Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
      return EFI_ABORTED;
    }    
       
    CurrentFv->EncapData->Level = Level;
    CurrentFv->EncapData->Type  = FMMT_ENCAP_TREE_FV;
    CurrentFv->EncapData->Data  = (EFI_FIRMWARE_VOLUME_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_HEADER));
    
    if (CurrentFv->EncapData->Data == NULL) {
      Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
      return EFI_ABORTED;
    }      
    
    memcpy (CurrentFv->EncapData->Data, Fv, sizeof (EFI_FIRMWARE_VOLUME_HEADER)); 

    if (((EFI_FIRMWARE_VOLUME_HEADER *)(CurrentFv->EncapData->Data))->ExtHeaderOffset != 0) {
      CurrentFv->EncapData->FvExtHeader = (EFI_FIRMWARE_VOLUME_EXT_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER));

      if (CurrentFv->EncapData->FvExtHeader == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }   

      //
      // Get the FV extended Header information
      //  
      memcpy (CurrentFv->EncapData->FvExtHeader, (VOID *)((UINTN)Fv + ((EFI_FIRMWARE_VOLUME_HEADER *)(CurrentFv->EncapData->Data))->ExtHeaderOffset), sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER)); 
    }
    
    CurrentFv->EncapData->NextNode  = NULL;
	CurrentFv->EncapData->RightNode  = NULL;
  } else if (IsChildFv) {

      LocalEncapData = CurrentFv->EncapData;
      while (LocalEncapData->NextNode != NULL) {
        LocalEncapData = LocalEncapData->NextNode;
      }
      
      //
      // Construct the new ENCAP_DATA
      //
      LocalEncapData->NextNode = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));
      
      if (LocalEncapData->NextNode == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }             
      
      LocalEncapData           = LocalEncapData->NextNode;
      
      LocalEncapData->Level = Level;
      LocalEncapData->Type  = FMMT_ENCAP_TREE_FV;
      LocalEncapData->Data  = (EFI_FIRMWARE_VOLUME_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_HEADER));    
      LocalEncapData->FvExtHeader = NULL;
                     
      if (LocalEncapData->Data == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }      
      
      memcpy (LocalEncapData->Data, Fv, sizeof (EFI_FIRMWARE_VOLUME_HEADER)); 

      if (((EFI_FIRMWARE_VOLUME_HEADER *)(LocalEncapData->Data))->ExtHeaderOffset != 0) {
        LocalEncapData->FvExtHeader = (EFI_FIRMWARE_VOLUME_EXT_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER));

        if (LocalEncapData->FvExtHeader == NULL) {
          Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
          return EFI_ABORTED;
        }   

        //
        // Get the FV extended Header information
        //  
        memcpy (LocalEncapData->FvExtHeader, (VOID *)((UINTN)Fv + ((EFI_FIRMWARE_VOLUME_HEADER *)(LocalEncapData->Data))->ExtHeaderOffset), sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER)); 
      }
      
      LocalEncapData->NextNode  = NULL; 
	  CurrentFv->EncapData->RightNode  = NULL;

  }
  
  
  //
  // Get the first file
  //
  Status = FvBufFindNextFile (Fv, &Key, (VOID **) &CurrentFile);
  if (Status == EFI_NOT_FOUND) {
    CurrentFile = NULL;
  } else if (EFI_ERROR (Status)) {
    Error ("FMMT", 0, 0003, "error parsing FV image", "cannot find the first file in the FV image");
    return Status;
  } 

  while (CurrentFile != NULL) {
  
    //
    // Increment the number of files counter
    //
    NumberOfFiles++;
 
    //
    // Store FFS file Header information
    // 
    CurrentFv->FfsHeader[*FfsCount].Attributes       = CurrentFile->Attributes;
    CurrentFv->FfsHeader[*FfsCount].IntegrityCheck   = CurrentFile->IntegrityCheck;
    CurrentFv->FfsHeader[*FfsCount].Name             = CurrentFile->Name;
    CurrentFv->FfsHeader[*FfsCount].Size[0]          = CurrentFile->Size[0];
    CurrentFv->FfsHeader[*FfsCount].Size[1]          = CurrentFile->Size[1];
    CurrentFv->FfsHeader[*FfsCount].Size[2]          = CurrentFile->Size[2];
    CurrentFv->FfsHeader[*FfsCount].State            = CurrentFile->State;
    CurrentFv->FfsHeader[*FfsCount].Type             = CurrentFile->Type;
    CurrentFv->FfsHeader[*FfsCount].ExtendedSize     = CurrentFile->ExtendedSize;

    //
    // Display info about this file
    //
    Status = LibGetFileInfo (Fv, CurrentFile, ErasePolarity, CurrentFv, FvName, Level, FfsCount, ViewFlag);
    if (EFI_ERROR (Status)) {
      Error ("FMMT", 0, 0003, "error parsing FV image", "failed to parse a file in the FV");
      return Status;
    }   

    //
    // Get the next file
    //
    Status = FvBufFindNextFile (Fv, &Key, (VOID **) &CurrentFile);
    if (Status == EFI_NOT_FOUND) {
      CurrentFile = NULL;
    } else if (EFI_ERROR (Status)) {
      Error ("FMMT", 0, 0003, "error parsing FV image", "cannot find the next file in the FV image");
      return Status;
    }
  }  
  
  return EFI_SUCCESS;   
}

/**

  TODO: Add function description

  FvImage       - TODO: add argument description
  FileHeader    - TODO: add argument description
  ErasePolarity - TODO: add argument description

  EFI_SUCCESS - TODO: Add description for return value
  EFI_ABORTED - TODO: Add description for return value

**/
EFI_STATUS
LibGetFileInfo (
  EFI_FIRMWARE_VOLUME_HEADER  *FvImage,
  EFI_FFS_FILE_HEADER2        *CurrentFile,
  BOOLEAN                     ErasePolarity,
  FV_INFORMATION              *CurrentFv,
  CHAR8                       *FvName,  
  UINT8                       Level,
  UINT32                      *FfsCount,
  BOOLEAN                     ViewFlag
  )
{
  UINT32              FileLength;
  UINT8               FileState;
  UINT8               Checksum;
  EFI_FFS_FILE_HEADER2 BlankHeader;
  EFI_STATUS          Status;
  UINT8               GuidBuffer[PRINTED_GUID_BUFFER_SIZE];
  ENCAP_INFO_DATA     *LocalEncapData;
  ENCAP_INFO_DATA     *ParentEncapData;
  BOOLEAN             EncapDataNeedUpdateFlag;
  BOOLEAN             IsGeneratedFfs;
  UINT8               FfsFileHeaderSize;

  Status = EFI_SUCCESS;
  
  LocalEncapData  = NULL;
  EncapDataNeedUpdateFlag = TRUE;
  ParentEncapData  = NULL;
  IsGeneratedFfs   = FALSE;

  if (CurrentFile->Attributes & FFS_ATTRIB_LARGE_FILE) {
    FfsFileHeaderSize = sizeof (EFI_FFS_FILE_HEADER2);
    FileLength        = CurrentFile->ExtendedSize;
  } else {
    FfsFileHeaderSize = sizeof (EFI_FFS_FILE_HEADER);
    FileLength        = GetLength (CurrentFile->Size);
  }
  //
  // Check if we have free space
  //
  if (ErasePolarity) {
    memset (&BlankHeader, -1, FfsFileHeaderSize);
  } else {
    memset (&BlankHeader, 0, FfsFileHeaderSize);
  }
  
  //
  // Is this FV blank?
  //
  if (memcmp (&BlankHeader, CurrentFile, FfsFileHeaderSize) == 0) {
    return EFI_SUCCESS;
  }  
  
  //
  // Print file information.
  //
  FileState = GetFileState (ErasePolarity, (EFI_FFS_FILE_HEADER *)CurrentFile);
  PrintGuidToBuffer (&(CurrentFile->Name), GuidBuffer, PRINTED_GUID_BUFFER_SIZE, FALSE);
  if (FileState == EFI_FILE_DATA_VALID) {
    //
    // Calculate header checksum
    //
    Checksum  = CalculateSum8 ((UINT8 *) CurrentFile, FfsFileHeaderSize);
    Checksum  = (UINT8) (Checksum - CurrentFile->IntegrityCheck.Checksum.File);
    Checksum  = (UINT8) (Checksum - CurrentFile->State);
    if (Checksum != 0) {
      Error ("FMMT", 0, 0003, "error parsing FFS file", "FFS file with Guid %s has invalid header checksum", GuidBuffer);
      return EFI_ABORTED;
    }
	
    if (CurrentFile->Attributes & FFS_ATTRIB_CHECKSUM) {
      //
      // Calculate file checksum
      //
      Checksum  = CalculateSum8 ((UINT8 *) (CurrentFile + 1), FileLength - FfsFileHeaderSize);
      Checksum  = Checksum + CurrentFile->IntegrityCheck.Checksum.File;
      if (Checksum != 0) {
        Error ("FMMT", 0, 0003, "error parsing FFS file", "FFS file with Guid %s has invalid file checksum", GuidBuffer);
        return EFI_ABORTED;
      }
    } else {
      if (CurrentFile->IntegrityCheck.Checksum.File != FFS_FIXED_CHECKSUM) {
        Error ("FMMT", 0, 0003, "error parsing FFS file", "FFS file with Guid %s has invalid header checksum -- not set to fixed value of 0xAA", GuidBuffer);
        return EFI_ABORTED;
      }
    }
  } else {
    Error ("FMMT", 0, 0003, "error parsing FFS file", "FFS file with Guid %s has the invalid/unrecognized file state bits", GuidBuffer);
    return EFI_ABORTED;
  }

	Level += 1;
	
  if ((CurrentFile->Type != EFI_FV_FILETYPE_ALL) && (CurrentFile->Type != EFI_FV_FILETYPE_FFS_PAD)) {

    //
    // Put in encapsulate data information.
    //
    LocalEncapData = CurrentFv->EncapData;
    while (LocalEncapData!= NULL) {    
      if (LocalEncapData->Level == Level) {
        EncapDataNeedUpdateFlag = FALSE;
		while (LocalEncapData->RightNode != NULL){
			LocalEncapData = LocalEncapData->RightNode;
		}
        break;
      }     
	  ParentEncapData = LocalEncapData;
      LocalEncapData = LocalEncapData->NextNode;
    }
    
    if (EncapDataNeedUpdateFlag) {
      //
      // Construct the new ENCAP_DATA
      //
	  LocalEncapData = ParentEncapData;
      LocalEncapData->NextNode = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));

      if (LocalEncapData->NextNode == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }             

      LocalEncapData        = LocalEncapData->NextNode;

      LocalEncapData->Level = Level;
      LocalEncapData->Type  = FMMT_ENCAP_TREE_FFS;
      LocalEncapData->FvExtHeader = NULL;

      //
      // Store the header of FFS file.
      //
      LocalEncapData->Data     = malloc (FfsFileHeaderSize);
      if (LocalEncapData->Data == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }   
      
      memcpy (LocalEncapData->Data, CurrentFile, FfsFileHeaderSize);
      
      LocalEncapData->NextNode = NULL;  
	  LocalEncapData->RightNode = NULL;
    }else{
      //
      // Construct the new ENCAP_DATA
      //
      LocalEncapData->RightNode = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));

      if (LocalEncapData->RightNode == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }             

      LocalEncapData        = LocalEncapData->RightNode;

      LocalEncapData->Level = Level;
      LocalEncapData->Type  = FMMT_ENCAP_TREE_FFS;
	  LocalEncapData->FvExtHeader = NULL;

      //
      // Store the header of FFS file.
      //
      LocalEncapData->Data     = malloc (FfsFileHeaderSize);
      if (LocalEncapData->Data == NULL) {
        Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
        return EFI_ABORTED;
      }   
      
      memcpy (LocalEncapData->Data, CurrentFile, FfsFileHeaderSize);
      
      LocalEncapData->RightNode = NULL;  
      LocalEncapData->NextNode = NULL;  
    }

    /*if ( CurrentFile->Type == EFI_FV_FILETYPE_FREEFORM ){
      //printf("File type FREEFROM\n");
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;
      if (!ViewFlag) {
        LibGenFfsFile(CurrentFile, CurrentFv, FvName, Level, FfsCount, ErasePolarity);			
      }
    }else */if ( CurrentFile->Type == EFI_FV_FILETYPE_RAW){
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;
      //printf("File type RAW\n");
      if (!ViewFlag){
        LibGenFfsFile(CurrentFile, CurrentFv, FvName, Level, FfsCount, ErasePolarity);			
      }
    } /*else if ( CurrentFile->Type == EFI_FV_FILETYPE_SECURITY_CORE){
      //printf("File type SEC CORE\n");
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;
      if (!ViewFlag){;
        LibGenFfsFile(CurrentFile, CurrentFv, FvName, Level, FfsCount, ErasePolarity);			
      }
    } */else if( CurrentFile->Type == EFI_FV_FILETYPE_FFS_PAD){
      //EFI_FV_FILETYPE_FFS_PAD
    } else {
    //
    // All other files have sections
    //  
    Status = LibParseSection (
      (UINT8 *) ((UINTN) CurrentFile + FfsFileHeaderSize),
      FileLength - FfsFileHeaderSize,
      CurrentFv,
      FvName,
      CurrentFile,
      Level,
      Level,
      FfsCount,
      ViewFlag,
      ErasePolarity,
      &IsGeneratedFfs
      );
    }
    if (EFI_ERROR (Status)) {  
      printf ("ERROR: Parsing the FFS file.\n");
      return Status;
    }  
  }

  
  return EFI_SUCCESS;
}


/**

  Parses EFI Sections, if the view flag turn on, then will collect FFS section information
  and extract FFS files.

  @param[in]      SectionBuffer - Buffer containing the section to parse.
  @param[in]      BufferLength  - Length of SectionBuffer
  @param[in, out] CurrentFv
  @param[in]      FvName
  @param[in]      CurrentFile
  @param[in]      Level
  @param[in, out] FfsCount
  @param[in]      ViewFlag 
  @param[in]      ErasePolarity

  @retval       EFI_SECTION_ERROR - Problem with section parsing.
                      (a) compression errors
                      (b) unrecognized section 
  @retval       EFI_UNSUPPORTED - Do not know how to parse the section.
  @retval       EFI_SUCCESS - Section successfully parsed.
  @retval       EFI_OUT_OF_RESOURCES - Memory allocation failed.

--*/
EFI_STATUS
LibParseSection (
  UINT8                  *SectionBuffer,
  UINT32                 BufferLength,
  FV_INFORMATION         *CurrentFv,
  CHAR8                  *FvName,    
  EFI_FFS_FILE_HEADER2   *CurrentFile,
  UINT8                  Level,
  UINT8                  FfsLevel,
  UINT32                 *FfsCount,
  BOOLEAN                ViewFlag,
  BOOLEAN                ErasePolarity,
  BOOLEAN                *IsFfsGenerated
  )
{
  UINT32              ParsedLength;
  UINT8               *Ptr;
  UINT32              SectionLength;
  UINT32              UiSectionLength;
  EFI_SECTION_TYPE    Type;  
  EFI_STATUS          Status;
  CHAR8               *ExtractionTool;
  CHAR8               *ToolInputFile;
  CHAR8               *ToolOutputFile;
  CHAR8               *SystemCommandFormatString;
  CHAR8               *SystemCommand; 
  UINT8               *ToolOutputBuffer;
  UINT32              ToolOutputLength;
  CHAR16              *UIName;
  UINT32              UINameSize;
  BOOLEAN             HasDepexSection;
  UINT32              NumberOfSections;
  ENCAP_INFO_DATA     *LocalEncapData;
  CHAR8               *BlankChar;
  UINT8               *UncompressedBuffer;
  UINT32              UncompressedLength;  
  UINT8               *CompressedBuffer;
  UINT32              CompressedLength;
  UINT8               CompressionType;
  DECOMPRESS_FUNCTION DecompressFunction;
  GETINFO_FUNCTION    GetInfoFunction;
  UINT32              DstSize;
  UINT32              ScratchSize;
  UINT8               *ScratchBuffer;
  BOOLEAN             EncapDataNeedUpdata;
  CHAR8               *TempDir;
  CHAR8               *ToolInputFileFullName;
  CHAR8               *ToolOutputFileFullName;
  UINT8               Index;
  UINT8               LargeHeaderOffset;

  ParsedLength               = 0;
  ToolOutputLength           = 0;
  UINameSize                 = 0;
  NumberOfSections           = 0;
  UncompressedLength         = 0;
  CompressedLength           = 0;
  CompressionType            = 0;
  DstSize                    = 0;
  ScratchSize                = 0;
  Index                      = 0;
  Ptr                        = NULL;
  ExtractionTool             = NULL;
  ToolInputFile              = NULL;
  ToolOutputFile             = NULL;
  SystemCommand              = NULL;
  SystemCommandFormatString  = NULL;
  ToolOutputBuffer           = NULL;
  UIName                     = NULL;
  LocalEncapData             = NULL;
  BlankChar                  = NULL;
  UncompressedBuffer         = NULL;
  CompressedBuffer           = NULL;
  ScratchBuffer              = NULL;
  TempDir                    = NULL;
  ToolInputFileFullName      = NULL;
  ToolOutputFileFullName     = NULL;
  HasDepexSection            = FALSE;
  EncapDataNeedUpdata        = TRUE;
  LargeHeaderOffset          = 0;

  
  while (ParsedLength < BufferLength) {
    Ptr           = SectionBuffer + ParsedLength;

    SectionLength = GetLength (((EFI_COMMON_SECTION_HEADER *) Ptr)->Size);
    Type          = ((EFI_COMMON_SECTION_HEADER *) Ptr)->Type;
    
    //
    // This is sort of an odd check, but is necessary because FFS files are
    // padded to a QWORD boundary, meaning there is potentially a whole section
    // header worth of 0xFF bytes.
    //
    if (SectionLength == 0xffffff && Type == 0xff) {
      ParsedLength += 4;
      continue;
    }  
	//
	//If Size is 0xFFFFFF then ExtendedSize contains the size of the section.
	//
    if (SectionLength == 0xffffff) {
	  SectionLength     = ((EFI_COMMON_SECTION_HEADER2 *) Ptr)->ExtendedSize;
	  LargeHeaderOffset = sizeof (EFI_COMMON_SECTION_HEADER2) - sizeof (EFI_COMMON_SECTION_HEADER);
	} 
    
    switch (Type) {
         
    case EFI_SECTION_FIRMWARE_VOLUME_IMAGE:


      //
	  //save parent level FFS file's GUID name
	  //
	  LocalEncapData = CurrentFv->EncapData;
      while (LocalEncapData->NextNode != NULL) {    
        if (LocalEncapData->Level == FfsLevel) {
			for(Index=0;Index < (UINT32)(*FfsCount) && LocalEncapData->RightNode != NULL;Index++){
				LocalEncapData = LocalEncapData->RightNode;
			}
			if (LocalEncapData != NULL && LocalEncapData->FvExtHeader == NULL){
				LocalEncapData->FvExtHeader = (EFI_FIRMWARE_VOLUME_EXT_HEADER *) malloc (sizeof (EFI_FIRMWARE_VOLUME_EXT_HEADER));
				if (LocalEncapData->FvExtHeader == NULL) {
					Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
					return EFI_ABORTED;
				}   
				LocalEncapData->FvExtHeader->FvName.Data1 = CurrentFile->Name.Data1;
				LocalEncapData->FvExtHeader->FvName.Data2 = CurrentFile->Name.Data2;
				LocalEncapData->FvExtHeader->FvName.Data3 = CurrentFile->Name.Data3;
				LocalEncapData->FvExtHeader->FvName.Data4[0] = CurrentFile->Name.Data4[0];
				LocalEncapData->FvExtHeader->FvName.Data4[1] = CurrentFile->Name.Data4[1];
				LocalEncapData->FvExtHeader->FvName.Data4[2] = CurrentFile->Name.Data4[2];
				LocalEncapData->FvExtHeader->FvName.Data4[3] = CurrentFile->Name.Data4[3];
				LocalEncapData->FvExtHeader->FvName.Data4[4] = CurrentFile->Name.Data4[4];
				LocalEncapData->FvExtHeader->FvName.Data4[5] = CurrentFile->Name.Data4[5];
				LocalEncapData->FvExtHeader->FvName.Data4[6] = CurrentFile->Name.Data4[6];
				LocalEncapData->FvExtHeader->FvName.Data4[7] = CurrentFile->Name.Data4[7];
			}
            break;
        }     
        LocalEncapData = LocalEncapData->NextNode;
      }


      EncapDataNeedUpdata = TRUE;
    
      Level ++;
      			
	    NumberOfSections ++;
			
			//CurrentFv->FfsAttuibutes[*FfsCount].IsLeaf = FALSE;
			//
			// For FV image, doesn't generate its parent FFS.
			//
			*IsFfsGenerated = TRUE;
			
		  //
      // Put in encapsulate data information.
      //
      LocalEncapData = CurrentFv->EncapData;
      while (LocalEncapData->NextNode != NULL) {    
        if (LocalEncapData->Level == Level) {
          EncapDataNeedUpdata = FALSE;
          break;
        }     
        LocalEncapData = LocalEncapData->NextNode;
      }
      
      if (EncapDataNeedUpdata) {
			  //
			  // Put in this is an FFS with FV section
			  //
			  LocalEncapData = CurrentFv->EncapData;
			  while (LocalEncapData->NextNode != NULL) {
			    LocalEncapData = LocalEncapData->NextNode;
			  }
  			
        //
        // Construct the new ENCAP_DATA
        //
        LocalEncapData->NextNode = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));
        
        if (LocalEncapData->NextNode == NULL) {
          Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
          return EFI_ABORTED;
        }             
        
        LocalEncapData        = LocalEncapData->NextNode;

        LocalEncapData->Level = Level;
        LocalEncapData->Type  = FMMT_ENCAP_TREE_FV_SECTION;
        
        //
        // We don't need additional data for encapsulate this FFS but type.
        //
        LocalEncapData->Data        = NULL;
        LocalEncapData->FvExtHeader = NULL;
        LocalEncapData->NextNode    = NULL;
		LocalEncapData->RightNode = NULL;
      }

      Status = LibGetFvInfo ((UINT8*)((EFI_FIRMWARE_VOLUME_IMAGE_SECTION*)Ptr + 1) + LargeHeaderOffset, CurrentFv, FvName, Level, FfsCount, ViewFlag, TRUE);
      if (EFI_ERROR (Status)) {
        Error ("FMMT", 0, 0003, "printing of FV section contents failed", NULL);
        return EFI_SECTION_ERROR;
      }   
      break;

    case EFI_SECTION_COMPRESSION:
      Level ++;
			NumberOfSections ++;
			
			EncapDataNeedUpdata = TRUE;
		  //
      // Put in encapsulate data information.
      //
      LocalEncapData = CurrentFv->EncapData;
      while (LocalEncapData->NextNode != NULL) {    
        if (LocalEncapData->Level == Level) {
          EncapDataNeedUpdata = FALSE;
          break;
        }     
        LocalEncapData = LocalEncapData->NextNode;
      }
  		 
  		if (EncapDataNeedUpdata) {	
        //
        // Put in this is an FFS with FV section
        //
        LocalEncapData = CurrentFv->EncapData;
        while (LocalEncapData->NextNode != NULL) {
          LocalEncapData = LocalEncapData->NextNode;
        }

        //
        // Construct the new ENCAP_DATA
        //
        LocalEncapData->NextNode = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));

        if (LocalEncapData->NextNode == NULL) {
          Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
          return EFI_ABORTED;
          }             

        LocalEncapData        = LocalEncapData->NextNode;

        LocalEncapData->Level = Level;
        LocalEncapData->Type  = FMMT_ENCAP_TREE_COMPRESS_SECTION;

        //
        // Store the compress type
        //
        LocalEncapData->Data     = malloc (sizeof (UINT8));
        
        if (LocalEncapData->Data == NULL) {
          Error ("FMMT", 0, 0003, "Allocate memory failed", NULL);
          return EFI_OUT_OF_RESOURCES;
        }
        
        *(UINT8 *)LocalEncapData->Data     = ((EFI_COMPRESSION_SECTION *) (Ptr + LargeHeaderOffset))->CompressionType; 
        LocalEncapData->FvExtHeader = NULL;
        LocalEncapData->NextNode = NULL;
		LocalEncapData->RightNode = NULL;
      }	
      
      //
      // Process compressed section
      //
      //CurrentFv->FfsAttuibutes[*FfsCount].IsLeaf = FALSE;
      
      UncompressedBuffer  = NULL;
      CompressedLength    = SectionLength - sizeof (EFI_COMPRESSION_SECTION) - LargeHeaderOffset;
      UncompressedLength  = ((EFI_COMPRESSION_SECTION *) (Ptr + LargeHeaderOffset))->UncompressedLength;
      CompressionType     = ((EFI_COMPRESSION_SECTION *) (Ptr + LargeHeaderOffset))->CompressionType;  
      
      if (CompressionType == EFI_NOT_COMPRESSED) {
        //printf ("  Compression Type:  EFI_NOT_COMPRESSED\n");
        if (CompressedLength != UncompressedLength) {
          Error ("FMMT", 0, 0, "file is not compressed, but the compressed length does not match the uncompressed length", NULL);
          return EFI_SECTION_ERROR;
        }
        
        UncompressedBuffer = Ptr + sizeof (EFI_COMPRESSION_SECTION) + LargeHeaderOffset;
      } else if (CompressionType == EFI_STANDARD_COMPRESSION) {   
        GetInfoFunction     = EfiGetInfo;
        DecompressFunction  = EfiDecompress;     
        
        CompressedBuffer  = Ptr + sizeof (EFI_COMPRESSION_SECTION) + LargeHeaderOffset;  
        
        Status            = GetInfoFunction (CompressedBuffer, CompressedLength, &DstSize, &ScratchSize);
        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0003, "error getting compression info from compression section", NULL);
          return EFI_SECTION_ERROR;
        }

        if (DstSize != UncompressedLength) {
          Error ("FMMT", 0, 0003, "compression error in the compression section", NULL);
          return EFI_SECTION_ERROR;
        }     
           
        ScratchBuffer       = malloc (ScratchSize);
        UncompressedBuffer  = malloc (UncompressedLength);
        
        if ((ScratchBuffer == NULL) || (UncompressedBuffer == NULL)) {
          Error ("FMMT", 0, 0003, "Allocate memory failed", NULL);
          return EFI_OUT_OF_RESOURCES;
        }
        //
        // Decompress the section.
        //
        Status = DecompressFunction (
                  CompressedBuffer,
                  CompressedLength,
                  UncompressedBuffer,
                  UncompressedLength,
                  ScratchBuffer,
                  ScratchSize
                  );
        free (ScratchBuffer);
        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0003, "decompress failed", NULL);
          free (UncompressedBuffer);
          return EFI_SECTION_ERROR;
        }         
      } else {
        Error ("FMMT", 0, 0003, "unrecognized compression type", "type 0x%X", CompressionType);
        return EFI_SECTION_ERROR;
      }

      Status = LibParseSection (  UncompressedBuffer, 
                                  UncompressedLength,
                                  CurrentFv,
                                  FvName,
                                  CurrentFile,
                                  Level,
                                  FfsLevel,
                                  FfsCount,
                                  ViewFlag,
                                  ErasePolarity,
                                  IsFfsGenerated);

      if (CompressionType == EFI_STANDARD_COMPRESSION) {
        //
        // We need to deallocate Buffer
        //
        free (UncompressedBuffer);
      }

      if (EFI_ERROR (Status)) {
        Error (NULL, 0, 0003, "failed to parse section", NULL);
        return EFI_SECTION_ERROR;
      }
 
      break;	
			
    case EFI_SECTION_GUID_DEFINED:
      //
      // Process GUID defined 
      // looks up the appropriate tool to use for extracting
      // a GUID defined FV section.
      //
      Level ++;
			NumberOfSections ++;
			
      
 			EncapDataNeedUpdata = TRUE;
		  //
      // Put in encapsulate data information.
      //
      LocalEncapData = CurrentFv->EncapData;
      while (LocalEncapData->NextNode != NULL) {    
        if (LocalEncapData->Level == Level) {
          EncapDataNeedUpdata = FALSE;
          break;
        }     
        LocalEncapData = LocalEncapData->NextNode;
      }    
      if (EncapDataNeedUpdata)  {

        //
        // Put in this is an FFS with FV section
        //
        LocalEncapData = CurrentFv->EncapData;
        while (LocalEncapData->NextNode != NULL) {
          LocalEncapData = LocalEncapData->NextNode;
          }

        //
        // Construct the new ENCAP_DATA
        //
        LocalEncapData->NextNode = (ENCAP_INFO_DATA *) malloc (sizeof (ENCAP_INFO_DATA));

        if (LocalEncapData->NextNode == NULL) {
          Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
          return EFI_ABORTED;
        }             

        LocalEncapData        = LocalEncapData->NextNode;

        LocalEncapData->Level = Level;
        LocalEncapData->Type  = FMMT_ENCAP_TREE_GUIDED_SECTION;

        //
        // We don't need additional data for encapsulate this FFS but type.
        // include DataOffset + Attributes
        //

        LocalEncapData->Data     = (EFI_GUID *) malloc (sizeof (EFI_GUID) + 4);
        
        if (LocalEncapData->Data == NULL) {
          Error (NULL, 0, 4001, "Resource: Memory can't be allocated", NULL);
          return EFI_ABORTED;
        }        
        
        //
		// include guid attribute and dataoffset
		//
		memcpy (LocalEncapData->Data, &((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->SectionDefinitionGuid, sizeof (EFI_GUID) + 4);
        
        LocalEncapData->FvExtHeader = NULL;
        LocalEncapData->NextNode = NULL;
		LocalEncapData->RightNode = NULL;
      }
            
      //CurrentFv->FfsAttuibutes[*FfsCount].IsLeaf = FALSE;
      
      ExtractionTool =
        LookupGuidedSectionToolPath (
          mParsedGuidedSectionTools,
          &((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->SectionDefinitionGuid
          );
       
      if (ExtractionTool != NULL && (strcmp (ExtractionTool, "GenCrc32")) != 0) {

        TempDir = _getcwd (NULL, MAX_PATH);
        sprintf(TempDir, "%s/%s", TempDir, TEMP_DIR_NAME);
        _mkdir (TempDir);

        ToolInputFile  = CloneString (tmpname (NULL));
        ToolOutputFile = CloneString (tmpname (NULL));

    
        ToolInputFileFullName   = malloc (strlen("%s%s") + strlen(TempDir) + strlen(ToolInputFile) + 1);
        ToolOutputFileFullName  = malloc (strlen("%s%s") + strlen(TempDir) + strlen(ToolOutputFile) + 1);

        sprintf (ToolInputFileFullName, "%s/%s", TempDir, ToolInputFile);
        sprintf (ToolOutputFileFullName, "%s/%s", TempDir, ToolOutputFile);

        //
        // Construction 'system' command string
        //
        SystemCommandFormatString = "%s -d -o \"%s\" \"%s\"";
        SystemCommand = malloc (
          strlen (SystemCommandFormatString) +
          strlen (ExtractionTool) +
          strlen (ToolInputFileFullName) +
          strlen (ToolOutputFileFullName) + 
          1
          );
        sprintf (
          SystemCommand,
          SystemCommandFormatString,
          ExtractionTool,
          ToolOutputFileFullName,
          ToolInputFileFullName
          );
        free (ExtractionTool);

        Status = PutFileImage (
        ToolInputFileFullName,
        (CHAR8*) Ptr + ((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->DataOffset,
        SectionLength - ((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->DataOffset
        );

        if (HasDepexSection) {
          HasDepexSection = FALSE;
        } 

        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "unable to decoded GUIDED section", NULL);
          return EFI_SECTION_ERROR;
        }
        
        if (system (SystemCommand) != EFI_SUCCESS) {
          free (SystemCommand);
          return EFI_ABORTED;
        }
        free (SystemCommand);
        remove (ToolInputFileFullName);
        free (ToolInputFile);
        free (ToolInputFileFullName);
        ToolInputFile = NULL;
        ToolInputFileFullName = NULL;
                
        
        Status = GetFileImage (
                   ToolOutputFileFullName,
                   (CHAR8 **)&ToolOutputBuffer,
                   &ToolOutputLength
                   );
        remove (ToolOutputFileFullName);
        free (ToolOutputFile);
        free (ToolOutputFileFullName);
        ToolOutputFile = NULL;
        ToolOutputFileFullName = NULL;

        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "unable to read decoded GUIDED section", NULL);
          return EFI_SECTION_ERROR;
        }

        Status = LibParseSection (
                  ToolOutputBuffer,
                  ToolOutputLength,
                  CurrentFv,
                  FvName,
                  CurrentFile,
                  Level,
                  FfsLevel,
                  FfsCount,
                  ViewFlag,
                  ErasePolarity,
                  IsFfsGenerated
                  );
        if (EFI_ERROR (Status)) {
          Error (NULL, 0, 0003, "parse of decoded GUIDED section failed", NULL);
          return EFI_SECTION_ERROR;  
        }      
      } else if (!CompareGuid (&((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->SectionDefinitionGuid, &mEfiCrc32GuidedSectionExtractionProtocolGuid)){
          //
          // CRC32 guided section
          //
          Status = LibParseSection (
            Ptr + ((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->DataOffset,
            SectionLength - ((EFI_GUID_DEFINED_SECTION *) (Ptr + LargeHeaderOffset))->DataOffset,
            CurrentFv,
            FvName,
            CurrentFile,
            Level,
            FfsLevel,
            FfsCount,
            ViewFlag,
            ErasePolarity,
            IsFfsGenerated
            );
          if (EFI_ERROR (Status)) {
            Error (NULL, 0, 0003, "parse of CRC32 GUIDED section failed", NULL);
            return EFI_SECTION_ERROR;
          }              
      }else {
        //
        // We don't know how to parse it now.
        //
        Error ("FMMT", 0, 0003, "Error parsing section", \
                              "EFI_SECTION_GUID_DEFINED cannot be parsed at this time. Tool to decode this section should have been defined in Conf.ini file.");
        return EFI_UNSUPPORTED;      
      }       
      
      break;
		
		//
	  //Leaf sections
	  //
    case EFI_SECTION_RAW:
      NumberOfSections ++;
			CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;

			break;
    case EFI_SECTION_PE32:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;

      break;    				
    case EFI_SECTION_PIC:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;
			
      break;    			
    case EFI_SECTION_TE:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;
      break;   
			
    case EFI_SECTION_COMPATIBILITY16:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;
			
      break;   
	
    case EFI_SECTION_FREEFORM_SUBTYPE_GUID:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;  
      break; 
			
    case EFI_SECTION_VERSION:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;   
      break; 
    case EFI_SECTION_PEI_DEPEX:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;    
      HasDepexSection = TRUE;
      break;
    case EFI_SECTION_DXE_DEPEX:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;    
      HasDepexSection = TRUE;
      break;
    case EFI_SECTION_SMM_DEPEX:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;      
      HasDepexSection = TRUE;
      break;
			
    case EFI_SECTION_USER_INTERFACE:
      NumberOfSections ++;
      CurrentFv->FfsAttuibutes[*FfsCount].Level = Level;

      UiSectionLength = GetLength (((EFI_USER_INTERFACE_SECTION *) Ptr)->CommonHeader.Size);
	  if (UiSectionLength == 0xffffff) {
	    UiSectionLength   = ((EFI_USER_INTERFACE_SECTION2 *) Ptr)->CommonHeader.ExtendedSize;
		UINameSize        = UiSectionLength - sizeof(EFI_COMMON_SECTION_HEADER2);
	  } else {
	    UINameSize = UiSectionLength - sizeof(EFI_COMMON_SECTION_HEADER);
	  }
		  
      UIName     = (CHAR16 *) malloc (UINameSize + 2);
      memset (UIName, '\0', UINameSize + 2);
      if (UIName != NULL) {
	    if (UiSectionLength >= 0xffffff) {
          memcpy(UIName, ((EFI_USER_INTERFACE_SECTION2 *) Ptr)->FileNameString, UINameSize);
		} else {
		  memcpy(UIName, ((EFI_USER_INTERFACE_SECTION *) Ptr)->FileNameString, UINameSize);
		}
      } else {
        Error ("FMMT", 0, 0001, "Memory allocate error!", NULL);
      }
      
      BlankChar = LibConstructBlankChar( CurrentFv->FvLevel * 2);
      
      if (ViewFlag) {
        fprintf(stdout, "%sFile \"%S\"\n", BlankChar, UIName);
      }
      
      //
      // If Ffs file has been generated, then the FfsCount should decrease 1.
      //
      memcpy (CurrentFv->FfsAttuibutes[*FfsCount].UiName, UIName, UINameSize);
      
      
      HasDepexSection = FALSE;	
	  free(UIName);
	  UINameSize = 0;
					
      break;		
    default:
      break;
    }
    
    ParsedLength += SectionLength;
    //
    // We make then next section begin on a 4-byte boundary
    //
    ParsedLength = GetOccupiedSize (ParsedLength, 4);        
  } 
  
  if (ParsedLength < BufferLength) {
    Error ("FMMT", 0, 0003, "sections do not completely fill the sectioned buffer being parsed", NULL);
    return EFI_SECTION_ERROR;
  }
  
	if (!ViewFlag) {
	  if (!*IsFfsGenerated) {
	    LibGenFfsFile(CurrentFile, CurrentFv, FvName, Level, FfsCount, ErasePolarity);
	    *IsFfsGenerated = TRUE;
	  }
	}

  return EFI_SUCCESS; 
}

/*
  Generate the leaf FFS files.

*/
EFI_STATUS
LibGenFfsFile (
  EFI_FFS_FILE_HEADER2   *CurrentFile,
  FV_INFORMATION         *CurrentFv,
  CHAR8                  *FvName,
  UINT8                  Level,    
  UINT32                 *FfsCount,
  BOOLEAN                ErasePolarity
)
{
  UINT32                      FfsFileSize;
  CHAR8                       *FfsFileName;  
  FILE                        *FfsFile;
  CHAR8                       *TempDir;


  FfsFileSize   = 0;
  FfsFileName   = NULL;  
  FfsFile       = NULL;   
  TempDir       = NULL;
  
  TempDir = _getcwd (NULL, MAX_PATH);

  sprintf(TempDir, "%s/%s", TempDir, TEMP_DIR_NAME);

  _mkdir (TempDir);

  FfsFileName = (CHAR8 *) malloc (MAX_PATH);
  memset (FfsFileName, '\0', MAX_PATH);
  if (CurrentFile->Attributes & FFS_ATTRIB_LARGE_FILE) {
	FfsFileSize = CurrentFile->ExtendedSize;
  } else {
    FfsFileSize = FvBufExpand3ByteSize (CurrentFile->Size);
  }
  sprintf (
    (CHAR8 *)FfsFileName,
    "%s/Num%d-%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X-Level%d",
    TempDir,
    *FfsCount,
    (unsigned) CurrentFile->Name.Data1,
    CurrentFile->Name.Data2,
    CurrentFile->Name.Data3,
    CurrentFile->Name.Data4[0],
    CurrentFile->Name.Data4[1],
    CurrentFile->Name.Data4[2],
    CurrentFile->Name.Data4[3],
    CurrentFile->Name.Data4[4],
    CurrentFile->Name.Data4[5],
    CurrentFile->Name.Data4[6],
    CurrentFile->Name.Data4[7],
    Level
  );
  
  memcpy (CurrentFv->FfsAttuibutes[*FfsCount].FfsName, FfsFileName, strlen(FfsFileName));
  
  //
  // Update current FFS files file state.
  //   
  if (ErasePolarity) {
    CurrentFile->State = (UINT8)~(CurrentFile->State);
  }
  
  FfsFile = fopen (FfsFileName, "wb+");
  if (FfsFile == NULL) {
    Error ("FMMT", 0, 0003, "error writing FFS file", "cannot Create a new ffs file.");
    return EFI_ABORTED;
  }
  
  if (fwrite (CurrentFile, 1, FfsFileSize, FfsFile) != FfsFileSize) {
     Error ("FMMT", 0, 0004, "error writing FFS file", "cannot Create a new ffs file.");
     fclose(FfsFile);
     return EFI_ABORTED;
  }
  
  fclose(FfsFile);    
  free(FfsFileName);
  FfsFileName = NULL;
  
  CurrentFv->FfsNumbers  = *FfsCount;
  
  *FfsCount += 1;
  
  if (ErasePolarity) {
    CurrentFile->State = (UINT8)~(CurrentFile->State);
  }     
  
  return EFI_SUCCESS;
}


/**

  This function returns the next larger size that meets the alignment 
  requirement specified.

  @param[in]      ActualSize      The size.
  @param[in]      Alignment       The desired alignment.
    
  @retval         EFI_SUCCESS     Function completed successfully.
  @retval         EFI_ABORTED     The function encountered an error.

**/
UINT32
GetOccupiedSize (
  IN UINT32  ActualSize,
  IN UINT32  Alignment
  )
{
  UINT32  OccupiedSize;

  OccupiedSize = ActualSize;
  while ((OccupiedSize & (Alignment - 1)) != 0) {
    OccupiedSize++;
  }

  return OccupiedSize;
}



EFI_STATUS 
LibDeleteAndRenameFfs(
  IN CHAR8*    DeleteFile,
  IN CHAR8*    NewFile
)
{
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand;
  CHAR8*                 TemDir;
    
  
  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;
  
  if (DeleteFile == NULL ||
      NewFile    == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  //
  // Delete the file of the specified extract FFS file. 
  //
  SystemCommandFormatString = "del \"%s\"";

  SystemCommand = malloc (
    strlen (SystemCommandFormatString) +
    strlen (DeleteFile)  +
    1
    );

  sprintf (
    SystemCommand,
    SystemCommandFormatString,
    DeleteFile 
    ); 

  if (system (SystemCommand) != EFI_SUCCESS) {
    free(SystemCommand); 
    return EFI_ABORTED;
  }
  free(SystemCommand);      
  
  TemDir = _getcwd (NULL, MAX_PATH);

  sprintf(TemDir, "%s/%s", TemDir, TEMP_DIR_NAME);

  _mkdir (TemDir);  
  //
  // Create a copy the new file. 
  //  
  SystemCommandFormatString = "copy \"%s\" \"%s\"";
  SystemCommand = malloc (
    strlen (SystemCommandFormatString) +
    strlen (NewFile)    +
    strlen (DeleteFile) +
    1
    );

  sprintf (
    SystemCommand,
    SystemCommandFormatString,
    NewFile,
    DeleteFile 
    ); 

  if (system (SystemCommand) != EFI_SUCCESS) {
	free(SystemCommand);
    return EFI_ABORTED;
  } 
  free(SystemCommand); 

  return EFI_SUCCESS;
  
}

/**
  Converts ASCII characters to Unicode.
  Assumes that the Unicode characters are only these defined in the ASCII set.

  String      - Pointer to string that is written to FILE.
  UniString   - Pointer to unicode string
  
  The address to the ASCII string - same as AsciiStr.

**/
VOID
LibAscii2Unicode (
  IN   CHAR8          *String,
  OUT  CHAR16         *UniString
  )
{
  while (*String != '\0') {
    *(UniString++) = (CHAR16) *(String++);
    }
  //
  // End the UniString with a NULL.
  //
  *UniString = '\0';
}


EFI_STATUS 
LibCreateGuidedSectionOriginalData(
  IN CHAR8*    FileIn,
  IN CHAR8*    ToolName,
  IN CHAR8*    FileOut
)
{
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand;
  
  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;
  
  if (FileIn   == NULL ||
      ToolName == NULL ||
      FileOut  == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  //
  // Delete the file of the specified extract FFS file. 
  //
  SystemCommandFormatString = "%s -e \"%s\" -o \"%s\"";

  SystemCommand = malloc (
    strlen (SystemCommandFormatString) +
    strlen (FileIn)  +
    strlen (ToolName)  +
    strlen (FileOut)  +
    1
    );

  sprintf (
    SystemCommand,
    SystemCommandFormatString,
    ToolName,
    FileIn,
    FileOut
    ); 

  if (system (SystemCommand) != EFI_SUCCESS) {
	free(SystemCommand);
    return EFI_ABORTED;
  }   
  free(SystemCommand);
  
  return EFI_SUCCESS;
}

/**

   This function convert the FV header's attribute to a string. The converted string
   will be put into an INF file as the input of GenFV.
   
   @param[in]      Attr       FV header's attribute.
   @param[out]     InfFile    InfFile contain FV header attribute information.
   
   @retval         EFI_SUCCESS.
   @retval         EFI_INVLID_PARAMETER
   @retval         EFI_OUT_OF_RESOURCES

**/
EFI_STATUS
LibFvHeaderAttributeToStr (
  IN     EFI_FVB_ATTRIBUTES_2     Attr,
  IN     FILE*                  InfFile 
)
{
  CHAR8     *LocalStr;
  
  LocalStr  = NULL;
  
  LocalStr = (CHAR8 *) malloc (1024 * 4);
  
  if (LocalStr == NULL) {
    printf ("Memory allocate error!\n");
    return EFI_OUT_OF_RESOURCES;
  }
  
  memset (LocalStr, '\0', 1024 * 4);
  
  if (Attr == 0 || InfFile  == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  strncat (LocalStr, "[attributes] \n", sizeof("[attributes] \n"));
  
  if (Attr & EFI_FVB2_READ_DISABLED_CAP) {
    strncat (LocalStr, "EFI_READ_DISABLED_CAP = TRUE \n", sizeof ("EFI_READ_DISABLED_CAP = TRUE \n"));
  } 
  
  if (Attr & EFI_FVB2_READ_ENABLED_CAP) {
    strncat (LocalStr, "EFI_READ_ENABLED_CAP = TRUE \n", sizeof ("EFI_READ_ENABLED_CAP = TRUE \n"));
  }

  if (Attr & EFI_FVB2_READ_STATUS) {
    strncat (LocalStr, "EFI_READ_STATUS = TRUE \n", sizeof ("EFI_READ_STATUS = TRUE \n"));
  }
  
  if (Attr & EFI_FVB2_WRITE_DISABLED_CAP) {
    strncat (LocalStr, "EFI_WRITE_DISABLED_CAP = TRUE \n", sizeof ("EFI_WRITE_DISABLED_CAP = TRUE \n"));
  } 
  
  if (Attr & EFI_FVB2_WRITE_ENABLED_CAP) {
    strncat (LocalStr, "EFI_WRITE_ENABLED_CAP = TRUE \n", sizeof ("EFI_WRITE_ENABLED_CAP = TRUE \n"));
  }

  if (Attr & EFI_FVB2_WRITE_STATUS) {
    strncat (LocalStr, "EFI_WRITE_STATUS = TRUE \n", sizeof ("EFI_WRITE_STATUS = TRUE \n"));
  }
   
  if (Attr & EFI_FVB2_LOCK_CAP) {
    strncat (LocalStr, "EFI_LOCK_CAP = TRUE \n", sizeof ("EFI_LOCK_CAP = TRUE \n"));
  }
  
  if (Attr & EFI_FVB2_LOCK_STATUS) {
    strncat (LocalStr, "EFI_LOCK_STATUS = TRUE \n", sizeof ("EFI_LOCK_STATUS = TRUE \n"));
  }

  if (Attr & EFI_FVB2_STICKY_WRITE) {
    strncat (LocalStr, "EFI_STICKY_WRITE = TRUE \n", sizeof ("EFI_STICKY_WRITE = TRUE \n"));
  }  

  if (Attr & EFI_FVB2_MEMORY_MAPPED) {
    strncat (LocalStr, "EFI_MEMORY_MAPPED = TRUE \n", sizeof ("EFI_MEMORY_MAPPED = TRUE \n"));
  }
    
  if (Attr & EFI_FVB2_ERASE_POLARITY) {
    strncat (LocalStr, "EFI_ERASE_POLARITY = 1 \n", sizeof ("EFI_ERASE_POLARITY = 1 \n"));
  }
  
  if (Attr & EFI_FVB2_READ_LOCK_CAP) {
    strncat (LocalStr, "EFI_READ_LOCK_CAP = TRUE \n", sizeof ("EFI_READ_LOCK_CAP = TRUE \n"));
  }
    
  if (Attr & EFI_FVB2_READ_LOCK_STATUS) {
    strncat (LocalStr, "EFI_READ_LOCK_STATUS = TRUE \n", sizeof ("EFI_READ_LOCK_STATUS = TRUE \n"));
  }
 
  if (Attr & EFI_FVB2_WRITE_LOCK_CAP) {
    strncat (LocalStr, "EFI_WRITE_LOCK_CAP = TRUE \n", sizeof ("EFI_WRITE_LOCK_CAP = TRUE \n"));
  } 

  if (Attr & EFI_FVB2_WRITE_LOCK_STATUS) {
    strncat (LocalStr, "EFI_WRITE_LOCK_STATUS = TRUE \n", sizeof ("EFI_WRITE_LOCK_STATUS = TRUE \n"));
  }  
  
  if (Attr & EFI_FVB2_LOCK_STATUS) {
    strncat (LocalStr, "EFI_READ_LOCK_STATUS = TRUE \n", sizeof ("EFI_READ_LOCK_STATUS = TRUE \n"));
  } 
 
  //
  // Alignment
  //
  if (Attr & EFI_FVB2_ALIGNMENT_1) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_1 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_1 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_2) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_2 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_2 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_4) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_4 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_4 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_8) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_8 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_8 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_16) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_16 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_16 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_32) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_32 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_32 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_64) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_64 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_64 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_128) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_128 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_128 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_256) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_256 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_256 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_512) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_512 = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_512 = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_1K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_1K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_1K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_2K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_2K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_2K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_4K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_4K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_4K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_8K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_8K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_8K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_16K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_16K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_16K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_32K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_32K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_32K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_64K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_64K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_64K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_128K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_128K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_128K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_256K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_256K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_256K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_512K) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_512K = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_512K = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_1M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_1M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_1M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_2M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_2M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_2M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_4M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_4M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_4M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_8M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_8M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_8M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_16M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_16M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_16M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_32M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_32M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_32M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_64M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_64M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_64M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_128M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_128M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_128M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_256M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_256M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_256M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_512M) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_512M = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_512M = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_1G) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_1G = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_1G = TRUE \n"));
  } else if (Attr & EFI_FVB2_ALIGNMENT_2G) {
    strncat (LocalStr, "EFI_FVB2_ALIGNMENT_2G = TRUE \n", sizeof ("EFI_FVB2_ALIGNMENT_2G = TRUE \n"));
  }                               
   
  if (fwrite (LocalStr, 1, (size_t) strlen (LocalStr), InfFile) != (size_t) strlen (LocalStr)) {
    printf ("Error while writing data to %s file.", "InfFile");
	return EFI_ABORTED;
  }
  
  free (LocalStr);
  
  return EFI_SUCCESS; 
}


/**
   This function fill the FV inf files option field.
   
   @param[in]      BlockMap       FV header's attribute.
   @param[out]     InfFile    InfFile contain FV header attribute information.
   
   @retval         EFI_SUCCESS.
   @retval         EFI_INVLID_PARAMETER

**/
EFI_STATUS
LibFvHeaderOptionToStr (
  IN     EFI_FV_BLOCK_MAP_ENTRY  *BlockMap,
  IN     FILE*                   InfFile,
  IN     BOOLEAN                 IsRootFv
)
{
  CHAR8     *LocalStr;
  CHAR8     *BlockSize;
  CHAR8     *NumOfBlocks;
  
  LocalStr     = NULL;
  BlockSize    = NULL;
  NumOfBlocks  = NULL;
  
  if (BlockMap == NULL || InfFile  == NULL) {
    return EFI_INVALID_PARAMETER;
  }  
  
  //
  // This section will not over 1024 bytes and each line will never over 128 bytes. 
  //
  LocalStr    = (CHAR8 *) malloc (1024);
  BlockSize   = (CHAR8 *) malloc (128);
  NumOfBlocks = (CHAR8 *) malloc (128);
  
  if (LocalStr    == NULL ||
      BlockSize   == NULL ||
      NumOfBlocks == NULL) {
    printf ("Memory allocate error! \n");
    return EFI_OUT_OF_RESOURCES;
  }
  
  memset (LocalStr, '\0', 1024);
  memset (BlockSize, '\0', 128);
  memset (NumOfBlocks, '\0', 128); 
  
  strncat (LocalStr, "[options] \n", sizeof("[Options] \n"));  
  
  sprintf (BlockSize, "EFI_BLOCK_SIZE  = 0x%x \n", BlockMap->Length); 
  strncat (LocalStr, BlockSize, strlen(BlockSize));
  
  if (IsRootFv) {
  sprintf (NumOfBlocks, "EFI_NUM_BLOCKS  = 0x%x \n", BlockMap->NumBlocks);
  strncat (LocalStr, NumOfBlocks, strlen(NumOfBlocks));
  }

  if (fwrite (LocalStr, 1, (size_t) strlen (LocalStr), InfFile) != (size_t) strlen (LocalStr)) {
    printf ("Error while writing data to %s file.", "InfFile");
    free (LocalStr);
    free (BlockSize);
    free (NumOfBlocks);
    return EFI_ABORTED;
  }  
  
  free (LocalStr);
  free (BlockSize);
  free (NumOfBlocks);
  
  return EFI_SUCCESS;
}

/**
   This function fill the FV inf files option field.
   
   @param[in]      FfsName    Ffs file path/name.
   @param[out]     InfFile    InfFile contain FV header attribute information
   @param[in]      FirstIn    Is the first time call this function? If yes, should create [files] section.
   
   @retval         EFI_SUCCESS.
   @retval         EFI_INVLID_PARAMETER

**/
EFI_STATUS
LibAddFfsFileToFvInf (
  IN     CHAR8                   *FfsName,
  IN     FILE*                   InfFile,
  IN     BOOLEAN                 FirstIn
)
{

  CHAR8     *LocalStr;
  
  LocalStr     = NULL;  
  
  if (FfsName == NULL || InfFile  == NULL) {
    return EFI_INVALID_PARAMETER;
  }  
  
  if (strlen(FfsName) == 0) {
    return EFI_SUCCESS;
  }
  
  LocalStr    = (CHAR8 *) malloc (MAX_PATH);  
  
  if (LocalStr == NULL) {
    printf ("Memory allocate error! \n");
    return EFI_OUT_OF_RESOURCES;
  }  
  
  memset (LocalStr, '\0', MAX_PATH);
  
  if (FirstIn) {
    sprintf (LocalStr, "[files] \nEFI_FILE_NAME = %s \n", FfsName);
  } else {
    sprintf (LocalStr, "EFI_FILE_NAME = %s \n", FfsName);
  }
  
  if (fwrite (LocalStr, 1, (size_t) strlen (LocalStr), InfFile) != (size_t) strlen (LocalStr)) {
    printf ("Error while writing data to %s file.", "InfFile");
    free (LocalStr);
    return EFI_ABORTED;
  }  
  
  free (LocalStr);
  
  return EFI_SUCCESS;    
}


/**
  Convert EFI file to PE or TE section
  
  @param[in]   InputFilePath   .efi file, it's optional unless process PE/TE section.
  @param[in]   Type            PE or TE and UI/Version
  @param[in]   OutputFilePath  .te or .pe file
  @param[in]   UiString        String for generate UI section usage, this parameter is optional
                               unless Type is EFI_SECTION_USER_INTERFACE.
  @param[in]   VerString       String for generate Version section usage, this parameter is optional
                               unless Type is EFI_SECTION_VERSION.  

  @retval EFI_SUCCESS  

**/
EFI_STATUS
LibCreateFfsSection (
  IN CHAR8*     InputFilePath,      OPTIONAL
  IN CHAR8*     Sections,           OPTIONAL
  IN UINT8      Type,
  IN CHAR8*     OutputFilePath,
  IN CHAR8*     UiString,           OPTIONAL
  IN CHAR8*     VerString,          OPTIONAL
  IN CHAR8*     GuidToolGuid,       OPTIONAL
  IN UINT16     GuidHeaderLength,
  IN UINT16     GuidAttr,
  IN CHAR8*     CompressType       OPTIONAL
  )
{
  //EFI_STATUS             Status;
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand;
    
  
  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;
  
  
  //
  // Call GenSec tool to generate FFS section.
  //
  
  //
  // -s SectionType. 
  //
  if (Type != 0) {
    switch (Type) {  
      //
      // Process compression section
      //
      case EFI_SECTION_COMPRESSION:
        SystemCommandFormatString = "GenSec -s %s -c %s  \"%s\" -o \"%s\"";
        SystemCommand = malloc (
          strlen (SystemCommandFormatString) +
          strlen (mSectionTypeName[Type]) +
          strlen (CompressType) +
          strlen (InputFilePath) +
          strlen (OutputFilePath) +
          1
          );
        sprintf (
          SystemCommand,
          SystemCommandFormatString,
          mSectionTypeName[Type],
          CompressType,
          InputFilePath,
          OutputFilePath
          ); 

        if (system (SystemCommand) != EFI_SUCCESS) {
          free(SystemCommand);
          return EFI_ABORTED;
        }
        free(SystemCommand);         
        break;
      
      //
      // Process GUID defined section
      //
      case EFI_SECTION_GUID_DEFINED:
        SystemCommandFormatString = "GenSec -s %s -g %s \"%s\" -o \"%s\" -r %s -r %s -l %d";
        SystemCommand = malloc (
          strlen (SystemCommandFormatString) +
          strlen (mSectionTypeName[Type]) +
          strlen (GuidToolGuid) +
          strlen (InputFilePath) +
          strlen (OutputFilePath) +
          strlen (mGuidSectionAttr[GuidAttr&0x01]) +
          strlen (mGuidSectionAttr[GuidAttr&0x02]) +
          4 +
          1
          );
        sprintf (
          SystemCommand,
          SystemCommandFormatString,
          mSectionTypeName[Type],
          GuidToolGuid,
          InputFilePath,
          OutputFilePath,
          mGuidSectionAttr[GuidAttr&0x01],
          mGuidSectionAttr[GuidAttr&0x02],
          GuidHeaderLength
          ); 
        
        if (system (SystemCommand) != EFI_SUCCESS) {
          free(SystemCommand);
          return EFI_ABORTED;
        }
        free(SystemCommand);         
        break;

      case EFI_SECTION_FIRMWARE_VOLUME_IMAGE:
      
        SystemCommandFormatString = "GenSec -s %s \"%s\" -o \"%s\"";
        SystemCommand = malloc (
          strlen (SystemCommandFormatString) +
          strlen (mSectionTypeName[Type]) +
          strlen (InputFilePath) +
          strlen (OutputFilePath) +
          1
          );
        sprintf (
          SystemCommand,
          SystemCommandFormatString,
          mSectionTypeName[Type],
          InputFilePath,
          OutputFilePath
          ); 

        if (system (SystemCommand) != EFI_SUCCESS) {
          free(SystemCommand);
          return EFI_ABORTED;
        }
        free(SystemCommand);    
        break;      
        
      default:
        Error ("FMMT", 0, 0003, "Please specify the section type while call GenSec tool.", NULL);
        return EFI_UNSUPPORTED;
    }
  } else {
    //
    // Create Dummy section.
    //
    SystemCommandFormatString = "GenSec --sectionalign 16 \"%s\" -o \"%s\"";
    SystemCommand = malloc (
      strlen (SystemCommandFormatString) +
      strlen (InputFilePath) +
      strlen (OutputFilePath) +
      1
      );
    sprintf (
      SystemCommand,
      SystemCommandFormatString,
      InputFilePath,
      OutputFilePath
      ); 

    if (system (SystemCommand) != EFI_SUCCESS) {
      free(SystemCommand);
      return EFI_ABORTED;
    }
    free(SystemCommand);    
     
  }
 
  return EFI_SUCCESS;  
}

/**
  Encapsulate FFSs to FV

  @param[in]   InputFilePath   Section file will be read into this FFS file. This option is required.     
  @param[in]   OutputFilePath  The created PI firmware file name. This option is required.
  @param[in]   BlockSize       BlockSize is one HEX or DEC format value required by FV image.
  @param[in]   FileTakeSize   

  @retval EFI_SUCCESS  

**/
EFI_STATUS
LibEncapsulateFfsToFv (
  IN CHAR8*     InfFilePath, 
  IN CHAR8*     InputFFSs,
  IN CHAR8*     OutputFilePath,
  IN CHAR8*     FvGuidName
  )
{
  
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand; 
  CHAR8*                 FfsGuid = "8c8ce578-8a3d-4f1c-9935-896185c32dd3";

  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;

  if (OutputFilePath  == NULL ||
      InfFilePath     == NULL ) {
    return EFI_INVALID_PARAMETER;    
  }
  
  if (InfFilePath != NULL) {
    if (FvGuidName == NULL) {
      SystemCommandFormatString = "GenFv -i \"%s\" -o \"%s\"";

      SystemCommand = malloc (
        strlen (SystemCommandFormatString) +
        strlen (InfFilePath)   +
        strlen (OutputFilePath)  +
        1
        );

      sprintf (
        SystemCommand,
        SystemCommandFormatString,
        InfFilePath,          // -i   
        OutputFilePath        // -o
        ); 
 
      if (system (SystemCommand) != EFI_SUCCESS) {
        free(SystemCommand);
        return EFI_ABORTED;
      }

      free(SystemCommand);
    } else {
      //
      // Have FvGuidName in it.
      //
      SystemCommandFormatString = "GenFv -i \"%s\" -o \"%s\" --FvNameGuid %s";

      SystemCommand = malloc (
        strlen (SystemCommandFormatString) +
        strlen (InfFilePath)   +
        strlen (OutputFilePath)  +
        strlen (FvGuidName) +
        1
        );

      sprintf (
        SystemCommand,
        SystemCommandFormatString,
        InfFilePath,          // -i   
        OutputFilePath,       // -o
        FvGuidName            // FvNameGuid
        ); 

      if (system (SystemCommand) != EFI_SUCCESS) {
        free(SystemCommand);
        return EFI_ABORTED;
      }
      free(SystemCommand);      

    }
  }
  
  if (InputFFSs != NULL) {
    SystemCommandFormatString = "GenFv -f \"%s\" -g %s -o \"%s\"";
    SystemCommand = malloc (
      strlen (SystemCommandFormatString) +
      strlen (InputFFSs)   +
      strlen (FfsGuid)         +
      strlen (OutputFilePath)  +
      100
      );

    sprintf (
      SystemCommand,
      SystemCommandFormatString,
      InputFFSs,              // -f   
      FfsGuid,                // -g
      OutputFilePath          // -o
      ); 

    if (system (SystemCommand) != EFI_SUCCESS) {
      free(SystemCommand);
      return EFI_ABORTED;
    }  
    free(SystemCommand);      
  }
  
   
  return EFI_SUCCESS;  
}

/**
  Encapsulate an FFS section file to an FFS file.
  
  @param[in]   Type            Type is one FV file type defined in PI spec, which is one type of EFI_FV_FILETYPE_RAW, EFI_FV_FILETYPE_FREEFORM, 
                               EFI_FV_FILETYPE_SECURITY_CORE, EFI_FV_FILETYPE_PEIM, EFI_FV_FILETYPE_PEI_CORE, EFI_FV_FILETYPE_DXE_CORE, 
                               EFI_FV_FILETYPE_DRIVER, EFI_FV_FILETYPE_APPLICATION, EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER,
                               EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE. This option is required.         
  @param[in]   InputFilePath   Section file will be read into this FFS file. This option is required.     
  @param[in]   OutputFilePath  The created PI firmware file name. This option is required.
  @param[in]   FileGuid        FileGuid is the unique identifier for this FFS file. This option is required.
  @param[in]   Fixed           Set fixed attribute in FFS file header to indicate that the file may not be moved from its present location.
  @param[in]   SectionAlign    FileAlign specifies FFS file alignment, which only support the following alignment: 8,16,128,512,1K,4K,32K,64K.

  @retval EFI_SUCCESS  

**/
EFI_STATUS
LibEncapSectionFileToFFS (
  IN UINT8      Type,
  IN CHAR8*     InputFilePath,  
  IN CHAR8*     OutputFilePath,
  IN EFI_GUID   FileGuid,
  IN BOOLEAN    Fixed,
  IN UINT32     SectionAlign    
  )
{
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand;
  CHAR8*                 GuidStr;
    
  
  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;
  GuidStr                   = NULL;
  
  GuidStr  = LibFmmtGuidToStr(&FileGuid);
  
  
  //
  // -t  Type
  // -i  InputFilePath
  // -o  OutPutFilePath
  // -g  FileGuid
  // -x  Fixed
  // -n  SectionAlign
  //
  
  if (Fixed) {
    SystemCommandFormatString = "GenFfs -t %s -i \"%s\" -g %s -x -o \"%s\"";
    SystemCommand = malloc (
      strlen (SystemCommandFormatString) +
      strlen (mFfsFileType[Type]) +
      strlen (InputFilePath) +
      strlen (GuidStr) +
      strlen (OutputFilePath) +
      1
      );
    sprintf (
      SystemCommand,
      SystemCommandFormatString,
      mFfsFileType[Type],     // -t
      InputFilePath,          // -i 
      GuidStr,                // -g
      OutputFilePath          // -o
      ); 

    if (system (SystemCommand) != EFI_SUCCESS) {
      free(SystemCommand);
      return EFI_ABORTED;
    }   
    free(SystemCommand);     
  } else {
    SystemCommandFormatString = "GenFfs -t %s -i \"%s\" -g %s -o \"%s\"";
    SystemCommand = malloc (
      strlen (SystemCommandFormatString) +
      strlen (mFfsFileType[Type]) +
      strlen (InputFilePath) +
      strlen (GuidStr) +
      strlen (OutputFilePath) +
      1
      );
    sprintf (
      SystemCommand,
      SystemCommandFormatString,
      mFfsFileType[Type],     // -t
      InputFilePath,          // -i  
      GuidStr,                // -g
      OutputFilePath          // -o
      ); 

    if (system (SystemCommand) != EFI_SUCCESS) {
      free(SystemCommand);
      return EFI_ABORTED;
    } 
    free(SystemCommand);     
  }
  
  
  return EFI_SUCCESS;
}

EFI_STATUS 
LibCreateNewFdCopy(
  IN CHAR8*    OldFd,
  IN CHAR8*    NewFd
)
{

  FILE*                       NewFdFile;
  FILE*                       OldFdFile;
  CHAR8                       *NewFdDir;
  CHAR8                       *OldFdDir;
  UINT64                      FdLength;
  UINT32                      Count;
  BOOLEAN                     UseNewDirFlag;
  CHAR8                       *Buffer;
  
  NewFdFile = NULL;
  OldFdFile = NULL;
  NewFdDir  = NULL;
  OldFdDir  = NULL;
  Count     = 0;
  UseNewDirFlag = FALSE;

  if (OldFd == NULL ||
    NewFd    == NULL) {
      return EFI_INVALID_PARAMETER;
  }
  
  
  NewFdDir = _getcwd (NULL, MAX_PATH);

  Count = strlen(NewFdDir);
  
  if (strlen(NewFd) > Count) {

    do {
      if (NewFdDir[Count-1] == NewFd[Count-1]) {
        Count--;
      } else {
        sprintf(NewFdDir, "%s/%s", NewFdDir, NewFd);
        UseNewDirFlag = TRUE;
        break;
      }

    } while (Count != 1);

  }else {
    sprintf(NewFdDir, "%s/%s", NewFdDir, NewFd);
    UseNewDirFlag = TRUE;
  }

  if (UseNewDirFlag) {
    NewFdFile = fopen (NewFdDir, "wb+");
    if (NewFdFile == NULL) {
      NewFdFile = fopen (NewFd, "wb+");
    }
  } else {
    NewFdFile = fopen (NewFd, "wb+");
  }
  // support network path file  
  if (OldFd[0] == '/' && OldFd[1] == '/') {
	  OldFdFile = fopen (OldFd, "rb");
  } else {
  UseNewDirFlag = FALSE;

  OldFdDir = _getcwd (NULL, MAX_PATH);

  Count = strlen(OldFdDir);

  if (strlen(OldFd) > Count) {

    do {
      if (OldFdDir[Count-1] == OldFd[Count-1]) {
        Count--;
      } else {
        sprintf(OldFdDir, "%s/%s", OldFdDir, OldFd);
        UseNewDirFlag = TRUE;
        break;
      }

    } while (Count != 1);

  }else {
    sprintf(OldFdDir, "%s/%s", OldFdDir, OldFd);
    UseNewDirFlag = TRUE;
  }

  if (UseNewDirFlag) {
    OldFdFile = fopen (OldFdDir, "rb+");
    if (OldFdFile == NULL) {
      OldFdFile = fopen (OldFd, "rb+");
    }
  } else {
    OldFdFile = fopen (OldFd, "rb+");
  }
  }

  if (NewFdFile == NULL) {
    Error ("FMMT", 0, 0003, "error Open FD file", "cannot Create a new FD file.");
    return EFI_ABORTED;
  } 

  if (OldFdFile == NULL) {
    Error ("FMMT", 0, 0003, "error Open FD file", "cannot Create a new FD file.");
    return EFI_ABORTED;
  } 


  fseek(OldFdFile,0,SEEK_SET);
  fseek(OldFdFile,0,SEEK_END);

  FdLength = ftell(OldFdFile);

  fseek(OldFdFile,0,SEEK_SET);
  fseek(NewFdFile,0,SEEK_SET);
  
  Buffer = malloc ((size_t)FdLength);

  if (Buffer == NULL)  {
    return EFI_ABORTED;
  }

  if (fread (Buffer, 1, (size_t) FdLength, OldFdFile) != (size_t) FdLength) {
    Error ("FMMT", 0, 0003, "error reading FD file %s", OldFd);
    return EFI_ABORTED;    
  }

  if (fwrite (Buffer, 1, (size_t) FdLength, NewFdFile) != (size_t) FdLength) {
    Error ("FMMT", 0, 0004, "error writing FD file", "cannot Create a new FD file.");
    fclose(OldFdFile);
    fclose(NewFdFile);
    return EFI_ABORTED;
  }

  fclose(OldFdFile);
  fclose (NewFdFile);  

  return EFI_SUCCESS; 
}


/**
  This function will assemble the filename, directory and extend and return the combined string.
  Like FileName = file1, Dir = c:\temp extend = txt, the output string will be:
  c:\temp\file1.txt.
  
  @param[in]
  @param[in]
  @param[in]
  
  @retrun     A string contain all the input information.
  
**/
CHAR8 *
LibFilenameStrExtended (
  IN CHAR8      *FileName,
  IN CHAR8      *Dir,
  IN CHAR8      *Extend
) 
{
  CHAR8 *RetStr;
  
  RetStr = NULL;
  
  if (FileName == NULL) {
    return NULL;
  }
  
  if (Dir == NULL || Extend == NULL) {
    return FileName;
  }
  
  RetStr = (CHAR8 *) malloc (strlen (FileName) +
                             strlen (Dir) +
                             strlen (Extend) +
                             2);
  
  memset (RetStr, '\0', (strlen (FileName) + strlen (Dir) + strlen (Extend) + 2));
  
  sprintf (RetStr, "%s/%s.%s", Dir, FileName, Extend);
  
  return RetStr;
}

/**
  Delete a directory and files in it.
  
  @param[in]   DirName   Name of the directory need to be deleted.
  
  @return EFI_INVALID_PARAMETER
  @return EFI_SUCCESS
**/
EFI_STATUS
LibRmDir (
  IN  CHAR8*  DirName 
)
{
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand;
  
  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;

  
  if (DirName == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  
  //
  // Create a copy the new file. 
  //  
  SystemCommandFormatString = "rm -rf \"%s\" ";
  SystemCommand = malloc (
    strlen (SystemCommandFormatString) +
    strlen (DirName)     +
    1
    );

  sprintf (
    SystemCommand,
    SystemCommandFormatString,
    DirName
    ); 
    
  if (system (SystemCommand) != EFI_SUCCESS) {
    free(SystemCommand);
    return EFI_ABORTED;
  }  
  free(SystemCommand); 
  
  return EFI_SUCCESS;   
}

/*
  Construct a set of blank chars based on the number.
  
  @param[in]   Count The number of blank chars.
  
  @return      A string contained the blank chars.

*/
CHAR8 *
LibConstructBlankChar (
  IN UINT8    Count
)
{
  CHAR8    *RetStr;
  UINT8    Index;
  
  Index  = 0;
  RetStr = NULL;
  
  RetStr = (CHAR8 *) malloc (Count +1);
  
  memset (RetStr , '\0', Count + 1);

  for (Index=0; Index <= Count -1; Index ++) {
    RetStr[Index] = ' ';
  }
  
  return RetStr;
  
}

/**
  Delete a file.
  
  @param[in]   FileName   Name of the file need to be deleted.
  
  @return EFI_INVALID_PARAMETER
  @return EFI_SUCCESS
**/
EFI_STATUS
LibFmmtDeleteFile(
  IN   CHAR8    *FileName
)
{
  CHAR8*                 SystemCommandFormatString;
  CHAR8*                 SystemCommand;
  
  SystemCommandFormatString = NULL;
  SystemCommand             = NULL;

  
  if (FileName == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  
  //
  // Create a copy the new file. 
  //  
  SystemCommandFormatString = "del \"%s\" ";
  SystemCommand = malloc (
    strlen (SystemCommandFormatString) +
    strlen (FileName)     +
    1
    );

  sprintf (
    SystemCommand,
    SystemCommandFormatString,
    FileName
    ); 

  if (system (SystemCommand) != EFI_SUCCESS) {
    free(SystemCommand);
    return EFI_ABORTED;
  } 
  free(SystemCommand); 
  
  return EFI_SUCCESS;   

}


/**

  Convert a GUID to a string.


  @param[in]   Guid       - Pointer to GUID to print.


  @return The string after convert.  

**/
CHAR8 *
LibFmmtGuidToStr (
  IN  EFI_GUID  *Guid
)
{
  CHAR8 * Buffer;
  
  Buffer = NULL;
  
  if (Guid == NULL) {
    printf ("The guid is NULL while convert guid to string! \n");
    return NULL;
  }
  
  Buffer = (CHAR8 *) malloc (36 + 1);
  memset (Buffer, '\0', 36 + 1);
   
  if (Buffer == NULL) {
    printf ("Error while allocate resource! \n");
    return NULL;
  }
  
  sprintf (
      Buffer,
      "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      Guid->Data1,
      Guid->Data2,
      Guid->Data3,
      Guid->Data4[0],
      Guid->Data4[1],
      Guid->Data4[2],
      Guid->Data4[3],
      Guid->Data4[4],
      Guid->Data4[5],
      Guid->Data4[6],
      Guid->Data4[7]
      );

  return Buffer;
}


/**
  
  Free the whole Fd data structure.

  @param[in]  Fd  The pointer point to the Fd data structure.
  
**/
VOID
LibFmmtFreeFd (
  FIRMWARE_DEVICE *Fd
  )
{
  FV_INFORMATION   *CurrentFv;
  FV_INFORMATION   *TempFv;
  ENCAP_INFO_DATA  *EncapData1;
  ENCAP_INFO_DATA  *EncapData2;
  
  CurrentFv        = NULL;
  TempFv           = NULL;
  EncapData1       = NULL;
  EncapData2       = NULL;
  
  if (Fd == NULL) {
    return;
  }
  
  CurrentFv = Fd->Fv;
  
  do {
    TempFv = CurrentFv;
    CurrentFv = CurrentFv->FvNext;
    
    free (TempFv->FvHeader);

    if (TempFv->FvExtHeader != NULL) {
      free (TempFv->FvExtHeader);
    }
    
    // 
    // Free encapsulate data;
    //
    EncapData1 = TempFv->EncapData;
    
    while (EncapData1 != NULL) {
    
      EncapData2 = EncapData1;
      EncapData1 = EncapData1->NextNode;
      
      if (EncapData2->Data != NULL) {
        free (EncapData2->Data);
      }
	  if (EncapData2->FvExtHeader != NULL) {
	  	free(EncapData2->FvExtHeader);
	  }
      free (EncapData2);
      EncapData2 = NULL; 
    }
    
    EncapData1 = NULL;
    
    free (TempFv);
    TempFv = NULL;
    
  } while (CurrentFv != NULL);
  
  CurrentFv = NULL;
  
  return;
}

/**
  Generate the compressed section with specific type.
  Type could be EFI_STANDARD_COMPRESSION or EFI_NOT_COMPRESSED
  
  @param[in]  InputFileName    File name of the raw data.
  @param[in]  OutPutFileName   File name of the sectioned data.
  @param[in]  CompressionType  The compression type.
  
  @return  EFI_INVALID_PARAMETER
  @return  EFI_ABORTED
  @return  EFI_OUT_OF_RESOURCES
  @return  EFI_SUCCESS

**/
EFI_STATUS
LibGenCompressedSection (
  CHAR8         *InputFileName,
  CHAR8         *OutPutFileName,
  UINT8         CompressionType
) 
{
  //FILE                        *UnCompressFile;
	//FILE                        *CompressedFile;
	//VOID                        *UnCompressedBuffer;
	//VOID                        *CompressedBuffer;
	//UINT32                      UnCompressedSize;
	//UINT32                      CompressedSize;
	//CHAR8                       *TempName;
	//CHAR8                       *TemDir;
	//EFI_STATUS                  Status;
	
	//UnCompressFile     = NULL;
	//CompressedFile     = NULL;
	//UnCompressedBuffer = NULL;
	//CompressedBuffer   = NULL;
	//TempName           = NULL;
	//TemDir             = NULL;
	//UnCompressedSize   = 0;
	//CompressedSize     = 0;
	
	if ( InputFileName == NULL ||
	     OutPutFileName == NULL) {
	  printf ("Error while generate compressed section!\n");
	  return EFI_INVALID_PARAMETER;     
	}
	
	if (CompressionType == EFI_STANDARD_COMPRESSION) {
    /*
	
	  UnCompressFile = fopen (InputFileName, "rb");
	  if (UnCompressFile == NULL) {
	    printf ("Error while open file %s \n", InputFileName);
	    return EFI_ABORTED;
	  }
	  
    TemDir = _getcwd (NULL, MAX_PATH);
    sprintf(TemDir, "%s\\%s", TemDir, TEMP_DIR_NAME);
    
	  TempName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "comp");
	  
	  CompressedFile = fopen (TempName, "wb+");
	  if (CompressedFile == NULL) {
	    printf ("Error while open file %s \n", TempName);
	    return EFI_ABORTED;
	  }	  
	  
	  //
	  // Get the original file size;
	  //
    fseek(UnCompressFile,0,SEEK_SET);
    fseek(UnCompressFile,0,SEEK_END);

    UnCompressedSize = ftell(UnCompressFile);
    
    fseek(UnCompressFile,0,SEEK_SET);
    
    UnCompressedBuffer = malloc (UnCompressedSize);
    
    if (UnCompressedBuffer == NULL) {
      printf("Error while allocate memory! \n");
      return EFI_OUT_OF_RESOURCES;
    }
    
    CompressedBuffer = malloc (UnCompressedSize);
    
    if (CompressedBuffer == NULL) {
      printf("Error while allocate memory! \n");
      return EFI_OUT_OF_RESOURCES;
    }    
    
    if (fread (UnCompressedBuffer, 1, (size_t) UnCompressedSize, UnCompressFile) == (size_t) UnCompressedSize) {
      CompressedSize = UnCompressedSize;
      
      Status = EfiCompress ( UnCompressedBuffer,
                             UnCompressedSize,
                             CompressedBuffer,
                             &CompressedSize);
                    
      if (EFI_ERROR(Status)) {
        printf("Error while do compress operation! \n");
        return EFI_ABORTED;        
      }
      
      if (CompressedSize > UnCompressedSize) {
        printf("Error while do compress operation! \n");
        return EFI_ABORTED;            
      }
    } else {
      printf("Error while reading file %s! \n", InputFileName);
      return EFI_ABORTED;      
    }
    
    //
    // Write the compressed data into output file
    //
    if (fwrite (CompressedBuffer, 1, (size_t) CompressedSize, CompressedFile) != (size_t) CompressedSize) {
      Error ("FMMT", 0, 0004, "error writing %s file", OutPutFileName);
      fclose(UnCompressFile);
      fclose (CompressedFile);
      return EFI_ABORTED;
    }
    
    fclose(UnCompressFile);
    fclose (CompressedFile);
    */
    
    //
    // Call GenSec tool to generate the compressed section.
    // 
    LibCreateFfsSection(InputFileName, NULL, EFI_SECTION_COMPRESSION, OutPutFileName, NULL, NULL, NULL, 0, 0, "PI_STD");

	} else if (CompressionType == EFI_NOT_COMPRESSED) {
	
	  LibCreateFfsSection(InputFileName, NULL, EFI_SECTION_COMPRESSION, OutPutFileName, NULL, NULL, NULL, 0, 0, "PI_NONE");
	
  } else {
    printf ("Error while generate compressed section, unknown compression type! \n");
    return EFI_INVALID_PARAMETER;
  }
	
	
	return EFI_SUCCESS;
}

EFI_STATUS
LibEncapNewFvFile(
  IN     FV_INFORMATION              *FvInFd,
  IN     CHAR8                       *TemDir,
  OUT    CHAR8                       **OutputFile
)
{
  EFI_STATUS                  Status;
  UINT32                      ParentType;
  UINT8                       ParentLevel; 
  UINT32                      Type;
  UINT8                       Level; 
  CHAR8                       *InfFileName;
  FILE                        *InfFile;
  ENCAP_INFO_DATA             *LocalEncapData;
  BOOLEAN                     FfsFoundFlag;
  UINT32                      Index;   
  UINT32                      IndexCounter;
  UINT32                      OuterIndex;
  CHAR8                       *ExtractionTool;
  BOOLEAN                     IsLastLevelFfs;
  BOOLEAN                     IsLeafFlagIgnore;	
  BOOLEAN                     FirstInFlag;
  BOOLEAN                     OutputFileNameListFlag;
  CHAR8                       *InputFileName;  
  CHAR8                       *OutputFileName;
  CHAR8                       * OutputFileNameList[MAX_NUMBER_OF_FILES_IN_FV];
  CHAR8                       *FvGuidName;
  UINT16                      GuidAttributes;
  UINT16                      GuidDataOffset;
  	  
  Index                       = 0;  
  IndexCounter                = 0;
  OuterIndex                  = 0;
  ParentType                  = 0; 
  ParentLevel                 = 0; 
  Type                        = 0;
  Level                       = 0;
  FfsFoundFlag                = FALSE;  
  ExtractionTool              = NULL;
  InputFileName               = NULL;
  OutputFileName              = NULL;
  IsLastLevelFfs              = TRUE;
  IsLeafFlagIgnore            = FALSE;
  FirstInFlag                 = TRUE;
  FvGuidName                  = NULL;
  OutputFileNameListFlag      = TRUE;

  for(IndexCounter=0;IndexCounter<MAX_NUMBER_OF_FILES_IN_FV;IndexCounter++){
			if (OutputFileNameList[IndexCounter] != NULL){
				OutputFileNameList[IndexCounter] = NULL;
			}
		}
  
  //
  // Encapsulate from the lowest FFS file level.
  //
  LocalEncapData = FvInFd->EncapData;
  Level = LocalEncapData->Level;
  Type  = LocalEncapData->Type; 
  
  //
  // Get FV Name GUID
  //

  while (LocalEncapData != NULL) {
    //
    // Has changed.
    //
    if (LocalEncapData->Level > Level) {
      if (LocalEncapData->Type == FMMT_ENCAP_TREE_FFS) {
        ParentLevel = Level;
        ParentType  = Type;
      }   
    
      Level       = LocalEncapData->Level;
      Type        = LocalEncapData->Type;       
    }
    
    if (LocalEncapData->NextNode != NULL) {
      LocalEncapData = LocalEncapData->NextNode;
    } else {
      break;
    }  
  }

  do {  
    switch (ParentType) {
      case FMMT_ENCAP_TREE_FV:
        OutputFileNameListFlag = TRUE;

		for(OuterIndex=0;OutputFileNameListFlag;OuterIndex++){
        //
        // Generate FV.inf attributes.
        //
        InfFileName = LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "inf");
		FirstInFlag = TRUE;
              
        InfFile = fopen (InfFileName, "wt+");

        if (InfFile == NULL) {
          Error ("FMMT", 0, 0004, "Could not open inf file %s to store FV information! \n", "");
         return EFI_ABORTED;
        }        
        
        LocalEncapData = FvInFd->EncapData;
        while (LocalEncapData->NextNode != NULL) {
          if (LocalEncapData->Level == ParentLevel) {
            break;
          }
          LocalEncapData = LocalEncapData->NextNode;
        }
         
        if (((EFI_FIRMWARE_VOLUME_HEADER *)(LocalEncapData->Data))->ExtHeaderOffset != 0) {
          //
          // FV GUID Name memory allocation
          //
          FvGuidName = (CHAR8 *) malloc (255);

          if (FvGuidName == NULL) {
            Error ("FMMT", 0, 0004, "Out of resource, memory allocation failed! \n", "");
            return EFI_ABORTED;
          }

          memset(FvGuidName, '\0', 255);

          sprintf(
            FvGuidName,
            "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
            LocalEncapData->FvExtHeader->FvName.Data1,
            LocalEncapData->FvExtHeader->FvName.Data2,
            LocalEncapData->FvExtHeader->FvName.Data3,
            LocalEncapData->FvExtHeader->FvName.Data4[0],
            LocalEncapData->FvExtHeader->FvName.Data4[1],
            LocalEncapData->FvExtHeader->FvName.Data4[2],
            LocalEncapData->FvExtHeader->FvName.Data4[3],
            LocalEncapData->FvExtHeader->FvName.Data4[4],
            LocalEncapData->FvExtHeader->FvName.Data4[5],
            LocalEncapData->FvExtHeader->FvName.Data4[6],
            LocalEncapData->FvExtHeader->FvName.Data4[7]
          );
 
        } else {
          FvGuidName = NULL;
        }


        if (ParentLevel == 1) {
          Status = LibFvHeaderOptionToStr(((EFI_FIRMWARE_VOLUME_HEADER *)LocalEncapData->Data)->BlockMap, InfFile, TRUE);
        } else {
          Status = LibFvHeaderOptionToStr(((EFI_FIRMWARE_VOLUME_HEADER *)LocalEncapData->Data)->BlockMap, InfFile, FALSE);
        }
        
        
        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "generate FV INF file [Options] section failed.");
          return Status;
        }                
  
        Status = LibFvHeaderAttributeToStr(((EFI_FIRMWARE_VOLUME_HEADER *)LocalEncapData->Data)->Attributes, InfFile);
        
        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV header attribute failed");
          return Status;
        }           
        
        //
        // Found FFSs from Fv structure.
        //
        FfsFoundFlag = FALSE;
        for (Index=0; Index <= FvInFd->FfsNumbers; Index++) {

          //
          // For the last level FFS, the level below FFSs we should not care the IsLeaf Flag. 
          //
          if (IsLastLevelFfs) {
            IsLeafFlagIgnore = TRUE;
            } else {
              IsLeafFlagIgnore = FvInFd->FfsAttuibutes[Index].IsLeaf;
            } 

          if (FvInFd->FfsAttuibutes[Index].Level >= ParentLevel + 1 && IsLeafFlagIgnore) {
            if (FirstInFlag) {
			  if (FvInFd->FfsAttuibutes[Index].Level < 0xFF) {
				  FfsFoundFlag = TRUE;
				  Status = LibAddFfsFileToFvInf (FvInFd->FfsAttuibutes[Index].FfsName, InfFile, TRUE);
				  FirstInFlag = FALSE;
			  }
              
              if (EFI_ERROR (Status)) {
                  Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV inf file [files] section failed!");
                  return Status;
              }   
              
            } else {
			    if (FvInFd->FfsAttuibutes[Index].Level < 0xFF) {
					FfsFoundFlag = TRUE;
					Status = LibAddFfsFileToFvInf (FvInFd->FfsAttuibutes[Index].FfsName, InfFile, FALSE);
				}
                
                if (EFI_ERROR (Status)) {
                  Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV inf file [files] section failed!");
                  return Status;
                }  
                                
              }
     
			
			//avoid a FV contain too many ffs files 
			if ((FvInFd->FfsAttuibutes[Index].Level != FvInFd->FfsAttuibutes[Index+1].Level) && (ParentLevel != 1) && FvInFd->FfsAttuibutes[Index].Level != 0xFF && FvInFd->FfsAttuibutes[Index+1].Level != 0xFF){
				FvInFd->FfsAttuibutes[Index].Level = 0;
				break;
			}else{
				if (FvInFd->FfsAttuibutes[Index].Level != 0xFF){
					FvInFd->FfsAttuibutes[Index].Level = 0;
				}
			}
            
            }
          }

        IsLastLevelFfs = FALSE;
        
		if (!FfsFoundFlag){
			OutputFileNameListFlag = FALSE;
			if (OuterIndex > 0){
				fclose (InfFile);
				break;
			}
		}
        if (!FfsFoundFlag && OuterIndex == 0) {
		  for(IndexCounter=0;OutputFileNameList[IndexCounter] != NULL;IndexCounter++){
			  if (FirstInFlag) {
                 Status = LibAddFfsFileToFvInf (OutputFileNameList[IndexCounter], InfFile, TRUE);  
				 FirstInFlag = FALSE;
                 if (EFI_ERROR (Status)) {
                     Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV inf file [files] section failed!");
                     return Status;
                 }
			  }else{
				  Status = LibAddFfsFileToFvInf (OutputFileNameList[IndexCounter], InfFile, FALSE);  
                  if (EFI_ERROR (Status)) {
                     Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV inf file [files] section failed!");
                     return Status;
                 }
			  }

		  }
		  for(IndexCounter=0;IndexCounter<MAX_NUMBER_OF_FILES_IN_FV;IndexCounter++){
			   if (OutputFileNameList[IndexCounter] != NULL){
				   free(OutputFileNameList[IndexCounter]);
				   OutputFileNameList[IndexCounter] = NULL;
			   }
		  }
        }
        
        if (OutputFileNameList[0] != NULL && FfsFoundFlag && OuterIndex == 0) {
           for(IndexCounter=0;OutputFileNameList[IndexCounter] != NULL;IndexCounter++){
			  if (FirstInFlag) {
                 Status = LibAddFfsFileToFvInf (OutputFileNameList[IndexCounter], InfFile, TRUE);   
				 FirstInFlag = FALSE;
                 if (EFI_ERROR (Status)) {
                    Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV inf file [files] section failed!");
                    return Status;
                 }
			  }
			  else{
				  Status = LibAddFfsFileToFvInf (OutputFileNameList[IndexCounter], InfFile, FALSE);  
                  if (EFI_ERROR (Status)) {
                     Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV inf file [files] section failed!");
                     return Status;
                 }
			  }
		   } 
		   for(IndexCounter=0;IndexCounter<MAX_NUMBER_OF_FILES_IN_FV;IndexCounter++){
			   if (OutputFileNameList[IndexCounter] != NULL){
				   free(OutputFileNameList[IndexCounter]);
				   OutputFileNameList[IndexCounter] = NULL;
			   }
			}
		}

        //
        // Create FV
        //
        fclose (InfFile);
          
        OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "FV");
        
        Status = LibEncapsulateFfsToFv (InfFileName, NULL, OutputFileName, FvGuidName);
        
        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV failed!");
          return Status;
        }

		OutputFileNameList[OuterIndex] = (char *)malloc(strlen(OutputFileName)+1);
		memcpy((char *)OutputFileNameList[OuterIndex], (char *)OutputFileName, strlen(OutputFileName)+1);

		}
       
        break;
      case FMMT_ENCAP_TREE_FFS:
        
        for(OuterIndex=0;OutputFileNameList[OuterIndex] != NULL;OuterIndex++){
          InputFileName  = OutputFileNameList[OuterIndex];
          OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "ffs");
                
          LocalEncapData = FvInFd->EncapData;
          while (LocalEncapData->NextNode != NULL) {
            if (LocalEncapData->Level == ParentLevel) {
				for(;LocalEncapData->RightNode != NULL;) {
					if(LocalEncapData->FvExtHeader != NULL) {
						break;
					}
					LocalEncapData = LocalEncapData->RightNode;
				}
                break;
            }
            LocalEncapData = LocalEncapData->NextNode;
          }       

          if (LocalEncapData->FvExtHeader == NULL) {
    			  Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FFS file failed!");
                  return EFI_ABORTED;
          }
          Status = LibEncapSectionFileToFFS(EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE, InputFileName, OutputFileName, LocalEncapData->FvExtHeader->FvName, FALSE, 0);
		  if (EFI_ERROR (Status)) {
			  Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FFS file failed!");
              return Status;
		  }
		  free(LocalEncapData->FvExtHeader);
		  LocalEncapData->FvExtHeader = NULL;
		  free(OutputFileNameList[OuterIndex]);
		  OutputFileNameList[OuterIndex] = (char *)malloc(strlen(OutputFileName)+1);
		  memcpy((char *)OutputFileNameList[OuterIndex], (char *)OutputFileName, strlen(OutputFileName)+1);
		}
        break;
      case FMMT_ENCAP_TREE_GUIDED_SECTION:
	    for(OuterIndex=0;OutputFileNameList[OuterIndex] != NULL;OuterIndex++){
        //
        // Create the guided section original data, do compress operation.
        //
        InputFileName  = OutputFileNameList[OuterIndex];
        OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "compressed");
        
        //
        // Use the guided section header guid to find out compress application name.
        //
        LocalEncapData = FvInFd->EncapData;
        while (LocalEncapData->NextNode != NULL) {
          if (LocalEncapData->Level == ParentLevel) {
            break;
          }
          LocalEncapData = LocalEncapData->NextNode;
        }  
                 
        ExtractionTool =
          LookupGuidedSectionToolPath (
            mParsedGuidedSectionTools,
            (EFI_GUID *)LocalEncapData->Data
            );    
        GuidDataOffset = *(UINT16 *) ((UINT8 *) LocalEncapData->Data + sizeof (EFI_GUID));
        GuidAttributes = *(UINT16 *) ((UINT8 *) LocalEncapData->Data + sizeof (EFI_GUID) + sizeof (UINT16));
                
        Status = LibCreateGuidedSectionOriginalData (InputFileName, ExtractionTool, OutputFileName);

        if (EFI_ERROR (Status) || GuidDataOffset < sizeof (EFI_GUID_DEFINED_SECTION)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Compress guided data failed!");
          return Status;
        }
                  
        GuidDataOffset = GuidDataOffset - sizeof (EFI_GUID_DEFINED_SECTION);
        InputFileName  = OutputFileName;
        OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "guided");
            
        Status = LibCreateFfsSection(InputFileName, NULL, EFI_SECTION_GUID_DEFINED, OutputFileName, NULL, NULL, LibFmmtGuidToStr((EFI_GUID *)LocalEncapData->Data), GuidDataOffset, GuidAttributes, NULL);
        free(OutputFileNameList[OuterIndex]);
		OutputFileNameList[OuterIndex] = (char *)malloc(strlen(OutputFileName)+1);
		memcpy((char *)OutputFileNameList[OuterIndex], (char *)OutputFileName, strlen(OutputFileName)+1);

        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate guided section failed!");
          return Status;
        }        
		}     
        break;
      case FMMT_ENCAP_TREE_COMPRESS_SECTION:
        for(OuterIndex=0;OutputFileNameList[OuterIndex] != NULL;OuterIndex++){
          InputFileName  = OutputFileNameList[OuterIndex];
 
          OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "comsec");
          
          LocalEncapData = FvInFd->EncapData;
          while (LocalEncapData->NextNode != NULL) {
            if (LocalEncapData->Level == ParentLevel) {
              break;
            }
            LocalEncapData = LocalEncapData->NextNode;
          }           
          
          Status = LibGenCompressedSection (InputFileName, OutputFileName, *(UINT8 *)(LocalEncapData->Data));
          free(OutputFileNameList[OuterIndex]);
		  OutputFileNameList[OuterIndex] = (char *)malloc(strlen(OutputFileName)+1);
		  memcpy((char *)OutputFileNameList[OuterIndex], (char *)OutputFileName, strlen(OutputFileName)+1);

          if (EFI_ERROR (Status)) {
            Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate compressed section failed!");
            return Status;
          }             
        }
        break;
      case FMMT_ENCAP_TREE_FV_SECTION:
        for(OuterIndex=0;OutputFileNameList[OuterIndex] != NULL;OuterIndex++){
        InputFileName  = OutputFileNameList[OuterIndex];
        OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "sec");
        
        Status = LibCreateFfsSection(InputFileName, NULL, EFI_SECTION_FIRMWARE_VOLUME_IMAGE, OutputFileName, NULL, NULL, NULL, 0, 0, NULL);
        
        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV section failed!");
          return Status;
        }           

        InputFileName  = OutputFileName;
        OutputFileName= LibFilenameStrExtended (CloneString (tmpname (NULL)), TemDir, "sec");
        
        //
        // Make it alignment.
        //
        Status = LibCreateFfsSection(InputFileName, NULL, 0, OutputFileName, NULL, NULL, NULL, 0, 0, NULL);
        free(OutputFileNameList[OuterIndex]);
		OutputFileNameList[OuterIndex] = (char *)malloc(strlen(OutputFileName)+1);
		memcpy((char *)OutputFileNameList[OuterIndex], (char *)OutputFileName, strlen(OutputFileName)+1);

        if (EFI_ERROR (Status)) {
          Error ("FMMT", 0, 0004, "error while encapsulate FD Image", "Generate FV section failed!");
          return Status;
        }           
		}
        break;
      default:
        printf("Don't know how to encapsulate the FD file! \n");
        return EFI_ABORTED;               
    }
    
  
    //
    // Find next level and encapsulate type
    //
    ParentLevel -= 1;
    LocalEncapData = FvInFd->EncapData;
    while (LocalEncapData->NextNode != NULL) {
      if (LocalEncapData->Level == ParentLevel) {
        ParentType = LocalEncapData->Type;
        break;
      }
      LocalEncapData = LocalEncapData->NextNode;
    }
  } while (ParentLevel != 0);
  
  
  *OutputFile = OutputFileNameList[0];
  
  return EFI_SUCCESS;
  
}


EFI_STATUS
LibLocateFvViaFvId (
  IN     FIRMWARE_DEVICE     *FdData,
  IN     CHAR8               *FvId,
  IN OUT FV_INFORMATION      **FvInFd
)
{
  UINT8                       FvIndex1;
  UINT8                       FvIndex2;  
  BOOLEAN                     FvFoundFlag;
  
  FvIndex1                    = 0;
  FvIndex2                    = 0;
  FvFoundFlag                 = FALSE;
  
  if (FdData == NULL || FvId == NULL) {
    Error ("FMMT", 0, 0005, "error while find FV in FD", "Invalid parameters.");
    return EFI_INVALID_PARAMETER;  
  }
  
  *FvInFd = FdData->Fv;
  
  if (strlen(FvId) < 3) {
    Error ("FMMT", 0, 0005, "error while find FV in FD", "Invalid FvId, please double check the FvId. You can use view operate to get the FvId information!");
    return EFI_ABORTED;
  }

  FvIndex1 = (UINT8) atoi (FvId + 2);
  
  while (FvInFd != NULL) {
    if ((*FvInFd)->FvName != NULL) {
      FvIndex2 = (UINT8) atoi ((*FvInFd)->FvName + 2);

      if ((FvIndex2 <= FvIndex1) && (((*FvInFd)->FvLevel + FvIndex2) -1 >= FvIndex1)) {
        FvFoundFlag = TRUE;
        break;
      }
      if ((*FvInFd)->FvNext == 0) {
        break;
      }
      *FvInFd = (*FvInFd)->FvNext;
    }
  }
  
  //
  // The specified FV id has issue, can not find the FV in FD.
  //
  if (!FvFoundFlag) {
    Error ("FMMT", 0, 0005, "error while find FV in FD", "Invalid FvId, please double check the FvId. You can use view operate to get the FvId information!");
    return EFI_ABORTED;
  }
  
  return EFI_SUCCESS;

}


EFI_HANDLE
LibPreDefinedGuidedTools (
  VOID
)
{
  EFI_GUID            Guid;
  STRING_LIST         *Tool;
  GUID_SEC_TOOL_ENTRY *FirstGuidTool;
  GUID_SEC_TOOL_ENTRY *LastGuidTool;
  GUID_SEC_TOOL_ENTRY *NewGuidTool; 
  UINT8               Index;
  EFI_STATUS          Status;
  
  CHAR8 PreDefinedGuidedTool[3][255] = {
    "a31280ad-481e-41b6-95e8-127f4c984779 TIANO TianoCompress",
    "ee4e5898-3914-4259-9d6e-dc7bd79403cf LZMA LzmaCompress",
    "fc1bcdb0-7d31-49aa-936a-a4600d9dd083 CRC32 GenCrc32",
  };

  Tool            = NULL;
  FirstGuidTool   = NULL;
  LastGuidTool    = NULL;
  NewGuidTool     = NULL;
  Index           = 0;
  
  for (Index = 0; Index < 3; Index++) {
    Tool = SplitStringByWhitespace (PreDefinedGuidedTool[Index]);
    if ((Tool != NULL) &&
        (Tool->Count == 3)
       ) {
      Status = StringToGuid (Tool->Strings[0], &Guid);
      if (!EFI_ERROR (Status)) {
        NewGuidTool = malloc (sizeof (GUID_SEC_TOOL_ENTRY));
        if (NewGuidTool != NULL) {
          memcpy (&(NewGuidTool->Guid), &Guid, sizeof (Guid));
          NewGuidTool->Name = CloneString(Tool->Strings[1]);
          NewGuidTool->Path = CloneString(Tool->Strings[2]);
          NewGuidTool->Next = NULL;
        }
        if (FirstGuidTool == NULL) {
          FirstGuidTool = NewGuidTool;
        } else {
          LastGuidTool->Next = NewGuidTool;
        }
        LastGuidTool = NewGuidTool;
      }
      FreeStringList (Tool);
    } else {
      fprintf (stdout, "Error");
    }
  }
  return FirstGuidTool;
}
