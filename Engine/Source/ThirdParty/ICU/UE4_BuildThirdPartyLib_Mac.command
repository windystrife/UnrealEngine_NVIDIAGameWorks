#!/bin/sh

echo "Coffee time..."

set -x

# Don't depend on a current working directory being set to a particular value, everything is relative to this script
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

pushd $SCRIPTDIR/icu4c-53_1

p4 edit $THIRD_PARTY_CHANGELIST Mac/...
p4 edit $THIRD_PARTY_CHANGELIST IOS/...
p4 edit $THIRD_PARTY_CHANGELIST TVOS/...

# These are the temporary build directories that do not get checked into source control
MACOSBUILDIR=BuildTmp/MacOS
TVOSBBUILDDIR=BuildTmp/TVOS
IOSBUILDDIR=BuildTmp/IOS

# Hold onto our main ICU directory
ICUDIR=$(pwd)

#
# BUILD MACOS LIBRARIES
#
# The mac tools must be built first to bootstrap the build process (i.e. ICU builds tools that run on the building machine to generate data files)

mkdir -p $MACOSBUILDIR
pushd $MACOSBUILDIR

# CROSSBUILD is used for the tvos and ios cross compile builds
CROSSBUILD=$(pwd)

# Build debug libraries
rm -rf *
$ICUDIR/source/configure --build=x86_64-apple-darwin11 --enable-debug --disable-release --enable-static --disable-shared --disable-samples --disable-extras --with-data-packaging=files --with-library-suffix=d
gnumake
cp -f ./lib/* $ICUDIR/Mac/lib/
cp -f ./stubdata/libicudatad.a $ICUDIR/Mac/lib/

# Build release libraries
rm -rf *
$ICUDIR/source/configure --build=x86_64-apple-darwin11 --disable-debug --enable-release --enable-static --disable-shared --disable-samples --disable-extras --with-data-packaging=files
gnumake
cp -f ./lib/* $ICUDIR/Mac/lib/
cp -f ./stubdata/libicudata.a $ICUDIR/Mac/lib/
cp -f ./bin/* $ICUDIR/Mac/bin/

popd

#
# BUILD TVOS LIBRARIES
#

mkdir -p $TVOSBBUILDDIR
pushd $TVOSBBUILDDIR

# Build debug libraries
rm -rf *
$ICUDIR/source/configure --build=x86_64-apple-darwin11 --host=tvos --with-cross-build=$CROSSBUILD --enable-debug --disable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --with-data-packaging=files --with-library-suffix=d
gnumake
cp -f ./lib/* $ICUDIR/TVOS/lib/
cp -f ./stubdata/libicudatad.a $ICUDIR/TVOS/lib/

# Build release libraries
rm -rf *
$ICUDIR/source/configure --build=x86_64-apple-darwin11 --host=tvos --with-cross-build=$CROSSBUILD --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --with-data-packaging=files
gnumake
cp -f ./lib/* $ICUDIR/TVOS/lib/
cp -f ./stubdata/libicudata.a $ICUDIR/TVOS/lib/

popd

#
# BUILD IOS LIBRARIES
#

mkdir -p $IOSBUILDDIR
pushd $IOSBUILDDIR

# Build debug libraries
rm -rf *
$ICUDIR/source/configure --build=x86_64-apple-darwin11 --host=ios --with-cross-build=$CROSSBUILD --enable-debug --disable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --with-data-packaging=files --with-library-suffix=d
gnumake
cp -f ./lib/* $ICUDIR/IOS/lib/
cp -f ./stubdata/libicudatad.a $ICUDIR/IOS/lib/

# Build release libraries
rm -rf *
$ICUDIR/source/configure --build=x86_64-apple-darwin11 --host=ios --with-cross-build=$CROSSBUILD --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --with-data-packaging=files
gnumake
cp -f ./lib/* $ICUDIR/IOS/lib/
cp -f ./stubdata/libicudata.a $ICUDIR/IOS/lib/

popd

# return to the original directory
popd
