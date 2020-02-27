/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _REGION_H_
#define _REGION_H_

#include <Library/BaseMemoryLib.h>
#include <Debug.h>
#include <Types.h>
#include <AcpiParsing.h>
#include <PlatformConfigData.h>
#include <Dimm.h>
#include <NvmTypes.h>

#define INTERLEAVE_WAYS_X1  1
#define INTERLEAVE_WAYS_X2  2
#define INTERLEAVE_WAYS_X3  3
#define INTERLEAVE_WAYS_X4  4
#define INTERLEAVE_WAYS_X8  8
#define INTERLEAVE_WAYS_X12 12
#define INTERLEAVE_WAYS_X16 16
#define INTERLEAVE_WAYS_X24 24

/** IS_STATE value indicates its priority (0 is lowest) **/
#define IS_STATE_HEALTHY          0
#define IS_STATE_SPA_MISSING      1
#define IS_STATE_CONFIG_INACTIVE  2
#define IS_STATE_DIMM_MISSING     3
#define IS_STATE_INIT_FAILURE     4
struct _NAMESPACE;
typedef struct _DIMM_REGION {
  LIST_ENTRY DimmRegionNode;
  UINT64 Signature;
  DIMM *pDimm;
  UINT64 PartitionOffset;
  UINT64 PartitionSize;
  UINT64 SpaRegionOffset;
} DIMM_REGION;

#define DIMM_REGION_SIGNATURE     SIGNATURE_64('D', 'I', 'M', 'M', 'R', 'E', 'O', 'N')
#define DIMM_REGION_FROM_NODE(a)  CR(a, DIMM_REGION, DimmRegionNode, DIMM_REGION_SIGNATURE)

typedef struct _NVM_IS
{
  LIST_ENTRY IsNode;
  UINT64 Signature;
  UINT16 SocketId;                  //!< Identifies the processor socket containing the DCPMM
  UINT16 InterleaveSetIndex;
  UINT16 RegionId;                 //!< Used to uniquely identify regions as InterleavesetIndex is not unique enough
  UINT64 Size;                      //!< Current total capacity of the Interleave Setqq
  /**
    bit0 set - IS_STATE_INIT_FAILURE - Interleave Set or dimm region (one or more) initialization failure
    bit1 set - IS_STATE_DIMM_MISSING - dimm missing (serial number of dimm from the Platform Config Data not found
                                       in the dimm list)
  **/
  UINT8 State;
  UINT16 InterleaveFormatChannel;
  UINT16 InterleaveFormatImc;
  UINT16 InterleaveFormatWays;
  BOOLEAN MirrorEnable;
  LIST_ENTRY DimmRegionList;
  SpaRangeTbl *pSpaTbl;
  LIST_ENTRY AppDirectNamespaceList;
  UINT64 InterleaveSetCookie;
  UINT64 InterleaveSetCookie_1_1;
} NVM_IS;

#define IS_SIGNATURE     SIGNATURE_64('I', 'N', 'T', 'S', '_', 'S', 'I', 'G')
#define IS_FROM_NODE(a)  CR(a, NVM_IS, IsNode, IS_SIGNATURE)

typedef struct _NVM_REGION {
  LIST_ENTRY RegionNode;
  UINT64 Signature;
  UINT16 RegionId;
  UINT16 Socket;
  UINT8 Type;
  BOOLEAN MirrorEnable;
  DIMM *pDimmsBlockOnly[MAX_DIMMS_PER_SOCKET];
  UINT32 DimmsBlockOnlyNum;
  UINT64 BlockOnlySize;
  NVM_IS *pISs[MAX_IS_PER_SOCKET];
  UINT32 ISsNum;
  UINT64 ISsSize;
  UINT64 VolatileSize;
} NVM_REGION;

typedef struct _REGION_GOAL_DIMM {
  DIMM *pDimm;
  UINT64 RegionSize;
  UINT64 VolatileSize;
} REGION_GOAL_DIMM;

#define NVM_REGION_SIGNATURE     SIGNATURE_64('R', 'E', 'G', 'I', '_', 'S', 'I', 'G')
#define NVM_REGION_FROM_NODE(a)  CR(a, NVM_REGION, RegionNode, NVM_REGION_SIGNATURE)

