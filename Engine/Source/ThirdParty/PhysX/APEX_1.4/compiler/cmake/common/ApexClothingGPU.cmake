#
# Build APEX_ClothingGPU common
#


CUDA_ADD_LIBRARY(APEX_ClothingGPU SHARED 
	${APEX_CLOTHINGGPU_PLATFORM_SOURCE_FILES}
	
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/Allocator.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/Factory.cpp
	
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/PhaseConfig.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwCloth.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwClothData.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwCollision.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwFabric.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwFactory.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwInterCollision.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSelfCollision.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSolver.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSolverKernel.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/TripletScheduler.cpp

	${AM_SOURCE_DIR}/embedded/CreateCuFactory.cpp
)

# Give SwSolverKernel some special treatment.
SET_SOURCE_FILES_PROPERTIES(${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/SwSolverKernel.cpp PROPERTIES COMPILE_FLAGS /c /Zi /Ox /MT /arch:AVX)

TARGET_INCLUDE_DIRECTORIES(APEX_ClothingGPU 
	PRIVATE ${APEX_CLOTHINGGPU_PLATFORM_INCLUDES}

	PRIVATE ${APEX_MODULE_DIR}/../include
	PRIVATE ${APEX_MODULE_DIR}/../include/PhysX3
	PRIVATE ${APEX_MODULE_DIR}/../include/clothing
	
	PRIVATE ${PXSHARED_ROOT_DIR}/include			
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
	
	PRIVATE ${APEX_MODULE_DIR}/../framework/public
	PRIVATE ${APEX_MODULE_DIR}/../framework/public/PhysX3

	PRIVATE ${AM_SOURCE_DIR}/embedded
	PRIVATE ${AM_SOURCE_DIR}/embedded/LowLevelCloth/include
	PRIVATE ${AM_SOURCE_DIR}/embedded/LowLevelCloth/src
	PRIVATE ${AM_SOURCE_DIR}/include
	PRIVATE ${AM_SOURCE_DIR}/include/autogen
	PRIVATE ${AM_SOURCE_DIR}/public

	PRIVATE ${APEX_MODULE_DIR}/common/include

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

TARGET_COMPILE_DEFINITIONS(APEX_ClothingGPU 
	PRIVATE ${APEX_CLOTHINGGPU_COMPILE_DEFS}
)
