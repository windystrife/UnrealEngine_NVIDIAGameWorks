#
# Build APEX_Loader
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)

SET(APEX_MODULE_DIR ${PROJECT_SOURCE_DIR}/../../../module)

SET(AM_SOURCE_DIR ${APEX_MODULE_DIR}/loader)

SET(APEX_LOADER_LIBTYPE SHARED)

SET(APEX_LOADER_PLATFORM_INCLUDES
	${PXSHARED_ROOT_DIR}/include/cudamanager
	${PROJECT_SOURCE_DIR}/../../../common/include/windows
)

SET(APEX_LOADER_PLATFORM_SOURCE_FILES
	${PROJECT_ROOT_DIR}/compiler/resource/apex.rc
)

SET(APEX_LOADER_COMPILE_DEFS
	# Common to all configurations

	${APEX_WINDOWS_COMPILE_DEFS};ENABLE_TEST=0;EXCLUDE_CUDA=1;EXCLUDE_PARTICLES=1;_USRDLL;

	$<$<CONFIG:debug>:${APEX_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${APEX_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${APEX_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${APEX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common ApexLoader.cmake
INCLUDE(../common/ApexLoader.cmake)

# Do final direct sets after the target has been defined
TARGET_LINK_LIBRARIES(APEX_Loader PUBLIC 
	${NVTOOLSEXT_LIBRARIES}
	APEX_Clothing APEX_Destructible APEX_Legacy
	ApexCommon ApexFramework ApexShared NvParameterized PsFastXml PxFoundation PxPvdSDK
)
