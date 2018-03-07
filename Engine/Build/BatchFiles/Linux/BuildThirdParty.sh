#!/bin/bash
# Script for building ThirdParty modules for linux.
# You must run UpdateDeps.sh to download and patch the sources before running
# this script.

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)
cd "${TOP_DIR}"
mkdir -p Binaries/Linux/
set -e

MAKE_ARGS=-j4
TARGET_ARCH=x86_64-unknown-linux-gnu
#TARGET_ARCH=arm-unknown-linux-gnueabihf
export TARGET_ARCH

SDL_DIR=SDL-gui-backend
SDL_BUILD_DIR=build-$SDL_DIR

# Open files for edit using p4 command line.
# Does nothing if the file is already writable (which is case for external
# github developers).
P4Open()
{
  for file in $@; do
    # If the file does not exist do nothing
    if [ ! -e $file ]; then
      return
    fi
    # If the file is already writable do nothing
    if [ -w $file ]; then
      return
    fi
    # If the file is already writable do nothing
    if ! which p4 > /dev/null; then
      echo "File is not writable and 'p4' command not found."
      exit 1
    fi
    set +x
    p4 open "$file"
    set -x
  done
}

BuildZ()
{
  echo "building zlib"
  set -x
  cd Source/ThirdParty/zlib/zlib-1.2.5/build
  tar xf zlib-1.2.5.tar.gz
  cd zlib-1.2.5
  CFLAGS=-fPIC ./configure
  make $MAKE_ARGS
  local LIB_DIR=../../Lib/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libz.a
  P4Open $LIB_DIR/libz_fPIC.a
  mkdir -p $LIB_DIR
  cp libz.a $LIB_DIR/libz_fPIC.a
  set +x
}

