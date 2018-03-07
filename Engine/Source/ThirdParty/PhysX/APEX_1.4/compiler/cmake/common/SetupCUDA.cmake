# When FIND_PACKAGE(CUDA) is called, it resets all of the user supplied data. So this needs to be done before the CUDA_ADD_LIBRARY calls that need these settings. 
# Keep in mind that other, dependent projects will reset these settings, so to be safe we need to call this every time right before.

FIND_PACKAGE(CUDA 6.5 REQUIRED)
FIND_PACKAGE(Boost 1.4 REQUIRED)
FIND_PACKAGE(PxShared REQUIRED)

#NOTE: Also wanted Boost - but probably not really.
CUDA_INCLUDE_DIRECTORIES(
	${Boost_INCLUDE_DIRS}

	${APEX_MODULE_DIR}/../include
	${APEX_MODULE_DIR}/../include/PhysX3
	
	${PXSHARED_ROOT_DIR}/include
	${PXSHARED_ROOT_DIR}/include/task
	${PXSHARED_ROOT_DIR}/include/cudamanager
	${PXSHARED_ROOT_DIR}/include/foundation
	${PXSHARED_ROOT_DIR}/src/foundation/include
	${PXSHARED_ROOT_DIR}/src/cudamanager/include

	${APEX_MODULE_DIR}/../shared/general/shared
	
	${APEX_MODULE_DIR}/common/cuda
	
	${APEX_MODULE_DIR}/../common/include
	${APEX_MODULE_DIR}/../common/autogen
	
	${APEX_MODULE_DIR}/../framework/public
	${APEX_MODULE_DIR}/../framework/public/PhysX3
	${APEX_MODULE_DIR}/../framework/src/cuda
)

SET(CUDA_PROPAGATE_HOST_FLAGS OFF)

SET(CUDA_NVCC_FLAGS "--ptxas-options=\"-v   \" -gencode arch=compute_20,code=sm_20 -gencode arch=compute_30,code=sm_30 -gencode arch=compute_35,code=sm_35 -gencode arch=compute_50,code=sm_50 -D_DEBUG -DWIN32 -D_CONSOLE -D_WIN32_WINNT=0x0501")

#	SET(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -DAPEX_CUDA_FILE=${APEX_CUDA_FILE_NAME}")


SET(CUDA_NVCC_FLAGS_DEBUG   "--compiler-options=/EHsc,/W3,/nologo,/Ot,/Ox,/Zi,/MTd")
SET(CUDA_NVCC_FLAGS_CHECKED "--compiler-options=/EHsc,/W3,/nologo,/Ot,/Ox,/Zi,/MT")
SET(CUDA_NVCC_FLAGS_PROFILE "--compiler-options=/EHsc,/W3,/nologo,/Ot,/Ox,/Zi,/MT")
SET(CUDA_NVCC_FLAGS_RELEASE "--compiler-options=/EHsc,/W3,/nologo,/Ot,/Ox,/Zi,/MT")	