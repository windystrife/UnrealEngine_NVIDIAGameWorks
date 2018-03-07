@echo off

echo This batch file, enables the Oodle plugin, and enables it as a packet handler.
echo.


REM This batch file can only work from within the Oodle folder.
set BaseFolder="..\..\..\..\..\.."

if exist %BaseFolder:"=%\Engine goto SetUE4Editor

echo Could not locate Engine folder. This .bat must be located within UE4\Engine\Plugins\Runtime\PacketHandlers\CompressionComponents\Oodle
goto End


:SetUE4Editor
set UE4EditorLoc="%BaseFolder:"=%\Engine\Binaries\Win64\UE4Editor.exe"

if exist %UE4EditorLoc:"=% goto GetGame

echo Could not locate UE4Editor.exe
goto End


:GetGame
set /p GameName=Type the name of the game you are working with: 
echo.



:EnablePlugin
set EnableCommandletParms=-run=plugin enable Oodle
set FinalEnableCmdLine=%GameName:"=% %EnableCommandletParms% -forcelogflush

echo Executing plugin enable commandlet - commandline:
echo %FinalEnableCmdLine%

@echo on
%UE4EditorLoc:"=% %FinalEnableCmdLine%
@echo off
echo.


if %errorlevel%==0 goto EnableHandler

echo WARNING! Detected error, plugin may not have been enabled. Will attempt to run Oodle enable commandlet.
pause


:EnableHandler
set HandlerCommandletParms=-run=OodleHandlerComponent.OodleTrainerCommandlet enable
set FinalHandlerCmdLine=%GameName:"=% %HandlerCommandletParms% -forcelogflush


echo Executing Oodle PacketHandler enable commandlet - commandline:
echo %FinalHandlerCmdLine%

@echo on
%UE4EditorLoc:"=% %FinalHandlerCmdLine%
@echo off
echo.


if %errorlevel%==0 goto End

echo WARNING! Detected error when executing PacketHandler enable commandlet. Review the logfile.


:End
echo Execution complete.
pause


REM Put nothing past here.

