#!/bin/bash
 
set -e

. Versions.inc

# Python
wget https://www.python.org/ftp/python/$PYTHON_VER/Python-$PYTHON_VER.tar.xz
# Boost
wget $BOOST_DOWNLOAD_URL
# TBB
wget https://github.com/01org/tbb/archive/$TBB_VER.tar.gz
#USD
#git clone https://github.com/PixarAnimationStudios/USD
#cd USD  
#git tag -l
#git checkout tags/$USD_VER  # or whatever is needed
git clone https://github.com/RCL/USD -b ver.0.7.5 USD
cd ..
 
