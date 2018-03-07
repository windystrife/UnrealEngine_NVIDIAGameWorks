#
# Build SceneQuery
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/SceneQuery/src)

SET(SCENEQUERY_PLATFORM_INCLUDES
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src/windows
)

# Use generator expressions to set config specific preprocessor definitions
SET(SCENEQUERY_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_WINDOWS_COMPILE_DEFS};PX_PHYSX_STATIC_LIB

	$<$<CONFIG:debug>:${PHYSX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

SET(SCENEQUERY_LIBTYPE STATIC)

# include common SceneQuery settings
INCLUDE(../common/SceneQuery.cmake)
