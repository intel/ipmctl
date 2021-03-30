/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This file contains definitions for structures and values necessary for perusing
 * the raw SMBIOS table data.
 *
 * These structures and values are defined in DMTF SMBIOS spec 3.0.0.
 */

#ifndef SRC_SMBIOS_SMBIOS_TYPES_H_
#define SRC_SMBIOS_SMBIOS_TYPES_H_

#include "lnx_common.h"

#define	SMBIOS_ANCHOR_STR_SIZE		4
#define	SMBIOS_3_ANCHOR_STR_SIZE	5

extern unsigned char SMBIOS_ANCHOR_STR[SMBIOS_ANCHOR_STR_SIZE];
extern unsigned char SMBIOS_3_ANCHOR_STR[SMBIOS_3_ANCHOR_STR_SIZE];

/*
 * Entry point structures - used to find the SMBIOS table in raw physical memory
 */
PACK_STRUCT(
struct smbios_entry_point
{
	UINT8 anchor_str[SMBIOS_ANCHOR_STR_SIZE];
	UINT8 checksum;
	UINT8 entry_point_length;
	UINT8 smbios_major_version;
	UINT8 smbios_minor_version;
	UINT16 max_structure_size;
	UINT8 entry_point_revision;
	UINT8 formatted_area[5];
	UINT8 intermediate_anchor_str[5];
	UINT8 intermediate_checksum;
	UINT16 structure_table_length;
	UINT32 structure_table_address;
	UINT16 num_smbios_structures;
	UINT8 bcd_revision;
} )

PACK_STRUCT(
struct smbios_3_entry_point
{
	UINT8 anchor_str[SMBIOS_3_ANCHOR_STR_SIZE];
	UINT8 checksum;
	UINT8 entry_point_length;
	UINT8 smbios_major_version;
	UINT8 smbios_minor_version;
	UINT8 smbios_doc_rev;
	UINT8 entry_point_revision;
	UINT8 reserved;
	UINT32 structure_table_max_length;
	UINT64 structure_table_address;
} )

/*
 * SMBIOS structure table values and structures
 */

enum smbios_structure_type
{
	SMBIOS_STRUCT_TYPE_BIOS_INFO = 0,
	SMBIOS_STRUCT_TYPE_SYSTEM_INFO = 1,
	SMBIOS_STRUCT_TYPE_SYSTEM_ENCLOSURE = 3,
	SMBIOS_STRUCT_TYPE_PROCESSOR_INFO = 4,
	SMBIOS_STRUCT_TYPE_CACHE_INFO = 7,
	SMBIOS_STRUCT_TYPE_SYSTEM_SLOTS = 9,
	SMBIOS_STRUCT_TYPE_PHYSICAL_MEMORY_ARRAY = 16,
	SMBIOS_STRUCT_TYPE_MEMORY_DEVICE = 17,
	SMBIOS_STRUCT_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS = 19,
	SMBIOS_STRUCT_TYPE_SYSTEM_BOOT_INFO = 32
};

PACK_STRUCT(
struct smbios_structure_header
{
	UINT8 type;
	UINT8 length;
	UINT16 handle;
} )

// Special values for the SMBIOS Type 17 Memory Device fields
#define SMBIOS_SIZE_KB_GRANULARITY_MASK	0x8000
#define SMBIOS_SIZE_MASK				0x7FFF
#define SMBIOS_EXTENDED_SIZE_MASK		0x7FFFFFFF
#define	SMBIOS_ATTRIBUTES_RANK_MASK		0x0F

#define SMBIOS_MEM_ERROR_INFO_NOT_PROVIDED	0xFFFE
#define SMBIOS_MEM_ERROR_INFO_NONE			0xFFFF

#define SMBIOS_WIDTH_UNKNOWN	0xFFFF

#define	SMBIOS_SIZE_EMPTY		0x0
#define SMBIOS_SIZE_UNKNOWN		0xFFFF
#define SMBIOS_SIZE_EXTENDED	0x7FFF

#define SMBIOS_DEVICE_SET_NONE		0x0
#define SMBIOS_DEVICE_SET_UNKNOWN	0xFF

#define	SMBIOS_SPEED_UNKNOWN	0x0

#define SMBIOS_RANK_UNKNOWN		0x0

#define	SMBIOS_VOLTAGE_UNKNOWN	0x0

// SMBIOS Type 17 - Memory Device
PACK_STRUCT(
struct smbios_memory_device
{
	struct smbios_structure_header header;
	UINT16 physical_mem_array_handle;
	UINT16 mem_error_info_handle;
	UINT16 total_width;
	UINT16 data_width;
	UINT16 size;
	UINT8 form_factor;
	UINT8 device_set;
	UINT8 device_locator_str_num;
	UINT8 bank_locator_str_num;
	UINT8 memory_type;
	UINT16 type_detail;
	UINT16 speed;
	UINT8 manufacturer_str_num;
	UINT8 serial_number_str_num;
	UINT8 asset_tag_str_num;
	UINT8 part_number_str_num;
	UINT8 attributes;
	UINT32 extended_size;
	UINT16 configured_mem_clock_speed;
	UINT16 min_voltage;
	UINT16 max_voltage;
	UINT16 configured_voltage;

} )

#endif /* SRC_SMBIOS_SMBIOS_TYPES_H_ */
