#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

set -e

cd "`dirname "$0"`"

if [ ! -f Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh ]; then
	echo "GenerateProjectFiles ERROR: This script does not appear to be located \
       in the root UE4 directory and must be run from there."
  exit 1
fi 

if [ -f Setup.sh ]; then
	if [ ! -f .ue4dependencies ]; then
		echo "Please run Setup to download dependencies before generating project files."
		exit 1
	fi
fi

if [ "$(uname)" = "Darwin" ]; then
	cd Engine/Build/BatchFiles/Mac
	sh ./GenerateLLDBInit.sh
	sh ./GenerateProjectFiles.sh $@
else
    # assume (GNU/)Linux
	cd Engine/Build/BatchFiles/Linux
	bash ./GenerateProjectFiles.sh $@
fi
