mkdir %LOCALAPPDATA%\Microsoft\VisualStudio\11.0Exp\Extensions\EpicGames\UnrealVS
xcopy ..\..\..\Extras\UnrealVS\Resources %LOCALAPPDATA%\Microsoft\VisualStudio\11.0Exp\Extensions\EpicGames\UnrealVS\Resources /I /Y
copy ..\..\..\Extras\UnrealVS\*.dll %LOCALAPPDATA%\Microsoft\VisualStudio\11.0Exp\Extensions\EpicGames\UnrealVS\ /Y
copy ..\..\..\Extras\UnrealVS\*.pdb %LOCALAPPDATA%\Microsoft\VisualStudio\11.0Exp\Extensions\EpicGames\UnrealVS\ /Y
copy ..\..\..\Extras\UnrealVS\extension.vsixmanifest %LOCALAPPDATA%\Microsoft\VisualStudio\11.0Exp\Extensions\EpicGames\UnrealVS\extension.vsixmanifest /Y
copy ..\..\..\Extras\UnrealVS\UnrealVS.pkgdef %LOCALAPPDATA%\Microsoft\VisualStudio\11.0Exp\Extensions\EpicGames\UnrealVS\UnrealVS.pkgdef /Y