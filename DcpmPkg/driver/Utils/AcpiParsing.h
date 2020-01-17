/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ACPI_PARSING_H_
#define _ACPI_PARSING_H_

#include <Debug.h>
#include <Utility.h>
#include <Types.h>
#include <NvmTables.h>

extern GUID gSpaRangeVolatileRegionGuid;
extern GUID gSpaRangePmRegionGuid;
extern GUID gSpaRangeControlRegionGuid;
extern GUID gSpaRangeBlockDataWindowRegionGuid;
extern GUID gSpaRangeRawVolatileRegionGuid;
extern GUID gSpaRangeIsoVolatileRegionGuid;
extern GUID gSpaRangeRawPmRegionGuid;
extern GUID gSpaRangeIsoPmRegionGuid;
extern GUID gAppDirectPmTypeGuid;
extern GUID gSpaRangeMailboxCustomGuid;

#define SPA_RANGE_VOLATILE_REGION_GUID \
  { 0x7305944F, 0xFDDA, 0x44E3, {0xB1, 0x6C, 0x3F, 0x22, 0xD2, 0x52, 0xE5, 0xD0} }

#define SPA_RANGE_PM_REGION_GUID \
  { 0x66F0D379, 0xB4F3, 0x4074, {0xAC, 0x43, 0x0D, 0x33, 0x18, 0xB7, 0x8C, 0xDB} }

#define SPA_RANGE_CONTROL_REGION_GUID \
  { 0x92F701F6, 0x13B4, 0x405D, {0x91, 0x0B, 0x29, 0x93, 0x67, 0xE8, 0x23, 0x4C} }

#define SPA_RANGE_BLOCK_DATA_WINDOW_REGION_GUID \
  { 0x91AF0530, 0x5D86, 0x470E, {0xA6, 0xB0, 0x0A, 0x2D, 0xB9, 0x40, 0x82, 0x49} }

#define SPA_RANGE_RAW_VOLATILE \
  { 0x77AB535A, 0x45FC, 0x624B, {0x55, 0x60, 0xF7, 0xB2, 0x81, 0xD1, 0xF9, 0x6E} }

#define SPA_RANGE_ISO_VOLATILE \
  { 0x3D5ABD30, 0x4175, 0x87CE, {0x6D, 0x64, 0xD2, 0xAD, 0xE5, 0x23, 0xC4, 0xBB} }

#define SPA_RANGE_RAW_PM \
  { 0x5CEA02C9, 0x4D07, 0x69D3, {0x26, 0x9F, 0x44, 0x96, 0xFB, 0xE0, 0x96, 0xF9} }

#define SPA_RANGE_ISO_PM \
  { 0x08018188, 0x42CD, 0xBB48, {0x10, 0x0F, 0x53, 0x87, 0xD5, 0x3D, 0xED, 0x3D} }

#define APPDIRECT_PM_TYPE \
  { 0x66F0D379, 0xB4F3, 0x4074, {0xAC, 0x43, 0x0D, 0x33, 0x18, 0xB7, 0x8C, 0xDB} }

/*
 * SPA_RANGE_CONTROL_REGION_GUID above should only be used for block windows.
 * This is a custom GUID so that we can find the mailbox spa range structs.
 */
#define SPA_RANGE_MAILBOX_CUSTOM_GUID \
  { 0x48D7624D, 0x5CD8, 0x4924, {0xAF, 0xD5, 0xDB, 0xCB, 0xF8, 0x50, 0xCC, 0x2B} }

#define NFIT_ACPI_NAMESPACE_ID SIGNATURE_64('A', 'C', 'P', 'I', '0', '0', '1', '0')
#define NFIT_TABLE_SIG         SIGNATURE_32('N', 'F', 'I', 'T') //!< NFIT Table signature
#define PCAT_TABLE_SIG         SIGNATURE_32('P', 'C', 'A', 'T') //!< PCAT Table signature
#define PMTT_TABLE_SIG         SIGNATURE_32('P', 'M', 'T', 'T') //!< PMTT Table signature

/**
  Offset of the MinFixed field in the Nfit Acpi Namespace.
  This field contains the pointer to the NFIT table.
**/
#define NFIT_POINTER_OFFSET 0x2A

typedef enum {
  MEMORY_MODE_1LM = 0,
  MEMORY_MODE_2LM = 1
} MEMORY_MODE;

/**
  ACPI Related Functions
**/

/**
  ParseNfitTable - Performs deserialization from binary memory block into parsed structure of pointers.

  @param[in] pTable pointer to the memory containing the NFIT binary representation.

  @retval NULL if there was an error while parsing the memory.
  @retval pointer to the allocated header with parsed NFIT.
**/
ParsedFitHeader *
ParseNfitTable(
  IN     VOID *pTable
  );

