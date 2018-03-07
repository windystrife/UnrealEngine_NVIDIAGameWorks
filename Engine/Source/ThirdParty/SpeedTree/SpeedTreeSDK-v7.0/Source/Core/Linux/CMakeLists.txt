project(SpeedTreeCore)
cmake_minimum_required(VERSION 2.8)
aux_source_directory( . SRC_LIST)

if (CMAKE_BUILD_TYPE STREQUAL "")
  # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
  # differentiation between debug and release builds.
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

option(USE_CLANG "Build libSpeedTree with clang ON(default)|OFF " ON)

if(${USE_CLANG} STREQUAL "OFF")
        message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
        set(CMAKE_CXX_FLAGS_RELEASE "-Wall -std=c++11 -O3 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Wall -std=c++11 -g -O2")
        set(CMAKE_CXX_FLAGS_DEBUG "-Wall -std=c++11 -g -Og -DDEBUG")
        set(CMAKE_LINKER_FLAGS "--export-dynamic")
elseif(${USE_CLANG} STREQUAL "ON")
        set(CMAKE_C_COMPILER             "/usr/bin/clang")
        set(CMAKE_C_FLAGS                "-Wall -std=c99")
        set(CMAKE_C_FLAGS_DEBUG          "-g")
        set(CMAKE_C_FLAGS_MINSIZEREL     "-Os -DNDEBUG ")
        set(CMAKE_C_FLAGS_RELEASE        "-O2 -DNDEBUG ")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

        set(CMAKE_CXX_COMPILER             "/usr/bin/clang++") # or "/usr/bin/clang++-3.5"
        set(CMAKE_CXX_FLAGS                "-Wall -std=c++11")
        set(CMAKE_CXX_FLAGS_DEBUG          "-g")
        set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE        "-O2 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
        message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
endif(${USE_CLANG} STREQUAL "OFF")

if((${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo") OR (${CMAKE_BUILD_TYPE} STREQUAL "Release"))
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../../../Lib/Linux/x86_64-unknown-linux-gnu/Release")
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../../../../Lib/Linux/x86_64-unknown-linux-gnu/Debug")
endif((${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo") OR (${CMAKE_BUILD_TYPE} STREQUAL "Release"))

INCLUDE_DIRECTORIES(
    "../Source/"
    "../ Source/Evaluation/"
    "../../../Include/"
    )

set(SRC_LIST
    ${CMAKE_SOURCE_DIR}/../Source/ScopeTrace.cpp
    ${CMAKE_SOURCE_DIR}/../Source/Core.cpp
    ${CMAKE_SOURCE_DIR}/../Source/ErrorHandler.cpp
    ${CMAKE_SOURCE_DIR}/../Source/Evaluation/Key.cpp
    ${CMAKE_SOURCE_DIR}/../Source/Fixed.cpp
    ${CMAKE_SOURCE_DIR}/../Source/Parser.cpp
    ${CMAKE_SOURCE_DIR}/../Source/Memory.cpp
    ${CMAKE_SOURCE_DIR}/../Source/Wind.cpp
    ${CMAKE_SOURCE_DIR}/../Source/AllocatorStats.cpp
    ${CMAKE_SOURCE_DIR}/../Source/FileSystem.cpp
    ${CMAKE_SOURCE_DIR}/../Source/CoordSys.cpp
    )

add_library(${PROJECT_NAME} STATIC ${SRC_LIST})

add_library(${PROJECT_NAME}_fPIC STATIC ${SRC_LIST})
set_target_properties(${PROJECT_NAME}_fPIC PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
