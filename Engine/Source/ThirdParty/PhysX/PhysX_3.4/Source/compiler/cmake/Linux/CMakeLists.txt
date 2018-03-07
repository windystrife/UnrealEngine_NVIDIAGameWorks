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
	IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	  # using Clang	  
	  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -Weverything -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-float-equal -Wno-padded -Wno-weak-vtables -Wno-cast-align -Wno-conversion -Wno-missing-noreturn -Wno-missing-variable-declarations -Wno-shift-sign-overflow -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-unreachable-code -Wno-unused-macros -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-deprecated -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-reserved-id-macro -Wno-c++98-compat-pedantic -Wno-unused-local-typedef -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-func-template -Wno-format-nonliteral -Wno-implicit-fallthrough -Wno-undefined-reinterpret-cast -Wno-disabled-macro-expansion -Wno-zero-as-null-pointer-constant -Wno-unused-template -Wno-shadow -Wno-shadow-field -Wno-cast-qual -Wno-undef")
	ELSEIF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	  # using GCC
	  SET(LIBPATH_SUFFIX "x64")
	  SET(CMAKE_CXX_FLAGS "-Werror -m64 -fPIC -msse2 -mfpmath=sse -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden -Wall -Wextra -fno-strict-aliasing -fdiagnostics-show-option -Wno-invalid-offsetof -Wno-uninitialized -Wno-missing-field-initializers")	  
	ENDIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")	
ELSEIF(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "arm-unknown-linux-gnueabihf")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -mfpu=neon -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -Weverything -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-float-equal -Wno-padded -Wno-weak-vtables -Wno-cast-align -Wno-conversion -Wno-missing-noreturn -Wno-missing-variable-declarations -Wno-shift-sign-overflow -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-unreachable-code -Wno-unused-macros -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-deprecated -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-reserved-id-macro -Wno-c++98-compat-pedantic -Wno-unused-local-typedef -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-reinterpret-cast -Wno-undefined-func-template -Wno-double-promotion -Wno-zero-as-null-pointer-constant -Wno-unused-template -Wno-shadow -Wno-shadow-field -Wno-cast-qual -Wno-undef")
ELSEIF(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "aarch64-unknown-linux-gnueabi")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -Weverything -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-float-equal -Wno-padded -Wno-weak-vtables -Wno-cast-align -Wno-conversion -Wno-missing-noreturn -Wno-missing-variable-declarations -Wno-shift-sign-overflow -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-unreachable-code -Wno-unused-macros -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-deprecated -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-reserved-id-macro -Wno-c++98-compat-pedantic -Wno-unused-local-typedef -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-reinterpret-cast -Wno-undefined-func-template -Wno-double-promotion -Wno-zero-as-null-pointer-constant -Wno-unused-template -Wno-shadow -Wno-shadow-field -Wno-cast-qual -Wno-undef")
ELSEIF(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "i686-unknown-linux-gnu")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Xclang -malign-double -fpack-struct=16 -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -Werror -ferror-limit=0 -Wall -Wextra -fstrict-aliasing -Wstrict-aliasing=2 -Weverything -Wno-documentation-deprecated-sync -Wno-documentation-unknown-command -Wno-float-equal -Wno-padded -Wno-weak-vtables -Wno-cast-align -Wno-conversion -Wno-missing-noreturn -Wno-missing-variable-declarations -Wno-shift-sign-overflow -Wno-covered-switch-default -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-unreachable-code -Wno-unused-macros -Wno-unused-member-function -Wno-used-but-marked-unused -Wno-weak-template-vtables -Wno-deprecated -Wno-non-virtual-dtor -Wno-invalid-noreturn -Wno-return-type-c-linkage -Wno-reserved-id-macro -Wno-c++98-compat-pedantic -Wno-unused-local-typedef -Wno-old-style-cast -Wno-newline-eof -Wno-unused-private-field -Wno-undefined-reinterpret-cast -Wno-undefined-func-template -Wno-double-promotion -Wno-zero-as-null-pointer-constant -Wno-unused-template -Wno-shadow -Wno-shadow-field -Wno-cast-qual -Wno-undef")
ELSE(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-unknown-linux-gnu" OR ${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-linux-gnu")
	MESSAGE(FATAL_ERROR "Unknown CMAKE_LIBRARY_ARCHITECTURE ${CMAKE_LIBRARY_ARCHITECTURE}")
ENDIF(${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-unknown-linux-gnu" OR ${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "x86_64-linux-gnu")


# Build debug info for all configurations
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -gdwarf-3")
SET(CMAKE_CXX_FLAGS_CHECKED "-O3 -g -gdwarf-3")
SET(CMAKE_CXX_FLAGS_PROFILE "-O3 -g -gdwarf-3")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -gdwarf-3")

# Controls PX_NVTX for all projects on linux
SET(PHYSX_LINUX_ENABLE_NVTX 0)

IF(DEFINED GENERATE_GPU_PROJECTS)
SET(PHYSX_LINUX_COMPILE_DEFS )
FIND_PACKAGE(CUDA 8 REQUIRED)
ENDIF()
SET(PHYSX_LINUX_DEBUG_COMPILE_DEFS _DEBUG;PX_DEBUG=1;PX_CHECKED=1;PX_NVTX=${PHYSX_LINUX_ENABLE_NVTX};PX_SUPPORT_PVD=1)
SET(PHYSX_LINUX_CHECKED_COMPILE_DEFS NDEBUG;PX_CHECKED=1;PX_NVTX=${PHYSX_LINUX_ENABLE_NVTX};PX_SUPPORT_PVD=1)
SET(PHYSX_LINUX_PROFILE_COMPILE_DEFS NDEBUG;PX_PROFILE=1;PX_NVTX=${PHYSX_LINUX_ENABLE_NVTX};PX_SUPPORT_PVD=1)
SET(PHYSX_LINUX_RELEASE_COMPILE_DEFS NDEBUG;PX_SUPPORT_PVD=0)

	
IF(DEFINED LIBPATH_SUFFIX)
	SET(CMAKE_DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}_${LIBPATH_SUFFIX}")
	SET(CMAKE_PROFILE_POSTFIX "${CMAKE_PROFILE_POSTFIX}_${LIBPATH_SUFFIX}")
	SET(CMAKE_CHECKED_POSTFIX "${CMAKE_CHECKED_POSTFIX}_${LIBPATH_SUFFIX}")
	SET(CMAKE_RELEASE_POSTFIX "${CMAKE_RELEASE_POSTFIX}_${LIBPATH_SUFFIX}")
ENDIF()

# Include the PxShared-exports.cmake file that PxShared created. There's other ways to do this, but there's a bunch of issues with it in a CI environment
#  such as - we don't want to be installing any built artifacts on this server, etc. So for now we hack it.

# Add PxShared as a dependency so that we can use project references
ADD_SUBDIRECTORY(${PXSHARED_ROOT_DIR}/src/compiler/cmake/Linux "${CMAKE_CURRENT_BINARY_DIR}/pxshared_bin")

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
IF(DEFINED GENERATE_GPU_PROJECTS)
		INCLUDE(PhysXBroadphaseGpu.cmake)
		INCLUDE(PhysXClothGpu.cmake)
		INCLUDE(PhysXCommonGpu.cmake)
		INCLUDE(PhysXNarrowphaseGpu.cmake)
		INCLUDE(PhysXParticlesGpu.cmake)
		INCLUDE(PhysXSimulationControllerGpu.cmake)
		INCLUDE(PhysXSolverGpu.cmake)
		INCLUDE(PhysXGpu.cmake)
ENDIF()
