#!/bin/bash


# TODO: CONVERT to CMAKE!


SYSTEM=$(uname)
if [[ $SYSTEM == *'_NT-'* ]]; then
	echo "ERROR: unable to run configure from windows"
	echo "ERROR: see Build_All_HTML5_libs.rc for details"
	exit
fi


# ----------------------------------------
ICU_HTML5=$(pwd)

cd ../../HTML5/
	. ./Build_All_HTML5_libs.rc
cd "$ICU_HTML5"


# ----------------------------------------
# using save files so i can run this script over and over again


# strip out "data" Makefile -- this is not necessary for HTML5 builds...
if [ ! -e source/Makefile.in.save ]; then
	mv source/Makefile.in source/Makefile.in.save
fi
sed -e 's/) data /) /' source/Makefile.in.save > source/Makefile.in


# change to "A = bc" -- archive to bytecode
if [ ! -e source/config/Makefile.inc.in.save ]; then
	mv source/config/Makefile.inc.in source/config/Makefile.inc.in.save
fi
sed -e 's/A = a/A = bc/' source/config/Makefile.inc.in.save > source/config/Makefile.inc.in

if [ ! -e source/icudefs.mk.in.save ]; then
	mv source/icudefs.mk.in source/icudefs.mk.in.save
fi
sed -e 's/A = a/A = bc/' -e 's/\(@ARFLAGS@\).*/\1/' source/icudefs.mk.in.save > source/icudefs.mk.in


# change STATIC_O to bc (bytecode) for both OSX and Linux
if [ ! -e source/config/mh-darwin.save ]; then
	mv source/config/mh-darwin source/config/mh-darwin.save
fi
sed -e 's/\(STATIC_O =\).*/\1 bc/' -e '/ARFLAGS/d' source/config/mh-darwin.save > source/config/mh-darwin

if [ ! -e source/config/mh-linux.save ]; then
	mv source/config/mh-linux source/config/mh-linux.save
fi
sed -e 's/\(STATIC_O =\).*/\1 bc/' -e '/ARFLAGS/d' source/config/mh-linux.save > source/config/mh-linux


# ----------------------------------------
# emcc & em++ will fail these checks and cause a missing default constructor linker issue so we force the tests to pass
export ac_cv_override_cxx_allocation_ok="yes"
export ac_cv_override_placement_new_ok="yes"


# ----------------------------------------
# MAKE

build_all()
{
	echo
	echo BUILDING $OPTIMIZATION
	echo

	if [ ! -d html5_build$OPTIMIZATION ]; then
		mkdir html5_build$OPTIMIZATION
	fi
	cd html5_build$OPTIMIZATION

	COMMONCOMPILERFLAGS="$OPTIMIZATION -DUCONFIG_NO_TRANSLITERATION=1 -DU_USING_ICU_NAMESPACE=0 -DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1 -DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -DU_STATIC_IMPLEMENTATION -DU_OVERRIDE_CXX_ALLOCATION=1"
#	EMFLAGS="-msse -msse2 -s FULL_ES2=1 -s USE_PTHREADS=1"
	EMFLAGS=""

	emconfigure ../source/configure CFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS" CXXFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS -std=c++14" CPPFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS" LDFLAGS="$OPTIMIZATION $EMFLAGS" ICULIBSUFFIX="$LIB_SUFFIX" AR="emcc" ARFLAGS="$OPTIMIZATION $EMFLAGS -o" RANLIB="echo" --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --disable-tests

	# for some reason ICULIBSUFFIX needs to be manually edited
	mv icudefs.mk icudefs.mk.save
	sed -e "s/\(ICULIBSUFFIX=\)/\1$LIB_SUFFIX/" icudefs.mk.save > icudefs.mk

	# and add build rules for .bc
	echo '
%.bc : %.o
	$(CC) $(CFLAGS) $< -o $@
' >> icudefs.mk

	# finally...
	emmake make clean VERBOSE=1
	emmake make -j VERBOSE=1 | tee log_build.txt

	cd ..
}

OPTIMIZATION=-O3; LIB_SUFFIX=_O3; build_all

OPTIMIZATION=-O2; LIB_SUFFIX=_O2; build_all

OPTIMIZATION=-Oz; LIB_SUFFIX=_Oz; build_all

OPTIMIZATION=-O0; LIB_SUFFIX=
build_all


# ----------------------------------------
# INSTALL

if [ ! -d HTML5 ]; then
	mkdir HTML5
fi

# TODO change this to p4 checkout
chmod 744 HTML5/*

cp -vp html5_build*/lib/* html5_build*/stubdata/lib* HTML5/.


# ----------------------------------------
# restore

if [ -e source/Makefile.in.save ]; then
	mv source/Makefile.in.save source/Makefile.in
fi
if [ -e source/config/Makefile.inc.in.save ]; then
	mv source/config/Makefile.inc.in.save source/config/Makefile.inc.in
fi
if [ -e source/icudefs.mk.in.save ]; then
	mv source/icudefs.mk.in.save source/icudefs.mk.in
fi
if [ -e source/config/mh-darwin.save ]; then
	mv source/config/mh-darwin.save source/config/mh-darwin
fi
if [ -e source/config/mh-linux.save ]; then
	mv source/config/mh-linux.save source/config/mh-linux
fi



# ----------------------------------------
# clean up
#rm -rf html5_build-O3 html5_build-Oz html5_build-O0


# ----------------------------------------
exit
# ----------------------------------------
# NO LONGER USED -- LEFT HERE FOR REFERENCE

build_lib ()
{
	libbasename="${1##*/}"
	finallibname="${libbasename%.a}.bc"
	echo Building $1 to $finallibname
	mkdir tmp
	cd tmp
	llvm-ar x ../$1
	for f in *.ao; do
		mv "$f" "${f%.ao}.bc";
	done
	BC_FILES=$(ls *.bc)
	emcc $OPTIMIZATION $BC_FILES -o "../$finallibname"
	cd ..
	rm -rf tmp
}

for f in ../html5_build/lib/*.a; do
	build_lib $f
done
for f in ../html5_build/stubdata/*.a; do
	build_lib $f
done
