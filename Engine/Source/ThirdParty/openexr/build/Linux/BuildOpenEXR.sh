#!/bin/bash

set -e

export CXX=clang++
TEMP_DIR="/tmp/local-openexr-$BASHPID"
UE_THIRD_PARTY_DIR=`cd "../../../"; pwd`
echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"

tar xf ilmbase-1.0.3.tar.gz
cd ilmbase-1.0.3
CXXFLAGS="-O2 -fPIC -nostdinc++ -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1" LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/ $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc"  ./configure --prefix=$TEMP_DIR
make install
cd ..

tar xf openexr-1.7.1.tar.gz
cd openexr-1.7.1
CXXFLAGS="-O2 -fPIC -nostdinc++ -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1" LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/ $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc" LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$TEMP_DIR/lib" ./configure --with-ilmbase-prefix=$TEMP_DIR --prefix=$TEMP_DIR
make install
cd ..

BUILT_LIBS_DIR=$TEMP_DIR/lib

cp $BUILT_LIBS_DIR/libHalf.a .
cp $BUILT_LIBS_DIR/libIex.a .
cp $BUILT_LIBS_DIR/libIexMath.a .
cp $BUILT_LIBS_DIR/libIlmImf.a .
cp $BUILT_LIBS_DIR/libIlmThread.a .
cp $BUILT_LIBS_DIR/libImath.a .

# FIXME: change arch if ever building for something else
DEST_PATH=../../Deploy/lib/Linux/StaticRelease/x86_64-unknown-linux-gnu/

mkdir -p $DEST_PATH
cp *.a $DEST_PATH
