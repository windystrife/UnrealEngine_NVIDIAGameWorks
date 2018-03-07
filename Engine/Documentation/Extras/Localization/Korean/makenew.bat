@echo off
if not exist %2 (
echo INTSourceChangelist:%3> %2
type %1 >> %2
p4 add %2
)
"C:\Program Files\Araxis\Araxis Merge\compare" %1 %2
d:\Perforce\ue4\Engine\Binaries\DotNET\UnrealDocTool.exe %1 -p
