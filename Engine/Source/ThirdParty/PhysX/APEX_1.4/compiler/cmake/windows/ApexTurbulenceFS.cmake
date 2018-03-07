#
# Build APEX_TurbulenceFS
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/turbulencefs)

FIND_PACKAGE(nvToolsExt REQUIRED)

# Set the CUDA includes, etc.
INCLUDE(../common/SetupCUDA.cmake)

# Add the special snowflake includes
CUDA_INCLUDE_DIRECTORIES(
	${AM_SOURCE_DIR}/cuda/include
)

SET(APEX_TURBULENCEFS_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS}
	${CUDA_INCLUDE_DIRS}

	${PXSHARED_ROOT_DIR}/include/cudamanager
	${PROJECT_SOURCE_DIR}/../../../common/include/windows

	${PHYSX_ROOT_DIR}/Source/PhysXGpu/include
	${PHYSX_ROOT_DIR}/Include/gpu
)

SET(APEX_TURBULENCEFS_COMPILE_DEFS

	# Common to all configurations
	${APEX_WINDOWS_COMPILE_DEFS};ENABLE_TEST=0;EXCLUDE_CUDA=1;EXCLUDE_PARTICLES=1;_USRDLL;

	$<$<CONFIG:debug>:${APEX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common ApexTurbulenceFS.cmake
INCLUDE(../common/ApexTurbulenceFS.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_TurbulenceFS ${NVTOOLSEXT_LIBRARIES} ${CUDA_CUDA_LIBRARY} DelayImp.lib
	ApexCommon ApexShared NvParameterized PsFastXml PxCudaContextManager PxFoundation PxPvdSDK PhysXCommon
)

