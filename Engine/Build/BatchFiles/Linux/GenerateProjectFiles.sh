#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)

set -e

TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)
cd "${TOP_DIR}"

if [ ! -e Build/OneTimeSetupPerformed ]; then
	echo
	echo "******************************************************"
	echo "You have not run Setup.sh, the build will likely fail."
	echo "******************************************************"
fi

echo
echo Setting up Unreal Engine 4 project files...
echo

set -x
xbuild Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj \
  /verbosity:quiet /nologo \
  /p:TargetFrameworkVersion=v4.5 \
  /p:Configuration="Development"

# pass all parameters to UBT
mono Binaries/DotNET/UnrealBuildTool.exe -projectfiles "$@"
set +x
