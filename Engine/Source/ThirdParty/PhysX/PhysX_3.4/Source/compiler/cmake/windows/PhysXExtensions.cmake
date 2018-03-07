#
# Build PhysXExtensions
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXExtensions/src)

SET(PHYSXEXTENSIONS_PLATFORM_INCLUDES
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src/windows
)

SET(PHYSXEXTENSIONS_PLATFORM_SRC_FILES

)


# Use generator expressions to set config specific preprocessor definitions
SET(PHYSXEXTENSIONS_COMPILE_DEFS

	# NOTE: PX_BUILD_NUMBER - probably not good!
	# Common to all configurations
	${PHYSX_WINDOWS_COMPILE_DEFS};PX_BUILD_NUMBER=0;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${PHYSX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common PhysXExtensions settings
INCLUDE(../common/PhysXExtensions.cmake)
