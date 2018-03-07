#
# Build NvCloth common
#


#run 
#> dir /a-d /b /s
#in /src, /include, /extensions to list files

SET( NV_CLOTH_UNIT_TESTS_SOURCE_LIST
	${NVCLOTH_UNIT_TESTS_PLATFORM_SOURCE_FILES}
	
	${PROJECT_ROOT_DIR}/src/main.cpp
#	${PROJECT_ROOT_DIR}/src/perfTests/Simulation.cpp
	${PROJECT_ROOT_DIR}/src/tests/ClothCloning.cpp
	${PROJECT_ROOT_DIR}/src/tests/ClothCreation.cpp
	${PROJECT_ROOT_DIR}/src/tests/Cooker.cpp
	${PROJECT_ROOT_DIR}/src/tests/ExampleCodeTest.cpp
	${PROJECT_ROOT_DIR}/src/tests/FabricCreation.cpp
	${PROJECT_ROOT_DIR}/src/tests/FactoryCreation.cpp
	${PROJECT_ROOT_DIR}/src/tests/Range.cpp
	${PROJECT_ROOT_DIR}/src/tests/Simd.cpp
	${PROJECT_ROOT_DIR}/src/tests/Simulation.cpp
	${PROJECT_ROOT_DIR}/src/tests/SolverCreation.cpp
	${PROJECT_ROOT_DIR}/src/tests/UnitTestCallbackTests.cpp
	${PROJECT_ROOT_DIR}/src/utilities/CallbackImplementations.cpp
	${PROJECT_ROOT_DIR}/src/utilities/CallbackImplementations.h
	${PROJECT_ROOT_DIR}/src/utilities/ClothMeshGenerator.cpp
	${PROJECT_ROOT_DIR}/src/utilities/ClothMeshGenerator.h
	${PROJECT_ROOT_DIR}/src/utilities/PerformanceTimer.cpp
	${PROJECT_ROOT_DIR}/src/utilities/PerformanceTimer.h
	${PROJECT_ROOT_DIR}/src/utilities/SimulationFixture.cpp
	${PROJECT_ROOT_DIR}/src/utilities/SimulationFixture.h
	${PROJECT_ROOT_DIR}/src/utilities/Utilities.cpp
	${PROJECT_ROOT_DIR}/src/utilities/Utilities.h

)

# add_executable
ADD_EXECUTABLE(NvClothUnitTests ${NV_CLOTH_UNIT_TESTS_SOURCE_LIST})

foreach(source IN LISTS NV_CLOTH_UNIT_TESTS_SOURCE_LIST)
	string(LENGTH ${PROJECT_ROOT_DIR} strlen)
	string(SUBSTRING ${source} ${strlen} -1 timmedSource)
	#MESSAGE("${source} -> ${timmedSource}")
	get_filename_component(source_path "${timmedSource}" PATH)
	string(REPLACE "/" "\\" source_path_msvc "${source_path}")
	source_group("${source_path_msvc}" FILES "${source}")
endforeach()

# Target specific compile options


TARGET_INCLUDE_DIRECTORIES(NvClothUnitTests 
	PRIVATE ${NVCLOTH_UNIT_TESTS_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include

	PRIVATE ${PROJECT_ROOT_DIR}/include
	PRIVATE ${PROJECT_ROOT_DIR}/extensions/include
	PRIVATE ${PROJECT_ROOT_DIR}/src
	PRIVATE ${PROJECT_ROOT_DIR}/extensions/src
	
	PRIVATE ${GOOGLETEST_INCLUDE_DIR}
	
	PRIVATE ${NVCLOTH_ROOT_DIR}/include
	PRIVATE ${NVCLOTH_ROOT_DIR}/extensions/include
)

SET(NVCLOTH_UNIT_TESTS_COMPILE_DEFS
	${NVCLOTH_UNIT_TESTS_COMPILE_DEFS};


	# Common to all configurations

)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(NvClothUnitTests

	# Common to all configurations
	PRIVATE ${NVCLOTH_UNIT_TESTS_COMPILE_DEFS}
)

IF(NOT ${NVCLOTH_UNIT_TESTS_LIBTYPE} STREQUAL "OBJECT")
	SET_TARGET_PROPERTIES(NvClothUnitTests PROPERTIES 
		COMPILE_PDB_NAME_DEBUG "NvClothUnitTests${CMAKE_DEBUG_POSTFIX}"
		COMPILE_PDB_NAME_CHECKED "NvClothUnitTests${CMAKE_CHECKED_POSTFIX}"
		COMPILE_PDB_NAME_PROFILE "NvClothUnitTests${CMAKE_PROFILE_POSTFIX}"
		COMPILE_PDB_NAME_RELEASE "NvClothUnitTests${CMAKE_RELEASE_POSTFIX}"
	)
ENDIF()
