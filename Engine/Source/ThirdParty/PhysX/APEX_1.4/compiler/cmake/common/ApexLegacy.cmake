#
# Build APEX_Legacy common
#


ADD_LIBRARY(APEX_Legacy ${APEX_LEGACY_LIBTYPE} 
	${APEX_LEGACY_PLATFORM_SOURCE_FILES}
	
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p10.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p11.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p12.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p13.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p14.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p15.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p16.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p17.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p18.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p3.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p4.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p5.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p6.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p7.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p8.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingActorParam_0p9.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p10.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p11.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p12.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p13.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p14.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p3.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p4.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p5.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p6.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p7.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p8.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingAssetParameters_0p9.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedParam_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedParam_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedParam_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedPhysX3Param_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedPhysX3Param_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedPhysX3Param_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedPhysX3Param_0p3.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingCookedPhysX3Param_0p4.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingDebugRenderParams_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingGraphicalLodParameters_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingGraphicalLodParameters_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingGraphicalLodParameters_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingGraphicalLodParameters_0p3.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingGraphicalLodParameters_0p4.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p10.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p11.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p12.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p13.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p14.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p3.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p4.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p5.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p6.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p7.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p8.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters_0p9.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingModuleParameters_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p1.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p10.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p2.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p3.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p4.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p5.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p6.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p7.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p8.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters_0p9.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ClothingPreviewParam_0p0.cpp
	${CLOTHING_SOURCE_DIR}/src/autogen/ModuleClothingLegacy.cpp
	${COMMON_SOURCE_DIR}/src/autogen/ConvexHullParameters_0p0.cpp
	${COMMON_SOURCE_DIR}/src/autogen/ConvexHullParameters_0p1.cpp
	${COMMON_SOURCE_DIR}/src/autogen/DebugColorParams_0p0.cpp
	${COMMON_SOURCE_DIR}/src/autogen/DebugRenderParams_0p1.cpp
	${COMMON_SOURCE_DIR}/src/autogen/ModuleCommonLegacy.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/CachedOverlaps_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/CachedOverlaps_0p1.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorChunks_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p1.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p10.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p11.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p12.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p13.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p14.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p15.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p16.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p17.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p18.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p19.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p2.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p20.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p21.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p22.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p23.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p24.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p25.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p26.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p27.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p28.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p29.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p3.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p30.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p31.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p4.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p5.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p6.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p7.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p8.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorParam_0p9.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorState_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorState_0p1.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorState_0p2.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleActorState_0p3.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetCollisionDataSet_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p1.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p10.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p11.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p12.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p13.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p14.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p15.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p16.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p17.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p18.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p19.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p2.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p20.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p21.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p22.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p23.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p24.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p25.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p3.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p4.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p5.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p6.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p7.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p8.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleAssetParameters_0p9.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleDebugRenderParams_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleModuleParameters_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleModuleParameters_0p1.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleModuleParameters_0p2.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructibleModuleParameters_0p3.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/DestructiblePreviewParam_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/MeshCookedCollisionStreamsAtScale_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/MeshCookedCollisionStream_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/ModuleDestructibleLegacy.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/SurfaceTraceParameters_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/SurfaceTraceSetParameters_0p0.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/SurfaceTraceSetParameters_0p1.cpp
	${DESTRUCTIBLE_SOURCE_DIR}/src/autogen/SurfaceTraceSetParameters_0p2.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x1_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x2_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x3_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x4_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x4_0p1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x1_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x2_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x3_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x4_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x1_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x2_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x3_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x4_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x1_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x2_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x3_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x4_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/ModuleFrameworkLegacy.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/RenderMeshAssetParameters_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/SubmeshParameters_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/SubmeshParameters_0p1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/SurfaceBufferParameters_0p1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/VertexBufferParameters_0p0.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/VertexBufferParameters_0p1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/VertexFormatParameters_0p0.cpp

	${LEGACY_SOURCE_DIR}/src/ModuleLegacy.cpp
	
)



TARGET_INCLUDE_DIRECTORIES(APEX_Legacy 
	PRIVATE ${APEX_LEGACY_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3

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


	PRIVATE ${CLOTHING_SOURCE_DIR}/include
	PRIVATE ${CLOTHING_SOURCE_DIR}/include/autogen
	PRIVATE ${CLOTHING_SOURCE_DIR}/public
	
	PRIVATE ${APEX_MODULE_DIR}/clothing/include
	PRIVATE ${APEX_MODULE_DIR}/clothing/include/autogen

	PRIVATE ${COMMON_SOURCE_DIR}/include
	PRIVATE ${COMMON_SOURCE_DIR}/include/autogen
	PRIVATE ${COMMON_SOURCE_DIR}/public
	
	PRIVATE ${APEX_MODULE_DIR}/common/include
	
	PRIVATE ${DESTRUCTIBLE_SOURCE_DIR}/include
	PRIVATE ${DESTRUCTIBLE_SOURCE_DIR}/include/autogen
	PRIVATE ${DESTRUCTIBLE_SOURCE_DIR}/public

	PRIVATE ${FRAMEWORK_SOURCE_DIR}/include
	PRIVATE ${FRAMEWORK_SOURCE_DIR}/include/autogen
	PRIVATE ${FRAMEWORK_SOURCE_DIR}/public

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

TARGET_COMPILE_DEFINITIONS(APEX_Legacy 
	PRIVATE ${APEX_LEGACY_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(APEX_Legacy PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "APEX_Legacy${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "APEX_Legacy${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "APEX_Legacy${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "APEX_Legacy${CMAKE_RELEASE_POSTFIX}"
)