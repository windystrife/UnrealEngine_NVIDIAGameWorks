TOOLCHAIN=/cygdrive/c/android/toolchainarm/bin
CC=$(TOOLCHAIN)/clang.cmd
AR_RC=$(TOOLCHAIN)/arm-linux-androideabi-ar.exe rc
RANLIB=$(TOOLCHAIN)/arm-linux-androideabi-ranlib.exe

RM_F=rm -f
CPP = $(TOOLCHAIN)/clang++.cmd

CFLAGS= -g -O3 -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -fno-exceptions -fno-rtti -fno-short-enums -fpic -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 #-std=c++11
CPPFLAGS= -I../Include -DFT2_BUILD_LIBRARY -DNDEBUG

OBJS = winfonts/winfnt.o \
	type42/type42.o \
	cid/type1cid.o \
	type1/type1.o \
	truetype/truetype.o \
	sfnt/sfnt.o \
	raster/raster.o \
	psnames/psmodule.o \
	pshinter/pshinter.o \
	psaux/psaux.o \
	pfr/pfr.o \
	pcf/pcf.o \
	base/ftmm.o \
	base/ftotval.o \
	base/ftpatent.o \
	base/ftpfr.o \
	base/ftsynth.o \
	base/fttype1.o \
	base/ftwinfnt.o \
	base/ftxf86.o \
	base/ftlcdfil.o \
	base/ftbbox.o \
	base/ftgxval.o \
	smooth/smooth.o \
	base/ftsystem.o \
	base/ftstroke.o \
	lzw/ftlzw.o \
	base/ftinit.o \
	gzip/ftgzip.o \
	base/ftglyph.o \
	base/ftgasp.o \
	base/ftfstype.o \
	base/ftdebug.o \
	cache/ftcache.o \
	base/ftbitmap.o \
	base/ftbase.o \
	cff/cff.o \
	bdf/bdf.o \
	autofit/autofit.o


all: X86 X64 libfreetype2412_arm.a

libfreetype2412_arm.a: $(OBJS)
	$(AR_RC) $@  $(OBJS)
	$(RANLIB) $@
	cp libfreetype2412_arm.a ../Lib/Android/ARMv7/libfreetype2412.a

clean:
	$(RM_F) *.o */*.o libfreetype2412_arm.a

X86:
	$(MAKE) -f makefile.x86

X64:
	$(MAKE) -f makefile.x64
