LIBCXX_ABS_ROOT_CYG=$(pushd ../../../../Linux/LibCxx/ > /dev/null && pwd && popd > /dev/null)
LIBCXX_ABS_ROOT_UNESCAPED=`cygpath -w $LIBCXX_ABS_ROOT_CYG`
LIBCXX_ABS_ROOT=`echo $LIBCXX_ABS_ROOT_UNESCAPED | sed 's@\\\@\\\\\\\@g'`
echo "LIBCXX_ABS_ROOT=$LIBCXX_ABS_ROOT"

SYSROOT=`echo $LINUX_ROOT | sed 's@\\\@\\\\\\\@g'`
echo "SYSROOT=$SYSROOT"

COMMONTOOLCHAINFLAGS="-target x86_64-unknown-linux-gnu --sysroot=$SYSROOT"
COMMONCOMPILERFLAGS="-fdiagnostics-format=msvc -fPIC -fno-exceptions -frtti"
export CC=clang
export CXX=clang++
export CFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -x c"
export CXXFLAGS="$COMMONTOOLCHAINFLAGS $COMMONCOMPILERFLAGS -x c++ -std=c++11 -nostdinc++ -I$LIBCXX_ABS_ROOT/include -I$LIBCXX_ABS_ROOT/include/c++/v1"
export CPPFLAGS="$COMMONTOOLCHAINFLAGS -DUCONFIG_NO_TRANSLITERATION=1 -DPIC -DU_HAVE_NL_LANGINFO_CODESET=0 -DU_TIMEZONE=0 -nostdinc++ -I$LIBCXX_ABS_ROOT/include -I$LIBCXX_ABS_ROOT/include/c++/v1"
export LDFLAGS="$COMMONTOOLCHAINFLAGS -nodefaultlibs -L$LIBCXX_ABS_ROOT/lib/Linux/x86_64-unknown-linux-gnu/"
export LIBS="-lc++ -lc++abi -lc -lgcc -lgcc_s"
../../Source/configure --prefix=$PWD --build=x86_64-unknown-cygwin --with-cross-build=$PWD/../../Win64/VS2015 --host=x86_64-unknown-linux-gnu --enable-debug --disable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --with-data-packaging=files