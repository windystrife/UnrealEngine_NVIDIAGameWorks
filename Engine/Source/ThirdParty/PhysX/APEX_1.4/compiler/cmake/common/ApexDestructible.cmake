#
# Build APEX_Destructible common
#

ADD_LIBRARY(APEX_Destructible ${APEX_DESTRUCTIBLE_LIBTYPE}
	${APEX_DESTRUCTIBLE_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/fracture/Actor.cpp
	${AM_SOURCE_DIR}/fracture/Compound.cpp
	${AM_SOURCE_DIR}/fracture/CompoundCreator.cpp
	${AM_SOURCE_DIR}/fracture/CompoundGeometry.cpp
	${AM_SOURCE_DIR}/fracture/Convex.cpp
	${AM_SOURCE_DIR}/fracture/Delaunay2d.cpp
	${AM_SOURCE_DIR}/fracture/Delaunay3d.cpp
	${AM_SOURCE_DIR}/fracture/FracturePattern.cpp
	${AM_SOURCE_DIR}/fracture/IceBoxPruning.cpp
	${AM_SOURCE_DIR}/fracture/IceRevisitedRadix.cpp
	${AM_SOURCE_DIR}/fracture/IslandDetector.cpp
	${AM_SOURCE_DIR}/fracture/Mesh.cpp
	${AM_SOURCE_DIR}/fracture/MeshClipper.cpp
	${AM_SOURCE_DIR}/fracture/PolygonTriangulator.cpp
	${AM_SOURCE_DIR}/fracture/Renderable.cpp
	${AM_SOURCE_DIR}/fracture/SimScene.cpp
	${AM_SOURCE_DIR}/fracture/Core/ActorBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/CompoundBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/CompoundCreatorBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/CompoundGeometryBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/ConvexBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/Delaunay2dBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/Delaunay3dBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/FracturePatternBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/IceBoxPruningBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/IceRevisitedRadixBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/IslandDetectorBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/MeshBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/MeshClipperBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/PolygonTriangulatorBase.cpp
	${AM_SOURCE_DIR}/fracture/Core/SimSceneBase.cpp
	${AM_SOURCE_DIR}/src/DestructibleActorImpl.cpp
	${AM_SOURCE_DIR}/src/DestructibleActorJointImpl.cpp
	${AM_SOURCE_DIR}/src/DestructibleAssetImpl.cpp
	${AM_SOURCE_DIR}/src/DestructiblePreviewImpl.cpp
	${AM_SOURCE_DIR}/src/DestructibleRenderableImpl.cpp
	${AM_SOURCE_DIR}/src/DestructibleScene.cpp
	${AM_SOURCE_DIR}/src/DestructibleSceneSyncParams.cpp
	${AM_SOURCE_DIR}/src/DestructibleStructure.cpp
	${AM_SOURCE_DIR}/src/DestructibleStructureStressSolver.cpp
	${AM_SOURCE_DIR}/src/ModuleDestructibleImpl.cpp
	${AM_SOURCE_DIR}/src/autogen/CachedOverlaps.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleActorChunks.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleActorParam.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleActorState.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleAssetCollisionDataSet.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleAssetParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructibleModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/DestructiblePreviewParam.cpp
	${AM_SOURCE_DIR}/src/autogen/MeshCookedCollisionStream.cpp
	${AM_SOURCE_DIR}/src/autogen/MeshCookedCollisionStreamsAtScale.cpp
	${AM_SOURCE_DIR}/src/autogen/SurfaceTraceParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/SurfaceTraceSetParameters.cpp
	
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
	
)



TARGET_INCLUDE_DIRECTORIES(APEX_Destructible
	PRIVATE ${APEX_DESTRUCTIBLE_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/destructible
	PRIVATE ${APEX_MODULE_DIR}/../include/emitter
	
	PRIVATE ${PXSHARED_ROOT_DIR}/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/filebuf			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/foundation			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/pvd			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/task			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/filebuf/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/src

	PRIVATE ${APEX_MODULE_DIR}/../common/include
	PRIVATE ${APEX_MODULE_DIR}/../common/include/autogen
	
	PRIVATE ${APEX_MODULE_DIR}/../framework/include
	PRIVATE ${APEX_MODULE_DIR}/../framework/include/autogen
	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3

	PRIVATE ${APEX_MODULE_DIR}/common/include

	PRIVATE ${AM_SOURCE_DIR}/fracture
	PRIVATE ${AM_SOURCE_DIR}/fracture/Core
	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public
	
	PRIVATE ${APEX_MODULE_DIR}/emitter/public
	
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

TARGET_COMPILE_DEFINITIONS(APEX_Destructible 
	PRIVATE ${APEX_DESTRUCTIBLE_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(APEX_Destructible PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "APEX_Destructible${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "APEX_Destructible${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "APEX_Destructible${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "APEX_Destructible${CMAKE_RELEASE_POSTFIX}"
)