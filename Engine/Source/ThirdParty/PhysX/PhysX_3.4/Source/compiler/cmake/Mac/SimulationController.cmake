#
# Build SimulationController
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/SimulationController/src)

SET(SIMULATIONCONTROLLER_PLATFORM_INCLUDES
	${PHYSX_SOURCE_DIR}/Common/src/mac
	${PHYSX_SOURCE_DIR}/LowLevel/mac/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(SIMULATIONCONTROLLER_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_MAC_COMPILE_DEFS};PX_PHYSX_STATIC_LIB

	$<$<CONFIG:debug>:${PHYSX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_MAC_RELEASE_COMPILE_DEFS};>
)


# include common SimulationController settings
INCLUDE(../common/SimulationController.cmake)

# enable -fPIC so we can link static libs with the editor
SET_TARGET_PROPERTIES(SimulationController PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
