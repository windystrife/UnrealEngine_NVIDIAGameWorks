#
# Build LowLevel common
#

SET(LL_API_DIR ${LL_SOURCE_DIR}/API/)
SET(LL_API_HEADERS	
	${LL_API_DIR}/include/PxsMaterialCore.h
	${LL_API_DIR}/include/PxsMaterialManager.h
	${LL_API_DIR}/include/PxvConfig.h
	${LL_API_DIR}/include/PxvContext.h
	${LL_API_DIR}/include/PxvDynamics.h
	${LL_API_DIR}/include/PxvGeometry.h
	${LL_API_DIR}/include/PxvGlobals.h
	${LL_API_DIR}/include/PxvManager.h
	${LL_API_DIR}/include/PxvSimStats.h
)
SOURCE_GROUP("API Includes" FILES ${LL_API_HEADERS})

SET(LL_API_SOURCE
	${LL_API_DIR}/src/px_globals.cpp
)
SOURCE_GROUP("API Source" FILES ${LL_API_SOURCE})

SET(LL_COMMON_DIR ${LL_SOURCE_DIR}/common/)
SET(LL_COMMON_COLLISION_HEADERS	
	${LL_COMMON_DIR}/include/collision/PxcContactMethodImpl.h
)
SOURCE_GROUP("Common Includes\\collision" FILES ${LL_COMMON_COLLISION_HEADERS})
SET(LL_COMMON_PIPELINE_HEADERS		
	${LL_COMMON_DIR}/include/pipeline/PxcCCDStateStreamPair.h
	${LL_COMMON_DIR}/include/pipeline/PxcConstraintBlockStream.h
	${LL_COMMON_DIR}/include/pipeline/PxcContactCache.h
	${LL_COMMON_DIR}/include/pipeline/PxcMaterialMethodImpl.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpBatch.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpCache.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpCacheStreamPair.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpContactPrepShared.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpMemBlockPool.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpThreadContext.h
	${LL_COMMON_DIR}/include/pipeline/PxcNpWorkUnit.h
	${LL_COMMON_DIR}/include/pipeline/PxcRigidBody.h
)
SOURCE_GROUP("Common Includes\\pipeline" FILES ${LL_COMMON_PIPELINE_HEADERS})
SET(LL_COMMON_UTILS_HEADERS	
	${LL_COMMON_DIR}/include/utils/PxcScratchAllocator.h
	${LL_COMMON_DIR}/include/utils/PxcThreadCoherentCache.h
)
SOURCE_GROUP("Common Includes\\utils" FILES ${LL_COMMON_UTILS_HEADERS})

SET(LL_COMMON_COLLISION_SOURCE
	${LL_COMMON_DIR}/src/collision/PxcContact.cpp	
)
SOURCE_GROUP("Common Source\\collision" FILES ${LL_COMMON_COLLISION_SOURCE})
SET(LL_COMMON_PIPELINE_SOURCE	
	${LL_COMMON_DIR}/src/pipeline/PxcContactCache.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcContactMethodImpl.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcMaterialHeightField.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcMaterialMesh.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcMaterialMethodImpl.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcMaterialShape.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcNpBatch.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcNpCacheStreamPair.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcNpContactPrepShared.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcNpMemBlockPool.cpp
	${LL_COMMON_DIR}/src/pipeline/PxcNpThreadContext.cpp
)
SOURCE_GROUP("Common Source\\pipeline" FILES ${LL_COMMON_PIPELINE_SOURCE})

