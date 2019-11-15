/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /**
 * @file NvmStatus.h
 * @brief Status Types for EFI_NVMDIMMS_CONFIG_PROTOCOL.
 */

#ifndef _NVM_STATUS_H_
#define _NVM_STATUS_H_

#include "NvmLimits.h"
#include "NvmSharedDefs.h"

#ifdef OS_BUILD
#include <os_types.h>
#endif

/** Return code status values as identified by #NvmStatusCode */
typedef INT16 NVM_STATUS;

/** Structures for CLI command status output **/

/** Object type used by #COMMAND_STATUS **/
typedef enum {
  ObjectTypeSocket    = 0,  ///< Socket
  ObjectTypeDimm      = 1,  ///< DIMM
  ObjectTypeRegion    = 2,  ///< Region
  ObjectTypeNamespace = 3,  ///< Namespace
  ObjectTypeUnknown   = 4   ///< Unknown
} OBJECT_TYPE;

/** List head functions **/

/** #OBJECT_STATUS Signature */
#define OBJECT_STATUS_SIGNATURE      SIGNATURE_64('O', 'B', 'J', 'S', 'T', 'A', 'T', 'S')

/* Helper function to get #OBJECT_STATUS from a node */
#define OBJECT_STATUS_FROM_NODE(a)   CR(a, OBJECT_STATUS, ObjectStatusNode, OBJECT_STATUS_SIGNATURE)

/** Status bit field for #OBJECT_STATUS */
typedef struct _NVM_STATUS_BIT_FIELD {
  UINT64 BitField[(NVM_LAST_STATUS_VALUE / 64) + 1];
} NVM_STATUS_BIT_FIELD;

/** Max length of the ObjectIdStr string */
#define MAX_OBJECT_ID_STR_LEN  30

/** Object status list structure **/
typedef struct {
  LIST_ENTRY ObjectStatusNode;                ///< Object status node list pointer
  UINT64 Signature;                           ///< Signature must match #OBJECT_STATUS_SIGNATURE
  UINT32 ObjectId;                            ///< Object ID
  BOOLEAN IsObjectIdStr;                      ///< Is #ObjectIdStr valid?
  CHAR16 ObjectIdStr[MAX_OBJECT_ID_STR_LEN];  ///< String representation of Object ID
  NVM_STATUS_BIT_FIELD StatusBitField;        ///< Bitfield for NVM Status
  UINT8 Progress;                             ///< Progress
} OBJECT_STATUS;

/**
  COMMAND_STATUS type contains executed command status code as well as
  status codes for every Object (SOCKET, DIMM, NAMESPACE) which were involved in this operation
**/
typedef struct {
  NVM_STATUS GeneralStatus;     ///< General return status
  OBJECT_TYPE ObjectType;       ///< Type of object
  LIST_ENTRY ObjectStatusList;  ///< List of #OBJECT_STATUS objects
  UINT16 ObjectStatusCount;     ///< Count of object on #ObjectStatusList
} COMMAND_STATUS;

/**
  Fill global variables containing all Error/Warning NVM Statuses
**/
VOID
InitErrorAndWarningNvmStatusCodes(
  );

/**
  Create command status as with specified command message.
  Function displays per DIMM status if such exists and
  summarizing status for whole command. Memory allocated
  for status message and command status is freed after
  status is displayed.

  @param[in] HiiHandle handle to the HII database that contains NvmStatusStrings
  @param[in] pStatusMessage String with command information
  @param[in] pStatusPreposition String with preposition
  @param[in] pCommandStatus Command status data
  @param[in] ObjectIdNumberPreferred Use Object ID number if true, use Object ID string otherwise
  @param[out] ppOutputMessage buffer where output will be saved

  Warning: ppOutputMessage - should be freed in caller.

  @retval EFI_INVALID_PARAMETER pCommandStatus is NULL
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
CreateCommandStatusString(
  IN     EFI_HANDLE HiiHandle,
  IN     CONST CHAR16 *pStatusMessage,
  IN     CONST CHAR16 *pStatusPreposition,
  IN     COMMAND_STATUS *pCommandStatus,
  IN     BOOLEAN ObjectIdNumberPreferred,
     OUT CHAR16 **ppOutputMessage
  );

/**
  Disable previously saved status code

  @param[in] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] Code code to check if is set
**/
VOID
ClearNvmStatus(
  IN     OBJECT_STATUS *pObjectStatus,
  IN     NvmStatusCode Code
  );