/**
  Performs deserialization from binary memory block, containing PCAT tables, into parsed structure of pointers.

  @param[in] pTable pointer to the memory containing the PCAT binary representation.

  @retval NULL if there was an error while parsing the memory.
  @retval pointer to the allocated header with parsed PCAT.
**/
ParsedPcatHeader *
ParsePcatTable (
  IN     VOID *pTable
  );

/**
  Performs deserialization from binary memory block, containing PMTT tables, into parsed structure of pointers.

  @param[in] pTable pointer to the memory containing the PMTT binary representation.

  @retval NULL if there was an error while parsing the memory.
  @retval pointer to the allocated header with parsed PMTT.
**/
ParsedPmttHeader *
ParsePmttTable(
  IN     VOID *pTable
  );

/**
  Get PMTT Dimm Module by Dimm ID
  Scan the dimm list for a dimm identified by Dimm ID

  @param[in] DimmID: The SMBIOS Type 17 handle of the dimm
  @param[in] pPmttHead: Parsed PMTT Table

  @retval PMTT_MODULE_INFO struct pointer if matching dimm has been found
  @retval NULL pointer if not found
**/
PMTT_MODULE_INFO *
GetDimmModuleByPidFromPmtt(
  IN     UINT32 DimmID,
  IN     ParsedPmttHeader *pPmttHead
  );

/**
  Retrieve the Logical Socket ID from PMTT Table

  @param[in] SocketId SocketID
  @param[in] DieId DieID
  @param[out] pLogicalSocketId Logical socket ID based on Dimm socket ID & Die ID

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_NOT_FOUND PCAT socket sku info table not found for given socketID
**/
EFI_STATUS
GetLogicalSocketIdFromPmtt(
  IN     UINT32 SocketId,
  IN     UINT32 DieId,
  OUT    UINT32 *pLogicalSocketId
  );

/**
  Conversion Functions
**/

/**
  Returns the FlushHint table associated with the provided NVDIMM region table.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] pNvDimmRegionMappingStructure the NVDIMM region table that contains the index.
  @param[out] ppFlushHintTable pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more parameters equal NULL.
  @retval EFI_NOT_FOUND if there is no Interleave table with the provided index.
**/
EFI_STATUS
GetFlushHintTableForNvDimmRegionTable(
  IN     ParsedFitHeader *pFitHead,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure,
     OUT FlushHintTbl **ppFlushHintTable
  );

/**
  GetBlockDataWindowRegDescTabl - returns the Block Data Window Table associated with the provided Control Region Table.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] pControlRegionTable the Control Region table that contains the index.
  @param[out] ppBlockDataWindowTable pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if pFitHead or ControlRegionTbl or BWRegionTbl equals NULL.
  @retval EFI_NOT_FOUND if there is no Block Data Window Descriptor table with the provided index.
**/
EFI_STATUS
GetBlockDataWindowRegDescTabl(
  IN     ParsedFitHeader *pFitHead,
  IN     ControlRegionTbl *pControlRegTbl,
     OUT BWRegionTbl **ppBlockDataWindowTable
  );

/**
  Returns the ControlRegion table associated with the provided NVDIMM region table.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] pNvDimmRegionMappingStructure the NVDIMM region table that contains the index.
  @param[out] ppControlRegionTable pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if one or more input parameters equal NULL.
  @retval EFI_NOT_FOUND if there is no Control Region table with the provided index.
**/
EFI_STATUS
GetControlRegionTableForNvDimmRegionTable(
  IN     ParsedFitHeader *pFitHead,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure,
     OUT ControlRegionTbl **ppControlRegionTable
  );

/**
  Get Control Region table for provided PhysicalID

  @param[in] pFitHead pointer to the parsed NFit Header structure
  @param[in] Pid Dimm PhysicalID
  @param[out] pControlRegionTables array to store Control Region tables pointers
  @param[in, out] pControlRegionTablesNum size of array on input, number of items stored in the array on output

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER One or more parameters are NULL
  @retval EFI_BUFFER_TOO_SMALL There is more Control Region tables in NFIT than size of provided array
**/
EFI_STATUS
GetControlRegionTablesForPID(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 Pid,
     OUT ControlRegionTbl *pControlRegionTables[],
  IN OUT UINT32 *pControlRegionTablesNum
  );

/**
  GetSpaRangeTable - returns the SpaRange Table with the provided Index.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] SpaRangeTblIndex index of the table to be found.
  @param[out] ppSpaRangeTbl pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if pFitHead or ppInterleaveTbl equals NULL.
  @retval EFI_NOT_FOUND if there is no Interleave table with the provided index.
**/
EFI_STATUS
GetSpaRangeTable(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 SpaRangeTblIndex,
     OUT SpaRangeTbl **ppSpaRangeTbl
  );

