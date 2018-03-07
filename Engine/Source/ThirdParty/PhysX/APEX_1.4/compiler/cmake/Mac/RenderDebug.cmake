#
# Build RenderDebug
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(RD_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../shared/general/RenderDebug)

# Use generator expressions to set config specific preprocessor definitions
SET(RENDERDEBUG_COMPILE_DEFS 
	# Common to all configurations

	${APEX_MAC_COMPILE_DEFS};PX_FOUNDATION_DLL=0;__Mac__;Mac;

	$<$<CONFIG:debug>:${APEX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_MAC_RELEASE_COMPILE_DEFS};>
)

# include common RenderDebug.cmake
INCLUDE(../common/RenderDebug.cmake)
