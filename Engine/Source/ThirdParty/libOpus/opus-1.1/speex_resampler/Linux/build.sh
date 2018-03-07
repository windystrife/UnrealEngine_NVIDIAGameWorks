#!/bin/bash

ARCHITECTURE=x86_64-unknown-linux-gnu
CFLAGS="-I../include -O2 -fvisibility=hidden"
SRC=resample.c

set -e
pushd ..
mkdir -p ../lib/Linux/$ARCHITECTURE

clang -c $CFLAGS $SRC -o resample.o
ar rcs libresampler.a resample.o
ranlib libresampler.a
cp libresampler.a ../lib/Linux/$ARCHITECTURE
rm *.o
rm *.a

clang -c $CFLAGS -fPIC $SRC -o resample_fPIC.o
ar rcs libresampler_fPIC.a resample_fPIC.o
ranlib libresampler_fPIC.a
cp libresampler_fPIC.a ../lib/Linux/$ARCHITECTURE
rm *.o
rm *.a

popd
set +e