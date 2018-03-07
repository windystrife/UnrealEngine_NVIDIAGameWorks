@echo off
goto START

-------------------------------------------------------
 buildhelp.bat


 Created: Mon Jul 11 23:00:50 2011
 Last Saved: <2011-July-11 23:20:34>

-------------------------------------------------------

:START
SETLOCAL
set MSBUILD=c:\.net3.5\msbuild.exe

%msbuild% /nologo HtmlHelp1.shfbproj
%msbuild% /nologo MSHelp2.shfbproj
%msbuild% /nologo HelpViewer.shfbproj

@REM  %MSBUILD% /p:HelpFileFormat=HtmlHelp1 HelpViewer.shfbproj

goto ALL_DONE

-------------------------------------------------------



--------------------------------------------
:USAGE
  echo usage:   buildhelp ^<arg^> [^<optionalarg^>]
  echo blah blah blah
  goto ALL_DONE

--------------------------------------------


:ALL_DONE
ENDLOCAL
