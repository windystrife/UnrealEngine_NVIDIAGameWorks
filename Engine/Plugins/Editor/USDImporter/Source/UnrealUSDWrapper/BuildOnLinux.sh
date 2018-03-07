#!/bin/bash

set -e

echo "Presuming that the header files in ../ThirdParty/Linux/ were already untarred."

rm -rf build
mkdir -p build
pushd build
#cmake -DCMAKE_BUILD_TYPE=Release ..
cmake -DCMAKE_BUILD_TYPE=Debug ..
make VERBOSE=1
#make
cp libUnrealUSDWrapper.so ../../../Binaries/Linux/x86_64-unknown-linux-gnu/
#cp libUnrealUSDWrapper.so ../Lib/Linux/x86_64-unknown-linux-gnu/
popd
