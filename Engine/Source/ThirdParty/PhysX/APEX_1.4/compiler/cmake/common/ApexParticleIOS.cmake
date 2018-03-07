#
# Build APEX_ParticleIOS common
#

CUDA_ADD_LIBRARY(APEX_ParticleIOS STATIC 
	${APEX_PARTICLEIOS_PLATFORM_SOURCE_FILES}

	${APEX_MODULE_DIR}/common/src/CudaModuleScene.cpp
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
	
	${AM_SOURCE_DIR}/src/ModuleParticleIosImpl.cpp
	${AM_SOURCE_DIR}/src/ParticleIosActorCPU.cpp
	${AM_SOURCE_DIR}/src/ParticleIosActorGPU.cpp
	${AM_SOURCE_DIR}/src/ParticleIosActorImpl.cpp
	${AM_SOURCE_DIR}/src/ParticleIosAssetImpl.cpp
	${AM_SOURCE_DIR}/src/ParticleIosScene.cpp
	${AM_SOURCE_DIR}/src/autogen/FluidParticleSystemParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ParticleIosAssetParam.cpp
	${AM_SOURCE_DIR}/src/autogen/ParticleIosDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ParticleIosModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/SimpleParticleSystemParams.cpp
	${AM_SOURCE_DIR}/test/src/ParticleIosTestActor.cpp

	${AM_SOURCE_DIR}/cuda/compact.cu
	${AM_SOURCE_DIR}/cuda/histogram.cu
	${AM_SOURCE_DIR}/cuda/reduce.cu
	${AM_SOURCE_DIR}/cuda/scan.cu
	${AM_SOURCE_DIR}/cuda/simulate.cu
	
)

TARGET_INCLUDE_DIRECTORIES(APEX_ParticleIOS 
	PRIVATE ${APEX_PARTICLEIOS_PLATFORM_INCLUDES}
	
	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/pxparticleios
	PRIVATE ${APEX_MODULE_DIR}/../include/iofx
	PRIVATE ${APEX_MODULE_DIR}/../include/fieldsampler
	
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

	PRIVATE ${APEX_MODULE_DIR}/common/include

	PRIVATE ${APEX_MODULE_DIR}/fieldsampler/public
	PRIVATE ${APEX_MODULE_DIR}/iofx/public
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/include
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/public
	PRIVATE ${APEX_MODULE_DIR}/pxparticleios/test/include
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public

	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared/inparser/include
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

TARGET_COMPILE_DEFINITIONS(APEX_ParticleIOS 
	PRIVATE ${APEX_PARTICLEIOS_COMPILE_DEFS}
)
