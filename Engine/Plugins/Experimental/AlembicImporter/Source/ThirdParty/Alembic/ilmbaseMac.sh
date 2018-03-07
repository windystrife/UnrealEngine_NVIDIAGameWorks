export ILMBASE_ROOT=$(pwd)/../deploy
echo $ILMBASE_ROOT
mkdir build
cd build
mkdir Mac
cd Mac
rm -rf ilmbase
mkdir ilmbase
cd ilmbase

cmake -G "Xcode" -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" -BUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$(pwd)/../../../deploy/Mac/ ../../../openexr/ilmbase/
/usr/bin/xcodebuild -target ALL_BUILD -configuration Debug
/usr/bin/xcodebuild -target install -configuration Debug
/usr/bin/xcodebuild -target ALL_BUILD -configuration Release
/usr/bin/xcodebuild -target install -configuration Release
cd ../../../
