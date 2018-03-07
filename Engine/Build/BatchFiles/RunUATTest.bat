
@echo off

if "%uebp_testfail%" == "1" exit /B 1

if not "%uebp_LogFolder%" == "" del /s /f /q "%uebp_LogFolder%\*.*"
call runuat.bat %*
if ERRORLEVEL 1 goto fail

ECHO Test Success: call runuat.bat %*
exit /B 0

:fail

ECHO ****************** FAILED TEST:
ECHO call runuat.bat %*
ECHO ******************

set uebp_testfail=1

exit /B 1



