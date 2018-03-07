@echo off

rem CMake
set CMAKE_PATH=
pushd ..\..\..\..\..\Extras\ThirdPartyNotUE\CMake\bin
set CMAKE_PATH=%CD%
popd

rem ## Change relative path into absolute path. We need that for cmake.
set REL_PATH=..\..\..\..\..\Extras\ThirdPartyNotUE\GNU_Make\make-3.81\bin
set MAKE_PATH=
pushd %REL_PATH%
set MAKE_PATH=%CD%
popd

setlocal

rem ## Give a nice message.
echo Cross Compiling glslang ...

rem ## Set the architecture.
set ARCHITECTURE_TRIPLE="x86_64-unknown-linux-gnu"
rem set ARCHITECTURE_TRIPLE="i686-unknown-linux-gnu"
rem set ARCHITECTURE_TRIPLE="arm-unknown-linux-gnueabihf"
rem set ARCHITECTURE_TRIPLE="aarch64-unknown-linux-gnueabi"

rem LibCxx
set LIBCXX_INCLUDE1=
pushd ..\..\..\..\..\Source\ThirdParty\Linux\LibCxx\include\c++\v1\
set LIBCXX_INCLUDE1=%CD%
popd

set LIBCXX_INCLUDE2=
pushd ..\..\..\..\..\Source\ThirdParty\Linux\LibCxx\include\
set LIBCXX_INCLUDE2=%CD%
popd

set LIBCXX_LIB=
pushd ..\..\..\..\..\Source\ThirdParty\Linux\LibCxx\lib\Linux\%ARCHITECTURE_TRIPLE%
set LIBCXX_LIB=%CD%
popd

rem ## Compile and Build.
rmdir /S /Q cmake-temp
mkdir cmake-temp
cd cmake-temp
%CMAKE_PATH%\cmake -DCMAKE_MAKE_PROGRAM=%MAKE_PATH%\make.exe -DCMAKE_TOOLCHAIN_FILE="../LinuxCrossToolchain.multiarch.cmake" -DARCHITECTURE_TRIPLE=%ARCHITECTURE_TRIPLE% -DUSE_CUSTOM_LIBCXX=ON -DLIBCXX_INCLUDE=%LIBCXX_INCLUDE1% -DLIBCXX_ABI_INCLUDE=%LIBCXX_INCLUDE2% -DLIBCXX_LIB_DIR=%LIBCXX_LIB% -G"Unix Makefiles" ../../../../glslang/glslang/src/glslang_lib

%MAKE_PATH%\make.exe

rem ## Copy to destination.
set DESTINATION_DIRECTORY="../../../../glslang/glslang/lib/Linux"

if not exist %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE% mkdir %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%

copy /y glslang\libglslang.a %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%\libglslang.a
copy /y glslang\OSDependent\Unix\libOSDependent.a %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%\libOSDependent.a
copy /y hlsl\libHLSL.a %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%\libHLSL.a
copy /y OGLCompilersDLL\libOGLCompiler.a %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%\libOGLCompiler.a
copy /y SPIRV\libSPIRV.a %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%\libSPIRV.a
copy /y SPIRV\libSPVRemapper.a %DESTINATION_DIRECTORY%\%ARCHITECTURE_TRIPLE%\libSPVRemapper.a