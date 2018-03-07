#!/bin/bash

set -x
tar xf zlib-1.2.8.tar.gz

cd zlib-1.2.8
CFLAGS=-fPIC ./configure
make
cp libz.a ../libz_fPIC.a

make distclean
./configure
make
cp libz.a ../

echo "Success!"

set +x
