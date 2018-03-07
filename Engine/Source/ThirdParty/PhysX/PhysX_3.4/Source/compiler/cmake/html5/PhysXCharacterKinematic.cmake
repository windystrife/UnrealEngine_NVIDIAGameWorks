#
# Build PhysXCharacterKinematic
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXCharacterKinematic/src)

SET(PHYSXCHARACTERKINEMATICS_COMPILE_DEFS 

	# Common to all configurations

	${PHYSX_HTML5_COMPILE_DEFS}

	$<$<CONFIG:debug>:${PHYSX_HTML5_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PHYSX_HTML5_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PHYSX_HTML5_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PHYSX_HTML5_RELEASE_COMPILE_DEFS};>
)

# include common PhysXCharacterKinematic settings
INCLUDE(../common/PhysXCharacterKinematic.cmake)



