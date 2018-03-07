#
# Build LowLevelParticles
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/LowLevelParticles/src)

SET(LOWLEVELPARTICLES_PLATFORM_INCLUDES
	$ENV{EMSCRIPTEN}/system/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(LOWLEVELPARTICLES_COMPILE_DEFS 

	${PHYSX_HTML5_COMPILE_DEFS}

	$<$<CONFIG:debug>:${PHYSX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_HTML5_RELEASE_COMPILE_DEFS};>
)

# include common low level particles settings
INCLUDE(../common/LowLevelParticles.cmake)
