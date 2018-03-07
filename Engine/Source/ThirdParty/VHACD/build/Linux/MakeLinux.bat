SET LINUX_MULTIARCH_ROOT=%UE_SDKS_ROOT%\HostWin64\Linux_x64\v8_clang-3.9.0-centos7

SET LINUX_ARCH=arm-unknown-linux-gnueabihf
call CrossCompile.bat

SET LINUX_ARCH=x86_64-unknown-linux-gnu
call CrossCompile.bat