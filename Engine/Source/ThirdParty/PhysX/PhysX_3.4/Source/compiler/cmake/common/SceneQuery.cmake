#
# Build SceneQuery common
#

SET(SCENEQUERY_BASE_DIR ${PHYSX_ROOT_DIR}/Source/SceneQuery)
SET(SCENEQUERY_HEADERS		
	${SCENEQUERY_BASE_DIR}/include/SqPruner.h
	${SCENEQUERY_BASE_DIR}/include/SqPrunerMergeData.h
	${SCENEQUERY_BASE_DIR}/include/SqPruningStructure.h
	${SCENEQUERY_BASE_DIR}/include/SqSceneQueryManager.h	
)
SOURCE_GROUP(include FILES ${SCENEQUERY_HEADERS})

SET(SCENEQUERY_SOURCE			
	${SCENEQUERY_BASE_DIR}/src/SqAABBPruner.cpp
	${SCENEQUERY_BASE_DIR}/src/SqAABBPruner.h
	${SCENEQUERY_BASE_DIR}/src/SqAABBTree.cpp
	${SCENEQUERY_BASE_DIR}/src/SqAABBTree.h
	${SCENEQUERY_BASE_DIR}/src/SqAABBTreeQuery.h
	${SCENEQUERY_BASE_DIR}/src/SqAABBTreeUpdateMap.cpp
	${SCENEQUERY_BASE_DIR}/src/SqAABBTreeUpdateMap.h
	${SCENEQUERY_BASE_DIR}/src/SqBounds.cpp
	${SCENEQUERY_BASE_DIR}/src/SqBounds.h
	${SCENEQUERY_BASE_DIR}/src/SqBucketPruner.cpp
	${SCENEQUERY_BASE_DIR}/src/SqBucketPruner.h
	${SCENEQUERY_BASE_DIR}/src/SqExtendedBucketPruner.cpp
	${SCENEQUERY_BASE_DIR}/src/SqExtendedBucketPruner.h
	${SCENEQUERY_BASE_DIR}/src/SqMetaData.cpp
	${SCENEQUERY_BASE_DIR}/src/SqPrunerTestsSIMD.h
	${SCENEQUERY_BASE_DIR}/src/SqPruningPool.cpp
	${SCENEQUERY_BASE_DIR}/src/SqPruningPool.h
	${SCENEQUERY_BASE_DIR}/src/SqPruningStructure.cpp
	${SCENEQUERY_BASE_DIR}/src/SqSceneQueryManager.cpp
	${SCENEQUERY_BASE_DIR}/src/SqTypedef.h
)
SOURCE_GROUP(src FILES ${SCENEQUERY_SOURCE})

ADD_LIBRARY(SceneQuery ${SCENEQUERY_LIBTYPE}
	${SCENEQUERY_HEADERS}
	${SCENEQUERY_SOURCE}
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(SceneQuery 
	PRIVATE ${SCENEQUERY_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/pvd
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
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

	PRIVATE ${PHYSX_SOURCE_DIR}/SceneQuery/include
	PRIVATE ${PHYSX_SOURCE_DIR}/SimulationController/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysX/src
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysX/src/buffering
)

# No linked libraries

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(SceneQuery 

	# Common to all configurations
	PRIVATE ${SCENEQUERY_COMPILE_DEFS}
)

IF(NOT ${SCENEQUERY_LIBTYPE} STREQUAL "OBJECT")
	SET_TARGET_PROPERTIES(SceneQuery PROPERTIES 
		COMPILE_PDB_NAME_DEBUG "SceneQuery${CMAKE_DEBUG_POSTFIX}"
		COMPILE_PDB_NAME_CHECKED "SceneQuery${CMAKE_CHECKED_POSTFIX}"
		COMPILE_PDB_NAME_PROFILE "SceneQuery${CMAKE_PROFILE_POSTFIX}"
		COMPILE_PDB_NAME_RELEASE "SceneQuery${CMAKE_RELEASE_POSTFIX}"
	)
ENDIF()
