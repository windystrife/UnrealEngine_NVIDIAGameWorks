#!/bin/sh

set -x

dos2unix -p source/*.m4 source/*.in source/*.sub source/*.guess source/*.ac source/configure source/runConfigureICU source/mkinstalldirs source/install-sh

# Release
rm -rf Linux/Release
mkdir -p Linux/Release
cd Linux/Release
bash -c 'CXXFLAGS="-std=c++0x" CPPFLAGS="-DUCONFIG_NO_TRANSLITERATION=1" ../../source/runConfigureICU Linux --enable-static --enable-shared --with-library-bits=64 --with-data-packaging=files'
make clean
make all

cd ../..

# Debug
rm -rf Linux/Debug
mkdir -p Linux/Debug
cd Linux/Debug
bash -c 'CXXFLAGS="-std=c++0x" CPPFLAGS="-DUCONFIG_NO_TRANSLITERATION=1" ../../source/runConfigureICU --enable-debug --disable-release Linux --enable-static --enable-shared --with-library-bits=64 --with-data-packaging=files'
make clean
make all

set +x
