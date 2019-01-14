/*
 * Copyright (c) 2018, Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NVM_LIMITS_H_
#define _NVM_LIMITS_H_


#define MAX_SOCKETS           8
#define MAX_IMCS_PER_SOCKET   2
#define MAX_CHANNELS_PER_IMC  3
#define MAX_DIMMS_PER_CHANNEL 2
#define MAX_DIMMS_PER_IMC     (MAX_CHANNELS_PER_IMC * MAX_DIMMS_PER_CHANNEL)
#define MAX_DIMMS_PER_SOCKET  (MAX_DIMMS_PER_IMC * MAX_IMCS_PER_SOCKET)

/**
  MAX_DIMMS = MAX_SOCKETS * MAX_DIMMS_PER_SOCKET, but we have to use a pure number in this case, because there is
  a compilation issue while HII is generating (HII uses MAX_DIMMS)
**/
#define MAX_DIMMS             96
#define MAX_IS_PER_DIMM       2
#define MAX_IS_PER_SOCKET     (MAX_DIMMS_PER_SOCKET * MAX_IS_PER_DIMM)
/**
  MAX_IS_CONFIGS = MAX_DIMMS * MAX_IS_PER_DIMM, but we have to use a pure number in this case,
  because there is a compilation issue while HII is generating
**/
#define MAX_IS_CONFIGS        192

/**
  MAX_REGIONS = MAX_SOCKETS * MAX_REGIONS_PER_SOCKET, but we have to use a pure number in this case,
  because there is a compilation issue while HII is generating
**/

#define MAX_REGIONS             44

/**
  Cache line size in bytes. Used by all of the platforms supporting Intel DCPMMs
**/
#define CACHE_LINE_SIZE 64

/**
  Label Storage Area structure information
**/
#define NAMESPACE_INDEXES         2

#define MAX_NAMESPACE_RANGES      64

#define NAMESPACE_LABEL_INDEX_ALIGN     256
#define MIN_NAMESPACE_LABEL_INDEX_SIZE  256

#define TEMPERATURE_THRESHOLD_MIN 0
#define TEMPERATURE_CONTROLLER_THRESHOLD_MAX 2047  //!< max is (2^15)32767 / 16  = 2047
#define TEMPERATURE_MEDIA_THRESHOLD_MAX 2047       //!< max is (2^15)32767 / 16  = 2047
#define TEMPERATURE_MEDIA_THRESHOLD_DEFAULT 82
#define TEMPERATURE_CONTROLLER_THRESHOLD_DEFAULT 98
#define CAPACITY_THRESHOLD_MIN     1
#define CAPACITY_THRESHOLD_MAX     99
#define CAPACITY_THRESHOLD_DEFAULT 50

#define ENABLED_STATE_DISABLE 0
#define ENABLED_STATE_ENABLE  1

#define PASSPHRASE_BUFFER_SIZE 32     //!< Length of a passphrase buffer

#define MAX_OVERWRITE_PASS_COUNT  15  //!< Overwrite DIMM max pass count

#define MAX_IFC_NUM  2

#define MAX_APPDIRECT_SETTINGS_SUPPORTED 25
#endif /** _NVM_LIMITS_H_ **/
