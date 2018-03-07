#
# Build APEX_IOFX common
#

CUDA_ADD_LIBRARY(APEX_IOFX STATIC 
	${APEX_IOFX_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/src/IofxActorCPU.cpp
	${AM_SOURCE_DIR}/src/IofxActorImpl.cpp
	${AM_SOURCE_DIR}/src/IofxAssetImpl.cpp
	${AM_SOURCE_DIR}/src/IofxManager.cpp
	${AM_SOURCE_DIR}/src/IofxManagerGPU.cpp
	${AM_SOURCE_DIR}/src/IofxRenderDataMesh.cpp
	${AM_SOURCE_DIR}/src/IofxRenderDataSprite.cpp
	${AM_SOURCE_DIR}/src/IofxScene.cpp
	${AM_SOURCE_DIR}/src/ModifierCPU.cpp
	${AM_SOURCE_DIR}/src/ModifierGPU.cpp
	${AM_SOURCE_DIR}/src/ModifierImpl.cpp
	${AM_SOURCE_DIR}/src/ModuleIofxImpl.cpp
	${AM_SOURCE_DIR}/src/RenderVolumeImpl.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsDensityCompositeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsDensityModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsLifeCompositeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsLifeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsTemperatureCompositeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsTemperatureModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsVelocityCompositeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ColorVsVelocityModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/InitialColorModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/IofxAssetParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/IofxDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/IofxModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/MeshIofxParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/OrientAlongVelocityModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/OrientScaleAlongScreenVelocityModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RandomRotationModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RandomScaleModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RandomSubtextureModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RotationModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RotationRateModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/RotationRateVsLifeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleAlongVelocityModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleByMassModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsCameraDistance2DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsCameraDistance3DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsCameraDistanceModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsDensity2DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsDensity3DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsDensityModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsLife2DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsLife3DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsLifeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsTemperature2DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsTemperature3DModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ScaleVsTemperatureModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/SimpleScaleModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/SpriteIofxParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/SubtextureVsLifeModifierParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ViewDirectionSortingModifierParams.cpp
	
	# Tests - can exclude?
	${AM_SOURCE_DIR}/test/src/IofxTestImpl.cpp
	${AM_SOURCE_DIR}/test/src/IofxTestManager.cpp
	${AM_SOURCE_DIR}/test/src/IofxTestManagerGPU.cpp
	${AM_SOURCE_DIR}/test/src/IofxTestScene.cpp
	
	${AM_SOURCE_DIR}/cuda/actorRanges.cu
	${AM_SOURCE_DIR}/cuda/bbox.cu
	${AM_SOURCE_DIR}/cuda/migration.cu
	${AM_SOURCE_DIR}/cuda/modifier.cu
	${AM_SOURCE_DIR}/cuda/remap.cu
	${AM_SOURCE_DIR}/cuda/sort.cu
	${AM_SOURCE_DIR}/cuda/sortNew.cu
	
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(APEX_IOFX 
	PRIVATE ${APEX_IOFX_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/iofx
	
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

	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public
	PRIVATE ${AM_SOURCE_DIR}/test/include
	PRIVATE ${AM_SOURCE_DIR}/test/public
	
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

TARGET_COMPILE_DEFINITIONS(APEX_IOFX 
	PRIVATE ${APEX_IOFX_COMPILE_DEFS}
)
