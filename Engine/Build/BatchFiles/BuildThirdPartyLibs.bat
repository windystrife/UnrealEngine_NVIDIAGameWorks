setlocal
set LIB_DIR=..\..\Source\ThirdParty
pushd %LIB_DIR%

REM make a string for using a particular changelist if specified, otherwise use default
if "%1" NEQ "" set CHANGE=-c %1

REM libPNG
pushd libPNG\libPNG-1.5.2\projects

	p4 edit %CHANGE% ..\pnglibconf.h
	p4 edit %CHANGE% ..\lib\...

	REM vs2010
	pushd vstudio
	msbuild vstudio.sln /target:Clean,libpng /p:Platform=Win32;Configuration="Release Library"
	msbuild vstudio.sln /target:Clean,libpng /p:Platform=x64;Configuration="Release Library"
	popd

	REM vs2012
	pushd vstudio11
	msbuild vstudio11.sln /target:Clean,libpng /p:Platform=Win32;Configuration="Release Library"
	msbuild vstudio11.sln /target:Clean,libpng /p:Platform=x64;Configuration="Release Library"
	popd

	REM XboxOne
	pushd XboxOne
	msbuild libpng_XboxOne.sln /target:Clean,libpng /p:Platform=x64;Configuration=Release
	popd

	REM PS4
	pushd PS4
	msbuild libpng.sln /target:Clean,libpng /p:Configuration=Release
	popd

popd

