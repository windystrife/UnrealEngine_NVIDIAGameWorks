mkdir build
cd build
mkdir Mac
cd Mac
rm -rf hdf5
mkdir hdf5
cd hdf5
cmake -G "Xcode" -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" -DBUILD_SHARED_LIBS=OFF -DHDF5_BUILD_CPP_LIB=OFF -DHDF5_BUILD_HL_LIB=OFF -DHDF5_BUILD_EXAMPLES=OFF -DHDF5_BUILD_TOOLS=OFF -DHDF5_EXTERNAL_LIB_PREFIX="" -DHDF5_ENABLE_Z_LIB_SUPPORT=ON -DHDF5_ENABLE_THREADSAFE=ON -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../deploy/Mac/ ../../../hdf5
/usr/bin/xcodebuild -target ALL_BUILD -configuration Debug
/usr/bin/xcodebuild -target install -configuration Debug
/usr/bin/xcodebuild -target ALL_BUILD -configuration Release
/usr/bin/xcodebuild -target install -configuration Release
cd ../../../
