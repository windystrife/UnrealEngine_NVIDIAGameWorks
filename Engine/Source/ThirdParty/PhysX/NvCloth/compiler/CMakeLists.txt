cmake_minimum_required(VERSION 3.3)

set(CMAKE_CONFIGURATION_TYPES debug checked profile release)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "debug;checked")
set(CMAKE_CXX_FLAGS_CHECKED "")
set(CMAKE_CXX_FLAGS_PROFILE "")
set(CMAKE_SHARED_LINKER_FLAGS_CHECKED "")
set(CMAKE_SHARED_LINKER_FLAGS_PROFILE "")

set(CMAKE_DEBUG_POSTFIX   "Debug")
set(CMAKE_CHECKED_POSTFIX "Checked")
set(CMAKE_PROFILE_POSTFIX "Profile")
set(CMAKE_RELEASE_POSTFIX "")

get_filename_component(TARGET_DIRECTORY ${CMAKE_BINARY_DIR} NAME)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "../../bin/${TARGET_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_CHECKED "../../bin/${TARGET_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_PROFILE "../../bin/${TARGET_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "../../bin/${TARGET_DIRECTORY}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "../../lib/${TARGET_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_CHECKED "../../lib/${TARGET_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_PROFILE "../../lib/${TARGET_DIRECTORY}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "../../lib/${TARGET_DIRECTORY}")

project(NvCloth CXX)

set(CudaCompilerSupport "true")
if (${CMAKE_GENERATOR} MATCHES "Visual Studio 11")
	set(GeneratorFolderNameBase "vc11win")
elseif(${CMAKE_GENERATOR} MATCHES "Visual Studio 12")
	set(GeneratorFolderNameBase "vc12win")
elseif(${CMAKE_GENERATOR} MATCHES "Visual Studio 14")
	set(GeneratorFolderNameBase "vc14win")
	message( WARNING "MSVC14 is not yet supported (cuda 7.5 doesn't support 2015), so CuSolverKernelBlob.h needs to be regenerated with VC2013 when the cuda solver is changed" )
	set(CudaCompilerSupport "false")
else()
	message( FATAL_ERROR "This generator is not yet defined in CMakeList.txt" )
	set(CudaCompilerSupport "false")
endif()

if (${CMAKE_SIZEOF_VOID_P} MATCHES "4")
	set(Arch32_64 "32")
	set(Arch86_64 "86")
else()
	set(Arch32_64 "64")
	set(Arch86_64 "64")
endif()

file(GLOB SOURCE_FILES 
	"../include/NvCloth/*.*" 
	"../src/*.*"
	"../src/NvSimd/*.*"
	"../src/NvSimd/neon/*.*"
	"../src/NvSimd/scalar/*.*"
	"../src/NvSimd/sse2/*.*"
	"../extensions/include/NvClothExt/*.*"
	"../extensions/src/*.*"
)

FILE(GLOB FilterSourceFiles "../src/*.*")
SOURCE_GROUP(scr FILES ${FilterSourceFiles})
FILE(GLOB FilterSourceFiles "../include/NvCloth/*.*")
SOURCE_GROUP(include\\NvCloth FILES ${FilterSourceFiles})

FILE(GLOB FilterSourceFiles "../src/NvSimd/*.*")
SOURCE_GROUP(scr\\NvSimd FILES ${FilterSourceFiles})
FILE(GLOB FilterSourceFiles "../src/NvSimd/neon/*.*")
SOURCE_GROUP(scr\\NvSimd\\neon FILES ${FilterSourceFiles})
FILE(GLOB FilterSourceFiles "../src/NvSimd/scalar/*.*")
SOURCE_GROUP(scr\\NvSimd\\scalar FILES ${FilterSourceFiles})
FILE(GLOB FilterSourceFiles "../src/NvSimd/sse2/*.*")
SOURCE_GROUP(scr\\NvSimd\\sse2 FILES ${FilterSourceFiles})

FILE(GLOB FilterSourceFiles "../extensions/include/NvClothExt/*.*")
SOURCE_GROUP(extensions\\include\\NvClothExt FILES ${FilterSourceFiles})
FILE(GLOB FilterSourceFiles "../extensions/src/*.*")
SOURCE_GROUP(extensions\\src FILES ${FilterSourceFiles})

set(PxSharedPath "${CMAKE_CURRENT_LIST_DIR}/../../../../../physx/PxShared/1.0/trunk/")
set(CUDA_TOOLKIT_ROOT_DIR "../../../../libdev/external/CUDA/7.0.28")

if(TARGET_BUILD_PLATFORM STREQUAL "Windows")
	file(GLOB PLATFORM_FILES 
		"../src/sse2/*.*" 
		"../src/dx/*.h" 
		"../src/dx/*.cpp"
		"../src/cuda/*.h" 
		"../src/cuda/*.cpp"
	)

	file(GLOB AVX_FILES 
		"../src/avx/*.*" 
	)

	set_source_files_properties(${AVX_FILES} PROPERTIES COMPILE_FLAGS "/arch:AVX")

	file(GLOB HLSL_FILES 
		"../src/dx/*.hlsl"
	)

	set_source_files_properties(${HLSL_FILES} PROPERTIES VS_SHADER_TYPE Compute VS_SHADER_MODEL 5.0 VS_SHADER_FLAGS "/Vn gDxSolverKernel /Fh ../../src/dx/DxSolverKernelBlob.h")
	
	FILE(GLOB FilterSourceFiles "../src/dx/*.*")
	SOURCE_GROUP(scr\\dx FILES ${FilterSourceFiles})
	FILE(GLOB FilterSourceFiles "../src/cuda/*.*")
	SOURCE_GROUP(scr\\cuda FILES ${FilterSourceFiles})
	FILE(GLOB FilterSourceFiles "../src/sse2/*.*")
	SOURCE_GROUP(scr\\sse2 FILES ${FilterSourceFiles})
	FILE(GLOB FilterSourceFiles "../src/avx/*.*")
	SOURCE_GROUP(scr\\avx FILES ${FilterSourceFiles})
	
if(CudaCompilerSupport)

	file(GLOB CUDA_FILES 
		"../src/cuda/*.cu"
	)

	find_package(CUDA QUIET REQUIRED)

#		-use_fast_math -ftz=true -prec-div=false -prec-sqrt=false -g -G
#		-gencode arch=compute_20,code=sm_20 -gencode arch=compute_30,code=sm_30 -gencode arch=compute_50,code=sm_50 -gencode arch=compute_50,code=compute_50
	set(
		CUDA_NVCC_FLAGS
		${CUDA_NVCC_FLAGS};
		-use_fast_math -ftz=true -prec-div=false -prec-sqrt=false  -gencode arch=compute_20,code=sm_20 -gencode arch=compute_30,code=sm_30 -gencode arch=compute_35,code=sm_35 -gencode arch=compute_50,code=sm_50 -gencode arch=compute_50,code=compute_50 -D_USE_MATH_DEFINES -DNDEBUG -DPX_PROFILE=1
    )

	cuda_include_directories(
		"${CMAKE_CURRENT_SOURCE_DIR}/../include"
		"${PxSharedPath}/include/"
		"${PxSharedPath}/src/foundation/include/"
	)

	cuda_compile_fatbin(CUDA_FATBIN ${CUDA_FILES})

	set(CUDA_BLOB "${CMAKE_CURRENT_SOURCE_DIR}/../src/cuda/CuSolverKernelBlob.h")

	add_custom_command(
		OUTPUT ${CUDA_BLOB}
        COMMAND ${CUDA_TOOLKIT_ROOT_DIR}/bin/bin2c.exe --name kCuSolverKernel ${CUDA_FATBIN} > ${CUDA_BLOB}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${CUDA_FATBIN}
		VERBATIM
	)

	set(SOURCE_FILES ${SOURCE_FILES}  ${CUDA_BLOB})
endif()
	set(SOURCE_FILES ${SOURCE_FILES} ${PLATFORM_FILES} ${AVX_FILES} ${HLSL_FILES})
endif()

add_library(NvCloth SHARED 
	${SOURCE_FILES}
)

target_include_directories(NvCloth
	PUBLIC "../src"
	PUBLIC "../include"
	PUBLIC "${PxSharedPath}/include/"
	PUBLIC "${PxSharedPath}/src/foundation/include/"
	PUBLIC "../extensions/include"
)

target_compile_definitions(NvCloth
	PRIVATE NV_CLOTH_IMPORT=PX_DLL_EXPORT PRIVATE $<$<CONFIG:profile>:NDEBUG> PRIVATE $<$<CONFIG:CHECKED>:NDEBUG> _HAS_EXCEPTIONS=0 $<$<CONFIG:DEBUG>:PX_DEBUG PX_CHECKED> $<$<CONFIG:CHECKED>:PX_CHECKED> $<$<CONFIG:PROFILE>:PX_PROFILE>
)

if(TARGET_BUILD_PLATFORM STREQUAL "Windows")

	target_compile_definitions(NvCloth
		PRIVATE NV_CLOTH_ENABLE_DX11=1
	)

	target_include_directories(NvCloth
		PRIVATE "${CUDA_TOOLKIT_ROOT_DIR}/include"
	)
	
	target_link_libraries(NvCloth
		PRIVATE "../${CUDA_TOOLKIT_ROOT_DIR}/lib/\$(Platform)/cuda"
	)
	
	set(GeneratorFolderName "${GeneratorFolderNameBase}${Arch32_64}")
	
	set(PxSharedPathLib "${PxSharedPath}/lib/${GeneratorFolderName}/")

	get_filename_component(PxSharedPathLibA "${PxSharedPathLib}" ABSOLUTE)

	add_library(PxShared UNKNOWN IMPORTED)
	set_target_properties(PxShared PROPERTIES IMPORTED_LOCATION_RELEASE ${PxSharedPathLibA}/PxFoundation_x${Arch86_64}.lib)
	set_target_properties(PxShared PROPERTIES IMPORTED_LOCATION_DEBUG  ${PxSharedPathLibA}/PxFoundationDEBUG_x${Arch86_64}.lib)
	set_target_properties(PxShared PROPERTIES IMPORTED_LOCATION_PROFILE ${PxSharedPathLibA}/PxFoundationPROFILE_x${Arch86_64}.lib)
	set_target_properties(PxShared PROPERTIES IMPORTED_LOCATION_CHECKED ${PxSharedPathLibA}/PxFoundationCHECKED_x${Arch86_64}.lib)
	target_link_libraries(NvCloth PRIVATE PxShared)
	
	#compile flags
	SET_TARGET_PROPERTIES(NvCloth PROPERTIES COMPILE_FLAGS "/MP /GR- /GS- /fp:fast /GF /WX")
	target_compile_options(NvCloth  PRIVATE $<$<CONFIG:DEBUG>: /MTd  /RTCu>)
	

	#debug compile flags
	target_compile_options(NvCloth  PRIVATE $<$<CONFIG:DEBUG>:/Ob2 /d2Zi+ /Zo>)

	#release compile flags
	if (${CMAKE_SIZEOF_VOID_P} MATCHES "4")
		SET_TARGET_PROPERTIES(NvCloth PROPERTIES COMPILE_FLAGS "/arch:SSE2") #only needed for 32bit
	endif()
	target_compile_options(NvCloth  PRIVATE $<$<CONFIG:RELEASE>:/GL /fp:except- /Oi /Ox /MT>)
	
	#disable c++ exceptions
	if(CMAKE_CXX_FLAGS MATCHES "/EHsc ")
		string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	endif()
	if(CMAKE_CXX_FLAGS MATCHES "/EHsc$")
		string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	endif()
	
	#warning level 4
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()

	#Release linker flags
	set_property(TARGET NvCloth APPEND PROPERTY LINK_FLAGS_RELEASE "/LTCG")
	
	#linker flags
	set_property(TARGET NvCloth APPEND PROPERTY LINK_FLAGS "/INCREMENTAL:NO")
	
endif()
