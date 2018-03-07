#
# Build ApexShared common
#

ADD_LIBRARY(ApexShared STATIC 
	${APEXSHARED_PLATFORM_SOURCE_FILES}
	
	${SHARED_SOURCE_DIR}/general/floatmath/src/FloatMath.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/AutoGeometry.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/ConvexDecomposition.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/ConvexHull.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgAABBPolygonSoup.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgConvexHull3d.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgGoogol.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgIntersections.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgMatrix.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgMeshEffect.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgPolygonSoupBuilder.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgPolyhedra.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgQuaternion.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgSmallDeterminant.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgSphere.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgTree.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/dgTypes.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/HACD.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/MergeHulls.cpp
	${SHARED_SOURCE_DIR}/general/HACD/src/WuQuantizer.cpp
	${SHARED_SOURCE_DIR}/general/PairFilter/src/PairFilter.cpp
	${SHARED_SOURCE_DIR}/general/stan_hull/src/StanHull.cpp
	${SHARED_SOURCE_DIR}/internal/src/PvdNxParamSerializer.cpp
	${SHARED_SOURCE_DIR}/internal/src/authoring/ApexCSG.cpp
	${SHARED_SOURCE_DIR}/internal/src/authoring/ApexCSGHull.cpp
	${SHARED_SOURCE_DIR}/internal/src/authoring/ApexCSGMeshCleaning.cpp
	${SHARED_SOURCE_DIR}/internal/src/authoring/Cutout.cpp
	${SHARED_SOURCE_DIR}/internal/src/authoring/Fracturing.cpp
	
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(ApexShared 
	PRIVATE ${APEXSHARED_PLATFORM_INCLUDES}
	
	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/destructible
	
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
	PRIVATE ${APEX_MODULE_DIR}/destructible/public
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public
	
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/floatmath/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/HACD/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/HACD/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/HACD/src
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/PairFilter/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/PairFilter/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared/inparser/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/stan_hull/include
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

TARGET_COMPILE_DEFINITIONS(ApexShared 
	PRIVATE ${APEXSHARED_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(ApexShared PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "ApexShared${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "ApexShared${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "ApexShared${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "ApexShared${CMAKE_RELEASE_POSTFIX}"
)