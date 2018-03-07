SET ILMBASE_ROOT=%cd%/deploy/
mkdir build
cd build
mkdir VS2015
cd VS2015
rmdir /s /q ilmbase
mkdir ilmbase
cd ilmbase

cmake -G "Visual Studio 14 2015 Win64" -BUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=%cd%/../../../deploy/VS2015/x64/ ../../../openexr/ilmbase/
cd ../../../
