/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _COMPONENT_NAME_H_
#define _COMPONENT_NAME_H_

/**
  Retrieves a Unicode string that is the user-readable name of the EFI Driver.

  @param  pThis       A pointer to the EFI_COMPONENT_NAME_PROTOCOL2 instance.
  @param  pLanguage     A pointer to an ASCII string containing the ISO 639-2 or the
   RFC 4646 language code. This is the language of the driver name that that the caller
   is requesting, and it must match one of the languages specified
   in SupportedLanguages.  The number of languages supported by a
   driver is up to the driver writer.
  @param  ppDriverName A pointer to the Unicode string to return.  This Unicode string
   is the name of the driver specified by pThis in the language
   specified by Language.

  @retval EFI_SUCCESS           The Unicode string for the Driver specified by pThis
   and the language specified by pLanguage was returned
   in ppDriverName.
  @retval EFI_INVALID_PARAMETER pLanguage is NULL.
  @retval EFI_INVALID_PARAMETER ppDriverName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by pThis does not support the
   language specified by pLanguage.

**/
EFI_STATUS
EFIAPI
NvmDimmDriverComponentNameGetDriverName (
  IN EFI_COMPONENT_NAME2_PROTOCOL *pThis,
  IN CHAR8 *pLanguage,
  OUT CHAR16 **ppDriverName
);

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by an EFI Driver.

  @param  pThis             A pointer to the EFI_COMPONENT_NAME2_PROTOCOL instance.
  @param  ControllerHandle The handle of a controller that the driver specified by
   This is managing.  This handle specifies the controller
   whose name is to be returned.
  @param  ChildHandle      The handle of the child controller to retrieve the name
   of.  This is an optional parameter that may be NULL.  It
   will be NULL for device drivers.  It will also be NULL
   for a bus drivers that wish to retrieve the name of the
   bus controller.  It will not be NULL for a bus driver
   that wishes to retrieve the name of a child controller.
  @param  pLanguage         A pointer to a three character ISO 639-2 language
   identifier.  This is the language of the controller name
   that the caller is requesting, and it must match one
   of the languages specified in SupportedLanguages.  The
   number of languages supported by a driver is up to the
   driver writer.
  @param  ppControllerName   A pointer to the Unicode string to return.  This Unicode
   string is the name of the controller specified by
   ControllerHandle and ChildHandle in the language specified
   by Language, from the point of view of the driver specified by This.

  @retval EFI_SUCCESS           The Unicode string for the user-readable name in the
   language specified by Language for the driver specified by This was returned in DriverName.
  @retval EFI_INVALID_PARAMETER ControllerHandle is NULL.
  @retval EFI_INVALID_PARAMETER ChildHandle is not NULL and it is not a valid EFI_HANDLE.
  @retval EFI_INVALID_PARAMETER Language is NULL.
  @retval EFI_INVALID_PARAMETER ControllerName is NULL.
  @retval EFI_UNSUPPORTED       The driver specified by This is not currently managing
   the controller specified by ControllerHandle and ChildHandle.
  @retval EFI_UNSUPPORTED       The driver specified by This does not support the
   language specified by Language.
**/
EFI_STATUS
EFIAPI
NvmDimmDriverComponentNameGetControllerName (
  IN EFI_COMPONENT_NAME2_PROTOCOL *pThis,
  IN EFI_HANDLE ControllerHandle,
  IN EFI_HANDLE ChildHandle OPTIONAL,
  IN CHAR8 *pLanguage,
  OUT CHAR16 **ppControllerName
);

#endif /* _COMPONENT_NAME_H_ */
