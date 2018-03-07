#!/bin/bash

set -x
tar xzf zlib-1.2.5.tar.gz

cd zlib-1.2.5
CFLAGS=-fPIC ./configure
make
cp libz.a ../libz_fPIC.a

make distclean
./configure
make
cp libz.a ../

echo "Success!"

set +x
