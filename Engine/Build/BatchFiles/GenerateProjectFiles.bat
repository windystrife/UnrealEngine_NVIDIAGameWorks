@echo off

rem ## Unreal Engine 4 Visual Studio project setup script
rem ## Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script is expecting to exist in the UE4 root directory.  It will not work correctly
rem ## if you copy it to a different location and run it.

setlocal
echo Setting up Unreal Engine 4 project files...


rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to
rem ## verify that our relative path to the /Engine/Source directory is correct
if not exist "%~dp0..\..\Source" goto Error_BatchFileInWrongLocation


rem ## Change the CWD to /Engine/Source.  We always need to run UnrealBuildTool from /Engine/Source!
pushd "%~dp0..\..\Source"
if not exist ..\Build\BatchFiles\GenerateProjectFiles.bat goto Error_BatchFileInWrongLocation


rem ## Check to make sure that we have a Binaries directory with at least one dependency that we know that UnrealBuildTool will need
rem ## in order to run.  It's possible the user acquired source but did not download and unpack the other prerequiste binaries.
if not exist ..\Binaries\DotNET\RPCUtility.exe goto Error_MissingBinaryPrerequisites


rem ## Get the path to MSBuild
call "%~dp0GetMSBuildPath.bat"
if errorlevel 1 goto Error_NoVisualStudioEnvironment


rem Check to see if the files in the UBT directory have changed. We conditionally include platform files from the .csproj file, but MSBuild doesn't recognize the dependency when new files are added. 
md ..\Intermediate\Build >nul 2>nul
dir /s /b Programs\UnrealBuildTool\*.cs >..\Intermediate\Build\UnrealBuildToolFiles.txt
fc /b ..\Intermediate\Build\UnrealBuildToolFiles.txt ..\Intermediate\Build\UnrealBuildToolPrevFiles.txt >nul 2>nul
if not errorlevel 1 goto SkipClean
copy /y ..\Intermediate\Build\UnrealBuildToolFiles.txt ..\Intermediate\Build\UnrealBuildToolPrevFiles.txt >nul
%MSBUILD_EXE% /nologo /verbosity:quiet Programs\UnrealBuildTool\UnrealBuildTool.csproj /property:Configuration=Development /property:Platform=AnyCPU /target:Clean
:SkipClean
%MSBUILD_EXE% /nologo /verbosity:quiet Programs\UnrealBuildTool\UnrealBuildTool.csproj /property:Configuration=Development /property:Platform=AnyCPU /target:Build
if errorlevel 1 goto Error_UBTCompileFailed

rem ## Run UnrealBuildTool to generate Visual Studio solution and project files
rem ## NOTE: We also pass along any arguments to the GenerateProjectFiles.bat here
..\Binaries\DotNET\UnrealBuildTool.exe -ProjectFiles %*
if errorlevel 1 goto Error_ProjectGenerationFailed

rem ## Success!
popd
exit /B 0


:Error_BatchFileInWrongLocation
echo.
echo GenerateProjectFiles ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
echo.
pause
goto Exit


:Error_MissingBinaryPrerequisites
echo.
echo GenerateProjectFiles ERROR: It looks like you're missing some files that are required in order to generate projects.  Please check that you've downloaded and unpacked the engine source code, binaries, content and third-party dependencies before running this script.
echo.
pause
goto Exit


:Error_NoVisualStudioEnvironment
echo.
echo GenerateProjectFiles ERROR: We couldn't find a valid installation of Visual Studio.  This program requires Visual Studio 2015.  Please check that you have Visual Studio installed, then verify that the HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\14.0\InstallDir (or HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\VisualStudio\14.0\InstallDir on 32-bit machines) registry value is set.  Visual Studio configures this value when it is installed, and this program expects it to be set to the '\Common7\IDE\' sub-folder under a valid Visual Studio installation directory.
echo.
pause
goto Exit


:Error_UBTCompileFailed
echo.
echo GenerateProjectFiles ERROR: UnrealBuildTool failed to compile.
echo.
pause
goto Exit


:Error_ProjectGenerationFailed
echo.
echo GenerateProjectFiles ERROR: UnrealBuildTool was unable to generate project files.
echo.
pause
goto Exit


:Exit
rem ## Restore original CWD in case we change it
popd
exit /B 1