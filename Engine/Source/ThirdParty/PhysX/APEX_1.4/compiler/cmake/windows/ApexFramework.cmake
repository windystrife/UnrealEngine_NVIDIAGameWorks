#
# Build ApexFramework
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(FRAMEWORK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../framework)

FIND_PACKAGE(nvToolsExt REQUIRED)

SET(APEXFRAMEWORK_LIBTYPE SHARED)

SET(APEXFRAMEWORK_PLATFORM_INCLUDES
	${NVTOOLSEXT_INCLUDE_DIRS}
	${PROJECT_SOURCE_DIR}/../../../common/include/windows
)

SET(APEXFRAMEWORK_PLATFORM_SOURCE_FILES
	${FRAMEWORK_SOURCE_DIR}/src/windows/AgMMFile.cpp
	${FRAMEWORK_SOURCE_DIR}/src/windows/PhysXIndicator.cpp
	${PROJECT_ROOT_DIR}/compiler/resource/apex.rc
)

SET(APEXFRAMEWORK_COMPILE_DEFS
	# Common to all configurations

	${APEX_WINDOWS_COMPILE_DEFS};EXCLUDE_CUDA=1;EXCLUDE_PARTICLES=1;_USRDLL;

	$<$<CONFIG:debug>:${APEX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common ApexFramework.cmake
INCLUDE(../common/ApexFramework.cmake)

# Do final direct sets after the target has been defined
#	TARGET_LINK_LIBRARIES(ApexFramework PUBLIC ${NVTOOLSEXT_LIBRARIES})
	TARGET_LINK_LIBRARIES(ApexFramework PUBLIC ApexCommon ApexShared NvParameterized PsFastXml PxFoundation PxPvdSDK PxTask RenderDebug
#		PhysxCommon - does it really need this?
	)

