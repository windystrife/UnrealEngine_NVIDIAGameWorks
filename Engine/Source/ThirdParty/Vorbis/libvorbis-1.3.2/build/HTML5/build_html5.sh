#!/bin/bash

VORBIS_HTML5=$(pwd)

cd ../../../../HTML5/
	. ./Build_All_HTML5_libs.rc
cd ..
	OGG_DIR="$(pwd)/Ogg/libogg-1.2.2"
	VORB_FLAGS="-I\"$OGG_DIR/include\" -Wno-comment -Wno-shift-op-parentheses"
cd "$VORBIS_HTML5"


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
	else
		DBGFLAG=NDEBUG
	fi
	# ----------------------------------------
	emcmake cmake -G "Unix Makefiles" \
		-DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake \
		-DBUILD_SHARED_LIBS=OFF \
		-DOGG_INCLUDE_DIRS=$OGG_DIR/include \
		-DOGG_LIBRARIES=$OGG_DIR/lib/HTML5 \
		-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=ON \
		-DCMAKE_BUILD_TYPE=$type \
		-DCMAKE_C_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG $VORB_FLAGS" \
		-DCMAKE_CXX_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG $VORB_FLAGS" \
		../../..
	cmake --build . -- -j VERBOSE=1
	# ----------------------------------------
	if [ $OLEVEL == 0 ]; then
		SUFFIX=
	fi
	cp lib/libvorbis.bc ../../../lib/HTML5/libvorbis${SUFFIX}.bc
#	cp lib/libvorbisenc.bc ../../../lib/HTML5/libvorbisenc${SUFFIX}.bc
	cp lib/libvorbisfile.bc ../../../lib/HTML5/libvorbisfile${SUFFIX}.bc
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
	cd ../../lib
	
	proj1=libvorbis
	proj2=libvorbisfile
	makefile=../build/HTML5/Makefile.HTML5
	EMFLAGS="-msse -msse2 -s FULL_ES2=1 -s USE_PTHREADS=1"
	
	make clean   OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O3.bc LIB2=${proj2}_O3.bc -f ${makefile}
	make install OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O3.bc LIB2=${proj2}_O3.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O2.bc LIB2=${proj2}_O2.bc -f ${makefile}
	make install OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O2.bc LIB2=${proj2}_O2.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_Oz.bc LIB2=${proj2}_Oz.bc -f ${makefile}
	make install OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_Oz.bc LIB2=${proj2}_Oz.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}.bc LIB2=${proj2}.bc -f ${makefile}
	make install OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}.bc LIB2=${proj2}.bc -f ${makefile}
	
	ls -l ../lib/HTML5
}

