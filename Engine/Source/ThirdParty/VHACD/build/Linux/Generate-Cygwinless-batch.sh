#!/bin/sh
# This file generates a Windows batch file that can be then used by builders for cross-compiling without relying on Cygwin

make clean
make -n > CrossCompile.bat

sed -i s@clang\+\+@\%LINUX_ROOT\%\/bin\/clang\+\+.exe@g CrossCompile.bat
sed -i s@-Wno-switch@-Wno-switch\ -target\ x86_64-unknown-linux-gnu\ --sysroot=\%LINUX_ROOT\%@g CrossCompile.bat
sed -i s@\\bar\\b@\%LINUX_ROOT\%\/bin\/x86_64-unknown-linux-gnu-ar.exe@g CrossCompile.bat