SET(LL_SOFTWARE_DIR ${LL_SOURCE_DIR}/software/)
SET(LL_SOFTWARE_HEADERS		
	${LL_SOFTWARE_DIR}/include/PxsBodySim.h
	${LL_SOFTWARE_DIR}/include/PxsCCD.h
	${LL_SOFTWARE_DIR}/include/PxsContactManager.h
	${LL_SOFTWARE_DIR}/include/PxsContactManagerState.h
	${LL_SOFTWARE_DIR}/include/PxsContext.h
	${LL_SOFTWARE_DIR}/include/PxsDefaultMemoryManager.h
	${LL_SOFTWARE_DIR}/include/PxsHeapMemoryAllocator.h
	${LL_SOFTWARE_DIR}/include/PxsIncrementalConstraintPartitioning.h
	${LL_SOFTWARE_DIR}/include/PxsIslandManagerTypes.h
	${LL_SOFTWARE_DIR}/include/PxsIslandSim.h
	${LL_SOFTWARE_DIR}/include/PxsKernelWrangler.h
	${LL_SOFTWARE_DIR}/include/PxsMaterialCombiner.h
	${LL_SOFTWARE_DIR}/include/PxsMemoryManager.h
	${LL_SOFTWARE_DIR}/include/PxsNphaseImplementationContext.h
	${LL_SOFTWARE_DIR}/include/PxsRigidBody.h
	${LL_SOFTWARE_DIR}/include/PxsShapeSim.h
	${LL_SOFTWARE_DIR}/include/PxsSimpleIslandManager.h
	${LL_SOFTWARE_DIR}/include/PxsSimulationController.h
	${LL_SOFTWARE_DIR}/include/PxsTransformCache.h
	${LL_SOFTWARE_DIR}/include/PxvNphaseImplementationContext.h
)
SOURCE_GROUP("Software Includes" FILES ${LL_SOFTWARE_HEADERS})
SET(LL_SOFTWARE_SOURCE			
	${LL_SOFTWARE_DIR}/src/PxsCCD.cpp
	${LL_SOFTWARE_DIR}/src/PxsContactManager.cpp
	${LL_SOFTWARE_DIR}/src/PxsContext.cpp
	${LL_SOFTWARE_DIR}/src/PxsDefaultMemoryManager.cpp
	${LL_SOFTWARE_DIR}/src/PxsIslandSim.cpp
	${LL_SOFTWARE_DIR}/src/PxsMaterialCombiner.cpp
	${LL_SOFTWARE_DIR}/src/PxsNphaseImplementationContext.cpp
	${LL_SOFTWARE_DIR}/src/PxsSimpleIslandManager.cpp
)
SOURCE_GROUP("Software Source" FILES ${LL_SOFTWARE_SOURCE})

ADD_LIBRARY(LowLevel ${LOWLEVEL_LIBTYPE}
	${LL_API_HEADERS}
	${LL_API_SOURCE}
	
	${LL_COMMON_COLLISION_HEADERS}
	${LL_COMMON_COLLISION_SOURCE}
	
	${LL_COMMON_PIPELINE_HEADERS}
	${LL_COMMON_PIPELINE_SOURCE}
	
	${LL_COMMON_UTILS_HEADERS}
	
	${LL_SOFTWARE_HEADERS}
	${LL_SOFTWARE_SOURCE}	
)

TARGET_INCLUDE_DIRECTORIES(LowLevel 
	PRIVATE ${LOWLEVEL_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/include
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/common
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/distance
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/sweep
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/gjk
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/intersection
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/pcm
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/ccd
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/collision
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/software/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/include

)

TARGET_COMPILE_DEFINITIONS(LowLevel 
	PRIVATE ${LOWLEVEL_COMPILE_DEFS}
)

IF(NOT ${LOWLEVEL_LIBTYPE} STREQUAL "OBJECT")
	SET_TARGET_PROPERTIES(LowLevel PROPERTIES 
		COMPILE_PDB_NAME_DEBUG "LowLevel${CMAKE_DEBUG_POSTFIX}"
		COMPILE_PDB_NAME_CHECKED "LowLevel${CMAKE_CHECKED_POSTFIX}"
		COMPILE_PDB_NAME_PROFILE "LowLevel${CMAKE_PROFILE_POSTFIX}"
		COMPILE_PDB_NAME_RELEASE "LowLevel${CMAKE_RELEASE_POSTFIX}"
	)
ENDIF()