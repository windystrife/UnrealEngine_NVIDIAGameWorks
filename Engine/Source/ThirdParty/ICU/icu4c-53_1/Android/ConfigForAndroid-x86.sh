SYSROOT=$NDKROOT/platforms/android-19/arch-x86
GCCTOOLCHAIN=$NDKROOT/toolchains/x86-4.9/prebuilt/windows-x86_64
GNUSTL=$NDKROOT/sources/cxx-stl/gnu-libstdc++/4.9
COMMONTOOLCHAINFLAGS="-target i686-none-linux-android --sysroot=$SYSROOT -gcc-toolchain $GCCTOOLCHAIN"
COMMONCOMPILERFLAGS="-fdiagnostics-format=msvc -fPIC -fno-exceptions -frtti -fno-short-enums -march=atom -fsigned-char"
COMMONINCLUDES="-I$GNUSTL/include -I$GNUSTL/libs/x86/include -I$SYSROOT/usr/include"
export CFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -MD -MF /dev/null -DANDROID -fdata-sections -ffunction-sections $COMMONINCLUDES -x c"
export CXXFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -MD -MF /dev/null $COMMONINCLUDES -x c++ -std=c++11"
export CPPFLAGS="$COMMONTOOLCHAINFLAGS $COMMONINCLUDES -MD -MF /dev/null -DANDROID -frtti -fno-exceptions -fdata-sections -ffunction-sections -DUCONFIG_NO_TRANSLITERATION=1 -DPIC -DU_HAVE_NL_LANGINFO_CODESET=0"
export LDFLAGS="$COMMONTOOLCHAINFLAGS -nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -lgnustl_shared -lc -lgcc -L$GNUSTL/libs/x86 -march=atom"
../../Source/configure --prefix=$PWD --build=x86_64-unknown-cygwin --with-cross-build=$PWD/../../Win64/VS2015 --host=i686-none-linux-android --enable-debug --disable-release --enable-static --disable-shared --with-data-packaging=files