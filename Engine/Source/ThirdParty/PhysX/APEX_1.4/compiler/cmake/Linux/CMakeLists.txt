cmake_minimum_required(VERSION 3.3)
include(../common/CMakeLists.txt)

STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWERCASE)

IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to Linux
	SET(TARGET_BUILD_PLATFORM "Linux")
ENDIF()

SET(PLATFORM_LIST Linux)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

IF (${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-unknown-linux-gnu" OR ${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-linux-gnu")
	SET(CMAKE_CXX_FLAGS "-std=c++11 -fdiagnostics-show-option -Wall -Wextra -Werror -fno-rtti -fno-exceptions -fno-strict-aliasing -ffunction-sections -fdata-sections -Wno-unknown-pragmas -Wno-dynamic-class-memaccess -Wno-reorder -Wno-logical-not-parentheses -Wno-unused-parameter -Wno-unused-variable -Wno-unused-const-variable -Werror -Wno-unused-function -Wno-unknown-pragmas -Wno-overloaded-virtual  -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-reserved-id-macro -Wno-c++98-compat-pedantic -Wno-unused-local-typedef -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-reinterpret-cast -Wno-invalid-offsetof -Wno-local-type-template-args -Wno-undefined-func-template -Wno-expansion-to-defined -Wno-zero-as-null-pointer-constant -Wno-unused-template")
ELSEIF(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "arm-unknown-linux-gnueabihf")
	SET(CMAKE_CXX_FLAGS "-std=c++11 -mfpu=neon -fdiagnostics-show-option -Wall -Wextra -Werror -fno-rtti -fno-exceptions -fno-strict-aliasing -ffunction-sections -fdata-sections -Wno-unknown-pragmas -Wno-dynamic-class-memaccess -Wno-reorder -Wno-logical-not-parentheses -Wno-unused-parameter -Wno-unused-variable -Wno-unused-const-variable -Werror -Wno-unused-function -Wno-unknown-pragmas -Wno-overloaded-virtual  -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-c++98-compat-pedantic -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-reinterpret-cast -Wno-invalid-offsetof -Wno-local-type-template-args")
ELSE(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-unknown-linux-gnu" OR ${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-linux-gnu")
	MESSAGE(FATAL_ERROR "Unknown CMAKE_LIBRARY_ARCHITECTURE ${CMAKE_LIBRARY_ARCHITECTURE}")
ENDIF(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-unknown-linux-gnu" OR ${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-linux-gnu")

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -gdwarf-3")
SET(CMAKE_CXX_FLAGS_CHECKED "-O3 -g -gdwarf-3")
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g -gdwarf-3")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -gdwarf-3")

SET(APEX_LINUX_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG;PX_CHECKED;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER;PX_PROFILE)
SET(APEX_LINUX_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER;PX_ENABLE_CHECKED_ASSERTS)
SET(APEX_LINUX_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER)
SET(APEX_LINUX_RELEASE_COMPILE_DEFS NDEBUG;APEX_SHIPPING)

# Add PhysX as a dependency so that we can use project references
ADD_SUBDIRECTORY(${PHYSX_ROOT_DIR}/Source/compiler/cmake/Linux "${CMAKE_CURRENT_BINARY_DIR}/physx_bin")

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


