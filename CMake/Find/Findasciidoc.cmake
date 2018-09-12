# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Try to find asciidoc
# Once done this will define
# ASCIIDOC_FOUND - asciidoc found

find_program(ASCIIDOC_BINARY NAMES asciidoc
	HINTS ${ASCIIDOC_BINARY_PATH})

find_program(A2X_BINARY NAMES a2x
	HINTS ${A2X_BINARY_PATH})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ASCIIDOC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(asciidoc DEFAULT_MSG
                                  ASCIIDOC_BINARY)

find_package_handle_standard_args(a2x DEFAULT_MSG
                                  A2X_BINARY)

mark_as_advanced(ASCIIDOC_BINARY)
mark_as_advanced(A2X_BINARY)

set(ASCIIDOC_BINARIES ${ASCIIDOC_BINARY})
set(A2X_BINARIES ${A2X_BINARY})

if((NOT ASCIIDOC_FOUND) AND (NOT A2X_FOUND) AND (NOT ASCIIDOCTOR_FOUND))
	MESSAGE("Manpages will not be generated")
endif()
