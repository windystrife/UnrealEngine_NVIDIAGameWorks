@echo off

echo This batch file, individually converts all discovered ucap files in the specific directory, into Oodle-example-code compatible .bin files
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


:GetCaptureDir
set /p CaptureDir=Paste the directory where the ucap files are located
echo.

if exist %CaptureDir:"=% goto GetOutputDir

echo Could not locate capture files directory
goto End


:GetOutputDir
set /p OutputDir=Paste the directory where the .bin files should be output to
echo.

if exist %OutputDir:"=% goto AutoGenDictionaries

echo Could not locate output directory
goto End



:AutoGenDictionaries
set DebugDumpParms=-run=OodleHandlerComponent.OodleTrainerCommandlet DebugDump
set PreDumpCmdLine=%GameName:"=% %DebugDumpParms% %OutputDir:"=% %CaptureDir:"=%
set PostDumpCmdLine=-forcelogflush

echo Executing dictionary generation commandlet - commandline:
echo %UE4EditorLoc:"=% %PreDumpCmdLine:"=% %PostDumpCmdLine:"=%


%UE4EditorLoc:"=% %PreDumpCmdLine:"=% %PostDumpCmdLine:"=%

echo.


if %errorlevel%==0 goto End

echo WARNING! Detected error, dictionaries may not have been generated. Check output and logfile for errors.
pause


:End
echo Execution complete.
pause


REM Put nothing past here.

