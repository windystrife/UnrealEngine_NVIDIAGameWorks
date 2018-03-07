set GAME=UE4
pushd ..\..\..\..\
cd Engine\Binaries\Win64\
UE4Editor-Cmd.exe ..\..\..\Samples\SampleGames\SwingNinja\SwingNinja.uproject -run=cook -targetplatform=HTML5 -Map=P_Starting+SimpleLevel2
popd 
