/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _SHOW_ACPI_COMMAND_H_
#define _SHOW_ACPI_COMMAND_H_

#include "CommandParser.h"

typedef enum {
  AcpiUnknown = 0,
  AcpiNfit    = BIT0,
  AcpiPcat    = BIT1,
  AcpiPMTT    = BIT2,
  AcpiAll     = AcpiUnknown | AcpiNfit | AcpiPcat | AcpiPMTT
} AcpiType;

/**
  Register the show ACPI tables command
**/
EFI_STATUS
RegisterShowAcpiCommand(
  );

/**
  Execute the show ACPI command
**/
EFI_STATUS
showAcpi(
  IN     struct Command *pCmd
  );

#endif /** _SHOW_ACPI_COMMAND_H_ **/
