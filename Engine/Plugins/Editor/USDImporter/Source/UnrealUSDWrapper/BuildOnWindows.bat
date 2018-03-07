rem @echo off

echo "Presuming that the header files in ../ThirdParty/Linux/ were already untarred."

call :GetAbsPath "..\..\..\..\..\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe"
set CMAKE=%RETVAL%
echo "CMake is located at %CMAKE%"

call :GetAbsPath "..\..\..\..\..\Extras\ThirdPartyNotUE\GNU_Make\make-3.81\bin\make.exe"
set MAKE=%RETVAL%
echo "Make is located at %MAKE%"

rmdir /S /Q build-Windows
mkdir build-Windows
cd build-Windows
%CMAKE% -G "Unix Makefiles" -DCMAKE_MAKE_PROGRAM=%MAKE% -DARCHITECTURE_TRIPLE=x86_64-unknown-linux-gnu -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../LinuxCrossToolchain.multiarch.cmake ..

rem %CMAKE% -DCMAKE_BUILD_TYPE=Debug ..
rem %MAKE% VERBOSE=1
%MAKE% VERBOSE=1
copy /B libUnrealUSDWrapper.so ..\..\..\Binaries\Linux\x86_64-unknown-linux-gnu\
popd

exit /B

:GetAbsPath
set RETVAL=%~dpfn1
exit /B 0

