@echo off
REM Batch file for configuring Android platforms of ICU
REM Run this from the ICU directory

setlocal
set CYGWIN=winsymlinks:native

:SETARCH
set ARCH=""
if "%1"=="" goto USAGE

set Array=ARMv7 ARM64 x64 x86
for %%i in (%Array%) do (
	if %%i==%1 set ARCH="%1" 
)

if %ARCH%=="" goto USAGE

:SETDEBUG
set DEBUG=0
if "%2"=="d" set DEBUG=1

@echo flags %ARCH% %DEBUG%

REM Android Configs
if not exist ./Android mkdir Android 
cd ./Android


REM ARMv7 Config
REM Set path out here bc for unknown reason it errors inside if statement
if %ARCH%=="ARMv7" set PATH=%NDKROOT%\toolchains\llvm\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\arm-linux-androideabi\bin\;%PATH%
if %ARCH%=="ARMv7" (
	@echo Config For ARMv7 being run
	if not exist ./ARMv7 mkdir ARMv7 
	cd ./ARMv7
	bash -c '../ConfigForAndroid-armv7.sh %DEBUG%' 
	cd ../
)

	
REM ARM64 Config
REM Set path out here bc for unknown reason it errors inside if statement
if %ARCH%=="ARM64" set PATH=%NDKROOT%\toolchains\llvm\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\aarch64-linux-android-4.9\prebuilt\windows-x86_64\bin\;%NDKROOT%\toolchains\aarch64-linux-android-4.9\prebuilt\windows-x86_64\aarch64-linux-android\bin\;%PATH%
if %ARCH%=="ARM64" (
	@echo Config For ARM64 being run
	if not exist ./ARM64 mkdir ARM64 
	cd ./ARM64		
	bash -c '../ConfigForAndroid-arm64.sh'
	cd ../
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