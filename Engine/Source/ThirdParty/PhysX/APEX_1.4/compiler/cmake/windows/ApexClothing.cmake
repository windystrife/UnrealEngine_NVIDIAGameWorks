#
# Build APEX_Clothing
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/clothing)

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(APEX_CLOTHING_LIBTYPE SHARED)

SET(APEX_CLOTHING_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}

	${PXSHARED_ROOT_DIR}/src/cudamanager/include
	${PROJECT_SOURCE_DIR}/../../../common/include/windows
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows
	${AM_SOURCE_DIR}/include/windows

	${PHYSX_ROOT_DIR}/Source/PhysXGpu/include
	${PHYSX_ROOT_DIR}/Include/gpu
)

SET(APEX_CLOTHING_PLATFORM_SOURCE_FILES

	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/avx/SwSolveConstraints.cpp
	${PROJECT_ROOT_DIR}/compiler/resource/apex.rc
)

SET_SOURCE_FILES_PROPERTIES(${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/avx/SwSolveConstraints.cpp PROPERTIES COMPILE_FLAGS /arch:AVX)


SET(APEX_CLOTHING_COMPILE_DEFS

	# Common to all configurations
	${APEX_WINDOWS_COMPILE_DEFS};ENABLE_TEST=0;EXCLUDE_CUDA=1;EXCLUDE_PARTICLES=1;_USRDLL;

	$<$<CONFIG:debug>:${APEX_WINDOWS_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${APEX_WINDOWS_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${APEX_WINDOWS_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${APEX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common ApexClothing.cmake
INCLUDE(../common/ApexClothing.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_Clothing ${NVTOOLSEXT_LIBRARIES} ApexCommon ApexShared NvParameterized PsFastXml PxFoundation PxPvdSDK PhysXCommon)

