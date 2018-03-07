REM @echo off

setlocal

REM * build openssl as static lib
REM * build_openssl_win <source directory> <2012/2013/2015> <x86/x64>

set ROOT_DIR=%cd%
set OPENSSL_SDK=%1
set OPENSSLSDK_ROOT=%ROOT_DIR%\%OPENSSL_SDK%\openssl-%OPENSSL_SDK%
set BUILD_DIR=%ROOT_DIR%\%OPENSSL_SDK%\Builds

call setup_buildenv.bat %2 %3

if "%ARCH_VER%" == "" (
	echo "Something failed to set up the target architecture"
	goto:end
)

if "%ARCH_VER%" == "x86" (
	set OPENSSL_CONFIG_DEBUG=debug-VC-WIN32 no-asm
	set OPENSSL_CONFIG_RELEASE=VC-WIN32 no-asm
	set OPENSSL_DOCOMMAND=ms\do_ms
	set OPENSSL_DESTINATION_DEBUG=%COMPILER_VER%\x86\debug
	set OPENSSL_DESTINATION_RELEASE=%COMPILER_VER%\x86\release
	set BUILD_FINAL_DESTINATION=Win32
	echo "x86 selected"
)
if "%ARCH_VER%" == "x64" (
	set OPENSSL_CONFIG_DEBUG=debug-VC-WIN64A
	set OPENSSL_CONFIG_RELEASE=VC-WIN64A
	set OPENSSL_DOCOMMAND=ms\do_win64a
	set OPENSSL_DESTINATION_DEBUG=%COMPILER_VER%\x64\debug
	set OPENSSL_DESTINATION_RELEASE=%COMPILER_VER%\x64\release
	set BUILD_FINAL_DESTINATION=Win64
	echo "x64 selected"
)

REM start build
pushd %OPENSSLSDK_ROOT%
perl Configure %OPENSSL_CONFIG_DEBUG% --prefix=%BUILD_DIR%\%OPENSSL_DESTINATION_DEBUG%
call %OPENSSL_DOCOMMAND%
nmake -f ms\nt.mak reallyclean
nmake -f ms\nt.mak
nmake -f ms\nt.mak install
perl Configure %OPENSSL_CONFIG_RELEASE% --prefix=%BUILD_DIR%\%OPENSSL_DESTINATION_RELEASE%
call %OPENSSL_DOCOMMAND%
nmake -f ms\nt.mak reallyclean
nmake -f ms\nt.mak
nmake -f ms\nt.mak install
popd

set COMPILER_DIR=VS%COMPILER_VER%

REM copy include files from the release directory to the general include folder
mkdir ..\%1\include\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%
xcopy /s /c /d /y %BUILD_DIR%\%OPENSSL_DESTINATION_RELEASE%\include\* ..\%1\include\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%
perl -p -i.bak -e "s/#define (OPENSSLDIR|ENGINESDIR) \".+?\"/#define $1 \"\"/" ../%OPENSSL_SDK%/include/%BUILD_FINAL_DESTINATION%/%COMPILER_DIR%/openssl/opensslconf.h
del ..\%OPENSSL_SDK%\include\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%\openssl\opensslconf.h.bak

REM copy libraries to general library folder
mkdir ..\%1\lib\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%
REM copy release libraries to general library folder
copy /y %BUILD_DIR%\%OPENSSL_DESTINATION_RELEASE%\lib\libeay32.lib ..\%1\lib\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%\libeay.lib
copy /y %BUILD_DIR%\%OPENSSL_DESTINATION_RELEASE%\lib\ssleay32.lib ..\%1\lib\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%\ssleay.lib

REM copy debug libraries to general library folder and add a 'd' i.e. xxxxxxxxx.lib -> xxxxxxxxxd.lib
copy /y %BUILD_DIR%\%OPENSSL_DESTINATION_DEBUG%\lib\libeay32.lib ..\%1\lib\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%\libeayd.lib
copy /y %BUILD_DIR%\%OPENSSL_DESTINATION_DEBUG%\lib\ssleay32.lib ..\%1\lib\%BUILD_FINAL_DESTINATION%\%COMPILER_DIR%\ssleayd.lib

endlocal

:end