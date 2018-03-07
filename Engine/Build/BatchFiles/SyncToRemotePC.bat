@echo off
setlocal

if "%1" == "" (
	echo Usage: SyncToRemotePC RemotePath GameName [-full]
	goto :eof
)

set REMOTEPATH=%1
set GAMENAME=%2
set OPTION=%3
set ENGINEPATH=%REMOTEPATH%\Engine
set GAMEPATH=%REMOTEPATH%\%GAMENAME%
set XCOPYFLAGS=/i /y /d /s /exclude:Build\BatchFiles\SyncToRemotePC.exclude 

xcopy Binaries 					%ENGINEPATH%\Binaries 	%XCOPYFLAGS%
xcopy ..\%GAMENAME%\Binaries 	%GAMEPATH%\Binaries		%XCOPYFLAGS%

if "%OPTION%" NEQ "-full" goto :EOF

xcopy Shaders 					%ENGINEPATH%\Shaders 	%XCOPYFLAGS%
xcopy . 						%ENGINEPATH%	 		%XCOPYFLAGS%
xcopy ..\%GAMENAME%	 			%GAMEPATH%				%XCOPYFLAGS%
