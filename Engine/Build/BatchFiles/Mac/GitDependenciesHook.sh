#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

set -e

SCRIPT_PATH=$0
if [ -h $SCRIPT_PATH ]; then
	SCRIPT_PATH=$(dirname $SCRIPT_PATH)/$(readlink $SCRIPT_PATH)
fi
cd $(dirname $SCRIPT_PATH)

# Swallow the arguments passed in to this script; we don't need them when running a hook
./GitDependencies.sh
