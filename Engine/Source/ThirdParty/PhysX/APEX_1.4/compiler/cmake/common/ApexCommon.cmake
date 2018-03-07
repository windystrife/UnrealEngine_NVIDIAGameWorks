#
# Build ApexCommon common
#

ADD_LIBRARY(ApexCommon STATIC 
	${APEXCOMMON_PLATFORM_SOURCE_FILES}
	
	${COMMON_SOURCE_DIR}/src/ApexActor.cpp
	${COMMON_SOURCE_DIR}/src/ApexAssetAuthoring.cpp
	${COMMON_SOURCE_DIR}/src/ApexAssetTracker.cpp
	${COMMON_SOURCE_DIR}/src/ApexCollision.cpp
	${COMMON_SOURCE_DIR}/src/ApexContext.cpp
	${COMMON_SOURCE_DIR}/src/ApexCudaProfile.cpp
	${COMMON_SOURCE_DIR}/src/ApexCudaTest.cpp
	${COMMON_SOURCE_DIR}/src/ApexGeneralizedCubeTemplates.cpp
	${COMMON_SOURCE_DIR}/src/ApexGeneralizedMarchingCubes.cpp
	${COMMON_SOURCE_DIR}/src/ApexIsoMesh.cpp
	${COMMON_SOURCE_DIR}/src/ApexMeshContractor.cpp
	${COMMON_SOURCE_DIR}/src/ApexMeshHash.cpp
	${COMMON_SOURCE_DIR}/src/ApexPreview.cpp
	${COMMON_SOURCE_DIR}/src/ApexPvdClient.cpp
	${COMMON_SOURCE_DIR}/src/ApexQuadricSimplifier.cpp
	${COMMON_SOURCE_DIR}/src/ApexResource.cpp
	${COMMON_SOURCE_DIR}/src/ApexRWLockable.cpp
	${COMMON_SOURCE_DIR}/src/ApexSDKCachedDataImpl.cpp
	${COMMON_SOURCE_DIR}/src/ApexSDKHelpers.cpp
	${COMMON_SOURCE_DIR}/src/ApexShape.cpp
	${COMMON_SOURCE_DIR}/src/ApexSharedUtils.cpp
	${COMMON_SOURCE_DIR}/src/ApexSubdivider.cpp
	${COMMON_SOURCE_DIR}/src/ApexTetrahedralizer.cpp
	${COMMON_SOURCE_DIR}/src/CurveImpl.cpp
	${COMMON_SOURCE_DIR}/src/ModuleBase.cpp
	${COMMON_SOURCE_DIR}/src/ModuleUpdateLoader.cpp
	${COMMON_SOURCE_DIR}/src/PVDParameterizedHandler.cpp
	${COMMON_SOURCE_DIR}/src/ReadCheck.cpp
	${COMMON_SOURCE_DIR}/src/Spline.cpp
	${COMMON_SOURCE_DIR}/src/variable_oscillator.cpp
	${COMMON_SOURCE_DIR}/src/WriteCheck.cpp
	${COMMON_SOURCE_DIR}/src/autogen/ConvexHullParameters.cpp
	${COMMON_SOURCE_DIR}/src/autogen/DebugColorParams.cpp
	${COMMON_SOURCE_DIR}/src/autogen/DebugRenderParams.cpp
	
)


TARGET_INCLUDE_DIRECTORIES(ApexCommon 
	PRIVATE ${APEXCOMMON_PLATFORM_INCLUDES}

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

	PRIVATE ${AM_SOURCE_DIR}/include
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public
	
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/general
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

TARGET_COMPILE_DEFINITIONS(ApexCommon 
	PRIVATE ${APEXCOMMON_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(ApexCommon PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "ApexCommon${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "ApexCommon${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "ApexCommon${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "ApexCommon${CMAKE_RELEASE_POSTFIX}"
)