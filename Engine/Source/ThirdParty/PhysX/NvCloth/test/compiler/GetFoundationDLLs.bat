@echo off
setlocal enabledelayedexpansion

call "%~dp0../../scripts/locate_px_shared.bat" PX_SHARED_BIN

echo found %PX_SHARED_BIN%, updating PxShared dll's
call :strLen PX_SHARED_BIN pathlen
set /a pathlen=%pathlen%+5
FOR /D %%G IN ("%PX_SHARED_BIN%\bin\*") DO (
	set toolplatform=%%Gx
	call set toolplatform2=%%toolplatform:~%pathlen%,-1%%
	echo "%~dp0!toolplatform2!"
	FOR %%D IN ("%~dp0!toolplatform2!\") DO (
		xcopy /y /d "%%G\PxFoundation*.dll" %%D|find /v "File(s) copied"
		xcopy /y /d "%%G\PxFoundation*.pdb" %%D|find /v "File(s) copied"
	)
)

goto :end

rem functions
:strLen  strVar  [rtnVar]
setlocal disableDelayedExpansion
set len=0
if defined %~1 for /f "delims=:" %%N in (
  '"(cmd /v:on /c echo(!%~1!&echo()|findstr /o ^^"'
) do set /a "len=%%N-3"
endlocal & if "%~2" neq "" (set %~2=%len%) else echo %len%
exit /b


:end
