@echo off 
rem do not call this bat file directly, use BuildThirdParty.bat 
rem set nmake envs. 
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86
FOR /F "eol=; tokens=1,2,3* delims=, " %%i  IN (MakeFileDirs.txt) DO (
			echo Building %%i 
			pushd .
			cd %%j 
			nmake -f %%k %1 
			popd
		)	
pause 		
exit		
		
	
