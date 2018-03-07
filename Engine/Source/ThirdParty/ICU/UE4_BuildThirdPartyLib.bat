REM ICU

	set ICU_DATA=%cd%\icu4c-51_2\source\data\out\build\

pushd icu4c-51_2\source\allinone

	p4 edit %THIRD_PARTY_CHANGELIST% ..\..\include\...
	p4 edit %THIRD_PARTY_CHANGELIST% ..\..\Win32\VS2015\bin\...
	p4 edit %THIRD_PARTY_CHANGELIST% ..\..\Win64\VS2015\bin\...

	REM vs2015
	REM msbuild allinone_2015.sln /target:Clean,Build /p:Platform=Win32;Configuration="Release"
	REM msbuild allinone_2015.sln /target:Clean,Build /p:Platform=Win32;Configuration="Debug"
	REM msbuild allinone_2015.sln /target:Clean,Build /p:Platform=x64;Configuration="Release"
	REM msbuild allinone_2015.sln /target:Clean,Build /p:Platform=x64;Configuration="Debug"

	REM Missing XboxOne, PS4, Android, Linux, HTML5
popd