BuildJemalloc()
{
  echo "building jemalloc"
  set -x
  cd Source/ThirdParty/jemalloc/build
  tar xf jemalloc-3.6.0.tar.bz2
  cd jemalloc-3.6.0
  ./configure --with-mangling --with-jemalloc-prefix=je_
  make $MAKE_ARGS
  local LIB_DIR=../../lib/Linux/$TARGET_ARCH
  local INC_DIR=../../include/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libjemalloc.a
  P4Open $LIB_DIR/libjemalloc_pic.a
  mkdir -p $LIB_DIR
  cp lib/libjemalloc.a $LIB_DIR
  cp lib/libjemalloc_pic.a $LIB_DIR
  mkdir -p $INC_DIR
  P4Open $INC_DIR/jemalloc_defs.h
  P4Open $INC_DIR/jemalloc.h
  cp -rp include/jemalloc/* $INC_DIR
  set +x
}

BuildOpus()
{
  echo "building libOpus"
  set -x
  cd Source/ThirdParty/libOpus/opus-1.1/
  P4Open configure
  chmod +x configure
  ./configure --with-pic
  make $MAKE_ARGS
  local LIB_DIR=Linux/$TARGET_ARCH
  mkdir -p $LIB_DIR
  P4Open $LIB_DIR/libopus_fPIC.a
  cp .libs/libopus.a $LIB_DIR/libopus_fPIC.a
  set +x
}

BuildOgg()
{
  echo "building Ogg"
  set -x
  cd Source/ThirdParty/Ogg/libogg-1.2.2/
  P4Open configure
  chmod +x configure
  ./configure --with-pic
  make $MAKE_ARGS

  local LIB_DIR=lib/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libogg.a
  P4Open $LIB_DIR/libogg_fPIC.a
  mkdir -p $LIB_DIR
  cp src/.libs/libogg.a $LIB_DIR
  cp $LIB_DIR/libogg.a $LIB_DIR/libogg_fPIC.a
  set +x
}

BuildVorbis()
{
  echo "building Vorbis"
  set -x
  cd Source/ThirdParty/Vorbis/libvorbis-1.3.2/
  P4Open configure
  chmod +x configure
  OGG_LIBS=../../Ogg/libogg-1.2.2/lib/Linux \
      OGG_CFLAGS=-I../../../Ogg/libogg-1.2.2/include \
      ./configure --with-pic --disable-oggtest
  make $MAKE_ARGS

  local LIB_DIR=lib/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libvorbis.a $LIB_DIR/libvorbisfile.a $LIB_DIR/libvorbis_fPIC.a $LIB_DIR/libvorbisfile_fPIC.a
  P4Open $LIB_DIR/libvorbisenc.a $LIB_DIR/libvorbisenc_fPIC.a 
  mkdir -p $LIB_DIR
  cp lib/.libs/libvorbis*.a $LIB_DIR/
  cp $LIB_DIR/libvorbis.a libvorbis_fPIC.a
  cp $LIB_DIR/libvorbisfile.a libvorbisfile_fPIC.a
  cp $LIB_DIR/libvorbisenc.a libvorbisenc_fPIC.a
  set +x
  cd -
}

BuildHLSLCC()
{
  echo "building hlslcc"
  set -x
  cd Source/ThirdParty/hlslcc
  # not building anymore P4Open hlslcc/bin/Linux/hlslcc_64
  P4Open hlslcc/lib/Linux/$TARGET_ARCH/libhlslcc.a
  cd hlslcc/projects/Linux
  set +e
  CLANG_TO_USE=`which clang`
  set -e
  if [ ! -f "$CLANG_TO_USE" ]; then
    echo "Please install clang package and/or update alternatives appropriately so it is available as \"clang\""
    exit 1
  fi

  set +e
  CLANGXX_TO_USE=`which clang++`
  set -e
  if [ ! -f "$CLANGXX_TO_USE" ]; then
    echo "Please install clang++ package and/or update alternatives appropriately so it is available as \"clang++\""
    exit 1
  fi

  make $MAKE_ARGS CC=$CLANG_TO_USE CXX=$CLANGXX_TO_USE clean
  make $MAKE_ARGS CC=$CLANG_TO_USE CXX=$CLANGXX_TO_USE 
  set +x
}

BuildMcpp()
{
  echo "building MCPP"
  set -x
  cd Source/ThirdParty/MCPP/mcpp-2.7.2
  P4Open configure
  chmod +x configure
  ./configure --enable-shared --enable-shared --enable-mcpplib
  make $MAKE_ARGS

  local LIB_DIR=lib/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libmcpp.a
  P4Open $LIB_DIR/libmcpp.so
  P4Open ${TOP_DIR}/Binaries/Linux/libmcpp.so.0
  mkdir -p $LIB_DIR
  cp --remove-destination ./src/.libs/libmcpp.a $LIB_DIR/
  cp --remove-destination ./src/.libs/libmcpp.so $LIB_DIR/
  cp --remove-destination ./src/.libs/libmcpp.so ${TOP_DIR}/Binaries/Linux/libmcpp.so.0
  set +x
}

BuildFreeType()
{
  echo "building freetype"
  set -x
  cd Source/ThirdParty/FreeType2/FreeType2-2.4.12/src
  pwd
  make $MAKE_ARGS -f ../Builds/Linux/makefile $*

  local LIB_DIR=../Lib/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libfreetype2412.a
  P4Open $LIB_DIR/libfreetype2412_fPIC.a
  cp --remove-destination libfreetype2412.a $LIB_DIR/libfreetype2412.a
  cp $LIB_DIR/libfreetype2412.a $LIB_DIR/libfreetype2412_fPIC.a
  set +x
}

BuildICU()
{
  echo "building libICU"
  set -x
  cd Source/ThirdParty/ICU/icu4c-53_1/source
  P4Open configure
  chmod +x configure
  CPPFLAGS=-fPIC ./configure --enable-static
  pwd
  make $MAKE_ARGS $*

  local LIB_DIR=../Linux/$TARGET_ARCH
  mkdir -p $LIB_DIR
  mkdir -p ${TOP_DIR}/Binaries/ThirdParty/ICU/icu4c-53_1/Linux/$TARGET_ARCH/
  P4Open $LIB_DIR/libicudata.a $LIB_DIR/libicudatad.a $LIB_DIR/libicui18n.a $LIB_DIR/libicui18nd.a
  P4Open $LIB_DIR/libicuio.a $LIB_DIR/libicuiod.a $LIB_DIR/libicule.a $LIB_DIR/libiculed.a
  P4Open $LIB_DIR/libiculx.a $LIB_DIR/libiculxd.a $LIB_DIR/libicutu.a $LIB_DIR/libicutud.a
  P4Open $LIB_DIR/libicuuc.a $LIB_DIR/libicuucd.a
  cp --remove-destination lib/*.a $LIB_DIR
  cp -P --remove-destination lib/*.so* ${TOP_DIR}/Binaries/ThirdParty/ICU/icu4c-53_1/Linux/$TARGET_ARCH/

  set +x
}

BuildForsythTriOO()
{
  echo "building ForsythTriOO"
  set -x
  cd Source/ThirdParty/ForsythTriOO
  rm -rf Build/Linux
  mkdir -p Build/Linux
  mkdir -p Lib/Linux/$TARGET_ARCH
  cd Build/Linux
  cmake -DCMAKE_BUILD_TYPE=Debug ../../
  make
  cmake -DCMAKE_BUILD_TYPE=Release ../../
  make
  P4Open ../../Lib/Linux/$TARGET_ARCH/libForsythTriOptimizer*
  cp --remove-destination libForsythTriOptimizer* ../../Lib/Linux/$TARGET_ARCH/
  set +x
}

BuildnvTriStrip()
{
  echo "building nvTriStrip"
  set -x
  cd Source/ThirdParty/nvTriStrip/nvTriStrip-1.0.0
  rm -rf Build/Linux
  mkdir -p Build/Linux
  mkdir -p Lib/Linux/$TARGET_ARCH
  cd Build/Linux
  cmake -DCMAKE_BUILD_TYPE=Debug ../../
  make
  cmake -DCMAKE_BUILD_TYPE=Release ../../
  make
  P4Open ../../Lib/Linux/$TARGET_ARCH/libnvtristrip*
  cp --remove-destination libnvtristrip* ../../Lib/Linux/$TARGET_ARCH/
  set +x
}

BuildnvTextureTools()
{
  echo "building nvTextureTools"
  set -x
  cd Source/ThirdParty/nvTextureTools/nvTextureTools-2.0.8
  mkdir -p lib/Linux/$TARGET_ARCH
  cd src
  rm -rf build
  P4Open configure
  chmod +x ./configure
  CXXFLAGS=-fPIC ./configure --release
  make

  local LIB_DIR=../lib/Linux/$TARGET_ARCH
  P4Open $LIB_DIR/libnvcore.so
  cp --remove-destination build/src/nvcore/libnvcore.so $LIB_DIR/
  P4Open $LIB_DIR/libnvimage.so
  cp --remove-destination build/src/nvimage/libnvimage.so $LIB_DIR/
  P4Open $LIB_DIR/libnvmath.so
  cp --remove-destination build/src/nvmath/libnvmath.so $LIB_DIR/
  P4Open $LIB_DIR/libnvtt.so
  cp --remove-destination build/src/nvtt/libnvtt.so $LIB_DIR/
  P4Open $LIB_DIR/libsquish.so
  cp --remove-destination build/src/nvtt/squish/libsquish.a $LIB_DIR/
  cp --remove-destination $LIB_DIR/*.so ${TOP_DIR}/Binaries/Linux/
  set +x
}

BuildSDL2()
{
  echo "building SDL2"
  set -x
  cd Source/ThirdParty/SDL2/
  rm -rf $SDL_BUILD_DIR
  mkdir -p $SDL_BUILD_DIR
  cd $SDL_BUILD_DIR

  cmake ../$SDL_DIR
  make ${MAKE_ARGS}
  P4Open ../$SDL_DIR/lib/Linux/x86_64-unknown-linux-gnu/libSDL2.a
  P4Open ../$SDL_DIR/lib/Linux/x86_64-unknown-linux-gnu/libSDL2_fPIC.a

  cp --remove-destination libSDL2.a ../$SDL_DIR/lib/Linux/x86_64-unknown-linux-gnu/libSDL2.a
  cp --remove-destination libSDL2_fPIC.a ../$SDL_DIR/lib/Linux/x86_64-unknown-linux-gnu/libSDL2_fPIC.a
  set +x
}

Buildcoremod()
{
  echo "building coremod"
  set -x
  cd Source/ThirdParty/coremod/coremod-4.2.6
  pwd
  autoconf
  CFLAGS=-fPIC ./configure --enable-static
  make $MAKE_ARGS
  local LIB_DIR=lib/Linux/$TARGET_ARCH
  mkdir -p $LIB_DIR

  P4Open $LIB_DIR/libcoremodLinux.a
  cp lib/libxmp-coremod.a $LIB_DIR/libcoremodLinux.a
  set +x
}

FixICU()
{
  echo "setting symlinks for ICU libraries (possibly temporary)"
  set -x
  cd Binaries/ThirdParty/ICU/icu4c-53_1/Linux/x86_64-unknown-linux-gnu
  pwd
  ln -sf libicudata.so libicudata.so.53
  ln -sf libicudata.so libicudata.53.1.so
  ln -sf libicui18n.so libicui18n.so.53
  ln -sf libicui18n.so libicui18n.53.1.so
  ln -sf libicuio.so libicuio.so.53
  ln -sf libicuio.so libicuio.53.1.so
  ln -sf libicule.so libicule.so.53
  ln -sf libicule.so libicule.53.1.so
  ln -sf libiculx.so libiculx.so.53
  ln -sf libiculx.so libiculx.53.1.so
  ln -sf libicutu.so libicutu.so.53
  ln -sf libicutu.so libicutu.53.1.so
  ln -sf libicuuc.so libicuuc.so.53
  ln -sf libicuuc.so libicuuc.53.1.so
  set +x
}

Run()
{
  cd ${TOP_DIR}
  echo "==> $1"
  if [[ $VERBOSE -eq 1 ]]; then
    $1
  else
    $1 >> ${SCRIPT_DIR}/BuildThirdParty.log 2>&1
  fi
}

Success() {
    if [ -z $1 ]; then
        echo "**********  SUCCESS ****************"
    else
        echo "**********  SUCCESS $1 ****************"
    fi
}
build_all() {
   Run BuildJemalloc
   Run BuildZ
   Run BuildOpus
   Run BuildOgg
   Run BuildVorbis
   Run BuildHLSLCC
   Run BuildMcpp
   Run BuildFreeType
   Run BuildForsythTriOO
   Run BuildnvTriStrip
   Run BuildnvTextureTools
   Run BuildSDL2
   Run Buildcoremod
   Run FixICU
}

print_help() {

 echo "-a|--all Rebuild all ThirdParty libs"
 echo "-v|--verbose Print full output, otherwise output is logged to BuildThirdParty.log"
 echo "-b|--build BUILD_ME|--build=BUILD_ME Rebuild a specific library"
 print_valid_build_opts
 echo "-h|--help Print this help and exit"

}

print_valid_build_opts() {
  echo "  Valid build choices are"
  echo "    Jemalloc"
  echo "    Z"
  echo "    Opus"
  echo "    Ogg"
  echo "    Vorbis"
  echo "    HLSLCC"
  echo "    Mcpp"
  echo "    FreeType"
  echo "    ForsythTriOO"
  echo "    nvTriStrip"
  echo "    nvTextureTools"
  echo "    SDL2"
  echo "    coremod"
  echo "    ICU"
}

BuildList=()

# Parse command line parameters
while :; do
    case $1 in
        -h|--help)
            print_help
            exit
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            continue
            ;;
        -a|--all)
            ALL=1
            shift
            continue
            ;;
        -b|--build) # Add to the list of packages to build
            if [ "$2" ]; then
                BuildList+=("$2")
                BUILD_LIST=1
                shift 2
                continue
             else
                echo 'ERROR: Must specify non empty "--build=BUILD" argument.' >&2
                print_valid_build_opts
                exit 1
             fi
             ;;
        --build=?*) # Add to the list of packages to build
            BuildList+=("${1#*=}")
            BUILD_LIST=1
            shift
            continue
            ;;
        --build=)
            echo 'ERROR: Must specify non empty "--build=BUILD" argument.' >&2
            exit 1
            ;;
        --)
            shift
            break
            ;;
        -?*)
            printf "WARN: Unknown Option (ignored): %s\n' "$1" >&2"
            ;;
        *)
            break
  esac

  shift
done

echo "Building ThirdParty libraries"
echo
echo "If you don't see SUCCESS message in the end, then building did not finish properly."
echo "In that case, take a look into ${SCRIPT_DIR}/BuildThirdParty.log for details."
echo

rm -f ${SCRIPT_DIR}/BuildThirdParty.log

# if -b --build or --build=?* is used, build the list
# instead of the default or all
if [[ ${#BuildList[@]} -ne 0 &&  $ALL -eq 0 ]]; then
    for var in "${BuildList[@]}"
    do
        if [[ "${var}" -ne "ICU" ]]; then
            Run Fix"${var}"
            Success "${var}"
        else
            Run Build"${var}"
            Success "${var}"
        fi
    done
    exit
elif [[ ${#BuildList[@]} -ne 0 ]] && [[ $ALL -eq 1 ]]; then
    echo "ERROR: Can not build all and individual packages at the same time"
    exit
fi

if [[ $ALL -eq 1 ]]; then
  build_all
else
  # we should not need to build hlslcc locally because it is now using the same bundled STL
  #Run BuildHLSLCC
  echo "No third party libs needed to be built locally"
fi

echo
Success

