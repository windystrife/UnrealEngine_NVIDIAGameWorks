#!/bin/sh

#
# Script that prepares to build CEF for UE4.
#

set -e

SCRIPT_DIR=`pwd`

#
# Setup build directory.
#

#
# Select the build configuration.
#
BUILD_TYPE=Release
TARGET=x86_64-unknown-linux-gnu
#TARGET=aarch64-unknown-linux-gnueabi

cd $BUILD_DIR


BuildLibs() {
	CMAKE_SOURCE_ROOT=$SCRIPT_DIR/../../glslang/src/glslang_lib
	LIBCXX_INCLUDE=$SCRIPT_DIR/../../../Linux/LibCxx/include/c++/v1/
	LIBCXX_ABI_INCLUDE=$SCRIPT_DIR/../../../Linux/LibCxx/include/
	LIBCXX_LIB_DIR=$SCRIPT_DIR/../../../Linux/LibCxx/lib/Linux/$TARGET

	cd $CMAKE_SOURCE_ROOT
	cmake -DUSE_CUSTOM_LIBCXX=ON -DLIBCXX_INCLUDE=$LIBCXX_INCLUDE -DLIBCXX_ABI_INCLUDE=$LIBCXX_ABI_INCLUDE -DLIBCXX_LIB_DIR=$LIBCXX_LIB_DIR -DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/Toolchain-clang.cmake .
	make -j4
}

CopyFiles() {

	DESTINATION_ROOT=$SCRIPT_DIR/../../glslang/lib/Linux/x86_64-unknown-linux-gnu
	SOURCE_ROOT=$SCRIPT_DIR/../../glslang/src/glslang_lib
	#
	# Copy files to their needed directories.
	#
	mkdir -pv $DESTINATION_ROOT


	#
	# Copy to Binaries/ThirdParty/CEF3/Linux
	#
	cp --remove-destination $SOURCE_ROOT/glslang/libglslang.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/glslang/OSDependent/Unix/libOSDependent.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/hlsl/libHLSL.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/OGLCompilersDLL/libOGLCompiler.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/SPIRV/libSPIRV.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/SPIRV/libSPVRemapper.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/SPIRV/libSPVRemapper.a $DESTINATION_ROOT 
	cp --remove-destination $SOURCE_ROOT/StandAlone/glslangValidator $SCRIPT_DIR/../../../../../Binaries/ThirdParty/glslang/
	cp --remove-destination $SOURCE_ROOT/StandAlone/spirv-remap $SCRIPT_DIR/../../../../../Binaries/ThirdParty/glslang/ 


}

BuildLibs
CopyFiles

set +e
