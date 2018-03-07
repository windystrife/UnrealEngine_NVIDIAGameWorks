#
# Build APEX_Clothing common
#

ADD_LIBRARY(APEX_Clothing ${APEX_CLOTHING_LIBTYPE} 
	${APEX_CLOTHING_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/embedded/Cooking.cpp
#	${AM_SOURCE_DIR}/embedded/CreateCuFactory.cpp
	${AM_SOURCE_DIR}/embedded/ExtClothFabricCooker.cpp
	${AM_SOURCE_DIR}/embedded/ExtClothGeodesicTetherCooker.cpp
	${AM_SOURCE_DIR}/embedded/ExtClothMeshQuadifier.cpp
	${AM_SOURCE_DIR}/embedded/ExtClothSimpleTetherCooker.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/Allocator.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/Factory.cpp
	
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/PhaseConfig.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwCloth.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwClothData.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwCollision.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwFabric.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwFactory.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwInterCollision.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSelfCollision.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSolver.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSolverKernel.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/TripletScheduler.cpp
	${AM_SOURCE_DIR}/embedded/Simulation.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingActorParam.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingAssetParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingCookedParam.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingCookedPhysX3Param.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingGraphicalLodParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingMaterialLibraryParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingPhysicalMeshParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ClothingPreviewParam.cpp
	${AM_SOURCE_DIR}/src/ClothingActorData.cpp
	${AM_SOURCE_DIR}/src/ClothingActorImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingActorTasks.cpp
	${AM_SOURCE_DIR}/src/ClothingAssetAuthoringImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingAssetData.cpp
	${AM_SOURCE_DIR}/src/ClothingAssetImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingCollisionImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingCooking.cpp
#	${AM_SOURCE_DIR}/src/ClothingIsoMeshImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingPhysicalMeshImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingRenderProxyImpl.cpp
	${AM_SOURCE_DIR}/src/ClothingScene.cpp
	${AM_SOURCE_DIR}/src/CookingAbstract.cpp
	${AM_SOURCE_DIR}/src/ModuleClothingHelpers.cpp
	${AM_SOURCE_DIR}/src/ModuleClothingImpl.cpp
	${AM_SOURCE_DIR}/src/SimulationAbstract.cpp	
	${AM_SOURCE_DIR}/src/SimdMath.cpp	
	
	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(APEX_Clothing 
	PRIVATE ${APEX_CLOTHING_PLATFORM_INCLUDES}
	
	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/clothing

	PRIVATE ${PXSHARED_ROOT_DIR}/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/cudamanager
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
	
	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3
	

	PRIVATE ${AM_SOURCE_DIR}/embedded
	PRIVATE ${AM_SOURCE_DIR}/embedded/LowLevelCloth/include
	PRIVATE ${AM_SOURCE_DIR}/embedded/LowLevelCloth/src
	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public
	
	PRIVATE ${APEX_MODULE_DIR}/common/include

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

TARGET_COMPILE_DEFINITIONS(APEX_Clothing 
	PRIVATE ${APEX_CLOTHING_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(APEX_Clothing PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "APEX_Clothing${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "APEX_Clothing${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "APEX_Clothing${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "APEX_Clothing${CMAKE_RELEASE_POSTFIX}"
)