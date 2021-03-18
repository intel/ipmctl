# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Try to find asciidoc
# Once done this will define
# ASCIIDOC_FOUND - asciidoc found

find_program(ASCIIDOC_BINARY NAMES asciidoc
	HINTS ${ASCIIDOC_BINARY_PATH})

find_package_handle_standard_args(asciidoc DEFAULT_MSG
                                  ASCIIDOC_BINARY)

mark_as_advanced(ASCIIDOC_BINARY)