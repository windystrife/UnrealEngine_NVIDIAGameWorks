set(CMAKE_SYSTEM_NAME Linux)

#set(CMAKE_SYSROOT /media/Data/chromium/chromium_git/src/build/linux/centos7_amd64-sysroot/)

set(CMAKE_C_COMPILER /usr/bin/clang-3.8)
set(CMAKE_CXX_COMPILER /usr/bin/clang++-3.8)
set(CMAKE_LINKER  "/usr/bin/llvm-ld")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
