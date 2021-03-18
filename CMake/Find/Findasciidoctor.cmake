# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# Try to find asciidoctor
# Once done this will define
# ASCIIDOCTOR_FOUND - asciidoctor found

find_program(ASCIIDOCTOR_BINARY NAMES asciidoctor
	HINTS ${ASCIIDOCTOR_BINARY_PATH})

# Get the version of asciidoctor
execute_process( COMMAND ${ASCIIDOCTOR_BINARY} --version OUTPUT_VARIABLE stdout_str )
separate_arguments(stdout_str)
if (NOT "" STREQUAL "${stdout_str}")
  # Only parse stdout_str if found
  # Example output: "Asciidoctor 2.0.10 [https://asciidoctor.org]"
  # The argument at position 1 is the version
  list(GET stdout_str 1 ASCIIDOCTOR_VERSION)
endif()

find_package_handle_standard_args(asciidoctor
                                  REQUIRED_VARS ASCIIDOCTOR_BINARY
                                  VERSION_VAR ASCIIDOCTOR_VERSION
                                  )

mark_as_advanced(ASCIIDOCTOR_BINARY)