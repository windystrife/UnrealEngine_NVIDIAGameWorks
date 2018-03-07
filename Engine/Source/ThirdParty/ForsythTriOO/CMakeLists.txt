project(ForsythTriOO)

cmake_minimum_required(VERSION 2.8.8)

IF (CMAKE_BUILD_TYPE STREQUAL "Debug") 
add_library(ForsythTriOptimizerd STATIC Src/forsythtriangleorderoptimizer.cpp Src/forsythtriangleorderoptimizer.h)
SET_TARGET_PROPERTIES(ForsythTriOptimizerd PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF (CMAKE_BUILD_TYPE STREQUAL "Debug") 

IF (CMAKE_BUILD_TYPE STREQUAL "Release") 
add_library(ForsythTriOptimizer STATIC Src/forsythtriangleorderoptimizer.cpp Src/forsythtriangleorderoptimizer.h)
SET_TARGET_PROPERTIES(ForsythTriOptimizer PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF (CMAKE_BUILD_TYPE STREQUAL "Release") 
