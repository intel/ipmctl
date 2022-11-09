# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Try to find a2x
# Once done this will define
# A2X_FOUND - a2x found

find_program(A2X_BINARY NAMES a2x
	HINTS ${A2X_BINARY_PATH})

find_package_handle_standard_args(a2x DEFAULT_MSG
                                  A2X_BINARY)
mark_as_advanced(A2X_BINARY)