#!/bin/bash

set -x


# the following is needed for Ubuntu 16.04 and up
# - https://robinwinslow.uk/2016/06/23/fix-docker-networking-dns/
DNS_IP4=`nmcli device list | grep IP4.DNS | head -1 | awk '{print $2}'`
DNS="--dns $DNS_IP4"


# UE4 supports CentOS 7.x Linux for some vendors
# this build script targets that specific platform.
# - will setup a build environment
# - and will run the [ ./BUILD.EPIC.sh ] build script (that's also used for
#   all of the other major platforms)
# this is primarily for automated builds on newer Linux distros...
#
# if you're just looking to build the libraries for your Linux distribution
# - just run the ./BUILD.EPIC.sh script by itself

# ------------------------------------------------------------
docker $DNS pull centos:7
# can test image with:
# docker $DNS run -it --rm --name test centos:7 bash

# ------------------------------------------------------------
docker $DNS build -t centos:7_buildtools -f Dockerfile.CentOS-7-build_tools .
# can test image with:
# docker $DNS run -it --rm --name test centos:7_buildtools bash

# ------------------------------------------------------------
docker $DNS build -t centos:7_z_ssl_curl_lws -f Dockerfile.CentOS-7-z_ssl_curl_websockets .
# can test image with:
# docker $DNS run -it --rm --name test centos:7_z_ssl_curl_lws bash


# ------------------------------------------------------------
# copy the compiled library files
docker $DNS run --name tmp_z_ssl_curl_lws centos:7_z_ssl_curl_lws
FOLDER=$(docker $DNS run --rm centos:7_z_ssl_curl_lws \
	find . -type d -name "INSTALL.*" -print); \
	for i in $FOLDER; do docker cp tmp_z_ssl_curl_lws:/home/work/$i/. $i ; done
docker rm tmp_z_ssl_curl_lws

# ------------------------------------------------------------
# handy commands to know:

# docker ps -a    # all
# docker ps -aq
# docker ps -l    # latest
# docker rm <containerID>

# docker images
# docker rmi <image>

