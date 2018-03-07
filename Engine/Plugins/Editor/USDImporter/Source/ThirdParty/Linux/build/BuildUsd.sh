#!/bin/bash
 
set -e

. Versions.inc
STEP_TO_START=$1
if [ -z $STEP_TO_START ]; then
	STEP_TO_START=0
fi

PYTHON_INSTALL_DIR=`pwd`/localpython

export CC=clang
export CXX=clang++
export CXXFLAGS="-fPIC -nostdinc++ -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
#export LDFLAGS="-stdlib=libc++ -L $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/ $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a"
export LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/"
export LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc"

if [ $STEP_TO_START -le 0 ]; then

	# Python
	rm -rf Python-$PYTHON_VER
	tar xf Python-$PYTHON_VER.tar.xz
	
	
	pushd Python-$PYTHON_VER
	export CPPFLAGS=$CXXFLAGS
	./configure --prefix=$PYTHON_INSTALL_DIR --disable-shared --oldincludedir=$PYTHON_INSTALL_DIR/include
	make install -j $MAX_JOBS 2>&1 >../python_build_log.txt
	popd

	STEP_TO_START=$(($STEP_TO_START+1))
fi

#exit 0
 
if [ $STEP_TO_START -le 1 ]; then

	# Boost
	tar xf $BOOST_VER.tar.bz2
	pushd $BOOST_VER
	./bootstrap.sh --with-python-root=$PYTHON_INSTALL_DIR
	#./b2 -j $MAX_JOBS
	# using UE4's libc++abi
	# python-config can return wrong directory (a system one), hence this hack with adding Python includes here
	./b2 --with-date_time --with-iostreams --with-program_options --with-python --with-regex --with-system toolset=clang variant=release link=static cxxflags="-stdlib=libc++ -fPIC -I$PYTHON_INSTALL_DIR/include/python2.7 -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include" linkflags="-stdlib=libc++ -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a" -j $MAX_JOBS -s NO_BZIP2=1
	popd
	#FIXME: prefix doesn't seem to work--prefix=$HOME/absolute/path/  # e.g.  

	STEP_TO_START=$(($STEP_TO_START+1))
fi
 
TBB_ABS_ROOT=`pwd`/tbb-$TBB_VER

if [ $STEP_TO_START -le 2 ]; then

	# TBB
	tar xf $TBB_VER.tar.gz
	pushd $TBB_ABS_ROOT
	make compiler=clang stdlib=libc++ tbb_build_prefix=localbuild -j $MAX_JOBS
	popd
	# need to have libc++-dev package installed

	STEP_TO_START=$(($STEP_TO_START+1))
fi

if [ $STEP_TO_START -le 3 ]; then

	#USD
	# RPATH!!

	USD_INSTALL_LOCATION=`pwd`/InstalledUSD

	rm -rf USDBuild
	mkdir -p USDBuild
	cd USDBuild

	# unset C/C++ flags and take another approach
	unset CXXFLAGS
	unset LDFLAGS
	#export CXXGLAGS="-std=c++11 -stdlib=libc++ $CXXFLAGS"
	export CXXFLAGS="-std=c++11 -fPIC -nostdinc++ -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
	CMAKE_LINKER_FLAGS="-stdlib=libc++ -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/ $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a"
	cmake -DCMAKE_INSTALL_PREFIX=$USD_INSTALL_LOCATION  -DPXR_INSTALL_LOCATION=$USD_INSTALL_LOCATION -DPYTHON_INCLUDE_DIR=$PYTHON_INSTALL_DIR/include/python2.7 -DPYTHON_LIBRARY=$PYTHON_INSTALL_DIR/lib/libpython2.7.a -DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DCMAKE_MODULE_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DCMAKE_SHARED_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DCMAKE_STATIC_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DPXR_BUILD_IMAGING=FALSE -DPXR_BUILD_USD_IMAGING=FALSE -DPXR_BUILD_TESTS=FALSE -DBOOST_ROOT=../$BOOST_VER -DTBB_INCLUDE_DIRS=../tbb-$TBB_VER/include -DTBB_LIBRARIES=../tbb-$TBB_VER/build/localbuild_release -DTBB_tbb_LIBRARY=$TBB_ABS_ROOT/build/localbuild_release/libtbb.so --config Release ../USD
	# cmake -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" -DCMAKE_SHARED_LIBRARY_LINK_C_FLAGS="$LIBS" -DCMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS="$LIBS" -DCMAKE_MODULE_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DCMAKE_SHARED_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DCMAKE_STATIC_LINKER_FLAGS="$CMAKE_LINKER_FLAGS" -DPXR_BUILD_IMAGING=FALSE -DPXR_BUILD_USD_IMAGING=FALSE -DBOOST_ROOT=../boost_1_64_0 -DTBB_INCLUDE_DIRS=../tbb-$TBB_VER/include -DTBB_LIBRARIES=../tbb-$TBB_VER/build/localbuild_release -DTBB_tbb_LIBRARY=$TBB_ABS_ROOT/build/localbuild_release/libtbb.so ../USD
	make install -j $MAX_JOBS

	STEP_TO_START=$(($STEP_TO_START+1))
fi

