@echo off
REM * build libcurl as static lib
REM * unzip curl-*.zip to base directory of this batch file before running

set ROOT_DIR=%cd%
set CURL_SDK=%ROOT_DIR%\curl-7.41.0
set CURL_SDK_BUILDS=%CURL_SDK%\builds

set PROGFILES=%ProgramFiles%
if not "%ProgramFiles(x86)%" == "" set PROGFILES=%ProgramFiles(x86)%

REM select compiler (default to 2015)
set COMPILER_VER="2015"
if not "%1" == "" set COMPILER_VER=%1

if "%COMPILER_VER%" == "2015" (
	set MSVCDIR="%PROGFILES%\Microsoft Visual Studio 14.0"
	set VCVERSION=14
)

REM select architecture (default to 64 bit)
set ARCH_VER="x64"
if not "%2" == "" set ARCH_VER=%2%
call %MSVCDIR%\VC\vcvarsall.bat %ARCH_VER%
	
REM start build
pushd %CURL_SDK%\winbuild
nmake /f Makefile.vc mode=static VC=%VCVERSION% MACHINE=%ARCH_VER% ENABLE_IPV6=yes ENABLE_WINSSL=yes DEBUG=no GEN_PDB=yes
popd

set ARTIFACT_DIR=%ROOT_DIR%\Artifacts
set ARTIFACT_HEADERS=%ARTIFACT_DIR%\include\Windows\curl
mkdir %ARTIFACT_HEADERS%
for /r %CURL_SDK_BUILDS%\ %%f in (*.h) do @copy /y %%f %ARTIFACT_HEADERS%\%%~nxf

set ARTIFACT_LIBS=%ARTIFACT_DIR%\lib\Win64\VS%COMPILER_VER%
if not "%ARCH_VER%" == "x64" (
	set ARTIFACT_LIBS=%ARTIFACT_DIR%\lib\Win32\VS%COMPILER_VER%
)
mkdir %ARTIFACT_LIBS%
xcopy /s /c /d /y %CURL_SDK_BUILDS%\libcurl-vc%VCVERSION%-%ARCH_VER%-release-static-ipv6-sspi-winssl\lib\*.* %ARTIFACT_LIBS%\

