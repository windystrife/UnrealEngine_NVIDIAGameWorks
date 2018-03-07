start /wait E:\UE3\Jeff_Wilson_DocsPublish\UnrealDocTool\Binaries\DotNET\unrealdoctool.exe  ./;Engine/*;Gameplay/*;GettingStarted/*;Platforms/*;Programming/*;Resources/*;Support/* -s=%~dp0\..\Source -pathPrefix=%~dp0\..\Source -o=%~dp0\..\PublicRelease -lang=INT,KOR,JPN,CHN -publish=rocket -v=info -outputFormat=HTML -doxygenCache=%~dp0\..\XML -env=working -dbsetup -dbupdate -videoupdate

start /wait E:\UE3\Jeff_Wilson_DocsPublish\UnrealDocTool\Binaries\DotNET\unrealdoctool.exe  ./;Engine/*;Gameplay/*;GettingStarted/*;Platforms/*;Programming/*;Resources/*;Search/*;Shared/*;SiteIndex/*;Support/*;Videos/* -s=%~dp0\..\Source -pathPrefix=%~dp0\..\Source -o=%~dp0\..\PublicRelease -lang=INT,KOR,JPN,CHN -publish=rocket -v=info -outputFormat=HTML -doxygenCache=%~dp0\..\XML -env=working -changelist -sidebar -decor

start /wait %~dp0\..\..\Binaries\DotNET\unrealdocfiles.exe -s=%~dp0\..\PublicRelease -o=%~dp0\..\PublicRelease -file=sitemap -match=html -type=rich -noindex

copy %~dp0\404.html %~dp0\..\PublicRelease\INT\404.html
copy %~dp0\404.html %~dp0\..\PublicRelease\KOR\404.html
copy %~dp0\404.html %~dp0\..\PublicRelease\JPN\404.html
copy %~dp0\404.html %~dp0\..\PublicRelease\CHN\404.html


copy %~dp0\google45d118c22307b85b.html %~dp0\..\PublicRelease\google45d118c22307b85b.html