project(nvTriStrip)

cmake_minimum_required(VERSION 2.8.8)

IF (CMAKE_BUILD_TYPE STREQUAL "Debug") 
add_library(nvtristripd STATIC Src/NvTriStrip.cpp Src/NvTriStrip.h Src/NvTriStripObjects.cpp Src/NvTriStripObjects.h Src/VertexCache.h)
SET_TARGET_PROPERTIES(nvtristripd PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF (CMAKE_BUILD_TYPE STREQUAL "Debug") 

IF (CMAKE_BUILD_TYPE STREQUAL "Release") 
add_library(nvtristrip STATIC Src/NvTriStrip.cpp Src/NvTriStrip.h Src/NvTriStripObjects.cpp Src/NvTriStripObjects.h Src/VertexCache.h)
SET_TARGET_PROPERTIES(nvtristrip PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF (CMAKE_BUILD_TYPE STREQUAL "Release") 
