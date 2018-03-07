@echo off
REM glslang
pushd glslang\projects

	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\...

	REM vs2015 x64
	pushd vs2015
	msbuild glslang.sln /target:Clean,glslang_lib /p:Platform=x64;Configuration="Debug"
	msbuild glslang.sln /target:Clean,glslang_lib /p:Platform=x64;Configuration="Release"
	popd

	REM Linux (only if LINUX_ROOT is defined)
	set CheckLINUX_ROOT=%LINUX_ROOT%
	if "%CheckLINUX_ROOT%"=="" goto SkipLinux

	pushd Linux
	CrossCompile.bat
	popd

:SkipLinux

popd
