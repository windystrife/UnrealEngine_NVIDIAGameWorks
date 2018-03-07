#
# Build ApexCommon
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/common)

SET(COMMON_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../common)

SET(APEXCOMMON_PLATFORM_INCLUDES
	${PROJECT_SOURCE_DIR}/../../../common/include/mac
)

# Use generator expressions to set config specific preprocessor definitions
SET(APEXCOMMON_COMPILE_DEFS 

	# Common to all configurations
	${APEX_MAC_COMPILE_DEFS};_LIB;__Mac__;Mac;NX_USE_SDK_STATICLIBS;NX_FOUNDATION_STATICLIB;NV_PARAMETERIZED_HIDE_DESCRIPTIONS=1;ENABLE_TEST=0

	$<$<CONFIG:debug>:${APEX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_MAC_RELEASE_COMPILE_DEFS};>
)

# include common ApexCommon.cmake
INCLUDE(../common/ApexCommon.cmake)

