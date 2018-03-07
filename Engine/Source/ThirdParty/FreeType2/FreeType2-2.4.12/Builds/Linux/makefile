CC=gcc
AR_RC=ar rc
RANLIB=ranlib
RM_F=rm -f
CPP = g++

CFLAGS= -g -O2 -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -fno-exceptions -fno-short-enums -fPIC #-std=c++11
CPPFLAGS= -I../include/ -DFT2_BUILD_LIBRARY -DNDEBUG

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


all: libfreetype2412.a

libfreetype2412.a: $(OBJS)
	$(AR_RC) $@  $(OBJS)
	$(RANLIB) $@

clean:
	$(RM_F) *.o */*.o libfreetype2412.a
