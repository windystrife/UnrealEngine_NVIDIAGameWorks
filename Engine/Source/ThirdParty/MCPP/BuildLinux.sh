#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

cd mcpp-2.7.2
CFLAGS="-O2 -fPIC -g0" CPPFLAGS="-fPIC -g0" ./configure --enable-mcpplib
make -j4
cp -f ./src/.libs/libmcpp.a ./lib/Linux/x86_64-unknown-linux-gnu

