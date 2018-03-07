#
# Build APEX_BasicIOS common
#

CUDA_ADD_LIBRARY(APEX_BasicIOS STATIC 
	${APEX_BASICIOS_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/src/BasicIosActorCPU.cpp
	${AM_SOURCE_DIR}/src/BasicIosActorGPU.cpp
	${AM_SOURCE_DIR}/src/BasicIosActorImpl.cpp
	${AM_SOURCE_DIR}/src/BasicIosAssetImpl.cpp
	${AM_SOURCE_DIR}/src/BasicIosScene.cpp
	${AM_SOURCE_DIR}/src/ModuleBasicIosImpl.cpp
	${AM_SOURCE_DIR}/src/autogen/BasicIOSAssetParam.cpp
	${AM_SOURCE_DIR}/src/autogen/BasicIosDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/BasicIosModuleParameters.cpp
	${AM_SOURCE_DIR}/test/src/BasicIosTestActor.cpp
	${AM_SOURCE_DIR}/test/src/BasicIosTestImpl.cpp
	${AM_SOURCE_DIR}/test/src/BasicIosTestScene.cpp
	

	${APEX_MODULE_DIR}/common/src/CudaModuleScene.cpp
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
)

# Target specific compile options


TARGET_INCLUDE_DIRECTORIES(APEX_BasicIOS 
	PRIVATE ${APEX_BASICIOS_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/basicios
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

	PRIVATE ${APEX_MODULE_DIR}/common/include

	PRIVATE ${APEX_MODULE_DIR}/../framework/include
	PRIVATE ${APEX_MODULE_DIR}/../framework/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3

	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public
	PRIVATE ${AM_SOURCE_DIR}/test/include
	PRIVATE ${AM_SOURCE_DIR}/test/public	
	
	PRIVATE ${APEX_MODULE_DIR}/fieldsampler/public
	
	PRIVATE ${APEX_MODULE_DIR}/iofx/public
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public
	
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/general
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

TARGET_COMPILE_DEFINITIONS(APEX_BasicIOS 
	PRIVATE ${APEX_BASICIOS_COMPILE_DEFS}
)

# TODO: CUDA compile


