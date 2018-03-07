@echo off
REM Batch file for building Android platforms of OpenSSL
REM Run this from the CURL directory

setlocal
set CYGWIN=winsymlinks:native
set ANDROID_NDK_ROOT=%NDKROOT%
set SSL_VER=openssl-1.0.1s
set CURL_VER=curl-7.46.0

set SAVEPATH=%PATH%

set ANDROID_NDK=android-ndk-r10e
set ANDROID_EABI=arm-linux-androideabi-4.8
set ANDROID_ARCH=arch-arm
set ANDROID_API=android-19
set ANDROID_GCC_VER=4.8
set PATH=%NDKROOT%/toolchains/llvm-3.6/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/arm-linux-androideabi-4.8/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/arm-linux-androideabi-4.8/prebuilt/windows-x86_64/arm-linux-androideabi/bin/;%SAVEPATH%
bash -c './BuildForAndroid.sh'

set ANDROID_NDK=android-ndk-r10e
set ANDROID_EABI=aarch64-linux-android-4.9
set ANDROID_ARCH=arch-arm64
set ANDROID_API=android-21
set ANDROID_GCC_VER=4.9
set PATH=%NDKROOT%/toolchains/llvm-3.6/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64/aarch64-linux-android/bin/;%SAVEPATH%
REM bash -c './BuildForAndroid.sh'

set ANDROID_NDK=android-ndk-r10e
set ANDROID_EABI=x86-4.8
set ANDROID_ARCH=arch-x86
set ANDROID_API=android-19
set ANDROID_GCC_VER=4.8
set PATH=%NDKROOT%/toolchains/llvm-3.6/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86-4.8/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86-4.8/prebuilt/windows-x86_64/i686-linux-android/bin/;%SAVEPATH%
bash -c './BuildForAndroid.sh'

set ANDROID_NDK=android-ndk-r10e
set ANDROID_EABI=x86_64-4.9
set ANDROID_ARCH=arch-x86_64
set ANDROID_API=android-21
set ANDROID_GCC_VER=4.8
set PATH=%NDKROOT%/toolchains/llvm-3.6/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86_64-4.9/prebuilt/windows-x86_64/bin/;%NDKROOT%/toolchains/x86_64-4.9/prebuilt/windows-x86_64/x86_64-linux-android/bin/;%SAVEPATH%
bash -c './BuildForAndroid.sh'

endlocal