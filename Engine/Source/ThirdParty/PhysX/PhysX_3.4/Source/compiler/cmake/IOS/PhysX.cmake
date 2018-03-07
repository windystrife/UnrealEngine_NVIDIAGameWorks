#
# Build PhysX (PROJECT not SOLUTION)
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(PX_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysX/src)
SET(MD_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXMetaData)

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(PHYSX_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}
)

SET(PHYSX_PLATFORM_SRC_FILES
	${PX_SOURCE_DIR}/gpu/NpPhysicsGpu.cpp
	${PX_SOURCE_DIR}/gpu/PxGpu.cpp
	${PX_SOURCE_DIR}/gpu/PxParticleDeviceExclusive.cpp
	${PX_SOURCE_DIR}/gpu/PxParticleGpu.cpp
	${PX_SOURCE_DIR}/gpu/PxPhysXGpuModuleLoader.cpp
	${PX_SOURCE_DIR}/gpu/PxPhysXIndicatorDeviceExclusive.cpp
)


# Use generator expressions to set config specific preprocessor definitions
SET(PHYSX_COMPILE_DEFS
	# Common to all configurations
	${PHYSX_IOS_COMPILE_DEFS};PX_PHYSX_CORE_EXPORTS

	$<$<CONFIG:debug>:${PHYSX_IOS_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${PHYSX_IOS_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${PHYSX_IOS_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${PHYSX_IOS_RELEASE_COMPILE_DEFS};>
)

SET(PHYSX_LIBTYPE STATIC)

# include common PhysX settings
INCLUDE(../common/PhysX.cmake)

SET_TARGET_PROPERTIES(PhysX PROPERTIES 
	LINK_FLAGS_DEBUG ""
	LINK_FLAGS_CHECKED ""
	LINK_FLAGS_PROFILE ""
	LINK_FLAGS_RELEASE ""
)

TARGET_LINK_LIBRARIES(PhysX PUBLIC LowLevel LowLevelAABB LowLevelCloth LowLevelDynamics LowLevelParticles PhysXCommon PxFoundation PxPvdSDK PxTask SceneQuery SimulationController)

# enable -fPIC so we can link static libs with the editor
SET_TARGET_PROPERTIES(PhysX PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
