#
# Build APEX_ForceField common
#

CUDA_ADD_LIBRARY(APEX_ForceField STATIC 
	${APEX_FORCEFIELD_PLATFORM_SOURCE_FILES}

	${AM_SOURCE_DIR}/src/ForceFieldActorImpl.cpp
	${AM_SOURCE_DIR}/src/ForceFieldAssetImpl.cpp
	${AM_SOURCE_DIR}/src/ForceFieldAssetPreviewImpl.cpp
	${AM_SOURCE_DIR}/src/ForceFieldFSActor.cpp
	${AM_SOURCE_DIR}/src/ForceFieldScene.cpp
	${AM_SOURCE_DIR}/src/ModuleForceFieldImpl.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldAssetPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldFalloffParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldModuleParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ForceFieldNoiseParams.cpp
	${AM_SOURCE_DIR}/src/autogen/GenericForceFieldKernelParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RadialForceFieldKernelParams.cpp
	
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
)


TARGET_INCLUDE_DIRECTORIES(APEX_ForceField 
	PRIVATE ${APEX_FORCEFIELD_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/fieldsampler
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

	PRIVATE ${APEX_MODULE_DIR}/common/include

	PRIVATE ${APEX_MODULE_DIR}/fieldsampler/public
	
	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public
	
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

TARGET_COMPILE_DEFINITIONS(APEX_ForceField 
	PRIVATE ${APEX_FORCEFIELD_COMPILE_DEFS}
)

