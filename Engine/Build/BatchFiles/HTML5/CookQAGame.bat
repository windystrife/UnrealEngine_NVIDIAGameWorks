set GAME=QAGame
pushd ..\..\..\..\
cd Engine\Binaries\Win64\
UE4Editor.exe QAGame -run=cook -targetplatform=HTML5 -map=/Game/Maps/QAEntry 
popd 
