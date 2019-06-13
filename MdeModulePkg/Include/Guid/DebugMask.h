/** @file

  Debug Mask Protocol.

Copyright (c) 2011, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under 
the terms and conditions of the BSD License that accompanies this distribution.  
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.                                            

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __DEBUG_MASK_H__
#define __DEBUG_MASK_H__

///
/// Protocol GUID for DXE Phase Debug Mask support
///
#define EFI_DEBUG_MASK_PROTOCOL_GUID \
  { 0x4c8a2451, 0xc207, 0x405b, {0x96, 0x94, 0x99, 0xea, 0x13, 0x25, 0x13, 0x41} }

///
/// Forward reference for pure ANSI compatibility
///
typedef struct _EFI_DEBUG_MASK_PROTOCOL  EFI_DEBUG_MASK_PROTOCOL;

///
///
///  
#define EFI_DEBUG_MASK_REVISION        0x00010000

//
// DebugMask member functions definition
//
typedef
EFI_STATUS
(EFIAPI * EFI_GET_DEBUG_MASK) (
  IN EFI_DEBUG_MASK_PROTOCOL  *This,             
  IN OUT UINTN                *CurrentDebugMask  
  );

typedef 
EFI_STATUS
(EFIAPI *EFI_SET_DEBUG_MASK) (
  IN EFI_DEBUG_MASK_PROTOCOL  *This,
  IN UINTN                    NewDebugMask
  );

///
/// DebugMask protocol definition
///
struct _EFI_DEBUG_MASK_PROTOCOL {
  INT64               Revision;
  EFI_GET_DEBUG_MASK  GetDebugMask;
  EFI_SET_DEBUG_MASK  SetDebugMask;
};

extern EFI_GUID gEfiDebugMaskProtocolGuid;

///
/// GUID used to store the global debug mask in an the "EFIDebug" EFI Variabe
/// Also used as a GUIDed HOB that contains a UINT32 debug mask default value
///
#define EFI_GENERIC_VARIABLE_GUID \
  { 0x59d1c24f, 0x50f1, 0x401a, {0xb1, 0x01, 0xf3, 0x3e, 0x0d, 0xae, 0xd4, 0x43} }
  
#define DEBUG_MASK_VARIABLE_NAME  L"EFIDebug"

extern EFI_GUID gEfiGenericVariableGuid;

#endif
