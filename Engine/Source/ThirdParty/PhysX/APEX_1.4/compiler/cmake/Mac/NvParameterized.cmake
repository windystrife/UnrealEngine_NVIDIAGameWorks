#
# Build NvParameterized
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(NVP_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../NvParameterized)
#SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/{{TARGET_MODULE_DIR}})

# Use generator expressions to set config specific preprocessor definitions
SET(NVPARAMETERIZED_COMPILE_DEFS 

	# Common to all configurations

	${APEX_MAC_COMPILE_DEFS};__Mac__;Mac;_CRT_SECURE_NO_DEPRECATE;_CRT_NONSTDC_NO_DEPRECATE;PX_FOUNDATION_DLL=0;

	$<$<CONFIG:debug>:${APEX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_MAC_RELEASE_COMPILE_DEFS};>
)

# include common NvParameterized.cmake
INCLUDE(../common/NvParameterized.cmake)