@echo off
REM Batch file for configuring Android platforms of ICU
REM Run this from the ICU directory

setlocal
set CYGWIN=winsymlinks:native

:SETARCH
set ARCH=""
if "%1"=="" goto USAGE

set Array=ARMv7 ARM64
for %%i in (%Array%) do (
	if %%i==%1 set ARCH="%1" 
)

if %ARCH%=="" goto USAGE

:SETDEBUG
set DEBUG=0
if "%2"=="d" set DEBUG=1

@echo flags %ARCH% %DEBUG%


REM Android Configs
if not exist ./Android (
	echo Error: Android directory does not exist. Did you forget to run configuration?
	goto:eof)
cd ./Android
	
	REM ARMv7 Make

if %ARCH%=="ARMv7" set PATH=%NDKROOT%\toolchains\llvm\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\arm-linux-androideabi\bin\;%PATH%
if %ARCH%=="ARMv7" (	
	
	if not exist ./ARMv7 (
		echo Error: ARMv7 directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./ARMv7			
		
	bash -c 'make clean'
	bash -c 'make all'
	cd ./data
	bash -c 'make'
	
	cd ..
	
	REM Copying libicudata.a to the lib directory for consistency 
	copy /y stubdata\libicudata.a lib\libicudata.a
	
	if %DEBUG%==1 (
		@echo WARNING - Renaming libs for debug.  You will need to rebuild release since you just over wrote the release libs.
		cd lib
		del "*d.a" 2>NUL
		for %%A in (*.a) do ren "%%~fA" "%%~nAd.*"
		cd ..
	)
	
	cd ../../
)

if %ARCH%=="ARM64" set PATH=%NDKROOT%\toolchains\llvm\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\aarch64-linux-android-4.9\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\aarch64-linux-android-4.9\prebuilt\windows-x86_64\aarch64-linux-android\bin\;%PATH%
if %ARCH%=="ARM64" (	
	
	if not exist ./ARM64 (
		echo Error: ARM64 directory does not exist. Did you forget to run configuration?
		goto:eof)
	cd ./ARM64			
		
	bash -c 'make clean'
	bash -c 'make all'
	cd ./data
	bash -c 'make'
	cd ..
	
	REM Copying libicudata.a to the lib directory for consistency 
	copy /y stubdata\libicudata.a lib\libicudata.a
	
	cd ../../
)


REM Back to root
cd ../

goto END

:USAGE
@echo Usage: %0 ARCH d
@echo ***Acceptable ARCH values are ARMv7 ARM64.  Not supported: x64 x86.
@echo ***The debug flag (d) is not necessary and only used on ARMv7.

:END
endlocal