# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 2.8.12)

project(ipmctl)

set(CMAKE_VERBOSE_MAKEFILE on)

add_executable(nvm_api_sample
	main.c)

target_include_directories(nvm_api_sample
	PRIVATE
	src/os/nvm_api/
	${OUTPUT_DIR}
)

target_link_libraries(nvm_api_sample
	ipmctl)