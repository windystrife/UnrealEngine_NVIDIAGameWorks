@ECHO off

SET VSComnToolsPath=
SET TmpPath=""

FOR /f "tokens=2,*" %%A IN ('REG.exe query HKCU\SOFTWARE\Microsoft\VisualStudio\SxS\VS7 /v "%1.0" 2^>Nul') DO (
	SET TmpPath="%%B\Common7\Tools"
	GOTO havePath
)
FOR /f "tokens=2,*" %%A IN ('REG.exe query HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7 /v "%1.0" 2^>Nul') DO (
	SET TmpPath="%%B\Common7\Tools"
	GOTO havePath
)
FOR /f "tokens=2,*" %%A IN ('REG.exe query HKCU\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7 /v "%1.0" 2^>Nul') DO (
	SET TmpPath="%%B\Common7\Tools"
	GOTO havePath
)
FOR /f "tokens=2,*" %%A IN ('REG.exe query HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7 /v "%1.0" 2^>Nul') DO (
	SET TmpPath="%%B\Common7\Tools"
	GOTO havePath
)

:havePath
IF NOT %TmpPath% == "" (
	CALL :normalisePath %TmpPath%
)

GOTO :EOF

:normalisePath
SET VSComnToolsPath=%~f1
GOTO :EOF
