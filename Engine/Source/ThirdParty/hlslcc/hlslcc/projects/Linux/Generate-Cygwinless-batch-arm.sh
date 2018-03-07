#!/bin/sh
# This file generates a Windows batch file that can be then used by builders for cross-compiling without relying on Cygwin

make clean
make -n > CrossCompile-arm.bat

sed -i s@clang\+\+@\%LINUX_ROOT\%\/bin\/clang\+\+.exe@g CrossCompile-arm.bat
sed -i s@-Wno-switch@-Wno-switch\ -target\ arm-unknown-linux-gnueabihf\ --sysroot=\%LINUX_ROOT\%@g CrossCompile-arm.bat
sed -i s@\\bar\\b@\%LINUX_ROOT\%\/bin\/arm-unknown-linux-gnueabihf-ar.exe@g CrossCompile-arm.bat