/**
  Check if Object status got proper NVM status code set.

  @param[in, out] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] ObjectId - object for clearing status
  @param[in] Status Status code to check if is set

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
BOOLEAN
IsSetNvmStatusForObject(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     NVM_STATUS Status
  );

/**
  Check if Object status got proper NVM status code set.

  @param[in] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] Code code to check if is set

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
BOOLEAN
IsSetNvmStatus(
  IN     OBJECT_STATUS *pObjectStatus,
  IN     NvmStatusCode Code
  );

/**
  Set proper code in Object status

  @param[in] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] Code code to set
**/
VOID
SetNvmStatus(
  IN     OBJECT_STATUS *pObjectStatus,
  IN     NvmStatusCode Code
  );

/**
  Search ObjectStatus from command status object list by specified Id and return pointer.

  @param[in] pCommandStatus - command status
  @param[in] ObjectId - Id for specified object

  @retval pointer to OBJECT_STATUS.
  @retval NULL if object with specified Id not found.
**/
OBJECT_STATUS *
GetObjectStatus(
  IN     COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId
  );

/**
  Add (or update!) status for specified ID in command status list

  @param[in, out] pCommandStatus - command status
  @param[in] ObjectId - Id for specified object
  @param[in] pObjectIdStr - Id for specified object as string representation, OPTIONAL
  @param[in] ObjectIdStrLength - Max length of pObjectIdStr, OPTIONAL
  @param[in] Status - status for update/set
**/
VOID
SetObjStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     CHAR16 *pObjectIdStr OPTIONAL,
  IN     UINT32 ObjectIdStrLength OPTIONAL,
  IN     NVM_STATUS Status
  );

/**
  Set progress for specified ID in command status list

  @param[in, out] pCommandStatus - command status
  @param[in] ObjectId - Id for specified object
  @param[in] Progress - progress to set
**/
VOID
SetObjProgress(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     UINT8 Progress
  );

/**
  Translate all NVM Statuses of operation into
  Unicode string representing its brief description.

  @param[in] pObjectStatus - object containing lit NvmDimm status codes
  @param[in] pPrefixString - string prefix for each status
  @param[out] pppOutputLines - pointer for table of rows
  @param[out] pRowCount - number of rows in pppOutputLines

  @retval Pointer to a decoded string. It points to a
    static memory block, the caller should not change the
    returned buffer nor free it.
**/
VOID
GetAllNvmStatusCodeMessagesAsTableOfLines(
  IN     OBJECT_STATUS *pObjectStatus,
  IN     CONST CHAR16 *pPrefixString,
     OUT CHAR16 ***pppOutputLines,
     OUT UINT32 *pRowCount
  );

/**
  Translate all NVM Statuses of operation into
  Unicode string representing its brief description.

  @param[in] HiiHandle - Handle to the Hii Database that contains the NvmStatus strings
  @param[in] pObjectStatus - Object status containing BitField with NVM Statuses
  @param[in] pPrefixString - prefix for all lines of statuses

  @retval Pointer to a decoded string. Memory is dynamically allocated. It should be freed by caller.
**/
CHAR16 *
GetAllNvmStatusCodeMessages(
  IN     EFI_HANDLE HiiHandle,
  IN     OBJECT_STATUS *pObjectStatus,
  IN     CONST CHAR16 *pPrefixString
  );

/**
  Clear Nvm status code for given object ID

  @param[in,out] pObjectStatus pointer to object status (with Nvm Status bit field)
  @param[in] ObjectId - object for checking status
  @param[in] Code code to clear

  @retval TRUE - if Object Status has got code set
  @retval FALSE - else
**/
VOID
ClearNvmStatusForObject(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     NvmStatusCode Code
  );

/**
  Translate NVM operation return code into
  Unicode string representing its brief description.

  @param[in] HiiHandle EFI HANDLE to the HII database that contains NvmStatus strings
  @param[in] Code the status code returned from
    a NVM command.

  @retval Pointer to a decoded string. Memory is dynamically allocated. It should be freed by caller.
**/
CHAR16 *
GetSingleNvmStatusCodeMessage(
  IN     EFI_HANDLE    HiiHandle,
  IN     NvmStatusCode Code
  );