/**
  Finds in the provided Nfit structure the requested NVDIMM region.

  If the pAddrRangeTypeGuid equals NULL, the first table matching the Pid will be returned.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] Pid the Dimm ID that the NVDIMM region must be for.
  @param[in] pAddrRangeTypeGuid pointer to GUID type of the range that we are looking for. OPTIONAL
  @param[in] SpaRangeIndexProvided Determine if SpaRangeIndex is provided
  @param[in] SpaRangeIndex Looking for NVDIMM region table that is related with provided SPA table. OPTIONAL
  @param[out] ppNvDimmRegionMappingStructure pointer to a pointer for the return NVDIMM region.

  @retval EFI_SUCCESS if the table was found and was returned.
  @retval EFI_INVALID_PARAMETER if one or more input parameters equal NULL.
  @retval EFI_NOT_FOUND if there is no NVDIMM region for the provided Dimm PID and AddrRangeType.
**/
EFI_STATUS
GetNvDimmRegionMappingStructureForPid(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 Pid,
  IN     GUID *pAddrRangeTypeGuid OPTIONAL,
  IN     BOOLEAN SpaRangeIndexProvided,
  IN     UINT16 SpaRangeIndex OPTIONAL,
     OUT NvDimmRegionMappingStructure **ppNvDimmRegionMappingStructure
  );

/**
  GetInterleaveTable - returns the Interleave Table with the provided Index.

  @param[in] pFitHead pointer to the parsed NFit Header structure.
  @param[in] InterleaveTblIndex index of the table to be found.
  @param[out] ppInterleaveTbl pointer to a pointer where the table will be stored.

  @retval EFI_SUCCESS if the table was found and is properly returned.
  @retval EFI_INVALID_PARAMETER if pFitHead or ppInterleaveTbl equals NULL.
  @retval EFI_NOT_FOUND if there is no Interleave table with the provided index.
**/
EFI_STATUS
GetInterleaveTable(
  IN     ParsedFitHeader *pFitHead,
  IN     UINT16 InterleaveTblIndex,
     OUT InterleaveStruct **ppInterleaveTbl
  );

/**
  RdpaToSpa() - Convert Device Region Physical to System Physical Address

  @param[in] Rdpa Device Region Physical Address to convert
  @param[in] pNvDimmRegionTable The NVDIMM region that helps describe this region of memory
  @param[in] pInterleaveTable Interleave table referenced by the mdsparng_tbl
  @param[out] SpaAddr output for SPA address

  A memory device could have multiple regions. As such we cannot convert
  to a device physical address. Instead we refer to the address for a region
  within the device as device region physical address (RDPA), where Rdpa is
  a zero based address from the start of the region within the device.

  @retval EFI_SUCCESS on success
  @retval EFI_INVALID_PARAMETER on a divide by zero error
**/
EFI_STATUS
RdpaToSpa(
  IN     UINT64 Rdpa,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionTable,
  IN     SpaRangeTbl *pSpaRangeTable,
  IN     InterleaveStruct *pInterleaveTable OPTIONAL,
     OUT UINT64 *pSpaAddr
  );

/**
  Return the current memory mode chosen by the BIOS during boot-up. 1LM is
  the fallback option and will always be available. 2LM will only be enabled
  if the AllowedMemoryMode is 2LM, there is memory configured for 2LM, and
  it's in a BIOS-supported configuration. We read this information from the
  PCAT table provided by BIOS.

  @param[out] pResult The current memory mode chosen by BIOS

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_ABORTED PCAT tables not found
**/
EFI_STATUS
CurrentMemoryMode(
     OUT MEMORY_MODE *pResult
  );


/**
  Return the allowed memory mode selected in the BIOS setup menu under
  Socket Configuration -> Memory Configuration -> Memory Map -> Volatile Memory Mode.
  Even if 2LM is allowed, it implies that 1LM is allowed as well (even
  though the memory mode doesn't indicate this).
  We read this information from the PCAT table provided by BIOS.

  @param[out] pResult The allowed memory mode setting in BIOS

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_ABORTED PCAT tables not found
**/
EFI_STATUS
AllowedMemoryMode(
     OUT MEMORY_MODE *pResult
  );

/**
  Check if BIOS supports changing configuration through management software

  @param[out] pConfigChangeSupported The Config Change support in BIOS

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_LOAD_ERROR PCAT tables not found
  @retval EFI_UNSUPPORTED Config change request through management software not supported
**/
EFI_STATUS
CheckIfBiosSupportsConfigChange(
  OUT BOOLEAN *pConfigChangeSupported
  );

