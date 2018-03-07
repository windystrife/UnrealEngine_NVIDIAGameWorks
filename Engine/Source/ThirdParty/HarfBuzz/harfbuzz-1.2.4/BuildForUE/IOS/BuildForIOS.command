#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

CUR_PATH=$(dirname "$0")
PATH_TO_CMAKE_FILE="$CUR_PATH/.."
PATH_TO_CMAKE_FILE=`cd "$PATH_TO_CMAKE_FILE"; pwd`
echo PATH_TO_CMAKE_FILE=$PATH_TO_CMAKE_FILE

# Temporary build directories (used as working directories when running CMake)
MAKE_PATH="$CUR_PATH/../../IOS/Build"
mkdir -p $MAKE_PATH
MAKE_PATH=`cd "$MAKE_PATH"; pwd`
echo MAKE_PATH=$MAKE_PATH

PATH_IOS_TOOLCHAIN=$CUR_PATH
PATH_IOS_TOOLCHAIN=`cd "$PATH_IOS_TOOLCHAIN"; pwd`
echo PATH_IOS_TOOLCHAIN=$PATH_IOS_TOOLCHAIN

rm -rf $MAKE_PATH
mkdir -p $MAKE_PATH
cd $MAKE_PATH
echo "Generating HarfBuzz makefile..."
cmake -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=$PATH_IOS_TOOLCHAIN/iOS.cmake -DCMAKE_IOS_DEPLOYMENT_TARGET=7.0 $PATH_TO_CMAKE_FILE
echo "Building HarfBuzz..."
make harfbuzz
cd $PATH_TO_CMAKE_FILE
#rm -rf $MAKE_PATH
