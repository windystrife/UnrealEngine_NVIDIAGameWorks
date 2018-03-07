#
# Build APEX_ClothingGPU
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/clothing)

FIND_PACKAGE(nvToolsExt REQUIRED)

# Set the CUDA includes, etc.
INCLUDE(../common/SetupCUDA.cmake)

SET(APEX_CLOTHINGGPU_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}

	${PXSHARED_ROOT_DIR}/include/cudamanager
	${PROJECT_SOURCE_DIR}/../../../common/include/windows
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows
	${AM_SOURCE_DIR}/include/windows

	${PHYSX_ROOT_DIR}/Source/PhysXGpu/include
	${PHYSX_ROOT_DIR}/Include/gpu
)

SET(APEX_CLOTHINGGPU_PLATFORM_SOURCE_FILES

	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuCloth.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuClothClone.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuClothData.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuContextLock.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuFabric.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuFactory.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/cuPrintf.cu
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuSolver.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/CuSolverKernel.cu
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxCloth.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxClothClone.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxClothData.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxContextLock.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxFabric.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxFactory.cpp
	${AM_SOURCE_DIR}/embedded/LowLevelCloth/src/windows/DxSolver.cpp

	${AM_SOURCE_DIR}/embedded/src/windows/cuPrintf.cu
	${AM_SOURCE_DIR}/embedded/src/windows/cuSolverKernel.cu

)


SET(APEX_CLOTHINGGPU_COMPILE_DEFS

	# Common to all configurations
	${APEX_WINDOWS_COMPILE_DEFS};ENABLE_TEST=0;_USRDLL;

	$<$<CONFIG:debug>:${APEX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common ApexClothingGPU.cmake
INCLUDE(../common/ApexClothingGPU.cmake)


# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_ClothingGPU PUBLIC ${NVTOOLSEXT_LIBRARIES} ApexCommon ApexShared NvParameterized PsFastXml PxCudaContextManager PxFoundation PxPvdSDK)



