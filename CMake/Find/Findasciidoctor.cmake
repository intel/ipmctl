# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Try to find asciidoctor 
# Once done this will define
# ASCIIDOCTOR_FOUND - asciidoctor found

find_program(ASCIIDOCTOR_BINARY NAMES asciidoctor
	HINTS ${ASCIIDOCTOR_BINARY_PATH})

find_program(ASCIIDOCTOR_PDF_BINARY NAMES asciidoctor-pdf
	HINTS ${ASCIIDOCTOR_PDF_BINARY_PATH})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ASCIIDOCTOR_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(asciidoctor DEFAULT_MSG
                                  ASCIIDOCTOR_BINARY)

find_package_handle_standard_args(asciidoctor-pdf DEFAULT_MSG
                                  ASCIIDOCTOR_PDF_BINARY)

mark_as_advanced(ASCIIDOCTOR_BINARY)

set(ASCIIDOCTOR_BINARIES ${ASCIIDOCTOR_BINARY})
set(ASCIIDOCTOR_PDF_BINARIES ${ASCIIDOCTOR_PDF_BINARY})

if(NOT ASCIIDOCTOR_FOUND)
	MESSAGE("No documentation will be generated.")
endif()

if(NOT ASCIIDOCTOR-PDF_FOUND)
	MESSAGE("asciidoctor-pdf not found")
endif()
