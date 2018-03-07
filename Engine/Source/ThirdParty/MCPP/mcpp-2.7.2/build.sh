#!/bin/sh
# RCL: always building with -fPIC since this is never used by non-monolithic programs.

CFLAGS=-fPIC ./configure --enable-mcpplib && make clean && make


