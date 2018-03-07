#!/bin/sh
clang -x c -arch x86_64 -O3  -mmacosx-version-min=10.9 -Iinc -c src/mikktspace.c -o mikktspace.o
export MACOSX_DEPLOYMENT_TARGET=10.9
mkdir -p lib/Mac
libtool -static -arch_only x86_64 mikktspace.o -o lib/Mac/libMikkTSpace.a
rm mikktspace.o
