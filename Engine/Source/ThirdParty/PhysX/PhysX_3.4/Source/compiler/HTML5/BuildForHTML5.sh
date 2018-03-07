#!/bin/bash
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


# WARNING: connot build libs if absolute paths contains any space -- will revisit this in the future...
# WARNING: on OSX - max process per user may need to be bumped up...
#          see: https://gist.github.com/jamesstout/4546975#file-find_zombies-sh-L26-L46


#set -x


# ----------------------------------------
# env

cd ../../../..
export GW_DEPS_ROOT=$(pwd)

cd ../HTML5/
	# source in emscripten SDK paths and env vars
	. ./Build_All_HTML5_libs.rc
cd "$GW_DEPS_ROOT"


# ----------------------------------------
# MAKE

export CMAKE_MODULE_PATH="$GW_DEPS_ROOT/Externals/CMakeModules"

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
	if [ $OLEVEL == 0 ]; then
		SUFFIX=
	fi
	export LIB_SUFFIX=$SUFFIX
	emcmake cmake -G "Unix Makefiles" \
		-DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake \
		-DAPEX_ENABLE_UE4=1 \
		-DTARGET_BUILD_PLATFORM=HTML5 \
		-DPHYSX_ROOT_DIR="$GW_DEPS_ROOT"/PhysX_3.4 \
		-DPXSHARED_ROOT_DIR="$GW_DEPS_ROOT"/PxShared \
		-DNVSIMD_INCLUDE_DIR="$GW_DEPS_ROOT"/PxShared/src/NvSimd \
		-DNVTOOLSEXT_INCLUDE_DIRS="$GW_DEPS_ROOT"/PhysX_3.4/externals/nvToolsExt/include \
		-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=ON \
		-DCMAKE_BUILD_TYPE=$type \
		-DCMAKE_C_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG" \
		-DCMAKE_CXX_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG" \
		"$GW_DEPS_ROOT"/PhysX_3.4/Source/compiler/cmake/html5

# disabling APEX build for HTML5
#		"$GW_DEPS_ROOT"/APEX_1.4/compiler/cmake/HTML5

	cmake --build . -- -j VERBOSE=1
	# ----------------------------------------
	if [ $OLEVEL == "z" ]; then
		# for some reason: _Oz is not getting done here...
		find . -type f -name "*.bc" -print | while read i; do b=`basename $i .bc`; cp $i "$GW_DEPS_ROOT"/Lib/HTML5/${b}_Oz.bc; done
	else
		find . -type f -name "*.bc" -exec cp {} "$GW_DEPS_ROOT"/Lib/HTML5 \;
	fi
	cd ..
}
chmod +w "$GW_DEPS_ROOT"/Lib/HTML5/*
type=Debug;       OLEVEL=0;  build_via_cmake
type=Release;     OLEVEL=2;  build_via_cmake
type=Release;     OLEVEL=3;  build_via_cmake
type=MinSizeRel;  OLEVEL=z;  build_via_cmake
ls -l "$GW_DEPS_ROOT"/Lib/HTML5


exit

# NOT USED: LEFT HERE FOR REFERENCE
# NOT USED: LEFT HERE FOR REFERENCE
# NOT USED: LEFT HERE FOR REFERENCE

build_all()
{
	echo
	echo BUILDING $OPTIMIZATION
	echo

	if [ ! -d $MAKE_PATH$OPTIMIZATION ]; then
		mkdir -p $MAKE_PATH$OPTIMIZATION
	fi

	# modify emscripten CMAKE_TOOLCHAIN_FILE
	sed -e "s/\(FLAGS_RELEASE \)\".*-O2\"/\1\"$OPTIMIZATION\"/" "$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake" > $MAKE_PATH$OPTIMIZATION/Emscripten.cmake


	cd $MAKE_PATH$OPTIMIZATION
		echo "Generating $MAKETARGET makefile..."
		export CMAKE_MODULE_PATH="$GW_DEPS_ROOT/Externals/CMakeModules"
		cmake -DCMAKE_TOOLCHAIN_FILE="Emscripten.cmake" -DTARGET_BUILD_PLATFORM="HTML5" \
			-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=ON \
			-DCMAKE_BUILD_TYPE="Release" \
			-DPHYSX_ROOT_DIR="$GW_DEPS_ROOT/PhysX_3.4" \
			-DPXSHARED_ROOT_DIR="$GW_DEPS_ROOT/PxShared" \
			-DNVSIMD_INCLUDE_DIR="$GW_DEPS_ROOT/PxShared/src/NvSimd" \
			-DNVTOOLSEXT_INCLUDE_DIRS="$GW_DEPS_ROOT/PhysX_3.4/externals/nvToolsExt/include" \
			$CustomFlags -G "Unix Makefiles" "$GW_DEPS_ROOT/$MAKETARGET/compiler/cmake/HTML5"

		echo "Building $MAKETARGET ..."
		emmake make clean VERBOSE=1
		emmake make VERBOSE=1 | tee log_build.txt
	cd -
}


build_pxshared()
{
	MAKETARGET=PxShared/src
	MAKE_PATH=PxShared/lib/HTML5/Build
	
	OPTIMIZATION=-O3; export LIB_SUFFIX=_O3; build_all

	OPTIMIZATION=-O2; export LIB_SUFFIX=_O2; build_all
	
	OPTIMIZATION=-Oz; export LIB_SUFFIX=_Oz; build_all
	
	OPTIMIZATION=-O0; export LIB_SUFFIX=""
	build_all
}

build_phsyx()
{
	MAKETARGET=PhysX_3.4/Source
	MAKE_PATH=PhysX_3.4/lib/HTML5/Build
	
	OPTIMIZATION=-O3; export LIB_SUFFIX=_O3; build_all

	OPTIMIZATION=-O2; export LIB_SUFFIX=_O2; build_all
	
	OPTIMIZATION=-Oz; export LIB_SUFFIX=_Oz; build_all
	
	OPTIMIZATION=-O0; export LIB_SUFFIX=""
	build_all
}

build_apex()
{
	MAKETARGET=APEX_1.4
	MAKE_PATH=APEX_1.4/lib/HTML5/Build
	CustomFlags="-DAPEX_ENABLE_UE4=1"
	
	OPTIMIZATION=-O3; export LIB_SUFFIX=_O3; build_all

	OPTIMIZATION=-O2; export LIB_SUFFIX=_O2; build_all
	
	OPTIMIZATION=-Oz; export LIB_SUFFIX=_Oz; build_all
	
	OPTIMIZATION=-O0; export LIB_SUFFIX=""
	build_all
}

#build_pxshared
#build_phsyx
# seems - apex ==> includes physx ==> which includes pxshared
# meaning, just build apex - it will build the others
build_apex


# ----------------------------------------
# install

find APEX_1.4/lib/HTML5 -type f -name "*.bc" -exec cp "{}" Lib/HTML5/. \;
#find PhysX_3.4/lib/HTML5 -type f -name "*.bc" -exec cp "{}" Lib/HTML5/. \;
#find PxShared/lib/HTML5 -type f -name "*.bc" -exec cp "{}" Lib/HTML5/. \;

