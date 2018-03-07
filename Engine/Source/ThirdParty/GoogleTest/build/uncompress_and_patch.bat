REM @echo off

REM * this unzips the tar.gz using 7zip and applies any patches stored in the google-test-source-patches directory
REM * uncompress_and_patch.bat

"c:\Program Files\7-Zip\7z.exe" x google-test-source.tar.gz
"c:\Program Files\7-Zip\7z.exe" x google-test-source.tar -y
del google-test-source.tar

xcopy /s /c /d /y google-test-source-patches\* google-test-source-patches


