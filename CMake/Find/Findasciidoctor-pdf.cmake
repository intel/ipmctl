# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Try to find asciidoctor-pdf
# Once done this will define
# ASCIIDOCTOR-PDF_FOUND - asciidoctor-pdf found


find_program(ASCIIDOCTOR_PDF_BINARY NAMES asciidoctor-pdf
	HINTS ${ASCIIDOCTOR_PDF_BINARY_PATH})

# Get the version of asciidoctor-pdf
execute_process( COMMAND ${ASCIIDOCTOR_PDF_BINARY} --version OUTPUT_VARIABLE stdout_str )
separate_arguments(stdout_str)
if (NOT "" STREQUAL "${stdout_str}")
  # Only parse stdout_str if found
  # Example output: "Asciidoctor PDF 1.5.3 using Asciidoctor 2.0.10 [https://asciidoctor.org]"
  # The argument at position 2 is the version
  list(GET stdout_str 2 ASCIIDOCTOR_PDF_VERSION)
endif()

find_package_handle_standard_args(asciidoctor-pdf
                                  REQUIRED_VARS ASCIIDOCTOR_PDF_BINARY
                                  VERSION_VAR ASCIIDOCTOR_PDF_VERSION
                                  )

mark_as_advanced(ASCIIDOCTOR_PDF_BINARY)