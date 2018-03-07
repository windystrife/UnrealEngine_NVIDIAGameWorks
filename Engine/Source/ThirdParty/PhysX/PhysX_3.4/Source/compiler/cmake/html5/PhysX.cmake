#
# Build PhysX (PROJECT not SOLUTION)
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(PX_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysX/src)
SET(MD_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXMetaData)

SET(PHYSX_PLATFORM_INCLUDES
	$ENV{EMSCRIPTEN}/system/include
)

SET(PHYSX_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_HTML5_COMPILE_DEFS};

	$<$<CONFIG:debug>:${PHYSX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_HTML5_RELEASE_COMPILE_DEFS};>
)

SET(PHYSX_LIBTYPE STATIC)

# include common PhysX settings
INCLUDE(../common/PhysX.cmake)

TARGET_LINK_LIBRARIES(PhysX PUBLIC LowLevel LowLevelAABB LowLevelCloth LowLevelDynamics LowLevelParticles PhysXCommon PxFoundation PxPvdSDK PxTask SceneQuery SimulationController)

