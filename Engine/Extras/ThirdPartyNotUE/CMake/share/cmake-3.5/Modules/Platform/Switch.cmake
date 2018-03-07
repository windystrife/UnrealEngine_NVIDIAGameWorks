STRING(REGEX REPLACE "\\\\" "/" SWITCH_SDK_DIR $ENV{NINTENDO_SDK_ROOT})
SET(CMAKE_AR ${SWITCH_SDK_DIR}/Compilers/NX/nx/aarch64/bin/aarch64-nintendo-nx-elf-ar.exe)
