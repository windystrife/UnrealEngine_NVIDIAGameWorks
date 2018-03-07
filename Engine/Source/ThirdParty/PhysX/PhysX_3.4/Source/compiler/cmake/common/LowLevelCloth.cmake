#
# Build LowLevelCloth common
#

SET(LLCLOTH_INCLUDE_FILES	
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/Cloth.h
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/Fabric.h
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/Factory.h
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/PhaseConfig.h
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/Range.h
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/Solver.h
	${PHYSX_SOURCE_DIR}/LowLevelCloth/include/Types.h
)
SOURCE_GROUP("include" FILES ${LLCLOTH_INCLUDE_FILES})

SET(LLCLOTH_SOURCE_FILES
	${LL_SOURCE_DIR}/Allocator.cpp
	${LL_SOURCE_DIR}/Allocator.h
	${LL_SOURCE_DIR}/Array.h
	${LL_SOURCE_DIR}/BoundingBox.h
	${LL_SOURCE_DIR}/ClothBase.h
	${LL_SOURCE_DIR}/ClothImpl.h
	${LL_SOURCE_DIR}/Factory.cpp
	${LL_SOURCE_DIR}/IndexPair.h
	${LL_SOURCE_DIR}/IterationState.h
	${LL_SOURCE_DIR}/MovingAverage.h
	${LL_SOURCE_DIR}/PhaseConfig.cpp
	${LL_SOURCE_DIR}/PointInterpolator.h
	${LL_SOURCE_DIR}/Simd.h
	${LL_SOURCE_DIR}/Simd4f.h
	${LL_SOURCE_DIR}/Simd4i.h
	${LL_SOURCE_DIR}/StackAllocator.h
	${LL_SOURCE_DIR}/SwCloth.cpp
	${LL_SOURCE_DIR}/SwCloth.h
	${LL_SOURCE_DIR}/SwClothData.cpp
	${LL_SOURCE_DIR}/SwClothData.h
	${LL_SOURCE_DIR}/SwCollision.cpp
	${LL_SOURCE_DIR}/SwCollision.h
	${LL_SOURCE_DIR}/SwCollisionHelpers.h
	${LL_SOURCE_DIR}/SwFabric.cpp
	${LL_SOURCE_DIR}/SwFabric.h
	${LL_SOURCE_DIR}/SwFactory.cpp
	${LL_SOURCE_DIR}/SwFactory.h
	${LL_SOURCE_DIR}/SwInterCollision.cpp
	${LL_SOURCE_DIR}/SwInterCollision.h
	${LL_SOURCE_DIR}/SwSelfCollision.cpp
	${LL_SOURCE_DIR}/SwSelfCollision.h
	${LL_SOURCE_DIR}/SwSolver.cpp
	${LL_SOURCE_DIR}/SwSolver.h
	${LL_SOURCE_DIR}/SwSolverKernel.cpp
	${LL_SOURCE_DIR}/SwSolverKernel.h
	${LL_SOURCE_DIR}/TripletScheduler.cpp
	${LL_SOURCE_DIR}/TripletScheduler.h
	${LL_SOURCE_DIR}/Vec4T.h
)
SOURCE_GROUP("src" FILES ${LLCLOTH_SOURCE_FILES})

ADD_LIBRARY(LowLevelCloth ${LOWLEVELCLOTH_LIBTYPE}
	${LOWLEVELCLOTH_PLATFORM_SOURCE_FILES}
	
	${LLCLOTH_INCLUDE_FILES}
	
	${LLCLOTH_SOURCE_FILES}
)

TARGET_INCLUDE_DIRECTORIES(LowLevelCloth 
	PRIVATE ${LOWLEVELCLOTH_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelCloth/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelCloth/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelAABB/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelAABB/src
)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(LowLevelCloth 

	# Common to all configurations
	PRIVATE ${LOWLEVELCLOTH_COMPILE_DEFS}
)

IF(NOT ${LOWLEVELCLOTH_LIBTYPE} STREQUAL "OBJECT")
	SET_TARGET_PROPERTIES(LowLevelCloth PROPERTIES 
		COMPILE_PDB_NAME_DEBUG "LowLevelCloth${CMAKE_DEBUG_POSTFIX}"
		COMPILE_PDB_NAME_CHECKED "LowLevelCloth${CMAKE_CHECKED_POSTFIX}"
		COMPILE_PDB_NAME_PROFILE "LowLevelCloth${CMAKE_PROFILE_POSTFIX}"
		COMPILE_PDB_NAME_RELEASE "LowLevelCloth${CMAKE_RELEASE_POSTFIX}"
	)
ENDIF()