typedef struct _REGION_GOAL {
  UINT32 SequenceIndex;       //!< Variable to keep an order of REGIONS on DIMMs
  UINT64 Size;                //!< Size of the pool in bytes
  UINT8 InterleaveSetType;    //!< Type of interleave set: non-interleaved, interleaved, mirrored
  UINT8 ImcInterleaving;      //!< IMC interleaving as bit field
  UINT8 ChannelInterleaving;  //!< Channel interleaving as bit field
  UINT16 NumOfChannelWays;    //!< Number of channel ways as bit field
  UINT16 InterleaveSetIndex;  //!< Logical index number, it should be the same for all the DIMMs in the interleave set
  DIMM *pDimms[MAX_DIMMS_PER_SOCKET];
  UINT32 DimmsNum;
} REGION_GOAL;

/**
  Allocate and initialize the Interleave Set by using NFIT table

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pNvDimmRegionMappingStructure The NVDIMM region that helps describe this region of memory
  @param[in] RegionId The next consecutive region id
  @param[out] ppIS Interleave Set parent for new dimm region

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeISFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionTbl,
  IN     UINT16 RegionId,
  OUT NVM_IS **ppIS
);

/**
  Allocate and initialize the Interleave Set by using Interleave Information table from Platform Config Data

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeIS(
  IN     VOID *pInterleaveInfoTable,
  IN     UINT16 RegionId,
  IN     ACPI_REVISION PcdConfRevision,
  OUT NVM_IS **ppIS
  );

/**
  Create and initialize all Interleave Sets.

  When something goes wrong with particular Interleave Set then no additional Interleave Set structs created or
  error state on Interleave Set is set.

  @param[in] pFitHead NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pISList Head of the list for Interleave Sets


  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeISs(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
  IN     BOOLEAN UseNfit,
     OUT LIST_ENTRY *pISList
  );

/**
  Initialize interleave sets
  It initializes the interleave sets using NFIT or PCD

  @param[in] UseNfit Flag to indicate usage of NFIT or else default to PCD

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeInterleaveSets(
  IN     BOOLEAN UseNfit
  );

/**
Determine Region Type based on the Interleave sets

@param[in, out] pRegion    The region whose type needs to be determined

@retval EFI_SUCCESS
@retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
DetermineRegionType(
  IN NVM_IS *pRegion,
  OUT UINT8 *pRegionType
);

/**
  Get Region by ID
  Scan the Region list for a Region identified by ID

  @param[in] pRegionList Head of the list for Regions
  @param[in] RegionId Region identifier

  @retval NVM_IS struct pointer if matching Region has been found
  @retval NULL pointer if not found
  **/
NVM_IS *
GetRegionById(
  IN     LIST_ENTRY *pRegionList,
  IN     UINT16 RegionId
);

/**
  Get Region List
  Retruns the pointer to the region list.
  It's also initializing the region list if it's necessary.

  @param[in] pRegionList Head of the list for Regions
  @param[in] UseNfit Flag to indicate usage of NFIT

  @retval pointer to the region list
**/
EFI_STATUS
GetRegionList(
  IN     LIST_ENTRY **ppRegionList,
  IN     BOOLEAN UseNfit
  );

/**
  Clean the Interleave Set

  @param[in, out] pDimmList: the list of DCPMMs
  @param[in, out] pISList: the list of Interleave Sets to clean
**/
VOID
CleanISLists(
  IN OUT LIST_ENTRY *pDimmList,
  IN OUT LIST_ENTRY *pISList
  );

/**
  Free a Interleave Set and all memory resources in use by the Interleave Set.

  @param[in, out] pIS the Interleave Set and its regions that will be released
**/
VOID
FreeISResources(
  IN OUT NVM_IS *pIS
  );

