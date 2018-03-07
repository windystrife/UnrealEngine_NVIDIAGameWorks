#
# Build LowLevelParticles common
#

SET(LLPARTICLES_BASE_DIR ${PHYSX_ROOT_DIR}/Source/LowLevelParticles)
SET(LLPARTICLES_INCLUDES		
	${LLPARTICLES_BASE_DIR}/include/PtBodyTransformVault.h
	${LLPARTICLES_BASE_DIR}/include/PtContext.h
	${LLPARTICLES_BASE_DIR}/include/PtGridCellVector.h
	${LLPARTICLES_BASE_DIR}/include/PtParticle.h
	${LLPARTICLES_BASE_DIR}/include/PtParticleContactManagerStream.h
	${LLPARTICLES_BASE_DIR}/include/PtParticleData.h
	${LLPARTICLES_BASE_DIR}/include/PtParticleShape.h
	${LLPARTICLES_BASE_DIR}/include/PtParticleSystemCore.h
	${LLPARTICLES_BASE_DIR}/include/PtParticleSystemFlags.h
	${LLPARTICLES_BASE_DIR}/include/PtParticleSystemSim.h
)
SOURCE_GROUP("include" FILES ${LLPARTICLES_INCLUDES})

SET(LLPARTICLES_SOURCE			
	${LLPARTICLES_BASE_DIR}/src/PtBatcher.cpp
	${LLPARTICLES_BASE_DIR}/src/PtBatcher.h
	${LLPARTICLES_BASE_DIR}/src/PtBodyTransformVault.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollision.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollision.h
	${LLPARTICLES_BASE_DIR}/src/PtCollisionBox.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollisionCapsule.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollisionConvex.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollisionData.h
	${LLPARTICLES_BASE_DIR}/src/PtCollisionHelper.h
	${LLPARTICLES_BASE_DIR}/src/PtCollisionMesh.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollisionMethods.h
	${LLPARTICLES_BASE_DIR}/src/PtCollisionParameters.h
	${LLPARTICLES_BASE_DIR}/src/PtCollisionPlane.cpp
	${LLPARTICLES_BASE_DIR}/src/PtCollisionSphere.cpp
	${LLPARTICLES_BASE_DIR}/src/PtConfig.h
	${LLPARTICLES_BASE_DIR}/src/PtConstants.h
	${LLPARTICLES_BASE_DIR}/src/PtContextCpu.cpp
	${LLPARTICLES_BASE_DIR}/src/PtContextCpu.h
	${LLPARTICLES_BASE_DIR}/src/PtDynamicHelper.h
	${LLPARTICLES_BASE_DIR}/src/PtDynamics.cpp
	${LLPARTICLES_BASE_DIR}/src/PtDynamics.h
	${LLPARTICLES_BASE_DIR}/src/PtDynamicsKernels.h
	${LLPARTICLES_BASE_DIR}/src/PtDynamicsParameters.h
	${LLPARTICLES_BASE_DIR}/src/PtDynamicsTempBuffers.h
	${LLPARTICLES_BASE_DIR}/src/PtHeightFieldAabbTest.h
	${LLPARTICLES_BASE_DIR}/src/PtPacketSections.h
	${LLPARTICLES_BASE_DIR}/src/PtParticleCell.h
	${LLPARTICLES_BASE_DIR}/src/PtParticleData.cpp
	${LLPARTICLES_BASE_DIR}/src/PtParticleOpcodeCache.h
	${LLPARTICLES_BASE_DIR}/src/PtParticleShapeCpu.cpp
	${LLPARTICLES_BASE_DIR}/src/PtParticleShapeCpu.h
	${LLPARTICLES_BASE_DIR}/src/PtParticleSystemSimCpu.cpp
	${LLPARTICLES_BASE_DIR}/src/PtParticleSystemSimCpu.h
	${LLPARTICLES_BASE_DIR}/src/PtSpatialHash.cpp
	${LLPARTICLES_BASE_DIR}/src/PtSpatialHash.h
	${LLPARTICLES_BASE_DIR}/src/PtSpatialHashHelper.h
	${LLPARTICLES_BASE_DIR}/src/PtSpatialLocalHash.cpp
	${LLPARTICLES_BASE_DIR}/src/PtTwoWayData.h
)
SOURCE_GROUP("src" FILES ${LLPARTICLES_SOURCE})

ADD_LIBRARY(LowLevelParticles ${LOWLEVELPARTICLES_LIBTYPE}
	${LLPARTICLES_INCLUDES}
	${LLPARTICLES_SOURCE}

	${LOWLEVELPARTICLES_PLATFORM_SRC_FILES}
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(LowLevelParticles 
	PRIVATE ${LOWLEVELPARTICLES_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelParticles/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelParticles/src
	
)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(LowLevelParticles 

	# Common to all configurations
	PRIVATE ${LOWLEVELPARTICLES_COMPILE_DEFS}
)

IF(NOT ${LOWLEVELPARTICLES_LIBTYPE} STREQUAL "OBJECT")
	SET_TARGET_PROPERTIES(LowLevelParticles PROPERTIES 
		COMPILE_PDB_NAME_DEBUG "LowLevelParticles${CMAKE_DEBUG_POSTFIX}"
		COMPILE_PDB_NAME_CHECKED "LowLevelParticles${CMAKE_CHECKED_POSTFIX}"
		COMPILE_PDB_NAME_PROFILE "LowLevelParticles${CMAKE_PROFILE_POSTFIX}"
		COMPILE_PDB_NAME_RELEASE "LowLevelParticles${CMAKE_RELEASE_POSTFIX}"
	)
ENDIF()