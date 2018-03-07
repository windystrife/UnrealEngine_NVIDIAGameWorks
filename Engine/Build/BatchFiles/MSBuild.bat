@echo off

rem ## Unreal Engine 4 Visual Studio MSBuild Execution Script
rem ## Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script is expecting to exist in the UE4 Engine/Build/BatchFiles directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

setlocal

rem ## Determine path to MSBuild
call "%~dp0GetMSBuildPath.bat"

rem ## Call MSBuild, passing in any supplied parameters
%MSBUILD_EXE% /nologo %*