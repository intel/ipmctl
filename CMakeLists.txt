# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 2.8.12)

project(ipmctl)

set(CMAKE_VERBOSE_MAKEFILE on)

get_filename_component(ROOT ./ ABSOLUTE)

message(BUILD_TYPE: ${CMAKE_BUILD_TYPE})
set(LIBIPMCTL_VERSION_MAJOR 2)
set(LIBIPMCTL_VERSION_MINOR 0)
set(LIBIPMCTL_VERSION_PATCH 0)
set(LIBIPMCTL_VERSION_STRING ${LIBIPMCTL_VERSION_MAJOR}.${LIBIPMCTL_VERSION_MINOR}.${LIBIPMCTL_VERSION_PATCH})

message(VERSION: ${BUILDNUM})


if(NOT BUILDNUM)
	set(BUILDNUM "5.5.5.5555")
	message(BUILDNUM: ${BUILDNUM})
endif()

if(RELEASE)
	set(BUILD_TYPE release)
	set(CMAKE_BUILD_TYPE Release)
	SET(CMAKE_SKIP_BUILD_RPATH TRUE)
else()
	set(BUILD_TYPE debug)
	set(CMAKE_BUILD_TYPE Debug)
	add_definitions(
		-DDEBUG_BUILD
    -DPLAYBACK_RECORD_SUPPORTED
		)
endif()

if(BUILD_STATIC)
	set(LIB_TYPE STATIC)
else()
	set(LIB_TYPE SHARED)
endif()

if(ESX_BUILD)
	set(OS_TYPE esx)
	set(FILE_PREFIX esx)
elseif(UNIX)
	set(LNX_BUILD 1)
	set(OS_TYPE linux)
	set(FILE_PREFIX lnx)

	find_package(Threads REQUIRED)
	find_package(PkgConfig)
	pkg_check_modules(NDCTL REQUIRED libndctl>=58.2)
	pkg_check_modules(SYSTEMD systemd)
	pkg_check_modules(LIBSAFEC REQUIRED safec-3.3>=03032018.0-g570fa5)
elseif(MSVC)
	set(WIN_BUILD 1)
	set(OS_TYPE win)
	set(FILE_PREFIX win)
endif()

message(BUILD_TYPE: ${CMAKE_BUILD_TYPE})
message(OS_TYPE: ${OS_TYPE})
message(LIB_TYPE: ${LIB_TYPE})
message(COMPILER: ${CMAKE_C_COMPILER})
message(SYSROOT: ${MY_CMAKE_SYSROOT})
message(TOOLCHAIN: ${CMAKE_TOOLCHAIN_FILE})

set(OUTPUT_DIR ${ROOT}/output/${BUILD_TYPE})
file(MAKE_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ROOT}/output/${BUILD_TYPE})
set(CMAKE_BINARY_DIR ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(INTERNAL_MFG)
        include(CMake/mfg.cmake)
endif()

if(EXISTS "${ROOT}/CMake/internal.cmake")
        include("${ROOT}/CMake/internal.cmake")
endif()

#----------------------------------------------------------------------------------------------------
# Project wide defines and flags
#----------------------------------------------------------------------------------------------------
add_definitions(
	-DOS_BUILD
	-D__VERSION_NUMBER__=${BUILDNUM}
	)

if(MSVC)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
	set(CMAKE_C_FLAGS_DEBUG_ "${CMAKE_C_FLAGS_DEBUG} /Od")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
else()
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -fno-strict-aliasing -D_FORTIFY_SOURCE=2")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fno-strict-aliasing -D_FORTIFY_SOURCE=2")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")
endif()

if(UNIX)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_MSABI_VA_FUNCS -std=c99 -Wformat -Wformat-security -D_XOPEN_SOURCE=500 -Wall -Wfatal-errors -MMD -fPIC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wformat -Wformat-security -D_XOPEN_SOURCE=500 -Drestrict=__restrict__ -Wall -Wfatal-errors -MMD -fPIC")
	set(CMAKE_SHARED_LINKER_FLAGS "-z noexecstack -z relro -z now -pie")
