PYTHON_VER=2.7.13
BOOST_VER=boost_1_61_0  # must match tar.bz2 name and directory inside it
# boost seems to be changing hosts, URLs may differ between versions
BOOST_DOWNLOAD_URL=https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.bz2
TBB_VER=4.4.6   # must match .tar.gz name and tbb-$TBB_VER directory inside it
USD_VER=v0.7.5     # must match tag name
MAX_JOBS=64	# assume 64-core builder
UE_THIRD_PARTY_DIR=`cd "../../../../../../../Source/ThirdParty"; pwd`
echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"
