#!/bin/sh

set -x

#LOCAL_WORKSPACE="/d/WORK/ue4/dev platform"
LOCAL_WORKSPACE="/d/WORK/ue4/portal"
#LOCAL_WORKSPACE="/Users/nickshin/Perforce/devplatform2"
#LOCAL_WORKSPACE="/Users/nickshin/Perforce/portal2"
#LOCAL_WORKSPACE="/home/nickshin/Perforce/devplat3"
#LOCAL_WORKSPACE="/home/nickshin/Perforce/portal3"
TPS="$LOCAL_WORKSPACE"/Engine/Source/ThirdParty

# ================================================================================

configure_platform()
{
	SYSTEM=$(uname)

	COPY="cp -duvpr"

	case $SYSTEM in
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Linux)
			PLATFORM=Linux/x86_64-unknown-linux-gnu
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Darwin)
			PLATFORM=Mac
			COPY="cp -vpr"    # osx doesn't support update only copies
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*_NT-*)
			;;
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		*)
			;;
	esac


}

# ================================================================================

copy_zlib()
{
	if [[ $SYSTEM == *'_NT-'* ]]; then
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=zlib/v1.2.8/INSTALL.v1.2.8/Win64/VS2015
		DST=zlib/v1.2.8/include/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=zlib/v1.2.8/lib/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		
		SRC=zlib/v1.2.8/INSTALL.v1.2.8/Win64/VS2013
		DST=zlib/v1.2.8/include/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=zlib/v1.2.8/lib/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		
		SRC=zlib/v1.2.8/INSTALL.v1.2.8/Win32/VS2015
		DST=zlib/v1.2.8/include/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=zlib/v1.2.8/lib/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
	#			$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		
		SRC=zlib/v1.2.8/INSTALL.v1.2.8/Win32/VS2013
		DST=zlib/v1.2.8/include/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=zlib/v1.2.8/lib/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
	#			$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	else
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=zlib/v1.2.8/INSTALL.v1.2.8/$PLATFORM
		DST=zlib/v1.2.8/include/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=zlib/v1.2.8/lib/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/{lib,bin}/* "$TPS"/$DST/.
		
	fi
}

copy_openssl()
{
	if [[ $SYSTEM == *'_NT-'* ]]; then
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=OpenSSL/OpenSSL_1_0_2h/INSTALL.OpenSSL_1_0_2h/Win64/VS2015
		DST=OpenSSL/1_0_2h/include/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=OpenSSL/1_0_2h/lib/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
	
		SRC=OpenSSL/OpenSSL_1_0_2h/INSTALL.OpenSSL_1_0_2h/Win64/VS2013
		DST=OpenSSL/1_0_2h/include/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=OpenSSL/1_0_2h/lib/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
	
		SRC=OpenSSL/OpenSSL_1_0_2h/INSTALL.OpenSSL_1_0_2h/Win32/VS2015
		DST=OpenSSL/1_0_2h/include/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=OpenSSL/1_0_2h/lib/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
	
		SRC=OpenSSL/OpenSSL_1_0_2h/INSTALL.OpenSSL_1_0_2h/Win32/VS2013
		DST=OpenSSL/1_0_2h/include/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=OpenSSL/1_0_2h/lib/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	else
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=OpenSSL/OpenSSL_1_0_2h/INSTALL.OpenSSL_1_0_2h/$PLATFORM
		DST=OpenSSL/1_0_2h/include/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=OpenSSL/1_0_2h/lib/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/{lib,bin}/* "$TPS"/$DST/.
		
	fi
}

copy_libcurl()
{
	if [[ $SYSTEM == *'_NT-'* ]]; then
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win64/VS2015/libcurl-vc14-x64-release-static-ssl-static-zlib-static-ipv6-sspi
		DST=libcurl/7_48_0/include/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=libcurl/7_48_0/lib/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win64/VS2015/libcurl-vc14-x64-release-dll-ssl-static-zlib-static-ipv6-sspi
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
	
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win64/VS2013/libcurl-vc12-x64-release-static-ssl-static-zlib-static-ipv6-sspi
		DST=libcurl/7_48_0/include/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=libcurl/7_48_0/lib/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win64/VS2013/libcurl-vc12-x64-release-dll-ssl-static-zlib-static-ipv6-sspi
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win32/VS2015/libcurl-vc14-x86-release-static-ssl-static-zlib-static-ipv6-sspi
		DST=libcurl/7_48_0/include/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=libcurl/7_48_0/lib/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win32/VS2015/libcurl-vc14-x86-release-dll-ssl-static-zlib-static-ipv6-sspi
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win32/VS2013/libcurl-vc12-x86-release-static-ssl-static-zlib-static-ipv6-sspi
		DST=libcurl/7_48_0/include/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=libcurl/7_48_0/lib/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/Win32/VS2013/libcurl-vc12-x86-release-dll-ssl-static-zlib-static-ipv6-sspi
				$COPY $SRC/lib/* "$TPS"/$DST/.
				$COPY $SRC/bin/*.dll "$TPS"/$DST/.
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	else
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/$PLATFORM/
		DST=libcurl/7_48_0/include/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=libcurl/7_48_0/lib/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
		SRC=libcurl/curl-7_48_0/INSTALL.curl-7_48_0/$PLATFORM/
				$COPY $SRC/{lib,bin}/* "$TPS"/$DST/.
		
	fi
}

copy_webrtc()
{
	if [[ $SYSTEM == *'_NT-'* ]]; then
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=/d/googVS2015/WebRTC/src/INSTALL.12643/Win64/VS2015
		DST=WebRTC/rev.12643/include/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=WebRTC/rev.12643/lib/Win64/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
	#			$COPY $SRC/bin/*.dll "$TPS"/$DST/.
	
		SRC=/d/googVS2015/WebRTC/src/INSTALL.12643/Win32/VS2015
		DST=WebRTC/rev.12643/include/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=WebRTC/rev.12643/lib/Win32/VS2015
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
	#			$COPY $SRC/bin/*.dll "$TPS"/$DST/.
	
		# NOTE: VS2013 is no longer supported by google
		SRC=/d/googVS2013/WebRTC/src/INSTALL.9862/Win64/VS2013
		DST=WebRTC/rev.9862/include/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/* "$TPS"/$DST/.
		DST=WebRTC/rev.9862/lib/Win64/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/* "$TPS"/$DST/.
	
		SRC=/d/googVS2013/WebRTC/src/INSTALL.9862/Win32/VS2013
		DST=WebRTC/rev.9862/include/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/* "$TPS"/$DST/.
		DST=WebRTC/rev.9862/lib/Win32/VS2013
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/* "$TPS"/$DST/.
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	else
		# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		SRC=WebRTC/src/INSTALL.12643/$PLATFORM
		DST=WebRTC/rev.12643/include/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/include/* "$TPS"/$DST/.
		DST=WebRTC/rev.12643/lib/$PLATFORM
			rm -rf "$TPS"/$DST
			mkdir -p "$TPS"/$DST
				$COPY $SRC/lib/* "$TPS"/$DST/.
		
	fi
}

# ================================================================================

configure_platform
copy_zlib
copy_openssl
copy_libcurl
copy_webrtc

