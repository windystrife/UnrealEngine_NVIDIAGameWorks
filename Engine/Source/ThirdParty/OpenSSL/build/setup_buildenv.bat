REM @echo off

REM * script to setup a command prompt seesion for building with visual studio
REM * setup_buildenv <2012/2013/2015> <x86/x64>

set PROGFILES=%ProgramFiles%
if not "%ProgramFiles(x86)%" == "" set PROGFILES=%ProgramFiles(x86)%

REM select compiler (default to 2013)
set COMPILER_VER="2013"
if not "%1" == "" set COMPILER_VER=%1

if "%COMPILER_VER%" == "2012" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 11.0"
	set VCVERSION=11
)
if "%COMPILER_VER%" == "2013" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 12.0"
	set VCVERSION=12
)
if "%COMPILER_VER%" == "2015" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 14.0"
	set VCVERSION=14
)

REM select architecture (default to 64 bit)
set ARCH_VER="x64"
if not "%2" == "" set ARCH_VER=%2

REM if running the same build config in the same command prompt session this skips the VC setup
REM if "%VCSETUP_USED%" == "%COMPILER_VER%\%ARCH_VER%" (
REM 	echo Skipping VC setup(%COMPILER_VER%\%ARCH_VER%)
REM )
REM if not "%VCSETUP_USED%" == "%COMPILER_VER%\%ARCH_VER%" (
	call %MSVCDIR%\VC\vcvarsall.bat %ARCH_VER%
REM 	set VCSETUP_USED=%COMPILER_VER%\%ARCH_VER%
REM )
