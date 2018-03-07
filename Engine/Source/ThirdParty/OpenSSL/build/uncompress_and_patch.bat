REM @echo off

REM * this unzips the tar.gz using 7zip and applies any patches stored in the xxxxxxxxxxxxxxxx-patches directory
REM * uncompress_and_patch.bat <1.0.2g/...>
pushd %1
"c:\Program Files\7-Zip\7z.exe" x openssl-%1.tar.gz
"c:\Program Files\7-Zip\7z.exe" x openssl-%1.tar -y
del openssl-%1.tar
xcopy /s /c /d /y openssl-%1-patches\* openssl-%1
popd

