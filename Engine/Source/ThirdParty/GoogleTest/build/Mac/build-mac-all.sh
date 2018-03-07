#!/bin/sh
# Prerequisites:
#  xcode
#  cmake 3.5

sh build-mac.sh MinSizeRel
sh build-mac.sh MinSizeRel Shared
