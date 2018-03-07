@echo off

REM * builds all current configs for openssl
REM * build_openssl_win_all <1.0.1g/...>

call build_openssl_win.bat %1 2012 x86
call build_openssl_win.bat %1 2012 x64
call build_openssl_win.bat %1 2013 x86
call build_openssl_win.bat %1 2013 x64
call build_openssl_win.bat %1 2015 x86
call build_openssl_win.bat %1 2015 x64