/**
  Set general command status and zero status object counter

  @param[in, out] pCommandStatus - command status
  @param[in] Status - status for update/set
**/
VOID
ResetCmdStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     NVM_STATUS Status
  );

/**
  Set general command status

  @param[in, out] pCommandStatus - command status
  @param[in] Status - status for update/set
**/
VOID
SetCmdStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     NVM_STATUS Status
  );

#define NVM_ERROR(a)   ((a) != NVM_SUCCESS && (a) != NVM_SUCCESS_FW_RESET_REQUIRED)

#define THRESHOLD_UNDEFINED     0x7FFF                    // INT16 max positive value is treated as undefined value
#define ENABLED_STATE_UNDEFINED 0xFFUL                    // UINT8 max value is treated as undefined value
#define SENSOR_ID_UNDEFINED     0xFFUL

/**
  FV return codes.
**/
#define FW_SUCCESS                    0x00
#define FW_INVALID_COMMAND_PARAMETER  0x01
#define FW_DATA_TRANSFER_ERROR        0x02
#define FW_INTERNAL_DEVICE_ERROR      0x03
#define FW_UNSUPPORTED_COMMAND        0x04
#define FW_DEVICE_BUSY                0x05
#define FW_INCORRECT_PASSPHRASE       0x06
#define FW_AUTH_FAILED                0x07
#define FW_INVALID_SECURITY_STATE     0x08
#define FW_SYSTEM_TIME_NOT_SET        0x09
#define FW_DATA_NOT_SET               0x0A
#define FW_ABORTED                    0x0B
#define FW_REVISION_FAILURE           0x0D
#define FW_INJECTION_NOT_ENABLED      0x0E
#define FW_CONFIG_LOCKED              0x0F
#define FW_INVALID_ALIGNMENT          0x10
#define FW_INCOMPATIBLE_DIMM_TYPE     0x11
#define FW_TIMEOUT_OCCURED            0X12
#define FW_MEDIA_DISABLED             0x14
#define FW_UPDATE_ALREADY_OCCURED     0x15
#define FW_NO_RESOURCES               0x16

#define FW_ERROR(A)                   (A != FW_SUCCESS)

/**
  Initialize command status structure.
  Allocate memory and assign default values.

  @param[in] ppCommandStatus pointer to address of the structure

  @retval EFI_OUT_OF_RESOURCES Unable to allocate memory
  @retval EFI_SUCCESS All Ok
**/
EFI_STATUS
InitializeCommandStatus (
  IN OUT COMMAND_STATUS **ppCommandStatus
  );

/**
  Free previously allocated and initialized command status structure

  @param[in] ppCommandStatus pointer to address of the structure
**/
VOID
FreeCommandStatus(
  IN     COMMAND_STATUS **ppCommandStatus
  );

/**
  Iterate all lit NVM status codes and count warnings and errors

  @param[in] pCommandStatus pointer to address of the structure
  @param[out] pNumberOfWarnings - output address to keep no warnings
  @param[out] pNumberOfErrors - output address to keep no errors

  @retval EFI_SUCCESS All Ok
  @retval EFI_INVALID_PARAMETER if the parameter is a NULL.
**/
EFI_STATUS
CountNumberOfErrorsAndWarnings(
  IN     COMMAND_STATUS *pCommandStatus,
     OUT UINT64 *pNumberOfWarnings,
     OUT UINT64 *pNumberOfErrors
  );



/**
Erase all Nvm status codes

@param[in,out] pObjectStatus pointer to object status (with Nvm Status bit field)
**/
VOID
EraseNvmStatus(
  IN OUT OBJECT_STATUS *pObjectStatus
);

/**
Erase status for specified ID in command status list (or create a new one with no status)

@param[in, out] pCommandStatus - command status
@param[in] ObjectId - Id for specified object
@param[in] pObjectIdStr - Id for specified object as string representation, OPTIONAL
@param[in] ObjectIdStrLength - Max length of pObjectIdStr, OPTIONAL
**/
VOID
EraseObjStatus(
  IN OUT COMMAND_STATUS *pCommandStatus,
  IN     UINT32 ObjectId,
  IN     CHAR16 *pObjectIdStr OPTIONAL,
  IN     UINT32 ObjectIdStrLength OPTIONAL
);

#endif /** _NVM_STATUS_H_ **/
