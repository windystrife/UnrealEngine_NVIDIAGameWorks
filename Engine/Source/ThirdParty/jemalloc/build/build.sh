#!/bin/sh

set +x

tar xf jemalloc-3.6.0.tar.bz2

cd jemalloc-3.6.0
sed -i s/initial-exec/local-dynamic/g configure
sed -i s/initial-exec/local-dynamic/g configure.ac
./configure --with-mangling --with-jemalloc-prefix=je_
make

set -x
