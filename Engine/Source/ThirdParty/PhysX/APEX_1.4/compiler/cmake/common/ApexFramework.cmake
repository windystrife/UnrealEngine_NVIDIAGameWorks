#
# Build ApexFramework common
#

ADD_LIBRARY(ApexFramework ${APEXFRAMEWORK_LIBTYPE}
	${APEXFRAMEWORK_PLATFORM_SOURCE_FILES}

	${FRAMEWORK_SOURCE_DIR}/src/ApexAssetPreviewScene.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexCreateSDK.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexCustomBufferIterator.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexRenderDebug.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexRenderMeshActor.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexRenderMeshAsset.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexRenderMeshAssetAuthoring.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexRenderSubmesh.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexResourceProvider.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexScene.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexSceneTasks.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexSceneUserNotify.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexSDKImpl.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexSDKVersionString.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexVertexBuffer.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ApexVertexFormat.cpp
	${FRAMEWORK_SOURCE_DIR}/src/FrameworkProfile.cpp
	${FRAMEWORK_SOURCE_DIR}/src/MirrorSceneImpl.cpp
	${FRAMEWORK_SOURCE_DIR}/src/ThreadPool.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x2.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x3.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferF32x4.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x2.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x3.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU16x4.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x2.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x3.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU32x4.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x1.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x2.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x3.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/BufferU8x4.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/RenderMeshAssetParameters.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/SubmeshParameters.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/SurfaceBufferParameters.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/VertexBufferParameters.cpp
	${FRAMEWORK_SOURCE_DIR}/src/autogen/VertexFormatParameters.cpp
	
)


TARGET_INCLUDE_DIRECTORIES(ApexFramework 
	PRIVATE ${APEXFRAMEWORK_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/include/cudamanager
	PRIVATE ${PXSHARED_ROOT_DIR}/src/cudamanager/include
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
	
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/include
	PRIVATE ${APEX_MODULE_DIR}/../NvParameterized/public
	
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/PairFilter/include
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/PairFilter/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/RenderDebug/public
	PRIVATE ${APEX_MODULE_DIR}/../shared/general/shared
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

TARGET_COMPILE_DEFINITIONS(ApexFramework 
	PRIVATE ${APEXFRAMEWORK_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(ApexFramework PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "ApexFramework${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "ApexFramework${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "ApexFramework${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "ApexFramework${CMAKE_RELEASE_POSTFIX}"
)