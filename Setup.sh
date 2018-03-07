#!/bin/bash
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

set -e

cd "`dirname "$0"`"

if [ ! -f Engine/Binaries/DotNET/GitDependencies.exe ]; then
	echo "GitSetup ERROR: This script does not appear to be located \
       in the root UE4 directory and must be run from there."
	exit 1
fi 

if [ "$(uname)" = "Darwin" ]; then
	# Setup the git hooks
	if [ -d .git/hooks ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		rm -f .git/hooks/post-checkout
		rm -f .git/hooks/post-merge
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh .git/hooks/post-checkout
		ln -s ../../Engine/Build/BatchFiles/Mac/GitDependenciesHook.sh .git/hooks/post-merge
	fi

	# Get the dependencies for the first time
	Engine/Build/BatchFiles/Mac/GitDependencies.sh --prompt $@
else
	# Setup the git hooks
	if [ -d .git/hooks ]; then
		echo "Registering git hooks... (this will override existing ones!)"
		echo \#!/bin/sh >.git/hooks/post-checkout
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >>.git/hooks/post-checkout
		chmod +x .git/hooks/post-checkout

		echo \#!/bin/sh >.git/hooks/post-merge
		echo Engine/Build/BatchFiles/Linux/GitDependencies.sh >>.git/hooks/post-merge
		chmod +x .git/hooks/post-merge
	fi

	pushd Engine/Build/BatchFiles/Linux > /dev/null
	./Setup.sh "$@"
	popd > /dev/null
fi
