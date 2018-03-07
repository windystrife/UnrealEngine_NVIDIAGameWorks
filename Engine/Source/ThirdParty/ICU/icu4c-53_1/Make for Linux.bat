@echo off
REM Batch file for configuring Linux platforms of ICU
REM Run this from the ICU directory
ECHO Remember to compile Windows version first, this is needed when cross-compiling

setlocal
set CYGWIN=winsymlinks:native

REM Linux Configs
if not exist ./Linux (
	echo Error: Linux directory does not exist. Did you forget to run configuration?
	goto:eof)
cd ./Linux

	REM x86_64 Config
	if not exist ./x86_64-unknown-linux-gnu (
		echo Error: x86_64-unknown-linux-gnu directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./x86_64-unknown-linux-gnu
		set PATH=%LINUX_ROOT%\bin;%PATH%;
		bash -c 'make clean'
		bash -c 'make all'
	cd ../

REM Back to root
cd ../
endlocal
