#
# Build PhysXExtensions
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXExtensions/src)

SET(PHYSXEXTENSIONS_PLATFORM_INCLUDES
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src/tvos
)

SET(PHYSXEXTENSIONS_PLATFORM_SRC_FILES

)


# Use generator expressions to set config specific preprocessor definitions
SET(PHYSXEXTENSIONS_COMPILE_DEFS

	# NOTE: PX_BUILD_NUMBER - probably not good!
	# Common to all configurations
	${PHYSX_TVOS_COMPILE_DEFS};PX_BUILD_NUMBER=0;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${PHYSX_TVOS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_TVOS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_TVOS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_TVOS_RELEASE_COMPILE_DEFS};>
)

# include common PhysXExtensions settings
INCLUDE(../common/PhysXExtensions.cmake)

# enable -fPIC so we can link static libs with the editor
SET_TARGET_PROPERTIES(PhysXExtensions PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
