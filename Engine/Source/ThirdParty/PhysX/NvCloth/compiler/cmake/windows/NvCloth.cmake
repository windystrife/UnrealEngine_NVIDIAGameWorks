#
# Build NvCloth
#

MESSAGE("[NvCloth]cmake/windows/NvCloth.cmake")

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
#FIND_PACKAGE(PxShared REQUIRED)

SET(NVCLOTH_PLATFORM_SOURCE_FILES
	
	#${PROJECT_ROOT_DIR}/src/cuda/CuSolverKernel.h
	#${PROJECT_ROOT_DIR}/src/cuda/CuSolverKernelBlob.h
	#${PROJECT_ROOT_DIR}/src/dx/DxBatchedVector.h
	#${PROJECT_ROOT_DIR}/src/dx/DxCheckSuccess.h
	#${PROJECT_ROOT_DIR}/src/dx/DxCloth.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxCloth.h
	#${PROJECT_ROOT_DIR}/src/dx/DxClothClone.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxClothData.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxClothData.h
	#${PROJECT_ROOT_DIR}/src/dx/DxContextLock.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxContextLock.h
	#${PROJECT_ROOT_DIR}/src/dx/DxDeviceVector.h
	#${PROJECT_ROOT_DIR}/src/dx/DxFabric.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxFabric.h
	#${PROJECT_ROOT_DIR}/src/dx/DxFactory.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxFactory.h
	#${PROJECT_ROOT_DIR}/src/dx/DxSolver.cpp
	#${PROJECT_ROOT_DIR}/src/dx/DxSolver.h
	
	#${PROJECT_ROOT_DIR}/src/dx/DxSolverKernelBlob.h
	#${PROJECT_ROOT_DIR}/src/dx/DxSortKernel.inc
	#${PROJECT_ROOT_DIR}/src/neon/NeonCollision.cpp
	#${PROJECT_ROOT_DIR}/src/neon/NeonSelfCollision.cpp
	#${PROJECT_ROOT_DIR}/src/neon/NeonSolverKernel.cpp
	#${PROJECT_ROOT_DIR}/src/neon/SwCollisionHelpers.h
)

SET(NVCLOTH_AVX_SOURCE_FILES
		${PROJECT_ROOT_DIR}/src/avx/SwSolveConstraints.cpp
)

set_source_files_properties(${NVCLOTH_AVX_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "/arch:AVX")

#SET(NVCLOTH_HLSL_FILES
#	${PROJECT_ROOT_DIR}/src/dx/DxSolverKernel.hlsl
#)
#set_source_files_properties(${NVCLOTH_HLSL_FILES} PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL 5.0 VS_SHADER_FLAGS "/Vn gDxSolverKernel /Fh ${PROJECT_ROOT_DIR}/src/dx/DxSolverKernelBlob.h")

SET(NVCLOTH_PLATFORM_SOURCE_FILES ${NVCLOTH_PLATFORM_SOURCE_FILES} ${NVCLOTH_AVX_SOURCE_FILES})


SET(NVCLOTH_COMPILE_DEFS

	NV_CLOTH_IMPORT=PX_DLL_EXPORT
	NV_CLOTH_ENABLE_DX11=0
	NV_CLOTH_ENABLE_CUDA=0

	$<$<CONFIG:debug>:${NVCLOTH_WINDOWS_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${NVCLOTH_WINDOWS_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${NVCLOTH_WINDOWS_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${NVCLOTH_WINDOWS_RELEASE_COMPILE_DEFS};>
)



SET(NVCLOTH_LIBTYPE SHARED)

# include common NvCloth settings
INCLUDE(../common/NvCloth.cmake)

# Add linked libraries

TARGET_LINK_LIBRARIES(NvCloth PUBLIC PxFoundation)

SET_TARGET_PROPERTIES(NvCloth PROPERTIES 
	LINK_FLAGS_DEBUG "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundationDEBUG_${LIBPATH_SUFFIX}.dll /DEBUG"
	LINK_FLAGS_CHECKED "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundationCHECKED_${LIBPATH_SUFFIX}.dll"
	LINK_FLAGS_PROFILE "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundationPROFILE_${LIBPATH_SUFFIX}.dll /INCREMENTAL:NO /DEBUG"
	LINK_FLAGS_RELEASE "/DELAYLOAD:nvcuda.dll /MAP /DELAYLOAD:PxFoundation_${LIBPATH_SUFFIX}.dll /INCREMENTAL:NO"
)
MESSAGE("[NvCloth]cmake/windows/NvCloth.cmake END")