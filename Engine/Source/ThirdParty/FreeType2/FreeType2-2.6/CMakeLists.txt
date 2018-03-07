# CMakeLists.txt
#
# Copyright 2013-2015 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# Written by John Cary <cary@txcorp.com>
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.
#
#
# Say
#
#   cmake CMakeLists.txt
#
# to create a Makefile that builds a static version of the library.
#
# For a dynamic library, use
#
#   cmake CMakeLists.txt -DBUILD_SHARED_LIBS:BOOL=true
#
# For a framework on OS X, use
#
#   cmake CMakeLists.txt -DBUILD_FRAMEWORK:BOOL=true -G Xcode
#
# instead.
#
# For an iOS static library, use
#
#   cmake CMakeLists.txt -DIOS_PLATFORM=OS -G Xcode
#
# or
#
#   cmake CMakeLists.txt -DIOS_PLATFORM=SIMULATOR -G Xcode
#
# Please refer to the cmake manual for further options, in particular, how
# to modify compilation and linking parameters.
#
# Some notes.
#
# . `cmake' will overwrite FreeType's original (top-level) `Makefile' file.
#
# . You can use `cmake' directly on a freshly cloned FreeType git
#   repository.
#
# . `CMakeLists.txt'  is provided as-is since it is not used by the
#   developer team.


cmake_minimum_required(VERSION 2.6)

# CMAKE_TOOLCHAIN_FILE must be set before `project' is called, which
# configures the base build environment and references the toolchain file
if (APPLE)
  if (DEFINED IOS_PLATFORM)
    if (NOT "${IOS_PLATFORM}" STREQUAL "OS"
        AND NOT "${IOS_PLATFORM}" STREQUAL "SIMULATOR")
      message(FATAL_ERROR
        "IOS_PLATFORM must be set to either OS or SIMULATOR")
    endif ()
    if (NOT "${CMAKE_GENERATOR}" STREQUAL "Xcode")
      message(AUTHOR_WARNING
        "You should use Xcode generator with IOS_PLATFORM enabled to get Universal builds.")
    endif ()
    if (BUILD_SHARED_LIBS)
      message(FATAL_ERROR
        "BUILD_SHARED_LIBS can not be on with IOS_PLATFORM enabled")
    endif ()
    if (BUILD_FRAMEWORK)
      message(FATAL_ERROR
        "BUILD_FRAMEWORK can not be on with IOS_PLATFORM enabled")
    endif ()

    # iOS only uses static libraries
    set(BUILD_SHARED_LIBS OFF)

    set(CMAKE_TOOLCHAIN_FILE
      ${PROJECT_SOURCE_DIR}/builds/cmake/iOS.cmake)
  endif ()
else ()
  if (DEFINED IOS_PLATFORM)
    message(FATAL_ERROR "IOS_PLATFORM is not supported on this platform")
  endif ()
endif ()

project(freetype)

if (BUILD_FRAMEWORK)
  if (NOT "${CMAKE_GENERATOR}" STREQUAL "Xcode")
    message(FATAL_ERROR
      "You should use Xcode generator with BUILD_FRAMEWORK enabled")
  endif ()
  set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD_32_64_BIT)")
  set(BUILD_SHARED_LIBS ON)
endif ()

set(VERSION_MAJOR "2")
set(VERSION_MINOR "6")
set(VERSION_PATCH "0")
set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

# Compiler definitions for building the library
add_definitions(-DFT2_BUILD_LIBRARY)

# Specify library include directories
include_directories("${PROJECT_SOURCE_DIR}/Include")

# Create the configuration file
message(STATUS "Creating directory, ${PROJECT_BINARY_DIR}/Include/freetype2.")
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/Include/freetype2)

# For the auto-generated ftconfig.h file
include_directories(BEFORE "${PROJECT_BINARY_DIR}/Include/freetype2")
message(STATUS "Creating ${PROJECT_BINARY_DIR}/Include/freetype2/ftconfig.h.")
execute_process(
  COMMAND sed -e "s/FT_CONFIG_OPTIONS_H/<ftoption.h>/" -e "s/FT_CONFIG_STANDARD_LIBRARY_H/<ftstdlib.h>/" -e "s?/undef ?#undef ?"
  INPUT_FILE ${PROJECT_SOURCE_DIR}/builds/unix/ftconfig.in
  OUTPUT_FILE ${PROJECT_BINARY_DIR}/Include/freetype2/ftconfig.h
)

