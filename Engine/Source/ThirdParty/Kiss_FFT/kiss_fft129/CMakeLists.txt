project(KissFFT)
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
        set(CMAKE_C_COMPILER             "/usr/bin/gcc")
        set(CMAKE_C_FLAGS                "-Wall -std=c99 -Wstrict-prototypes -Wmissing-prototypes -Waggregate-return -Wcast-align -Wcast-qual -Wnested-externs -Wshadow -Wbad-function-cast -Wwrite-strings")
        set(CMAKE_C_FLAGS_DEBUG          "-g")
        set(CMAKE_C_FLAGS_MINSIZEREL     "-Os -DNDEBUG ")
        set(CMAKE_C_FLAGS_RELEASE        "-O4 -DNDEBUG ")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

        set(CMAKE_CXX_FLAGS_RELEASE "-Wall -std=c++11 -O3 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Wall -std=c++11 -g -O2 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_DEBUG "-Wall -std=c++11 -g -Og -DDEBUG")
        set(CMAKE_LINKER_FLAGS "--export-dynamic")
elseif(${USE_CLANG} STREQUAL "ON")
        set(CMAKE_C_COMPILER             "/usr/bin/clang")
        set(CMAKE_C_FLAGS                "-Wall -std=c99")
        set(CMAKE_C_FLAGS_DEBUG          "-g")
        set(CMAKE_C_FLAGS_MINSIZEREL     "-Os -DNDEBUG ")
        set(CMAKE_C_FLAGS_RELEASE        "-O2 -DNDEBUG ")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG ")

        set(CMAKE_CXX_COMPILER             "/usr/bin/clang++") # or "/usr/bin/clang++-3.5"
        set(CMAKE_CXX_FLAGS                "-Wall -std=c++11")
        set(CMAKE_CXX_FLAGS_DEBUG          "-g")
        set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE        "-O2 -DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
        message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
endif(${USE_CLANG} STREQUAL "OFF")

if((${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo") OR (${CMAKE_BUILD_TYPE} STREQUAL "Release"))
    # todo: support other triplets
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../Lib/Linux/Release/x86_64-unknown-linux-gnu")
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    # todo: support other triplets
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "../Lib/Linux/Debug/x86_64-unknown-linux-gnu")
endif((${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo") OR (${CMAKE_BUILD_TYPE} STREQUAL "Release"))

INCLUDE_DIRECTORIES(
	"."
    "tools/"
    )

set(SRC_LIST
    ${CMAKE_SOURCE_DIR}/kiss_fft.c 
    ${CMAKE_SOURCE_DIR}/tools/kfc.c
    ${CMAKE_SOURCE_DIR}/tools/kiss_fftnd.c
    ${CMAKE_SOURCE_DIR}/tools/kiss_fftndr.c
    ${CMAKE_SOURCE_DIR}/tools/kiss_fftr.c
    )

add_library(${PROJECT_NAME} STATIC ${SRC_LIST})

add_library(${PROJECT_NAME}_fPIC STATIC ${SRC_LIST})
set_target_properties(${PROJECT_NAME}_fPIC PROPERTIES POSITION_INDEPENDENT_CODE TRUE)


