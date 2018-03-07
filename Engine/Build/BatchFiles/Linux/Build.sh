#!/bin/sh

# This script gets can be used to build clean individual projects using UnrealBuildTool

set -e

cd "`dirname "$0"`/../../../.." 

# First make sure that the UnrealBuildTool is up-to-date
if ! xbuild /property:Configuration=Development /property:TargetFrameworkVersion=v4.5 /verbosity:quiet /nologo Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj; then
  echo "Failed to build to build tool (UnrealBuildTool)"
  exit 1
fi

echo "Building $1..."
mono Engine/Binaries/DotNET/UnrealBuildTool.exe "$@"
exit $?
