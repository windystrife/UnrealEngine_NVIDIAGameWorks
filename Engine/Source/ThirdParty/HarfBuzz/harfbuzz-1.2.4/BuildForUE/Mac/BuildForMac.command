#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

CUR_PATH=$(dirname "$0")
PATH_TO_CMAKE_FILE="$CUR_PATH/../"
PATH_TO_CMAKE_FILE=`cd "$PATH_TO_CMAKE_FILE"; pwd`

# Temporary build directories (used as working directories when running CMake)
MAKE_PATH="$CUR_PATH/../../Mac/Build"
mkdir -p $MAKE_PATH
MAKE_PATH=`cd "$MAKE_PATH"; pwd`

rm -rf $MAKE_PATH
mkdir -p $MAKE_PATH
cd $MAKE_PATH
echo "Generating HarfBuzz makefile..."
cmake -G "Unix Makefiles" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 $PATH_TO_CMAKE_FILE
echo "Building HarfBuzz..."
make harfbuzz
cd $PATH_TO_CMAKE_FILE
rm -rf $MAKE_PATH
