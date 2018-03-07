@echo off

REM %1 is the game name
REM %2 is the platform name
REM %3 is the configuration name

IF NOT EXIST "%~dp0\Clean.bat" GOTO Error_MissingCleanBatchFile

call "%~dp0\Clean.bat" %*

IF NOT EXIST "%~dp0\Build.bat" GOTO Error_MissingBuildBatchFile

call "%~dp0\Build.bat" %*
GOTO Exit

:Error_MissingCleanBatchFile
ECHO Clean.bat not found in "%~dp0"
EXIT /B 999

:Error_MissingBuildBatchFile
ECHO Build.bat not found in "%~dp0"
EXIT /B 999

:Exit
