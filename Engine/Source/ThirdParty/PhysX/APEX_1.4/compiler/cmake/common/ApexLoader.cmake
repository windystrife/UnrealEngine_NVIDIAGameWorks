#
# Build APEX_Loader common
#

ADD_LIBRARY(APEX_Loader ${APEX_LOADER_LIBTYPE} 
	${APEX_LOADER_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/src/ModuleLoaderImpl.cpp
	
)


TARGET_INCLUDE_DIRECTORIES(APEX_Loader 
	PRIVATE ${APEX_LOADER_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/loader
	PRIVATE ${APEX_MODULE_DIR}/../include/clothing
	PRIVATE ${APEX_MODULE_DIR}/../include/legacy
	PRIVATE ${APEX_MODULE_DIR}/../include/destructible
	PRIVATE ${APEX_MODULE_DIR}/../include/basicfs
	PRIVATE ${APEX_MODULE_DIR}/../include/basicios
	PRIVATE ${APEX_MODULE_DIR}/../include/emitter
	PRIVATE ${APEX_MODULE_DIR}/../include/fieldsampler
	PRIVATE ${APEX_MODULE_DIR}/../include/forcefield
	PRIVATE ${APEX_MODULE_DIR}/../include/iofx
	PRIVATE ${APEX_MODULE_DIR}/../include/pxparticleios
	PRIVATE ${APEX_MODULE_DIR}/../include/particles
	PRIVATE ${APEX_MODULE_DIR}/../include/turbulencefs
	PRIVATE ${APEX_MODULE_DIR}/../include/wind
	
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

	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3
	
	PRIVATE ${APEX_MODULE_DIR}/clothing_legacy/public
	
	PRIVATE ${APEX_MODULE_DIR}/clothing/public
	
	PRIVATE ${APEX_MODULE_DIR}/common/include
	
	PRIVATE ${APEX_MODULE_DIR}/destructible_legacy/public
	
	PRIVATE ${APEX_MODULE_DIR}/destructible/public
	
	PRIVATE ${APEX_MODULE_DIR}/framework_legacy/public
	
	PRIVATE ${APEX_MODULE_DIR}/legacy/public
	
	PRIVATE ${APEX_MODULE_DIR}/loader/include
	PRIVATE ${APEX_MODULE_DIR}/loader/public
	
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

TARGET_COMPILE_DEFINITIONS(APEX_Loader 
	PRIVATE ${APEX_LOADER_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(APEX_Loader PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "APEX_Loader${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "APEX_Loader${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "APEX_Loader${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "APEX_Loader${CMAKE_RELEASE_POSTFIX}"
)