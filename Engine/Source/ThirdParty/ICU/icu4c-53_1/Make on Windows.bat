@echo off
REM Batch file for configuring Windows platforms of ICU
REM Run this from the ICU directory

REM Win32 Configs
if not exist ./Win32 (
	echo Error: Win32 directory does not exist. Did you forget to run configuration?
	goto:eof)
cd ./Win32

	REM VS2015 Make
	if not exist ./VS2015 (
		echo Error: VS2015 directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./VS2015
		call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x86
		bash -c "make clean"
		bash -c "make all"
	cd ../
	
REM Back to root
cd ../

REM Win64 Configs
if not exist ./Win64 (
	echo Error: Win64 directory does not exist. Did you forget to run configuration?
	goto:eof)
cd ./Win64

	REM VS2015 Make
	if not exist ./VS2015 (
		echo Error: VS2015 directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./VS2015
		call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" amd64
		bash -c "make clean"
		bash -c "make all"
	cd ../
	
REM Back to root
cd ../
