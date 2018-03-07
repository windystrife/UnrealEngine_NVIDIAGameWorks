#
# Build APEX_TurbulenceFS common
#

CUDA_ADD_LIBRARY(APEX_TurbulenceFS SHARED 
	${APEX_TURBULENCEFS_PLATFORM_SOURCE_FILES}

	${AM_SOURCE_DIR}/src/FlameEmitterActorImpl.cpp
	${AM_SOURCE_DIR}/src/FlameEmitterAssetImpl.cpp
	${AM_SOURCE_DIR}/src/FlameEmitterAssetPreviewImpl.cpp
	${AM_SOURCE_DIR}/src/HeatSourceActorImpl.cpp
	${AM_SOURCE_DIR}/src/HeatSourceAssetImpl.cpp
	${AM_SOURCE_DIR}/src/HeatSourceAssetPreviewImpl.cpp
	${AM_SOURCE_DIR}/src/ModuleTurbulenceFSImpl.cpp
	${AM_SOURCE_DIR}/src/ScalarSourceActor.cpp
	${AM_SOURCE_DIR}/src/SubstanceSourceActorImpl.cpp
	${AM_SOURCE_DIR}/src/SubstanceSourceAssetImpl.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSActorCPU.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSActorDC.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSActorGPU.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSActorImpl.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSAssetImpl.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSAssetPreviewImpl.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSOutputFieldData.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSScene.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSSceneCPU.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSSceneDC.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSSceneGPU.cpp
	${AM_SOURCE_DIR}/src/TurbulenceFSVelocityFieldData.cpp
	${AM_SOURCE_DIR}/src/VelocitySourceActorImpl.cpp
	${AM_SOURCE_DIR}/src/VelocitySourceAssetImpl.cpp
	${AM_SOURCE_DIR}/src/VelocitySourceAssetPreviewImpl.cpp
	${AM_SOURCE_DIR}/src/autogen/FlameEmitterActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/FlameEmitterAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/FlameEmitterPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourceActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourceAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourceGeomBoxParams.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourceGeomSphereParams.cpp
	${AM_SOURCE_DIR}/src/autogen/HeatSourcePreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/SubstanceSourceActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/SubstanceSourceAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFSActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFSAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFSDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFSModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/TurbulenceFSPreviewParams.cpp
	${AM_SOURCE_DIR}/src/autogen/VelocitySourceActorParams.cpp
	${AM_SOURCE_DIR}/src/autogen/VelocitySourceAssetParams.cpp
	${AM_SOURCE_DIR}/src/autogen/VelocitySourcePreviewParams.cpp
	${AM_SOURCE_DIR}/test/src/TurbulenceFSTestActor.cpp
	${AM_SOURCE_DIR}/test/src/TurbulenceFSTestImpl.cpp
	${AM_SOURCE_DIR}/test/src/TurbulenceFSTestScene.cpp

	${AM_SOURCE_DIR}/cuda/source/advection3d.cpp
	${AM_SOURCE_DIR}/cuda/source/advectionmc3d.cpp
	${AM_SOURCE_DIR}/cuda/source/boundary_condition.cpp
	${AM_SOURCE_DIR}/cuda/source/grid1ddev.cpp
	${AM_SOURCE_DIR}/cuda/source/grid1dhost.cpp
	${AM_SOURCE_DIR}/cuda/source/mgpressure3d.cpp
	${AM_SOURCE_DIR}/cuda/source/project3d.cpp
	
	# NOTE: Windows only?
	${AM_SOURCE_DIR}/cuda/kernels/advectionmc3d.cu
	${AM_SOURCE_DIR}/cuda/kernels/boundary_condition.cu
	${AM_SOURCE_DIR}/cuda/kernels/flame.cu
	${AM_SOURCE_DIR}/cuda/kernels/grid1ddev.cu
	${AM_SOURCE_DIR}/cuda/kernels/mgpressure3d.cu
	${AM_SOURCE_DIR}/cuda/kernels/project3d.cu
	${AM_SOURCE_DIR}/cuda/kernels/reduce.cu
	${AM_SOURCE_DIR}/cuda/kernels/simulation.cu
	${AM_SOURCE_DIR}/cuda/kernels/turbulencefs.cu
	
	${APEX_MODULE_DIR}/common/src/CudaModuleScene.cpp
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
	
	
)


TARGET_INCLUDE_DIRECTORIES(APEX_TurbulenceFS 
	PRIVATE ${APEX_TURBULENCEFS_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/turbulencefs
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
	
	PRIVATE ${AM_SOURCE_DIR}/cuda/include
	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public
	PRIVATE ${AM_SOURCE_DIR}/test/include
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public

	PRIVATE ${APEX_MODULE_DIR}/../shared/external/include
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

TARGET_COMPILE_DEFINITIONS(APEX_TurbulenceFS 
	PRIVATE ${APEX_TURBULENCEFS_COMPILE_DEFS}
)
