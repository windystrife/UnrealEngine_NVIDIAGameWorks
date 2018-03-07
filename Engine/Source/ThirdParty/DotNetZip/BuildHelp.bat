
msbuild.exe  /p:Configuration=Release   Help\HtmlHelp1.shfbproj

msbuild.exe  /p:Configuration=Release   Help\HelpViewer.shfbproj


if exist c:\dev\dotnet\pronounceword.exe (c:\dev\dotnet\pronounceword.exe Build Complete >nul:)