file(GLOB PUBLIC_HEADERS "Include/*.h")
file(GLOB PUBLIC_CONFIG_HEADERS "Include/config/*.h")
file(GLOB PRIVATE_HEADERS "Include/internal/*.h")

set(BASE_SRCS
  Src/autofit/autofit.c
  Src/base/ftadvanc.c
  Src/base/ftbbox.c
  Src/base/ftbdf.c
  Src/base/ftbitmap.c
  Src/base/ftcalc.c
  Src/base/ftcid.c
  Src/base/ftdbgmem.c
  Src/base/ftdebug.c
  Src/base/ftfntfmt.c
  Src/base/ftfstype.c
  Src/base/ftgasp.c
  Src/base/ftgloadr.c
  Src/base/ftglyph.c
  Src/base/ftgxval.c
  Src/base/ftinit.c
  Src/base/ftlcdfil.c
  Src/base/ftmm.c
  Src/base/ftobjs.c
  Src/base/ftotval.c
  Src/base/ftoutln.c
  Src/base/ftpatent.c
  Src/base/ftpfr.c
  Src/base/ftrfork.c
  Src/base/ftsnames.c
  Src/base/ftstream.c
  Src/base/ftstroke.c
  Src/base/ftsynth.c
  Src/base/ftsystem.c
  Src/base/fttrigon.c
  Src/base/fttype1.c
  Src/base/ftutil.c
  Src/base/ftwinfnt.c
  Src/bdf/bdf.c
  Src/bzip2/ftbzip2.c
  Src/cache/ftcache.c
  Src/cff/cff.c
  Src/cid/type1cid.c
  Src/gzip/ftgzip.c
  Src/lzw/ftlzw.c
  Src/pcf/pcf.c
  Src/pfr/pfr.c
  Src/psaux/psaux.c
  Src/pshinter/pshinter.c
  Src/psnames/psmodule.c
  Src/raster/raster.c
  Src/sfnt/sfnt.c
  Src/smooth/smooth.c
  Src/truetype/truetype.c
  Src/type1/type1.c
  Src/type42/type42.c
  Src/winfonts/winfnt.c
)

include_directories("Src/truetype")
include_directories("Src/sfnt")
include_directories("Src/autofit")
include_directories("Src/smooth")
include_directories("Src/raster")
include_directories("Src/psaux")
include_directories("Src/psnames")

if (BUILD_FRAMEWORK)
  set(BASE_SRCS
    ${BASE_SRCS}
    builds/mac/freetype-Info.plist
  )
endif ()

add_library(freetype
  ${PUBLIC_HEADERS}
  ${PUBLIC_CONFIG_HEADERS}
  ${PRIVATE_HEADERS}
  ${BASE_SRCS}
)

add_library(freetype_fPIC
  ${PUBLIC_HEADERS}
  ${PUBLIC_CONFIG_HEADERS}
  ${PRIVATE_HEADERS}
  ${BASE_SRCS}
)

set_property(TARGET freetype_fPIC PROPERTY POSITION_INDEPENDENT_CODE TRUE)

if (BUILD_FRAMEWORK)
  set_property(SOURCE ${PUBLIC_CONFIG_HEADERS}
    PROPERTY MACOSX_PACKAGE_LOCATION Headers/config
  )
  set_target_properties(freetype PROPERTIES
    FRAMEWORK TRUE
    MACOSX_FRAMEWORK_INFO_PLIST builds/mac/freetype-Info.plist
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
    XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
  )
endif ()

# Installations
# Note the trailing slash in the argument to the `DIRECTORY' directive
install(DIRECTORY ${PROJECT_SOURCE_DIR}/Include/
  DESTINATION Include/freetype2
  PATTERN "internal" EXCLUDE
)
install(TARGETS freetype
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  FRAMEWORK DESTINATION Library/Frameworks
)

# Packaging
# CPack version numbers for release tarball name.
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH}})
if (NOT DEFINED CPACK_PACKAGE_DESCRIPTION_SUMMARY)
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${CMAKE_PROJECT_NAME}")
endif ()
if (NOT DEFINED CPACK_SOURCE_PACKAGE_FILE_NAME)
  set(CPACK_SOURCE_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${PROJECT_VERSION}-r${PROJECT_REV}"
    CACHE INTERNAL "tarball basename"
  )
endif ()
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_IGNORE_FILES
  "/CVS/;/.svn/;.swp$;.#;/#;/build/;/serial/;/ser/;/parallel/;/par/;~;/preconfig.out;/autom4te.cache/;/.config")
set(CPACK_GENERATOR TGZ)
include(CPack)

# add make dist target
add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)

# eof
