/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
 // The following are message definitions.
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//				7 - EFI DEVICE ERROR
//
//
// Define the facility codes
//
#define FACILITY_SERVICE                 0x0

//
// MessageId: NVMDIMM_INFORMATIONAL
//
// MessageText:
//
// %1
//
#define NVMDIMM_INFORMATIONAL            ((DWORD)0x60000007L)

//
// MessageId: NVMDIMM_WARNING
//
// MessageText:
//
// %1
//
#define NVMDIMM_WARNING                  ((DWORD)0xa0000007L)

//
// MessageId: NVMDIMM_ERROR
//
// MessageText:
//
// %1
//
#define NVMDIMM_ERROR                    ((DWORD)0xe0000007L)

 // A message file must end with a period on its own line
 // followed by a blank line.

 //
 // Define the severity codes
 //
#define ERROR_EVENT                      0x1
#define WARNING_EVENT                    0x2
#define INFORMATIONAL_EVENT              0x4

