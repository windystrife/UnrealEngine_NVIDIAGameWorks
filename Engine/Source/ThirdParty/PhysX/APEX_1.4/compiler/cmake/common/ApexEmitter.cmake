#
# Build APEX_Emitter common
#

ADD_LIBRARY(APEX_Emitter STATIC 
	${APEX_EMITTER_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/src/EmitterActorImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterAssetImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterAssetPreview.cpp
	${AM_SOURCE_DIR}/src/EmitterGeomBase.cpp
	${AM_SOURCE_DIR}/src/EmitterGeomBoxImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterGeomCylinderImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterGeomExplicitImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterGeomSphereImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterGeomSphereShellImpl.cpp
	${AM_SOURCE_DIR}/src/EmitterScene.cpp
	${AM_SOURCE_DIR}/src/GroundEmitterActorImpl.cpp
	${AM_SOURCE_DIR}/src/GroundEmitterAssetImpl.cpp
	${AM_SOURCE_DIR}/src/GroundEmitterAssetPreview.cpp
	${AM_SOURCE_DIR}/src/ImpactEmitterActorImpl.cpp
	${AM_SOURCE_DIR}/src/ImpactEmitterAssetImpl.cpp
	${AM_SOURCE_DIR}/src/ModuleEmitterImpl.cpp
	${AM_SOURCE_DIR}/src/autogen/ApexEmitterActorParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ApexEmitterAssetParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterAssetPreviewParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterDebugRenderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterGeomBoxParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterGeomCylinderParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterGeomExplicitParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterGeomSphereParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterGeomSphereShellParams.cpp
	${AM_SOURCE_DIR}/src/autogen/EmitterModuleParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/GroundEmitterActorParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/GroundEmitterAssetParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ImpactEmitterActorParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ImpactEmitterAssetParameters.cpp
	${AM_SOURCE_DIR}/src/autogen/ImpactExplosionEvent.cpp
	${AM_SOURCE_DIR}/src/autogen/ImpactObjectEvent.cpp

	${APEX_MODULE_DIR}/common/src/ModuleProfileCommon.cpp
	
)

TARGET_INCLUDE_DIRECTORIES(APEX_Emitter 
	PRIVATE ${APEX_EMITTER_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/emitter
	PRIVATE ${APEX_MODULE_DIR}/../include/iofx
	
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

	PRIVATE ${APEX_MODULE_DIR}/common/include

	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public

	PRIVATE ${APEX_MODULE_DIR}/iofx/public
	
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

TARGET_COMPILE_DEFINITIONS(APEX_Emitter 
	PRIVATE ${APEX_EMITTER_COMPILE_DEFS}
)
