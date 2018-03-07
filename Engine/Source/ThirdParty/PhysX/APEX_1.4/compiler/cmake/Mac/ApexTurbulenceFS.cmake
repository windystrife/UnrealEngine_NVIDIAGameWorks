#
# Build APEX_TurbulenceFS
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/turbulencefs)


# Use generator expressions to set config specific preprocessor definitions
SET(APEX_TURBULENCEFS_COMPILE_DEFS 

	# Common to all configurations
	${APEX_MAC_COMPILE_DEFS};_LIB;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${APEX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_MAC_RELEASE_COMPILE_DEFS};>
)

# include common ApexTurbulenceFS.cmake
INCLUDE(../common/ApexTurbulenceFS.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_TurbulenceFS ${NVTOOLSEXT_LIBRARIES} ${CUDA_CUDA_LIBRARY} DelayImp.lib
	ApexCommon ApexShared NvParameterized PsFastXml PxCudaContextManager PxFoundation PxPvdSDK PhysXCommon
)
