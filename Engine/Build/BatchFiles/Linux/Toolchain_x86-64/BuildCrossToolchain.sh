#!/bin/sh
# by rcl^epic

SelfName=`basename $0`
LinuxBuildTemp=linux-host-build
Win64BuildTemp_x86_64=win64-host-build-x86_64
Win64BuildTemp_ARM32=win64-host-build-ARM32
Win64BuildTemp_i686=win64-host-build-i686
OutputFolder=CrossToolchainMultiarch

# prereq:
# mercurial autoconf gperf bison flex libtool ncurses-dev


#############
# build recent ct-ng

rm -rf crosstool-ng
git clone http://github.com/RCL/crosstool-ng -b 1.22
cd crosstool-ng

# install Ubuntu (16.04) packages
UbuntuPackages="autoconf gperf bison flex help2man libtool-bin libncurses5-dev"
echo Installing packages $UbuntuPackages
sudo apt install -y $UbuntuPackages

./bootstrap
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

./configure --enable-local
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

make
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd ..

#############
# build Linux-hosted toolchain that targets mingw64

rm -rf $LinuxBuildTemp
mkdir $LinuxBuildTemp

cp linux-host.config $LinuxBuildTemp/.config 
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd $LinuxBuildTemp
../crosstool-ng/ct-ng build
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd ..


#############
# build Windows-hosted toolchain that targets Linux x86_64

rm -rf $Win64BuildTemp_x86_64
mkdir $Win64BuildTemp_x86_64

cp win64-host_x86_64.config $Win64BuildTemp_x86_64/.config
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd $Win64BuildTemp_x86_64

../crosstool-ng/ct-ng build
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd ..

#############
# build Windows-hosted toolchain that targets Linux ARM32

rm -rf $Win64BuildTemp_ARM32
mkdir $Win64BuildTemp_ARM32

cp win64-host_ARM32.config $Win64BuildTemp_ARM32/.config
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd $Win64BuildTemp_ARM32

../crosstool-ng/ct-ng build
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd ..

#############
# build Windows-hosted toolchain that targets Linux i686

rm -rf $Win64BuildTemp_i686
mkdir $Win64BuildTemp_i686

cp win64-host_i686.config $Win64BuildTemp_i686/.config
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd $Win64BuildTemp_i686

../crosstool-ng/ct-ng build
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

cd ..

if [ -d $OutputFolder ]; then
	chmod -R 777 $OutputFolder
	rm -rf $OutputFolder
fi

mkdir $OutputFolder
cp -r ~/x-tools/x86_64-unknown-linux-gnu $OutputFolder
cp -r ~/x-tools/arm-unknown-linux-gnueabihf $OutputFolder
cp -r ~/x-tools/i686-unknown-linux-gnu $OutputFolder
[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

#############
# rearrange files the way we need them

cd $OutputFolder/x86_64-unknown-linux-gnu
find . -type d -exec chmod +w {} \;
cp -r -L x86_64-unknown-linux-gnu/include .
cp -r -L x86_64-unknown-linux-gnu/sysroot/lib64 .
mkdir -p usr
cp -r -L x86_64-unknown-linux-gnu/sysroot/usr/lib64 usr
cp -r -L x86_64-unknown-linux-gnu/sysroot/usr/include usr
rm -rf x86_64-unknown-linux-gnu

cd ../..

cd $OutputFolder/arm-unknown-linux-gnueabihf
find . -type d -exec chmod +w {} \;
cp -r -L arm-unknown-linux-gnueabihf/include .
cp -r -L arm-unknown-linux-gnueabihf/sysroot/lib .
mkdir -p usr
cp -r -L arm-unknown-linux-gnueabihf/sysroot/usr/lib usr
cp -r -L arm-unknown-linux-gnueabihf/sysroot/usr/include usr
rm -rf arm-unknown-linux-gnueabihf

cd ../..

cd $OutputFolder/i686-unknown-linux-gnu
find . -type d -exec chmod +w {} \;
cp -r -L i686-unknown-linux-gnu/include .
cp -r -L i686-unknown-linux-gnu/sysroot/lib .
mkdir -p usr
cp -r -L i686-unknown-linux-gnu/sysroot/usr/lib usr
cp -r -L i686-unknown-linux-gnu/sysroot/usr/include usr
rm -rf i686-unknown-linux-gnu

cd ../..
