/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVM_LIB_API_EXPORT_H_
#define	_NVM_LIB_API_EXPORT_H_

/*
* Macros for controlling what is exported by the library
*/
#ifdef _MSC_VER // Windows
#define	NVM_API_DLL_IMPORT __declspec(dllimport)
#define	NVM_API_DLL_EXPORT __declspec(dllexport)
#else // Linux/ESX
#define	NVM_API_DLL_IMPORT __attribute__((visibility("default")))
#define	NVM_API_DLL_EXPORT __attribute__((visibility("default")))
#endif // end Linux/ESX

// NVM_API is used for the public API symbols.
// NVM_LOCAL is used for non-api symbols.
#ifdef	__NVM_DLL__ // defined if compiled as a DLL
#ifdef	__NVM_API_DLL_EXPORTS__ // defined if we are building the DLL (instead of using it)
#define	NVM_API NVM_API_DLL_EXPORT
#else
#define	NVM_API NVM_API_DLL_IMPORT
#endif // NVM_DLL_EXPORTS
#else // NVM_DLL is not defined, everything is exported
#define	NVM_API
#endif // NVM_DLL

#endif // _NVM_LIB_API_EXPORT_H_
