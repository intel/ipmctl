SET(CMAKE_SKIP_BUILD_RPATH FALSE)

# --------------------------------------------------------------------------------------------------
# Google Test and Google Mock
# --------------------------------------------------------------------------------------------------
# Download and unpack googletest at configure time
configure_file(CMake/GTest.txt.in ${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download
	)

if(result)
	message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download
	)

if(result)
	message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
	${CMAKE_BINARY_DIR}/googletest-build
	EXCLUDE_FROM_ALL
	)


# --------------------------------------------------------------------------------------------------
# libipmctl tests
# --------------------------------------------------------------------------------------------------
file(GLOB CORE_TEST_SRC
	src/os/nvm_api/unittest/*
	)

message(TESTS: ${CORE_TEST_SRC})
add_executable(ipmctl_test ${CORE_TEST_SRC})

target_link_libraries(ipmctl_test
	gtest
	gtest_main
	gmock
	ipmctl
	)

include_directories(ipmctl_test SYSTEM PUBLIC
	src/os/nvm_api
	)
