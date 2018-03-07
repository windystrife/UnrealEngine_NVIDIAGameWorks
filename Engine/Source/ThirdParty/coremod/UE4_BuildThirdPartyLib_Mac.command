#!/bin/sh

pushd coremode-4.2.6

p4 edit $THIRD_PARTY_CHANGELIST lib/Mac/...
p4 edit $THIRD_PARTY_CHANGELIST lib/IOS/...

# compile Mac
cp scripts/makefile.darwin makefile
xcrun make clean
xcrun make ZLIBINC=../../zlib/zlib-1.2.5 libpng.a
rm -f makefile
mv libpng.a lib/Mac/libpng.a

# compile IOS
pushd projects/IOS/png152
xcodebuild -sdk iphoneos clean
xcodebuild -sdk iphoneos
cp build/Release-iphoneos/libpng152.a ../../../lib/IOS/Device/libpng152.a
xcodebuild -sdk iphonesimulator clean
xcodebuild -sdk iphonesimulator
cp build/Release-iphonesimulator/libpng152.a ../../../lib/IOS/Simulator/libpng152.a
popd

popd
