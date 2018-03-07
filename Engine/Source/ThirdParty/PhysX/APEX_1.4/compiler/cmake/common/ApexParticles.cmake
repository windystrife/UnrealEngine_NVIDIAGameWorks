#
# Build APEX_Particles common
#

CUDA_ADD_LIBRARY(APEX_Particles SHARED 
	${APEX_PARTICLES_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/src/EffectPackageActorImpl.cpp
	${AM_SOURCE_DIR}/src/EffectPackageAssetImpl.cpp
	${AM_SOURCE_DIR}/src/ModuleParticlesImpl.cpp
	${AM_SOURCE_DIR}/src/ParticlesScene.cpp
	${AM_SOURCE_DIR}/src/autogen/AttractorFieldSamplerData.cpp
	${AM_SOURCE_DIR}/src/autogen/AttractorFieldSamplerEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageData.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageDatabaseParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageEmitterDatabaseParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageFieldSamplerDatabaseParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageGraphicsMaterialsParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageIOFXDatabaseParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EffectPackageIOSDatabaseParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterData.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/FlameEmitterData.cpp
	${AM_SOURCE_DIR}/src/autogen/FlameEmitterEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldData.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/GraphicsEffectData.cpp
	${AM_SOURCE_DIR}/src/autogen/GraphicsMaterialData.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourceData.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourceEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/JetFieldSamplerData.cpp
	${AM_SOURCE_DIR}/src/autogen/JetFieldSamplerEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/NoiseFieldSamplerData.cpp
	${AM_SOURCE_DIR}/src/autogen/NoiseFieldSamplerEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/ParticlesDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ParticleSimulationData.cpp
	${AM_SOURCE_DIR}/src/autogen/ParticlesModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/RigidBodyEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/SubstanceSourceData.cpp
	${AM_SOURCE_DIR}/src/autogen/SubstanceSourceEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFieldSamplerData.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFieldSamplerEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/VelocitySourceData.cpp
	${AM_SOURCE_DIR}/src/autogen/VelocitySourceEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/VolumeRenderMaterialData.cpp
	${AM_SOURCE_DIR}/src/autogen/VortexFieldSamplerData.cpp
	${AM_SOURCE_DIR}/src/autogen/VortexFieldSamplerEffect.cpp
	${AM_SOURCE_DIR}/src/autogen/WindFieldSamplerData.cpp
	${AM_SOURCE_DIR}/src/autogen/WindFieldSamplerEffect.cpp
	
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
)


TARGET_INCLUDE_DIRECTORIES(APEX_Particles 
	PRIVATE ${APEX_PARTICLES_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/particles
	PRIVATE ${APEX_MODULE_DIR}/../include/emitter
	PRIVATE ${APEX_MODULE_DIR}/../include/basicios
	PRIVATE ${APEX_MODULE_DIR}/../include/basicfs
	PRIVATE ${APEX_MODULE_DIR}/../include/pxparticleios
	PRIVATE ${APEX_MODULE_DIR}/../include/iofx
	PRIVATE ${APEX_MODULE_DIR}/../include/turbulencefs
	PRIVATE ${APEX_MODULE_DIR}/../include/forcefield
	
	PRIVATE ${PXSHARED_ROOT_DIR}/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/filebuf			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/foundation			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/pvd			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/task			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/filebuf/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include			

	PRIVATE ${APEX_MODULE_DIR}/../common/include
	PRIVATE ${APEX_MODULE_DIR}/../common/include/autogen

	PRIVATE ${APEX_MODULE_DIR}/../framework/include
	PRIVATE ${APEX_MODULE_DIR}/../framework/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3

	PRIVATE ${APEX_MODULE_DIR}/basicfs/include
	PRIVATE ${APEX_MODULE_DIR}/basicfs/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/basicfs/public
	
	PRIVATE ${APEX_MODULE_DIR}/basicios/include
	PRIVATE ${APEX_MODULE_DIR}/basicios/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/basicios/public
	
	PRIVATE ${APEX_MODULE_DIR}/common/include
	
	PRIVATE ${APEX_MODULE_DIR}/emitter/include
	PRIVATE ${APEX_MODULE_DIR}/emitter/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/emitter/public
	
	PRIVATE ${APEX_MODULE_DIR}/forcefield/include
	PRIVATE ${APEX_MODULE_DIR}/forcefield/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/forcefield/public
	
	PRIVATE ${APEX_MODULE_DIR}/iofx/include
	PRIVATE ${APEX_MODULE_DIR}/iofx/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/iofx/public
	
	PRIVATE ${APEX_MODULE_DIR}/particles/include
	PRIVATE ${APEX_MODULE_DIR}/particles/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/particles/public
	
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/include
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/public
	
	PRIVATE ${APEX_MODULE_DIR}/turbulencefs/include
	PRIVATE ${APEX_MODULE_DIR}/turbulencefs/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/turbulencefs/public
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public

	PRIVATE ${APEX_MODULE_DIR}/../shared/external/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/floatmath/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/HACD/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/HACD/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/PairFilter/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared/inparser/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/stan_hull/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/internal/include
	
	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/characterdynamic
	PRIVATE ${PHYSX_ROOT_DIR}/Include/characterkinematic
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/cooking
	PRIVATE ${PHYSX_ROOT_DIR}/Include/deformable
	PRIVATE ${PHYSX_ROOT_DIR}/Include/extensions
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/particles
	PRIVATE ${PHYSX_ROOT_DIR}/Include/vehicle
	PRIVATE ${PHYSX_ROOT_DIR}/Source/GeomUtils/headers

)

TARGET_COMPILE_DEFINITIONS(APEX_Particles PRIVATE ${APEX_PARTICLES_COMPILE_DEFS} 
)
