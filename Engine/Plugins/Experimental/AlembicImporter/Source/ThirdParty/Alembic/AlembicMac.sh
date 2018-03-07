export ILMBASE_ROOT=$(pwd)/deploy/Mac/
export HDF5_ROOT=$(pwd)/deploy/Mac/
mkdir build
cd build
mkdir Mac
cd Mac
rm -rf alembic
mkdir alembic
cd alembic
cmake -G "Xcode" -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" -DZLIB_INCLUDE_DIR=$(pwd)/../../../../../..//Source/ThirdParty/zlib/v1.2.8/include/Win64/VS2015/ -DZLIB_LIBRARY=$(pwd)/../../../../../../Source/ThirdParty/zlib/v1.2.8/lib/Win64/VS2015/ -DALEMBIC_SHARED_LIBS=OFF -DUSE_TESTS=OFF -DUSE_BINARIES=OFF  -DUSE_HDF5=ON -DALEMBIC_ILMBASE_LINK_STATIC=ON -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../deploy/Mac/ ../../../alembic/
/usr/bin/xcodebuild -target ALL_BUILD -configuration Debug
/usr/bin/xcodebuild -target install -configuration Debug
/usr/bin/xcodebuild -target ALL_BUILD -configuration Release
/usr/bin/xcodebuild -target install -configuration Release
cd ../../../
