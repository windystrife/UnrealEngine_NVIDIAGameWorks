SYSROOT=$NDKROOT/platforms/android-19/arch-arm
GCCTOOLCHAIN=$NDKROOT/toolchains/arm-linux-androideabi-4.9/prebuilt/windows-x86_64
GNUSTL=$NDKROOT/sources/cxx-stl/gnu-libstdc++/4.9
COMMONTOOLCHAINFLAGS="-target armv7a-none-linux-androideabi --sysroot=$SYSROOT -gcc-toolchain $GCCTOOLCHAIN"
COMMONCOMPILERFLAGS="-fdiagnostics-format=msvc -fPIC -fno-exceptions -frtti -fno-short-enums -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -fsigned-char"
COMMONINCLUDES="-I$GNUSTL/include -I$GNUSTL/libs/armeabi-v7a/include -I$SYSROOT/usr/include"
export CFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -DANDROID -fdata-sections -ffunction-sections $COMMONINCLUDES -x c"
export CXXFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS $COMMONINCLUDES -x c++ -std=c++11"
export CPPFLAGS="$COMMONTOOLCHAINFLAGS $COMMONINCLUDES -DANDROID -frtti -fno-exceptions -fdata-sections -ffunction-sections -DUCONFIG_NO_TRANSLITERATION=1 -DPIC -DU_HAVE_NL_LANGINFO_CODESET=0"
export LDFLAGS="$COMMONTOOLCHAINFLAGS -nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -lgnustl_shared -lc -lgcc -L$GNUSTL/libs/armeabi-v7a -march=armv7-a -Wl,--fix-cortex-a8"
if [ $1 -eq 1 ]
then
	echo "Running Debug Config"
	../../Source/configure --prefix=$PWD --build=x86_64-unknown-cygwin --with-cross-build=$PWD/../../Win64/VS2015 --host=armv7a-none-linux-androideabi --enable-debug --disable-release --enable-static --disable-shared --with-data-packaging=files
else
	echo "Running Release Config"
	../../Source/configure --prefix=$PWD --build=x86_64-unknown-cygwin --with-cross-build=$PWD/../../Win64/VS2015 --host=armv7a-none-linux-androideabi --disable-debug --enable-release --enable-static --disable-shared --with-data-packaging=files
fi
