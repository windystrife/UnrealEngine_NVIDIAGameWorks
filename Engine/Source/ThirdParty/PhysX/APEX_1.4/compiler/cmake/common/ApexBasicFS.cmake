#
# Build APEX_BasicFS common
#


CUDA_ADD_LIBRARY(APEX_BasicFS STATIC 
	${GENERATED_CUDA_FILES}
	${APEX_BASICFS_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/src/AttractorFSActorImpl.cpp
	${AM_SOURCE_DIR}/src/AttractorFSAsset.cpp
	${AM_SOURCE_DIR}/src/AttractorFSAssetPreview.cpp
	${AM_SOURCE_DIR}/src/BasicFSActor.cpp
	${AM_SOURCE_DIR}/src/BasicFSAssetImpl.cpp
	${AM_SOURCE_DIR}/src/BasicFSScene.cpp
	${AM_SOURCE_DIR}/src/JetFSActorImpl.cpp
	${AM_SOURCE_DIR}/src/JetFSAsset.cpp
	${AM_SOURCE_DIR}/src/JetFSAssetPreview.cpp
	${AM_SOURCE_DIR}/src/ModuleBasicFSImpl.cpp
	${AM_SOURCE_DIR}/src/NoiseFSActorImpl.cpp
	${AM_SOURCE_DIR}/src/NoiseFSAsset.cpp
	${AM_SOURCE_DIR}/src/NoiseFSAssetPreview.cpp
	${AM_SOURCE_DIR}/src/VortexFSActorImpl.cpp
	${AM_SOURCE_DIR}/src/VortexFSAsset.cpp
	${AM_SOURCE_DIR}/src/VortexFSAssetPreview.cpp
	${AM_SOURCE_DIR}/src/WindFSActorImpl.cpp
	${AM_SOURCE_DIR}/src/WindFSAsset.cpp
	${AM_SOURCE_DIR}/src/WindFSAssetPreview.cpp
	${AM_SOURCE_DIR}/src/autogen/AttractorFSActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/AttractorFSAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/AttractorFSPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/BasicFSDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/BasicFSModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/JetFSActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/JetFSAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/JetFSPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/NoiseFSActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/NoiseFSAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/NoiseFSPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/VortexFSActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/VortexFSAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/VortexFSPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/WindFSActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/WindFSAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/WindFSPreviewParams.cpp

	${APEX_MODULE_DIR}/common/src/CudaModuleScene.cpp
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
)

# Target specific compile options


TARGET_INCLUDE_DIRECTORIES(APEX_BasicFS 
	PRIVATE ${APEX_BASICFS_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/basicfs
	PRIVATE ${APEX_MODULE_DIR}/../include/fieldsampler
	
	PRIVATE ${APEX_MODULE_DIR}/common/include

	PRIVATE ${APEX_MODULE_DIR}/basicfs/public
	PRIVATE ${APEX_MODULE_DIR}/basicfs/include
	PRIVATE ${APEX_MODULE_DIR}/basicfs/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/fieldsampler/public
	
	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/cooking
	PRIVATE ${PHYSX_ROOT_DIR}/Include/extensions
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/deformable
	PRIVATE ${PHYSX_ROOT_DIR}/Include/particles
	PRIVATE ${PHYSX_ROOT_DIR}/Include/characterkinematic
	PRIVATE ${PHYSX_ROOT_DIR}/Include/characterdynamic
	PRIVATE ${PHYSX_ROOT_DIR}/Include/vehicle
	PRIVATE ${PHYSX_ROOT_DIR}/Source/GeomUtils/headers
	
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/include/filebuf
	PRIVATE ${PXSHARED_ROOT_DIR}/include/foundation
	PRIVATE ${PXSHARED_ROOT_DIR}/include/task
	PRIVATE ${PXSHARED_ROOT_DIR}/include/pvd
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/filebuf/include

	PRIVATE ${APEX_MODULE_DIR}/../shared/general/general
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared/inparser/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/internal/include

	
	PRIVATE ${APEX_MODULE_DIR}/../common/include
	PRIVATE ${APEX_MODULE_DIR}/../common/include/autogen
	
	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../framework/include
	PRIVATE ${APEX_MODULE_DIR}/../framework/include/autogen

	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include

	
)



TARGET_COMPILE_DEFINITIONS(APEX_BasicFS 
	PRIVATE ${APEX_BASICFS_COMPILE_DEFS}
)

#TODO: CUDA compilation

