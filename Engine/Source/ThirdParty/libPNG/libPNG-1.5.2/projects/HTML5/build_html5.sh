#!/bin/bash

PNG_HTML5=$(pwd)

cd ../../../../HTML5/
	. ./Build_All_HTML5_libs.rc
cd ..
	ZLIB_DIR="$(pwd)/zlib/zlib-1.2.5"
cd "$PNG_HTML5"


build_via_cmake()
{
	SUFFIX=_O$OLEVEL
	OPTIMIZATION=-O$OLEVEL
	# ----------------------------------------
	rm -rf BUILD$SUFFIX
	mkdir BUILD$SUFFIX
	cd BUILD$SUFFIX
	# ----------------------------------------
#	TYPE=${type^^} # OSX-bash doesn't like this
	TYPE=`echo $type | tr "[:lower:]" "[:upper:]"`
	if [ $TYPE == "DEBUG" ]; then
		DBGFLAG=_DEBUG
		d=d
	else
		DBGFLAG=NDEBUG
		d=
	fi
	# ----------------------------------------
	emcmake cmake -G "Unix Makefiles" \
		-DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake \
		-DM_LIBRARY="" \
		-DZLIB_INCLUDE_DIR="$ZLIB_DIR/Src" \
		-DZLIB_LIBRARY="$ZLIB_DIR/Lib/HTML5" \
		-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=ON \
		-DCMAKE_BUILD_TYPE=$type \
		-DCMAKE_C_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG -I\"$ZLIB_DIR/Src\" -I\"$ZLIB_DIR/Src/HTML5/BUILD$SUFFIX\" " \
		../../..
	cmake --build . -- png15_static VERBOSE=1
	# ----------------------------------------
	if [ $OLEVEL == 0 ]; then
		SUFFIX=
	fi
	cp libpng15${d}.bc ../../../lib/HTML5/libpng${SUFFIX}.bc
	cd ..
}
type=Debug;       OLEVEL=0;  build_via_cmake
type=Release;     OLEVEL=2;  build_via_cmake
type=Release;     OLEVEL=3;  build_via_cmake
type=MinSizeRel;  OLEVEL=z;  build_via_cmake
ls -l ../../lib/HTML5


# NOT USED: LEFT HERE FOR REFERENCE
build_via_makefile()
{
	cd ../..
	
	proj=libpng
	makefile=projects/HTML5/Makefile.HTML5
	EMFLAGS="-msse -msse2 -s FULL_ES2=1 -s USE_PTHREADS=1"
	
	make clean   OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O3.bc -f ${makefile}
	make install OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O3.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O2.bc -f ${makefile}
	make install OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O2.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_Oz.bc -f ${makefile}
	make install OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_Oz.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}.bc -f ${makefile}
	make install OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}.bc -f ${makefile}
	
	ls -l lib/HTML5
}

