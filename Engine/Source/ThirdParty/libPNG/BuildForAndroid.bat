@ECHO OFF

REM Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

setlocal

set LIBPNG_PATH=.\libPNG-1.5.27

REM copy to work directory to build libpng with ndk-build
xcopy /s /I /Q %LIBPNG_PATH% jni

REM build all 4 architectures
call ndk-build

REM create directories for libraries
mkdir %LIBPNG_PATH%\lib\Android\ARMv7
mkdir %LIBPNG_PATH%\lib\Android\ARM64
mkdir %LIBPNG_PATH%\lib\Android\x86
mkdir %LIBPNG_PATH%\lib\Android\x64

REM copy .a to destination
copy obj\local\armeabi-v7a\libpng.a %LIBPNG_PATH%\lib\Android\ARMv7
copy obj\local\arm64-v8a\libpng.a %LIBPNG_PATH%\lib\Android\ARM64
copy obj\local\x86\libpng.a %LIBPNG_PATH%\lib\Android\x86
copy obj\local\x86_64\libpng.a %LIBPNG_PATH%\lib\Android\x64

REM clean up the work directories
rmdir jni /s/q
rmdir obj /s/q

endlocal
