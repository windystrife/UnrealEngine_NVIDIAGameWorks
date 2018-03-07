#
# Build APEX_Particles
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/particles)


# Use generator expressions to set config specific preprocessor definitions
SET(APEX_PARTICLES_COMPILE_DEFS 
	# Common to all configurations

	${APEX_MAC_COMPILE_DEFS};_LIB;PX_PHYSX_STATIC_LIB;

	$<$<CONFIG:debug>:${APEX_MAC_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_MAC_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_MAC_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_MAC_RELEASE_COMPILE_DEFS};>
)

# include common ApexParticles.cmake
INCLUDE(../common/ApexParticles.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_Particles ${NVTOOLSEXT_LIBRARIES} ${CUDA_CUDA_LIBRARY} DelayImp.lib 
	PhysXCommon PhysX PhysXExtensions PhysXVehicle 
	APEX_BasicFS APEX_BasicIOS APEX_Emitter APEX_FieldSampler APEX_ForceField APEX_IOFX APEX_ParticleIOS
	ApexCommon ApexFramework ApexShared
	NvParameterized PsFastXml PxCudaContextManager PxFoundation PxPvdSDK
)
