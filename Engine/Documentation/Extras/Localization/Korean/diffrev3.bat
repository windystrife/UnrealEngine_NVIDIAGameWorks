@echo off
echo INTSourceChangelist:%4> %3.txt
p4 print -q %1@%3 >> %3.txt
p4 edit %2
"C:\Program Files\Araxis\Araxis Merge\compare" /a3 /3 %5 %2 %3.txt
del %3.txt
d:\Perforce\ue4\Engine\Binaries\DotNET\UnrealDocTool.exe %5 -p