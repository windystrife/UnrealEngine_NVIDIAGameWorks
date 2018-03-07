This is from The ELF Toolchain project: http://sourceforge.net/apps/trac/elftoolchain/

What was done to build this (read if you are upgrading from 0.6.1 - you will probably need to redo these manual steps):

- Extracted just libelf, libdwarf and common dirs from the tarball
- Did not use Makefiles for both, constructed .cmd files based on them instead.
- Fixed some includes (from <gelf.h> to "gelf.h" and similar - see fix-incl.sh)
- Run m4 manually on .m4 files (as described in makefiles) that needed that. These lines remain commented out in .cmd files, but I did not a standalone m4 exe to the toolchain.
- Run native-elf-format script on a target machine (64-bit CentOS 6.4) to get a target-specific native-elf-format.h file


If you want to just rebuild the .a files (e.g. using a newer clang version or a newer LINUX_ROOT toolchain altogether), the above is not necessary, just run cross-compile-without-make.cmd

-RCL
