# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 2.8.11)

# CMake script build HarfBuzz for using our pre-built third-party libraries

option(BUILD_WITH_FREETYPE_2_6 OFF)
if (${WIN32})
	set(BUILD_WITH_FREETYPE_2_6 ON)
endif()
#if ( NOT $ENV{EMSCRIPTEN} EQUAL "" )
#	set(BUILD_WITH_FREETYPE_2_6 ON)
#endif()

if(BUILD_WITH_FREETYPE_2_6)
	set(FREETYPE2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../FreeType2/FreeType2-2.6")
else()
	set(FREETYPE2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../FreeType2/FreeType2-2.4.12")
endif()
set(ICU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../ICU/icu4c-53_1")

project(harfbuzz)

# Some platforms need to define these in order to build
option(USE_INTEL_ATOMIC_PRIMITIVES "Platform needs to define HAVE_INTEL_ATOMIC_PRIMITIVES" FALSE)
option(USE_STUB_GETENV "Platform needs to define stub getenv as it doesn't have its own version" FALSE)

# Put our build output one level up so we can easily delete the temporary files and only check-in the final libs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../")

# Gather up all the HarfBuzz source files
file(GLOB HARFBUZZ_SRC_FILES 
	LIST_DIRECTORIES=false
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/*.hh"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cc"
	)
	
# We exclude some source files that we don't need (we'll be using ICU rather than GLib for our Unicode support, and we don't need CoreText, DirectWrite, Graphite2, HarfBuzz's internal UCDN library, or Uniscribe).
list(REMOVE_ITEM HARFBUZZ_SRC_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-coretext.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-directwrite.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-glib.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-gobject-structs.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-graphite2.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-ucdn.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/hb-uniscribe.cc"
	)

# Exclude the source files that only generate tests
list(REMOVE_ITEM HARFBUZZ_SRC_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/test.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/test-buffer-serialize.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/test-size-params.cc"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/test-would-substitute.cc"
	)

# Include our configuration header
add_definitions(-DHAVE_CONFIG_H)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
list(APPEND HARFBUZZ_SRC_FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/config.h"
	)
	
# Some platforms need to define these in order to build
if(USE_INTEL_ATOMIC_PRIMITIVES)
	add_definitions(-DHAVE_INTEL_ATOMIC_PRIMITIVES)
endif()
if(USE_STUB_GETENV)
	add_definitions(-DHAVE_STUB_GETENV)
endif()
	
# Add the include directory for our own build of FreeType2
add_definitions(-DHAVE_OT)
include_directories("${FREETYPE2_ROOT}/Devel" "${FREETYPE2_ROOT}/Include")

# Add the include directory for our own build of ICU
add_definitions(-DHAVE_ICU -DHAVE_ICU_BUILTIN)
include_directories("${ICU_ROOT}/include")

# Specify that we are using our own allocator
add_definitions(-Dhb_malloc_impl=HarfBuzzMalloc -Dhb_calloc_impl=HarfBuzzCalloc -Dhb_realloc_impl=HarfBuzzRealloc -Dhb_free_impl=HarfBuzzFree)

# Tell CMake to generate a static library
add_library(harfbuzz ${HARFBUZZ_SRC_FILES})
