#
# Build PhysXCooking common
#

SET(PHYSX_COOKING_HEADERS
	${PHYSX_ROOT_DIR}/Include/cooking/PxBVH33MidphaseDesc.h
	${PHYSX_ROOT_DIR}/Include/cooking/PxBVH34MidphaseDesc.h
	${PHYSX_ROOT_DIR}/Include/cooking/Pxc.h
	${PHYSX_ROOT_DIR}/Include/cooking/PxConvexMeshDesc.h
	${PHYSX_ROOT_DIR}/Include/cooking/PxCooking.h
	${PHYSX_ROOT_DIR}/Include/cooking/PxMidphaseDesc.h
	${PHYSX_ROOT_DIR}/Include/cooking/PxTriangleMeshDesc.h	
)
SOURCE_GROUP(include FILES ${PHYSX_COOKING_HEADERS})

SET(PHYSX_COOKING_SOURCE
	${LL_SOURCE_DIR}/Adjacencies.cpp
	${LL_SOURCE_DIR}/Cooking.cpp
	${LL_SOURCE_DIR}/CookingUtils.cpp
	${LL_SOURCE_DIR}/EdgeList.cpp
	${LL_SOURCE_DIR}/MeshCleaner.cpp
	${LL_SOURCE_DIR}/Quantizer.cpp	
	${LL_SOURCE_DIR}/Adjacencies.h
	${LL_SOURCE_DIR}/Cooking.h
	${LL_SOURCE_DIR}/CookingUtils.h
	${LL_SOURCE_DIR}/EdgeList.h
	${LL_SOURCE_DIR}/MeshCleaner.h
	${LL_SOURCE_DIR}/Quantizer.h
)
SOURCE_GROUP(src FILES ${PHYSX_COOKING_SOURCE})

SET(PHYSX_COOKING_MESH_SOURCE
	${LL_SOURCE_DIR}/mesh/HeightFieldCooking.cpp
	${LL_SOURCE_DIR}/mesh/RTreeCooking.cpp
	${LL_SOURCE_DIR}/mesh/TriangleMeshBuilder.cpp	
	${LL_SOURCE_DIR}/mesh/GrbTriangleMeshCooking.cpp
	${LL_SOURCE_DIR}/mesh/GrbTriangleMeshCooking.h
	${LL_SOURCE_DIR}/mesh/HeightFieldCooking.h
	${LL_SOURCE_DIR}/mesh/QuickSelect.h
	${LL_SOURCE_DIR}/mesh/RTreeCooking.h
	${LL_SOURCE_DIR}/mesh/TriangleMeshBuilder.h
)
SOURCE_GROUP(src\\mesh FILES ${PHYSX_COOKING_MESH_SOURCE})


SET(PHYSX_COOKING_CONVEX_SOURCE
	${LL_SOURCE_DIR}/convex/BigConvexDataBuilder.cpp
	${LL_SOURCE_DIR}/convex/ConvexHullBuilder.cpp
	${LL_SOURCE_DIR}/convex/ConvexHullLib.cpp
	${LL_SOURCE_DIR}/convex/ConvexHullUtils.cpp
	${LL_SOURCE_DIR}/convex/ConvexMeshBuilder.cpp
	${LL_SOURCE_DIR}/convex/ConvexPolygonsBuilder.cpp
	${LL_SOURCE_DIR}/convex/InflationConvexHullLib.cpp
	${LL_SOURCE_DIR}/convex/QuickHullConvexHullLib.cpp
	${LL_SOURCE_DIR}/convex/VolumeIntegration.cpp
	${LL_SOURCE_DIR}/convex/BigConvexDataBuilder.h
	${LL_SOURCE_DIR}/convex/ConvexHullBuilder.h
	${LL_SOURCE_DIR}/convex/ConvexHullLib.h
	${LL_SOURCE_DIR}/convex/ConvexHullUtils.h
	${LL_SOURCE_DIR}/convex/ConvexMeshBuilder.h
	${LL_SOURCE_DIR}/convex/ConvexPolygonsBuilder.h
	${LL_SOURCE_DIR}/convex/InflationConvexHullLib.h
	${LL_SOURCE_DIR}/convex/QuickHullConvexHullLib.h
	${LL_SOURCE_DIR}/convex/VolumeIntegration.h
)
SOURCE_GROUP(src\\convex FILES ${PHYSX_COOKING_CONVEX_SOURCE})

ADD_LIBRARY(PhysXCooking ${PHYSXCOOKING_LIBTYPE} 
	${PHYSX_COOKING_HEADERS}
	
	${PHYSX_COOKING_SOURCE}
	${PHYSX_COOKING_MESH_SOURCE}
	${PHYSX_COOKING_CONVEX_SOURCE}
	
	${PHYSXCOOKING_PLATFORM_SRC_FILES}
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(PhysXCooking 
	PRIVATE ${PHYSXCOOKING_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/cloth
	PRIVATE ${PHYSX_ROOT_DIR}/Include/cooking
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/common
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/distance
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/sweep
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/gjk
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/intersection
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/pcm
	PUBLIC ${PHYSX_SOURCE_DIR}/GeomUtils/src/ccd

	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXCooking/src
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXCooking/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXCooking/src/convex

	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXExtensions/src

	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
	
)


# NOTE: Doesn't work! FIXME



TARGET_LINK_LIBRARIES(PhysXCooking PUBLIC PhysXCommon PhysXExtensions PxFoundation)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(PhysXCooking 
	PRIVATE ${PHYSXCOOKING_COMPILE_DEFS}
)

#TODO: Link flags

SET_TARGET_PROPERTIES(PhysXCooking PROPERTIES
	OUTPUT_NAME PhysX3Cooking
)

SET_TARGET_PROPERTIES(PhysXCooking PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "PhysX3Cooking${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "PhysX3Cooking${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "PhysX3Cooking${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "PhysX3Cooking${CMAKE_RELEASE_POSTFIX}"
)