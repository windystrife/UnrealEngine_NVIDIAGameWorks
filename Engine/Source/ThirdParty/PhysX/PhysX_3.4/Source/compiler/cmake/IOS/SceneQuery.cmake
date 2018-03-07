#
# Build SceneQuery
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/SceneQuery/src)

SET(SCENEQUERY_PLATFORM_INCLUDES
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src/ios
)

# Use generator expressions to set config specific preprocessor definitions
SET(SCENEQUERY_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_IOS_COMPILE_DEFS};PX_PHYSX_STATIC_LIB

	$<$<CONFIG:debug>:${PHYSX_IOS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_IOS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_IOS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_IOS_RELEASE_COMPILE_DEFS};>
)

# include common SceneQuery settings
INCLUDE(../common/SceneQuery.cmake)

# enable -fPIC so we can link static libs with the editor
SET_TARGET_PROPERTIES(SceneQuery PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
