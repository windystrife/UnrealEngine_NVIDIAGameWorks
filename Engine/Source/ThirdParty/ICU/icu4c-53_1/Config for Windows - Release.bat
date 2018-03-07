@echo off
REM Batch file for configuring Windows platforms of ICU
REM Run this from the ICU directory

REM Win32 Configs
if not exist ./Win32 mkdir Win32
cd ./Win32

	REM VS2015 Config
	if not exist ./VS2015 mkdir VS2015
	cd ./VS2015
		call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x86
		bash -c 'CFLAGS="/Zp4" CXXFLAGS="/Zp4" CPPFLAGS="-DUCONFIG_NO_TRANSLITERATION=1" ../../Source/runConfigureICU Cygwin/MSVC --enable-static --enable-shared --with-library-bits=32 --with-data-packaging=files'
	cd ../

REM Back to root
cd ../

REM Win64 Configs
if not exist ./Win64 mkdir Win64
cd ./Win64

	REM VS2015 Config
	if not exist ./VS2015 mkdir VS2015
	cd ./VS2015
		call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" amd64
		bash -c 'CPPFLAGS="-DUCONFIG_NO_TRANSLITERATION=1" ../../Source/runConfigureICU Cygwin/MSVC --enable-static --enable-shared --with-library-bits=64 --with-data-packaging=files'
	cd ../

REM Back to root
cd ../