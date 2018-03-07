#!/bin/bash

Architecture=x86_64-unknown-linux-gnu
#Architecture=i686-unknown-linux-gnu
#Architecture=aarch64-unknown-linux-gnueabi

BuildWithOptions()
{
	local BuildDir=$1
	local SdlDir=$2
	local SdlLibName=$3
	shift
	shift
	shift
	local Options=$@

	rm -rf $BuildDir
	mkdir -p $BuildDir
	pushd $BuildDir
	cmake $Options $SdlDir
	#exit 0
	make -j 4
	mkdir -p $SdlDir/lib/Linux/$Architecture/
	cp --remove-destination libSDL2.a $SdlDir/lib/Linux/$Architecture/$SdlLibName
	popd
}

set -e
SDL_DIR=SDL-gui-backend
BUILD_DIR=build-$SDL_DIR

# build Debug with -fPIC so it's usable in any type of build
BuildWithOptions $BUILD_DIR-Debug ../$SDL_DIR libSDL2_fPIC_Debug.a -DCMAKE_BUILD_TYPE=Debug -DSDL_STATIC_PIC=ON
#exit 0
BuildWithOptions $BUILD_DIR-Release ../$SDL_DIR libSDL2.a -DCMAKE_BUILD_TYPE=Release
BuildWithOptions $BUILD_DIR-ReleasePIC ../$SDL_DIR libSDL2_fPIC.a -DCMAKE_BUILD_TYPE=Release -DSDL_STATIC_PIC=ON
set +e

