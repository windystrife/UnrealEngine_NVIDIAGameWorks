@echo off
setlocal

rem set CMAKE_PATH="c:\Program Files (x86)\CMake\bin\cmake.exe"
set CMAKE_PATH="cmake.exe"

rmdir /s /q vc11win32
mkdir vc11win32
pushd vc11win32
%CMAKE_PATH% .. -G "Visual Studio 11" -AWin32 -DTARGET_BUILD_PLATFORM=Windows
popd

rmdir /s /q vc11win64
mkdir vc11win64
pushd vc11win64
%CMAKE_PATH% .. -G "Visual Studio 11" -Ax64 -DTARGET_BUILD_PLATFORM=Windows
popd

rmdir /s /q vc12win32
mkdir vc12win32
pushd vc12win32
%CMAKE_PATH% .. -G "Visual Studio 12" -AWin32 -DTARGET_BUILD_PLATFORM=Windows
popd

rmdir /s /q vc12win64
mkdir vc12win64
pushd vc12win64
%CMAKE_PATH% .. -G "Visual Studio 12" -Ax64 -DTARGET_BUILD_PLATFORM=Windows
popd

rmdir /s /q vc14win32
mkdir vc14win32
pushd vc14win32
%CMAKE_PATH% .. -G "Visual Studio 14" -AWin32 -DTARGET_BUILD_PLATFORM=Windows
popd

rmdir /s /q vc14win64
mkdir vc14win64
pushd vc14win64
%CMAKE_PATH% .. -G "Visual Studio 14" -Ax64 -DTARGET_BUILD_PLATFORM=Windows
popd
