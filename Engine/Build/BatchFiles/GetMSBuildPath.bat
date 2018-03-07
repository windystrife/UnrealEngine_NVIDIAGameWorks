@echo off

rem ## Unreal Engine 4 utility script
rem ## Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
rem ##
rem ## This script determines the path to MSBuild necessary to compile C# tools for the current version of the engine.
rem ## The discovered path is set to the MSBUILD_EXE environment variable on success.

set MSBUILD_EXE=

rem ## Check for MSBuild 15. This is installed alongside Visual Studio 2017, so we get the path relative to that.

call :ReadInstallPath Microsoft\VisualStudio\SxS\VS7 15.0 MSBuild\15.0\bin\MSBuild.exe
if not errorlevel 1 goto Succeeded

rem ## Try to get the MSBuild 14.0 path directly (see https://msdn.microsoft.com/en-us/library/hh162058(v=vs.120).aspx)

if exist "%ProgramFiles(x86)%\MSBuild\14.0\bin\MSBuild.exe" (
	set MSBUILD_EXE="%ProgramFiles(x86)%\MSBuild\14.0\bin\MSBuild.exe"
	goto Succeeded
)

rem ## Try to get the MSBuild 14.0 path from the registry

call :ReadInstallPath Microsoft\MSBuild\ToolsVersions\14.0 MSBuildToolsPath MSBuild.exe
if not errorlevel 1 goto Succeeded

rem ## Check for older versions of MSBuild. These are registered as separate versions in the registry.

call :ReadInstallPath Microsoft\MSBuild\ToolsVersions\12.0 MSBuildToolsPath MSBuild.exe
if not errorlevel 1 goto Succeeded

call :ReadInstallPath Microsoft\MSBuild\ToolsVersions\4.0 MSBuildToolsPath MSBuild.exe
if not errorlevel 1 goto Succeeded

rem ## Couldn't find anything
exit /B 1

rem ## Did manage to locate MSBuild
:Succeeded
exit /B 0

rem ## Subroutine to query the registry under HKCU/HKLM Win32/Wow64 software registry keys for a certain install directory.
rem ## Arguments: 
rem ##     %1 = Registry path under the 'SOFTWARE' registry key
rem ##     %2 = Value name
rem ##     %3 = Relative path under this directory to look for an installed executable.
:ReadInstallPath
for /f "tokens=2,*" %%A in ('REG.exe query HKCU\SOFTWARE\%1 /v %2 2^>Nul') do (
	if exist "%%B%%3" (
		set MSBUILD_EXE="%%B%3"
		exit /B 0
	)
)
for /f "tokens=2,*" %%A in ('REG.exe query HKLM\SOFTWARE\%1 /v %2 2^>Nul') do (
	if exist "%%B%3" (
		set MSBUILD_EXE="%%B%3"
		exit /B 0
	)
)
for /f "tokens=2,*" %%A in ('REG.exe query HKCU\SOFTWARE\Wow6432Node\%1 /v %2 2^>Nul') do (
	if exist "%%B%%3" (
		set MSBUILD_EXE="%%B%3"
		exit /B 0
	)
)
for /f "tokens=2,*" %%A in ('REG.exe query HKLM\SOFTWARE\Wow6432Node\%1 /v %2 2^>Nul') do (
	if exist "%%B%3" (
		set MSBUILD_EXE="%%B%3"
		exit /B 0
	)
)
exit /B 1
