@ECHO OFF

REM Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set VS2013_X86_PATH="%PATH_TO_CMAKE_FILE%\..\Win32\VS2013\Build"
set VS2013_X64_PATH="%PATH_TO_CMAKE_FILE%\..\Win64\VS2013\Build"
set VS2015_X86_PATH="%PATH_TO_CMAKE_FILE%\..\Win32\VS2015\Build"
set VS2015_X64_PATH="%PATH_TO_CMAKE_FILE%\..\Win64\VS2015\Build"

REM Build for VS2013 (32-bit)
echo Generating HarfBuzz solution for VS2013 (32-bit)...
if exist %VS2013_X86_PATH% (rmdir %VS2013_X86_PATH% /s/q)
mkdir %VS2013_X86_PATH%
cd %VS2013_X86_PATH%
cmake -G "Visual Studio 12 2013" %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz solution for VS2013 (32-bit, Debug)...
"%VS120COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Debug
echo Building HarfBuzz solution for VS2013 (32-bit, Release)...
"%VS120COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2013_X86_PATH%\harfbuzz.dir\Debug\vc120.pdb" "%VS2013_X86_PATH%\..\Debug\harfbuzz.pdb"
copy /B/Y "%VS2013_X86_PATH%\harfbuzz.dir\RelWithDebInfo\vc120.pdb" "%VS2013_X86_PATH%\..\RelWithDebInfo\harfbuzz.pdb"
rmdir %VS2013_X86_PATH% /s/q

REM Build for VS2013 (64-bit)
echo Generating HarfBuzz solution for VS2013 (64-bit)...
if exist %VS2013_X64_PATH% (rmdir %VS2013_X64_PATH% /s/q)
mkdir %VS2013_X64_PATH%
cd %VS2013_X64_PATH%
cmake -G "Visual Studio 12 2013 Win64" %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz solution for VS2013 (64-bit, Debug)...
"%VS120COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Debug
echo Building HarfBuzz solution for VS2013 (64-bit, Release)...
"%VS120COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2013_X64_PATH%\harfbuzz.dir\Debug\vc120.pdb" "%VS2013_X64_PATH%\..\Debug\harfbuzz.pdb"
copy /B/Y "%VS2013_X64_PATH%\harfbuzz.dir\RelWithDebInfo\vc120.pdb" "%VS2013_X64_PATH%\..\RelWithDebInfo\harfbuzz.pdb"
rmdir %VS2013_X64_PATH% /s/q

REM Build for VS2015 (32-bit)
echo Generating HarfBuzz solution for VS2015 (32-bit)...
if exist %VS2015_X86_PATH% (rmdir %VS2015_X86_PATH% /s/q)
mkdir %VS2015_X86_PATH%
cd %VS2015_X86_PATH%
cmake -G "Visual Studio 14 2015" %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz solution for VS2015 (32-bit, Debug)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Debug
echo Building HarfBuzz solution for VS2015 (32-bit, Release)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2015_X86_PATH%\harfbuzz.dir\Debug\harfbuzz.pdb" "%VS2015_X86_PATH%\..\Debug\harfbuzz.pdb"
copy /B/Y "%VS2015_X86_PATH%\harfbuzz.dir\RelWithDebInfo\harfbuzz.pdb" "%VS2015_X86_PATH%\..\RelWithDebInfo\harfbuzz.pdb"
rmdir %VS2015_X86_PATH% /s/q

REM Build for VS2015 (64-bit)
echo Generating HarfBuzz solution for VS2015 (64-bit)...
if exist %VS2015_X64_PATH% (rmdir %VS2015_X64_PATH% /s/q)
mkdir %VS2015_X64_PATH%
cd %VS2015_X64_PATH%
cmake -G "Visual Studio 14 2015 Win64" %PATH_TO_CMAKE_FILE%
echo Building HarfBuzz solution for VS2015 (64-bit, Debug)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build Debug
echo Building HarfBuzz solution for VS2015 (64-bit, Release)...
"%VS140COMNTOOLS%\..\IDE\devenv.exe" harfbuzz.sln /Build RelWithDebInfo
cd %PATH_TO_CMAKE_FILE%
copy /B/Y "%VS2015_X64_PATH%\harfbuzz.dir\Debug\harfbuzz.pdb" "%VS2015_X64_PATH%\..\Debug\harfbuzz.pdb"
copy /B/Y "%VS2015_X64_PATH%\harfbuzz.dir\RelWithDebInfo\harfbuzz.pdb" "%VS2015_X64_PATH%\..\RelWithDebInfo\harfbuzz.pdb"
rmdir %VS2015_X64_PATH% /s/q

endlocal
