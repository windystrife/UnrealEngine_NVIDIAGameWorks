rem * sync latest trunk of webrtc, generates .gyp files, and builds with ninja
rem * make sure to download latest depo tools first
rem * https://sites.google.com/a/chromium.org/dev/developers/how-tos/depottools

set OUTPUT_DIR=out
set GYP_GENERATORS=ninja
rem set GYP_GENERATORS=msvs
set GYP_MSVS_VERSION=2013
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

call gclient config --unmanaged --verbose --name=src https://chromium.googlesource.com/external/webrtc.git
cmd /c echo | set /p="target_os = ['win']" >> .gclient
call gclient sync --verbose --force

set BUILD_MODE=Release
if "%2" == "-debug" set BUILD_MODE=Debug
set BUILD_MODE_FULL=%BUILD_MODE%

if "%1" == "-x64" goto build_x64
if "%1" == "-x86" goto build_ia32
goto build

:build_x64
set GYP_DEFINES=os=win target_arch=x64 build_with_libjingle=1 build_with_chromium=0 use_system_ssl=0 use_openssl=1 use_nss=0
set BUILD_MODE_FULL=%BUILD_MODE%_x64
set LIB_OUT=Win64
goto build

:build_ia32
set GYP_DEFINES=os=win target_arch=ia32 build_with_libjingle=1 build_with_chromium=0 use_system_ssl=0 use_openssl=1 use_nss=0
set LIB_OUT=Win32
goto build

:build
call gclient runhooks --verbose --force 
rem pause

pushd .\src

call ninja -C %OUTPUT_DIR%\%BUILD_MODE_FULL% -t clean
call ninja -v -C %OUTPUT_DIR%\%BUILD_MODE_FULL% libjingle rtc_base rtc_base_approved rtc_xmllite rtc_xmpp boringssl expat
rem pause

set ARTIFACT_DIR=.\out\Artifacts
set ARTIFACT_HEADERS=%ARTIFACT_DIR%\include
xcopy /s /c /d .\net\*.h %ARTIFACT_HEADERS%\net\
xcopy /s /c /d .\talk\*.h %ARTIFACT_HEADERS%\talk\
xcopy /s /c /d .\webrtc\*.h %ARTIFACT_HEADERS%\webrtc\
xcopy /s /c /d .\third_party\*.h %ARTIFACT_HEADERS%\third_party\
rem pause

set ARTIFACT_LIBS=%ARTIFACT_DIR%\lib
rmdir /s /q %ARTIFACT_LIBS%\%LIB_OUT%\VS%GYP_MSVS_VERSION%\%BUILD_MODE%
mkdir %ARTIFACT_LIBS%\%LIB_OUT%\VS%GYP_MSVS_VERSION%\%BUILD_MODE%
for /r .\%OUTPUT_DIR%\%BUILD_MODE_FULL% %%f in (*.lib) do "cmd /c @copy /y %%f %ARTIFACT_LIBS%\%LIB_OUT%\VS%GYP_MSVS_VERSION%\%BUILD_MODE%\%%~nxf"
rem pause

popd
