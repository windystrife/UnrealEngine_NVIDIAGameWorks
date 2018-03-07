cmake_minimum_required(VERSION 3.3)
include(../common/CMakeLists.txt)


IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to Mac
	SET(TARGET_BUILD_PLATFORM "Mac")
ENDIF()

SET(PLATFORM_LIST Mac)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

# TODO: Get real flags
SET(CMAKE_CXX_FLAGS "-fdiagnostics-show-option -Wall -Wextra -Werror -fno-rtti -fno-exceptions -ffast-math -fno-strict-aliasing -ffunction-sections -fdata-sections -Wno-unknown-pragmas -Wno-dynamic-class-memaccess -Wno-reorder -Wno-logical-not-parentheses -Wno-unused-parameter -Wno-unused-variable -Wno-unused-const-variable -Werror -Wno-unused-function -Wno-unknown-pragmas -Wno-overloaded-virtual  -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-reserved-id-macro -Wno-c++98-compat-pedantic -Wno-unused-local-typedef -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-reinterpret-cast -Wno-invalid-offsetof -Wno-local-type-template-args -gdwarf-2")

SET(CMAKE_SHARED_LINKER_FLAGS "")

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_CHECKED "-O3 -g")
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

# Build libs compatible with OS X 10.9
SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")

#set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")

SET(APEX_MAC_COMPILE_DEFS _CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE)
SET(APEX_MAC_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG;PX_CHECKED;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER;PX_PROFILE)
SET(APEX_MAC_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER;PX_ENABLE_CHECKED_ASSERTS)
SET(APEX_MAC_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER)
SET(APEX_MAC_RELEASE_COMPILE_DEFS NDEBUG;APEX_SHIPPING)

# Add PhysX as a dependency so that we can use project references
ADD_SUBDIRECTORY(${PHYSX_ROOT_DIR}/Source/compiler/cmake/Mac "${CMAKE_CURRENT_BINARY_DIR}/physx_bin")

#Include projects
INCLUDE(ApexCommon.cmake)
INCLUDE(ApexClothing.cmake)
INCLUDE(ApexDestructible.cmake)
INCLUDE(ApexLegacy.cmake)
INCLUDE(ApexLoader.cmake)
INCLUDE(ApexFramework.cmake)
INCLUDE(ApexShared.cmake)
INCLUDE(NvParameterized.cmake)
INCLUDE(RenderDebug.cmake)
