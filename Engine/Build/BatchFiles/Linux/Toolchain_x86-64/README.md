How to build the cross-toolchain.
=================================

Big picture view, steps are described in detail later:

1.1 Build gcc-based cross-toolchain (we only need binutils/sysroot from it) on a Linux machine 
     and copy them to a your Windows one.
1.2 Build/download clang on a Windows machine and add it to above toolchain.

How to use the cross-toolchain.
=================================

2.1 Set LINUX_MULTIARCH_ROOT environment variable to point to asbolute (Windows) path of the toolchain.
2.2 Restart Visual Studio and regenerate projects with UBT.


Building gcc-based cross-toolchain
==================================

Copy BuildCrossToolchain.sh together with all .config files to a Linux machine running Ubuntu 16.04. 
Run the script, which will clone a fork of crosstool-ng and will use it to perform
so called "canadian cross", i.e. will build Linux-hosted toolchain which targets
mingw64 first, and then will build Windows-hosted toolchain that targets Linux.

Note that the choice of kernel and binutils is pretty conservative and you are free
to experiment (using "ct-ng menuconfig") if you need more recent ones. Not every
combination is buildable though.

If the script finishes without error, the new toolchain will be copied to subfolder
called CrossToolchainMultiarch of the current folder. Copy it to your Windows machine,
e.g. to C:\CrossToolchainMultiarch

Building clang
==============

Note that you can grab a pre-built clang from here: http://llvm.org/releases/download.html
Detailed instructions how to build clang are given here: http://clang.llvm.org/get_started.html

To recap, you will need: cmake, python and a supported compiler (recent Visual Studio is Ok).
Grab stable release of llvm and clang from here: http://llvm.org/releases/download.html
and unpack it to your local folder (unpack llvm first, then unpack clang into llvm/tools subdirectory).

Use cmake to generate project files and then build Release (or MinSizeRel) x64 configuration of "install" 
(or INSTALL, if you are using Visual Studio) target. Here's an example how to do the above from comamnd line:

cmake -G "Visual Studio 11 Win64" -DPYTHON_EXECUTABLE=full_path_to_pythonw.exe -DCMAKE_INSTALL_PREFIX=path_to_copy_results_to ..\llvm
devenv.exe LLVM.sln /Build Release /Project INSTALL

Having done that, copy from install directory the following

- bin/clang.exe
- bin/clang++.exe
- lib/clang/x.y.0/include 

into all the architecture-specific toolchains (so their relative path from toolchain root stays the same).


Using toolchain to build UE4 Linux targets
==========================================

Set environment variable (Control Panel->System->Advanced system settings->Advanced->Environment variables)
LINUX_MULTIARCH_ROOT to absolute path to your toolchain, without trailing backslash, so it looks like this:

LINUX_MULTIARCH_ROOT=C:\CrossToolchainMultiarch

After you made sure that variable is set, you can restart Visual Studio and regenerate UE4 project files
(using GenerateProjectFiles.bat). After this, you should have "Linux" available among Win32/Win64
configurations, and you should be able to cross-compile for it. Note that for the moment being,
only Server (Debug Server, Development Server, etc) targets are buildable for Linux.
