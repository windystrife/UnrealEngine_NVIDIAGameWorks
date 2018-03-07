#!/bin/sh

#
# Script that prepares to build CEF for UE4.
#

set -e

SCRIPT_DIR=`pwd`

#
# Setup variables.
#
CEF_VERSION_MAJOR=3
CEF_VERSION_MINOR=2623
CEF_VERSION_PATCH=1395
CEF_COMMIT_POINT=g3034273

#
# Create file name version.
#
CEF_VERSION=$CEF_VERSION_MAJOR.$CEF_VERSION_MINOR.$CEF_VERSION_PATCH.$CEF_COMMIT_POINT
CEF3_DIR=cef_binary_${CEF_VERSION}_linux64


#
# Setup build directory.
#
BUILD_DIR=$SCRIPT_DIR/../../$CEF3_DIR

#
# Select the build configuration.
#
BUILD_TYPE=Release


cd $BUILD_DIR


PatchSource() {

	#
	# Patch CMakeLists.txt so that it's not using -nostdinc++ and uses LibCxx.
	# -i = inplace replacement
	# todo If this runs twice the result will be wrong.
	#
	sed -i -e '207d' -e '206a set(CEF_CXX_COMPILER_FLAGS      "-nostdinc++ -fno-exceptions -fno-rtti -fno-threadsafe-statics -fvisibility-inlines-hidden -std=gnu++11 -Wsign-compare")' CMakeLists.txt
	sed -i -e '4a include_directories( ../../../Linux/LibCxx/include ../../../Linux/LibCxx/include/c++/v1)' libcef_dll/CMakeLists.txt

	#
	# Patch workaround for "redefinition of typedef".
	# todo If this is run twice the result will be wrong.
	#
	sed -i -e '44a #ifndef PLATFORM_LINUX' -e '51a #endif' include/base/cef_basictypes.h

	#
	# Patch OVERRIDE compile error.
	#
	sed -i -e 's+OVERRIDE+override+g' include/cef_base.h

	#
	# Patch gyp build folder names.
	#
	sed -i -e 's+cefclient/+tests/cefclient/+g' cef_paths2.gypi
	sed -i -e 's+cefsimple/+tests/cefsimple/+g' cef_paths2.gypi

}


BuildCEF() {

	#
	# Build Debug version.
	#

	rm -rf build_debug
	mkdir build_debug
	cd build_debug
	echo "Configuring CEF3... log is located at `pwd`/config.log"
	cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/Toolchain-clang.cmake > config_debug.log 2>&1
	make clean


	echo "Building CEF3... log is located at `pwd`/build.log"
	make -j4 libcef_dll_wrapper > build_debug.log 2>&1

	cd ..

	#
	# Build Release configuration.
	#
	rm -rf build_release
	mkdir build_release
	cd build_release
	echo "Configuring CEF3... log is located at `pwd`/config.log"
	cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/Toolchain-clang.cmake > config_release.log 2>&1
	make clean


	echo "Building CEF3... log is located at `pwd`/build.log"
	make -j4 libcef_dll_wrapper > build_release.log 2>&1

	cd ..

}

CopyFiles() {

	#
	# Copy files to their needed directories.
	#
	mkdir -pv ../../../../Binaries/ThirdParty/CEF3/Linux/

	#
	# Copy to Binaries/ThirdParty/CEF3/Linux
	#
	cd ../../../../Binaries/ThirdParty/CEF3/Linux/
	cp --remove-destination -rf ../../../../Source/ThirdParty/CEF3/$CEF3_DIR/Resources/ .
	cp --remove-destination ../../../../Source/ThirdParty/CEF3/$CEF3_DIR/$BUILD_TYPE/libcef.so .
	cp --remove-destination ../../../../Source/ThirdParty/CEF3/$CEF3_DIR/$BUILD_TYPE/natives_blob.bin .
	cp --remove-destination ../../../../Source/ThirdParty/CEF3/$CEF3_DIR/$BUILD_TYPE/snapshot_blob.bin .
	cp --remove-destination ../../../../Source/ThirdParty/CEF3/$CEF3_DIR/Resources/icudtl.dat .

}

PatchSource
BuildCEF
CopyFiles

set +e
