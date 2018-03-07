#!/bin/bash -x
set -e

#
# This script automates the build process described by webrtc:
# https://code.google.com/p/webrtc/source/browse/trunk/talk/app/webrtc/objc/README
#
SCRIPT_HOME=$(pwd)
export BUILD_MODE=Release
export OUTPUT_DIR=out_mac

ROOT=${SCRIPT_HOME}
if [ -z "${WEBRTC_BRANCH}" ]; then
    WEBRTC_BRANCH=40
fi
WEBRTC_ROOT=$ROOT/src

export WEBRTC_OUT=$OUTPUT_DIR/$BUILD_MODE
if [ -z $WEBRTC_REVISION ]; then
    export SYNC_REVISION=""
else
    export SYNC_REVISION="-r $WEBRTC_REVISION"
fi

gclient config --unmanaged --verbose --name=src https://chromium.googlesource.com/external/webrtc.git
perl -i -wpe "s/svn\/trunk/svn\/branches\/${WEBRTC_BRANCH}/g" .gclient

echo "target_os = ['mac']" >> .gclient
gclient sync --verbose $SYNC_REVISION

pushd ${WEBRTC_ROOT}
export GYP_DEFINES="enable_tracing=1 build_with_libjingle=1 build_with_chromium=0 libjingle_objc=1 OS=mac target_arch=x64 use_system_ssl=0 use_openssl=1 use_nss=0 use_system_libcxx=1 clang=1 mac_sdk=10.9"
if [ "1" == "$DEBUG" ]; then
    export GYP_DEFINES="$GYP_DEFINES fastbuild=0"
else
    export GYP_DEFINES="$GYP_DEFINES fastbuild=1"
fi
export GYP_GENERATORS="ninja"
export GYP_GENERATOR_FLAGS="output_dir=$OUTPUT_DIR"
export GYP_CROSSCOMPILE=1
gclient runhooks -f
ninja -C $WEBRTC_OUT -t clean || ls $WEBRTC_OUT
#ninja -v -C $WEBRTC_OUT libjingle_peerconnection_objc_test
ninja -v -C $WEBRTC_OUT libjingle rtc_base rtc_base_approved rtc_xmllite rtc_xmpp boringssl expat

ARTIFACT=$OUTPUT_DIR/artifacts
rm -rf $ARTIFACT/lib/$BUILD_MODE || echo "clean $ARTIFACT"
mkdir -p $ARTIFACT/lib/$BUILD_MODE
mkdir -p $ARTIFACT/include
cp $WEBRTC_ROOT/$WEBRTC_OUT/*.a $ARTIFACT/lib/$BUILD_MODE
HEADERS_OUT=`find net talk third_party webrtc -name *.h`
for HEADER in $HEADERS_OUT
do
    HEADER_DIR=`dirname $HEADER`
    mkdir -p $ARTIFACT/include/$HEADER_DIR
    cp $HEADER $ARTIFACT/include/$HEADER
done

popd