elseif(MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS /DynamicBase /sdl")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS /DynamicBase /sdl")
	set(CMAKE_SHARED_LINKER_FLAGS "/NXCompat")
endif()

if(LNX_BUILD)
	add_definitions(
		-D_GNU_SOURCE
		-D__LINUX__
		-DHAVE_C99
		)
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.9)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
	endif()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
	endif()
endif()

#----------------------------------------------------------------------------------------------------
# OS driver interface library
#----------------------------------------------------------------------------------------------------

if(MSVC)
	FILE(GLOB OS_INTERFACE_SOURCE_FILES
		src/os/os_common.c
		src/os/win/win_scm2_ioctl.c
		src/os/win/win_scm2_passthrough.c
		src/os/win/win_scm2_ioctl_passthrough.c
		src/os/win/win_common.c
		src/os/win/win_api.c
		src/os/win/win_adapter_acpi_events.c
		src/os/win/win_scm2_adapter.c
		src/os/win/win_system.c
		)
elseif(UNIX)
	FILE(GLOB OS_INTERFACE_SOURCE_FILES
		src/os/os_common.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_adapter_passthrough.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_acpi.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_common.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_api.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_adapter.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_adapter_acpi_events.c
		src/os/${OS_TYPE}/${FILE_PREFIX}_system.c
		)
endif()

add_library(ipmctl_os_interface STATIC ${OS_INTERFACE_SOURCE_FILES})

target_link_libraries(ipmctl_os_interface
	${CMAKE_THREAD_LIBS_INIT}
	)

if(LNX_BUILD)
	target_link_libraries(ipmctl_os_interface
		${LIBSAFEC_LIBRARIES}
		)
endif()

target_include_directories(ipmctl_os_interface PUBLIC
	src/os
	src/os/${OS_TYPE}
	DcpmemPkg/common
	src/os/nvm_api
	src/os/s_string
	${LIBSAFEC_INCLUDE_DIRS}
	)