/**
  Check Memory Mode Capabilties from PCAT table type 0

  @param[in] pMemMode2LMSupported 2LM Mode Support
  @param[in] pAppDirectPMSupported AppDirect PM Mode Support
  @param[in] pMemMode1LMSupported 2LM Memory Mode Support

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_LOAD_ERROR PCAT tables not found
  @retval EFI_UNSUPPORTED Config change request through management software not supported
**/
EFI_STATUS
CheckMemModeCapabilities(
  OUT BOOLEAN *pMemMode2LMSupported,
  OUT BOOLEAN *pAppDirectPMSupported,
  OUT BOOLEAN *pMemMode1LMSupported
  );

/**
  Retrieve the PCAT Socket SKU info table for a given Socket

  @param[in] SocketId SocketID to retrieve the table for
  @param[out] ppSocketSkuInfoTable Sku info table referenced by socket ID
  @param[out] PCAT Table revision

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
  @retval EFI_NOT_FOUND PCAT socket sku info table not found for given socketID
**/
EFI_STATUS
RetrievePcatSocketSkuInfoTable(
  IN     UINT32 SocketId,
  OUT    VOID **ppSocketSkuInfoTable,
  OUT    ACPI_REVISION *pPcatRevison
  );

/**
  Retrieve the list of supported Channel & iMC Interleave sizes

  @param[out] ppChannelInterleaveSize Array of supported Channel Interleave sizes
  @param[out] ppiMCInterleaveSize Array of supported iMC Interleave sizes
  @param[out] ppRecommendedFormats Array of recommended formats
  @param[out] ppChannelWays Array of supported channel ways
  @param[out] pLength Length of the array
  @param[out] pInterleaveAlignmentSize Interleave Alignment Size
  @param[out] pRevision PCAT Table revision

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES Memory Allocation failure
  @retval EFI_INVALID_PARAMETER ppChannelInterleaveSize, ppiMCInterleaveSize or pLength is NULL
  @retval EFI_NOT_FOUND Interleave size info not found
**/
EFI_STATUS
RetrieveSupportediMcAndChannelInterleaveSizes(
  OUT  UINT32 **ppChannelInterleaveSize,
  OUT  UINT32 **ppiMCInterleaveSize,
  OUT  UINT32 **ppRecommendedFormats,
  OUT  UINT32 **ppChannelWays,
  OUT  UINT32 *pLength,
  OUT  UINT32 *pInterleaveAlignmentSize,
  OUT  ACPI_REVISION *pRevision
  );

/**
  Retrieve InterleaveSetMap Info

  @param[out] ppInterleaveMap Info List used to determine the best interleave based on requested DCPMMs
  @param[out] pInterleaveMapListLength Pointer to the InterleaveSetMap Length

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES Memory Allocation failure
  @retval EFI_INVALID_PARAMETER ppInterleaveSetMap or InterleaveMapListLength is NULL
  @retval EFI_NOT_FOUND InterleaveSetMap Info not found
**/
EFI_STATUS
RetrieveInterleaveSetMap(
  OUT  UINT32 **ppInterleaveMap,
  OUT  UINT32 *pInterleaveMapListLength
  );

/**
  Retrieve Channel ways from InterleaveSetMap Info

  @param[out] ppChannelWays Array of channel ways supported
  @param[out] pChannelWaysListLength Pointer to the ppChannelWays array Length

  @retval EFI_SUCCESS Success
  @retval EFI_OUT_OF_RESOURCES Memory Allocation failure
  @retval EFI_INVALID_PARAMETER ppInterleaveSetMap or InterleaveMapListLength is NULL
  @retval EFI_NOT_FOUND InterleaveSetMap Info not found
**/
EFI_STATUS
RetrieveChannelWaysFromInterleaveSetMap(
  OUT  UINT32 **ppChannelWays,
  OUT  UINT32 *pChannelWaysListLength
  );

/**
  Performs deserialization from binary memory block containing PMTT table and checks if memory mode can be configured.

  @param[in] pTable pointer to the memory containing the PMTT binary representation.

  @retval false if topology does NOT allows MM.
  @retval true if topology allows MM.
**/
BOOLEAN
CheckIsMemoryModeAllowed(
  IN TABLE_HEADER *pPMTT
  );

/**
  Retrieve Maximum PM Interleave Sets per Die & DCPMM

  @param[out] pMaxPMInterleaveSets Pointer to Maximum PM Interleave Sets per Die & Dcpmm

  @retval EFI_SUCCESS Success
  @retval EFI_INVALID_PARAMETER pMaxPMInterleaveSetsPerDie or pMaxPMInterleaveSetsPerDcpmm is NULL
  @retval EFI_NOT_FOUND InterleaveSetMap Info not found
**/
EFI_STATUS
RetrieveMaxPMInterleaveSets(
  OUT  MAX_PMINTERLEAVE_SETS *pMaxPMInterleaveSets
  );
#endif
