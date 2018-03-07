#!/bin/sh
# Script by RCL^People Can Fly to automate building of libcurl (+OpenSSL)
# Needs to be run on a Linux installation
# Prerequisites:
#  gcc
#  (usual core utils)

#####################
# configuration

# library versions - expected to match tarball and directory names
CURL_VER=curl-7.48.0
OPENSSL_VER=openssl-1.0.2h
OPENSSL_ARCH=linux-elf
#OPENSSL_ARCH=linux-x86_64
#OPENSSL_ARCH=linux-aarch64

# don't forget to match archive options with tarball type (bz/gz)
CURL_TARBALL=$CURL_VER.tar.bz2
OPENSSL_TARBALL=$OPENSSL_VER.tar.gz

# includ PID in scratch dir - needs to be absolute
SCRATCH_DIR=/tmp/scratch/$$
OPENSSL_DIR=$SCRATCH_DIR/$OPENSSL_VER
CURL_DIR=$SCRATCH_DIR/$CURL_VER
OPENSSL_INSTALL_DIR=$SCRATCH_DIR/$OPENSSL_VER-install

DEST_DIR=`pwd`

#####################
# unpack

rm -rf $SCRATCH_DIR
mkdir -p $SCRATCH_DIR

echo "#######################################"
echo "# Unpacking the tarballs"
tar xzf $OPENSSL_TARBALL -C $SCRATCH_DIR
tar xjf $CURL_TARBALL -C $SCRATCH_DIR

#####################
# build openssl

cd $OPENSSL_DIR
echo "#######################################"
echo "# Configuring $OPENSSL_VER for $OPENSSL_ARCH"
# Don't forget to remove -m32 for 64-bit builds
./Configure shared threads no-ssl2 no-zlib $OPENSSL_ARCH -m32 --prefix=$OPENSSL_DIR > $DEST_DIR/openssl-configure.log
echo "# Building $OPENSSL_VER"
make > $DEST_DIR/openssl-build.log
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi

#####################
# build libcurl

cd $CURL_DIR
echo "#######################################"
echo "# Configuring $CURL_VER"
# configure says use PKG_CONFIG_PATH whenever possible instead of --with-ssl, but it doesn't seem to work that well alone
export PKG_CONFIG_PATH=$OPENSSL_DIR
export LD_LIBRARY_PATH=$OPENSSL_DIR:$LD_LIBRARY_PATH
env LDFLAGS=-L$OPENSSL_DIR ./configure --with-ssl=$OPENSSL_DIR --enable-static --enable-threaded-resolver --disable-shared --enable-hidden-symbols --disable-ftp --disable-file --disable-ldap --disable-ldaps --disable-rtsp --disable-dict --disable-telnet --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smtp --disable-gopher --disable-manual --disable-idn > $DEST_DIR/curl-configure.log 
echo "# Building $CURL_VER"
env LDFLAGS=-L$OPENSSL_DIR make > $DEST_DIR/curl-build.log 
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi

#####################
# deploy

# use some hardcoded knowledge and get static library out
cp $OPENSSL_DIR/libssl.a $DEST_DIR
cp $OPENSSL_DIR/libcrypto.a $DEST_DIR
cp $CURL_DIR/include/curl/curlbuild.h $DEST_DIR
cp $CURL_DIR/lib/.libs/libcurl.a $DEST_DIR

if [ $? -eq 0 ]; then
	echo ""
	echo "#######################################"
	echo "# Newly built libssl.a, libcrypto.a, libcurl.a and corresponding curlbuild.h have been put into current directory."
	echo ""
fi