# TODO: Remove the backup winioctl.h file when the 17650+ Windows SDK is publicly released
if (MSVC)
	string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+\\.[0-9]+" "\\1" sdk_version_major $ENV{WindowsSDKVersion})
	string(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" sdk_version_major2 $ENV{WindowsSDKVersion})
	string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1" sdk_version_minor $ENV{WindowsSDKVersion})
	if (${sdk_version_major} GREATER 9 AND ${sdk_version_minor} GREATER 17650)
		MESSAGE("Using Windows SDK 17650+")
	else()
		MESSAGE("Using internal winioctl.h")
		target_include_directories(ipmctl_os_interface PUBLIC
			src/os/win/sdk
		)
	endif()
endif()

#----------------------------------------------------------------------------------------------------
# libipmctl
#----------------------------------------------------------------------------------------------------
file(GLOB LIBIPMCTL_SOURCE_FILES
	src/os/efi_shim/AutoGen.c
	src/os/efi_shim/os_efi_api.c
	src/os/efi_shim/os_efi_preferences.c
	src/os/efi_shim/os_efi_shell_parameters_protocol.c
	src/os/efi_shim/os_efi_simple_file_protocol.c
	src/os/efi_shim/os_efi_bs_protocol.c
	src/os/ini/ini.c
	src/os/eventlog/event.c
	src/os/nvm_api/nvm_management.c
	src/os/nvm_api/nvm_output_parsing.c
	src/os/s_string/s_str.c
	DcpmemPkg/cli/NvmDimmCli.c
	DcpmemPkg/cli/CommandParser.c
	DcpmemPkg/cli/Common.c
	DcpmemPkg/cli/ShowAcpiCommand.c
	DcpmemPkg/cli/ShowSensorCommand.c
	DcpmemPkg/cli/ShowDimmsCommand.c
	DcpmemPkg/cli/ShowSocketsCommand.c
	DcpmemPkg/cli/ShowMemoryResourcesCommand.c
	DcpmemPkg/cli/ShowSystemCapabilitiesCommand.c
	DcpmemPkg/cli/ShowFirmwareCommand.c
	DcpmemPkg/cli/ShowPcdCommand.c
	DcpmemPkg/cli/DeletePcdCommand.c
	DcpmemPkg/cli/ShowRegionsCommand.c
	DcpmemPkg/cli/CreateGoalCommand.c
	DcpmemPkg/cli/ShowGoalCommand.c
	DcpmemPkg/cli/DeleteGoalCommand.c
	DcpmemPkg/cli/ShowErrorCommand.c
	DcpmemPkg/cli/DumpDebugCommand.c
	DcpmemPkg/cli/StartDiagnosticCommand.c
	DcpmemPkg/cli/ShowPreferencesCommand.c
	DcpmemPkg/cli/ShowTopologyCommand.c
	DcpmemPkg/cli/SetPreferencesCommand.c
	DcpmemPkg/cli/SetSensorCommand.c
	DcpmemPkg/cli/LoadGoalCommand.c
	DcpmemPkg/cli/DumpGoalCommand.c
	DcpmemPkg/cli/SetDimmCommand.c
	DcpmemPkg/cli/LoadCommand.c
	DcpmemPkg/cli/DeleteDimmCommand.c
	src/os/cli_cmds/DumpSupportCommand.c
	DcpmemPkg/cli/LoadRecoverCommand.c
	DcpmemPkg/cli/ShowRegisterCommand.c
	DcpmemPkg/cli/StartFormatCommand.c
	DcpmemPkg/cli/ShowEventCommand.c
	DcpmemPkg/cli/SetEventCommand.c
	DcpmemPkg/cli/ShowPerformanceCommand.c
	DcpmemPkg/cli/ShowHostServerCommand.c
	DcpmemPkg/cli/ShowCmdAccessPolicyCommand.c
	DcpmemPkg/common/FwUtility.c
	DcpmemPkg/common/Utility.c
	DcpmemPkg/common/NvmTables.c
	DcpmemPkg/common/ShowAcpi.c
	DcpmemPkg/common/NvmStatus.c
	DcpmemPkg/common/NvmHealth.c
	DcpmemPkg/common/LbaCommon.c
	DcpmemPkg/common/Convert.c
	DcpmemPkg/common/PcdCommon.c
	DcpmemPkg/common/OsCommon.c
	DcpmemPkg/common/DataSet.c
	DcpmemPkg/common/Show.c
	DcpmemPkg/driver/Protocol/Driver/NvmDimmConfig.c
	DcpmemPkg/driver/NvmDimmDriver.c
	DcpmemPkg/driver/Core/Dimm.c
	DcpmemPkg/driver/Core/Namespace.c
	DcpmemPkg/driver/Core/NvmSecurity.c
	DcpmemPkg/driver/Core/Region.c
	DcpmemPkg/driver/Core/Btt.c
	DcpmemPkg/driver/Core/Pfn.c
	DcpmemPkg/driver/Core/Diagnostics/ConfigDiagnostic.c
	DcpmemPkg/driver/Core/Diagnostics/CoreDiagnostics.c
	DcpmemPkg/driver/Core/Diagnostics/FwDiagnostic.c
	DcpmemPkg/driver/Core/Diagnostics/QuickDiagnostic.c
	DcpmemPkg/driver/Core/Diagnostics/SecurityDiagnostic.c
	DcpmemPkg/driver/Protocol/Device/NvmFirmwareManagement.c
	DcpmemPkg/driver/Protocol/Device/StorageSecurityCommand.c
	DcpmemPkg/driver/Protocol/Namespace/NvmDimmBlockIo.c
	DcpmemPkg/driver/Utils/PlatformConfigData.c
	DcpmemPkg/driver/Utils/AcpiParsing.c
	DcpmemPkg/driver/Utils/Interleave.c
	DcpmemPkg/driver/Utils/SmbiosUtility.c
	DcpmemPkg/driver/Utils/DumpLoadPools.c
	MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.c
	MdePkg/Library/UefiDevicePathLib/DevicePathUtilities.c
	MdePkg/Library/UefiDevicePathLib/DevicePathToText.c
	MdePkg/Library/UefiDevicePathLib/DevicePathFromText.c
	MdePkg/Library/BaseLib/Math64.c
	MdePkg/Library/BaseLib/MultU64x32.c
	MdePkg/Library/BaseLib/DivU64x32Remainder.c
	MdePkg/Library/BaseLib/Unaligned.c
	MdePkg/Library/BaseLib/BitField.c
	MdePkg/Library/BaseLib/LinkedList.c
	MdePkg/Library/BaseLib/LShiftU64.c
	MdePkg/Library/BaseLib/Math64.c
	MdePkg/Library/BaseLib/SwapBytes64.c
	MdePkg/Library/BaseLib/SwapBytes32.c
	MdePkg/Library/BaseLib/SwapBytes16.c
	MdePkg/Library/BaseLib/String.c
	MdePkg/Library/BaseLib/SafeString.c
	MdePkg/Library/BaseLib/RShiftU64.c
	MdePkg/Library/BaseLib/Math64.c
	MdePkg/Library/BaseLib/DivU64x32.c
	${LIBIPMCTL_MFG_FILES}
	)

# OS Files
list(APPEND LIBIPMCTL_SOURCE_FILES
	src/os/efi_shim/${FILE_PREFIX}_efi_api.c
	)

if(NOT MSVC)
	list(APPEND LIBIPMCTL_SOURCE_FILES
	MdePkg/Library/UefiMemoryAllocationLib/AllocTracer.c
	)
endif()

add_library(ipmctl ${LIB_TYPE} ${LIBIPMCTL_SOURCE_FILES})

target_include_directories(ipmctl PUBLIC
	DcpmemPkg/cli
	src/os
	src/os/efi_shim
	src/os/ini
	src/os/eventlog
	src/os/nvm_api
	src/os/s_string
	src/os/cli_cmds
	DcpmemPkg/common
	DcpmemPkg/driver
	DcpmemPkg/driver/Utils
	DcpmemPkg/driver/Protocol/Driver
	DcpmemPkg/driver/Core
	DcpmemPkg/driver/Protocol/Namespace
	DcpmemPkg/driver/Protocol/Device
	DcpmemPkg/driver/Core/Diagnostics
	MdePkg
	MdePkg/Include
	MdePkg/Include/Protocol
	MdePkg/Include/Uefi
	MdePkg/Include/Library
	ShellPkg
	ShellPkg/Include
	ShellPkg/Include/Protocol
	ShellPkg/Include/Library
	ShellPkg/Library
	MdeModulePkg
	MdeModulePkg/Include
	MdeModulePkg/Include/Library
	${LIBSAFEC_INCLUDE_DIRS}
        )

if(MSVC)
	set_target_properties(ipmctl PROPERTIES PREFIX "lib")
	target_compile_options(ipmctl PRIVATE /FIAutoGen.h -D__NVM_DLL__ -D__NVM_API_DLL_EXPORTS__ )
	# FIXME: Ignore warnings for secure functions
	SET_SOURCE_FILES_PROPERTIES(src/os/efi_shim/os_efi_simple_file_protocol.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/eventlog/event.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/ini/ini.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/efi_shim/os_efi_preferences.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/nvm_api/nvm_management.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(DcpmemPkg/cli/Common.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/nvm_api/nvm_output_parsing.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/efi_shim/os_efi_shell_parameters_protocol.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
	SET_SOURCE_FILES_PROPERTIES(src/os/cli_cmds/DumpSupportCommand.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(ipmctl PRIVATE
		-include AutoGen.h
		)
endif()

add_dependencies(ipmctl
	stringdefs
	iniconfig
	)

target_link_libraries(ipmctl
	ipmctl_os_interface
	)

if(LNX_BUILD)
	target_link_libraries(ipmctl
		${NDCTL_LIBRARIES}
		)
endif()

set_target_properties(ipmctl PROPERTIES
	VERSION ${LIBIPMCTL_VERSION_STRING}
	SOVERSION ${LIBIPMCTL_VERSION_MAJOR}
	)

#---------------------------------------------------------------------------------------------------
# ipmctl executable
#---------------------------------------------------------------------------------------------------
FILE(GLOB IPMCTL_SOURCE_FILES
	src/os/os_main.c
	)

add_executable(ipmctl-bin ${IPMCTL_SOURCE_FILES})

target_link_libraries(ipmctl-bin
	ipmctl
	)


target_include_directories(ipmctl-bin PUBLIC
	src/os
	src/os/nvm_api
	src/os/ini
	${LIBSAFEC_INCLUDE_DIRS}
	)

set_target_properties(ipmctl-bin
	PROPERTIES OUTPUT_NAME ipmctl
	)

if(MSVC)
	set_target_properties(ipmctl-bin PROPERTIES LINK_FLAGS "/STACK:3000000")
else()
	target_compile_options(ipmctl-bin PRIVATE "-fPIE")
endif()

#---------------------------------------------------------------------------------------------------
# Monitor service executable
#---------------------------------------------------------------------------------------------------
if (NOT ESX_BUILD)
	FILE(GLOB IPMCTL_MONITOR_SOURCE_FILES
		src/os/monitor/NvmMonitorBase.cpp
		src/os/monitor/PerformanceMonitor.cpp
		src/os/monitor/EventMonitor.cpp
		src/os/monitor/AcpiEventMonitor.cpp
		)

	if(MSVC)
		list(APPEND IPMCTL_MONITOR_SOURCE_FILES
			src/os/monitor/win_main.cpp
			src/os/monitor/win_service.cpp
			)
	else()
		list(APPEND IPMCTL_MONITOR_SOURCE_FILES
			src/os/monitor/lnx_main.cpp
			)
	endif()

	add_executable(ipmctl-monitor ${IPMCTL_MONITOR_SOURCE_FILES})

	target_link_libraries(ipmctl-monitor
		ipmctl
		${LIBSAFEC_LIBRARIES}
		)

	target_include_directories(ipmctl-monitor PUBLIC
		src/os
		src/os/nvm_api
		src/os/ini
		DcpmemPkg/common
		${LIBSAFEC_INCLUDE_DIRS}
		)

if(UNIX)
        target_compile_options(ipmctl-monitor PRIVATE "-fPIE")
endif()

endif()

#----------------------------------------------------------------------------------------------------
# Generate String Definitions
#----------------------------------------------------------------------------------------------------
set(STRING_DEFS_INPUT_FILES
	${ROOT}/DcpmemPkg/common/NvmStatus.uni
	${ROOT}/DcpmemPkg/driver/Core/Diagnostics/DiagnosticsMessages.uni
	${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings.py
	)

set(STRING_DEFS_OUTPUT_FILES
	${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings.h
	${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_defs.h
	)

add_custom_target(stringdefs ALL
	DEPENDS ${STRING_DEFS_OUTPUT_FILES} ${STRING_DEFS_INPUT_FILES})

add_custom_command(OUTPUT ${STRING_DEFS_OUTPUT_FILES}
	COMMAND python ${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings.py
	COMMENT "Generating String Definitions"
	DEPENDS ${STRING_DEFS_INPUT_FILES}
	)

#----------------------------------------------------------------------------------------------------
# Generate INI Default Config File
#----------------------------------------------------------------------------------------------------

add_custom_target(iniconfig ALL
	DEPENDS ${OUTPUT_DIR}/ipmctl_default.conf
	)

if(MSVC)
	set(PRECOMPILER_FLAG /EP)
else()
	set(PRECOMPILER_FLAG -E -P -D__LINUX__)
endif()

add_custom_command(OUTPUT ${OUTPUT_DIR}/ipmctl_default.conf
	COMMAND ${CMAKE_C_COMPILER} ${MY_CMAKE_SYSROOT} ${PRECOMPILER_FLAG} ${ROOT}/src/os/ini/ipmctl_default.c > ${ROOT}/src/os/ini/ipmctl_default.i
	COMMAND python ${ROOT}/src/os/ini/ini_auto_gen_default_config.py ${ROOT}/src/os/ini/ipmctl_default.i ${OUTPUT_DIR}/ipmctl_default.conf
	COMMAND ${CMAKE_COMMAND} -E remove -f ${ROOT}/src/os/ini/ipmctl_default.i
	COMMENT "Generating INI Default Config File"
	)

#----------------------------------------------------------------------------------------------------
# Install
#----------------------------------------------------------------------------------------------------
FILE(GLOB NVM_HEADERS
	src/os/nvm_api/export_api.h
	src/os/nvm_api/nvm_management.h
	src/os/nvm_api/nvm_types.h
	DcpmemPkg/common/NvmStatusValues.h
	)

FILE(COPY ${NVM_HEADERS}
       DESTINATION ${OUTPUT_DIR})

if(LNX_BUILD)
	include(GNUInstallDirs)
	find_package(PkgConfig)

	pkg_check_modules(SYSTEMD systemd)

	configure_file(${ROOT}/install/linux/libipmctl.pc.in ${OUTPUT_DIR}/libipmctl.pc @ONLY)

	if(DEFINED INSTALL_UNITDIR)
		set(INSTALL_SYSTEMDSYSTEMUNITDIR ${INSTALL_UNITDIR})
	else()
		if(SYSTEMD_FOUND)
			if(CMAKE_VERSION VERSION_LESS 3.4.3)
				set(INSTALL_SYSTEMDSYSTEMUNITDIR "/usr/lib/systemd/system")
			else()
				pkg_get_variable(INSTALL_SYSTEMDSYSTEMUNITDIR systemd systemdsystemunitdir)
			endif()
		endif()
	endif()

	if(BUILD_STATIC)
		install(TARGETS ipmctl-bin ipmctl-monitor
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			)
	else()
		install(TARGETS ipmctl-bin ipmctl ipmctl-monitor
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			)
	endif()

	install(FILES ${OUTPUT_DIR}/ipmctl_default.conf
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ipmctl
		)

	install(FILES ${OUTPUT_DIR}/ipmctl_default.conf
		DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
		RENAME ipmctl.conf
		)

	install(FILES ${NVM_HEADERS}
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		)

	file(MAKE_DIRECTORY output/ipmctl)

	install(DIRECTORY output/ipmctl
		DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/log
		)

	if(DEFINED INSTALL_SYSTEMDSYSTEMUNITDIR)
		install (FILES src/os/monitor/ipmctl-monitor.service
			DESTINATION ${INSTALL_SYSTEMDSYSTEMUNITDIR}
			)
	endif()

	install(FILES ${OUTPUT_DIR}/libipmctl.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

#----------------------------------------------------------------------------------------------------
# Documentation
#----------------------------------------------------------------------------------------------------
# add a target to generate API documentation with Doxygen^M
find_package(Doxygen)


if(DOXYGEN_FOUND)

#----------------------------------------------------------------------------------------------------
# Generate NVM C API documentation with Doxygen
#----------------------------------------------------------------------------------------------------
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/os/docs/Doxyfile_api.in ${ROOT}/output/${BUILD_TYPE}/Doxyfile @ONLY)
	add_custom_target(doc-api
		${DOXYGEN_EXECUTABLE} ${ROOT}/output/${BUILD_TYPE}/Doxyfile
		COMMAND make -C ${ROOT}/output/${BUILD_TYPE}/docs/nvm_api/latex pdf
		WORKING_DIRECTORY ${ROOT}/output/${BUILD_TYPE}
		COMMENT "Generating API documentation with Doxygen" VERBATIM
		)
endif()

# --------------------------------------------------------------------------------------------------
# Additional includes
# --------------------------------------------------------------------------------------------------
if(LNX_BUILD AND UNIT_TEST)
	include(CMake/unit_test.cmake)
endif()

if(ESX_BUILD)
	include(CMake/esx.cmake)
endif()
