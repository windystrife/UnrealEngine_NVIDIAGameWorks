cmake_minimum_required(VERSION 3.3)
#set(CMAKE_VERBOSE_MAKEFILE ON)
include(../common/CMakeLists.txt)


IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to HTML5
	SET(TARGET_BUILD_PLATFORM "HTML5")
ENDIF()

SET(PLATFORM_LIST HTML5)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

SET(CMAKE_CXX_FLAGS "${EPIC_BUILD_FLAGS} -fdiagnostics-show-option -fno-rtti -fno-exceptions -ffast-math -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -pedantic -Weverything -Wno-c++11-long-long -Wno-padded -Wno-reserved-id-macro -Wno-float-equal -Wno-sign-conversion -Wno-covered-switch-default -Wno-documentation-unknown-command -Wno-weak-vtables -Wno-missing-prototypes -Wno-unused-local-typedef -Wno-float-conversion -Wno-global-constructors -Wno-missing-variable-declarations -Wno-exit-time-destructors -Wno-unused-macros -Wno-undef -Wno-c++11-extra-semi -Wno-c++11-extensions -Wno-non-virtual-dtor -Wno-unknown-pragmas -Wno-old-style-cast -Wno-extra-semi -Wno-cast-align -Wno-documentation -Wno-shadow -Wno-conversion -Wno-newline-eof -Wno-header-hygiene -Wno-switch-enum -Wno-undefined-reinterpret-cast -Wno-variadic-macros -Wno-gnu-zero-variadic-macro-arguments -Wno-overloaded-virtual -Wno-dynamic-class-memaccess -Wno-nested-anon-types -Wno-invalid-offsetof -Wno-reorder -Wno-local-type-template-args -Wno-unreachable-code -Wno-unreachable-code-return -Wno-format-pedantic -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-unused-variable -Wno-format-nonliteral -Wno-shift-sign-overflow -Wno-comma -Wno-expansion-to-defined -Wno-undefined-func-template -Wno-weak-template-vtables -Wno-double-promotion -Wno-nonportable-include-path -Wno-disabled-macro-expansion -Wno-missing-noreturn")
SET(CMAKE_STATIC_LIBRARY_PREFIX "")

SET(APEX_HTML5_COMPILE_DEFS _CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE)
SET(APEX_HTML5_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG;PX_CHECKED;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER;PX_PROFILE)
SET(APEX_HTML5_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER;PX_ENABLE_CHECKED_ASSERTS)
SET(APEX_HTML5_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE;PHYSX_PROFILE_SDK;PX_SUPPORT_VISUAL_DEBUGGER)
SET(APEX_HTML5_RELEASE_COMPILE_DEFS NDEBUG;APEX_SHIPPING)

SET(CMAKE_DEBUG_POSTFIX $ENV{LIB_SUFFIX})
SET(CMAKE_PROFILE_POSTFIX $ENV{LIB_SUFFIX})
SET(CMAKE_CHECKED_POSTFIX $ENV{LIB_SUFFIX})
SET(CMAKE_RELEASE_POSTFIX $ENV{LIB_SUFFIX})

ADD_DEFINITIONS(-DPX_SIMD_DISABLED)

# Add PhysX as a dependency so that we can use project references
ADD_SUBDIRECTORY(${PHYSX_ROOT_DIR}/Source/compiler/cmake/html5 "${CMAKE_CURRENT_BINARY_DIR}/physx_bin")

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

