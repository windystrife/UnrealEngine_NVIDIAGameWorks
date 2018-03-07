@echo off
REM * build Google Test as static lib
REM * unzip google-test-source.7z in place to '\ThirdParty\GoogleTest\build\google-test-source' before running

set GTEST_SDK=%cd%\..\google-test-source

set PROGFILES=%ProgramFiles%
if not "%ProgramFiles(x86)%" == "" set PROGFILES=%ProgramFiles(x86)%

set SHAREDLIBS=OFF
if "%4" == "Shared" set SHAREDLIBS=ON

REM select build configuration (default to MinSizeRel) other possible configurations include: Debug, Release, RelWithDebInfo
set CONFIG=MinSizeRel
if not "%3" == "" set CONFIG=%3%

REM select architecture (default to 64 bit)
set ARCH_VER=x64
if not "%2" == "" set ARCH_VER=%2%

REM select compiler (default to 2015)
set COMPILER_VER=2015
if not "%1" == "" set COMPILER_VER=%1

if "%COMPILER_VER%" == "2013" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 12.0"
	set VCVERSION=12
	set MSBUILDDIR="%PROGFILES%\MSBuild\12.0\Bin"
	
	if "%ARCH_VER%" == "x64" (
		set CMAKE_GENERATOR="Visual Studio 12 2013 Win64"
	)
	
	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR="Visual Studio 12 2013"
	)
)
if "%COMPILER_VER%" == "2015" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 14.0"
	set VCVERSION=14
	set MSBUILDDIR="%PROGFILES%\MSBuild\14.0\Bin"

	if "%ARCH_VER%" == "x64" (
		set CMAKE_GENERATOR="Visual Studio 14 2015 Win64"
	)
	
	if "%ARCH_VER%" == "x86" (
		set CMAKE_GENERATOR="Visual Studio 14 2015"
	)
)

REM setup output directory
set OUTPUT_DIR=%cd%\Artifacts_VS%COMPILER_VER%_%ARCH_VER%_%CONFIG%
if "%4" == "Shared" (
	set OUTPUT_DIR=%OUTPUT_DIR%_Shared
)

rmdir /s /q %OUTPUT_DIR%
mkdir %OUTPUT_DIR%

REM ensure source has been unpacked
if not exist %GTEST_SDK% (
	pushd %cd%\..\
	call uncompress_and_patch
	popd
)

REM config cmake project
pushd %OUTPUT_DIR%
cmake -D BUILD_SHARED_LIBS:BOOL=%SHAREDLIBS% -D gtest_force_shared_crt:BOOL=ON -D gtest_disable_pthreads:BOOL=ON -G %CMAKE_GENERATOR% %GTEST_SDK%
popd

REM build project
pushd %MSBUILDDIR%
msbuild %OUTPUT_DIR%\googletest-distribution.sln /target:ALL_BUILD /p:PlatformTarget=%ARCH_VER%;Configuration="%CONFIG%"
popd

REM setup binary output directory
set OUTPUT_LIBS=%cd%\..\..\lib\Win64\VS%COMPILER_VER%\%CONFIG%
if "%ARCH_VER%" == "x86" (
	set OUTPUT_LIBS=%cd%\..\..\lib\Win32\VS%COMPILER_VER%\%CONFIG%
)

if "%4" == "Shared" (
	set OUTPUT_LIBS=%OUTPUT_LIBS%_Shared
)

REM delete any existing library output directories
rmdir /s /q %OUTPUT_LIBS%
mkdir %OUTPUT_LIBS%

REM copy binaries
mkdir %OUTPUT_LIBS%
xcopy /s /c /d /y %OUTPUT_DIR%\googlemock\gtest\%CONFIG%\*.* %OUTPUT_LIBS%\
xcopy /s /c /d /y %OUTPUT_DIR%\googlemock\%CONFIG%\*.* %OUTPUT_LIBS%\


