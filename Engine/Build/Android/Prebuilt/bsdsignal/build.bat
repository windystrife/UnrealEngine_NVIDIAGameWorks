set NDK_PROJECT_PATH=src
call ndk-build APP_ABI="armeabi-v7a x86" APP_PLATFORM=android-21
copy src\obj\local\armeabi-v7a\libbsdsignal.a lib\armeabi-v7a
copy src\obj\local\x86\libbsdsignal.a lib\x86