#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

set -e

SCRIPT_PATH=$0
if [ -L "$SCRIPT_PATH" ]; then
	SCRIPT_PATH=$(dirname "$SCRIPT_PATH")/$(readlink "$SCRIPT_PATH")
fi

cd "$(dirname "$SCRIPT_PATH")" && SCRIPT_PATH="`pwd`/$(basename "$SCRIPT_PATH")"

if [ ! -f ../../../Binaries/DotNET/GitDependencies.exe ]; then
	echo "Cannot find GitDependencies.exe. This script should be placed in Engine/Build/BatchFiles/Mac."
	exit 1
fi 

source SetupMono.sh "`pwd`"

cd ../../../..

if [ ! -f Engine/Binaries/ThirdParty/Mono/Mac/lib/libmsvcrt.dylib ]; then
	ln -s /usr/lib/libc.dylib Engine/Binaries/ThirdParty/Mono/Mac/lib/libmsvcrt.dylib
fi

mono Engine/Binaries/DotNET/GitDependencies.exe "$@"

pushd "$(dirname "$SCRIPT_PATH")" > /dev/null
sh FixDependencyFiles.sh
popd > /dev/null
