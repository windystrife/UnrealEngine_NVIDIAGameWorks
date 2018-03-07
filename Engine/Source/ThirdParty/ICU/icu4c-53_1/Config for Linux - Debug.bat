@echo off
REM Batch file for configuring Linux (cross-compile) of ICU
REM Run this from the ICU directory
ECHO Remember to compile Windows version first, this is needed when cross-compiling

setlocal
set CYGWIN=winsymlinks:native

REM Linux Configs
if not exist ./Linux mkdir Linux
cd ./Linux

	REM x86_64 Config
	if not exist ./x86_64-unknown-linux-gnu mkdir x86_64-unknown-linux-gnu
	cd ./x86_64-unknown-linux-gnu
		set PATH=%LINUX_ROOT%\bin;%PATH%;
		bash -c '../../ConfigForLinux-Debug.sh'
	cd ../
	
REM Back to root
cd ../
endlocal
