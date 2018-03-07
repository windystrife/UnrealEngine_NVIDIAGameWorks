#!/bin/sh

pushd libPNG-1.5.2

p4 edit $THIRD_PARTY_CHANGELIST pnglibconf.h
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

# compile TVOS
pushd projects/IOS/png152
xcodebuild -sdk appletvos clean
xcodebuild -sdk appletvos
cp build/Release-appletvos/libpng152.a ../../../lib/TVOS/Device/libpng152.a
xcodebuild -sdk appletvsimulator clean
xcodebuild -sdk appletvsimulator
cp build/Release-appletvsimulator/libpng152.a ../../../lib/TVOS/Simulator/libpng152.a
popd

popd
