#!/bin/bash
# * build openssl as static lib on Unix-like architectures. (Currently only OS X)
# * build_openssl.sh [source version]

ROOT_DIR=$(pwd)
OPENSSL_SDK=${1:-1.0.2g}
OPENSSLSDK_ROOT=$ROOT_DIR/$OPENSSL_SDK/openssl-$OPENSSL_SDK
BUILD_DIR=$ROOT_DIR/$OPENSSL_SDK/Builds

if [[ $(uname) = 'Darwin' ]]
then
	OPENSSL_DESTINATION_RELEASE=Mac/release
	BUILD_FINAL_DESTINATION=Mac
	OPENSSL_CONFIG_RELEASE="darwin64-x86_64-cc no-gost zlib no-shared"
	export MACOSX_DEPLOYMENT_TARGET=10.9
else
	echo "Unsupported OS. Please add platform specific defs for" $(uname) >&2
	exit -1
fi

# uncompress source if needed
if [[ ! -d $OPENSSLSDK_ROOT ]]
then
	pushd $ROOT_DIR/$OPENSSL_SDK
	tar xvzf openssl-$OPENSSL_SDK.tar.gz
	popd
fi

# start build
pushd $OPENSSLSDK_ROOT
perl Configure $OPENSSL_CONFIG_RELEASE --prefix=$BUILD_DIR/$OPENSSL_DESTINATION_RELEASE
make reallyclean
make
make install
popd

# copy include files from the release directory to the general include folder
mkdir -p ../$OPENSSL_SDK/include/$BUILD_FINAL_DESTINATION
cp -R $BUILD_DIR/$OPENSSL_DESTINATION_RELEASE/include/* ../$OPENSSL_SDK/include/$BUILD_FINAL_DESTINATION

# copy libraries to general library folder
mkdir -p ../$OPENSSL_SDK/lib/$BUILD_FINAL_DESTINATION
# copy release libraries to general library folder
cp -R $BUILD_DIR/$OPENSSL_DESTINATION_RELEASE/lib/libcrypto.a ../$OPENSSL_SDK/lib/$BUILD_FINAL_DESTINATION
cp -R $BUILD_DIR/$OPENSSL_DESTINATION_RELEASE/lib/libssl.a ../$OPENSSL_SDK/lib/$BUILD_FINAL_DESTINATION
# remove the build path from the openssl config header

perl -p -i -e's/#define (OPENSSLDIR|ENGINESDIR) ".+?"/#define $1 ""/' ../$OPENSSL_SDK/include/$BUILD_FINAL_DESTINATION/openssl/opensslconf.h