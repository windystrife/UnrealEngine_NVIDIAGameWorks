#
# Build NvParameterized common
#

ADD_LIBRARY(NvParameterized STATIC 
	${NVPARAMETERIZED_PLATFORM_SOURCE_FILES}

	${NVP_SOURCE_DIR}/src/BinaryHelper.cpp
	${NVP_SOURCE_DIR}/src/BinSerializer.cpp
	${NVP_SOURCE_DIR}/src/NvParameterized.cpp
	${NVP_SOURCE_DIR}/src/NvParameterizedTraits.cpp
	${NVP_SOURCE_DIR}/src/NvSerializer.cpp
	${NVP_SOURCE_DIR}/src/NvTraits.cpp
	${NVP_SOURCE_DIR}/src/PlatformABI.cpp
	${NVP_SOURCE_DIR}/src/PlatformInputStream.cpp
	${NVP_SOURCE_DIR}/src/PlatformOutputStream.cpp
	${NVP_SOURCE_DIR}/src/SerializerCommon.cpp
	${NVP_SOURCE_DIR}/src/XmlDeserializer.cpp
	${NVP_SOURCE_DIR}/src/XmlSerializer.cpp
	
)

TARGET_INCLUDE_DIRECTORIES(NvParameterized 
	PRIVATE ${NVPARAMETERIZED_PLATFORM_INCLUDES}

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

	PRIVATE ${NVP_SOURCE_DIR}/public
	PRIVATE ${NVP_SOURCE_DIR}/include
)

TARGET_COMPILE_DEFINITIONS(NvParameterized 
	PRIVATE ${NVPARAMETERIZED_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(NvParameterized PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "NvParameterized${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "NvParameterized${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "NvParameterized${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "NvParameterized${CMAKE_RELEASE_POSTFIX}"
)