@echo off

rem ## Unreal Engine 4 cleanup script
rem ## Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script is expecting to exist in the UE4 root directory.  It will not work correctly
rem ## if you copy it to a different location and run it.
rem ##
rem ##     %1 is the game name
rem ##     %2 is the platform name
rem ##     %3 is the configuration name

setlocal
echo Cleaning %1 Binaries...

rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
rem ## verify that our relative path to the /Engine/Source directory is correct
if not exist "%~dp0..\..\Source" goto Error_BatchFileInWrongLocation


rem ## Change the CWD to /Engine/Source.  We always need to run UnrealBuildTool from /Engine/Source!
pushd "%~dp0..\..\Source"
if not exist ..\Build\BatchFiles\Clean.bat goto Error_BatchFileInWrongLocation

rem ## If this is an installed build, we don't need to rebuild UBT. Go straight to cleaning.
if exist ..\Build\InstalledBuild.txt goto ReadyToClean

rem ## Get the path to MSBuild
call "%~dp0GetMSBuildPath.bat"
if errorlevel 1 goto Error_NoVisualStudioEnvironment


rem ## Compile UBT if the project file exists
if not exist Programs\UnrealBuildTool\UnrealBuildTool.csproj goto NoProjectFile
%MSBUILD_EXE% /nologo /verbosity:quiet Programs\UnrealBuildTool\UnrealBuildTool.csproj /property:Configuration=Development /property:Platform=AnyCPU
if errorlevel 1 goto Error_UBTCompileFailed
:NoProjectFile


rem ## Execute UBT
:ReadyToClean
if not exist ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe goto Error_UBTMissing
..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe %* -clean
goto Exit


:Error_BatchFileInWrongLocation
echo Clean ERROR: The batch file does not appear to be located in the UE4/Engine/Build/BatchFiles directory.  This script must be run from within that directory.
pause
goto Exit

:Error_NoVisualStudioEnvironment
echo GenerateProjectFiles ERROR: A valid version of Visual Studio does not appear to be installed.
pause
goto Exit

:Error_UBTCompileFailed
echo Clean ERROR: Failed to build UnrealBuildTool.
pause
goto Exit

:Error_UBTMissing
echo Clean ERROR: UnrealBuildTool.exe not found in ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe 
pause
goto Exit

:Error_CleanFailed
echo Clean ERROR: Clean failed.
pause
goto Exit

:Exit

