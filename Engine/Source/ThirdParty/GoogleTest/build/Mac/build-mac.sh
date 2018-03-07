#!/bin/sh
# Prerequisites:
#  xcode
#  cmake 3.5

#####################
# configuration
CONFIG=MinSizeRel
if [ -n "$1" ]
 then
	CONFIG=$1
fi

GTEST_SDK=$(pwd)/../google-test-source
OUTPUT_LIBS=$(pwd)/../../lib/Mac/$CONFIG
OUTPUT_DIR=$(pwd)/Artifacts_$CONFIG

SHAREDLIBS=OFF
if [ "$2" = "Shared" ]; then
	SHAREDLIBS=ON
	OUTPUT_LIBS=$(pwd)/../../lib/Mac/${CONFIG}_Shared
	OUTPUT_DIR=$(pwd)/Artifacts_${CONFIG}_Shared
fi

echo "$OUTPUT_LIBS"

#####################
# create output directories
mkdir -p $OUTPUT_DIR
mkdir -p $OUTPUT_LIBS


#####################
# unpack source if needed
if [ ! -d "$GTEST_SDK" ]; then
	pushd $(pwd)/../
	bash uncompress_and_patch.sh
	popd
fi


#####################
# config cmake project
pushd $OUTPUT_DIR 
cmake -D BUILD_SHARED_LIBS:BOOL=$SHAREDLIBS -D CMAKE_OSX_DEPLOYMENT_TARGET=10.9 -G Xcode $GTEST_SDK
popd


#####################
# compile project
pushd $OUTPUT_DIR 
xcodebuild -target ALL_BUILD -configuration $CONFIG
popd


#####################
# remove existing binaries
rm -rfv $OUTPUT_LIBS/*


#####################
# copy new binaries
cp $OUTPUT_DIR/googlemock/gtest/$CONFIG/* $OUTPUT_LIBS
cp $OUTPUT_DIR/googlemock/$CONFIG/* $OUTPUT_LIBS


#####################
# update embedded shared library location
if [ $SHAREDLIBS = ON ]; then
	install_name_tool -id @rpath/libgmock_main.dylib $OUTPUT_LIBS/libgmock_main.dylib
	install_name_tool -id @rpath/libgmock.dylib $OUTPUT_LIBS/libgmock.dylib
	install_name_tool -id @rpath/libgtest_main.dylib $OUTPUT_LIBS/libgtest_main.dylib
	install_name_tool -id @rpath/libgtest.dylib $OUTPUT_LIBS/libgtest.dylib
fi

