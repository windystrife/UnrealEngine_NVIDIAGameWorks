@echo off 
rem ## Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


echo Generating globalization files...

rem ## First, make sure the batch file exists in the folder we expect it to.  This is necessary in order to verify that our relative paths are correct
if not exist %~dp0..\..\Binaries\Win64 goto Error_BatchFileInWrongLocation

rem ## Change the CWD to /Engine/Binaries/Win64.
pushd %~dp0..\..\Binaries\Win64
if not exist ..\..\Build\BatchFiles\GenerateGlobalizationFiles.bat goto Error_BatchFileInWrongLocation

SET GENERATED_CONFIG=../../../Engine/Config/Localization/ResourceFileGen.ini
if not "%1" == "" (
	SET GENERATED_CONFIG=%1
)

if not exist %GENERATED_CONFIG% goto Error_ConfigFileGenFailed

rem ## TODO: When -DisableSCCSubmit appears in a build we can pass in the following p4 flags to do auto checkout. -EnableSCC -DisableSCCSubmit.  For now files need to be checked out manually
UE4Editor.exe -run=GatherText -config=%GENERATED_CONFIG% -unattended > %~dp0\GenerateGlobalizationLog.txt
if not %ERRORLEVEL% == 0 goto Error_ResourceFileGenFailed

rem ## Success!
goto Exit


:Error_BatchFileInWrongLocation
echo GenerateGlobalizationFiles ERROR: The batch file does not appear to be located in the /Engine/Build/BatchFiles directory.  This script must be run from within that directory.
pause
goto Exit

:Error_ConfigFileGenFailed
echo GenerateGlobalizationFiles ERROR: Failed to find config file in the following location: "%GENERATED_CONFIG%"
pause
goto Exit

:Error_ResourceFileGenFailed
echo GenerateGlobalizationFiles ERROR: Failed to generate localization resource files.  See GenerateGlobalizationLog.txt for details.
pause
goto Exit


:Exit
rem ## Restore original CWD in case we change it
popd

