cmake_minimum_required(VERSION 3.3)
PROJECT(PxShared CXX)
include(../common/CMakeLists.txt)


IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to TVOS
	SET(TARGET_BUILD_PLATFORM "TVOS")
ENDIF()

SET(PLATFORM_LIST TVOS)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

SET(CMAKE_CXX_FLAGS "-std=c++11 -fno-rtti -fno-exceptions -ffast-math -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -Weverything -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-float-equal -Wno-padded -Wno-weak-vtables -Wno-cast-align -Wno-conversion -Wno-missing-noreturn -Wno-missing-variable-declarations -Wno-shift-sign-overflow -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-unreachable-code -Wno-unused-macros -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-deprecated -Wno-non-virtual-dtor -Wno-old-style-cast -Wno-return-type-c-linkage -Wno-format-nonliteral -Wno-unused-local-typedef -Wno-implicit-fallthrough -gdwarf-2")

SET(CMAKE_SHARED_LINKER_FLAGS "")

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_CHECKED "-O3 -g")
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

SET(CMAKE_OSX_DEPLOYMENT_TARGET "")

SET(CMAKE_OSX_ARCHITECTURES "arm64")

SET(TVOS_PLATFORM_LOCATION "AppleTVOS.platform")

# Setup tvOS developer location unless specified manually with CMAKE_TVOS_DEVELOPER_ROOT
EXEC_PROGRAM(/usr/bin/xcode-select ARGS -print-path OUTPUT_VARIABLE CMAKE_XCODE_DEVELOPER_DIR)
SET(XCODE_ROOT "${CMAKE_XCODE_DEVELOPER_DIR}/Platforms/${TVOS_PLATFORM_LOCATION}/Developer")
IF(NOT DEFINED CMAKE_TVOS_DEVELOPER_ROOT)
	IF(EXISTS ${XCODE_ROOT})
		SET(CMAKE_TVOS_DEVELOPER_ROOT ${XCODE_ROOT})
	ENDIF(EXISTS ${XCODE_ROOT})
ENDIF(NOT DEFINED CMAKE_TVOS_DEVELOPER_ROOT)
SET(CMAKE_TVOS_DEVELOPER_ROOT ${CMAKE_TVOS_DEVELOPER_ROOT} CACHE PATH "Location of tvOS Platform")

# Find and use the most recent tvOS sdk unless specified manually with CMAKE_TVOS_SDK_ROOT
IF(NOT DEFINED CMAKE_TVOS_SDK_ROOT)
	FILE(GLOB _CMAKE_TVOS_SDKS "${CMAKE_TVOS_DEVELOPER_ROOT}/SDKs/*")
	IF(_CMAKE_TVOS_SDKS) 
		LIST(SORT _CMAKE_TVOS_SDKS)
		LIST(REVERSE _CMAKE_TVOS_SDKS)
		LIST(GET _CMAKE_TVOS_SDKS 0 CMAKE_TVOS_SDK_ROOT)
	ELSE(_CMAKE_TVOS_SDKS)
		MESSAGE(FATAL_ERROR "No tvOS SDK's found in default search path ${CMAKE_TVOS_DEVELOPER_ROOT}. Manually set CMAKE_TVOS_SDK_ROOT or install the tvOS SDK.")
	ENDIF(_CMAKE_TVOS_SDKS)
	MESSAGE(STATUS "Toolchain using default tvOS SDK: ${CMAKE_TVOS_SDK_ROOT}")
ENDIF(NOT DEFINED CMAKE_TVOS_SDK_ROOT)
SET(CMAKE_TVOS_SDK_ROOT ${CMAKE_TVOS_SDK_ROOT} CACHE PATH "Location of the selected tvOS SDK")

# Set the sysroot default to the most recent SDK
SET(CMAKE_OSX_SYSROOT ${CMAKE_TVOS_SDK_ROOT} CACHE PATH "Sysroot used for tvOS support")

SET(CMAKE_XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "appletvos")
SET(CMAKE_XCODE_ATTRIBUTE_SDKROOT ${CMAKE_TVOS_SDK_ROOT})
SET(CMAKE_XCODE_ATTRIBUTE_TVOS_DEPLOYMENT_TARGET "9.0")

SET(PXSHARED_TVOS_COMPILE_DEFS _LIB;DISABLE_CUDA_PHYSX;DISABLE_COMPUTE_PHYSX)
SET(PXSHARED_TVOS_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG=1;PX_CHECKED=1)
SET(PXSHARED_TVOS_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED=1)
SET(PXSHARED_TVOS_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE=1)
SET(PXSHARED_TVOS_RELEASE_COMPILE_DEFS NDEBUG)

# NOTE: PxCudaContextManager excluded on this platform

# Include project cmake files here
INCLUDE(PxFoundation.cmake)
INCLUDE(PsFastXml.cmake)
INCLUDE(PxPvdSDK.cmake)
INCLUDE(PxTask.cmake)
