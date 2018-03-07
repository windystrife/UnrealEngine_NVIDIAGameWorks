#
# Build RenderDebug common
#

ADD_LIBRARY(RenderDebug STATIC 
	${RENDERDEBUG_PLATFORM_SOURCE_FILES}

	${RD_SOURCE_DIR}/src/ClientServer.cpp
	${RD_SOURCE_DIR}/src/FileRenderDebug.cpp
	${RD_SOURCE_DIR}/src/Hull2MeshEdges.cpp
	${RD_SOURCE_DIR}/src/InternalRenderDebug.cpp
	${RD_SOURCE_DIR}/src/ProcessRenderDebug.cpp
	${RD_SOURCE_DIR}/src/PsCommLayer.cpp
	${RD_SOURCE_DIR}/src/PsCommStream.cpp
	${RD_SOURCE_DIR}/src/RenderDebug.cpp
	${RD_SOURCE_DIR}/src/RenderDebugDLLMain.cpp
	
)

TARGET_INCLUDE_DIRECTORIES(RenderDebug 
	PRIVATE ${RENDERDEBUG_PLATFORM_INCLUDES}

	PRIVATE ${RD_SOURCE_DIR}/include
	PRIVATE ${RD_SOURCE_DIR}/public

	PRIVATE ${PXSHARED_ROOT_DIR}/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/filebuf			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/foundation			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/pvd			
	PRIVATE ${PXSHARED_ROOT_DIR}/include/task			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/filebuf/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include			
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include			
	

	PRIVATE ${APEX_MODULE_DIR}/../shared/general/FoundationUnported/include
)

TARGET_COMPILE_DEFINITIONS(RenderDebug 
	PRIVATE ${RENDERDEBUG_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(RenderDebug PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "RenderDebug${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "RenderDebug${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "RenderDebug${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "RenderDebug${CMAKE_RELEASE_POSTFIX}"
)