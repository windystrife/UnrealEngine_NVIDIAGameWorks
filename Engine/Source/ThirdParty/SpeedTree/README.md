# For Linux users to build SpeedTree plugin
# you can just: bash README.md
# from its location to execute commands below

cd SpeedTreeSDK-v7.0/Source/Core/Linux
rm -rf build
mkdir build
cd build
cmake ..
make

