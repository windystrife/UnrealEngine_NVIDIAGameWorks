#!/bin/bash

## Unreal Engine 4 XBuild setup script
## Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

## This script is expecting to exist in the UE4/Engine/Build/BatchFiles directory.  It will not work correctly
## if you copy it to a different location and run it.

echo
echo Running XBuild...
echo

# source "`dirname "$0"`/SetupMono.sh" "`dirname "$0"`"

# put ourselves into Engine directory (two up from location of this script)
pushd "`dirname "$0"`/../../.."

if [ ! -f Build/BatchFiles/Linux/RunXBuild.sh ]; then
	echo RunXBuild ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
	exit 1
fi

xbuild /verbosity:quiet /nologo "$@" |grep -i error
if [ $? -ne 1 ]; then
	exit 1
else
	exit 0
fi
