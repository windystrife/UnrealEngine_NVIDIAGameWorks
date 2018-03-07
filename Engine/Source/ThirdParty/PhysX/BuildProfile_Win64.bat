@echo off

pushd ..\..\..\Build\BatchFiles
call RunUAT.bat BuildPhysX -TargetPlatforms=Win64 -TargetConfigs=profile -TargetWindowsCompilers=VisualStudio2015 -SkipCreateChangelist
popd