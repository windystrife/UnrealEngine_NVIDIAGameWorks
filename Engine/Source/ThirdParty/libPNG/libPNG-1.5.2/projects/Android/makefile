TOOLCHAIN=/cygdrive/c/android/toolchainarm/bin
CC=$(TOOLCHAIN)/clang.cmd
AR_RC=$(TOOLCHAIN)/arm-linux-androideabi-ar.exe rc
RANLIB=$(TOOLCHAIN)/arm-linux-androideabi-ranlib.exe
RM_F=rm -f
CPP = $(TOOLCHAIN)/clang++.cmd

DFNFLAGS = # DFNFLAGS contains -D options to use in the libpng build
CFLAGS= -g -O3 -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -fno-exceptions -fno-rtti -fno-short-enums -fpic -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 #-std=c++11

OBJS = png.o pngset.o pngget.o pngrutil.o pngtrans.o pngwutil.o \
	pngread.o pngrio.o pngwio.o pngwrite.o pngrtran.o \
	pngwtran.o pngmem.o pngerror.o pngpread.o

all: libpng_arm.a

libpng_arm.a: X86 X64 $(OBJS)
	$(AR_RC) $@  $(OBJS)
	$(RANLIB) $@
	cp libpng_arm.a ./lib/Android/ARMv7/libpng.a

clean:
	$(RM_F) *.o libpng_arm.a

X86:
	$(MAKE) -f makefile.x86

X64:
	$(MAKE) -f makefile.x64
