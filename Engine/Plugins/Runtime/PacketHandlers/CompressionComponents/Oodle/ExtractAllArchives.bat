@echo off

echo.
echo Downloading packet captures from S3
echo.

if "%1" == "" goto getdirectory
set Directory=%1
goto getoptions

:getdirectory
set /p Directory=Type the root directory of the zipped captures: 
echo.

:getoptions
if "%2" == "" GOTO continue
set Options= --exclude "*" --include "*%2*.ucap.gz"

:continue
aws s3 sync %3 %Directory% %Options%

pushd %CD%
cd %Directory%
FOR /D /r %%F in ("*") DO (
	pushd %CD%
	cd %%F
		FOR %%X in (*.gz) DO (
			"C:\Program Files\7-zip\7z.exe" x "%%X" -y -aos
		)
	popd
)
popd