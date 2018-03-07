@ECHO OFF

REM Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\

REM Temporary build directories (used as working directories when running CMake)
set ANDROID_BUILD_PATH="%PATH_TO_CMAKE_FILE%\..\Android\Build"

REM Build for Android (Debug) Using android-cmake
if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android ARMv7 (Debug)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="armeabi-v7a" -DCMAKE_BUILD_TYPE=Debug -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE -fPIC %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\ARMv7\Debug"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\ARMv7\Debug\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Debug" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android ARM64 (Debug)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="arm64-v8a" -DCMAKE_BUILD_TYPE=Debug -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\ARM64\Debug"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\ARM64\Debug\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Debug" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android x86 (Debug)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="x86" -DANDROID_NATIVE_API_LEVEL="android-19" -DCMAKE_BUILD_TYPE=Debug -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\x86\Debug"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\x86\Debug\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Debug" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android x64 (Debug)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="x86_64" -DCMAKE_BUILD_TYPE=Debug -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\x64\Debug"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\x64\Debug\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Debug" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


REM Build for Android (Release) Using android-cmake
if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android ARMv7 (Release)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="armeabi-v7a" -DCMAKE_BUILD_TYPE=Release -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE -fPIC %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\ARMv7\Release"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\ARMv7\Release\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Release" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android ARM64 (Release)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="arm64-v8a" -DCMAKE_BUILD_TYPE=Release -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\ARM64\Release"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\ARM64\Release\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Release" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android x86 (Release)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="x86" -DANDROID_NATIVE_API_LEVEL="android-19" -DCMAKE_BUILD_TYPE=Release -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\x86\Release"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\x86\Release\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Release" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q


if exist %ANDROID_BUILD_PATH% (rmdir %ANDROID_BUILD_PATH% /s/q)
mkdir %ANDROID_BUILD_PATH%
cd %ANDROID_BUILD_PATH%
echo Building HarfBuzz makefile for Android x64 (Release)...
cmake -DCMAKE_TOOLCHAIN_FILE=%PATH_TO_CMAKE_FILE%\Android\android.toolchain.cmake -G"MinGW Makefiles" -DANDROID_NDK=%NDK_ROOT% -DCMAKE_MAKE_PROGRAM="%NDK_ROOT%/prebuilt/windows-x86_64/bin/make.exe" -DANDROID_ABI="x86_64" -DCMAKE_BUILD_TYPE=Release -DUSE_INTEL_ATOMIC_PRIMITIVES=TRUE %PATH_TO_CMAKE_FILE%
cmake --build .
mkdir "%ANDROID_BUILD_PATH%\..\x64\Release"
move /y "%ANDROID_BUILD_PATH%\..\libharfbuzz.a" "%ANDROID_BUILD_PATH%\..\x64\Release\libharfbuzz.a"
cd %PATH_TO_CMAKE_FILE%
rmdir "%ANDROID_BUILD_PATH%\..\Release" /s/q
rmdir %ANDROID_BUILD_PATH% /s/q

endlocal
