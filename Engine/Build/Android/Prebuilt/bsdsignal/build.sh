#!/bin/bash
NDK_PROJECT_PATH=src
ndk-build APP_ABI="armeabi-v7a x86" APP_PLATFORM=android-21
cp src/obj/local/armeabi-v7a/libbsdsignal.a lib/armeabi-v7a
cp src/obj/local/x86/libbsdsignal.a lib/x86