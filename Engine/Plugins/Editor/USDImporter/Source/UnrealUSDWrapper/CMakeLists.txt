PROJECT(UnrealUSDWrapper)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# use clang only, and compile against our libc++
if (${CMAKE_HOST_SYSTEM} MATCHES "Linux")
	message(STATUS "================== NOT CROSSCOMPILING! ============================")
	SET(CMAKE_CXX_COMPILER clang++)
	SET(CMAKE_C_COMPILER clang)
endif (NOT ${CMAKE_CROSSCOMPILING})

GET_FILENAME_COMPONENT(UE4_THIRD_PARTY "../../../../../Source/ThirdParty" ABSOLUTE)
message(STATUS "UE4_THIRD_PARTY=${UE4_THIRD_PARTY}")
SET(CMAKE_CXX_FLAGS "-std=c++14 -nostdinc++")
# no libstdc++
SET(CMAKE_SHARED_LINKER_FLAGS "-nodefaultlibs")
SET(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")

GET_FILENAME_COMPONENT(PATH_TO_USD_LIBS "../../Binaries/Linux/x86_64-unknown-linux-gnu" ABSOLUTE)
message(STATUS "PATH_TO_USD_LIBS=${PATH_TO_USD_LIBS}")


INCLUDE_DIRECTORIES(Source/Public ../ThirdParty/Linux/include ../ThirdParty/Linux/include/python2.7 ${UE4_THIRD_PARTY}/Linux/LibCxx/include ${UE4_THIRD_PARTY}/Linux/LibCxx/include/c++/v1)
ADD_DEFINITIONS(-DNDEBUG -DUNREALUSDWRAPPER_EXPORTS -DTF_NO_GNU_EXT -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")

LINK_DIRECTORIES(${PATH_TO_USD_LIBS})
LINK_LIBRARIES(
	-larch 
	-lgf  
	-lkind  
	-lplug
	-ltbb
	-ltracelite
	-lusdHydra
      	-lusdShade
      	-lusdUI
	-lvt
	-lar
	-ljs
	-lpcp
	-lsdf
        -ltf
	-lusdGeom
	-lusdRi
	-lusd
	-lusdUtils
	-lwork

	# these need to be after everything else
	${UE4_THIRD_PARTY}/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a
	${UE4_THIRD_PARTY}/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a
	-lm
	-lc
	-lgcc_s
	-lgcc
	)
# we don't want to add link directories to RPATH as this will most likely be incorrect
SET(CMAKE_SKIP_BUILD_RPATH TRUE)
# we want to set our RPATH to "."
SET(CMAKE_INSTALL_RPATH "\$ORIGIN")
# and we want to set it at the moment of build since this file does not have an install step
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)


ADD_LIBRARY(UnrealUSDWrapper SHARED Source/Private/UnrealUSDWrapper.cpp Source/Public/UnrealUSDWrapper.h)

