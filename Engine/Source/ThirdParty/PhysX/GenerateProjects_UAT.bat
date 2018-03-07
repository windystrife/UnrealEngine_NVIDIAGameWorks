@echo off

pushd ..\..\..\Build\BatchFiles
call RunUAT.bat BuildPhysX -SkipBuild -SkipCreateChangelist
popd