# Copyright (c) 2018, Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 2.8.12)

message("cmake version: " ${CMAKE_VERSION})

project(ipmctl)

# Enable tests that may be contained in the below subdirectories
enable_testing()

set(CMAKE_VERBOSE_MAKEFILE on)

get_filename_component(ROOT ./ ABSOLUTE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake/Find")

set(LIBIPMCTL_VERSION_MAJOR 5)
set(LIBIPMCTL_VERSION_MINOR 2)
set(LIBIPMCTL_VERSION_PATCH 0)
set(LIBIPMCTL_VERSION_STRING ${LIBIPMCTL_VERSION_MAJOR}.${LIBIPMCTL_VERSION_MINOR}.${LIBIPMCTL_VERSION_PATCH})

set(IPMCTL_VERSION_MAJOR 03)
set(IPMCTL_VERSION_MINOR 01)
set(IPMCTL_VERSION_PATCH 00)

set(IPMCTL_SPEC_VERSION_MAJOR 3)
set(IPMCTL_SPEC_VERSION_MINOR 02)
set(IPMCTL_DRAFT "")
set(IPMCTL_SPEC_VERSION_STRING ${IPMCTL_SPEC_VERSION_MAJOR}.${IPMCTL_SPEC_VERSION_MINOR}${IPMCTL_DRAFT})

if(BUILDNUM)
  set(IPMCTL_VERSION_STRING ${BUILDNUM})
else()
  find_package(Git)
  if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tag --dirty
      OUTPUT_VARIABLE IPMCTL_VERSION_STRING
      RESULT_VARIABLE GIT_RETURN_CODE
      )
    # strip leading string in git tag
    string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" IPMCTL_VERSION_STRING "${IPMCTL_VERSION_STRING}")
    # strip ending newline
    string(REGEX REPLACE "\n" "" IPMCTL_VERSION_STRING "${IPMCTL_VERSION_STRING}")
  endif()

  if(NOT GIT_FOUND OR ${GIT_RETURN_CODE})
    set(IPMCTL_VERSION_STRING ${IPMCTL_VERSION_MAJOR}.${IPMCTL_VERSION_MINOR}.${IPMCTL_VERSION_PATCH}.0345)
  endif()
endif()

message(VERSION: ${IPMCTL_VERSION_STRING})

if(RELEASE OR CMAKE_BUILD_TYPE STREQUAL "Release")
  set(BUILD_TYPE release)
  set(CMAKE_BUILD_TYPE Release)
  SET(CMAKE_SKIP_BUILD_RPATH TRUE)
else()
  set(BUILD_TYPE debug)
  set(CMAKE_BUILD_TYPE Debug)
  add_definitions(
    -DDEBUG_BUILD
  )
endif()

message(CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE})

if(BUILD_STATIC)
  set(LIB_TYPE STATIC)
else()
  set(LIB_TYPE SHARED)
endif()

find_package(PythonInterp REQUIRED)

if(ESX_BUILD)
  set(OS_TYPE esx)
  set(FILE_PREFIX esx)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH "/opt/intel/bin")

else()
  ## Common packages between Windows and Linux

  # Optional packages for documentation
  ## Unfortunately it sounds like we can't do conditional inclusion
  ## based on the target selected.
  find_package(a2x)
  find_package(asciidoctor)
  find_package(asciidoc)

  if(UNIX)
    set(LNX_BUILD 1)
    set(OS_TYPE linux)
    set(FILE_PREFIX lnx)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NDCTL REQUIRED libndctl>=58.2)
    pkg_check_modules(SYSTEMD systemd)
    find_package(Threads REQUIRED)
  elseif(MSVC)
    set(WIN_BUILD 1)
    set(OS_TYPE win)
    set(FILE_PREFIX win)
  endif()
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

#----------------------------------------------------------------------------------------------------
# Project wide defines and flags
#----------------------------------------------------------------------------------------------------
add_definitions(
  -DOS_BUILD
  -DPCD_CACHE_ENABLED
  -D__VERSION_NUMBER__=${IPMCTL_VERSION_STRING}
  -DPLAYBACK_RECORD_SUPPORTED
  )