/**
  Allocate and initialize the DIMM region by using NFIT table

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimm Target DIMM structure pointer
  @param[in] pISList List of interleaveset formed so far
  @param[in] pNvDimmRegionMappingStructure The NVDIMM region that helps describe this region of memory
  @param[out] pRegionId The next consecutive region id
  @param[out] ppNewIS Interleave Set parent for new dimm region
  @param[out] ppDimmRegion new allocated dimm region will be put here
  @param[out] pISDimmRegionAlreadyExists TRUE if Interleave Set DIMM region already exists

  @retval EFI_SUCCESS
  @retval EFI_NOT_FOUND the Dimm related with DimmRegion has not been found on the Dimm list
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeDimmRegionFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN     DIMM *pDimm,
  IN     LIST_ENTRY *pISList,
  IN     NvDimmRegionMappingStructure *pNvDimmRegionMappingStructure,
  OUT    UINT16 *pRegionId,
  OUT    NVM_IS **ppCurrentIS,
  OUT    DIMM_REGION **ppDimmRegion,
  OUT    BOOLEAN *pISDimmRegionAlreadyExists
  );

/**
  Allocate and initialize the dimm region by using Interleave Information table from Platform Config Data

  @param[in] pCurDimm the DIMM from which Interleave Information table was retrieved
  @param[in] pDimmList Head of the list of all Intel NVM Dimm in the system
  @param[in] pISList List of interleaveset formed so far
  @param[in] pIdentificationInfoTable Identification Information table
  @param[in] pInterleaveInfoTable Interleave information for the particular dimm
  @param[in] PcdConfRevision Revision of the PCD Config tables
  @param[out] pRegionId The next consecutive region id
  @param[out] ppNewIS Interleave Set parent for new dimm region
  @param[out] ppDimmRegion new allocated dimm region will be put here
  @param[out] pISAlreadyExists TRUE if Interleave Set already exists

  @retval EFI_SUCCESS
  @retval EFI_NOT_FOUND the Dimm related with DimmRegion has not been found on the Dimm list
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
InitializeDimmRegion(
  IN     DIMM *pCurDimm,
  IN     LIST_ENTRY *pDimmList,
  IN     LIST_ENTRY *pISList,
  IN     VOID *pIdentificationInfoTable,
  IN     VOID *pInterleaveInfoTable,
  IN     ACPI_REVISION PcdConfRevision,
  OUT    UINT16 *pRegionId,
  OUT    NVM_IS **ppNewIS,
  OUT    DIMM_REGION **ppDimmRegion,
  OUT    BOOLEAN *pISAlreadyExists
  );

/**
  Retrieve Interleave Sets by using NFIT table

  Using the parsed NFIT table data to get information about Interleave Sets configuration.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pISList Head of the list for Interleave Sets

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveISsFromNfit(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
     OUT LIST_ENTRY *pISList
);

/**
  Retrieve Interleave Sets by using Platform Config Data from Intel NVM Dimms

  Using the Platform Config Data command to get information about Interleave Sets configuration.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pISList Head of the list for Interleave Sets

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveISsFromPlatformConfigData(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
     OUT LIST_ENTRY *pISList
  );

/**
  Parse Interleave Information table and create a Interleave Set if it doesn't exist yet.

  @param[in] pFitHead Fully populated NVM Firmware Interface Table
  @param[in] pDimmList Head of the list of all Intel NVM Dimms in the system
  @param[in] pInterleaveInfo Interleave Information table retrieve from DIMM
  @param[in] PcdCurrentConfRevision PCD Current Config table revision
  @param[in] pDimm the DIMM from which Interleave Information table was retrieved
  @param[in out] pRegionId Unique id for region
  @param[out] pISList Head of the list for Interleave Sets

  @retval EFI_SUCCESS
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveISFromInterleaveInformationTable(
  IN     ParsedFitHeader *pFitHead,
  IN     LIST_ENTRY *pDimmList,
  IN     VOID *pInterleaveInfoTable,
  IN     ACPI_REVISION PcdCurrentConfRevision,
  IN     DIMM *pDimm,
  IN OUT UINT16 *pRegionId,
     OUT LIST_ENTRY *pISList
  );

/**
  Clear previous pools goal configs and - if pools goal configs is specified - replace them with new one.

  1. Clear previous pools goal configs on all affected dimms
  2. [OPTIONAL] Send new pools goal configs to dimms
  3. Set information about synchronization with dimms

  @param[in] pDimmList Head of the list of all NVM DIMMs in the system
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pDimmList is NULL
  @retval return codes from SendConfigInputToDimm
**/
EFI_STATUS
ApplyGoalConfigsToDimms(
  IN     LIST_ENTRY *pDimmList,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Send new Configuration Input to dimm.

  Get Platform Config Data from dimm, replace Configuration Input with new one, and send it back
  to dimm. If pNewConfigInput is NULL, then the function will send Platform Config Data without
  Configuration Input (old one will be removed).

  @param[in] pDimm dimm that we replace Platform Config Data for
  @param[in] pNewConfigInput new config input for dimm

  @retval EFI_SUCCESS success
  @retval EFI_INVALID_PARAMETER pDimm is NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
  @retval other error codes from called functions:
          FwCmdGetPlatformConfigData, FwCmdSetPlatformConfigData
**/
EFI_STATUS
SendConfigInputToDimm(
  IN     DIMM *pDimm,
  IN     NVDIMM_PLATFORM_CONFIG_INPUT *pNewConfigInput OPTIONAL
  );


/**
  Verify that all specified features in goal config are supported by platform

  @param[in] VolatileSize Volatile region size
  @param[in] PersistentMemType Persistent memory type
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER If one or more parameters are NULL
  @retval EFI_UNSUPPORTED A given config is unsupported
  @retval EFI_LOAD_ERROR There is no PCAT
**/
EFI_STATUS
VerifyPlatformSupport(
  IN     UINT64 VolatileSize,
  IN     UINT8 PersistentMemType,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Check if the platform supports specified interleave sets. Also set default iMC and Channel interleave sizes if
  they are not specified.

  @param[in, out]  pRegionGoal Array of pointers to pools goal
  @param[in]       pRegionGoal Number of pointers in pRegionGoal
  @param[out]      pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_LOAD_ERROR  PCAT or its subtable not found
  @retval EFI_ABORTED     Invalid pool configuration, not supported by platform
**/
EFI_STATUS
VerifyInterleaveSetsPlatformSupport(
  IN OUT REGION_GOAL *pRegionGoal[],
  IN     UINT32 RegionGoalNum,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Determine Regions health based on health state of Interleave Sets

  @param[in] pRegion    The pool whose health is to be determined

  @param[out] pHealthState The health state of the pool
  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Could not allocate memory
**/
EFI_STATUS
DetermineRegionHealth(
  IN  NVM_IS *pRegion,
  OUT UINT16 *pHealthState
  );

/**
  Get minimum and maximum sizes of AppDirect Namespace that can be created on the Interleave Set

  @param[in]  pIS      Interleave Set that sizes of AppDirect Namespaces will be determined for
  @param[out] pMinSize Output parameter for minimum size
  @param[out] pMaxSize Output parameter for maximum size

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
ADNamespaceMinAndMaxAvailableSizeOnIS(
  IN     NVM_IS *pIS,
     OUT UINT64 *pMinSize,
     OUT UINT64 *pMaxSize
  );


/**
  Retrieve goal configurations by using Platform Config Data

  @param[in, out] pDimmList Head of the list of all NVM DIMMs in the system

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveGoalConfigsFromPlatformConfigData(
  IN OUT LIST_ENTRY *pDimmList,
  IN     BOOLEAN RestoreCorrupt
  );

/**
  Clear all internal goal configurations structures

  @param[in, out] pDimmList Head of the list of all NVM DIMMs in the system

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
ClearInternalGoalConfigsInfo(
  IN OUT LIST_ENTRY *pDimmList
  );

/**
  Check if specified persistent memory type contain valid value

  @param[in] PersistentMemType Persistent memory type

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER Specified value is invalid
**/
EFI_STATUS
PersistentMemoryTypeValidation(
  IN     UINT8 PersistentMemType
  );

/**
  Select one reserve Dimm from specified list of Dimms and remove it from the list

  @param[in, out] pDimms Array of pointers to DIMMs
  @param[in, out] pDimmsNum Number of pointers in pDimms
  @param[out] ppReserveDimm Selected Dimm from the list

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
SelectReserveDimm(
  IN OUT DIMM *pDimms[MAX_DIMMS],
  IN OUT UINT32 *pDimmsNum,
     OUT DIMM **ppReserveDimm
  );

/**
  Check if specified AppDirect Settings will conflict with existing AppDirect Interleaves

  @param[in] pDriverPreferences Driver preferences for AppDirect Provisioning
  @param[out] pConflict True, conflict exists with existing AppDirect Memory
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER parameter is NULL
**/
EFI_STATUS
AppDirectSettingsConflict(
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT BOOLEAN *pConflict,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Check if specified AppDirect Settings contain valid values

  @param[in] pDriverPreferences Driver preferences for AppDirect Provisioning

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER parameter is NULL or specified values are not valid
  @retval EFI_ABORTED Unable to find required system tables
**/
EFI_STATUS
AppDirectSettingsValidation(
  IN     DRIVER_PREFERENCES *pDriverPreferences
  );

/**
  Calculate actual volatile size with subtracted metadata size

  @param[in] RawDimmCapacity Raw capacity to calculate actual volatile size for
  @param[in] VolatileSizeRounded Rounded Volatile region size - without subtracted metadata size
  @param[out] pVolatileSizeActual Actual Volatile region size - with subtracted metadata size

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
CalculateActualVolatileSize(
  IN     UINT64 RawDimmCapacity,
  IN     UINT64 VolatileSizeRounded,
     OUT UINT64 *pVolatileSizeActual
  );

/**
  Calculate system wide capacity for a given percent

  @param[in] pDimms Array of pointers to DIMMs
  @param[in] pDimmsNum Number of pointers in pDimms
  @param[in] Percent Percent to calculate
  @param[out] pDimmsCapacity Output dimms capacity in bytes

  @retval EFI_INVALID_PARAMETER One or more parameters are invalid
  @retval EFI_SUCCESS Success
**/
EFI_STATUS
CalculateDimmCapacityFromPercent(
  IN     DIMM *pDimms[MAX_DIMMS],
  IN     UINT32 DimmsNum,
  IN     UINT32 Percent,
     OUT UINT64 *pDimmsCapacity
  );

/**
  Verify DIMMs SKU support

  @param[in] ppDimms Array of dimms
  @param[in] DimmsNum Number of dimms
  @param[in, out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS No SKU violation
  @retval EFI_ABORTED SKU violation
  @retval EFI_INVALID_PARAMETER Input parameter is NULL
**/
EFI_STATUS
VerifySKUSupportForCreateGoal(
  IN     DIMM **ppDimms,
  IN     UINT32 DimmsNum,
  IN OUT COMMAND_STATUS *pCommandStatus
  );

/**
Calculate free Region capacity

@param[in]  pRegion       Region that a free capacity will be calculated for
@param[out] pFreeCapacity Output parameter for result

@retval EFI_SUCCESS
@retval EFI_INVALID_PARAMETER one or more parameters are NULL
@retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
GetFreeRegionCapacity(
  IN  NVM_IS *pRegion,
  OUT UINT64 *pFreeCapacity
);

/**
  Map specified request to actual Region Goal templates. Resolve special "remaining" values.

  @param[in] pDimms Array of pointers to manageable DIMMs only
  @param[in] pDimmsNum Number of pointers in pDimms
  @param[out] DimmsSymmetrical Array of Dimms for symmetrical pool config
  @param[out] pDimmsSymmetricalNum Returned number of items in DimmsSymmetrical
  @param[out] DimmsAsymmetrical Array of Dimms for asymmetrical pool config
  @param[out] pDimmsAsymmetricalNum Returned number of items in DimmsAsymmetrical
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatileSize Volatile region size
  @param[in] ReservedPercent Amount of AppDirect memory to not map in percent
  @param[in] pMaxPMInterleaveSets Pointer to MaxPmInterleaveSets per Die & per Dcpmm
  @param[out] pVolatileSizeActual Actual Volatile region size
  @param[out] RegionGoalTemplates Array of pool goal templates
  @param[out] pRegionGoalTemplatesNum Number of items in RegionGoalTemplates
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
MapRequestToActualRegionGoalTemplates(
  IN     DIMM *pDimms[MAX_DIMMS],
  IN     UINT32 DimmsNum,
     OUT REGION_GOAL_DIMM DimmsSymmetrical[MAX_DIMMS],
     OUT UINT32 *pDimmsSymmetricalNum,
     OUT REGION_GOAL_DIMM DimmsAsymmetrical[MAX_DIMMS],
     OUT UINT32 *pDimmsAsymmetricalNum,
  IN     UINT8 PersistentMemType,
  IN     UINT64 VolatileSize,
  IN     UINT32 ReservedPercent,
  IN     MAX_PMINTERLEAVE_SETS *pMaxPMInterleaveSets,
     OUT UINT64 *pVolatileSizeActual OPTIONAL,
     OUT REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM],
     OUT UINT32 *pRegionGoalTemplatesNum,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Parse Interleave Information table and retrieve Region Goal (create a Region Goal if it doesn't exist yet)

  @param[in] pRegionGoals Array of all Region Goals in the system
  @param[in] RegionGoalsNum Number of pointers in pRegionGoals
  @param[in] pInterleaveInfo Interleave Information table retrieved from DIMM
  @param[in] PcdCinRev Revision of the PCD Config Input table
  @param[out] ppRegionGoal Output variable for Region Goal
  @param[out] pNew True if Region Goal new created, False if already exists

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES memory allocation failure
**/
EFI_STATUS
RetrieveRegionGoalFromInterleaveInformationTable(
  IN     REGION_GOAL *pRegionGoals[],
  IN     UINT32 RegionGoalsNum,
  IN     VOID *pInterleaveInfo,
  IN     ACPI_REVISION PcdCinRev,
     OUT REGION_GOAL **ppRegionGoal,
     OUT BOOLEAN *pNew
  );

/**
  Map Regions Goal configs on specified DIMMs

  @param[in] DimmsSym Array of Dimms for symmetrical region config
  @param[in] DimmsSymNum Number of items in DimmsSym
  @param[in] DimmsAsym Array of Dimms for asymmetrical region config
  @param[in] DimmsAsymNum Number of items in DimmsAsym
  @param[in, out] pReserveDimm Dimm that its whole capacity will be set as persistent partition
  @param[in] ReserveDimmType Type of reserve dimm
  @param[in] VolatileSize Volatile region size in bytes
  @param[in] RegionGoalTemplates Array of template goal REGIONs
  @param[in] RegionGoalTemplatesNum Number of elements in RegionGoalTemplates
  @param[in] pDriverPreferences Driver preferences for interleave sets
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or input DIMMs are different than affected DIMMs
**/
EFI_STATUS
MapRegionsGoal(
  IN     REGION_GOAL_DIMM DimmsSym[MAX_DIMMS],
  IN     UINT32 DimmsSymNum,
  IN     REGION_GOAL_DIMM DimmsAsym[MAX_DIMMS],
  IN     UINT32 DimmsAsymNum,
  IN OUT DIMM *pReserveDimm OPTIONAL,
  IN     UINT8 ReserveDimmType OPTIONAL,
  IN     UINT64 VolatileSize,
  IN     REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM],
  IN     UINT32 RegionGoalTemplatesNum,
  IN     DRIVER_PREFERENCES *pDriverPreferences,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Delete regions goal configs from input DIMMs and (if force is true) all DIMMs related by regions

  @param[in, out] pDimms Array of pointers to DIMMs
  @param[in] DimmsNum Number of pointers in pDimms
  @param[in] Force Force to perform deleting regions goal configs on all affected DIMMs
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or input DIMMs are different than affected DIMMs
**/
EFI_STATUS
DeleteRegionsGoalConfigs(
  IN OUT DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Verify that all the unconfigured DIMMs or all DIMMs on a given socket are configured at once to keep supported
  region configs.

  @param[in] pDimms List of DIMMs to configure
  @param[in] DimmsNum Number of DIMMs to configure
  @param[in] PersistentMemType Persistent memory type
  @param[in] VolatilePercent Volatile region size in percents
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one or more parameters are NULL
  @retval EFI_UNSUPPORTED A given config is unsupported
**/
EFI_STATUS
VerifyCreatingSupportedRegionConfigs(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     UINT8 PersistentMemType,
  IN     UINT32 VolatilePercent,
  OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Reduce system wide socket mapped memory to align with the system memory mapped SKU limits per Socket.
  @param[in] Socket  Socket Id for SKU limit calculations
  @param[in] pDimms Array of pointers to manageable DIMMs only
  @param[in] NumDimmsOnSocket Number of pointers in pDimms
  @param[in out] DimmsSymmetricalOnSocket Array of Dimms for symmetrical region config
  @param[in out] pDimmsSymmetricalNumOnSocket Returned number of items in DimmsSymmetrical
  @param[in out] DimmsAsymmetricalOnSocket Array of Dimms for asymmetrical region config
  @param[in out] pDimmsAsymmetricalNumOnSocket Returned number of items in DimmsAsymmetrical
  @param[in out] RegionGoalTemplates Array of region goal templates
  @param[in out] pRegionGoalTemplatesNum Number of items in RegionGoalTemplates
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_UNSUPPORTED Bad values retrieved from PCAT
**/
EFI_STATUS
ReduceCapacityForSocketSKU(
  IN     UINT32 Socket,
  IN     DIMM *pDimmsOnSocket[MAX_DIMMS],
  IN     UINT32 NumDimmsOnSocket,
  IN OUT REGION_GOAL_DIMM DimmsSymmetricalOnSocket[MAX_DIMMS],
  IN OUT UINT32 *pDimmsSymmetricalNumOnSocket,
  IN OUT REGION_GOAL_DIMM DimmsAsymmetricalOnSocket[MAX_DIMMS],
  IN OUT UINT32 *pDimmsAsymmetricalNumOnSocket,
  IN OUT REGION_GOAL_TEMPLATE RegionGoalTemplates[MAX_IS_PER_DIMM],
  IN OUT UINT32 *pRegionGoalTemplatesNum,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Verify that specified Dimms don't affect other Dimms by current Regions or Regions goal configs

  @param[in, out] pDimms Array of pointers to DIMMs
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pCommandStatus Pointer to command status structure

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL or specified Dimms affect other Dimms
**/
EFI_STATUS
ValidateRegionsCorrelations(
  IN OUT DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Find related dimms based on region configs relations

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pRelatedDimms Output array of pointers to dimms
  @param[out] pRelatedDimmsNum Output number of found dimms

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
FindRelatedDimmsByRegions(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT DIMM *pRelatedDimms[MAX_DIMMS],
     OUT UINT32 *pRelatedDimmsNum
  );

/**
  Find related dimms based on region goal configs relations

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pRelatedDimms Output array of pointers to dimms
  @param[out] pRelatedDimmsNum Output number of found dimms

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
FindRelatedDimmsByRegionGoalConfigs(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT DIMM *pRelatedDimms[MAX_DIMMS],
     OUT UINT32 *pRelatedDimmsNum
  );

/**
  Find an unique list of goal regions based on input list of dimms

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[out] pRegionsGoal Output array of pointers to REGION_GOAL
  @param[out] pRegionsGoalNum Output number of unique goal regions

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_ABORTED More config goals found than can exist
**/
EFI_STATUS
FindUniqueRegionsGoal(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT REGION_GOAL *pRegionsGoal[MAX_IS_CONFIGS],
     OUT UINT32 *pRegionsGoalNum
  );

/**
  Cleans up pointers that are about to be freed so that double-free doesn't take place later on

  @param[in] pDimms Input array of pointers to dimms
  @param[in] DimmsNum Number of pointers in pDimms
  @param[in] pRegionsGoal to list of regions containing the candidate pointers
  @param[in] pRegionsGoalNum the number of region goal items

  **/
EFI_STATUS
ClearRegionsGoal(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     REGION_GOAL **pRegionsGoal,
  IN     UINT32 pRegionsGoalNum
);

/**
  Create REGION goal

  @param[in] pRegionGoalTemplate Pointer to REGION goal template
  @param[in] pDimms Array of pointers to DIMMs
  @param[in] DimmsNum Number of pointers in pDimms
  @param[in] InterleaveSetSize Interleave set size
  @param[in] pDriverPreferences Driver preferences for interleave sets Optional
  @param[in] SequenceIndex Variable to keep an order of REGIONs on DIMMs
  @param[in, out] pInterleaveSetIndex Unique index for interleave set

  @retval REGION_GOAL success
  @retval NULL one or more parameters are NULL or memory allocation failure
**/
REGION_GOAL *
CreateRegionGoal(
  IN     REGION_GOAL_TEMPLATE *pRegionGoalTemplate,
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
  IN     UINT64 InterleaveSetSize,
  IN     DRIVER_PREFERENCES *pDriverPreferences OPTIONAL,
  IN     UINT16 SequenceIndex,
  IN OUT UINT16 *pInterleaveSetIndex
  );

/**
  Verify that all DIMMs with goal config are specified on a given socket at once to keep supported region configs.

  @param[in] pDimms List of DIMMs to configure
  @param[in] DimmsNum Number of DIMMs to configure
  @param[out] pCommandStatus Structure containing detailed NVM error codes

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER if one or more parameters are NULL
  @retval EFI_UNSUPPORTED A given config is unsupported
**/
EFI_STATUS
VerifyDeletingSupportedRegionConfigs(
  IN     DIMM *pDimms[],
  IN     UINT32 DimmsNum,
     OUT COMMAND_STATUS *pCommandStatus
  );

/**
  Find Region Goal Dimm in an array

  @param[in] pDimms Array that will be searched
  @param[in] DimmsNum Number of items in pDimms
  @param[in] pDimmToFind Dimm to find

  @retval Region Goal Dimm pointer - if found
  @retval NULL - if not found
**/
REGION_GOAL_DIMM *
FindRegionGoalDimm(
  IN     REGION_GOAL_DIMM *pDimms,
  IN     UINT32 DimmsNum,
  IN     DIMM *pDimmToFind
  );


/**
  Calculate actual volatile size

  @param[in] RawDimmCapacity Raw capacity to calculate actual volatile size for
  @param[in] VolatileSizeRounded
  @param[out] pVolatileSizeActual Actual Volatile region size - actual volatile size with metadata

  @retval EFI_SUCCESS
  @retval EFI_INVALID_PARAMETER one or more parameters are NULL
**/
EFI_STATUS
CalculateActualVolatileSize(
  IN     UINT64 RawDimmCapacity,
  IN     UINT64 VolatileSizeRequested,
     OUT UINT64 *pVolatileSizeActual
  );

/**
  Check for new goal configs for the DIMM

  @param[in] pDIMM The current DIMM
  @param[out] pHasNewGoal TRUE if any of the dimms have a new goal, else FALSE

  @retval EFI_SUCCESS
  #retval EFI_INVALID_PARAMETER If IS is null
**/
EFI_STATUS
FindIfNewGoalOnDimm(
  IN     DIMM *pDimm,
     OUT BOOLEAN *pHasNewGoal
  );

/**
  Check if security state of specified DIMM is locked

  @param[in] pDimm The current DIMM
  @param[out] pIsLocked TRUE if security state of specified dimm is locked

  @retval EFI_SUCCESS
  #retval EFI_INVALID_PARAMETER one or more parameters are NULL
  @retval EFI_OUT_OF_RESOURCES Allocation failed
**/
EFI_STATUS
IsDimmLocked(
  IN     DIMM *pDimm,
     OUT BOOLEAN *pIsLocked
  );

/**
  Set Interleave Set state taking states' priority into account

  @param[in] CurrentState Current IS state
  @param[in] NewState IS state to be set

  @retval UINT8 New IS state
**/
UINT8
SetISStateWithPriority(
  IN    UINT8 CurrentState,
  IN    UINT8 NewState
  );

/**
  Check for existing goal configs on a socket for which a new goal config has been requested

  @param[in] pDimms Array of pointers to DIMMs based on the goal config requested
  @param[in] pDimmsNum Number of pointers in pDimms

  @retval EFI_ABORTED one or more DIMMs on a socket already have goal configs
  @retval EFI_INVALID_PARAMETER pDimms or pDimmsNum is NULL
**/
EFI_STATUS
CheckForExistingGoalConfigPerSocket(
  IN    DIMM *pDimms[MAX_DIMMS],
  IN    UINT32 *pDimmsNum
  );
#endif
