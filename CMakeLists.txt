cmake_minimum_required(VERSION 3.21)

# CMake variables
get_filename_component(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)

# Project data
project(SOCSimulator)

# Directories
set(INC_DIR include)
set(LIB_DIR libs)
set(SRC_DIR src)
set(THIRD_PARTY_DIR third-party)
set(TEST_DIR gtest)

# Compile flags
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20 CACHE INTERNAL "This is a read-only variable")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
        Debug
        CACHE STRING
        "Choose the type of build, options are: empty, Debug, Release, RelWithDebInfo, MinSizeRel."
        FORCE
    )
endif()

# Compiler dependent flags
if(PROJECT_IS_TOP_LEVEL)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-ffile-prefix-map=${ROOT_DIR}=.)
    else(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(
            FATAL_ERROR
            "The compilation flag `-ffile-prefix-map` is only supported by GNU compilers. Please define an alternative for your compiler."
        )
    endif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
endif(PROJECT_IS_TOP_LEVEL)

# Link pthread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
if(NOT Threads_FOUND)
    find_package(Threads REQUIRED)
endif(NOT Threads_FOUND)

# Link Torch
if(NOT Torch_FOUND)
    find_package(Torch REQUIRED)
endif(NOT Torch_FOUND)

# CMake options
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

# Third-party options
set(JSON_BuildTests OFF CACHE INTERNAL "")

# Subdirectories
add_subdirectory(${LIB_DIR})
add_subdirectory(${SRC_DIR})
add_subdirectory(${THIRD_PARTY_DIR})
add_subdirectory(${TEST_DIR})
add_subdirectory(soc)