# Promote warnings to errors only for release builds
if(MSVC)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2 /WX")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /WX")
  set(CMAKE_C_FLAGS_DEBUG_ "${CMAKE_C_FLAGS_DEBUG} /Od")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
else()
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -fno-strict-aliasing -D_FORTIFY_SOURCE=2")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -fno-strict-aliasing -D_FORTIFY_SOURCE=2")
  if(LNX_BUILD)
    #A few warnings yet to resolve under esx
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Werror")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Werror")
  endif()
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb")
endif()

if(UNIX)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_MSABI_VA_FUNCS -std=c99 -Wformat -Wformat-security -D_XOPEN_SOURCE=500 -Wall -Wfatal-errors -MMD -fPIC -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wformat -Wformat-security -D_XOPEN_SOURCE=500 -Drestrict=__restrict__ -Wall -Wfatal-errors -MMD -fPIC -fno-strict-aliasing")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z noexecstack -z relro -z now -pie")
elseif(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS /DynamicBase /sdl")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS /DynamicBase /sdl")
  set(CMAKE_SHARED_LINKER_FLAGS "/NXCompat")
endif()

if(LNX_BUILD)
  if("${CMAKE_INSTALL_DATAROOTDIR}" STREQUAL "/usr/share")
    # Workaround for the RPM build cause the %{_datarootdir} is already prefixed
    set(INI_INSTALL_FILEPATH "${CMAKE_INSTALL_DATAROOTDIR}")
  else()
    if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/")
      # In case the CMAKE_INSTALL_PREFIX equals "/" for <dir> other than the SYSCONFDIR and LOCALSTATEDIR,
      # the value of CMAKE_INSTALL_<dir> is prefixed with usr/ if it is not user-specified as an absolute path.
      set(CMAKE_INSTALL_PREFIX "/usr")
      message("CMAKE_INSTALL_PREFIX value changed to: ${CMAKE_INSTALL_PREFIX}")
    endif()
    if(NOT DEFINED CMAKE_INSTALL_DATAROOTDIR)
      set(CMAKE_INSTALL_DATAROOTDIR "share")
      message("CMAKE_INSTALL_DATAROOTDIR not defined. Creating a new definition with value: ${CMAKE_INSTALL_DATAROOTDIR}")
    endif()
    set(INI_INSTALL_FILEPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}")
  endif()
  message("INI_INSTALL_FILEPATH definition create with value: ${INI_INSTALL_FILEPATH}")
  add_definitions(
    -D_GNU_SOURCE
    -D__LINUX__
    -DHAVE_C99
    -DINI_INSTALL_FILEPATH="${INI_INSTALL_FILEPATH}/ipmctl/"
    )
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 4.9)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
  endif()
endif()

if(MSVC)
  if(VCREDIST_STATIC)
    # Force vcredist to be linked static
    # Keep this after all other compiler flag setup
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    message("VCREDIST: STATIC")
  else()
    message("VCREDIST: SHARED")
  endif()
endif()

#----------------------------------------------------------------------------------------------------
# OS driver interface library
#----------------------------------------------------------------------------------------------------
if(MSVC)

  # setup resource file for file details
  string(REGEX REPLACE "[.]" "," IPMCTL_VERSION_COMMA_STRING "${IPMCTL_VERSION_STRING}")

  # exe
  set (NAME_RC ${CMAKE_PROJECT_NAME})
  set (NAME_RC_EXT "${NAME_RC}.exe")
  set (VFT_TYPE "VFT_APP")
  configure_file(src/os/win/ipmctl.rc.in src/os/win/ipmctl.rc @ONLY)

  # lib
  set (NAME_RC "lib${CMAKE_PROJECT_NAME}")
  set (NAME_RC_EXT "${NAME_RC}.dll")
  set (VFT_TYPE "VFT_DLL")
  configure_file(src/os/win/ipmctl.rc.in src/os/win/libipmctl.rc @ONLY)

  FILE(GLOB OS_INTERFACE_SOURCE_FILES
    src/os/os_str.c
    src/os/os_common.c
    src/os/win/win_scm2_ioctl.c
    src/os/win/win_scm2_passthrough.c
    src/os/win/win_scm2_ioctl_passthrough.c
    src/os/win/win_common.c
    src/os/win/win_api.c
    src/os/win/win_scm2_adapter.c
    src/os/win/win_system.c
    )
