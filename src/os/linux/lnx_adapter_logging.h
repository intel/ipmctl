/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CR_MGMT_LOGGING_H
#define CR_MGMT_LOGGING_H

#include <stdio.h>
#include <stdarg.h>
#include <nvm_types.h>
#include <os_types.h>

///
 /// 8-byte unsigned value
 ///
typedef unsigned long long  UINT64;
///
/// 8-byte signed value
///
typedef long long           INT64;
///
/// 4-byte unsigned value
///
typedef unsigned int        UINT32;
///
/// 4-byte signed value
///
typedef int                 INT32;
///
/// 2-byte unsigned value
///
typedef unsigned short      UINT16;
///
/// 2-byte Character.  Unless otherwise specified all strings are stored in the
/// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
///
typedef unsigned short      CHAR16;
///
/// 2-byte signed value
///
typedef short               INT16;
///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef unsigned char       BOOLEAN;
///
/// 1-byte unsigned value
///
typedef unsigned char       UINT8;
///
/// 1-byte Character
///
typedef char                CHAR8;
///
/// 1-byte signed value
///
typedef signed char         INT8;


#endif //CR_MGMT_LOGGING_H
