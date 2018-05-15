/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Common ACPI parsing functions
 */

#ifndef SRC_COMMON_ACPI_ACPI_H_
#define	SRC_COMMON_ACPI_ACPI_H_

#include "lnx_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define	ACPI_SIGNATURE_LEN	4
#define	ACPI_OEM_ID_LEN	6
#define	ACPI_OEM_TABLE_ID_LEN	8
#define	ACPI_CHECKSUM_OFFSET	9
#define ACPI_FW_TABLE_SIGNATURE	"ACPI"

/*
 * ACPI errors
 */
enum acpi_error
{
	ACPI_SUCCESS = 0,
	ACPI_ERR_BADINPUT = -1,
	ACPI_ERR_CHECKSUMFAIL = -2,
	ACPI_ERR_BADTABLE = -3,
	ACPI_ERR_BADTABLESIGNATURE = -4,
	ACPI_ERR_TABLENOTFOUND = -5
};

/*
 * ACPI Table Header
 */
PACK_STRUCT(
struct acpi_table_header
{
	char signature[ACPI_SIGNATURE_LEN];
	unsigned int length; /* Length in bytes for entire table */
	unsigned char revision;
	unsigned char checksum; /* Must sum to zero */
	char oem_id[ACPI_OEM_ID_LEN];
	char oem_table_id[ACPI_OEM_TABLE_ID_LEN];
	unsigned int oem_revision;
	unsigned int creator_id;
	unsigned int creator_revision;
	unsigned char reserved[4];
})

/*
 * ACPI Table
 */
#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
PACK_STRUCT(
struct acpi_table
{
	struct acpi_table_header header;
	/* Variable extension Tables */
	unsigned char p_ext_tables[0];
}
)
#if _MSC_VER
#pragma warning(pop)
#endif

/*!
 * Retrieve the specified ACPI table.
 * If p_table is NULL, return the size of the table.
 */
int get_acpi_table(
		const char *signature,
		struct acpi_table *p_table,
		const unsigned int size);

/*!
 * Verify the ACPI table size, checksum and signature
 */
int check_acpi_table(const char *signature,
		struct acpi_table *p_table);

/*!
 * Helper function to print an ACPI table header
 */
void print_acpi_table_header(
		struct acpi_table_header *p_table);

#ifdef __cplusplus
}
#endif

#endif /* SRC_COMMON_ACPI_ACPI_H_ */
