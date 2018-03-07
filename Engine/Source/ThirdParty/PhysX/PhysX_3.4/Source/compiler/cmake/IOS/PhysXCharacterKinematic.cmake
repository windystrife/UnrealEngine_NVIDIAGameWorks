#
# Build PhysXCharacterKinematic
#

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/PhysXCharacterKinematic/src)

# Use generator expressions to set config specific preprocessor definitions
SET(PHYSXCHARACTERKINEMATICS_COMPILE_DEFS 

	# Common to all configurations
	${PHYSX_IOS_COMPILE_DEFS};PX_PHYSX_CHARACTER_EXPORTS;PX_PHYSX_CORE_EXPORTS;PX_FOUNDATION_DLL=1;

	$<$<CONFIG:debug>:${PHYSX_IOS_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${PHYSX_IOS_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${PHYSX_IOS_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${PHYSX_IOS_RELEASE_COMPILE_DEFS};>
)

# include common PhysXCharacterKinematic settings
INCLUDE(../common/PhysXCharacterKinematic.cmake)

# enable -fPIC so we can link static libs with the editor
SET_TARGET_PROPERTIES(PhysXCharacterKinematic PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
