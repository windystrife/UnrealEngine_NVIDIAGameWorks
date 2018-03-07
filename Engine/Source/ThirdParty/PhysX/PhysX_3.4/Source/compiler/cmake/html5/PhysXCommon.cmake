#
# Build PhysXCommon
#

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(COMMON_SRC_DIR ${PHYSX_SOURCE_DIR}/Common/src)
SET(GU_SOURCE_DIR ${PHYSX_SOURCE_DIR}/GeomUtils)

SET(PHYSXCOMMON_LIBTYPE STATIC)

SET(PXCOMMON_PLATFORM_INCLUDES
	$ENV{EMSCRIPTEN}/system/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(PXCOMMON_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_HTML5_COMPILE_DEFS};
	
	$<$<CONFIG:debug>:${PHYSX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_HTML5_RELEASE_COMPILE_DEFS};>
)

# include common PhysXCommon settings
INCLUDE(../common/PhysXCommon.cmake)
