SYSROOT=$NDKROOT/platforms/android-21/arch-arm64
GCCTOOLCHAIN=$NDKROOT/toolchains/aarch64-linux-android-4.9/prebuilt/windows-x86_64
GNUSTL=$NDKROOT/sources/cxx-stl/gnu-libstdc++/4.9
COMMONTOOLCHAINFLAGS="-target aarch64-none-linux-android --sysroot=$SYSROOT -gcc-toolchain $GCCTOOLCHAIN"
COMMONCOMPILERFLAGS="-fdiagnostics-format=msvc -fPIC -fno-exceptions -frtti -fno-short-enums -march=armv8-a -fsigned-char"
COMMONINCLUDES="-I$GNUSTL/include -I$GNUSTL/libs/arm64-v8a/include -I$SYSROOT/usr/include"
export CFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -DANDROID -fdata-sections -ffunction-sections $COMMONINCLUDES -x c"
export CXXFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS $COMMONINCLUDES -x c++ -std=c++11"
export CPPFLAGS="$COMMONTOOLCHAINFLAGS $COMMONINCLUDES -DANDROID -frtti -fno-exceptions -fdata-sections -ffunction-sections -DUCONFIG_NO_TRANSLITERATION=1 -DPIC -DU_HAVE_NL_LANGINFO_CODESET=0"
export LDFLAGS="$COMMONTOOLCHAINFLAGS -nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -lgnustl_shared -lc -lgcc -L$GNUSTL/libs/arm64-v8a -march=armv8-a -Wl"

echo "Running Release Config"
../../Source/configure --prefix=$PWD --build=x86_64-unknown-cygwin --with-cross-build=$PWD/../../Win64/VS2015 --host=aarch64-none-linux-android --disable-debug --enable-release --enable-static --disable-shared --with-data-packaging=files