elseif(UNIX)
  FILE(GLOB OS_INTERFACE_SOURCE_FILES
    src/os/os_str.c
    src/os/os_common.c
    src/os/${OS_TYPE}/${FILE_PREFIX}_adapter_passthrough.c
    src/os/${OS_TYPE}/${FILE_PREFIX}_acpi.c
    src/os/${OS_TYPE}/${FILE_PREFIX}_common.c
    src/os/${OS_TYPE}/${FILE_PREFIX}_api.c
    src/os/${OS_TYPE}/${FILE_PREFIX}_adapter.c
    src/os/${OS_TYPE}/${FILE_PREFIX}_system.c
    )
endif()

add_library(ipmctl_os_interface STATIC ${OS_INTERFACE_SOURCE_FILES})

target_link_libraries(ipmctl_os_interface
  ${CMAKE_THREAD_LIBS_INIT}
  )

target_include_directories(ipmctl_os_interface PUBLIC
  src/os
  src/os/${OS_TYPE}
  DcpmPkg/common
  DcpmPkg/cli
  src/os/nvm_api
  src/os/s_string
  MdePkg/Include
  MdePkg/Include/Uefi
  src/os/efi_shim
  )

if (MSVC)
  string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+\\.[0-9]+" "\\1" sdk_version_major $ENV{WindowsSDKVersion})
  string(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" sdk_version_major2 $ENV{WindowsSDKVersion})
  string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1" sdk_version_minor $ENV{WindowsSDKVersion})
  if (${sdk_version_major} GREATER 9 AND ${sdk_version_minor} GREATER 17650)
    MESSAGE("Using Windows SDK 17650+")
  else()
    MESSAGE(FATAL_ERROR "Requires Windows SDK version 17650 or newer.")
  endif()
endif()

#----------------------------------------------------------------------------------------------------
# libipmctl
#----------------------------------------------------------------------------------------------------
file(GLOB LIBIPMCTL_SOURCE_FILES
  src/os/efi_shim/AutoGen.c
  src/os/efi_shim/AutoGenIo.c
  src/os/efi_shim/os_efi_api.c
  src/os/efi_shim/os_efi_api_io.c
  src/os/efi_shim/os_efi_preferences.c
  src/os/efi_shim/os_efi_shell_parameters_protocol.c
  src/os/efi_shim/os_efi_simple_file_protocol.c
  src/os/efi_shim/os_efi_bs_protocol.c
  src/os/ini/ini.c
  src/os/eventlog/event.c
  src/os/nvm_api/nvm_management.c
  src/os/nvm_api/nvm_output_parsing.c
  src/os/s_string/s_str.c
  DcpmPkg/cli/NvmDimmCli.c
  DcpmPkg/cli/CommandParser.c
  DcpmPkg/cli/Common.c
  DcpmPkg/cli/ShowAcpiCommand.c
  DcpmPkg/cli/ShowSensorCommand.c
  DcpmPkg/cli/ShowDimmsCommand.c
  DcpmPkg/cli/ShowSocketsCommand.c
  DcpmPkg/cli/ShowMemoryResourcesCommand.c
  DcpmPkg/cli/ShowSystemCapabilitiesCommand.c
  DcpmPkg/cli/ShowFirmwareCommand.c
  DcpmPkg/cli/ShowPcdCommand.c
  DcpmPkg/cli/DeletePcdCommand.c
  DcpmPkg/cli/ShowRegionsCommand.c
  DcpmPkg/cli/CreateGoalCommand.c
  DcpmPkg/cli/ShowGoalCommand.c
  DcpmPkg/cli/DeleteGoalCommand.c
  DcpmPkg/cli/ShowErrorCommand.c
  DcpmPkg/cli/ShowCelCommand.c
  DcpmPkg/cli/DumpDebugCommand.c
  DcpmPkg/cli/StartDiagnosticCommand.c
  DcpmPkg/cli/ShowPreferencesCommand.c
  DcpmPkg/cli/ShowTopologyCommand.c
  DcpmPkg/cli/SetPreferencesCommand.c
  DcpmPkg/cli/SetSensorCommand.c
  DcpmPkg/cli/LoadGoalCommand.c
  DcpmPkg/cli/DumpGoalCommand.c
  DcpmPkg/cli/SetDimmCommand.c
  DcpmPkg/cli/LoadCommand.c
  DcpmPkg/cli/DeleteDimmCommand.c
  src/os/cli_cmds/DumpSupportCommand.c
  DcpmPkg/cli/ShowRegisterCommand.c
  DcpmPkg/cli/StartFormatCommand.c
  DcpmPkg/cli/ShowPerformanceCommand.c
  DcpmPkg/cli/ShowHostServerCommand.c
  DcpmPkg/cli/ShowCmdAccessPolicyCommand.c
  DcpmPkg/cli/StartSessionCommand.c
  DcpmPkg/cli/StopSessionCommand.c
  DcpmPkg/cli/LoadSessionCommand.c
  DcpmPkg/cli/ShowSessionCommand.c
  DcpmPkg/cli/DumpSessionCommand.c
  DcpmPkg/common/FwUtility.c
  DcpmPkg/common/Utility.c
  DcpmPkg/common/UtilityIo.c
  DcpmPkg/common/NvmTables.c
  DcpmPkg/common/ShowAcpi.c
  DcpmPkg/common/NvmStatus.c
  DcpmPkg/common/NvmHealth.c
  DcpmPkg/common/LbaCommon.c
  DcpmPkg/common/Convert.c
  DcpmPkg/common/PcdCommon.c
  DcpmPkg/common/OsCommon.c
  DcpmPkg/common/DataSet.c
  DcpmPkg/common/Printer.c
  DcpmPkg/common/Strings.c
  DcpmPkg/common/Nlog.c
  DcpmPkg/common/ReadRunTimePreferences.c
  DcpmPkg/driver/Protocol/Driver/NvmDimmConfig.c
  DcpmPkg/driver/NvmDimmDriver.c
  DcpmPkg/driver/Core/Dimm.c
  DcpmPkg/driver/Core/Namespace.c
  DcpmPkg/driver/Core/NvmSecurity.c
  DcpmPkg/driver/Core/Region.c
  DcpmPkg/driver/Core/Btt.c
  DcpmPkg/driver/Core/Pfn.c
  DcpmPkg/driver/Core/Diagnostics/ConfigDiagnostic.c
  DcpmPkg/driver/Core/Diagnostics/CoreDiagnostics.c
  DcpmPkg/driver/Core/Diagnostics/FwDiagnostic.c
  DcpmPkg/driver/Core/Diagnostics/QuickDiagnostic.c
  DcpmPkg/driver/Core/Diagnostics/SecurityDiagnostic.c
  DcpmPkg/driver/Protocol/Device/NvmFirmwareManagement.c
  DcpmPkg/driver/Protocol/Namespace/NvmDimmBlockIo.c
  DcpmPkg/driver/Utils/PlatformConfigData.c
  DcpmPkg/driver/Utils/AcpiParsing.c
  DcpmPkg/driver/Utils/ProcessorAndTopologyInfo.c
  DcpmPkg/driver/Utils/Interleave.c
  DcpmPkg/driver/Utils/SmbiosUtility.c
  DcpmPkg/driver/Utils/DumpLoadRegions.c
  DcpmPkg/common/Pbr.c
  DcpmPkg/common/PbrDcpmm.c
  DcpmPkg/common/PbrOs.c
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

# if on Windows add rc file for file details
if (MSVC)
  list(APPEND LIBIPMCTL_SOURCE_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/src/os/win/libipmctl.rc
    )
endif()

add_library(ipmctl ${LIB_TYPE} ${LIBIPMCTL_SOURCE_FILES})

target_include_directories(ipmctl PUBLIC
  DcpmPkg/cli
  src/os
  src/os/efi_shim
  src/os/ini
  src/os/eventlog
  src/os/nvm_api
  src/os/s_string
  src/os/cli_cmds
  DcpmPkg/common
  DcpmPkg/driver
  DcpmPkg/driver/Utils
  DcpmPkg/driver/Protocol/Driver
  DcpmPkg/driver/Core
  DcpmPkg/driver/Protocol/Namespace
  DcpmPkg/driver/Protocol/Device
  DcpmPkg/driver/Core/Diagnostics
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
  SET_SOURCE_FILES_PROPERTIES(DcpmPkg/cli/Common.c PROPERTIES COMPILE_FLAGS -D_CRT_SECURE_NO_WARNINGS)
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

# if on Windows add rc file for file details
if (MSVC)
  list(APPEND IPMCTL_SOURCE_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/src/os/win/ipmctl.rc
    )
endif()

add_executable(ipmctl-bin ${IPMCTL_SOURCE_FILES})

target_link_libraries(ipmctl-bin
  ipmctl
  )


target_include_directories(ipmctl-bin PUBLIC
  src/os
  src/os/nvm_api
  src/os/ini
  )

set_target_properties(ipmctl-bin
  PROPERTIES OUTPUT_NAME ipmctl
  )

if ((ASCIIDOCTOR_FOUND OR A2X_FOUND) AND LNX_BUILD)
  add_dependencies(ipmctl-bin
    manpage
    )
endif()

if(MSVC)
  set_target_properties(ipmctl-bin PROPERTIES LINK_FLAGS "/STACK:3000000")
else()
  target_compile_options(ipmctl-bin PRIVATE "-fPIE")
    set_target_properties(ipmctl-bin PROPERTIES LINK_FLAGS "-pie")
endif()

#----------------------------------------------------------------------------------------------------
# Generate String Definitions
#----------------------------------------------------------------------------------------------------
set(STRING_DEFS_INPUT_FILES
  ${ROOT}/DcpmPkg/common/NvmStatus.uni
  ${ROOT}/DcpmPkg/driver/Core/Diagnostics/DiagnosticsMessages.uni
  ${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings.py
  ${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings_ordered_dict.py
  )

set(STRING_DEFS_OUTPUT_FILES
  ${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings.h
  ${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_defs.h
  )

add_custom_target(stringdefs ALL
  DEPENDS ${STRING_DEFS_OUTPUT_FILES} ${STRING_DEFS_INPUT_FILES})

add_custom_command(OUTPUT ${STRING_DEFS_OUTPUT_FILES}
  COMMAND ${PYTHON_EXECUTABLE} ${ROOT}/src/os/efi_shim/os_efi_hii_auto_gen_strings.py
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
  COMMAND ${PYTHON_EXECUTABLE} ${ROOT}/src/os/ini/ini_auto_gen_default_config.py ${ROOT}/src/os/ini/ipmctl_default.i ${OUTPUT_DIR}/ipmctl_default.conf
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
  DcpmPkg/common/NvmSharedDefs.h
  )

FILE(COPY opensource/opensource_LICENSE
  DESTINATION ${OUTPUT_DIR})

FILE(RENAME ${OUTPUT_DIR}/opensource_LICENSE ${OUTPUT_DIR}/LICENSE)

FILE(COPY ${NVM_HEADERS}
  DESTINATION ${OUTPUT_DIR})

FILE(COPY install/linux/logrotate/ipmctl.logrotate.conf
  DESTINATION ${OUTPUT_DIR})

if(LNX_BUILD)
  include(GNUInstallDirs)
  find_package(PkgConfig)

  pkg_check_modules(SYSTEMD systemd)

  configure_file(${ROOT}/install/linux/libipmctl.pc.in ${OUTPUT_DIR}/libipmctl.pc @ONLY)

  if(BUILD_STATIC)
    install(TARGETS ipmctl-bin
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      )
  else()
    install(TARGETS ipmctl-bin ipmctl
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      )
  endif()

  install(FILES ${OUTPUT_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ipmctl
    )

  install(FILES ${OUTPUT_DIR}/ipmctl_default.conf
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/ipmctl
    )

  install(FILES ${OUTPUT_DIR}/ipmctl_default.conf
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/ipmctl
    RENAME ipmctl.conf
    )

  install(FILES ${NVM_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

  file(MAKE_DIRECTORY output/ipmctl)

  install(DIRECTORY output/ipmctl
    DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/log
    )

  install(FILES ${OUTPUT_DIR}/ipmctl.logrotate.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/logrotate.d
    RENAME ipmctl
    )

  install(FILES ${OUTPUT_DIR}/libipmctl.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )

  if(ASCIIDOCTOR_FOUND OR A2X_FOUND)
    install(DIRECTORY ${OUTPUT_DIR}/manpage/
      DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
      )
  endif()
endif()

#----------------------------------------------------------------------------------------------------
# Generate man pages with asciidoctor
#----------------------------------------------------------------------------------------------------
if((ASCIIDOCTOR_FOUND OR A2X_FOUND) AND LNX_BUILD)
	file(MAKE_DIRECTORY ${OUTPUT_DIR}/manpage)
	file(MAKE_DIRECTORY ${OUTPUT_DIR}/docs)

	set(DOCUMENTATION_INPUT_FILES
		${ROOT}/Documentation/ipmctl/ipmctl.txt
		${ROOT}/Documentation/ipmctl/Discovery/ipmctl-show-dimm.txt
		${ROOT}/Documentation/ipmctl/Discovery/ipmctl-show-memory-resources.txt
		${ROOT}/Documentation/ipmctl/Discovery/ipmctl-show-socket.txt
		${ROOT}/Documentation/ipmctl/Discovery/ipmctl-show-system-capabilities.txt
		${ROOT}/Documentation/ipmctl/Discovery/ipmctl-show-topology.txt
		${ROOT}/Documentation/ipmctl/Memory_Subsystem_Provisioning/ipmctl-create-goal.txt
		${ROOT}/Documentation/ipmctl/Memory_Subsystem_Provisioning/ipmctl-delete-goal.txt
		${ROOT}/Documentation/ipmctl/Memory_Subsystem_Provisioning/ipmctl-dump-goal.txt
		${ROOT}/Documentation/ipmctl/Memory_Subsystem_Provisioning/ipmctl-load-goal.txt
		${ROOT}/Documentation/ipmctl/Memory_Subsystem_Provisioning/ipmctl-show-goal.txt
		${ROOT}/Documentation/ipmctl/Persistent_Memory_Provisioning/ipmctl-show-region.txt
		${ROOT}/Documentation/ipmctl/Instrumentation/ipmctl-set-sensor.txt
		${ROOT}/Documentation/ipmctl/Instrumentation/ipmctl-show-performance.txt
		${ROOT}/Documentation/ipmctl/Instrumentation/ipmctl-show-sensor.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-set-preferences.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-dump-support-data.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-help.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-set-dimm.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-show-firmware.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-show-preferences.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-load-dimm.txt
		${ROOT}/Documentation/ipmctl/Support_and_Maintenance/ipmctl-version.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-show-pcd.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-delete-pcd.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-dump-debug-log.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-inject-error.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-show-cap.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-show-cel.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-start-diagnostic.txt
#		${ROOT}/Documentation/ipmctl/Debug/ipmctl-diagnostic-events.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-show-system.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-show-error-log.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-show-session.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-start-session.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-stop-session.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-load-session.txt
		${ROOT}/Documentation/ipmctl/Debug/ipmctl-dump-session.txt
		)

	list(LENGTH DOCUMENTATION_INPUT_FILES LEN)
	math(EXPR INPUT_LENGTH "${LEN} -1")
	foreach(INDEX RANGE ${INPUT_LENGTH})
		list(GET DOCUMENTATION_INPUT_FILES ${INDEX} MAN_INPUT)
		string(REGEX MATCH "[^/]*$" MAN_BARE_FILENAME ${MAN_INPUT})
		set(MAN_TMP_INPUT ${OUTPUT_DIR}/manpage/${MAN_BARE_FILENAME})
		string(REGEX REPLACE "txt$" "1.gz" MAN_OUTPUT "${MAN_TMP_INPUT}")
		list(APPEND MANPAGE_OUTPUT_FILES ${MAN_OUTPUT})
		if(NOT ASCIIDOCTOR_FOUND)
			string(REGEX REPLACE "\\.[^.]*$" "" MAN_GZ_INPUT ${MAN_OUTPUT})
			add_custom_command(OUTPUT ${MAN_OUTPUT}
				COMMENT "Clean out link markup because they are invalid in the manpages"
				COMMAND sed -e 's/<</\\[/g' -e 's/>>/]/g' ${MAN_INPUT} > ${MAN_TMP_INPUT}
				COMMAND ${A2X_BINARY} -f manpage -a mansource=ipmctl -a manmanual="ipmctl" -a ipmctl_version=${IPMCTL_VERSION_STRING} -a os_build=1 -a manpage=1 -D ${OUTPUT_DIR}/manpage ${MAN_TMP_INPUT}
				COMMAND gzip -f ${MAN_GZ_INPUT}
				COMMAND rm -f ${MAN_TMP_INPUT}
				COMMENT "Generating man pages"
				DEPENDS ${MAN_INPUT}
			)
		endif()
	endforeach()
	add_custom_target(manpage ALL
		DEPENDS ${MANPAGE_OUTPUT_FILES} ${DOCUMENTATION_INPUT_FILES}
		)
	if(ASCIIDOCTOR_FOUND)
		add_custom_command(OUTPUT ${MANPAGE_OUTPUT_FILES}
			COMMAND ${ASCIIDOCTOR_BINARY} -b manpage -a mansource=ipmctl -a manmanual="ipmctl" -a ipmctl_version=${IPMCTL_VERSION_STRING} -a os_build=1 -a manpage=1 -D ${OUTPUT_DIR}/manpage ${DOCUMENTATION_INPUT_FILES}
			COMMAND gzip -f ${OUTPUT_DIR}/manpage/*.1
			COMMENT "Generating man pages"
			DEPENDS ${DOCUMENTATION_INPUT_FILES}
			)
	endif()
endif()

# Allow building more documentation if we have access to it
if(EXISTS "${ROOT}/CMake/internal.cmake")
  include("${ROOT}/CMake/internal.cmake")
endif()

# --------------------------------------------------------------------------------------------------
# Additional includes
# --------------------------------------------------------------------------------------------------
# CppUTest unit tests
# Ideally we want official builds to not include unit test executables and
# have it easy to build unit tests as an individual target from development
# environments. However, after some attempts of modifying linux_build.sh, I
# still haven't found a solution. So if you want to run unit tests, just
# uncomment this UNIT_TEST flag or define it using the build scripts for now.
#set(UNIT_TEST on)
if (UNIT_TEST)
  add_subdirectory("./tests/cpputest" "./cpputest_out")
endif()

if(LNX_BUILD AND UNIT_TEST)
  include(CMake/unit_test.cmake)
endif()

if(ESX_BUILD)
  include(CMake/esx.cmake)
endif()

add_subdirectory(src/os/nvm_api_sample)