# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

#----------------------------------------------------------------------------------------------------
# Project wide defines and flags
#----------------------------------------------------------------------------------------------------


#----------------------------------------------------------------------------------------------------
# Create Safe String Library
#----------------------------------------------------------------------------------------------------
    # safe c library requires the compiler path with double quotes. Also, doesn't accept few of the C compiler flags
	set(CMAKE_C_FLAGS_SAFEC "-DNO_MSABI_VA_FUNCS -fPIC")

    include(ExternalProject)
	find_package(Git REQUIRED)

	ExternalProject_Add(safeclib_proj
		PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/safeclib
		DOWNLOAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/safeclib
		GIT_REPOSITORY https://github.com/rurban/safeclib
		GIT_TAG 59eba324c20c07f7ca8190238dd415525f4925dc
		UPDATE_COMMAND ""
		CONFIGURE_COMMAND ./build-tools/autogen.sh && ./configure  CFLAGS=${CMAKE_C_FLAGS_SAFEC} --enable-strmax=0x8000 --enable-shared=no  --disable-doc
		BUILD_COMMAND make
		BUILD_IN_SOURCE ON
		STEP_TARGETS build
		BUILD_ALWAYS OFF
		BUILD_BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/safeclib/src/safeclib_proj/src/.libs/libsafec-3.3.a
		INSTALL_COMMAND ""
		LOG_DOWNLOAD ON
		LOG_UPDATE ON
		LOG_BUILD ON
	)

	ExternalProject_Get_property(safeclib_proj SOURCE_DIR)
	add_library(safeclib STATIC IMPORTED)
	set_property(TARGET safeclib PROPERTY IMPORTED_LOCATION ${SOURCE_DIR}/src/.libs/libsafec-3.3.a)
	add_dependencies(safeclib safeclib_proj)
	set(SAFECLIB_COMPILE_FLAGS "-DHAVE_C99")

#----------------------------------------------------------------------------------------------------
# OS driver interface library
#----------------------------------------------------------------------------------------------------

target_link_libraries(ipmctl_os_interface
	safeclib
	)
target_include_directories(ipmctl_os_interface
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/safeclib/src/safeclib_proj/include)

set_source_files_properties(${OS_INTERFACE_SOURCE_FILES}
	PROPERTIES COMPILE_FLAGS ${SAFECLIB_COMPILE_FLAGS})

#----------------------------------------------------------------------------------------------------
# libipmctl
#----------------------------------------------------------------------------------------------------
target_link_libraries(ipmctl
	safeclib
	)

target_include_directories(ipmctl
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/safeclib/src/safeclib_proj/include)

set_source_files_properties(${LIBIPMCTL_SOURCE_FILES}
	PROPERTIES COMPILE_FLAGS ${SAFECLIB_COMPILE_FLAGS})

#---------------------------------------------------------------------------------------------------
# ipmctl executable
#---------------------------------------------------------------------------------------------------
set_source_files_properties(${IPMCTL_SOURCE_FILES}
	PROPERTIES COMPILE_FLAGS ${SAFECLIB_COMPILE_FLAGS})

#---------------------------------------------------------------------------------------------------
# Monitor service executable
#---------------------------------------------------------------------------------------------------
set_source_files_properties(${IPMCTL_MONITOR_SOURCE_FILES}
	PROPERTIES COMPILE_FLAGS ${SAFECLIB_COMPILE_FLAGS})
