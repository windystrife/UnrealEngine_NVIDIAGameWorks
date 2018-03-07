#
# Build APEX_Destructible
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/destructible)

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(APEX_DESTRUCTIBLE_LIBTYPE SHARED)

SET(APEX_DESTRUCTIBLE_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}

	${PXSHARED_ROOT_DIR}/include/cudamanager
	${PROJECT_SOURCE_DIR}/../../../common/include/windows

	${PHYSX_ROOT_DIR}/Source/PhysXGpu/include
	${PHYSX_ROOT_DIR}/Include/gpu
)

SET(APEX_DESTRUCTIBLE_PLATFORM_SOURCE_FILES
	${PROJECT_ROOT_DIR}/compiler/resource/apex.rc
)

SET(APEX_DESTRUCTIBLE_COMPILE_DEFS
	# Common to all configurations

	${APEX_WINDOWS_COMPILE_DEFS};ENABLE_TEST=0;EXCLUDE_CUDA=1;EXCLUDE_PARTICLES=1;_USRDLL;

	$<$<CONFIG:debug>:${APEX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common ApexDestructible.cmake
INCLUDE(../common/ApexDestructible.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_Destructible PUBLIC ${NVTOOLSEXT_LIBRARIES} ApexCommon ApexShared NvParameterized PsFastXml PxFoundation PxPvdSDK PhysXExtensions)




