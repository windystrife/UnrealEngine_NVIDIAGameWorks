#!/bin/sh

echo
echo Setting up Unreal Engine 4 project files...
echo

# this is located inside an extra 'Mac' path unlike the Windows variant.

if [ ! -d ../../../Binaries/DotNET ]; then
 echo GenerateProjectFiles ERROR: It looks like you're missing some files that are required in order to generate projects.  Please check that you've downloaded and unpacked the engine source code, binaries, content and third-party dependencies before running this script.
 exit 1
fi

if [ ! -d ../../../Source ]; then
 echo GenerateProjectFiles ERROR: This script file does not appear to be located inside the Engine/Build/BatchFiles/Mac directory.
 exit 1
fi

source SetupMono.sh "`dirname "$0"`"

if [ -f ../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj ]; then
	xbuild ../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj /property:Configuration="Development" /verbosity:quiet /nologo |grep -i error
fi

# pass all parameters to UBT
mono ../../../Binaries/DotNET/UnrealBuildTool.exe -projectfiles "$@"
