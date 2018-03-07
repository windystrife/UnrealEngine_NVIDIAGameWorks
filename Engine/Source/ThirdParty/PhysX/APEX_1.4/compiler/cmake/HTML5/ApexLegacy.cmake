#
# Build APEX_Legacy
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

# These 4 are the only ones used in the HTML5 version
SET(CLOTHING_SOURCE_DIR ${APEX_MODULE_DIR}/clothing_legacy)
SET(COMMON_SOURCE_DIR ${APEX_MODULE_DIR}/common_legacy)
SET(DESTRUCTIBLE_SOURCE_DIR ${APEX_MODULE_DIR}/destructible_legacy)
SET(FRAMEWORK_SOURCE_DIR ${APEX_MODULE_DIR}/framework_legacy)

SET(BFS_SOURCE_DIR ${APEX_MODULE_DIR}/basicfs_legacy)
SET(BIOS_SOURCE_DIR ${APEX_MODULE_DIR}/basicios_legacy)
SET(EMITTER_SOURCE_DIR ${APEX_MODULE_DIR}/emitter_legacy)
SET(FORCEFIELD_SOURCE_DIR ${APEX_MODULE_DIR}/forcefield_legacy)
SET(IOFX_SOURCE_DIR ${APEX_MODULE_DIR}/iofx_legacy)
SET(PARTICLES_SOURCE_DIR ${APEX_MODULE_DIR}/particles_legacy)
SET(PXPARTICLEIOS_SOURCE_DIR ${APEX_MODULE_DIR}/pxparticleios_legacy)
SET(TURBFS_SOURCE_DIR ${APEX_MODULE_DIR}/turbulencefs_legacy)

SET(LEGACY_SOURCE_DIR ${APEX_MODULE_DIR}/legacy)

SET(APEX_LEGACY_LIBTYPE STATIC)

SET(APEX_LEGACY_PLATFORM_INCLUDES
	${PHYSX_ROOT_DIR}/Source/PhysXGpu/include
)

# Use generator expressions to set config specific preprocessor definitions
SET(APEX_LEGACY_COMPILE_DEFS

	# Common to all configurations
	${APEX_HTML5_COMPILE_DEFS};_LIB;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${APEX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_HTML5_RELEASE_COMPILE_DEFS};>
)

# include common ApexLegacy.cmake
INCLUDE(../common/ApexLegacy.cmake)

