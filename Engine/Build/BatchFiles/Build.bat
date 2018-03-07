@echo off
setlocal enabledelayedexpansion

REM The %~dp0 specifier resolves to the path to the directory where this .bat is located in.
REM We use this so that regardless of where the .bat file was executed from, we can change to
REM directory relative to where we know the .bat is stored.
pushd "%~dp0\..\..\Source"

REM %1 is the game name
REM %2 is the platform name
REM %3 is the configuration name

IF EXIST ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe (
        ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe %* -DEPLOY
		popd

		REM Ignore exit codes of 2 ("ECompilationResult.UpToDate") from UBT; it's not a failure.
		if "!ERRORLEVEL!"=="2" (
			EXIT /B 0
		)
		 
		EXIT /B !ERRORLEVEL!
) ELSE (
	ECHO UnrealBuildTool.exe not found in ..\..\Engine\Binaries\DotNET\UnrealBuildTool.exe 
	popd
	EXIT /B 999
)
