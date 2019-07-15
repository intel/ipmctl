/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Implementations of ACPI helper functions for Linux
 */

#include "lnx_acpi.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <os_str.h>

#define	SYSFS_ACPI_PATH	"/sys/firmware/acpi/tables/"
int g_count = 0;

/*!
* 8 bit unsigned integer as a boolean
*/
typedef unsigned char BOOL;

/*!
* 32-bit Unsigned Integer.
*/
typedef unsigned int UINT32;

/*!
* 8-bit Unsigned Integer.
*/
typedef unsigned char UINT8;

static inline int verify_checksum(
	const UINT8 *p_raw_data,
	const UINT32 length)
{
	BOOL valid = 1;

	UINT8 sum = 0;
	for (UINT32 i = 0; i < length; i++)
	{
		sum += p_raw_data[i];
	}
	if (sum != 0)
	{
		valid = 0;
	}

	return valid;
}

/*
* Verify the ACPI table size, checksum and signature
*/
int check_acpi_table(const char *signature,
	struct acpi_table *p_table)
{
	int rc = ACPI_SUCCESS;

	if (!p_table)
	{
		rc = ACPI_ERR_BADINPUT;
	}
	else if (!verify_checksum((UINT8 *)p_table, p_table->header.length))
	{
		rc = ACPI_ERR_CHECKSUMFAIL;
	}
	// check overall table length is at least as big as the header
	else if (p_table->header.length < sizeof(struct acpi_table))
	{
		rc = ACPI_ERR_BADTABLE;
	}
	// check signature
	else if (strncmp(signature, p_table->header.signature, ACPI_SIGNATURE_LEN) != 0)
	{
		rc = ACPI_ERR_BADTABLESIGNATURE;
	}

	return rc;
}


/*!
 * Return the specified ACPI table or the size
 * required
 */
int get_acpi_table(
		const char *signature,
		struct acpi_table *p_table,
		const unsigned int size)
{
	int rc = 0;

	char table_path[PATH_MAX];
	snprintf(table_path, sizeof(table_path), "%s%s", SYSFS_ACPI_PATH, signature);

	int fd = open(table_path, O_RDONLY|O_CLOEXEC);
	if (fd < 0)
	{
		rc = ACPI_ERR_TABLENOTFOUND;
	}
	else
	{
		struct acpi_table_header header;
		size_t header_size = sizeof (header);

		ssize_t hdr_bytes_read = read(fd, &header, header_size);
		if (hdr_bytes_read != header_size)
		{
			rc = ACPI_ERR_BADTABLE;
		}
		else
		{
			size_t total_table_size = header.length;
			rc = (int)total_table_size;
			if (p_table)
			{
				memset(p_table, 0, size);
				os_memcpy(&(p_table->header), sizeof(struct acpi_table_header), &header, header_size);
				if (size < total_table_size)
				{
					rc = ACPI_ERR_BADTABLE;
				}
				else
				{
					size_t requested_bytes = total_table_size - header_size;
					unsigned char *p_buff = p_table->p_ext_tables;
					ssize_t total_read = 0;

					while (total_read < requested_bytes)
					{
						ssize_t bytes_read = read(fd, p_buff, requested_bytes);
						p_buff += bytes_read;
						total_read += bytes_read;
					}

					if (total_read != requested_bytes)
					{
						rc = ACPI_ERR_BADTABLE;
					}
					else
					{
						rc = check_acpi_table(signature, p_table);
					}
				}
			}
		}
		close(fd);
	}

	return rc;
}
