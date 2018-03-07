cmake_minimum_required(VERSION 3.3)
include(../common/CMakeLists.txt)

STRING(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWERCASE)

IF(NOT DEFINED TARGET_BUILD_PLATFORM) # Not defined, default to Android
	SET(TARGET_BUILD_PLATFORM "Android")
ENDIF()

SET(PLATFORM_LIST Android)

IF (NOT ${TARGET_BUILD_PLATFORM} IN_LIST PLATFORM_LIST)
	MESSAGE(FATAL_ERROR "Invalid platform:" ${TARGET_BUILD_PLATFORM})
ENDIF()

if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++11 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -funwind-tables -fomit-frame-pointer -funswitch-loops -finline-limit=300 -fno-strict-aliasing -fstack-protector -Wno-invalid-offsetof ")
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a with NEON")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -ffast-math -fno-exceptions -ffunction-sections -funwind-tables -fomit-frame-pointer -funswitch-loops -finline-limit=300 -fno-strict-aliasing -fstack-protector -Wno-invalid-offsetof ")
elseif(${ANDROID_ABI} STREQUAL "arm64-v8a")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Wno-invalid-offsetof ")
elseif(${ANDROID_ABI} STREQUAL "x86")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Wno-invalid-offsetof -fpack-struct=8 -malign-double ")
elseif(${ANDROID_ABI} STREQUAL "x86_64")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Wno-invalid-offsetof -mstackrealign -msse3 ")
endif()

# Build debug info for all configurations
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_CXX_FLAGS_CHECKED "-O3 -g")
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

# Controls PX_NVTX for all projects on Android
SET(PHYSX_ANDROID_ENABLE_NVTX 0)

# Disable cuda and dx for all projects on Android
SET(PHYSX_ANDROID_COMPILE_DEFS __STDC_LIMIT_MACROS;)
SET(PHYSX_ANDROID_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG=1;PX_CHECKED=1;PX_NVTX=${PHYSX_ANDROID_ENABLE_NVTX};PX_SUPPORT_PVD=1)
SET(PHYSX_ANDROID_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED=1;PX_NVTX=${PHYSX_ANDROID_ENABLE_NVTX};PX_SUPPORT_PVD=1)
SET(PHYSX_ANDROID_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE=1;PX_NVTX=${PHYSX_ANDROID_ENABLE_NVTX};PX_SUPPORT_PVD=1)
SET(PHYSX_ANDROID_RELEASE_COMPILE_DEFS NDEBUG;PX_SUPPORT_PVD=0)

SET(CMAKE_DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
SET(CMAKE_PROFILE_POSTFIX "${CMAKE_PROFILE_POSTFIX}")
SET(CMAKE_CHECKED_POSTFIX "${CMAKE_CHECKED_POSTFIX}")
SET(CMAKE_RELEASE_POSTFIX "${CMAKE_RELEASE_POSTFIX}")

# Needed so FIND_PACKAGE works
SET(CMAKE_FIND_ROOT_PATH  "")

# Include the PxShared-exports.cmake file that PxShared created. There's other ways to do this, but there's a bunch of issues with it in a CI environment
#  such as - we don't want to be installing any built artifacts on this server, etc. So for now we hack it.

# Add PxShared as a dependency so that we can use project references
ADD_SUBDIRECTORY(${PXSHARED_ROOT_DIR}/src/compiler/cmake/Android "${CMAKE_CURRENT_BINARY_DIR}/pxshared_bin")

# Include all of the projects
INCLUDE(LowLevel.cmake)
INCLUDE(LowLevelAABB.cmake)
INCLUDE(LowLevelCloth.cmake)
INCLUDE(LowLevelDynamics.cmake)
INCLUDE(LowLevelParticles.cmake)
INCLUDE(PhysX.cmake)
INCLUDE(PhysXCharacterKinematic.cmake)
INCLUDE(PhysXCommon.cmake)
INCLUDE(PhysXCooking.cmake)
INCLUDE(PhysXExtensions.cmake)
INCLUDE(PhysXVehicle.cmake)
INCLUDE(SceneQuery.cmake)
INCLUDE(SimulationController.cmake)
